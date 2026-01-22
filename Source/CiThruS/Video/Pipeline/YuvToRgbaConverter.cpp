#include "YuvToRgbaConverter.h"

#ifdef CITHRUS_SSE41_AVAILABLE
#include <emmintrin.h>
#include <pmmintrin.h>
#include <smmintrin.h>
#endif // CITHRUS_SSE41_AVAILABLE

#include <algorithm>
#include <stdexcept>
#include <array>

YuvToRgbaConverter::YuvToRgbaConverter(const uint16_t& frameWidth, const uint16_t& frameHeight, const std::string& format)
    : outputFrameWidth_(frameWidth), outputFrameHeight_(frameHeight), converterFunction_(nullptr), initialized_(false)
{
    uint32_t outputSize = outputFrameWidth_ * outputFrameHeight_ * 4;
    outputData_ = new uint8_t[outputSize];

    if (format != "rgba" && format != "bgra")
    {
        throw std::invalid_argument("Unsupported output format: " + format);
    }

    if (outputFrameWidth_ % 2 != 0 || outputFrameHeight_ % 2 != 0)
    {
        throw std::runtime_error("The width and height of YUV 4:2:0 data must be divisible by 2");
    }

#ifdef CITHRUS_SSE41_AVAILABLE
    if (converterFunction_ == nullptr && frameWidth % 16 == 0 && frameHeight % 16 == 0)
    {
        converterFunction_ = &YuvToRgbaConverter::YuvToRgbaSse41;
    }
#endif // CITHRUS_SSE41_AVAILABLE

    if (converterFunction_ == nullptr)
    {
        converterFunction_ = &YuvToRgbaConverter::YuvToRgbaDefault;
    }

    GetInputPin<0>().SetAcceptedFormat("yuv420");
    GetOutputPin<0>().SetFormat(format);

    GetOutputPin<0>().SetData(outputData_);
    GetOutputPin<0>().SetSize(outputSize);
}

YuvToRgbaConverter::~YuvToRgbaConverter()
{
	delete[] outputData_;
	outputData_ = nullptr;

    GetOutputPin<0>().SetData(nullptr);
    GetOutputPin<0>().SetSize(0);
}

void YuvToRgbaConverter::Process()
{
    const uint8_t* inputData = GetInputPin<0>().GetData();
    size_t inputSize = GetInputPin<0>().GetSize();

    if (!inputData || inputSize != outputFrameWidth_ * outputFrameHeight_ * 3 / 2)
    {
        return;
    }

    (this->*converterFunction_)(inputData, &outputData_, outputFrameWidth_, outputFrameHeight_);
}

void YuvToRgbaConverter::YuvToRgbaDefault(const uint8_t* input, uint8_t** output, int width, int height)
{
    if (!initialized_)
    {
        initialized_ = true;

        std::string outputFormat = GetOutputPin<0>().GetFormat();

        if (outputFormat == "rgba")
        {
            rOffset_ = 0;
            gOffset_ = 1;
            bOffset_ = 2;
        }
        else if (outputFormat == "bgra")
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
            std::array<uint8_t, 4>& rgbaTopLeft     = *(reinterpret_cast<std::array<uint8_t, 4>*>(*output) + ((i * 2 + 0) * width + (j * 2 + 0)));
            std::array<uint8_t, 4>& rgbaTopRight    = *(reinterpret_cast<std::array<uint8_t, 4>*>(*output) + ((i * 2 + 0) * width + (j * 2 + 1)));
            std::array<uint8_t, 4>& rgbaBottomLeft  = *(reinterpret_cast<std::array<uint8_t, 4>*>(*output) + ((i * 2 + 1) * width + (j * 2 + 0)));
            std::array<uint8_t, 4>& rgbaBottomRight = *(reinterpret_cast<std::array<uint8_t, 4>*>(*output) + ((i * 2 + 1) * width + (j * 2 + 1)));

            int32_t u = input[i * width / 2 + j + width * height] - CHROMA_MID;
            int32_t v = input[i * width / 2 + j + width * height * 5 / 4] - CHROMA_MID;

            int32_t yTopLeft     = input[(i * 2 + 0) * width + (j * 2 + 0)];
            int32_t yTopRight    = input[(i * 2 + 0) * width + (j * 2 + 1)];
            int32_t yBottomLeft  = input[(i * 2 + 1) * width + (j * 2 + 0)];
            int32_t yBottomRight = input[(i * 2 + 1) * width + (j * 2 + 1)];

            // Fixed point multiplication
            rgbaTopLeft[rOffset_] = std::max(CHROMA_MIN, std::min(CHROMA_MAX, yTopLeft + ((R_V * v) >> 8)));
            rgbaTopLeft[gOffset_] = std::max(CHROMA_MIN, std::min(CHROMA_MAX, yTopLeft + ((G_U * u + G_V * v) >> 8)));
            rgbaTopLeft[bOffset_] = std::max(CHROMA_MIN, std::min(CHROMA_MAX, yTopLeft + ((B_U * u) >> 8)));
            rgbaTopLeft[3] = 255;

            rgbaTopRight[rOffset_] = std::max(CHROMA_MIN, std::min(CHROMA_MAX, yTopRight + ((R_V * v) >> 8)));
            rgbaTopRight[gOffset_] = std::max(CHROMA_MIN, std::min(CHROMA_MAX, yTopRight + ((G_U * u + G_V * v) >> 8)));
            rgbaTopRight[bOffset_] = std::max(CHROMA_MIN, std::min(CHROMA_MAX, yTopRight + ((B_U * u) >> 8)));
            rgbaTopRight[3] = 255;

            rgbaBottomLeft[rOffset_] = std::max(CHROMA_MIN, std::min(CHROMA_MAX, yBottomLeft + ((R_V * v) >> 8)));
            rgbaBottomLeft[gOffset_] = std::max(CHROMA_MIN, std::min(CHROMA_MAX, yBottomLeft + ((G_U * u + G_V * v) >> 8)));
            rgbaBottomLeft[bOffset_] = std::max(CHROMA_MIN, std::min(CHROMA_MAX, yBottomLeft + ((B_U * u) >> 8)));
            rgbaBottomLeft[3] = 255;

            rgbaBottomRight[rOffset_] = std::max(CHROMA_MIN, std::min(CHROMA_MAX, yBottomRight + ((R_V * v) >> 8)));
            rgbaBottomRight[gOffset_] = std::max(CHROMA_MIN, std::min(CHROMA_MAX, yBottomRight + ((G_U * u + G_V * v) >> 8)));
            rgbaBottomRight[bOffset_] = std::max(CHROMA_MIN, std::min(CHROMA_MAX, yBottomRight + ((B_U * u) >> 8)));
            rgbaBottomRight[3] = 255;
        }
    }
}

#ifdef CITHRUS_SSE41_AVAILABLE
void YuvToRgbaConverter::YuvToRgbaSse41(const uint8_t* input, uint8_t** output, int width, int height)
{
    // This efficiently converts pixels from YUV 4:2:0 to RGBA by using SSE 4.1 instructions to process multiple values simultaneously
    __m128i* rRow = new __m128i[width / 4];
    __m128i* gRow = new __m128i[width / 4];
    __m128i* bRow = new __m128i[width / 4];

    const uint8_t* yIn = input;
    const uint8_t* uIn = input + width * height;
    const uint8_t* vIn = input + width * height * 5 / 4;

    __m128i* out = reinterpret_cast<__m128i*>(*output);

    bool row = false;
    uint32_t pix = 0;

    if (!initialized_)
    {
        initialized_ = true;

        std::string outputFormat = GetOutputPin<0>().GetFormat();

        if (outputFormat == "rgba")
        {
            rShift_ = 0;
            gShift_ = 8;
            bShift_ = 16;
        }
        else if (outputFormat == "bgra")
        {
            rShift_ = 16;
            gShift_ = 8;
            bShift_ = 0;
        }
        else
        {
            throw std::runtime_error("Unsupported format");
        }
    }

    for (int i = 0; i < width * height / 16; i++)
    {
        // Load 16 bytes (16 luma pixels)
        __m128i yA = _mm_loadu_si128(reinterpret_cast<const __m128i*>(yIn));

        yIn += 16;

        __m128i aLuma = _mm_shuffle_epi8(yA, lumaShuffleMask_);

        __m128i uA;
        __m128i vA;
        __m128i uChroma;
        __m128i vChroma;

        // For every second row
        if (!row)
        {
            uA = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(uIn));
            uIn += 8;

            vA = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(vIn));
            vIn += 8;

            uChroma = _mm_shuffle_epi8(uA, chromaShuffleMask_);
            vChroma = _mm_shuffle_epi8(vA, chromaShuffleMask_);
        }

        __m128i rPixTemp;
        __m128i gPixTemp;
        __m128i bPixTemp;

        for (int j = 0; j < 4; j++)
        {
            // We use the same chroma for two rows
            if (row)
            {
                rPixTemp = _mm_loadu_si128(rRow + pix * 2 + j);
                gPixTemp = _mm_loadu_si128(gRow + pix * 2 + j);
                bPixTemp = _mm_loadu_si128(bRow + pix * 2 + j);
            }
            else
            {
                uChroma = _mm_sub_epi32(uChroma, middleVal_);
                vChroma = _mm_sub_epi32(vChroma, middleVal_);

                // Fixed point multiplication
                rPixTemp = _mm_srai_epi32(_mm_mullo_epi32(vChroma, rV_), 8);
                gPixTemp = _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(uChroma, gU_), _mm_mullo_epi32(vChroma, gV_)), 8);
                bPixTemp = _mm_srai_epi32(_mm_mullo_epi32(uChroma, bU_), 8);

                // Store results to be used for the next row
                _mm_storeu_si128(rRow + pix * 2 + j, rPixTemp);
                _mm_storeu_si128(gRow + pix * 2 + j, gPixTemp);
                _mm_storeu_si128(bRow + pix * 2 + j, bPixTemp);
            }

            __m128i rPix = _mm_slli_epi32(_mm_max_epi32(minVal_, _mm_min_epi32(maxVal_, _mm_add_epi32(aLuma, rPixTemp))), rShift_);
            __m128i gPix = _mm_slli_epi32(_mm_max_epi32(minVal_, _mm_min_epi32(maxVal_, _mm_add_epi32(aLuma, gPixTemp))), gShift_);
            __m128i bPix = _mm_slli_epi32(_mm_max_epi32(minVal_, _mm_min_epi32(maxVal_, _mm_add_epi32(aLuma, bPixTemp))), bShift_);

            __m128i rgba = _mm_adds_epu8(_mm_adds_epu8(rPix, gPix), _mm_adds_epu8(bPix, aPix_));

            _mm_storeu_si128(out, rgba);

            out++;

            if (j != 3)
            {
                uA = _mm_srli_si128(uA, 2);
                vA = _mm_srli_si128(vA, 2);
                uChroma = _mm_shuffle_epi8(uA, chromaShuffleMask_);
                vChroma = _mm_shuffle_epi8(vA, chromaShuffleMask_);

                yA = _mm_srli_si128(yA, 4);
                aLuma = _mm_shuffle_epi8(yA, lumaShuffleMask_);
            }
        }

        // Track rows for chroma
        pix++;

        if (pix == width / 16)
        {
            row = !row;
            pix = 0;
        }
    }

    delete[] rRow;
    delete[] gRow;
    delete[] bRow;
}
#endif // CITHRUS_SSE41_AVAILABLE
