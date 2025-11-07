#include "RgbaToYuvConverter.h"
#include "CoreMinimal.h"

#ifdef CITHRUS_SSE41_AVAILABLE
#include <emmintrin.h>
#include <pmmintrin.h>
#include <smmintrin.h>
#endif // CITHRUS_SSE41_AVAILABLE

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define CITHRUS_NEON_AVAILABLE 1
#endif

#include <algorithm>
#include <stdexcept>
#include <cstring>

// Integer BT.601-ish coefficients (match existing SSE path)
static inline uint8_t rgb_to_y(uint8_t r, uint8_t g, uint8_t b)
{ return static_cast<uint8_t>((76 * r + 150 * g + 29 * b) >> 8); }
static inline uint8_t rgb_to_u(uint8_t r, uint8_t g, uint8_t b)
{ int v = (-43 * r - 84 * g + 127 * b); v = (v >> 8) + 128; return static_cast<uint8_t>(std::min(std::max(v, 0), 255)); }
static inline uint8_t rgb_to_v(uint8_t r, uint8_t g, uint8_t b)
{ int v = (127 * r - 106 * g - 21 * b); v = (v >> 8) + 128; return static_cast<uint8_t>(std::min(std::max(v, 0), 255)); }

// Scalar fallback, processes any width/height, RGBA/BGRA -> I420
static void RgbaToYuvScalar_impl(const uint8_t* input, uint8_t* out, int width, int height, bool isRGBA)
{
    uint8_t* y_plane = out;
    uint8_t* u_plane = out + width * height;
    uint8_t* v_plane = out + width * height * 5 / 4;

    // Luma
    for (int y = 0; y < height; ++y) {
        const uint8_t* row = input + y * width * 4;
        uint8_t* y_out = y_plane + y * width;
        for (int x = 0; x < width; ++x) {
            const uint8_t* p = row + (x << 2);
            uint8_t r = isRGBA ? p[0] : p[2];
            uint8_t g = p[1];
            uint8_t b = isRGBA ? p[2] : p[0];
            y_out[x] = rgb_to_y(r, g, b);
        }
    }

    // Chroma (2x2 average)
    for (int y = 0; y < height; y += 2) {
        const uint8_t* row0 = input + y * width * 4;
        const uint8_t* row1 = (y + 1 < height) ? (input + (y + 1) * width * 4) : row0;
        uint8_t* u_out = u_plane + (y / 2) * (width / 2);
        uint8_t* v_out = v_plane + (y / 2) * (width / 2);
        int x = 0;
        for (; x + 1 < width; x += 2) {
            const uint8_t* p00 = row0 + x * 4;
            const uint8_t* p01 = p00 + 4;
            const uint8_t* p10 = row1 + x * 4;
            const uint8_t* p11 = p10 + 4;
            auto get = [&](const uint8_t* p, uint8_t& r, uint8_t& g, uint8_t& b){ r=isRGBA?p[0]:p[2]; g=p[1]; b=isRGBA?p[2]:p[0]; };
            uint8_t r00,g00,b00; get(p00,r00,g00,b00);
            uint8_t r01,g01,b01; get(p01,r01,g01,b01);
            uint8_t r10,g10,b10; get(p10,r10,g10,b10);
            uint8_t r11,g11,b11; get(p11,r11,g11,b11);
            uint8_t r = static_cast<uint8_t>((r00 + r01 + r10 + r11 + 2) >> 2);
            uint8_t g = static_cast<uint8_t>((g00 + g01 + g10 + g11 + 2) >> 2);
            uint8_t b = static_cast<uint8_t>((b00 + b01 + b10 + b11 + 2) >> 2);
            u_out[x >> 1] = rgb_to_u(r,g,b);
            v_out[x >> 1] = rgb_to_v(r,g,b);
        }
        if (x < width) { // odd last column
            int last = width - 1;
            const uint8_t* p00 = row0 + last * 4;
            const uint8_t* p10 = row1 + last * 4;
            uint8_t r00,g00,b00; uint8_t r10,g10,b10;
            auto get = [&](const uint8_t* p, uint8_t& r, uint8_t& g, uint8_t& b){ r=isRGBA?p[0]:p[2]; g=p[1]; b=isRGBA?p[2]:p[0]; };
            get(p00,r00,g00,b00); get(p10,r10,g10,b10);
            uint8_t r = static_cast<uint8_t>((r00 + r00 + r10 + r10 + 2) >> 2);
            uint8_t g = static_cast<uint8_t>((g00 + g00 + g10 + g10 + 2) >> 2);
            uint8_t b = static_cast<uint8_t>((b00 + b00 + b10 + b10 + 2) >> 2);
            u_out[last >> 1] = rgb_to_u(r,g,b);
            v_out[last >> 1] = rgb_to_v(r,g,b);
        }
    }
}

#if defined(CITHRUS_NEON_AVAILABLE)
// NEON path for AArch64: RGBA/BGRA -> I420
static void RgbaToYuvNeon_impl(const uint8_t* input, uint8_t* out, int width, int height, bool isRGBA)
{
    uint8_t* y_plane = out;
    uint8_t* u_plane = out + width * height;
    uint8_t* v_plane = out + width * height * 5 / 4;
    const int stride_rgba = width * 4;

    const uint16x8_t c76  = vdupq_n_u16(76);
    const uint16x8_t c150 = vdupq_n_u16(150);
    const uint16x8_t c29  = vdupq_n_u16(29);
    const int16x4_t cU_r = vdup_n_s16(-43);
    const int16x4_t cU_g = vdup_n_s16(-84);
    const int16x4_t cU_b = vdup_n_s16(127);
    const int16x4_t cV_r = vdup_n_s16(127);
    const int16x4_t cV_g = vdup_n_s16(-106);
    const int16x4_t cV_b = vdup_n_s16(-21);
    const int32x4_t cOff = vdupq_n_s32(128 << 8);

    for (int y = 0; y < height; ++y) {
        const uint8_t* row = input + y * stride_rgba; uint8_t* y_out = y_plane + y * width; int x = 0;
        for (; x + 16 <= width; x += 16) {
            const uint8_t* p = row + x * 4; uint8x16x4_t px = vld4q_u8(p);
            uint8x16_t r = isRGBA ? px.val[0] : px.val[2]; uint8x16_t g = px.val[1]; uint8x16_t b = isRGBA ? px.val[2] : px.val[0];
            uint16x8_t rl = vmovl_u8(vget_low_u8(r)); uint16x8_t gl = vmovl_u8(vget_low_u8(g)); uint16x8_t bl = vmovl_u8(vget_low_u8(b));
            uint16x8_t yl = vmulq_u16(rl, c76); yl = vmlaq_u16(yl, gl, c150); yl = vmlaq_u16(yl, bl, c29); uint8x8_t y8l = vshrn_n_u16(yl, 8);
            uint16x8_t rh = vmovl_u8(vget_high_u8(r)); uint16x8_t gh = vmovl_u8(vget_high_u8(g)); uint16x8_t bh = vmovl_u8(vget_high_u8(b));
            uint16x8_t yh = vmulq_u16(rh, c76); yh = vmlaq_u16(yh, gh, c150); yh = vmlaq_u16(yh, bh, c29); uint8x8_t y8h = vshrn_n_u16(yh, 8);
            vst1q_u8(y_out + x, vcombine_u8(y8l, y8h));
        }
        for (; x < width; ++x) { const uint8_t* p = row + x * 4; uint8_t r = isRGBA ? p[0] : p[2]; uint8_t g = p[1]; uint8_t b = isRGBA ? p[2] : p[0]; y_out[x] = rgb_to_y(r,g,b); }
    }

    for (int y = 0; y < height; y += 2) {
        const uint8_t* row0 = input + y * stride_rgba; const uint8_t* row1 = (y + 1 < height) ? (input + (y + 1) * stride_rgba) : row0;
        uint8_t* u_out = u_plane + (y / 2) * (width / 2); uint8_t* v_out = v_plane + (y / 2) * (width / 2); int x = 0;
        for (; x + 16 <= width; x += 16) {
            const uint8_t* p0 = row0 + x * 4; const uint8_t* p1 = row1 + x * 4; uint8x16x4_t a0 = vld4q_u8(p0); uint8x16x4_t a1 = vld4q_u8(p1);
            uint8x16_t r0 = isRGBA ? a0.val[0] : a0.val[2]; uint8x16_t g0 = a0.val[1]; uint8x16_t b0 = isRGBA ? a0.val[2] : a0.val[0];
            uint8x16_t r1 = isRGBA ? a1.val[0] : a1.val[2]; uint8x16_t g1 = a1.val[1]; uint8x16_t b1 = isRGBA ? a1.val[2] : a1.val[0];
            uint16x8_t r0_h = vpaddlq_u8(r0); uint16x8_t g0_h = vpaddlq_u8(g0); uint16x8_t b0_h = vpaddlq_u8(b0);
            uint16x8_t r1_h = vpaddlq_u8(r1); uint16x8_t g1_h = vpaddlq_u8(g1); uint16x8_t b1_h = vpaddlq_u8(b1);
            uint16x8_t r_avg16 = vrshrq_n_u16(vaddq_u16(r0_h, r1_h), 2); uint16x8_t g_avg16 = vrshrq_n_u16(vaddq_u16(g0_h, g1_h), 2); uint16x8_t b_avg16 = vrshrq_n_u16(vaddq_u16(b0_h, b1_h), 2);
            int16x8_t r16 = vreinterpretq_s16_u16(r_avg16); int16x8_t g16 = vreinterpretq_s16_u16(g_avg16); int16x8_t b16 = vreinterpretq_s16_u16(b_avg16);
            int16x4_t rL = vget_low_s16(r16), rH = vget_high_s16(r16); int16x4_t gL = vget_low_s16(g16), gH = vget_high_s16(g16); int16x4_t bL = vget_low_s16(b16), bH = vget_high_s16(b16);
            int32x4_t u0 = vmull_s16(rL, cU_r); u0 = vmlal_s16(u0, gL, cU_g); u0 = vmlal_s16(u0, bL, cU_b); u0 = vaddq_s32(u0, cOff); u0 = vshrq_n_s32(u0, 8);
            int32x4_t u1 = vmull_s16(rH, cU_r); u1 = vmlal_s16(u1, gH, cU_g); u1 = vmlal_s16(u1, bH, cU_b); u1 = vaddq_s32(u1, cOff); u1 = vshrq_n_s32(u1, 8);
            uint8x8_t u8 = vqmovun_s16(vcombine_s16(vqmovn_s32(u0), vqmovn_s32(u1)));
            int32x4_t v0 = vmull_s16(rL, cV_r); v0 = vmlal_s16(v0, gL, cV_g); v0 = vmlal_s16(v0, bL, cV_b); v0 = vaddq_s32(v0, cOff); v0 = vshrq_n_s32(v0, 8);
            int32x4_t v1 = vmull_s16(rH, cV_r); v1 = vmlal_s16(v1, gH, cV_g); v1 = vmlal_s16(v1, bH, cV_b); v1 = vaddq_s32(v1, cOff); v1 = vshrq_n_s32(v1, 8);
            uint8x8_t v8 = vqmovun_s16(vcombine_s16(vqmovn_s32(v0), vqmovn_s32(v1)));
            vst1_u8(u_out + (x >> 1), u8); vst1_u8(v_out + (x >> 1), v8);
        }
        for (; x + 1 < width; x += 2) { // tail pairs
            const uint8_t* p00 = row0 + x * 4; const uint8_t* p01 = p00 + 4; const uint8_t* p10 = row1 + x * 4; const uint8_t* p11 = p10 + 4;
            auto pick = [&](const uint8_t* p, uint8_t& r, uint8_t& g, uint8_t& b){ r=isRGBA?p[0]:p[2]; g=p[1]; b=isRGBA?p[2]:p[0]; };
            uint8_t r00,g00,b00; pick(p00,r00,g00,b00); uint8_t r01,g01,b01; pick(p01,r01,g01,b01); uint8_t r10,g10,b10; pick(p10,r10,g10,b10); uint8_t r11,g11,b11; pick(p11,r11,g11,b11);
            uint8_t r = static_cast<uint8_t>((r00 + r01 + r10 + r11 + 2) >> 2); uint8_t g = static_cast<uint8_t>((g00 + g01 + g10 + g11 + 2) >> 2); uint8_t b = static_cast<uint8_t>((b00 + b01 + b10 + b11 + 2) >> 2);
            u_out[x >> 1] = rgb_to_u(r,g,b); v_out[x >> 1] = rgb_to_v(r,g,b);
        }
        if (x < width) { int last = width - 1; const uint8_t* p00 = row0 + last * 4; const uint8_t* p10 = row1 + last * 4; uint8_t r00,g00,b00; uint8_t r10,g10,b10; auto pick=[&](const uint8_t* p,uint8_t& r,uint8_t& g,uint8_t& b){r=isRGBA?p[0]:p[2];g=p[1];b=isRGBA?p[2]:p[0];}; pick(p00,r00,g00,b00); pick(p10,r10,g10,b10); uint8_t r=static_cast<uint8_t>((r00+r00+r10+r10+2)>>2); uint8_t g=static_cast<uint8_t>((g00+g00+g10+g10+2)>>2); uint8_t b=static_cast<uint8_t>((b00+b00+b10+b10+2)>>2); u_out[last>>1]=rgb_to_u(r,g,b); v_out[last>>1]=rgb_to_v(r,g,b); }
    }
}
#endif // CITHRUS_NEON_AVAILABLE

// One-time compile-time macro reporting
static void LogCompileTimeArchOnce() {
    static bool done = false; if (done) return; done = true;
    FString Macros;
#if defined(__APPLE__)
    Macros += TEXT("__APPLE__ ");
#endif
#if defined(__aarch64__)
    Macros += TEXT("__aarch64__ ");
#endif
#if defined(__arm64__)
    Macros += TEXT("__arm64__ ");
#endif
#if defined(__ARM_NEON)
    Macros += TEXT("__ARM_NEON ");
#endif
#if defined(__ARM_NEON__)
    Macros += TEXT("__ARM_NEON__ ");
#endif
#if defined(__x86_64__)
    Macros += TEXT("__x86_64__ ");
#endif
#if defined(__SSE4_1__)
    Macros += TEXT("__SSE4_1__ ");
#endif
#if defined(CITHRUS_NEON_AVAILABLE)
    Macros += TEXT("CITHRUS_NEON_AVAILABLE ");
#endif
#if defined(CITHRUS_SSE41_AVAILABLE)
    Macros += TEXT("CITHRUS_SSE41_AVAILABLE ");
#endif
#if defined(PLATFORM_CPU_ARM_FAMILY)
    Macros += TEXT("PLATFORM_CPU_ARM_FAMILY ");
#endif
    UE_LOG(LogTemp, Display, TEXT("RgbaToYuvConverter compile-time macros: %s"), *Macros);
}

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
{ delete[] outputData_; outputData_ = nullptr; GetOutputPin<0>().SetData(nullptr); GetOutputPin<0>().SetSize(0); }

void RgbaToYuvConverter::Process()
{
    LogCompileTimeArchOnce();
    const uint8_t* inputData = GetInputPin<0>().GetData(); uint32_t inputSize = GetInputPin<0>().GetSize();
    if (!inputData || inputSize != outputFrameWidth_ * outputFrameHeight_ * 4) { return; }
    if ((outputFrameWidth_ & 1) || (outputFrameHeight_ & 1)) { UE_LOG(LogTemp, Warning, TEXT("RgbaToYuvConverter: Non-even dimensions %d x %d; YUV420 expects even dimensions."), outputFrameWidth_, outputFrameHeight_); }
    const bool isRGBA = (GetInputPin<0>().GetFormat() == "rgba");
#if defined(CITHRUS_NEON_AVAILABLE)
    static bool sLogged = false; if (!sLogged) { UE_LOG(LogTemp, Display, TEXT("RgbaToYuvConverter: Using NEON optimized path (ARM).")); sLogged = true; }
    RgbaToYuvNeon_impl(inputData, outputData_, outputFrameWidth_, outputFrameHeight_, isRGBA);
    static bool sVerified = false; if (!sVerified) {
        const size_t refSize = outputFrameWidth_ * outputFrameHeight_ * 3 / 2;
        TArray<uint8> RefBuf; RefBuf.AddUninitialized(refSize);
        RgbaToYuvScalar_impl(inputData, RefBuf.GetData(), outputFrameWidth_, outputFrameHeight_, isRGBA);
        int mismatches = 0; const uint8* neonOut = outputData_; const uint8* refOut = RefBuf.GetData();
        for (size_t i = 0; i < refSize; ++i) { if (neonOut[i] != refOut[i]) { ++mismatches; if (mismatches < 10) { UE_LOG(LogTemp, Warning, TEXT("NEON mismatch at index %d: neon=%d ref=%d"), (int)i, (int)neonOut[i], (int)refOut[i]); } } }
        if (mismatches == 0) { UE_LOG(LogTemp, Display, TEXT("RgbaToYuvConverter: NEON output validated against scalar reference.")); } else { UE_LOG(LogTemp, Warning, TEXT("RgbaToYuvConverter: NEON path had %d mismatches vs scalar reference."), mismatches); }
        sVerified = true;
    }
#elif defined(CITHRUS_SSE41_AVAILABLE)
    RgbaToYuvSse41(inputData, &outputData_, outputFrameWidth_, outputFrameHeight_);
#else
    RgbaToYuvScalar_impl(inputData, outputData_, outputFrameWidth_, outputFrameHeight_, isRGBA);
#endif
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
        throw std::runtime_error("Unsupported format");
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
