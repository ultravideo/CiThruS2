#include "RgbaToYuvConverter.h"

#ifdef CITHRUS_SSE41_AVAILABLE
#include <emmintrin.h>
#include <pmmintrin.h>
#include <smmintrin.h>
#endif // CITHRUS_SSE41_AVAILABLE

#include <algorithm>
#include <stdexcept>

RgbaToYuvConverter::RgbaToYuvConverter(const uint16_t& frameWidth, const uint16_t& frameHeight)
	: outputFrameWidth_(frameWidth), outputFrameHeight_(frameHeight), converterFunction_(nullptr), initialized_(false)
{
    uint32_t outputSize = outputFrameWidth_ * outputFrameHeight_ * 3 / 2;
    outputData_ = new uint8_t[outputSize];

    if (outputFrameWidth_ % 2 != 0 || outputFrameHeight_ % 2 != 0)
    {
        throw std::runtime_error("The width and height of YUV 4:2:0 data must be divisible by 2");
    }

#ifdef CITHRUS_SSE41_AVAILABLE
    if (converterFunction_ == nullptr && frameWidth % 16 == 0 && frameHeight % 16 == 0)
    {
        converterFunction_ = &RgbaToYuvConverter::RgbaToYuvSse41;
    }
#endif // CITHRUS_SSE41_AVAILABLE

    if (converterFunction_ == nullptr)
    {
        converterFunction_ = &RgbaToYuvConverter::RgbaToYuvDefault;
    }

    GetInputPin<0>().SetAcceptedFormats({ "rgba", "bgra" });
    GetOutputPin<0>().SetFormat("yuv420");

    GetOutputPin<0>().SetData(outputData_);
    GetOutputPin<0>().SetSize(outputSize);
}

RgbaToYuvConverter::~RgbaToYuvConverter()
{
	delete[] outputData_;
	outputData_ = nullptr;

    GetOutputPin<0>().SetData(nullptr);
    GetOutputPin<0>().SetSize(0);
}

void RgbaToYuvConverter::Process()
{
    const uint8_t* inputData = GetInputPin<0>().GetData();
    uint32_t inputSize = GetInputPin<0>().GetSize();

    if (!inputData || inputSize != outputFrameWidth_ * outputFrameHeight_ * 4)
    {
        return;
    }

	(this->*converterFunction_)(inputData, &outputData_, outputFrameWidth_, outputFrameHeight_);
}

void RgbaToYuvConverter::RgbaToYuvDefault(const uint8_t* input, uint8_t** output, int width, int height)
{
    if (!initialized_)
    {
        initialized_ = true;

        std::string inputFormat = GetInputPin<0>().GetFormat();

        if (inputFormat == "rgba")
        {
            rOffset_ = 0;
            gOffset_ = 1;
            bOffset_ = 2;
        }
        else if (inputFormat == "bgra")
        {
            rOffset_ = 2;
            gOffset_ = 1;
            bOffset_ = 0;
        }
        else
        {
            throw std::runtime_error("Unsupported format");
        }
    }

    for (int i = 0; i < height / 2; i++)
    {
        for (int j = 0; j < width / 2; j++)
        {
            const std::array<uint8_t, 4>& rgbaTopLeft     = *(reinterpret_cast<const std::array<uint8_t, 4>*>(input) + ((i * 2 + 0) * width + (j * 2 + 0)));
            const std::array<uint8_t, 4>& rgbaTopRight    = *(reinterpret_cast<const std::array<uint8_t, 4>*>(input) + ((i * 2 + 0) * width + (j * 2 + 1)));
            const std::array<uint8_t, 4>& rgbaBottomLeft  = *(reinterpret_cast<const std::array<uint8_t, 4>*>(input) + ((i * 2 + 1) * width + (j * 2 + 0)));
            const std::array<uint8_t, 4>& rgbaBottomRight = *(reinterpret_cast<const std::array<uint8_t, 4>*>(input) + ((i * 2 + 1) * width + (j * 2 + 1)));

            // Calculate Y for 4 pixels in a square
            int32_t yTopLeft     = R_Y * rgbaTopLeft[rOffset_]     + G_Y * rgbaTopLeft[gOffset_]     + B_Y * rgbaTopLeft[bOffset_];
            int32_t yTopRight    = R_Y * rgbaTopRight[rOffset_]    + G_Y * rgbaTopRight[gOffset_]    + B_Y * rgbaTopRight[bOffset_];
            int32_t yBottomLeft  = R_Y * rgbaBottomLeft[rOffset_]  + G_Y * rgbaBottomLeft[gOffset_]  + B_Y * rgbaBottomLeft[bOffset_];
            int32_t yBottomRight = R_Y * rgbaBottomRight[rOffset_] + G_Y * rgbaBottomRight[gOffset_] + B_Y * rgbaBottomRight[bOffset_];

            // Fixed point multiplication
            (*output)[(i * 2 + 0) * width + (j * 2 + 0)] = std::max(CHROMA_MIN, std::min(CHROMA_MAX, yTopLeft     >> 8));
            (*output)[(i * 2 + 0) * width + (j * 2 + 1)] = std::max(CHROMA_MIN, std::min(CHROMA_MAX, yTopRight    >> 8));
            (*output)[(i * 2 + 1) * width + (j * 2 + 0)] = std::max(CHROMA_MIN, std::min(CHROMA_MAX, yBottomLeft  >> 8));
            (*output)[(i * 2 + 1) * width + (j * 2 + 1)] = std::max(CHROMA_MIN, std::min(CHROMA_MAX, yBottomRight >> 8));

            // Average U and V from 4 pixels in a square
            int16_t rSum = rgbaTopLeft[rOffset_] + rgbaTopRight[rOffset_] + rgbaBottomLeft[rOffset_] + rgbaBottomRight[rOffset_];
            int16_t gSum = rgbaTopLeft[gOffset_] + rgbaTopRight[gOffset_] + rgbaBottomLeft[gOffset_] + rgbaBottomRight[gOffset_];
            int16_t bSum = rgbaTopLeft[bOffset_] + rgbaTopRight[bOffset_] + rgbaBottomLeft[bOffset_] + rgbaBottomRight[bOffset_];

            // Fixed point multiplication
            (*output)[i * width / 2 + j + width * height]         = std::max(CHROMA_MIN, std::min(CHROMA_MAX, (R_U * rSum + G_U * gSum + B_U * bSum + (CHROMA_MID << 10)) >> 10));
            (*output)[i * width / 2 + j + width * height * 5 / 4] = std::max(CHROMA_MIN, std::min(CHROMA_MAX, (R_V * rSum + G_V * gSum + B_V * bSum + (CHROMA_MID << 10)) >> 10));
        }
    }
}

#ifdef CITHRUS_SSE41_AVAILABLE
void RgbaToYuvConverter::RgbaToYuvSse41(const uint8_t* input, uint8_t** output, int width, int height)
{
    // This efficiently converts pixels from RGBA to YUV 4:2:0 by using SSE 4.1 instructions to process multiple values simultaneously

    const __m128i* in = reinterpret_cast<const __m128i*>(input);

    if (!initialized_)
    {
        initialized_ = true;

        std::string inputFormat = GetInputPin<0>().GetFormat();

        if (inputFormat == "rgba")
        {
            rShuffleMask_ = _mm_set_epi8(-1, -1, -1, 12, -1, -1, -1,  8, -1, -1, -1, 4, -1, -1, -1, 0);
            gShuffleMask_ = _mm_set_epi8(-1, -1, -1, 13, -1, -1, -1,  9, -1, -1, -1, 5, -1, -1, -1, 1);
            bShuffleMask_ = _mm_set_epi8(-1, -1, -1, 14, -1, -1, -1, 10, -1, -1, -1, 6, -1, -1, -1, 2);
        }
        else if (inputFormat == "bgra")
        {
            rShuffleMask_ = _mm_set_epi8(-1, -1, -1, 14, -1, -1, -1, 10, -1, -1, -1, 6, -1, -1, -1, 2);
            gShuffleMask_ = _mm_set_epi8(-1, -1, -1, 13, -1, -1, -1,  9, -1, -1, -1, 5, -1, -1, -1, 1);
            bShuffleMask_ = _mm_set_epi8(-1, -1, -1, 12, -1, -1, -1,  8, -1, -1, -1, 4, -1, -1, -1, 0);
        }
        else
        {
            throw std::runtime_error("Unsupported format");
        }
    }

    uint8_t* yOut = *output;
    uint8_t* uOut = *output + width * height;
    uint8_t* vOut = *output + width * height * 5 / 4;

    // Calculate Y 4 pixels at a time, 4x1 rectangle
    for (int i = 0; i < width * height / 4; i++)
    {
        // Load 16 bytes (4 pixels)
        const __m128i a = _mm_loadu_si128(in);
        in++;

        __m128i r = _mm_shuffle_epi8(a, rShuffleMask_);
        __m128i g = _mm_shuffle_epi8(a, gShuffleMask_);
        __m128i b = _mm_shuffle_epi8(a, bShuffleMask_);

        // Fixed point multiplication
        __m128i yRes = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(_mm_mullo_epi32(r, rY_), _mm_mullo_epi32(g, gY_)), _mm_mullo_epi32(b, bY_)), 8);
        __m128i yTemp = _mm_shuffle_epi8(_mm_max_epi32(minVal_, _mm_min_epi32(maxVal_, yRes)), shuffleMask_);

        memcpy(yOut, &yTemp, 4);
        yOut += 4;
    }

    // Calculate U and V 8 pixels at a time, 4x2 rectangle
    for (int i = 0; i < height / 2; i++)
    {
        for (int j = 0; j < width / 4; j++)
        {
            const __m128i a = _mm_loadu_si128(reinterpret_cast<__m128i const*>(input) + i * width / 2 + j);
            const __m128i a2 = _mm_loadu_si128(reinterpret_cast<__m128i const*>(input) + i * width / 2 + j + width / 4);

            __m128i r = _mm_shuffle_epi8(a, rShuffleMask_);
            __m128i g = _mm_shuffle_epi8(a, gShuffleMask_);
            __m128i b = _mm_shuffle_epi8(a, bShuffleMask_);

            __m128i r2 = _mm_shuffle_epi8(a2, rShuffleMask_);
            __m128i g2 = _mm_shuffle_epi8(a2, gShuffleMask_);
            __m128i b2 = _mm_shuffle_epi8(a2, bShuffleMask_);

            // Sum vertically
            __m128i rSum = _mm_add_epi32(r, r2);
            __m128i gSum = _mm_add_epi32(g, g2);
            __m128i bSum = _mm_add_epi32(b, b2);

            // Fixed point multiplication
            __m128i uResB = _mm_add_epi32(_mm_add_epi32(_mm_add_epi32(_mm_mullo_epi32(rSum, rU_), _mm_mullo_epi32(gSum, gU_)), _mm_mullo_epi32(bSum, bU_)), midVal_);
            __m128i uResTemp = _mm_max_epi32(minVal_, _mm_min_epi32(maxVal_, _mm_srai_epi32(uResB, 9)));

            // Sum horizontally
            __m128i uRes = _mm_srai_epi32(_mm_add_epi32(_mm_shuffle_epi32(uResTemp, 0xB1), uResTemp), 1);
            __m128i uTemp = _mm_shuffle_epi8(uRes, shuffleMaskChroma_);

            memcpy(uOut, &uTemp, 2);
            uOut += 2;

            // Fixed point multiplication
            __m128i vResB = _mm_add_epi32(_mm_add_epi32(_mm_add_epi32(_mm_mullo_epi32(rSum, rV_), _mm_mullo_epi32(gSum, gV_)), _mm_mullo_epi32(bSum, bV_)), midVal_);
            __m128i vResTemp = _mm_max_epi32(minVal_, _mm_min_epi32(maxVal_, _mm_srai_epi32(vResB, 9)));

            // Sum horizontally
            __m128i vRes = _mm_srai_epi32(_mm_add_epi32(_mm_shuffle_epi32(vResTemp, 0xB1), vResTemp), 1);
            __m128i vTemp = _mm_shuffle_epi8(vRes, shuffleMaskChroma_);

            memcpy(vOut, &vTemp, 2);
            vOut += 2;
        }
    }
}
#endif // CITHRUS_SSE41_AVAILABLE
