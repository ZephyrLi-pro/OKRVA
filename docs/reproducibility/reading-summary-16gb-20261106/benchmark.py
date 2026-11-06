#!/usr/bin/env python3
"""Measure fixed SSE requests against an existing Adapter; no service changes."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import statistics
import time
import urllib.request

HERE = Path(__file__).resolve().parent
BACKEND = "k1-reading-summary"


def headers(args):
    result = {"Content-Type": "application/json", "Accept": "text/event-stream"}
    if args.bearer_token_env:
        token = os.environ.get(args.bearer_token_env)
        if not token:
            raise ValueError("指定的凭据环境变量为空")
        result["Authorization"] = "Bearer " + token
    return result


def health(args):
    req = urllib.request.Request(args.base_url.rstrip("/") + "/health", headers=headers(args))
    with urllib.request.urlopen(req, timeout=5) as response:
        return json.load(response)


def backend(info):
    return next((p for p in info.get("processes", []) if p.get("name") == BACKEND), {})


def server_hwm(root_pid):
    """Read the real llama-server descendant, excluding its Python supervisor."""
    pending = [int(root_pid)] if root_pid else []
    visited = set()
    result = []
    while pending:
        pid = pending.pop()
        if pid in visited:
            continue
        visited.add(pid)
        base = Path("/proc") / str(pid)
        try:
            status = dict(line.split(":", 1) for line in (base / "status").read_text().splitlines())
            if status.get("Name", "").strip() == "llama-server":
                result.append({"process_name": "llama-server", "VmHWM_mib": int(status["VmHWM"].split()[0]) / 1024})
            children = base / "task" / str(pid) / "children"
            pending.extend(int(p) for p in children.read_text().split())
        except (OSError, KeyError, ValueError):
            continue
    return result


def sse_events(response):
    data = []
    for raw_line in response:
        line = raw_line.decode("utf-8").rstrip("\r\n")
        if not line:
            if data:
                yield "\n".join(data)
                data = []
        elif line.startswith("data:"):
            data.append(line[5:].lstrip(" "))
    if data:
        yield "\n".join(data)


def measure(args, request, index, before):
    body = json.dumps(request, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    req = urllib.request.Request(args.base_url.rstrip("/") + "/v1/chat/completions", data=body, headers=headers(args))
    row = {"case": index, "backend_running_before": backend(before).get("running"), "passed": False}
    parts = []
    first = None
    finish = None
    done = False
    start = time.perf_counter()
    with urllib.request.urlopen(req, timeout=args.timeout) as response:
        row["selected_backend"] = response.headers.get("X-RVAI-Backend")
        for raw in sse_events(response):
            if raw == "[DONE]":
                done = True
                break
            event = json.loads(raw)
            if event.get("error"):
                raise RuntimeError("模型接口返回错误；检查Adapter服务日志")
            for key in ("timings", "usage"):
                if event.get(key):
                    row[key] = event[key]
            for choice in event.get("choices", []):
                finish = choice.get("finish_reason") or finish
                content = choice.get("delta", {}).get("content")
                if content:
                    if first is None:
                        first = time.perf_counter() - start
                    parts.append(content)
    row.update(first_text_seconds=first, total_text_seconds=time.perf_counter() - start,
               answer="".join(parts), finish=finish, done=done,
               passed=bool(done and parts and finish == "stop"))
    return row


def summary(data):
    cases = data["cases"]
    ready = [r for r in cases if r.get("backend_running_before") is True and r.get("passed")]
    if not ready:
        raise ValueError("没有正常完成的模型就绪请求")
    rates = [r["timings"]["predicted_n"] / r["timings"]["predicted_ms"] * 1000 for r in ready]
    prefill = [r["timings"]["prompt_n"] / r["timings"]["prompt_ms"] * 1000 for r in ready]
    return {"completed_cases": sum(bool(r.get("passed")) for r in cases), "ready_cases": len(ready),
            "ready_mean_generation_token_s": statistics.mean(rates),
            "ready_mean_prefill_token_s": statistics.mean(prefill),
            "ready_mean_first_text_s": statistics.mean(r["first_text_seconds"] for r in ready),
            "ready_mean_total_text_s": statistics.mean(r["total_text_seconds"] for r in ready)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-url", default="http://127.0.0.1:18080", help="已运行的Adapter根地址")
    parser.add_argument("--request", type=Path, default=HERE / "request.json")
    parser.add_argument("--output", type=Path, default=Path("measurements.local.json"), help="新的结果文件，不覆盖已有文件")
    parser.add_argument("--requests", type=int, default=3)
    parser.add_argument("--timeout", type=float, default=600)
    parser.add_argument("--bearer-token-env", help="可选：提供Bearer凭据的环境变量名；凭据不写入结果")
    parser.add_argument("--summarize", type=Path, help="仅复算已有JSON，不访问服务")
    args = parser.parse_args()
    if args.summarize:
        print(json.dumps(summary(json.loads(args.summarize.read_text(encoding="utf-8"))), ensure_ascii=False, indent=2))
        return
    if args.requests < 1 or args.timeout <= 0:
        parser.error("requests和timeout必须为正数")
    if args.output.exists():
        parser.error("结果文件已存在，请更换--output路径")
    request = json.loads(args.request.read_text(encoding="utf-8"))
    if not request.get("stream"):
        parser.error("请求需要stream=true")
    initial = health(args)
    if any(v.get("active", 0) for v in initial.get("scheduler", {}).values()):
        parser.error("当前有模型请求执行中，请空闲后重试")
    data = {"measured_at_utc": datetime.now(timezone.utc).isoformat(), "request": request,
            "measurement_scope": "Adapter模型SSE请求，不包括唤醒、ASR、Agent规划或TTS播放", "cases": []}
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(data, stream, ensure_ascii=False, indent=2)
    for index in range(1, args.requests + 1):
        before = health(args)
        row = measure(args, request, index, before)
        data["cases"].append(row)
        args.output.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({k: row.get(k) for k in ("case", "backend_running_before", "first_text_seconds", "total_text_seconds", "passed")}, ensure_ascii=False), flush=True)
        if not row["passed"]:
            raise SystemExit("请求未正常完成，已保留本次结果；不计入成功均值")
    data["model_memory_after_requests"] = server_hwm(backend(health(args)).get("pid"))
    data["summary"] = summary(data)
    args.output.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(data["summary"], ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
