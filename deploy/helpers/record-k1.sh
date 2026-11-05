#!/usr/bin/env bash
# Wayland + wf-recorder: record screen + system playback + USB microphone.
set -Eeuo pipefail

for cmd in pactl wf-recorder taskset python3 ffmpeg ffprobe awk date; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "错误：缺少命令 $cmd。请先安装相应软件。" >&2
        exit 1
    fi
done

if [[ -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo '错误：没有检测到 WAYLAND_DISPLAY。请直接在 K1 的 Wayland 桌面终端中运行。' >&2
    exit 1
fi

finalizer="$HOME/.local/libexec/okrva-finalize-recording.py"
if [[ ! -r "$finalizer" ]]; then
    echo '错误：请先安装deploy/helpers/finalize-recording.py到~/.local/libexec/okrva-finalize-recording.py。' >&2
    exit 1
fi

original_sink=$(pactl get-default-sink)
original_source=$(pactl get-default-source)

mapfile -t sinks < <(pactl list short sinks | awk '{print $2}')
if ((${#sinks[@]} == 0)); then
    echo '错误：没有找到音频输出设备。' >&2
    exit 1
fi

echo '=== 可用的系统声音输出设备 ==='
default_index=1
for i in "${!sinks[@]}"; do
    flag=''
    if [[ "${sinks[$i]}" == "$original_sink" ]]; then
        flag=' [当前默认]'
        default_index=$((i + 1))
    fi
    printf '%d) %s%s\n' "$((i + 1))" "${sinks[$i]}" "$flag"
done

echo
echo '请选择「浏览器/播放器实际正在出声」的设备。'
echo '注意：当前默认设备不一定是正在使用的扬声器。'
read -r -p "输入编号（直接回车选 $default_index）：" choice
choice=${choice:-$default_index}
if ! [[ "$choice" =~ ^[0-9]+$ ]] || ((10#$choice < 1 || 10#$choice > ${#sinks[@]})); then
    echo '错误：选择的编号无效。' >&2
    exit 1
fi
selected_sink=${sinks[$((10#$choice - 1))]}
selected_mic=$original_source
if [[ "$selected_mic" == *.monitor ]]; then
    echo "错误：默认输入源是监听设备 $selected_mic，而不是麦克风。" >&2
    echo '请先在系统音频设置中选择 USB 摄像头麦克风，再运行脚本。' >&2
    exit 1
fi

mix_name="wfrec_mix_$$"
null_id=''
system_id=''
mic_id=''
started=0
recorder_pid=''
file="$HOME/recording-$(date '+%Y%m%d-%H%M%S').mp4"

cleanup() {
    local status=$?
    trap - EXIT INT TERM
    if [[ -n "$recorder_pid" ]] && kill -0 "$recorder_pid" 2>/dev/null; then
        kill -INT "$recorder_pid" 2>/dev/null || true
        wait "$recorder_pid" 2>/dev/null || true
    fi
    # Only remove modules loaded by this invocation.
    for module_id in "$mic_id" "$system_id" "$null_id"; do
        if [[ -n "$module_id" ]]; then
            pactl unload-module "$module_id" >/dev/null 2>&1 || true
        fi
    done
    pactl set-default-sink "$original_sink" >/dev/null 2>&1 || true
    pactl set-default-source "$original_source" >/dev/null 2>&1 || true
    if ((started)); then
        if [[ -s "$file" ]]; then
            printf '\n录制已停止，正在整理视频封装……\n'
            if python3 "$finalizer" "$file"; then
                printf '视频文件：%s\n' "$file"
            else
                printf '封装检查未通过，原始录制文件已保留：%s\n' "$file" >&2
                status=1
            fi
        else
            printf '\n录屏未生成有效视频，请查看上方错误信息。\n' >&2
        fi
    fi
    exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

printf '\n系统播放源：%s.monitor\n' "$selected_sink"
printf '麦克风输入源：%s\n' "$selected_mic"

null_id=$(pactl load-module module-null-sink "sink_name=$mix_name")
system_id=$(pactl load-module module-loopback "source=${selected_sink}.monitor" "sink=$mix_name")
mic_id=$(pactl load-module module-loopback "source=$selected_mic" "sink=$mix_name")

# Creating a virtual sink must not make it the default playback device.
pactl set-default-sink "$original_sink"
pactl set-default-source "$original_source"

# The monitor may take a moment to appear in PipeWire.
ready=0
for ((i=0; i<15; i++)); do
    if pactl list short sources | awk '{print $2}' | grep -Fxq "${mix_name}.monitor"; then
        ready=1
        break
    fi
    sleep 0.2
done
if ((ready == 0)); then
    echo '错误：虚拟混音设备没有出现。' >&2
    exit 1
fi

printf '\n开始录制画面 + 系统声音 + 麦克风。按 Ctrl+C 停止。\n'
printf '输出文件：%s\n\n' "$file"
started=1
record_log_dir="$HOME/.cache/okrva-recording"
mkdir -p "$record_log_dir"
record_log="$record_log_dir/$(basename "$file").log"
printf '配置：720p、25帧、H.264硬件编码；采集与音频使用CPU4–7。\n'
printf '录屏初始化中，请稍候……\n'
taskset -c 4-7 wf-recorder --audio="${mix_name}.monitor" \
    -r 25 -B 25 -F scale=1280:-2:flags=fast_bilinear -b 0 \
    -c h264_stcodec -x yuv420p -p threads=2 -f "$file" >"$record_log" 2>&1 &
recorder_pid=$!
audio_ready=0
for ((i=0; i<250; i++)); do
    if grep -Fq 'Using PulseAudio device:' "$record_log"; then
        audio_ready=1
        break
    fi
    if ! kill -0 "$recorder_pid" 2>/dev/null; then
        echo "错误：录屏启动失败，请查看$record_log。" >&2
        exit 1
    fi
    sleep 0.1
done
if ((audio_ready == 0)); then
    echo "错误：录屏初始化超时，请查看$record_log。" >&2
    exit 1
fi
printf '录屏已就绪，可以开始演示；按Ctrl+C停止。\n'
wait "$recorder_pid"
recorder_pid=''
