#include "HevcEncoder.h"
#include "Misc/Debug.h"

const uint64_t KVAZAAR_FRAMERATE_DENOM = 90000;

HevcEncoder::HevcEncoder(const uint16_t& frameWidth, const uint16_t& frameHeight, const uint8_t& threadCount, const uint8_t& qp, const uint8_t& wpp, const uint8_t& owf, const HevcEncoderPreset& preset)
	: frameWidth_(frameWidth), frameHeight_(frameHeight), outputData_(nullptr), startTime_(std::chrono::high_resolution_clock::time_point::min())
{
#ifdef CITHRUS_KVAZAAR_AVAILABLE
	// Set up Kvazaar for encoding
	kvazaarConfig_ = kvazaarApi_->config_alloc();

	kvazaarApi_->config_init(kvazaarConfig_);
	kvazaarApi_->config_parse(kvazaarConfig_, "threads", std::to_string(threadCount).c_str());
	//kvazaarApi_->config_parse(kvazaarConfig_, "force-level", "4");
	//kvazaar_api->config_parse(kvazaarConfig_, "intra_period", "16");
	//kvazaar_api->config_parse(kvazaarConfig_, "period", "64");
	//kvazaar_api->config_parse(kvazaarConfig_, "tiles", "3x3");
	//kvazaar_api->config_parse(kvazaarConfig_, "slices", "tiles");
	//kvazaar_api->config_parse(kvazaarConfig_, "mv-constraint", "frametilemargin");
	//kvazaar_api->config_parse(kvazaarConfig_, "slices", "wpp");

	switch (preset)
	{
	case HevcPresetMinimumLatency:
		kvazaarApi_->config_parse(kvazaarConfig_, "preset", "ultrafast");
		kvazaarApi_->config_parse(kvazaarConfig_, "gop", "lp-g8d1t1");
		kvazaarApi_->config_parse(kvazaarConfig_, "vps-period", "16");
		kvazaarApi_->config_parse(kvazaarConfig_, "sao", "off");
		//kvazaarApi_->config_parse(kvazaarConfig_, "slices", "wpp");

		kvazaarConfig_->qp = qp;
		kvazaarConfig_->wpp = wpp;
		kvazaarConfig_->owf = owf;
		kvazaarConfig_->deblock_enable = false;
		kvazaarConfig_->sao_type = KVZ_SAO_OFF;
		kvazaarConfig_->bipred = false;
		//kvazaarConfig_->intra_period = 16;
		break;

	case HevcPresetLossless:
		kvazaarConfig_->lossless = 1;
		kvazaarConfig_->wpp = wpp;
		kvazaarConfig_->owf = owf;
		break;
	}

	kvazaarConfig_->width = frameWidth;
	kvazaarConfig_->height = frameHeight;
	kvazaarConfig_->hash = KVZ_HASH_NONE;
	/*kvazaarConfig_->framerate_num = 1;
	kvazaarConfig_->framerate_denom = KVAZAAR_FRAMERATE_DENOM;*/

	kvazaarConfig_->aud_enable = 0;
	kvazaarConfig_->calc_psnr = 0;

	kvazaarEncoder_ = kvazaarApi_->encoder_open(kvazaarConfig_);
	kvazaarTransmitPicture_ = kvazaarApi_->picture_alloc(frameWidth_, frameHeight_);
#endif // CITHRUS_KVAZAAR_AVAILABLE

	GetInputPin<0>().Initialize(this, "yuv420");
	GetOutputPin<0>().Initialize(this, "hevc");
}

HevcEncoder::~HevcEncoder()
{
#ifdef CITHRUS_KVAZAAR_AVAILABLE
	kvazaarApi_->config_destroy(kvazaarConfig_);
	kvazaarApi_->encoder_close(kvazaarEncoder_);
	kvazaarApi_->picture_free(kvazaarTransmitPicture_);

	kvazaarConfig_ = nullptr;
	kvazaarEncoder_ = nullptr;
	kvazaarTransmitPicture_ = nullptr;
#endif // CITHRUS_KVAZAAR_AVAILABLE

	delete[] outputData_;
	outputData_ = nullptr;
}

void HevcEncoder::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	uint32_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData || inputSize == 0)
	{
		return;
	}

	std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();

	if (startTime_ == std::chrono::high_resolution_clock::time_point::min())
	{
		startTime_ = now;
	}

	const uint8_t* yuvFrame = inputData;

#ifdef CITHRUS_KVAZAAR_AVAILABLE
	memcpy(kvazaarTransmitPicture_->y, yuvFrame, frameWidth_ * frameHeight_);
	yuvFrame += frameWidth_ * frameHeight_;
	memcpy(kvazaarTransmitPicture_->u, yuvFrame, frameWidth_ * frameHeight_ / 4);
	yuvFrame += frameWidth_ * frameHeight_ / 4;
	memcpy(kvazaarTransmitPicture_->v, yuvFrame, frameWidth_ * frameHeight_ / 4);

	// TODO: Something about this doesn't work, causes choppy video. Probably dts since it affects the decoding
	/*kvazaarTransmitPicture_->pts = ((now - startTime_).count() * KVAZAAR_FRAMERATE_DENOM) / 1000000000ll;
	kvazaarTransmitPicture_->dts = kvazaarTransmitPicture_->pts;*/

	kvz_frame_info frame_info;
	kvz_data_chunk* data_out = nullptr;
	uint32_t len_out = 0;

	kvazaarApi_->encoder_encode(kvazaarEncoder_, kvazaarTransmitPicture_,
		&data_out, &len_out,
		nullptr, nullptr,
		&frame_info);

	delete[] outputData_;

	if (!data_out)
	{
		outputData_ = nullptr;

		GetOutputPin<0>().SetData(outputData_);
		GetOutputPin<0>().SetSize(0);

		return;
	}

	outputData_ = new uint8_t[len_out];
	uint8_t* data_ptr = outputData_;

	for (kvz_data_chunk* chunk = data_out; chunk != nullptr; chunk = chunk->next)
	{
		memcpy(data_ptr, chunk->data, chunk->len);
		data_ptr += chunk->len;
	}

	kvazaarApi_->chunk_free(data_out);

	kvazaarApi_->picture_free(kvazaarTransmitPicture_);
	kvazaarTransmitPicture_ = kvazaarApi_->picture_alloc(frameWidth_, frameHeight_);

	GetOutputPin<0>().SetData(outputData_);
	GetOutputPin<0>().SetSize(len_out);
#endif // CITHRUS_KVAZAAR_AVAILABLE
}
