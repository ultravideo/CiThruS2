#include "VideoStreaming/HevcEncoder.h"
#include "Misc/Debug.h"

HevcEncoder::HevcEncoder(const uint16_t& frameWidth, const uint16_t& frameHeight, const uint8_t& threadCount, const uint8_t& qp, const uint8_t& wpp, const uint8_t& owf)
	: frameWidth_(frameWidth), frameHeight_(frameHeight), inputFrame_(nullptr), outputFrame_(nullptr), outputSize_(0)
{
#ifdef CITHRUS_KVAZAAR_AVAILABLE
	// Set up Kvazaar for encoding
	kvazaarConfig_ = kvazaarApi_->config_alloc();

	kvazaarApi_->config_init(kvazaarConfig_);
	kvazaarApi_->config_parse(kvazaarConfig_, "preset", "ultrafast");
	kvazaarApi_->config_parse(kvazaarConfig_, "gop", "lp-g8d3t1");
	kvazaarApi_->config_parse(kvazaarConfig_, "vps-period", "1");
	kvazaarApi_->config_parse(kvazaarConfig_, "force-level", "4");
	kvazaarApi_->config_parse(kvazaarConfig_, "threads", std::to_string(threadCount).c_str());
	//kvazaar_api->config_parse(kvazaarConfig_, "intra_period", "16");
	//kvazaar_api->config_parse(kvazaarConfig_, "period", "64");
	//kvazaar_api->config_parse(kvazaarConfig_, "tiles", "3x3");
	//kvazaar_api->config_parse(kvazaarConfig_, "slices", "tiles");
	//kvazaar_api->config_parse(kvazaarConfig_, "mv-constraint", "frametilemargin");
	//kvazaar_api->config_parse(kvazaarConfig_, "slices", "wpp");

	kvazaarConfig_->width = frameWidth;
	kvazaarConfig_->height = frameHeight;
	kvazaarConfig_->hash = KVZ_HASH_NONE;
	kvazaarConfig_->qp = qp;
	kvazaarConfig_->wpp = wpp;
	kvazaarConfig_->owf = owf;
	//kvazaarConfig_->bipred = 0;
	//kvazaarConfig_->intra_period = 16;
	kvazaarConfig_->aud_enable = 0;
	kvazaarConfig_->calc_psnr = 0;

	kvazaarEncoder_ = kvazaarApi_->encoder_open(kvazaarConfig_);
	kvazaarTransmitPicture_ = kvazaarApi_->picture_alloc(frameWidth, frameHeight);
#endif // CITHRUS_KVAZAAR_AVAILABLE
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

	delete[] outputFrame_;
	outputFrame_ = nullptr;
}

void HevcEncoder::Process()
{
	uint8_t* yuvFrame = *inputFrame_;

#ifdef CITHRUS_KVAZAAR_AVAILABLE
	memcpy(kvazaarTransmitPicture_->y, yuvFrame, frameWidth_ * frameHeight_);
	yuvFrame += frameWidth_ * frameHeight_;
	memcpy(kvazaarTransmitPicture_->u, yuvFrame, frameWidth_ * frameHeight_ / 4);
	yuvFrame += frameWidth_ * frameHeight_ / 4;
	memcpy(kvazaarTransmitPicture_->v, yuvFrame, frameWidth_ * frameHeight_ / 4);

	kvz_picture* recon_pic = nullptr;
	kvz_frame_info frame_info;
	kvz_data_chunk* data_out = nullptr;
	uint32_t len_out = 0;

	kvazaarApi_->encoder_encode(kvazaarEncoder_, kvazaarTransmitPicture_,
		&data_out, &len_out,
		nullptr, nullptr,
		&frame_info);

	delete[] outputFrame_;

	if (!data_out)
	{
		outputFrame_ = nullptr;
		outputSize_ = 0;

		return;
	}

	outputFrame_ = new uint8_t[len_out];
	uint8_t* data_ptr = outputFrame_;

	kvz_data_chunk* previous_chunk = 0;
	for (kvz_data_chunk* chunk = data_out; chunk != nullptr; chunk = chunk->next)
	{
		memcpy(data_ptr, chunk->data, chunk->len);
		data_ptr += chunk->len;
		previous_chunk = chunk;
	}
	kvazaarApi_->chunk_free(data_out);

	kvazaarApi_->picture_free(kvazaarTransmitPicture_);
	kvazaarTransmitPicture_ = kvazaarApi_->picture_alloc(frameWidth_, frameHeight_);

	outputSize_ = len_out;
#endif // CITHRUS_KVAZAAR_AVAILABLE
}
