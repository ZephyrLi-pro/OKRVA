#define main numeric_main
#include "test_q6.cpp"
#undef main
int main(int argc,char **argv) {
    const bool packed=argc==2 && std::string(argv[1])=="--repack";
    auto backend=ggml_backend_cpu_init();ggml_backend_cpu_set_n_threads(backend,4);
    std::mt19937 rng(123);std::uniform_real_distribution<float> dist(-3,3);
    for(int n:{256,4096})for(int m:{1,32}) {
        const int k=1024;
        std::vector<float>wf(k*n),x(k*m);for(auto &v:wf)v=dist(rng);for(auto &v:x)v=dist(rng);
        std::vector<uint8_t>w(ggml_row_size(GGML_TYPE_Q6_K,k)*n);
        ggml_quantize_chunk(GGML_TYPE_Q6_K,wf.data(),w.data(),0,n,k,nullptr);
        job j(backend,packed,k,n,m,w,x);j.compute();j.compute();
        std::vector<double> times;
        for(int sample=0;sample<5;sample++) {
            const int repeat=10;
            auto start=std::chrono::steady_clock::now();for(int i=0;i<repeat;i++)j.compute();
            double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/repeat;
            times.push_back(ms);
        }
        std::sort(times.begin(),times.end());auto values=j.values();
        std::cout<<"{\"k\":"<<k<<",\"n\":"<<n<<",\"m\":"<<m<<",\"median_ms\":"<<times[2]
                 <<",\"min_ms\":"<<times.front()<<",\"max_ms\":"<<times.back()
                 <<",\"checksum\":"<<values[0]<<"}"<<std::endl;
    }
    ggml_backend_free(backend);
}
