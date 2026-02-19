#!/usr/bin/env python3
"""Batch rename non-compliant StaticMesh assets under /Game/Seed.

This script is intended to run in Unreal Python:
  UnrealEditor-Cmd.exe <Project.uproject> -run=pythonscript -script=<this_file>
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any, Dict, List

try:
    import unreal
except Exception as ex:  # pragma: no cover
    raise RuntimeError("This script must run in Unreal Python.") from ex


DEFAULT_ROOT = "/Game/Seed"
NAME_RE = re.compile(r"^SM_[A-Za-z0-9_]+$")


def _asset_class_name(asset_data: Any) -> str:
    try:
        return str(asset_data.asset_class_path.asset_name)
    except Exception:
        try:
            return str(asset_data.asset_class)
        except Exception:
            return "Unknown"


def _asset_package_name(asset_data: Any) -> str:
    try:
        return str(asset_data.package_name)
    except Exception:
        try:
            soft_path = asset_data.get_soft_object_path()
            soft_text = str(soft_path.to_string() if hasattr(soft_path, "to_string") else soft_path)
            if "." in soft_text:
                return soft_text.split(".", 1)[0]
        except Exception:
            pass
    return ""


def _asset_name(asset_data: Any) -> str:
    try:
        return str(asset_data.asset_name)
    except Exception:
        return ""


def _sanitize_name(base_name: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9]+", "_", base_name).strip("_")
    cleaned = re.sub(r"_+", "_", cleaned)
    if not cleaned:
        cleaned = "Mesh"
    return f"SM_{cleaned}"


def _build_target_package(old_package_name: str, new_asset_name: str) -> str:
    if "/" not in old_package_name:
        return f"/Game/Seed/{new_asset_name}"
    folder = old_package_name.rsplit("/", 1)[0]
    return f"{folder}/{new_asset_name}"


def _ensure_unique_package(target_package_name: str, existing_packages: set[str]) -> str:
    if target_package_name not in existing_packages and not unreal.EditorAssetLibrary.does_asset_exist(target_package_name):
        return target_package_name
    suffix = 1
    while True:
        candidate = f"{target_package_name}_{suffix:02d}"
        if candidate not in existing_packages and not unreal.EditorAssetLibrary.does_asset_exist(candidate):
            return candidate
        suffix += 1


def rename_seed_static_meshes(root_path: str, dry_run: bool) -> Dict[str, Any]:
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = registry.get_assets_by_path(root_path, recursive=True)

    existing_packages = set()
    for asset in assets:
        pkg = _asset_package_name(asset)
        if pkg:
            existing_packages.add(pkg)

    renamed: List[Dict[str, str]] = []
    skipped: List[Dict[str, str]] = []
    failed: List[Dict[str, str]] = []

    for asset in assets:
        if _asset_class_name(asset) != "StaticMesh":
            continue

        old_name = _asset_name(asset)
        old_package = _asset_package_name(asset)
        if not old_name or not old_package:
            failed.append({"asset_name": old_name, "old_package": old_package, "reason": "invalid metadata"})
            continue

        if NAME_RE.match(old_name):
            skipped.append({"asset_name": old_name, "old_package": old_package, "reason": "already compliant"})
            continue

        target_asset_name = _sanitize_name(old_name)
        target_package = _build_target_package(old_package, target_asset_name)
        target_package = _ensure_unique_package(target_package, existing_packages)

        if dry_run:
            renamed.append({"old_package": old_package, "new_package": target_package, "status": "plan"})
            existing_packages.add(target_package)
            continue

        ok = unreal.EditorAssetLibrary.rename_asset(old_package, target_package)
        if ok:
            renamed.append({"old_package": old_package, "new_package": target_package, "status": "renamed"})
            existing_packages.discard(old_package)
            existing_packages.add(target_package)
        else:
            failed.append({"asset_name": old_name, "old_package": old_package, "new_package": target_package, "reason": "rename_asset returned false"})

    return {
        "root_path": root_path,
        "dry_run": dry_run,
        "renamed_count": len(renamed),
        "skipped_count": len(skipped),
        "failed_count": len(failed),
        "renamed": renamed,
        "skipped": skipped,
        "failed": failed,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Rename non-compliant seed static meshes to SM_* style.")
    parser.add_argument("--root-path", default=DEFAULT_ROOT)
    parser.add_argument(
        "--output",
        default="SourceData/AbilityKits/seed_mesh_rename_report.json",
        help="Output report path relative to project dir.",
    )
    parser.add_argument("--dry-run", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output = Path(args.output)
    if not output.is_absolute():
        output = Path(unreal.Paths.project_dir()) / output
    output.parent.mkdir(parents=True, exist_ok=True)

    report = rename_seed_static_meshes(root_path=args.root_path, dry_run=args.dry_run)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log(f"[HCIAbilityKit][SeedRename] report={output}")
    unreal.log(
        "[HCIAbilityKit][SeedRename] "
        f"renamed={report['renamed_count']} skipped={report['skipped_count']} failed={report['failed_count']} dry_run={report['dry_run']}"
    )
    return 0 if report["failed_count"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
