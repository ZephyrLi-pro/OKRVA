// Standalone edge cases for the private IME1 Q4_K format/kernel.
// All inputs are synthetic; no production models, services or device controls.
#define GGML_COMMON_DECL_CPP
#include "ggml-common.h"
#include "ggml-impl.h"
#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-backend.h"
#include "spacemit/ime_kernels.h"
#include "spacemit/repack.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <random>
#include <sched.h>
#include <stdexcept>
#include <vector>

using spacemit_kernels::ime1::block_q4_k_x16;

int main() {
    // Initialize the CPU backend so IME1-capable preferred cores are selected.
    auto backend = ggml_backend_cpu_init();
    if (!backend) return 2;
    // K1's vendor kernel is valid only on preferred IME1 cores (0-3 here).
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask)) return 3;
    int cases = 0;
    int decode_checks = 0;
    std::mt19937 rng(20261013);
    std::uniform_int_distribution<int> nibble(0, 15);
    std::uniform_real_distribution<float> real(-1.0f, 1.0f);
    for (size_t k : {size_t(32), size_t(64), size_t(256), size_t(1024)}) {
        const size_t blocks = k / 32;
        for (size_t n : {size_t(1), size_t(7), size_t(16), size_t(17), size_t(32)}) {
            for (size_t rows : {size_t(1), size_t(4)}) {
                for (int pattern = 0; pattern < 4; pattern++) {
                    std::vector<block_q4_k_x16> b(((n + 15) / 16) * blocks);
                    for (auto & block : b) {
                        for (size_t j = 0; j < 16; j++) {
                            // Include zero scale with nonzero offset and clamped-zp cases.
                            block.d[j] = pattern == 2 ? 0.0f : 0.01234567f * (j + 1);
                            block.m[j] = pattern == 3 ? 0.0f : -0.07123456f * (j + 1);
                        }
                        for (auto & v : block.qs) v = nibble(rng) | (nibble(rng) << 4);
                    }
                    std::vector<float> input(rows * k);
                    for (auto & v : input) v = pattern == 0 ? 0.0f : pattern == 1 ? 0.7f : real(rng);
                    const size_t a_size = rows * blocks * spacemit_kernels::ime1::q4_k_activation_block_size;
                    std::vector<uint8_t> a(a_size + 64, 0xa5);
                    std::vector<uint8_t> old_a(rows * blocks * 36);
                    if (rows == 4) {
                        spacemit_kernels::ime1::quantize_a_4row_i8(32, input.data(), k, old_a.data());
                        spacemit_kernels::ime1::quantize_a_4row_i8q4_k(32, input.data(), k, a.data());
                    } else {
                        spacemit_kernels::ime1::quantize_a_row_i8(32, input.data(), k, old_a.data());
                        spacemit_kernels::ime1::quantize_a_row_i8q4_k(32, input.data(), k, a.data());
                    }
                    for (size_t kk = 0; kk < blocks; kk++) {
                        const auto * aa = a.data() + kk * rows * 40;
                        if (std::memcmp(aa, old_a.data() + kk * rows * 36, rows * 36))
                            throw std::runtime_error("Original activation quantization changed");
                        for (size_t r = 0; r < rows; r++) {
                            float scale, cached;
                            std::memcpy(&scale, aa + r * 4, 4);
                            std::memcpy(&cached, aa + rows * 36 + r * 4, 4);
                            int sum = 0;
                            for (size_t p = 0; p < 32; p++) {
                                const size_t ai = rows == 4 ? (p / 8) * 32 + r * 8 + p % 8 : p;
                                sum += (int8_t) aa[rows * 4 + ai];
                            }
                            const float expected = scale * float(sum);
                            if (std::memcmp(&expected, &cached, 4))
                                throw std::runtime_error("Cached sum mismatch");
                        }
                    }
                    for (size_t p = a_size; p < a.size(); p++)
                        if (a[p] != 0xa5) throw std::runtime_error("Activation scratch guard overwritten");
                    const size_t ldc = n + 7;
                    std::vector<float> out(rows * ldc, 123456.0f);
                    auto done = spacemit_kernels::ime1::gemm_kernel_i8q4_k(
                        32, a.data(), reinterpret_cast<uint8_t *>(b.data()), nullptr,
                        out.data(), rows, n, blocks, ldc);
                    if (done != rows) throw std::runtime_error("Wrong processed row count");
                    if (rows == 1) {
                        std::vector<float> decode(rows * ldc, 123456.0f);
                        spacemit_kernels::ime1::gemm_kernel_i8q4_k_decode(
                            32, old_a.data(), reinterpret_cast<uint8_t *>(b.data()), nullptr,
                            decode.data(), 1, n, blocks, ldc);
                        if (std::memcmp(out.data(), decode.data(), out.size() * sizeof(float)))
                            throw std::runtime_error("Decode and cached kernels differ");
                        decode_checks++;
                    }
                    double squared = 0, norm = 0, max_abs = 0;
                    for (size_t r = 0; r < rows; r++) {
                        for (size_t col = 0; col < n; col++) {
                            double ref = 0;
                            for (size_t kk = 0; kk < blocks; kk++) {
                                const auto * aa = a.data() + kk * rows * 40;
                                float da;
                                std::memcpy(&da, aa + r * 4, 4);
                                const auto & bb = b[(col / 16) * blocks + kk];
                                for (size_t p = 0; p < 32; p++) {
                                    const size_t ai = rows == 4 ? (p / 8) * 32 + r * 8 + p % 8 : p;
                                    const int qa = (int8_t) aa[rows * 4 + ai];
                                    const uint8_t qbyte = bb.qs[(p >= 16 ? 128 : 0) + (col % 16) * 8 + p % 8];
                                    const int qb = p % 16 < 8 ? qbyte & 15 : qbyte >> 4;
                                    ref += double(da) * qa * (double(bb.d[col % 16]) * (qb - 8) + bb.m[col % 16]);
                                }
                            }
                            const double err = out[r * ldc + col] - ref;
                            if (!std::isfinite(out[r * ldc + col])) throw std::runtime_error("Non-finite output");
                            squared += err * err;
                            norm += ref * ref;
                            max_abs = std::max(max_abs, std::abs(err));
                        }
                        for (size_t j = n; j < ldc; j++) {
                            if (out[r * ldc + j] != 123456.0f) throw std::runtime_error("Output padding overwritten");
                        }
                    }
                    const double rms = std::sqrt(squared / std::max(1e-30, norm));
                    std::cout << "{\"k\":" << k << ",\"n\":" << n << ",\"m\":" << rows
                              << ",\"pattern\":" << pattern << ",\"relative_rms\":" << rms
                              << ",\"max_abs\":" << max_abs << "}" << std::endl;
                    if (rms > 2e-5 || max_abs > 0.002) throw std::runtime_error("Affine numeric tolerance exceeded");
                    cases++;
                }
            }
        }
    }
    ggml_backend_free(backend);
    std::cout << "{\"passed\":" << cases << ",\"decode_equivalence_passed\":" << decode_checks << "}" << std::endl;
}
