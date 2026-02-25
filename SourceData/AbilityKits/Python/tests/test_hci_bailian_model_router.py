import asyncio
import pathlib
import sys
import unittest
from datetime import date

SCRIPT_DIR = pathlib.Path(__file__).resolve().parents[1]
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from hci_bailian_model_router import (
    ModelConfig,
    ModelRouter,
    build_model_catalog,
    build_router_config,
    build_router_weight,
    generate_markdown_report,
    run_api_tests,
)


class BailianModelRouterTests(unittest.TestCase):
    def test_build_model_catalog_days_remaining(self):
        catalog = build_model_catalog(today=date(2026, 2, 25))
        self.assertEqual(len(catalog), 17)

        first = catalog[0]
        self.assertEqual(first["model"], "deepseek-r1-distill-llama-70b")
        self.assertEqual(first["days_remaining"], 0)

        second = catalog[1]
        self.assertEqual(second["days_remaining"], 4)

    def test_build_router_weight_expired_model_is_zero(self):
        cfg = ModelConfig(model="expired-model", expires_on=date(2026, 2, 24))
        weight = build_router_weight(cfg, today=date(2026, 2, 25))
        self.assertEqual(weight, 0.0)

    def test_router_prefers_near_expiry_model(self):
        models = [
            ModelConfig(model="near-expiry", expires_on=date(2026, 2, 26)),
            ModelConfig(model="far-expiry", expires_on=date(2026, 5, 31)),
        ]
        router = ModelRouter(models=models, today=date(2026, 2, 25))

        selections = [router.get_next_model() for _ in range(20)]
        near_count = selections.count("near-expiry")
        far_count = selections.count("far-expiry")

        self.assertGreater(near_count, far_count)
        self.assertGreaterEqual(near_count, 15)

    def test_run_api_tests_aggregates_results(self):
        async def fake_requester(model, prompt, api_key, endpoint, timeout_seconds):
            if model == "model-a":
                return True, 120.0, 200, ""
            return False, 0.0, 500, "boom"

        models = [
            ModelConfig(model="model-a", expires_on=date(2026, 3, 1)),
            ModelConfig(model="model-b", expires_on=date(2026, 3, 1)),
        ]
        results = asyncio.run(
            run_api_tests(
                models=models,
                api_key="test-key",
                attempts_per_model=3,
                requester=fake_requester,
            )
        )
        rows = {row["model"]: row for row in results}
        self.assertEqual(rows["model-a"]["success_rate"], 1.0)
        self.assertAlmostEqual(rows["model-a"]["avg_latency_ms"], 120.0)
        self.assertEqual(rows["model-b"]["success_rate"], 0.0)
        self.assertIsNone(rows["model-b"]["avg_latency_ms"])

    def test_markdown_report_contains_table(self):
        rows = [
            {
                "model": "model-a",
                "expires_on": "2026-03-01",
                "days_remaining": 4,
                "success_rate": 1.0,
                "avg_latency_ms": 100.0,
                "router_weight": 20.0,
                "eligible": True,
            }
        ]
        text = generate_markdown_report(rows)
        self.assertIn("| 模型 | 到期日 |", text)
        self.assertIn("model-a", text)

    def test_build_router_config_enforces_success_rate_threshold(self):
        rows = [
            {
                "model": "m-ok",
                "success_rate": 1.0,
                "router_weight": 10.0,
                "eligible": True,
            },
            {
                "model": "m-bad",
                "success_rate": 0.33,
                "router_weight": 50.0,
                "eligible": True,
            },
        ]
        cfg = build_router_config(rows, min_success_rate=0.5, strategy="smooth_wrr")
        self.assertEqual(cfg["min_success_rate"], 0.5)
        self.assertEqual(cfg["strategy"], "smooth_wrr")
        self.assertEqual(len(cfg["models"]), 2)
        self.assertEqual(cfg["models"][1]["success_rate"], 0.33)


if __name__ == "__main__":
    unittest.main()
