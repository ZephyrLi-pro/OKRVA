// Private hybrid-layout/lifetime check. No system configuration changes.
#include <ggml.h>
#include <ggml-alloc.h>
#include <ggml-backend.h>
#include <ggml-cpu.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

static void require(bool ok, const char * message) {
    if (!ok) throw std::runtime_error(message);
}

static void check(ggml_backend_t backend, ggml_backend_buffer_type_t ime, int k, int n, int m) {
    auto * wc = ggml_init({ggml_tensor_overhead() * 4, nullptr, true});
    auto * ac = ggml_init({ggml_tensor_overhead() * 12 + ggml_graph_overhead_custom(32, false), nullptr, true});
    require(wc && ac, "Context allocation");
    auto * w = ggml_new_tensor_2d(wc, GGML_TYPE_Q4_K, k, n);
    auto * native = ggml_new_tensor_2d(wc, GGML_TYPE_Q4_K, k, n);
    auto * x = ggml_new_tensor_2d(ac, GGML_TYPE_F32, k, m);
    ggml_set_name(w, "hybrid_weight");
    ggml_set_name(native, "cpu_weight");
    // Allocate each weight separately so clear() tests sidecar lifetime too.
    auto wb = ggml_backend_buft_alloc_buffer(ime, ggml_backend_buft_get_alloc_size(ime, w) + 64);
    auto cb = ggml_backend_buft_alloc_buffer(ggml_backend_cpu_buffer_type(), ggml_nbytes(native) + 64);
    require(wb && cb, "Weight allocation");
    require(ggml_backend_tensor_alloc(wb, w, ggml_backend_buffer_get_base(wb)) == GGML_STATUS_SUCCESS, "IME alloc");
    require(ggml_backend_tensor_alloc(cb, native, ggml_backend_buffer_get_base(cb)) == GGML_STATUS_SUCCESS, "CPU alloc");
    auto * y = ggml_mul_mat(ac, w, x);
    auto * z = ggml_mul_mat(ac, native, x);
    auto * graph = ggml_new_graph_custom(ac, 32, false);
    ggml_build_forward_expand(graph, y);
    ggml_build_forward_expand(graph, z);
    auto ab = ggml_backend_alloc_ctx_tensors(ac, backend);
    require(ab, "Activation allocation");
    const size_t packed = size_t(n / 16) * (k / 32) * 384;
    const size_t raw_size = ggml_nbytes(w);
    require(ggml_backend_buft_get_alloc_size(ime, w) == packed + raw_size, "Missing accounted sidecar");
    std::vector<float> input(k * m), values(k * n), result(n * m), reference(n * m);
    std::vector<uint8_t> raw(raw_size);
    std::mt19937 rng(20261014 + k + n + m);
    std::uniform_real_distribution<float> dist(-1, 1);
    for (auto & value : input) value = dist(rng);
    ggml_backend_tensor_set(x, input.data(), 0, input.size() * sizeof(float));
    // 2 loads, a partial native-byte memset, whole-buffer zero, then reload.
    for (int phase = 0; phase < 5; phase++) {
        if (phase == 2) {
            const size_t offset = ggml_row_size(GGML_TYPE_Q4_K, k);
            const size_t length = ggml_type_size(GGML_TYPE_Q4_K);
            std::memset(raw.data() + offset, 0, length);
            ggml_backend_tensor_memset(w, 0, offset, length);
        } else if (phase == 3) {
            std::fill(raw.begin(), raw.end(), 0);
            ggml_backend_buffer_clear(wb, 0);
        } else {
            for (auto & value : values) value = dist(rng);
            const size_t encoded = ggml_quantize_chunk(GGML_TYPE_Q4_K, values.data(), raw.data(), 0, n, k, nullptr);
            require(encoded == raw_size, "Quantized size");
            ggml_backend_tensor_set(w, raw.data(), 0, raw.size());
        }
        require(std::memcmp((char *) w->data + packed, raw.data(), raw.size()) == 0, "Sidecar bytes changed");
        ggml_backend_tensor_set(native, raw.data(), 0, raw.size());
        require(ggml_backend_graph_compute(backend, graph) == GGML_STATUS_SUCCESS, "Graph failed");
        ggml_backend_tensor_get(y, result.data(), 0, result.size() * sizeof(float));
        ggml_backend_tensor_get(z, reference.data(), 0, reference.size() * sizeof(float));
        double error = 0, norm = 0;
        for (size_t i = 0; i < result.size(); i++) {
            require(std::isfinite(result[i]), "Nonfinite output");
            error += std::pow(double(result[i]) - reference[i], 2);
            norm += double(reference[i]) * reference[i];
        }
        if (m == 1) require(std::memcmp(result.data(), reference.data(), result.size() * sizeof(float)) == 0,
                            "CPU decode is not bit-exact");
        else require(std::sqrt(error / std::max(1e-30, norm)) < 0.02, "Prefill tolerance");
        std::cout << "{\"event\":\"hybrid_check\",\"k\":" << k << ",\"n\":" << n
                  << ",\"m\":" << m << ",\"phase\":" << phase
                  << ",\"cpu_bit_exact\":" << (m == 1 ? "true" : "false") << "}" << std::endl;
    }
    ggml_backend_buffer_free(ab);
    ggml_backend_buffer_free(wb);
    ggml_backend_buffer_free(cb);
    ggml_free(ac);
    ggml_free(wc);
}

int main() {
    try {
        auto backend = ggml_backend_cpu_init();
        require(backend, "Backend");
        ggml_backend_cpu_set_n_threads(backend, 4);
        auto dev = ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_CPU);
        auto reg = ggml_backend_dev_backend_reg(dev);
        auto fn = reinterpret_cast<ggml_backend_dev_get_extra_bufts_t>(
            ggml_backend_reg_get_proc_address(reg, "ggml_backend_dev_get_extra_bufts"));
        require(fn, "Extra buffers");
        ggml_backend_buffer_type_t ime = nullptr;
        for (auto p = fn(dev); p && *p; p++)
            if (std::string(ggml_backend_buft_name(*p)) == "CPU_RISCV64_SPACEMIT") ime = *p;
        require(ime, "IME buffer");
        for (int k : {256, 1024}) for (int n : {16, 32, 80}) for (int m : {1, 2, 5}) check(backend, ime, k, n, m);
        ggml_backend_free(backend);
    } catch (const std::exception & error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
