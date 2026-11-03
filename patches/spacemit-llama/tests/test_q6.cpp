#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include "ggml-impl.h"
#include "gguf.h"
#include "repack.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" void ggml_quantize_mat_q8_K_4x1(const float*,void*,int64_t);

struct job {
    ggml_context *wc=nullptr,*ac=nullptr;
    ggml_backend_buffer_t wb=nullptr,ab=nullptr;
    ggml_backend_t backend;
    ggml_cgraph *graph;
    ggml_tensor *w,*x,*y;
    int64_t k,n,m;
    job(ggml_backend_t b, bool packed, int64_t k_,int64_t n_,int64_t m_,
        const std::vector<uint8_t>&weights,const std::vector<float>&input):backend(b),k(k_),n(n_),m(m_) {
        wc=ggml_init({ggml_tensor_overhead()*4,nullptr,true});
        ac=ggml_init({ggml_tensor_overhead()*12+ggml_graph_overhead_custom(32,false),nullptr,true});
        if (!wc || !ac) throw std::runtime_error("contexts");
        w=ggml_new_tensor_2d(wc,GGML_TYPE_Q6_K,k,n);
        x=ggml_new_tensor_2d(ac,GGML_TYPE_F32,k,m);
        y=ggml_mul_mat(ac,w,x);
        graph=ggml_new_graph_custom(ac,32,false);ggml_build_forward_expand(graph,y);
        auto buft=packed?ggml_backend_cpu_repack_buffer_type():ggml_backend_cpu_buffer_type();
        if (packed) {
            auto dev=ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_CPU);
            auto fn=reinterpret_cast<ggml_backend_dev_get_extra_bufts_t>(ggml_backend_reg_get_proc_address(
                ggml_backend_dev_backend_reg(dev),"ggml_backend_dev_get_extra_bufts"));
            bool registered=false;
            for(auto *p=fn?fn(dev):nullptr;p && *p;p++)registered|=*p==buft;
            if(!registered)throw std::runtime_error("REPACK dispatch not registered");
        }
        wb=ggml_backend_alloc_ctx_tensors_from_buft(wc,buft);
        if (packed && !w->extra) throw std::runtime_error("Q6 repack trait not registered");
        ab=ggml_backend_alloc_ctx_tensors(ac,backend);
        if (!wb || !ab) throw std::runtime_error("buffers");
        ggml_backend_tensor_set(w,weights.data(),0,weights.size());
        ggml_backend_tensor_set(x,input.data(),0,input.size()*sizeof(float));
    }
    void compute() {
        if (ggml_backend_graph_compute(backend,graph)!=GGML_STATUS_SUCCESS) throw std::runtime_error("compute");
    }
    std::vector<float> values() {
        std::vector<float> out(n*m);ggml_backend_tensor_get(y,out.data(),0,out.size()*sizeof(float));return out;
    }
    ~job() {ggml_backend_buffer_free(wb);ggml_backend_buffer_free(ab);ggml_free(wc);ggml_free(ac);}
};

std::vector<double> reference(const std::vector<uint8_t>&weights,const std::vector<float>&input,int k,int n,int m,bool packed=false) {
    std::vector<float> w(k*n);ggml_get_type_traits(GGML_TYPE_Q6_K)->to_float(weights.data(),w.data(),k*n);
    std::vector<uint8_t> bytes(ggml_row_size(GGML_TYPE_Q8_K,k));
    std::vector<uint8_t> four(ggml_row_size(GGML_TYPE_Q8_K,k)*4);
    std::vector<float> x(k);std::vector<double> output(n*m);
    for (int row=0;row<m;row++) {
        if (packed && row<m-m%4) {
            // The existing dispatcher uses the four-row quantizer for GEMM.
            // Compare to those exact input bytes, not the separate RVV row
            // quantizer, which can differ at a rounding boundary.
            if (row%4==0) ggml_quantize_mat_q8_K_4x1(input.data()+row*k,four.data(),k);
            const auto *q=reinterpret_cast<const block_q8_Kx4*>(four.data());
            for(int b=0;b<k/QK_K;b++)for(int i=0;i<QK_K;i++)x[b*QK_K+i]=q[b].d[row%4]*q[b].qs[i*4+row%4];
        } else {
            ggml_get_type_traits_cpu(GGML_TYPE_Q8_K)->from_float(input.data()+row*k,bytes.data(),k);
            const auto *q=reinterpret_cast<const block_q8_K*>(bytes.data());
            for (int b=0;b<k/QK_K;b++)for (int i=0;i<QK_K;i++)x[b*QK_K+i]=q[b].d*q[b].qs[i];
        }
        for (int col=0;col<n;col++) {
            double sum=0;for (int i=0;i<k;i++)sum+=double(w[col*k+i])*x[i];
            output[row*n+col]=sum;
        }
    }
    return output;
}

void check(ggml_backend_t backend,bool packed,int k,int n,int m,const std::vector<uint8_t>&w,
           const std::vector<float>&x,const char *label) {
    auto ref=reference(w,x,k,n,m,packed);job j(backend,packed,k,n,m,w,x);j.compute();auto out=j.values();
    double error=0,norm=0,maxabs=0;
    for(size_t i=0;i<out.size();i++) {
        if (!std::isfinite(out[i])) throw std::runtime_error("non-finite");
        double d=out[i]-ref[i];error+=d*d;norm+=ref[i]*ref[i];maxabs=std::max(maxabs,std::abs(d));
    }
    double relative=std::sqrt(error/std::max(1e-30,norm));
    std::cout<<"{\"label\":\""<<label<<"\",\"k\":"<<k<<",\"n\":"<<n<<",\"m\":"<<m
       <<",\"packed\":"<<(packed?"true":"false")<<",\"relative_rms\":"<<relative<<",\"max_abs\":"<<maxabs<<"}"<<std::endl;
    if (relative>2e-5 || (norm==0 && maxabs!=0)) throw std::runtime_error("Q6 numeric error");
}

int main(int argc,char **argv) {
    try {
        if (argc<2 || argc>3) return 1;
        const bool packed=argc==3 && std::string(argv[2])=="--repack";
        auto backend=ggml_backend_cpu_init();ggml_backend_cpu_set_n_threads(backend,4);
        std::mt19937 rng(42);std::uniform_real_distribution<float> dist(-3,3);
        int cases=0;
        for(int k:{256,1024})for(int n:{16,32})for(int m:{1,2,3,4,5,7,16,33})for(int pattern=0;pattern<3;pattern++) {
            std::vector<float> wf(k*n),x(k*m);
            for(size_t i=0;i<wf.size();i++)wf[i]=pattern==0?0:pattern==1?((i%2)?3:-3):dist(rng);
            for(size_t i=0;i<x.size();i++)x[i]=pattern==0?0:pattern==1?((i%3)?127:-127):dist(rng);
            std::vector<uint8_t>w(ggml_row_size(GGML_TYPE_Q6_K,k)*n);
            if(ggml_quantize_chunk(GGML_TYPE_Q6_K,wf.data(),w.data(),0,n,k,nullptr)!=w.size())throw std::runtime_error("quantize");
            check(backend,packed,k,n,m,w,x,"synthetic");cases++;
        }
        ggml_context *meta=nullptr;auto *file=gguf_init_from_file(argv[1],{true,&meta});
        if(!file || !meta)throw std::runtime_error("model metadata");
        std::ifstream input(argv[1],std::ios::binary);int real=0;
        for(int64_t i=0;i<gguf_get_n_tensors(file) && real<3;i++) {
            if(gguf_get_tensor_type(file,i)!=GGML_TYPE_Q6_K)continue;
            auto *t=ggml_get_tensor(meta,gguf_get_tensor_name(file,i));
            if(ggml_n_dims(t)!=2 || t->ne[1]<16)continue;
            const int k=t->ne[0],n=std::min<int64_t>(32,t->ne[1]/16*16);
            std::vector<uint8_t>w(ggml_row_size(GGML_TYPE_Q6_K,k)*n);
            input.seekg(gguf_get_data_offset(file)+gguf_get_tensor_offset(file,i));
            input.read(reinterpret_cast<char*>(w.data()),w.size());if(!input)throw std::runtime_error("model rows");
            for(int m:{1,5,16}) {
                std::vector<float>x(k*m);for(auto &v:x)v=dist(rng);
                check(backend,packed,k,n,m,w,x,t->name);cases++;
            }
            real++;
        }
        if(!real)throw std::runtime_error("No actual Q6 weights tested");
        gguf_free(file);ggml_free(meta);ggml_backend_free(backend);
        std::cout<<"{\"passed\":true,\"cases\":"<<cases<<",\"real_weight_tensors\":"<<real<<"}"<<std::endl;
        return 0;
    } catch(const std::exception&e) {std::cerr<<e.what()<<std::endl;return 2;}
}
