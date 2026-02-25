#!/usr/bin/env python3
"""Aliyun Bailian model test runner and expiry-aware smart router."""

from __future__ import annotations

import argparse
import asyncio
import json
import random
import statistics
import urllib.error
import urllib.request
from dataclasses import dataclass
from datetime import date, datetime
from pathlib import Path
from typing import Awaitable, Callable, Dict, Iterable, List, Optional, Sequence, Tuple

DEFAULT_ENDPOINT = "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"
DEFAULT_PROMPT = "用200字详细介绍你是干什么的，并告诉我你契合本公司该岗位的缘由"

MODEL_EXPIRIES: Tuple[Tuple[str, str], ...] = (
    ("deepseek-r1-distill-llama-70b", "2026-02-25"),
    ("qwen-plus-2025-12-01", "2026-03-01"),
    ("deepseek-v3.2", "2026-03-03"),
    ("qwen3-max-preview", "2026-03-03"),
    ("qwen3-vl-plus-2025-12-19", "2026-03-19"),
    ("glm-4.7", "2026-03-25"),
    ("tongyi-xiaomi-analysis-pro", "2026-04-09"),
    ("tongyi-xiaomi-analysis-flash", "2026-04-09"),
    ("qwen3-vl-flash-2026-01-22", "2026-04-22"),
    ("qwen3-max-2026-01-23", "2026-04-23"),
    ("MiniMax-M2.1", "2026-04-23"),
    ("kimi-k2.5", "2026-04-30"),
    ("qwen3-coder-next", "2026-05-03"),
    ("glm-5", "2026-05-13"),
    ("qwen3.5-plus-2026-02-15", "2026-05-16"),
    ("qwen3.5-397b-a17b", "2026-05-16"),
    ("qwen3.5-plus", "2026-05-16"),
)


@dataclass(frozen=True)
class ModelConfig:
    model: str
    expires_on: date


Requester = Callable[[str, str, str, str, float], Awaitable[Tuple[bool, float, int, str]]]


def parse_model_configs(raw_items: Iterable[Tuple[str, str]] = MODEL_EXPIRIES) -> List[ModelConfig]:
    return [
        ModelConfig(model=name, expires_on=datetime.strptime(expires_on, "%Y-%m-%d").date())
        for name, expires_on in raw_items
    ]


def compute_days_remaining(expires_on: date, today: Optional[date] = None) -> int:
    current = today or date.today()
    return (expires_on - current).days


def build_model_catalog(today: Optional[date] = None) -> List[Dict[str, object]]:
    current = today or date.today()
    rows: List[Dict[str, object]] = []
    for cfg in parse_model_configs():
        rows.append(
            {
                "model": cfg.model,
                "expires_on": cfg.expires_on.isoformat(),
                "days_remaining": compute_days_remaining(cfg.expires_on, current),
            }
        )
    return rows


def build_router_weight(config: ModelConfig, today: Optional[date] = None) -> float:
    days_remaining = compute_days_remaining(config.expires_on, today=today)
    if days_remaining < 0:
        return 0.0
    return max(1.0, 100.0 / float(days_remaining + 1))


def _blocking_chat_completion_request(
    model: str,
    prompt: str,
    api_key: str,
    endpoint: str,
    timeout_seconds: float,
) -> Tuple[bool, float, int, str]:
    start = datetime.now().timestamp()
    payload = {
        "model": model,
        "messages": [{"role": "user", "content": prompt}],
        "stream": False,
        "enable_thinking": False,
    }
    body = json.dumps(payload).encode("utf-8")
    headers = {
        "Authorization": f"Bearer {api_key}",
        "Content-Type": "application/json",
    }
    request = urllib.request.Request(endpoint, data=body, headers=headers, method="POST")

    try:
        with urllib.request.urlopen(request, timeout=timeout_seconds) as response:
            status = int(getattr(response, "status", 200))
            raw = response.read().decode("utf-8")
            data = json.loads(raw)
            choices = data.get("choices", [])
            ok = status == 200 and bool(choices)
            elapsed_ms = (datetime.now().timestamp() - start) * 1000.0
            return ok, elapsed_ms, status, ""
    except urllib.error.HTTPError as exc:
        elapsed_ms = (datetime.now().timestamp() - start) * 1000.0
        return False, elapsed_ms, int(exc.code), f"http_error:{exc.reason}"
    except Exception as exc:  # pragma: no cover - network uncertainty
        elapsed_ms = (datetime.now().timestamp() - start) * 1000.0
        return False, elapsed_ms, -1, f"exception:{exc}"


async def bailian_chat_completion_request(
    model: str,
    prompt: str,
    api_key: str,
    endpoint: str = DEFAULT_ENDPOINT,
    timeout_seconds: float = 30.0,
) -> Tuple[bool, float, int, str]:
    return await asyncio.to_thread(
        _blocking_chat_completion_request,
        model,
        prompt,
        api_key,
        endpoint,
        timeout_seconds,
    )


async def run_api_tests(
    models: Optional[Sequence[ModelConfig]],
    api_key: str,
    attempts_per_model: int = 3,
    prompt: str = DEFAULT_PROMPT,
    endpoint: str = DEFAULT_ENDPOINT,
    timeout_seconds: float = 30.0,
    concurrency: int = 6,
    requester: Optional[Requester] = None,
    today: Optional[date] = None,
    drop_failed_models: bool = True,
    degraded_failure_factor: float = 0.2,
) -> List[Dict[str, object]]:
    current = today or date.today()
    actual_requester = requester or bailian_chat_completion_request
    active_models = list(models) if models is not None else parse_model_configs()
    semaphore = asyncio.Semaphore(max(1, concurrency))

    async def run_single_model(cfg: ModelConfig) -> Dict[str, object]:
        attempts: List[Dict[str, object]] = []
        for _ in range(attempts_per_model):
            async with semaphore:
                ok, latency_ms, status, error = await actual_requester(
                    cfg.model, prompt, api_key, endpoint, timeout_seconds
                )
            attempts.append(
                {
                    "success": ok,
                    "latency_ms": round(latency_ms, 2),
                    "status_code": status,
                    "error": error,
                }
            )

        success_count = sum(1 for item in attempts if item["success"])
        success_rate = success_count / float(attempts_per_model)
        success_latencies = [item["latency_ms"] for item in attempts if item["success"]]
        avg_latency_ms = (
            round(float(statistics.mean(success_latencies)), 2) if success_latencies else None
        )

        base_weight = build_router_weight(cfg, today=current)
        router_weight = base_weight
        if success_count == 0:
            router_weight = 0.0 if drop_failed_models else base_weight * degraded_failure_factor
        elif not drop_failed_models:
            router_weight = round(base_weight * success_rate, 4)

        return {
            "model": cfg.model,
            "expires_on": cfg.expires_on.isoformat(),
            "days_remaining": compute_days_remaining(cfg.expires_on, current),
            "attempts": attempts,
            "success_rate": round(success_rate, 4),
            "avg_latency_ms": avg_latency_ms,
            "router_weight": round(router_weight, 4),
            "eligible": router_weight > 0.0,
        }

    return list(await asyncio.gather(*(run_single_model(cfg) for cfg in active_models)))


class ModelRouter:
    """Expiry-aware weighted router. Supports smooth WRR or random selection."""

    def __init__(
        self,
        models: Sequence[ModelConfig],
        test_results: Optional[Sequence[Dict[str, object]]] = None,
        today: Optional[date] = None,
        strategy: str = "smooth_wrr",
    ) -> None:
        self._strategy = strategy
        self._items: List[Dict[str, float | str]] = []
        self._current: Dict[str, float] = {}

        result_by_model: Dict[str, Dict[str, object]] = {}
        for item in test_results or []:
            name = str(item.get("model", ""))
            if name:
                result_by_model[name] = item

        for cfg in models:
            if cfg.model in result_by_model:
                weight = float(result_by_model[cfg.model].get("router_weight", 0.0))
            else:
                weight = build_router_weight(cfg, today=today)
            if weight <= 0.0:
                continue
            self._items.append({"model": cfg.model, "weight": weight})
            self._current[cfg.model] = 0.0

    def get_weight_snapshot(self) -> List[Dict[str, float | str]]:
        return [dict(item) for item in self._items]

    def get_next_model(self) -> str:
        if not self._items:
            raise RuntimeError("no_eligible_models_in_router")

        if self._strategy == "random":
            names = [str(item["model"]) for item in self._items]
            weights = [float(item["weight"]) for item in self._items]
            return random.choices(names, weights=weights, k=1)[0]

        total_weight = sum(float(item["weight"]) for item in self._items)
        best_model = ""
        best_score = float("-inf")

        for item in self._items:
            model = str(item["model"])
            new_score = self._current[model] + float(item["weight"])
            self._current[model] = new_score
            if new_score > best_score:
                best_score = new_score
                best_model = model

        self._current[best_model] -= total_weight
        return best_model


def generate_markdown_report(results: Sequence[Dict[str, object]]) -> str:
    total = len(results)
    eligible = sum(1 for row in results if bool(row.get("eligible", False)))
    header = [
        "# 阿里云百炼模型 API 可用性与智能路由测试报告",
        "",
        f"- 生成时间：{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
        f"- 模型总数：{total}",
        f"- 可入池模型：{eligible}",
        "",
        "| 模型 | 到期日 | 剩余天数 | 成功率 | 平均响应(ms) | 路由权重 | 状态 |",
        "| --- | --- | ---: | ---: | ---: | ---: | --- |",
    ]

    lines = list(header)
    for row in results:
        avg_latency = row.get("avg_latency_ms")
        latency_text = "-" if avg_latency is None else f"{float(avg_latency):.2f}"
        success_rate = float(row.get("success_rate", 0.0)) * 100.0
        status_text = "可用" if bool(row.get("eligible", False)) else "剔除/降级"
        lines.append(
            "| {model} | {expires_on} | {days_remaining} | {success_rate:.2f}% | {latency} | {weight:.4f} | {status} |".format(
                model=row.get("model", "-"),
                expires_on=row.get("expires_on", "-"),
                days_remaining=row.get("days_remaining", "-"),
                success_rate=success_rate,
                latency=latency_text,
                weight=float(row.get("router_weight", 0.0)),
                status=status_text,
            )
        )
    lines.append("")
    return "\n".join(lines)


def save_markdown_report(path: Path, results: Sequence[Dict[str, object]]) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(generate_markdown_report(results), encoding="utf-8")
    return path


def build_router_config(
    results: Sequence[Dict[str, object]],
    min_success_rate: float = 0.5,
    strategy: str = "smooth_wrr",
) -> Dict[str, object]:
    models: List[Dict[str, object]] = []
    for row in results:
        models.append(
            {
                "model": row.get("model"),
                "weight": row.get("router_weight", 0.0),
                "success_rate": row.get("success_rate", 0.0),
                "eligible": row.get("eligible", False),
            }
        )

    return {
        "enabled": True,
        "strategy": strategy,
        "min_success_rate": max(0.0, min(1.0, float(min_success_rate))),
        "generated_at": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "models": models,
    }


def save_router_config(
    path: Path,
    results: Sequence[Dict[str, object]],
    min_success_rate: float = 0.5,
    strategy: str = "smooth_wrr",
) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = build_router_config(results, min_success_rate=min_success_rate, strategy=strategy)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    return path


async def _run_cli_async(args: argparse.Namespace) -> None:
    models = parse_model_configs()
    results = await run_api_tests(
        models=models,
        api_key=args.api_key,
        attempts_per_model=args.attempts,
        prompt=args.prompt,
        endpoint=args.endpoint,
        timeout_seconds=args.timeout,
        drop_failed_models=not args.degrade_failed,
    )
    report_path = save_markdown_report(Path(args.output), results)
    router_config_path = save_router_config(
        Path(args.router_config_output),
        results,
        min_success_rate=args.router_min_success_rate,
        strategy=args.router_strategy,
    )
    print(f"report_written={report_path}")
    print(f"router_config_written={router_config_path}")


def _build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Aliyun Bailian API test and smart routing helper")
    parser.add_argument("--api-key", required=True, help="Bailian API key")
    parser.add_argument("--attempts", type=int, default=3, help="Attempts per model")
    parser.add_argument("--timeout", type=float, default=30.0, help="HTTP timeout in seconds")
    parser.add_argument("--endpoint", default=DEFAULT_ENDPOINT, help="Bailian endpoint URL")
    parser.add_argument("--prompt", default=DEFAULT_PROMPT, help="Test prompt text")
    parser.add_argument("--output", default="Saved/HCIAbilityKit/bailian_api_test_report.md")
    parser.add_argument(
        "--router-config-output",
        default="Saved/HCIAbilityKit/Config/llm_router.local.json",
        help="Router config output path for C++ runtime",
    )
    parser.add_argument(
        "--router-min-success-rate",
        type=float,
        default=0.5,
        help="Models below this success rate are removed in router runtime",
    )
    parser.add_argument(
        "--router-strategy",
        default="smooth_wrr",
        choices=("smooth_wrr", "weighted_random"),
        help="Router strategy written to router config",
    )
    parser.add_argument(
        "--degrade-failed",
        action="store_true",
        help="Degrade failed models instead of removing from router pool",
    )
    return parser


def main() -> None:
    parser = _build_arg_parser()
    args = parser.parse_args()
    asyncio.run(_run_cli_async(args))


if __name__ == "__main__":
    main()
