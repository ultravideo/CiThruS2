#pragma once

#include "Optional/Sse41.h"
#include "Pipeline/Internal/PipelineFilter.h"

// Converts YUV 4:2:0 images to RGBA
class CITHRUS_API YuvToRgbaConverter : public PipelineFilter<1, 1>
{
public:
	YuvToRgbaConverter(const uint16_t& frameWidth, const uint16_t& frameHeight, const std::string& format = "rgba");
	virtual ~YuvToRgbaConverter();

	virtual void Process() override;

protected:
	uint8_t* outputData_;

	uint16_t outputFrameWidth_;
	uint16_t outputFrameHeight_;

	void (YuvToRgbaConverter::*converterFunction_)(const uint8_t* input, uint8_t** output, int width, int height);

	// Fixed point coefficients from ITU-R BT.601
	const static int16_t R_V = 358;
	const static int16_t G_U = -88;
	const static int16_t G_V = -182;
	const static int16_t B_U = 452;

	const static int CHROMA_MIN = 0;
	const static int CHROMA_MID = 128;
	const static int CHROMA_MAX = 255;

	uint8_t rOffset_;
	uint8_t gOffset_;
	uint8_t bOffset_;

	bool initialized_;

	void YuvToRgbaDefault(const uint8_t* input, uint8_t** output, int width, int height);

#ifdef CITHRUS_SSE41_AVAILABLE
	const __m128i rV_ = _mm_set_epi32(R_V, R_V, R_V, R_V);
	const __m128i gU_ = _mm_set_epi32(G_U, G_U, G_U, G_U);
	const __m128i gV_ = _mm_set_epi32(G_V, G_V, G_V, G_V);
	const __m128i bU_ = _mm_set_epi32(B_U, B_U, B_U, B_U);

	const __m128i minVal_    = _mm_set_epi32(CHROMA_MIN, CHROMA_MIN, CHROMA_MIN, CHROMA_MIN);
	const __m128i middleVal_ = _mm_set_epi32(CHROMA_MID, CHROMA_MID, CHROMA_MID, CHROMA_MID);
	const __m128i maxVal_    = _mm_set_epi32(CHROMA_MAX, CHROMA_MAX, CHROMA_MAX, CHROMA_MAX);

	const __m128i lumaShuffleMask_   = _mm_set_epi8(-1, -1, -1, 3, -1, -1, -1, 2, -1, -1, -1, 1, -1, -1, -1, 0);
	const __m128i chromaShuffleMask_ = _mm_set_epi8(-1, -1, -1, 1, -1, -1, -1, 1, -1, -1, -1, 0, -1, -1, -1, 0);

	const __m128i aPix_ = _mm_set_epi8(-1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0);

	int rShift_;
	int gShift_;
	int bShift_;

	void YuvToRgbaSse41(const uint8_t* input, uint8_t** output, int width, int height);
#endif // CITHRUS_SSE41_AVAILABLE
};
