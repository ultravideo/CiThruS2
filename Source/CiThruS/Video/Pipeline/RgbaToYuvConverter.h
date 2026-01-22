#pragma once

#include "Optional/Sse41.h"
#include "PipelineFilter.h"

// Converts RGBA or ABGR images to YUV 4:2:0
class CITHRUS_API RgbaToYuvConverter : public PipelineFilter<1, 1>
{
public:
	RgbaToYuvConverter(const uint16_t& frameWidth, const uint16_t& frameHeight);
	virtual ~RgbaToYuvConverter();

	virtual void Process() override;

protected:
	uint8_t* outputData_;
	uint32_t outputSize_;

	uint16_t outputFrameWidth_;
	uint16_t outputFrameHeight_;

    void (RgbaToYuvConverter::*converterFunction_)(const uint8_t* input, uint8_t** output, int width, int height);

    // Fixed point coefficients from ITU-R BT.601
    const static int16_t R_Y =   76;
    const static int16_t R_U =  -43;
    const static int16_t R_V =  128;

    const static int16_t G_Y =  150;
    const static int16_t G_U =  -84;
    const static int16_t G_V = -106;

    const static int16_t B_Y =   29;
    const static int16_t B_U =  128;
    const static int16_t B_V =  -21;

    const static int CHROMA_MIN = 0;
    const static int CHROMA_MID = 128;
    const static int CHROMA_MAX = 255;

    uint8_t rOffset_;
    uint8_t gOffset_;
    uint8_t bOffset_;

    bool initialized_;

    void RgbaToYuvDefault(const uint8_t* input, uint8_t** output, int width, int height);

#ifdef CITHRUS_SSE41_AVAILABLE
    const __m128i rY_ = _mm_set_epi32(R_Y, R_Y, R_Y, R_Y);
    const __m128i rU_ = _mm_set_epi32(R_U, R_U, R_U, R_U);
    const __m128i rV_ = _mm_set_epi32(R_V, R_V, R_V, R_V);

    const __m128i gY_ = _mm_set_epi32(G_Y, G_Y, G_Y, G_Y);
    const __m128i gU_ = _mm_set_epi32(G_U, G_U, G_U, G_U);
    const __m128i gV_ = _mm_set_epi32(G_V, G_V, G_V, G_V);

    const __m128i bY_ = _mm_set_epi32(B_Y, B_Y, B_Y, B_Y);
    const __m128i bU_ = _mm_set_epi32(B_U, B_U, B_U, B_U);
    const __m128i bV_ = _mm_set_epi32(B_V, B_V, B_V, B_V);

    const __m128i minVal_ = _mm_set_epi32(CHROMA_MIN,      CHROMA_MIN,      CHROMA_MIN,      CHROMA_MIN);
    const __m128i midVal_ = _mm_set_epi32(CHROMA_MID << 9, CHROMA_MID << 9, CHROMA_MID << 9, CHROMA_MID << 9);
    const __m128i maxVal_ = _mm_set_epi32(CHROMA_MAX,      CHROMA_MAX,      CHROMA_MAX,      CHROMA_MAX);

    const __m128i shuffleMask_       = _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 12,  8,  4,  0);
    const __m128i shuffleMaskChroma_ = _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  8,  0);

    __m128i rShuffleMask_;
    __m128i gShuffleMask_;
    __m128i bShuffleMask_;

    void RgbaToYuvSse41(const uint8_t* input, uint8_t** output, int width, int height);
#endif // CITHRUS_SSE41_AVAILABLE
};
