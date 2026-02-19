import json
import pathlib
import sys
import tempfile
import unittest

SCRIPT_DIR = pathlib.Path(__file__).resolve().parents[1]
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from hci_generate_synthetic_assets import (
    CORE_KEYWORDS,
    build_dataset,
    load_seed_mesh_manifest,
    performance_trap_flags,
    semantic_gap_flags,
    trap_flags,
)


class SyntheticGeneratorTests(unittest.TestCase):
    def test_build_dataset_shape_and_compatibility(self):
        dataset = build_dataset(count=200, seed=7, semantic_gap_ratio=0.3, trap_ratio=0.1)
        self.assertEqual(len(dataset), 200)

        sample = dataset[0]
        self.assertIn("id", sample)
        self.assertIn("name", sample)
        self.assertIn("display_name", sample)
        self.assertIn("type", sample)
        self.assertIn("virtual_path", sample)
        self.assertIn("tags", sample)
        self.assertIn("description", sample)
        self.assertIn("params", sample)
        self.assertIn("damage", sample["params"])
        self.assertIn("triangle_count_lod0", sample["params"])

    def test_semantic_gap_ratio_and_definition(self):
        count = 1000
        ratio = 0.3
        dataset = build_dataset(count=count, seed=13, semantic_gap_ratio=ratio, trap_ratio=0.1)
        gaps = semantic_gap_flags(dataset)
        self.assertEqual(sum(gaps), int(round(count * ratio)))

        for item, is_gap in zip(dataset, gaps):
            if not is_gap:
                continue
            name_l = item["name"].lower()
            desc_l = item["description"].lower()
            self.assertFalse(any(k in name_l for k in CORE_KEYWORDS))
            self.assertTrue(any(k in desc_l for k in CORE_KEYWORDS))

    def test_trap_assets_exist_and_are_contradictory(self):
        dataset = build_dataset(count=500, seed=5, semantic_gap_ratio=0.3, trap_ratio=0.12)
        traps = trap_flags(dataset)
        perf_traps = performance_trap_flags(dataset)
        self.assertEqual(sum(traps), int(round(500 * 0.12)))
        self.assertGreater(sum(perf_traps), 0)

        for item, is_trap in zip(dataset, traps):
            if not is_trap:
                continue
            self.assertIn("trap", item["tags"])
            description = item["description"].lower()
            if "trap:performance" in item["tags"]:
                self.assertGreaterEqual(item["params"]["triangle_count_lod0"], 50000)
                self.assertIn("lowpoly", item["virtual_path"].lower())
            else:
                self.assertTrue(
                    "deals physical damage" in description
                    or "not elemental" in description
                )

    def test_write_files(self):
        dataset = build_dataset(count=32, seed=11, semantic_gap_ratio=0.3, trap_ratio=0.1)
        with tempfile.TemporaryDirectory() as temp_dir:
            path = pathlib.Path(temp_dir)
            for item in dataset:
                (path / f"{item['id']}.hciabilitykit").write_text(
                    json.dumps(item, ensure_ascii=False, indent=2), encoding="utf-8"
                )

            files = list(path.glob("*.hciabilitykit"))
            self.assertEqual(len(files), 32)

    def test_seed_mesh_manifest_load_and_assignment(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            manifest_file = pathlib.Path(temp_dir) / "seed_mesh_manifest.json"
            manifest_file.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "seed_root": "/Game/Seed",
                        "entries": [
                            {"object_path": "/Game/Seed/HighPoly/SM_Rock_01.SM_Rock_01"},
                            {"object_path": "/Game/Seed/HighPoly/SM_Rock_02.SM_Rock_02"},
                        ],
                    },
                    ensure_ascii=False,
                    indent=2,
                ),
                encoding="utf-8",
            )

            pool = load_seed_mesh_manifest(manifest_file)
            dataset = build_dataset(
                count=80,
                seed=19,
                semantic_gap_ratio=0.3,
                trap_ratio=0.1,
                representing_mesh_pool=pool,
            )

            for item in dataset:
                self.assertIn("representing_mesh", item)
                self.assertIn(item["representing_mesh"], pool)


if __name__ == "__main__":
    unittest.main()
