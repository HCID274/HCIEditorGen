#!/usr/bin/env python3
"""Inspect /Game/Seed assets for naming compliance and triangle metadata.

Run inside Unreal Python environment:
  UnrealEditor-Cmd.exe <Project.uproject> -run=pythonscript -script=<this_file> -- --output <json>
"""

from __future__ import annotations

import argparse
import json
import re
import traceback
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Any, Dict, List, Optional

try:
    import unreal
except Exception as ex:  # pragma: no cover - this script is intended for UE Python runtime
    raise RuntimeError("This script must run inside Unreal Python environment.") from ex


DEFAULT_ROOT = "/Game/Seed"
STATIC_MESH_NAME_RE = re.compile(r"^SM_[A-Za-z0-9_]+$")


@dataclass
class MeshRecord:
    object_path: str
    asset_name: str
    package_path: str
    class_name: str
    name_ok: bool
    triangle_lod0: Optional[int]
    triangle_source: str
    notes: List[str]


def _asset_class_name(asset_data: Any) -> str:
    try:
        return str(asset_data.asset_class_path.asset_name)
    except Exception:
        try:
            return str(asset_data.asset_class)
        except Exception:
            return "Unknown"


def _asset_object_path(asset_data: Any) -> str:
    candidates = (
        "object_path",
        "object_path_string",
        "asset_object_path",
    )
    for key in candidates:
        if hasattr(asset_data, key):
            try:
                value = getattr(asset_data, key)
                if value:
                    return str(value)
            except Exception:
                continue
    try:
        soft_path = asset_data.get_soft_object_path()
        if soft_path:
            if hasattr(soft_path, "to_string"):
                return str(soft_path.to_string())
            return str(soft_path)
    except Exception:
        pass
    try:
        pkg_name = str(asset_data.package_name)
        asset_name = str(asset_data.asset_name)
        if pkg_name and asset_name:
            return f"{pkg_name}.{asset_name}"
    except Exception:
        pass
    return ""


def _asset_package_path(asset_data: Any) -> str:
    if hasattr(asset_data, "package_path"):
        try:
            return str(asset_data.package_path)
        except Exception:
            pass
    try:
        package_name = str(asset_data.package_name)
        if "/" in package_name:
            return package_name.rsplit("/", 1)[0]
    except Exception:
        pass
    return ""


def _tag_value(asset_data: Any, key: str) -> Optional[str]:
    try:
        value = asset_data.get_tag_value(key)
        if value is None:
            return None
        text = str(value).strip()
        return text if text else None
    except Exception:
        return None


def _parse_triangles_from_tags(asset_data: Any) -> tuple[Optional[int], str]:
    candidate_keys = (
        "Triangles",
        "LOD0Triangles",
        "LOD0TriCount",
        "NumTriangles",
        "NaniteTriangles",
    )
    for key in candidate_keys:
        raw = _tag_value(asset_data, key)
        if not raw:
            continue
        digits = "".join(ch for ch in raw if ch.isdigit())
        if not digits:
            continue
        try:
            return int(digits), f"tag:{key}"
        except Exception:
            continue
    return None, "missing"


def _try_editor_static_mesh_library_triangles(asset_object: Any) -> tuple[Optional[int], str]:
    try:
        helper = unreal.EditorStaticMeshLibrary
    except Exception:
        return None, "missing"

    candidate_funcs = (
        "get_lod_triangle_count",
        "get_number_triangles",
        "get_num_triangles",
    )
    for func_name in candidate_funcs:
        if not hasattr(helper, func_name):
            continue
        try:
            func = getattr(helper, func_name)
            value = func(asset_object, 0)
            return int(value), f"editor_static_mesh_library:{func_name}"
        except Exception:
            continue
    return None, "missing"


def _inspect_seed_assets(root_path: str) -> Dict[str, Any]:
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = asset_registry.get_assets_by_path(root_path, recursive=True)

    static_mesh_records: List[MeshRecord] = []
    non_static_assets: List[Dict[str, str]] = []

    for asset_data in assets:
        class_name = _asset_class_name(asset_data)
        asset_name = str(asset_data.asset_name)
        object_path = _asset_object_path(asset_data)
        package_path = _asset_package_path(asset_data)

        if class_name != "StaticMesh":
            non_static_assets.append(
                {
                    "object_path": object_path,
                    "asset_name": asset_name,
                    "class_name": class_name,
                }
            )
            continue

        name_ok = bool(STATIC_MESH_NAME_RE.match(asset_name))
        triangle_lod0, triangle_source = _parse_triangles_from_tags(asset_data)
        notes: List[str] = []
        if triangle_lod0 is None:
            try:
                asset_object = unreal.EditorAssetLibrary.load_asset(object_path)
            except Exception:
                asset_object = None

            if asset_object is not None:
                fallback_triangles, fallback_source = _try_editor_static_mesh_library_triangles(asset_object)
                if fallback_triangles is not None:
                    triangle_lod0 = fallback_triangles
                    triangle_source = fallback_source
            if triangle_lod0 is None:
                notes.append("triangle metadata unavailable")

        if not name_ok:
            notes.append("name should match ^SM_[A-Za-z0-9_]+$")

        static_mesh_records.append(
            MeshRecord(
                object_path=object_path,
                asset_name=asset_name,
                package_path=package_path,
                class_name=class_name,
                name_ok=name_ok,
                triangle_lod0=triangle_lod0,
                triangle_source=triangle_source,
                notes=notes,
            )
        )

    triangle_known = [r for r in static_mesh_records if r.triangle_lod0 is not None]
    high_poly = [r for r in triangle_known if int(r.triangle_lod0) >= 50000]
    bad_name = [r for r in static_mesh_records if not r.name_ok]

    report = {
        "root_path": root_path,
        "total_assets_under_seed": len(assets),
        "static_mesh_count": len(static_mesh_records),
        "non_static_asset_count": len(non_static_assets),
        "naming_noncompliant_count": len(bad_name),
        "triangle_known_count": len(triangle_known),
        "high_poly_ge_50000_count": len(high_poly),
        "static_meshes": [asdict(r) for r in static_mesh_records],
        "non_static_assets": non_static_assets,
    }
    return report


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Inspect /Game/Seed naming and triangle metadata.")
    parser.add_argument("--root-path", default=DEFAULT_ROOT, help="UE content root path to inspect.")
    parser.add_argument(
        "--output",
        default="SourceData/AbilityKits/seed_mesh_inspection_report.json",
        help="Output JSON path relative to project root.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output_path = Path(args.output)
    if not output_path.is_absolute():
        output_path = Path(unreal.Paths.project_dir()) / output_path
    output_path.parent.mkdir(parents=True, exist_ok=True)

    try:
        report = _inspect_seed_assets(args.root_path)
        output_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        unreal.log(f"[HCIAbilityKit][SeedInspect] report written: {output_path}")
        unreal.log(
            "[HCIAbilityKit][SeedInspect] "
            f"seed_assets={report['total_assets_under_seed']} static_mesh={report['static_mesh_count']} "
            f"non_static={report['non_static_asset_count']} noncompliant_name={report['naming_noncompliant_count']} "
            f"high_poly={report['high_poly_ge_50000_count']}"
        )
        return 0
    except Exception as ex:
        error_path = output_path.with_suffix(".error.txt")
        error_path.write_text(f"{ex}\n{traceback.format_exc()}", encoding="utf-8")
        unreal.log_error(f"[HCIAbilityKit][SeedInspect] failed: {ex}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
