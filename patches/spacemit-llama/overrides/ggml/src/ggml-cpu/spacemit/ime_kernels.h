#pragma once

#include <cassert>
#include <cstddef>
#include <functional>

namespace spacemit_kernels {

#define BLOCK_QNK_LEN 256

template <int N> struct nrow_block_q2_k {
    // [4bit scale + 4bit zp] * N * 16
    uint8_t  scales[N * BLOCK_QNK_LEN / 16];
    // [b0, b16, b32, b48] [b1, b17, b33, b49] ... [b15, b31, b47, b63]
    // [b64, b80, b96, b112] ...[b79, b95, b111, b127]
    // [b128, b144, b160, b176] ...[b143, b159, b175, b191]
    // [b192, b208, b224, b240] ...[b207, b223, b239, b255]
    uint8_t  qs[N * BLOCK_QNK_LEN / 4];
    uint16_t scales16[N];
    uint16_t zeros16[N];
};

template <int N> struct nrow_block_q3_k {
    // [8bit scale] * N * 16
    int8_t   scales[N * 16];
    // [b0, b1, b2, b3, b4, b5, b6, b7] ... [b248, b249, b250, b251, b252, b253, b254, b255]
    uint8_t  hmask[N * BLOCK_QNK_LEN / 8];
    // [b0, b16, b32, b48] [b1, b17, b33, b49] ... [b15, b31, b47, b63]
    // [b64, b80, b96, b112] ...[b79, b95, b111, b127]
    // [b128, b144, b160, b176] ...[b143, b159, b175, b191]
    // [b192, b208, b224, b240] ...[b207, b223, b239, b255]
    uint8_t  qs[N * BLOCK_QNK_LEN / 4];
    uint16_t scales16[N];
};

template <int N> struct nrow_block_mxfp4 {
    uint8_t e[N];
    uint8_t qh[4 * N];
    uint8_t qs[16 * N];
};

template <int N> struct __attribute__((packed)) nrow_block_q5_1 {
    uint16_t scales16[N];
    uint8_t  zp[N];
    // n0 [bh0, bh1, bh2, bh3, bh4, bh5, bh6, bh7] ....
    uint8_t  qh[4 * N];
    // n0 [b0, b1], [b2, b3] ....  [b30, b31]
    // n1 [b0, b1], [b2, b3] ....  [b30, b31]
    uint8_t  qs[16 * N];
};

static_assert(sizeof(nrow_block_q5_1<1>) == sizeof(uint8_t) + 22, "wrong nrow_block_q5_1 block size/padding");

template <int N> struct __attribute__((packed)) nrow_block_q5_0 {
    uint16_t scales16[N];
    // n0 [bh0, bh1, bh2, bh3, bh4, bh5, bh6, bh7] ....
    uint8_t  qh[4 * N];
    // n0 [b0, b1], [b2, b3] ....  [b30, b31]
    // n1 [b0, b1], [b2, b3] ....  [b30, b31]
    uint8_t  qs[16 * N];
};

static_assert(sizeof(nrow_block_q5_0<1>) == 22, "wrong nrow_block_q5_0 block size/padding");

using gemm_kernel_quantize_def = std::function<
    size_t(size_t, const uint8_t *, const uint8_t *, const uint8_t *, float *, size_t, size_t, size_t, size_t)>;

using moe_gemm_kernel_quantize_def = std::function<
    size_t(size_t, const uint8_t **, const uint8_t *, const uint8_t *, float **, size_t, size_t, size_t, size_t)>;

namespace ime1 {
// Original Q4_K subblock parameters: w = d*(q-8) + m.
// Keep fp32 scale/offset and the existing IME1 16-row nibble interleave.
struct block_q4_k_x16 {
    float   d[16];
    float   m[16];
    uint8_t qs[256];
};
static_assert(sizeof(block_q4_k_x16) == 384, "unexpected Q4_K IME1 block padding");

// Q4_K-specific activations: fp32 scales, the unchanged int8 interleave,
// then one fp32 scaled sum per row. M1/M4 blocks are 40/160 bytes.
constexpr size_t q4_k_activation_block_size = 2 * sizeof(float) + 32;
void quantize_a_row_i8q4_k(size_t blk_len, const float * a_ptr, size_t count_k, uint8_t * quant_a_ptr);
void quantize_a_4row_i8q4_k(size_t blk_len, const float * a_ptr, size_t count_k, uint8_t * quant_a_ptr);

size_t gemm_kernel_i8q4_k(size_t blk_len, const uint8_t * quant_a_ptr,
                        const uint8_t * quant_b_data, const uint8_t * quant_b_zp,
                        float * c_ptr, size_t count_m, size_t count_n,
                        size_t k_blks, size_t ldc);

// Dense single-token decode retains the original 36-byte activation layout.
size_t gemm_kernel_i8q4_k_decode(size_t blk_len, const uint8_t * quant_a_ptr,
                               const uint8_t * quant_b_data, const uint8_t * quant_b_zp,
                               float * c_ptr, size_t count_m, size_t count_n,
                               size_t k_blks, size_t ldc);

size_t gemm_kernel_i8i4(size_t          blk_len,
                        const uint8_t * quant_a_ptr,
                        const uint8_t * quant_b_data,
                        const uint8_t * quant_b_zp,
                        float *         c_ptr,
                        size_t          count_m,
                        size_t          count_n,
                        size_t          k_blks,
                        size_t          ldc);

void quantize_a_row_i8(size_t blk_len, const float * a_ptr, size_t count_k, uint8_t * quant_a_ptr);

void quantize_a_4row_i8(size_t blk_len, const float * a_ptr, size_t count_k, uint8_t * quant_a_ptr);

}  // namespace ime1

namespace ime2 {
size_t gemm_kernel_i8i2k(size_t          blk_len,
                         const uint8_t * quant_a_ptr,
                         const uint8_t * quant_b_data,
                         const uint8_t * quant_b_zp,
                         float *         c_ptr,
                         size_t          count_m,
                         size_t          count_n,
                         size_t          k_blks,
                         size_t          ldc);

size_t gemm_kernel_i8i3k(size_t          blk_len,
                         const uint8_t * quant_a_ptr,
                         const uint8_t * quant_b_data,
                         const uint8_t * quant_b_zp,
                         float *         c_ptr,
                         size_t          count_m,
                         size_t          count_n,
                         size_t          k_blks,
                         size_t          ldc);

size_t gemm_kernel_i8i4(size_t          blk_len,
                        const uint8_t * quant_a_ptr,
                        const uint8_t * quant_b_data,
                        const uint8_t * quant_b_zp,
                        float *         c_ptr,
                        size_t          count_m,
                        size_t          count_n,
                        size_t          k_blks,
                        size_t          ldc);

size_t gemm_kernel_i8i4_hp(size_t          blk_len,
                           const uint8_t * quant_a_ptr,
                           const uint8_t * quant_b_data,
                           const uint8_t * quant_b_zp,
                           float *         c_ptr,
                           size_t          count_m,
                           size_t          count_n,
                           size_t          k_blks,
                           size_t          ldc);

size_t moe_m2_gemm_kernel_i8i4(size_t           blk_len,
                               const uint8_t ** quant_a_ptr,
                               const uint8_t *  quant_b_data,
                               const uint8_t *  quant_b_zp,
                               float **         c_ptr,
                               size_t           count_m,
                               size_t           count_n,
                               size_t           k_blks,
                               size_t           ldc);

size_t gemm_kernel_i8i8(size_t          blk_len,
                        const uint8_t * quant_a_ptr,
                        const uint8_t * quant_b_data,
                        const uint8_t * quant_b_zp,
                        float *         c_ptr,
                        size_t          count_m,
                        size_t          count_n,
                        size_t          k_blks,
                        size_t          ldc);

size_t gemm_kernel_i8mxfp4(size_t          blk_len,
                           const uint8_t * quant_a_ptr,
                           const uint8_t * quant_b_data,
                           const uint8_t * quant_b_zp,
                           float *         c_ptr,
                           size_t          count_m,
                           size_t          count_n,
                           size_t          k_blks,
                           size_t          ldc);

size_t moe_m2_gemm_kernel_i8mxfp4(size_t           blk_len,
                                  const uint8_t ** quant_a_ptr,
                                  const uint8_t *  quant_b_data,
                                  const uint8_t *  quant_b_zp,
                                  float **         c_ptr,
                                  size_t           count_m,
                                  size_t           count_n,
                                  size_t           k_blks,
                                  size_t           ldc);

size_t gemm_kernel_i8i5(size_t          blk_len,
                        const uint8_t * quant_a_ptr,
                        const uint8_t * quant_b_data,
                        const uint8_t * quant_b_zp,
                        float *         c_ptr,
                        size_t          count_m,
                        size_t          count_n,
                        size_t          k_blks,
                        size_t          ldc);

size_t moe_m2_gemm_kernel_i8i5(size_t           blk_len,
                               const uint8_t ** quant_a_ptr,
                               const uint8_t *  quant_b_data,
                               const uint8_t *  quant_b_zp,
                               float **         c_ptr,
                               size_t           count_m,
                               size_t           count_n,
                               size_t           k_blks,
                               size_t           ldc);
}  // namespace ime2
}  // namespace spacemit_kernels
