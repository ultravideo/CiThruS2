#include "RgbaToYuvConverter.h"

#ifdef CITHRUS_SSE41_AVAILABLE
#include <emmintrin.h>
#include <pmmintrin.h>
#include <smmintrin.h>
#endif // CITHRUS_SSE41_AVAILABLE

#include <algorithm>
#include <stdexcept>

RgbaToYuvConverter::RgbaToYuvConverter(const uint16_t& frameWidth, const uint16_t& frameHeight)
	: outputFrameWidth_(frameWidth), outputFrameHeight_(frameHeight)
{
    uint32_t outputSize = outputFrameWidth_ * outputFrameHeight_ * 3 / 2;
    outputData_ = new uint8_t[outputSize];

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

	RgbaToYuvSse41(inputData, &outputData_, outputFrameWidth_, outputFrameHeight_);
}

void RgbaToYuvConverter::RgbaToYuvSse41(const uint8_t* input, uint8_t** output, int width, int height)
{
#ifdef CITHRUS_SSE41_AVAILABLE
    // This efficiently converts pixels from RGBA to YUV 4:2:0 by using SSE 4.1 instructions to process multiple values simultaneously

    const int r_mul_y[4] = {  76,  76,  76,  76 };
    const int r_mul_u[4] = { -43, -43, -43, -43 };
    const int r_mul_v[4] = { 127, 127, 127, 127 };

    const int g_mul_y[4] = {  150,  150,  150,  150 };
    const int g_mul_u[4] = {  -84,  -84,  -84,  -84 };
    const int g_mul_v[4] = { -106, -106, -106, -106 };

    const int b_mul_y[4] = {  29,  29,  29,  29 };
    const int b_mul_u[4] = { 127, 127, 127, 127 };
    const int b_mul_v[4] = { -21, -21, -21, -21 };

    const __m128i r_y = _mm_loadu_si128((__m128i const*)r_mul_y);
    const __m128i r_u = _mm_loadu_si128((__m128i const*)r_mul_u);
    const __m128i r_v = _mm_loadu_si128((__m128i const*)r_mul_v);

    const __m128i g_y = _mm_loadu_si128((__m128i const*)g_mul_y);
    const __m128i g_u = _mm_loadu_si128((__m128i const*)g_mul_u);
    const __m128i g_v = _mm_loadu_si128((__m128i const*)g_mul_v);

    const __m128i b_y = _mm_loadu_si128((__m128i const*)b_mul_y);
    const __m128i b_u = _mm_loadu_si128((__m128i const*)b_mul_u);
    const __m128i b_v = _mm_loadu_si128((__m128i const*)b_mul_v);

    const int mini[4] = { 0, 0, 0, 0 };
    const int maxi[4] = { 255, 255, 255, 255 };

    const int chroma_offset[4] = { 255 * 255, 255 * 255, 255 * 255, 255 * 255 };

    const uint8_t* in = input;

    const __m128i min_val = _mm_loadu_si128((__m128i const*)mini);
    const __m128i max_val = _mm_loadu_si128((__m128i const*)maxi);
    const __m128i cr_offset = _mm_loadu_si128((__m128i const*)chroma_offset);

    uint8_t* out_y = &(*output)[0];
    uint8_t* out_u = &(*output)[width * height];
    uint8_t* out_v = &(*output)[width * height * 5 / 4];

    __m128i shufflemask_r;
    __m128i shufflemask_g;
    __m128i shufflemask_b;

    std::string inputFormat = GetInputPin<0>().GetFormat();

    if (inputFormat == "rgba")
    {
        shufflemask_r = _mm_set_epi8(-1, -1, -1, 12, -1, -1, -1, 8, -1, -1, -1, 4, -1, -1, -1, 0);
        shufflemask_g = _mm_set_epi8(-1, -1, -1, 13, -1, -1, -1, 9, -1, -1, -1, 5, -1, -1, -1, 1);
        shufflemask_b = _mm_set_epi8(-1, -1, -1, 14, -1, -1, -1, 10, -1, -1, -1, 6, -1, -1, -1, 2);
    }
    else if (inputFormat == "bgra")
    {
        shufflemask_r = _mm_set_epi8(-1, -1, -1, 14, -1, -1, -1, 10, -1, -1, -1, 6, -1, -1, -1, 2);
        shufflemask_g = _mm_set_epi8(-1, -1, -1, 13, -1, -1, -1, 9, -1, -1, -1, 5, -1, -1, -1, 1);
        shufflemask_b = _mm_set_epi8(-1, -1, -1, 12, -1, -1, -1, 8, -1, -1, -1, 4, -1, -1, -1, 0);
    }
    else
    {
        throw std::exception("Unsupported format");
    }

    __m128i shufflemask = _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 12, 8, 4, 0);
    __m128i shufflemask_chroma = _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 8, 0);

    // Calculate Y 4 pixels at a time
    for (int32_t i = 0; i < width * height << 2; i += 16)
    {
        // Load 16 bytes (4 pixels)
        const __m128i a = _mm_loadu_si128((__m128i const*)in);
        in += 16;

        __m128i r = _mm_shuffle_epi8(a, shufflemask_r);
        __m128i g = _mm_shuffle_epi8(a, shufflemask_g);
        __m128i b = _mm_shuffle_epi8(a, shufflemask_b);

        __m128i res_y = _mm_max_epi32(min_val, _mm_min_epi32(max_val, _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(_mm_mullo_epi32(r, r_y), _mm_mullo_epi32(g, g_y)), _mm_mullo_epi32(b, b_y)), 8)));
        __m128i temp_y = _mm_shuffle_epi8(res_y, shufflemask);

        memcpy(out_y, &temp_y, 4);
        out_y += 4;
    }

    // Calculate U and V 8 pixels at a time
    for (int i = 0; i < width * height * 4; i += 2 * width * 4)
    {
        for (int j = i; j < i + width * 4; j += 4 * 4)
        {
            const __m128i a = _mm_loadu_si128((__m128i const*) & input[j]);
            const __m128i a2 = _mm_loadu_si128((__m128i const*) & input[j + width * 4]);

            __m128i r = _mm_shuffle_epi8(a, shufflemask_r);
            __m128i g = _mm_shuffle_epi8(a, shufflemask_g);
            __m128i b = _mm_shuffle_epi8(a, shufflemask_b);

            __m128i r2 = _mm_shuffle_epi8(a2, shufflemask_r);
            __m128i g2 = _mm_shuffle_epi8(a2, shufflemask_g);
            __m128i b2 = _mm_shuffle_epi8(a2, shufflemask_b);

            __m128i r_sum = _mm_add_epi32(r, r2);
            __m128i g_sum = _mm_add_epi32(g, g2);
            __m128i b_sum = _mm_add_epi32(b, b2);

            __m128i res_u_b = _mm_add_epi32(_mm_add_epi32(_mm_add_epi32(_mm_mullo_epi32(r_sum, r_u), _mm_mullo_epi32(g_sum, g_u)), _mm_mullo_epi32(b_sum, b_u)), cr_offset);
            __m128i res_u_tmp = _mm_max_epi32(min_val, _mm_min_epi32(max_val, _mm_srai_epi32(res_u_b, 9)));

            __m128i res_u = _mm_srai_epi32(_mm_add_epi32(_mm_shuffle_epi32(res_u_tmp, 0xb1), res_u_tmp), 1);
            __m128i temp_u = _mm_shuffle_epi8(res_u, shufflemask_chroma);
            memcpy(out_u, &temp_u, 2);
            out_u += 2;

            __m128i res_v_b = _mm_add_epi32(_mm_add_epi32(_mm_add_epi32(_mm_mullo_epi32(r_sum, r_v), _mm_mullo_epi32(g_sum, g_v)), _mm_mullo_epi32(b_sum, b_v)), cr_offset);
            __m128i res_v_tmp = _mm_max_epi32(min_val, _mm_min_epi32(max_val, _mm_srai_epi32(res_v_b, 9)));

            __m128i res_v = _mm_srai_epi32(_mm_add_epi32(_mm_shuffle_epi32(res_v_tmp, 0xb1), res_v_tmp), 1);
            __m128i temp_v = _mm_shuffle_epi8(res_v, shufflemask_chroma);
            memcpy(out_v, &temp_v, 2);
            out_v += 2;
        }
    }

#endif // CITHRUS_SSE41_AVAILABLE
}
