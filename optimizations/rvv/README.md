# 17项RVV算子优化

来源：用户仓库 https://github.com/ZephyrLi-pro/llama.cpp 。17项对应15个分支，TQ1两种VLEN及Q5两种格式分别共用分支。每个分支目录有完整修改文件和可应用patch；`branches.json`给出固定head/base及前后SHA256。

| 用户编号 | 分支 | 算子 | 变化 | 当前设备状态 |
|---|---|---|---|---|
| 1 | rvv-q6-k-vwmacc | ggml_vec_dot_q6_K_q8_K_vl256 | 宽乘加与单次规约 | 已集成 |
| 2、3 | rvv-tq1-vmacc | tq1_0 × q8_K vl256/vl512 | vmul+vmacc合并 | 独立分支 |
| 4 | rvv-iq4-nl-gemm-vwmacc | iq4_nl GEMM 16x1 | vwmacc累加 | 独立分支 |
| 5 | rvv-q4-0-gemm-loop-interchange | q4_0 GEMM 16x1 | 循环换序/权重复用 | 独立分支 |
| 6 | rvv-q5-k-vwmacc | q5_K × q8_K | 宽乘加/合并规约 | 独立分支 |
| 7 | rvv-q8-0-gemm-vwmacc | q8_0 GEMM 16x1 | 访问顺序/预取/宽乘加 | 独立分支 |
| 8 | riscv-iq3s-qh-vid | iq3_s × q8_K vl256 | vid生成索引 | 独立分支 |
| 9 | rvv-iq4-xs-cleanup | iq4_xs × q8_K vl256 | 移除屏障/复用零向量 | 独立分支 |
| 10 | riscv-tq2-accumulate-chunks | tq2_0 × q8_K vl256 | signed×unsigned宽乘加及ysum补偿 | 独立分支 |
| 11 | riscv-q4-1-vwmaccsu | q4_1 × q8_1 | 宽乘加减少转换 | 独立分支 |
| 12 | riscv-iq1-m-drop-barriers | iq1_m × q8_K vl256 | 常量外提/移除屏障 | 独立分支 |
| 13、14 | riscv-q5-vl256-fast-path | q5_0/q5_1 vl256 | 降低LMUL和寄存器压力 | 独立分支 |
| 15 | riscv-q2-0-vl256-vecdot | q2_0 × q8_0 vl256 | 新增RVV解包/点积 | 独立分支 |
| 16 | riscv-q8-0-4x1-rvv-quant | ggml_quantize_mat_q8_0_4x1 | RVV activation量化/repack | 独立分支 |
| 17 | rvv-q6-k-16x1-repack | q6_K 16x1 GEMV/GEMM | 16路重排及RVV快路径 | 已集成 |

当前正式集成仅第1、17项，经过vendor适配的最终结果包含在`patches/spacemit-llama/current-runtime.patch`。其他分支作为独立优化成果保存，不能写成全部已用于当前Qwen模型或全部在K1重测。

复现独立分支：从本仓库获取`branches.json`对应的base完整源码，在新目录执行`git -c core.autocrlf=false apply --check 分支.patch`再apply。各base不同；不要在同一树无条件叠加全部patch，也不要直接用某分支整份quants.c覆盖当前SpacemiT版本。

本次已对全部15个patch进行基线应用及修改后文件SHA验证；这属于源码完整性验证。用户此前K3单独算子/特定量化模型测速仍是其原测试条件下的数据，本包没有据此宣称K1五个场景的相同加速幅度。
