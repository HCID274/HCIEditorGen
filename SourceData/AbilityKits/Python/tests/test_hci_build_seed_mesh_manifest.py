import pathlib
import sys
import tempfile
import unittest

SCRIPT_DIR = pathlib.Path(__file__).resolve().parents[1]
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from hci_build_seed_mesh_manifest import (
    SeedEntry,
    build_manifest_data,
    collect_seed_entries,
    collect_seed_entries_with_filter,
    validate_manifest_data,
)


class SeedMeshManifestTests(unittest.TestCase):
    def test_collect_seed_entries_from_content_seed(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = pathlib.Path(temp_dir)
            content_root = project_root / "Content"
            (content_root / "Seed" / "HighPoly").mkdir(parents=True, exist_ok=True)
            (content_root / "Seed" / "HighPoly" / "SM_Cliff_A.uasset").write_bytes(b"dummy")
            (content_root / "Seed" / "HighPoly" / "SM_Cliff_B.uasset").write_bytes(b"dummy")

            entries = collect_seed_entries(content_root=content_root, seed_subdir=pathlib.Path("Seed"))
            self.assertEqual(len(entries), 2)
            self.assertEqual(entries[0].object_path, "/Game/Seed/HighPoly/SM_Cliff_A.SM_Cliff_A")
            self.assertEqual(entries[1].object_path, "/Game/Seed/HighPoly/SM_Cliff_B.SM_Cliff_B")

    def test_validate_manifest_success(self):
        manifest = build_manifest_data(
            entries=[
                SeedEntry(
                    object_path="/Game/Seed/HighPoly/SM_A.SM_A",
                    source_file="Content/Seed/HighPoly/SM_A.uasset",
                ),
            ],
            seed_root="/Game/Seed",
            content_dir="Content",
        )
        errors = validate_manifest_data(manifest, expected_seed_root="/Game/Seed")
        self.assertEqual(errors, [])

    def test_collect_seed_entries_with_prefix_filter(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = pathlib.Path(temp_dir)
            content_root = project_root / "Content"
            seed_dir = content_root / "Seed" / "HighPoly"
            seed_dir.mkdir(parents=True, exist_ok=True)
            (seed_dir / "SM_Cliff_A.uasset").write_bytes(b"dummy")
            (seed_dir / "M_Cliff_A.uasset").write_bytes(b"dummy")

            entries = collect_seed_entries_with_filter(
                content_root=content_root,
                seed_subdir=pathlib.Path("Seed"),
                name_prefixes=("SM_",),
            )
            self.assertEqual(len(entries), 1)
            self.assertEqual(entries[0].object_path, "/Game/Seed/HighPoly/SM_Cliff_A.SM_Cliff_A")

    def test_validate_manifest_rejects_os_path(self):
        manifest = {
            "schema_version": 1,
            "seed_root": "/Game/Seed",
            "count": 1,
            "entries": [
                {"object_path": "C:/Project/Content/Seed/SM_A.uasset", "source_file": "Content/Seed/SM_A.uasset"}
            ],
        }
        errors = validate_manifest_data(manifest, expected_seed_root="/Game/Seed")
        self.assertTrue(errors)
        self.assertIn("UE long object path", errors[0])


if __name__ == "__main__":
    unittest.main()
