#!/usr/bin/env python3
"""Build and validate seed mesh manifest for Stage B / Slice B0.

The manifest stores UE long object paths only. Python does not resolve asset validity;
it only emits and validates string format contract.
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Sequence

UE_LONG_OBJECT_PATH_RE = re.compile(r"^/Game(?:/[A-Za-z0-9_]+)+\.[A-Za-z0-9_]+$")


@dataclass(frozen=True)
class SeedEntry:
    object_path: str
    source_file: str


def _to_unix(path: Path) -> str:
    return path.as_posix()


def _asset_object_path_from_uasset(content_root: Path, uasset_file: Path) -> str:
    rel_no_ext = uasset_file.relative_to(content_root).with_suffix("")
    rel_parts = rel_no_ext.parts
    if not rel_parts:
        raise ValueError(f"Invalid seed mesh file path: {uasset_file}")
    package_path = "/".join(rel_parts)
    asset_name = rel_parts[-1]
    return f"/Game/{package_path}.{asset_name}"


def _is_valid_ue_long_object_path(path: str) -> bool:
    return bool(UE_LONG_OBJECT_PATH_RE.match(path))


def collect_seed_entries(content_root: Path, seed_subdir: Path) -> List[SeedEntry]:
    return collect_seed_entries_with_filter(
        content_root=content_root,
        seed_subdir=seed_subdir,
        name_prefixes=(),
    )


def collect_seed_entries_with_filter(
    content_root: Path,
    seed_subdir: Path,
    name_prefixes: Sequence[str],
) -> List[SeedEntry]:
    seed_dir = content_root / seed_subdir
    if not seed_dir.exists():
        raise FileNotFoundError(f"Seed directory not found: {seed_dir}")

    normalized_prefixes = [prefix for prefix in name_prefixes if prefix]
    entries: List[SeedEntry] = []
    seen = set()
    for uasset_file in sorted(seed_dir.rglob("*.uasset")):
        if normalized_prefixes:
            stem = uasset_file.stem
            if not any(stem.startswith(prefix) for prefix in normalized_prefixes):
                continue
        object_path = _asset_object_path_from_uasset(content_root, uasset_file)
        if object_path in seen:
            continue
        seen.add(object_path)
        entries.append(
            SeedEntry(
                object_path=object_path,
                source_file=_to_unix(uasset_file.relative_to(content_root.parent)),
            )
        )
    return entries


def build_manifest_data(
    entries: Sequence[SeedEntry],
    seed_root: str,
    content_dir: str,
    name_prefixes: Sequence[str] = (),
) -> Dict[str, object]:
    if not seed_root.startswith("/Game"):
        raise ValueError("seed_root must start with /Game")
    normalized_seed_root = seed_root.rstrip("/")

    manifest_entries: List[Dict[str, str]] = []
    for item in entries:
        manifest_entries.append(
            {
                "object_path": item.object_path,
                "source_file": item.source_file,
            }
        )

    return {
        "schema_version": 1,
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "seed_root": normalized_seed_root,
        "content_dir": content_dir,
        "name_prefixes": list(name_prefixes),
        "count": len(manifest_entries),
        "entries": manifest_entries,
    }


def validate_manifest_data(data: Dict[str, object], expected_seed_root: str | None = None) -> List[str]:
    errors: List[str] = []

    if not isinstance(data, dict):
        return ["manifest root must be a JSON object"]

    entries = data.get("entries")
    if not isinstance(entries, list):
        errors.append("entries must be a list")
        return errors

    seed_root = str(data.get("seed_root", "")).rstrip("/")
    if not seed_root.startswith("/Game"):
        errors.append("seed_root must start with /Game")

    if expected_seed_root is not None:
        expected = expected_seed_root.rstrip("/")
        if seed_root != expected:
            errors.append(f"seed_root mismatch: expected {expected}, got {seed_root}")

    seen_paths = set()
    for idx, entry in enumerate(entries):
        if not isinstance(entry, dict):
            errors.append(f"entries[{idx}] must be an object")
            continue

        object_path = str(entry.get("object_path", ""))
        if not _is_valid_ue_long_object_path(object_path):
            errors.append(f"entries[{idx}].object_path is not UE long object path: {object_path}")
            continue

        if seed_root and not object_path.startswith(f"{seed_root}/"):
            errors.append(
                f"entries[{idx}].object_path must be under seed_root {seed_root}: {object_path}"
            )

        if object_path in seen_paths:
            errors.append(f"duplicate object_path detected: {object_path}")
        seen_paths.add(object_path)

    count_value = data.get("count")
    if isinstance(count_value, int) and count_value != len(entries):
        errors.append(f"count mismatch: count={count_value}, len(entries)={len(entries)}")

    return errors


def _write_manifest(manifest: Dict[str, object], output_file: Path) -> None:
    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")


def _read_manifest(manifest_file: Path) -> Dict[str, object]:
    return json.loads(manifest_file.read_text(encoding="utf-8"))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build/validate UE seed mesh manifest.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    build_parser = subparsers.add_parser("build", help="Scan Content seed folder and generate manifest.")
    build_parser.add_argument(
        "--project-root",
        type=Path,
        default=Path.cwd(),
        help="UE project root containing Content/.",
    )
    build_parser.add_argument(
        "--content-dir",
        type=Path,
        default=Path("Content"),
        help="Content directory relative to project root.",
    )
    build_parser.add_argument(
        "--seed-subdir",
        type=Path,
        default=Path("Seed"),
        help="Seed folder under Content directory.",
    )
    build_parser.add_argument(
        "--seed-root",
        type=str,
        default="/Game/Seed",
        help="Expected UE root path for seed assets.",
    )
    build_parser.add_argument(
        "--output-file",
        type=Path,
        default=Path("SourceData/AbilityKits/seed_mesh_manifest.json"),
        help="Output manifest file.",
    )
    build_parser.add_argument(
        "--allow-empty",
        action="store_true",
        help="Allow writing empty entries. Default: fail when no seed assets found.",
    )
    build_parser.add_argument(
        "--name-prefix",
        nargs="*",
        default=(),
        help="Optional asset name prefixes to include, e.g. SM_.",
    )

    validate_parser = subparsers.add_parser("validate", help="Validate an existing manifest file.")
    validate_parser.add_argument(
        "--manifest-file",
        type=Path,
        required=True,
        help="Manifest JSON file path.",
    )
    validate_parser.add_argument(
        "--expected-seed-root",
        type=str,
        default=None,
        help="Optional expected seed root, e.g. /Game/Seed.",
    )

    return parser.parse_args()


def _cmd_build(args: argparse.Namespace) -> int:
    project_root = args.project_root.resolve()
    content_root = (project_root / args.content_dir).resolve()
    output_file = (project_root / args.output_file).resolve()

    entries = collect_seed_entries_with_filter(
        content_root=content_root,
        seed_subdir=args.seed_subdir,
        name_prefixes=args.name_prefix,
    )
    if not entries and not args.allow_empty:
        raise RuntimeError(
            f"No .uasset found under {(content_root / args.seed_subdir)}. "
            "Import seed meshes first or pass --allow-empty."
        )

    manifest = build_manifest_data(
        entries=entries,
        seed_root=args.seed_root,
        content_dir=_to_unix(content_root.relative_to(project_root)),
        name_prefixes=args.name_prefix,
    )
    errors = validate_manifest_data(manifest, expected_seed_root=args.seed_root)
    if errors:
        raise RuntimeError("Manifest validation failed:\n- " + "\n- ".join(errors))

    _write_manifest(manifest, output_file=output_file)
    print(f"[seed-manifest] wrote {manifest['count']} entries to {output_file}")
    return 0


def _cmd_validate(args: argparse.Namespace) -> int:
    manifest_file = args.manifest_file.resolve()
    manifest = _read_manifest(manifest_file)
    errors = validate_manifest_data(manifest, expected_seed_root=args.expected_seed_root)
    if errors:
        print("[seed-manifest] validation failed:")
        for issue in errors:
            print(f"- {issue}")
        return 1
    print(f"[seed-manifest] validation passed: {manifest_file}")
    return 0


def main() -> int:
    args = parse_args()
    if args.command == "build":
        return _cmd_build(args)
    if args.command == "validate":
        return _cmd_validate(args)
    raise RuntimeError(f"Unsupported command: {args.command}")


if __name__ == "__main__":
    raise SystemExit(main())
