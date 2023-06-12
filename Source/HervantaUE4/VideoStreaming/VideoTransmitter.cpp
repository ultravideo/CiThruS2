#include "VideoTransmitter.h"

// These come from some third-party include, but they break std::max and
// std::min so they have to be undefined
#undef max
#undef min

#include "../Debug.h"

#include <string>
#include <algorithm>

#define VIDEO_FRAME_MAXLEN 2048*4096*40

AVideoTransmitter::AVideoTransmitter()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AVideoTransmitter::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	DeleteStreams();
}

// Called every frame
void AVideoTransmitter::Tick(float deltaTime)
{
	Super::Tick(deltaTime);

	if (!transmit_enabled)
	{
		return;
	}

	CaptureAndTransmit();
}

void AVideoTransmitter::ResetStreams()
{
	DeleteStreams();
	StartStreams();
}

void AVideoTransmitter::DeleteStreams()
{
	// Stop encoding thread
	if (encoding_thread.joinable())
	{
		terminate_encoding_thread = true;
		rgba_frame_cv.notify_all();

		encoding_thread.join();
	}

	// Wait for frame operations to finish
	std::unique_lock<std::mutex> lock(vram_copy_mutex);

	if (vram_extraction_in_progress)
	{
		vram_copy_cv.wait(lock);
	}

#ifndef CITHRUS_NO_KVAZAAR_OR_UVGRTP
	if (stream)
	{
		stream_session->destroy_stream(stream);
		stream = nullptr;
	}

	if (stream_session)
	{
		stream_context.destroy_session(stream_session);
		stream_session = nullptr;
	}
#endif

	render_targets.clear();

	if (concat_buffer)
	{
		concat_buffer = nullptr;
	}

	if (staging_buffer)
	{
		staging_buffer = nullptr;
	}

	delete[] rgba_frame;
	delete[] warped_frame;
	delete[] yuv_frame;
	delete[] encoded_frame;

	rgba_frame = nullptr;
	warped_frame = nullptr;
	yuv_frame = nullptr;
	encoded_frame = nullptr;

	if (debug_outfile)
	{
		fclose(debug_outfile);
		debug_outfile = nullptr;
	}
}

void AVideoTransmitter::StartStreams()
{
	// Capturing below 16x16 causes corrupted video (maybe HEVC limitation?)
	transmit_frame_width = std::max(remote_stream_width, 16);
	transmit_frame_height = std::max(remote_stream_height, 16);

	// Width and height must be divisible by eight (HEVC limitation)
	// This rounds up to the nearest integers divisible by eight
	transmit_frame_width += (8 - (transmit_frame_width % 8)) % 8;
	transmit_frame_height += (8 - (transmit_frame_height % 8)) % 8;

	transmit_360_video = enable_360_capture;
	transmit_async = transmit_asynchronously;
	thread_count = std::max(processing_thread_count, 1);

	if (transmit_360_video)
	{
		capture_frame_width = width_and_height_per_capture_side;
		capture_frame_height = width_and_height_per_capture_side;

		scene_capture->SetRenderTargetResolution(capture_frame_width, capture_frame_height);

		cube_2_equirect = Cube2Equirectangular();
		cube_2_equirect.init(transmit_frame_width, transmit_frame_height, capture_frame_width, M_PI, 2.0 * M_PI);
		cube_2_equirect.genMap();

		ENQUEUE_RENDER_COMMAND(GetRenderTargets)(
			[this](FRHICommandListImmediate& RHICmdList)
			{
				render_targets.resize(scene_capture->Get360CaptureTargetCount(), nullptr);

				for (int i = 0; i < scene_capture->Get360CaptureTargetCount(); i++)
				{
					render_targets[i] = scene_capture->Get360FrameTarget(i)->GetRenderTargetResource()->GetRenderTargetTexture();
				}
			});
	}
	else
	{
		capture_frame_width = transmit_frame_width;
		capture_frame_height = transmit_frame_height;

		scene_capture->SetRenderTargetResolution(capture_frame_width, capture_frame_height);

		ENQUEUE_RENDER_COMMAND(GetRenderTargets)(
			[this](FRHICommandListImmediate& RHICmdList)
			{
				render_targets.resize(1, nullptr);

				render_targets[0] = scene_capture->GetPerspectiveFrameTarget()->GetRenderTargetResource()->GetRenderTargetTexture();
			});
	}

	const int num_frames = transmit_360_video ? scene_capture->Get360CaptureTargetCount() : 1;

	ENQUEUE_RENDER_COMMAND(CreateStagingTexture)(
		[this, num_frames](FRHICommandListImmediate& RHICmdList)
		{
			// Vulkan doesn't support copying texture data to a specific offset in a CPU-accessible texture so a separate
			// buffer is needed for concatenating the 360 cubemap sides. In DX11/12 it is supported, so this can be skipped
			if (FString(GDynamicRHI->GetName()) == TEXT("Vulkan"))
			{
				FRHIResourceCreateInfo concat_create_info(TEXT("HEVC Stream Concatenation Texture"));

				concat_buffer = RHICreateTexture2D(num_frames * capture_frame_width, capture_frame_height, PF_B8G8R8A8, 1, 1, TexCreate_None, concat_create_info);
			}

			FRHIResourceCreateInfo staging_create_info(TEXT("HEVC Stream Staging Texture"));

			staging_buffer = RHICreateTexture2D(num_frames * capture_frame_width, capture_frame_height, PF_B8G8R8A8, 1, 1, TexCreate_CPUReadback, staging_create_info);
		});

	rgba_frame = new uint8_t[num_frames * capture_frame_width * capture_frame_height * 4];
	warped_frame = new uint8_t[transmit_frame_width * transmit_frame_height * 4];
	yuv_frame = new uint8_t[transmit_frame_width * transmit_frame_height * 2];
	encoded_frame = new uint8_t[VIDEO_FRAME_MAXLEN];

	Debug::Log("rgba buffer initialized to the size of " + FString::FromInt(num_frames * transmit_frame_width * transmit_frame_height * 4));

#ifndef CITHRUS_NO_KVAZAAR_OR_UVGRTP
	stream_session = stream_context.create_session(TCHAR_TO_UTF8(*remote_stream_ip));
	stream = stream_session->create_stream(remote_src_port, remote_dst_port, RTP_FORMAT_H265, RCE_NO_SYSTEM_CALL_CLUSTERING);

	if (debug_write_to_hevc)
	{
		OpenDebugFile();
	}

	// Set up Kvazaar for encoding
	kvazaar_config = kvazaar_api->config_alloc();

	kvazaar_api->config_init(kvazaar_config);
	kvazaar_api->config_parse(kvazaar_config, "preset", "ultrafast");
	kvazaar_api->config_parse(kvazaar_config, "gop", "lp-g8d3t1");
	kvazaar_api->config_parse(kvazaar_config, "vps-period", "1");
	kvazaar_api->config_parse(kvazaar_config, "force-level", "4");
	kvazaar_api->config_parse(kvazaar_config, "threads", std::to_string(thread_count).c_str());
	//kvazaar_api->config_parse(kvazaar_config, "intra_period", "16");
	//kvazaar_api->config_parse(kvazaar_config, "period", "64");
	//kvazaar_api->config_parse(kvazaar_config, "tiles", "3x3");
	//kvazaar_api->config_parse(kvazaar_config, "slices", "tiles");
	//kvazaar_api->config_parse(kvazaar_config, "mv-constraint", "frametilemargin");
	//kvazaar_api->config_parse(kvazaar_config, "slices", "wpp");

	kvazaar_config->width = transmit_frame_width;
	kvazaar_config->height = transmit_frame_height;
	kvazaar_config->hash = KVZ_HASH_NONE;
	kvazaar_config->qp = quantization_parameter;
	kvazaar_config->wpp = wavefront_parallel_processing;
	kvazaar_config->owf = overlapped_wavefront;
	//kvazaar_config->intra_period = 16;
	kvazaar_config->aud_enable = 0;
	kvazaar_config->calc_psnr = 0;

	kvazaar_encoder = kvazaar_api->encoder_open(kvazaar_config);
	kvazaar_transmit_picture = kvazaar_api->picture_alloc(transmit_frame_width, transmit_frame_height);
#endif
}

void AVideoTransmitter::StartTransmit()
{
	if (!scene_capture)
	{
		Debug::Log("Scene capture is null, aborting");
		return;
	}

	ResetStreams();

	scene_capture->EnableCameras(transmit_360_video);

	transmit_enabled = true;
	use_editor_tick = true;
	terminate_encoding_thread = false;

	if (transmit_async)
	{
		encoding_thread = std::thread(&AVideoTransmitter::TransmitAsync, this);
	}
}

void AVideoTransmitter::StopTransmit()
{
	DeleteStreams();

	if (!scene_capture)
	{
		return;
	}

	scene_capture->DisableCameras();
	transmit_enabled = false;
	use_editor_tick = false;
}

void AVideoTransmitter::CaptureAndTransmit()
{
	ExtractFrameFromGPU();

	if (!transmit_async)
	{
		EncodeAndTransmitFrame();
	}
}

void AVideoTransmitter::ExtractFrameFromGPU()
{
	std::unique_lock<std::mutex> lock(vram_copy_mutex);

	if (vram_extraction_in_progress)
	{
		vram_copy_cv.wait(lock);
	}

	vram_extraction_in_progress = true;

	// Note that this is executed on the render thread later and not yet
	ENQUEUE_RENDER_COMMAND(ExtractRenderTargets)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			vram_copy_mutex.lock();

			// Copy render targets to the staging texture, with an extra concatenation buffer or not
			if (concat_buffer)
			{
				for (int i = 0; i < render_targets.size(); i++)
				{
					FRHICopyTextureInfo copy_texture_info = {};

					copy_texture_info.DestPosition = FIntVector(i * capture_frame_width, 0, 0);
					copy_texture_info.Size = FIntVector(capture_frame_width, capture_frame_height, 1);

					RHICmdList.CopyTexture(render_targets[i], concat_buffer, copy_texture_info);
				}

				FRHICopyTextureInfo copy_texture_info = {};

				RHICmdList.CopyTexture(concat_buffer, staging_buffer, copy_texture_info);
			}
			else
			{
				for (int i = 0; i < render_targets.size(); i++)
				{
					FRHICopyTextureInfo copy_texture_info = {};

					copy_texture_info.DestPosition = FIntVector(i * capture_frame_width, 0, 0);
					copy_texture_info.Size = FIntVector(capture_frame_width, capture_frame_height, 1);

					RHICmdList.CopyTexture(render_targets[i], staging_buffer, copy_texture_info);
				}
			}

			// Extract the contents of the staging texture
			void* resource;
			int32_t staging_buffer_width;
			int32_t staging_buffer_height;
			RHICmdList.MapStagingSurface(staging_buffer, resource, staging_buffer_width, staging_buffer_height);

			rgba_frame_mutex.lock();

			const int32_t thread_batch_size = ceil(staging_buffer_height / (float)thread_count);
			const int32_t num_frames = render_targets.size();

			if (staging_buffer_width == num_frames * capture_frame_width && staging_buffer_height == capture_frame_height)
			{
				// Mapped texture data has no padding: multiple rows can be copied in a single memcpy
				const int32_t staging_pitch = staging_buffer_width * 4;

				ParallelFor(processing_thread_count, [&](int32_t i)
					{
						const int32_t thread_batch_start = thread_batch_size * i * staging_pitch;
						const int32_t thread_batch_end = std::min(thread_batch_size * (i + 1), staging_buffer_height) * staging_pitch;

						memcpy(
							rgba_frame + thread_batch_start,
							reinterpret_cast<uint8_t*>(resource) + thread_batch_start,
							thread_batch_end - thread_batch_start);
					});
			}
			else
			{
				// Mapped texture data has padding: must be copied row by row to discard the padding
				const int32_t rgba_pitch = num_frames * capture_frame_width * 4;
				const int32_t staging_pitch = staging_buffer_width * 4;

				ParallelFor(processing_thread_count, [&](int32_t i)
					{
						const int32_t thread_rows_start = thread_batch_size * i;
						const int32_t thread_rows_end = std::min(thread_batch_size * (i + 1), staging_buffer_height);

						for (int j = thread_rows_start; j < thread_rows_end; j++)
						{
							memcpy(
								rgba_frame + j * rgba_pitch,
								reinterpret_cast<uint8_t*>(resource) + j * staging_pitch,
								rgba_pitch);
						}
					});
			}

			rgba_frame_mutex.unlock();
			rgba_frame_cv.notify_all();

			RHICmdList.UnmapStagingSurface(staging_buffer);

			vram_extraction_in_progress = false;

			vram_copy_mutex.unlock();
			vram_copy_cv.notify_all();
		});
}

void AVideoTransmitter::EncodeAndTransmitFrame()
{
	std::unique_lock<std::mutex> lock(rgba_frame_mutex);

	rgba_frame_cv.wait(lock);

	// Termination might have been requested when the frame mutex was released
	if (terminate_encoding_thread)
	{
		return;
	}

	// Encode
	if (transmit_360_video)
	{
		WarpFrameEquirectangular();

		Rgb2YuvSse41(warped_frame, &yuv_frame, transmit_frame_width, transmit_frame_height);
	}
	else
	{
		Rgb2YuvSse41(rgba_frame, &yuv_frame, transmit_frame_width, transmit_frame_height);
	}

	lock.unlock();

	uint8_t* ptr = yuv_frame;

#ifndef CITHRUS_NO_KVAZAAR_OR_UVGRTP
	memcpy(kvazaar_transmit_picture->y, ptr, transmit_frame_width * transmit_frame_height);
	ptr += transmit_frame_width * transmit_frame_height;
	memcpy(kvazaar_transmit_picture->u, ptr, transmit_frame_width * transmit_frame_height / 4);
	ptr += transmit_frame_width * transmit_frame_height / 4;
	memcpy(kvazaar_transmit_picture->v, ptr, transmit_frame_width * transmit_frame_height / 4);

	kvz_picture* recon_pic = nullptr;
	kvz_frame_info frame_info;
	kvz_data_chunk* data_out = nullptr;
	uint32_t len_out = 0;

	kvazaar_api->encoder_encode(kvazaar_encoder, kvazaar_transmit_picture,
		&data_out, &len_out,
		nullptr, nullptr,
		&frame_info);

	if (!data_out)
	{
		return;
	}

	uint8_t* data_ptr = encoded_frame;

	kvz_data_chunk* previous_chunk = 0;
	for (kvz_data_chunk* chunk = data_out; chunk != nullptr; chunk = chunk->next)
	{
		memcpy(data_ptr, chunk->data, chunk->len);
		data_ptr += chunk->len;
		previous_chunk = chunk;
	}
	kvazaar_api->chunk_free(data_out);

	if (debug_write_to_hevc && debug_outfile)
	{
		fwrite(encoded_frame, sizeof(uint8_t), len_out, debug_outfile);
		Debug::Log("wrote frame to file");
	}

	// Transmit
	if (!stream)
	{
		Debug::Log("failed to create media stream");
	}
	else if (stream->push_frame(encoded_frame, len_out, RTP_NO_FLAGS) != RTP_ERROR::RTP_OK)
	{
		Debug::Log("failed to push frame");
	}


#else
	Debug::Log("kvazaar and uvgRTP not found: streaming is unavailable");
#endif
}

void AVideoTransmitter::TransmitAsync()
{
	while (!terminate_encoding_thread)
	{
		Timer t;

		EncodeAndTransmitFrame();

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(0, 5.f, FColor::Green, "transmit fps " + FString::FromInt((int)t.Fps()));
		}
	}
}

void AVideoTransmitter::WarpFrameEquirectangular()
{
	const int num_frames = scene_capture->Get360CaptureTargetCount();
	const int panorama_width = cube_2_equirect.pxPanoSizeH;
	const int panorama_height = cube_2_equirect.pxPanoSizeV;

	const int thread_batch_size = ceil(panorama_width / (float)thread_count);

	ParallelFor(processing_thread_count, [&](int32 i)
		{
			const int thread_batch_start = thread_batch_size * i;
			const int thread_batch_end = std::min(thread_batch_size * (i + 1), panorama_width);

			for (int32 x = thread_batch_start; x < thread_batch_end; x++)
			{
				for (int32 y = 0; y < panorama_height; y++)
				{
					const int warped_buffer_index = (y * panorama_width + x) * 4;
					const auto face_coord = cube_2_equirect.getCoord(x, y);
					const CUBE_FACES& face = face_coord->face;

					if (!bilinear_filtering)
					{
						// No filtering
						const int rgba_buffer_index = ((int)face_coord->y * num_frames * capture_frame_width + (int)face_coord->x + face * capture_frame_width) * 4;
						memcpy(&warped_frame[warped_buffer_index], &rgba_frame[rgba_buffer_index], 4);

						continue;
					}

					// Sample the cubemap textures with bilinear interpolation
					// The 0.5s here are to center the sampling areas so that each pixel's color is blended around its center
					int face_x = floor(face_coord->x - 0.5);
					int face_y = floor(face_coord->y - 0.5);

					const float dx = face_coord->x - 0.5 - face_x;
					const float dy = face_coord->y - 0.5 - face_y;

					// These variables are named as follows:
					// b = bottom, r = right, t = top, l = left
					// bl = bottom left and so on

					const float weight_bl = (1.0f - dx) * (1.0f - dy);
					const float weight_br = dx * (1.0f - dy);
					const float weight_tl = (1.0f - dx) * dy;
					const float weight_tr = dx * dy;

					const bool r_boundary_crossed = face_x + 1 >= capture_frame_width;
					const bool t_boundary_crossed = face_y + 1 >= capture_frame_height;
					const bool l_boundary_crossed = face_x < 0;
					const bool b_boundary_crossed = face_y < 0;

					// Get the correct pixel indices for sampling
					int rgba_buffer_index_bl;
					int rgba_buffer_index_br;
					int rgba_buffer_index_tl;
					int rgba_buffer_index_tr;

					if (!r_boundary_crossed && !t_boundary_crossed && !l_boundary_crossed && !b_boundary_crossed)
					{
						// If no boundaries were crossed, getting the sample pixels is simple because they're on the same face
						rgba_buffer_index_bl = ((face_y + 0) * num_frames * capture_frame_width + (face_x + 0) + face * capture_frame_width) * 4;
						rgba_buffer_index_br = ((face_y + 0) * num_frames * capture_frame_width + (face_x + 1) + face * capture_frame_width) * 4;
						rgba_buffer_index_tl = ((face_y + 1) * num_frames * capture_frame_width + (face_x + 0) + face * capture_frame_width) * 4;
						rgba_buffer_index_tr = ((face_y + 1) * num_frames * capture_frame_width + (face_x + 1) + face * capture_frame_width) * 4;
					}
					else
					{
						// Adjacent faces for each face in order of the direction enum
						const static CUBE_FACES ADJACENT_FACES[6][4] =
						{
							{ CUBE_FACES::CUBE_BACK,  CUBE_FACES::CUBE_RIGHT, CUBE_FACES::CUBE_FRONT, CUBE_FACES::CUBE_LEFT  },
							{ CUBE_FACES::CUBE_TOP,   CUBE_FACES::CUBE_FRONT, CUBE_FACES::CUBE_DOWN,  CUBE_FACES::CUBE_BACK  },
							{ CUBE_FACES::CUBE_TOP,   CUBE_FACES::CUBE_RIGHT, CUBE_FACES::CUBE_DOWN,  CUBE_FACES::CUBE_LEFT  },
							{ CUBE_FACES::CUBE_TOP,   CUBE_FACES::CUBE_BACK,  CUBE_FACES::CUBE_DOWN,  CUBE_FACES::CUBE_FRONT },
							{ CUBE_FACES::CUBE_TOP,   CUBE_FACES::CUBE_LEFT,  CUBE_FACES::CUBE_DOWN,  CUBE_FACES::CUBE_RIGHT },
							{ CUBE_FACES::CUBE_FRONT, CUBE_FACES::CUBE_RIGHT, CUBE_FACES::CUBE_BACK,  CUBE_FACES::CUBE_LEFT  }
						};

						// The directions from which each face is entered from when coming from the corresponding adjacent face
						const static FilterDirection ENTRY_DIRECTIONS[6][4] =
						{
							{ FilterDirection::Down,  FilterDirection::Down, FilterDirection::Down,  FilterDirection::Down  },
							{ FilterDirection::Left,  FilterDirection::Left, FilterDirection::Left,  FilterDirection::Right },
							{ FilterDirection::Up,    FilterDirection::Left, FilterDirection::Down,  FilterDirection::Right },
							{ FilterDirection::Right, FilterDirection::Left, FilterDirection::Right, FilterDirection::Right },
							{ FilterDirection::Down,  FilterDirection::Left, FilterDirection::Up,    FilterDirection::Right },
							{ FilterDirection::Up,    FilterDirection::Up,   FilterDirection::Up,    FilterDirection::Up    }
						};

						CUBE_FACES face_bl = face;
						CUBE_FACES face_br = face;
						CUBE_FACES face_tl = face;
						CUBE_FACES face_tr = face;

						FilterDirection in_dir_bl;
						FilterDirection in_dir_br;
						FilterDirection in_dir_tl;
						FilterDirection in_dir_tr;

						FilterDirection out_dir_bl;
						FilterDirection out_dir_br;
						FilterDirection out_dir_tl;
						FilterDirection out_dir_tr;

						// Check if any of the sample pixels crosses over onto another side of the cubemap
						if (r_boundary_crossed)
						{
							FilterDirection out_dir = FilterDirection::Right;

							out_dir_br = out_dir;
							out_dir_tr = out_dir;

							in_dir_br = ENTRY_DIRECTIONS[face_br][out_dir];
							in_dir_tr = ENTRY_DIRECTIONS[face_tr][out_dir];

							face_br = ADJACENT_FACES[face_br][out_dir];
							face_tr = ADJACENT_FACES[face_tr][out_dir];
						}

						if (t_boundary_crossed)
						{
							FilterDirection out_dir = FilterDirection::Up;

							out_dir_tl = out_dir;
							out_dir_tr = out_dir;

							in_dir_tl = ENTRY_DIRECTIONS[face_tl][out_dir];
							in_dir_tr = ENTRY_DIRECTIONS[face_tr][out_dir];

							face_tl = ADJACENT_FACES[face_tl][out_dir];
							face_tr = ADJACENT_FACES[face_tr][out_dir];
						}

						if (l_boundary_crossed)
						{
							FilterDirection out_dir = FilterDirection::Left;

							out_dir_bl = out_dir;
							out_dir_tl = out_dir;

							in_dir_bl = ENTRY_DIRECTIONS[face_bl][out_dir];
							in_dir_tl = ENTRY_DIRECTIONS[face_tl][out_dir];

							face_bl = ADJACENT_FACES[face_bl][out_dir];
							face_tl = ADJACENT_FACES[face_tl][out_dir];
						}

						if (b_boundary_crossed)
						{
							FilterDirection out_dir = FilterDirection::Down;

							out_dir_bl = out_dir;
							out_dir_br = out_dir;

							in_dir_bl = ENTRY_DIRECTIONS[face_bl][out_dir];
							in_dir_br = ENTRY_DIRECTIONS[face_br][out_dir];

							face_bl = ADJACENT_FACES[face_bl][out_dir];
							face_br = ADJACENT_FACES[face_br][out_dir];
						}

						// If any sample pixel crossed over to another cubemap face, calculate the correct pixel there, otherwise
						// get the correct pixel on this face
						rgba_buffer_index_bl =
							face_bl == face
							? ((face_y + 0) * num_frames * capture_frame_width + (face_x + 0) + face_bl * capture_frame_width) * 4
							: EdgePixelIndexFromDirs(out_dir_bl, in_dir_bl, face_bl, face_x + 0, face_y + 0, num_frames);
						rgba_buffer_index_br =
							face_br == face
							? ((face_y + 0) * num_frames * capture_frame_width + (face_x + 1) + face_br * capture_frame_width) * 4
							: EdgePixelIndexFromDirs(out_dir_br, in_dir_br, face_br, face_x + 1, face_y + 0, num_frames);
						rgba_buffer_index_tl =
							face_tl == face
							? ((face_y + 1) * num_frames * capture_frame_width + (face_x + 0) + face_tl * capture_frame_width) * 4
							: EdgePixelIndexFromDirs(out_dir_tl, in_dir_tl, face_tl, face_x + 0, face_y + 1, num_frames);
						rgba_buffer_index_tr =
							face_tr == face
							? ((face_y + 1) * num_frames * capture_frame_width + (face_x + 1) + face_tr * capture_frame_width) * 4
							: EdgePixelIndexFromDirs(out_dir_tr, in_dir_tr, face_tr, face_x + 1, face_y + 1, num_frames);
					}

					// Calculate the final interpolated color based on the weights and sampled pixels
					for (int color_channel = 0; color_channel < 4; color_channel++)
					{
						warped_frame[warped_buffer_index + color_channel] =
							weight_bl * rgba_frame[rgba_buffer_index_bl + color_channel] +
							weight_br * rgba_frame[rgba_buffer_index_br + color_channel] +
							weight_tl * rgba_frame[rgba_buffer_index_tl + color_channel] +
							weight_tr * rgba_frame[rgba_buffer_index_tr + color_channel];
					}
				}
			}
		});
}

int AVideoTransmitter::EdgePixelIndexFromDirs(const FilterDirection& out_dir, const FilterDirection& in_dir,
	const CUBE_FACES& face, const int& face_x, const int& face_y, const int& num_frames)
{
	// This function gets the correct texture edge pixel based on the "exit" and "entry" directions when crossing
	// the edge between two sides of the cubemap. This is needed to filter the cubemap edges correctly because
	// the cubemap sides are not always adjacent in the flat texture where they are stored

	int side_coord;

	// First map the pixel coordinates into a generic "side coordinate" that always goes clockwise along the texture edge
	switch (out_dir)
	{
	case FilterDirection::Down:
		side_coord = (face_x + capture_frame_width) % capture_frame_width;
		break;

	case FilterDirection::Right:
		side_coord = (face_y + capture_frame_height) % capture_frame_height;
		break;

	case FilterDirection::Up:
		side_coord = (capture_frame_width - (face_x + capture_frame_width) % capture_frame_width - 1);
		break;

	case FilterDirection::Left:
		side_coord = (capture_frame_height - (face_y + capture_frame_height) % capture_frame_height - 1);
		break;

	default:
		throw std::invalid_argument("Invalid exit direction");
	}

	// Then calculate the pixel index on the other face beyond the edge using the clockwise side coordinate
	switch (in_dir)
	{
	case FilterDirection::Down:
		return (0 + (capture_frame_width - side_coord - 1) + face * capture_frame_width) * 4;

	case FilterDirection::Right:
		return ((capture_frame_height - side_coord - 1) * num_frames * capture_frame_width + (capture_frame_width - 1) + face * capture_frame_width) * 4;

	case FilterDirection::Up:
		return ((capture_frame_height - 1) * num_frames * capture_frame_width + side_coord + face * capture_frame_width) * 4;

	case FilterDirection::Left:
		return (side_coord * num_frames * capture_frame_width + 0 + face * capture_frame_width) * 4;

	default:
		throw std::invalid_argument("Invalid entry direction");
	}
}

bool AVideoTransmitter::ShouldTickIfViewportsOnly() const
{
	return use_editor_tick;
}

void AVideoTransmitter::OpenDebugFile()
{
	FString full_filepath = FPaths::ProjectDir() + file_write_hevc_name;
	char* filepath = TCHAR_TO_ANSI(*full_filepath);

	// Visual Studio suggests to use fopen_s here but it doesn't exist on Linux, don't use it
	debug_outfile = fopen(filepath, "wb");

	if (!debug_outfile)
	{
		Debug::Log("failed to open debug file");
	}
}

void AVideoTransmitter::TransmitDebugMessage()
{
#ifndef CITHRUS_NO_KVAZAAR_OR_UVGRTP
	const char msg[] = "hello world";
	const int msg_len = strlen(msg) + 1;

	if (!stream)
	{
		Debug::Log("media stream is null, abort transmit");
		return;
	}

	stream->push_frame((uint8_t*)msg, msg_len, RTP_NO_FLAGS);

	Debug::Log("debug message transmitted");
#endif
}
