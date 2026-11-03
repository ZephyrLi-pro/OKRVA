// Small, read-only operator check against the installed GGML shared libraries.
// Real model weight row samples + synthetic inputs, NOT a whole-model accuracy test.
#include <ggml.h>
#include <ggml-alloc.h>
#include <ggml-backend.h>
#include <ggml-cpu.h>
#include <gguf.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

struct error_stats {
    double relative_rms = 0;
    double max_abs = 0;
};

bool fp32_offset_format = false;

error_stats difference(const std::vector<float> & got, const std::vector<double> & ref) {
    double err = 0, norm = 0, max_abs = 0;
    for (size_t i = 0; i < got.size(); i++) {
        if (!std::isfinite(got[i])) throw std::runtime_error("Non-finite result");
        double d = got[i] - ref[i];
        err += d * d;
        norm += ref[i] * ref[i];
        max_abs = std::max(max_abs, std::abs(d));
    }
    return {std::sqrt(err / std::max(1e-30, norm)), max_abs};
}

std::vector<float> multiply(ggml_backend_buffer_type_t wbuf, ggml_backend_t backend,
                            int64_t k, int64_t n, int64_t m,
                            const std::vector<uint8_t> & weights,
                            const std::vector<float> & input,
                            std::vector<float> * unpacked_ime_weights = nullptr) {
    auto * wc = ggml_init({ggml_tensor_overhead() * 4, nullptr, true});
    auto * ac = ggml_init({ggml_tensor_overhead() * 12 + ggml_graph_overhead_custom(32, false), nullptr, true});
    if (!wc || !ac) throw std::runtime_error("Context allocation failed");
    auto * w = ggml_new_tensor_2d(wc, GGML_TYPE_Q4_K, k, n);
    auto * x = ggml_new_tensor_2d(ac, GGML_TYPE_F32, k, m);
    ggml_set_name(w, "actual_weight_sample");
    ggml_set_name(x, "synthetic_input");
    auto * y = ggml_mul_mat(ac, w, x);
    auto * graph = ggml_new_graph_custom(ac, 32, false);
    ggml_build_forward_expand(graph, y);
    auto wb = ggml_backend_alloc_ctx_tensors_from_buft(wc, wbuf);
    auto ab = ggml_backend_alloc_ctx_tensors(ac, backend);
    if (!wb || !ab) throw std::runtime_error("Tensor buffer allocation failed");
    ggml_backend_tensor_set(w, weights.data(), 0, weights.size());
    if (unpacked_ime_weights) {
        // Decode the actual installed IME1 Q4_K_16x32 buffer, not just the source formula.
        if (std::string(ggml_backend_buft_name(wbuf)) != "CPU_RISCV64_SPACEMIT")
            throw std::runtime_error("Expected IME1 buffer");
        const size_t block_bytes = fp32_offset_format ? 384 : 304;
        const size_t expected = size_t(n / 16) * size_t(k / 32) * block_bytes;
        std::cout << "{\"event\":\"packed_allocation\",\"expected\":" << expected
                  << ",\"actual\":" << ggml_backend_buft_get_alloc_size(wbuf, w) << "}" << std::endl;
        if (ggml_backend_buft_get_alloc_size(wbuf, w) < expected)
            throw std::runtime_error("Unexpected packed buffer allocation size");
        unpacked_ime_weights->resize(k * n);
        const auto * packed = static_cast<const uint8_t *>(w->data);
        for (int64_t row = 0; row < n; row++) {
            for (int64_t block = 0; block < k / 32; block++) {
                const uint8_t * b = packed + ((row / 16) * (k / 32) + block) * block_bytes;
                int within = row % 16;
                float d, offset = 0;
                int zp = 8;
                if (fp32_offset_format) {
                    std::memcpy(&d, b + within * 4, 4);
                    std::memcpy(&offset, b + 64 + within * 4, 4);
                } else {
                    ggml_fp16_t half_d;
                    std::memcpy(&half_d, b + within * 2, 2);
                    d = ggml_fp16_to_fp32(half_d);
                    zp = b[32 + within];
                }
                for (int p = 0; p < 32; p++) {
                    int q_byte = b[(fp32_offset_format ? 128 : 48) + (p >= 16 ? 128 : 0) + within * 8 + p % 8];
                    int q = p % 16 < 8 ? q_byte & 15 : q_byte >> 4;
                    (*unpacked_ime_weights)[row * k + block * 32 + p] = d * (q - zp) + offset;
                }
            }
        }
    }
    ggml_backend_tensor_set(x, input.data(), 0, input.size() * sizeof(float));
    auto status = ggml_backend_graph_compute(backend, graph);
    if (status != GGML_STATUS_SUCCESS) throw std::runtime_error("Graph compute failed");
    std::vector<float> out(n * m);
    ggml_backend_tensor_get(y, out.data(), 0, out.size() * sizeof(float));
    ggml_backend_buffer_free(wb);
    ggml_backend_buffer_free(ab);
    ggml_free(wc);
    ggml_free(ac);
    return out;
}

error_stats repack_rounding(const std::vector<uint8_t> & bytes,
                            const std::vector<float> & original,
                            std::vector<float> & zp_reconstruction) {
    if (ggml_type_size(GGML_TYPE_Q4_K) != 144 || ggml_blck_size(GGML_TYPE_Q4_K) != 256)
        throw std::runtime_error("Unexpected Q4_K storage format");
    std::vector<float> reconstructed(original.size());
    zp_reconstruction.resize(original.size());
    std::vector<double> reference(original.begin(), original.end());
    for (size_t block = 0; block < bytes.size() / 144; block++) {
        const uint8_t * b = bytes.data() + block * 144;
        ggml_fp16_t half_d, half_min;
        std::memcpy(&half_d, b, 2);
        std::memcpy(&half_min, b + 2, 2);
        float d = ggml_fp16_to_fp32(half_d), dmin = ggml_fp16_to_fp32(half_min);
        const uint8_t * scales = b + 4;
        for (int j = 0; j < 8; j++) {
            int sc = j < 4 ? scales[j] & 63 : (scales[j + 4] & 15) | ((scales[j - 4] >> 6) << 4);
            int mn = j < 4 ? scales[j + 4] & 63 : (scales[j + 4] >> 4) | ((scales[j] >> 6) << 4);
            float full_d = d * sc, full_m = dmin * mn;
            float rounded_d = ggml_fp16_to_fp32(ggml_fp32_to_fp16(full_d));
            float rounded_m = ggml_fp16_to_fp32(ggml_fp32_to_fp16(full_m));
            // This matches make_block_q4_1x16: a rounded/clamped integer zero point.
            float zp = rounded_d == 0 ? 0 : std::nearbyint(rounded_m / rounded_d);
            zp = std::min(15.0f, std::max(0.0f, zp));
            const uint8_t * q = b + 16 + (j / 2) * 32;
            for (int p = 0; p < 32; p++) {
                int v = j % 2 == 0 ? q[p] & 15 : q[p] >> 4;
                size_t i = block * 256 + j * 32 + p;
                if (std::abs((full_d * v - full_m) - original[i]) > 1e-6)
                    throw std::runtime_error("Repack reconstruction disagrees with native dequantization");
                reconstructed[i] = rounded_d * v - rounded_m;
                zp_reconstruction[i] = rounded_d * (v - zp);
            }
        }
    }
    return difference(reconstructed, reference);
}

// Exercise MUL_MAT_ID's separate activation/TCM strides with two distinct
// synthetic experts, including broadcast and non-broadcast input rows.
void check_moe(ggml_backend_buffer_type_t ime, ggml_backend_t backend,
               int64_t k, int64_t n, const std::vector<uint8_t> & weights,
               const std::vector<float> & w_float) {
    const size_t row_bytes = ggml_row_size(GGML_TYPE_Q4_K, k);
    std::vector<uint8_t> experts(weights.size() * 2);
    std::memcpy(experts.data(), weights.data(), weights.size());
    for (int64_t row = 0; row < n; row++)
        std::memcpy(experts.data() + weights.size() + row * row_bytes,
                    weights.data() + (n - 1 - row) * row_bytes, row_bytes);
    for (int64_t tokens : {1, 3, 5}) {
        for (int64_t input_rows : {1, 2}) {
            for (auto buft : {ggml_backend_cpu_buffer_type(), ime}) {
            auto * wc = ggml_init({ggml_tensor_overhead() * 4, nullptr, true});
            auto * ac = ggml_init({ggml_tensor_overhead() * 12 + ggml_graph_overhead_custom(32, false), nullptr, true});
            auto * w = ggml_new_tensor_3d(wc, GGML_TYPE_Q4_K, k, n, 2);
            auto * x = ggml_new_tensor_3d(ac, GGML_TYPE_F32, k, input_rows, tokens);
            auto * ids = ggml_new_tensor_2d(ac, GGML_TYPE_I32, 2, tokens);
            auto * y = ggml_mul_mat_id(ac, w, x, ids);
            auto * graph = ggml_new_graph_custom(ac, 32, false);
            ggml_build_forward_expand(graph, y);
            std::mt19937 rng(20261014);
            std::uniform_real_distribution<float> dist(-1, 1);
            std::vector<float> input(k * input_rows * tokens);
            for (auto & value : input) value = dist(rng);
            std::vector<int32_t> choices(2 * tokens);
            for (int64_t t = 0; t < tokens; t++)
                for (int64_t r = 0; r < 2; r++) choices[t * 2 + r] = (t + r) % 2;
            std::vector<double> reference(n * 2 * tokens);
            for (int64_t t = 0; t < tokens; t++) {
                for (int64_t r = 0; r < 2; r++) {
                    for (int64_t col = 0; col < n; col++) {
                        const int64_t weight_row = choices[t * 2 + r] ? n - 1 - col : col;
                        double sum = 0;
                        for (int64_t p = 0; p < k; p++)
                            sum += double(w_float[weight_row * k + p]) * input[(t * input_rows + r % input_rows) * k + p];
                        reference[(t * 2 + r) * n + col] = sum;
                    }
                }
            }
                auto wb = ggml_backend_alloc_ctx_tensors_from_buft(wc, buft);
                auto ab = ggml_backend_alloc_ctx_tensors(ac, backend);
                if (!wb || !ab) throw std::runtime_error("MoE allocation failed");
                ggml_backend_tensor_set(w, experts.data(), 0, experts.size());
                ggml_backend_tensor_set(x, input.data(), 0, input.size() * sizeof(float));
                ggml_backend_tensor_set(ids, choices.data(), 0, choices.size() * sizeof(int32_t));
                if (ggml_backend_graph_compute(backend, graph) != GGML_STATUS_SUCCESS)
                    throw std::runtime_error("MoE compute failed");
                std::vector<float> output(reference.size());
                ggml_backend_tensor_get(y, output.data(), 0, output.size() * sizeof(float));
                auto stats = difference(output, reference);
                std::cout << "{\"event\":\"moe_operator\",\"tokens\":" << tokens
                          << ",\"input_rows\":" << input_rows << ",\"buffer\":\"" << ggml_backend_buft_name(buft)
                          << "\",\"relative_rms\":" << stats.relative_rms << ",\"max_abs\":" << stats.max_abs << "}" << std::endl;
                if (stats.relative_rms > 0.015) throw std::runtime_error("MoE numeric tolerance exceeded");
                ggml_backend_buffer_free(wb);
                ggml_backend_buffer_free(ab);
                ggml_free(wc);
                ggml_free(ac);
            }
        }
    }
}

int main(int argc, char ** argv) {
    if (argc < 2 || argc > 3) return 2;
    fp32_offset_format = argc == 3 && std::string(argv[2]) == "--fp32-offset";
    try {
        auto backend = ggml_backend_cpu_init();
        ggml_backend_cpu_set_n_threads(backend, 4);
        auto dev = ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_CPU);
        auto reg = ggml_backend_dev_backend_reg(dev);
        auto extra_fn = reinterpret_cast<ggml_backend_dev_get_extra_bufts_t>(
            ggml_backend_reg_get_proc_address(reg, "ggml_backend_dev_get_extra_bufts"));
        if (!extra_fn) throw std::runtime_error("No extra-buffer entry point");
        ggml_backend_buffer_type_t ime = nullptr;
        for (auto * p = extra_fn(dev); p && *p; p++) {
            if (std::string(ggml_backend_buft_name(*p)) == "CPU_RISCV64_SPACEMIT") ime = *p;
        }
        if (!ime) throw std::runtime_error("SpacemiT extra buffer missing");
        std::cout << std::setprecision(12);
        std::cout << "{\"event\":\"setup\",\"ime_buffer\":\"" << ggml_backend_buft_name(ime)
                  << "\",\"generic_activation_type\":\""
                  << ggml_type_name(ggml_get_type_traits_cpu(GGML_TYPE_Q4_K)->vec_dot_type) << "\"}" << std::endl;
        ggml_context * meta = nullptr;
        auto * gguf = gguf_init_from_file(argv[1], {true, &meta});
        if (!gguf || !meta) throw std::runtime_error("Cannot read GGUF metadata");
        std::ifstream file(argv[1], std::ios::binary);
        int selected = 0;
        for (const char * name : {"blk.0.attn_q.weight", "blk.14.attn_q.weight", "blk.27.ffn_up.weight"}) {
            int64_t id = gguf_find_tensor(gguf, name);
            if (id < 0 || gguf_get_tensor_type(gguf, id) != GGML_TYPE_Q4_K) continue;
            auto * t = ggml_get_tensor(meta, name);
            int64_t k = t->ne[0], n = std::min<int64_t>(64, t->ne[1]);
            if (n % 16 || k % 256) throw std::runtime_error("Unexpected model shape");
            std::vector<uint8_t> weights(ggml_row_size(GGML_TYPE_Q4_K, k) * n);
            file.seekg(gguf_get_data_offset(gguf) + gguf_get_tensor_offset(gguf, id));
            file.read(reinterpret_cast<char *>(weights.data()), weights.size());
            if (!file) throw std::runtime_error("Cannot read weight rows");
            std::vector<float> w_float(k * n);
            ggml_get_type_traits(GGML_TYPE_Q4_K)->to_float(weights.data(), w_float.data(), k * n);
            std::vector<float> zp_weights;
            auto rounding = repack_rounding(weights, w_float, zp_weights);
            std::vector<double> original_weights(w_float.begin(), w_float.end());
            auto zp_loss = difference(zp_weights, original_weights);
            std::cout << "{\"event\":\"weight_rounding\",\"tensor\":\"" << name
                      << "\",\"k\":" << k << ",\"rows\":" << n
                      << ",\"relative_rms\":" << rounding.relative_rms
                      << ",\"max_abs\":" << rounding.max_abs
                      << ",\"integer_zero_point_relative_rms\":" << zp_loss.relative_rms
                      << ",\"integer_zero_point_max_abs\":" << zp_loss.max_abs << "}" << std::endl;
            for (int64_t m : {1, 2, 3, 4, 5, 7, 16, 104}) {
                std::mt19937 rng(20261013);
                std::uniform_real_distribution<float> dist(-1, 1);
                std::vector<float> input(k * m);
                for (auto & x : input) x = dist(rng);
                std::vector<double> reference(n * m);
                std::vector<double> zp_reference(n * m);
                for (int64_t row = 0; row < m; row++) {
                    for (int64_t col = 0; col < n; col++) {
                        double sum = 0;
                        double zp_sum = 0;
                        for (int64_t i = 0; i < k; i++) {
                            sum += double(w_float[col * k + i]) * input[row * k + i];
                            zp_sum += double(zp_weights[col * k + i]) * input[row * k + i];
                        }
                        reference[row * n + col] = sum;
                        zp_reference[row * n + col] = zp_sum;
                    }
                }
                auto cpu = multiply(ggml_backend_cpu_buffer_type(), backend, k, n, m, weights, input);
                std::vector<float> actual_packed_weights;
                auto accelerated = multiply(ime, backend, k, n, m, weights, input, &actual_packed_weights);
                auto cs = difference(cpu, reference);
                auto is = difference(accelerated, reference);
                std::vector<double> cpu_reference(cpu.begin(), cpu.end());
                auto between = difference(accelerated, cpu_reference);
                auto vs_zp = difference(accelerated, zp_reference);
                const auto & packed_reference = fp32_offset_format ? w_float : zp_weights;
                std::vector<double> reconstructed_ref(packed_reference.begin(), packed_reference.end());
                auto packed_match = difference(actual_packed_weights, reconstructed_ref);
                if (fp32_offset_format && (is.relative_rms > 0.015 || packed_match.max_abs > 1e-6))
                    throw std::runtime_error("Patched operator/packed weights exceeded numerical tolerance");
                std::cout << "{\"event\":\"operator\",\"tensor\":\"" << name
                          << "\",\"m\":" << m << ",\"k\":" << k << ",\"n\":" << n
                          << ",\"cpu_vs_dequant_relative_rms\":" << cs.relative_rms
                          << ",\"ime_vs_dequant_relative_rms\":" << is.relative_rms
                          << ",\"ime_vs_cpu_relative_rms\":" << between.relative_rms
                          << ",\"ime_vs_cpu_max_abs\":" << between.max_abs
                          << ",\"ime_vs_zero_point_reference_relative_rms\":" << vs_zp.relative_rms
                          << ",\"ime_vs_zero_point_reference_max_abs\":" << vs_zp.max_abs
                          << ",\"actual_packed_vs_reconstructed_weights_max_abs\":" << packed_match.max_abs
                          << "}" << std::endl;
            }
            selected++;
            if (fp32_offset_format && selected == 1) check_moe(ime, backend, k, n, weights, w_float);
        }
        if (selected != 3) throw std::runtime_error("Not all requested Q4_K tensors found");
        gguf_free(gguf);
        ggml_free(meta);
        ggml_backend_free(backend);
        return 0;
    } catch (const std::exception & ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
}
