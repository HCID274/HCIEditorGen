#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
hci_ingest_stage.py

Stage external DCC/AI-exported assets into:
  <ProjectRoot>/Saved/HCIAbilityKit/Ingest/<batch_id>/
and emit:
  manifest.hci.json

Design goals (Stage N-SliceN1):
- DCC-agnostic: works for any source that can output files.
- Preflight-first: reject obviously bad/unsupported files before UE import.
- Deterministic + traceable: batch_id, file hashes, sizes, structured metadata.

No third-party dependencies.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import os
import re
import shutil
import sys
from pathlib import Path


ALLOWED_EXTS_DEFAULT = [".fbx", ".png", ".tga", ".jpg", ".jpeg"]
DEFAULT_MAX_FILE_MB = 512


def _utc_now_iso() -> str:
    return _dt.datetime.utcnow().replace(microsecond=0).isoformat() + "Z"


def _sanitize_token(s: str, max_len: int = 48) -> str:
    s = (s or "").strip()
    s = re.sub(r"[^A-Za-z0-9_]+", "_", s)
    s = re.sub(r"_+", "_", s).strip("_")
    if not s:
        return "batch"
    return s[:max_len]


def _guess_kind(ext: str) -> str:
    ext = ext.lower()
    if ext == ".fbx":
        return "mesh"
    if ext in (".png", ".tga", ".jpg", ".jpeg"):
        return "texture"
    return "unknown"


def _sha1_file(path: Path) -> str:
    h = hashlib.sha1()
    with path.open("rb") as f:
        while True:
            chunk = f.read(1024 * 1024)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def _read_head(path: Path, n: int = 64) -> bytes:
    with path.open("rb") as f:
        return f.read(n)


def _preflight_signature(path: Path) -> tuple[bool, str]:
    """
    Returns (ok, reason). This is intentionally shallow (N1).
    """
    ext = path.suffix.lower()
    try:
        head = _read_head(path, 64)
    except Exception as e:
        return False, f"read_failed:{e.__class__.__name__}"

    if ext == ".png":
        sig = b"\x89PNG\r\n\x1a\n"
        return (head.startswith(sig), "png_signature_invalid" if not head.startswith(sig) else "ok")

    if ext in (".jpg", ".jpeg"):
        return (head.startswith(b"\xFF\xD8"), "jpg_signature_invalid" if not head.startswith(b"\xFF\xD8") else "ok")

    if ext == ".fbx":
        # FBX can be ASCII or binary; both usually contain "Kaydara FBX".
        # This is not a full validation, only a cheap reject for obviously wrong files.
        try:
            # Read a bit more for ASCII detection.
            text = _read_head(path, 512).decode("latin1", errors="ignore")
        except Exception:
            text = ""
        ok = ("Kaydara FBX" in text) or head.startswith(b"Kaydara FBX")
        return (ok, "fbx_header_missing_kaydara" if not ok else "ok")

    if ext == ".tga":
        # TGA is flexible; do minimal length check.
        try:
            size = path.stat().st_size
        except Exception as e:
            return False, f"stat_failed:{e.__class__.__name__}"
        return (size >= 18, "tga_too_small" if size < 18 else "ok")

    return False, "unsupported_extension"


def _suggest_asset_name(file_name: str, kind: str) -> str:
    stem = Path(file_name).stem
    stem = re.sub(r"[^A-Za-z0-9_]+", "_", stem)
    stem = re.sub(r"_+", "_", stem).strip("_")
    if not stem:
        stem = "Asset"
    prefix = "A_"
    if kind == "mesh":
        prefix = "SM_"
    elif kind == "texture":
        prefix = "T_"
    # UE object name regex is enforced later by UE-side rename tool; here is just a hint.
    return (prefix + stem)[:64]


def _iter_input_files(input_path: Path) -> list[Path]:
    if input_path.is_file():
        return [input_path]
    out: list[Path] = []
    for p in input_path.rglob("*"):
        if p.is_file():
            out.append(p)
    return out


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True, help="Input file or directory from DCC/AI export.")
    ap.add_argument("--source-app", default="unknown", help="Source app label (blender/maya/ai/unknown).")
    ap.add_argument("--batch-name", default="", help="Optional human-friendly batch suffix.")
    ap.add_argument("--suggest-target-root", default="/Game/Temp/Ingest", help="UE /Game target root prefix.")
    ap.add_argument("--allowed-ext", action="append", default=[], help="Override allowed extensions (repeatable).")
    ap.add_argument("--max-file-mb", type=int, default=DEFAULT_MAX_FILE_MB, help="Reject files larger than this.")
    ap.add_argument("--project-root", default="", help="Optional UE project root; defaults to cwd.")
    args = ap.parse_args(argv)

    project_root = Path(args.project_root).resolve() if args.project_root else Path.cwd().resolve()
    saved_root = project_root / "Saved" / "HCIAbilityKit" / "Ingest"
    saved_root.mkdir(parents=True, exist_ok=True)

    input_path = Path(args.input).resolve()
    if not input_path.exists():
        print(f"[hci_ingest_stage] error: input_not_found path={input_path}")
        return 2

    allowed_exts = [e.lower() for e in (args.allowed_ext or ALLOWED_EXTS_DEFAULT)]
    allowed_exts = [e if e.startswith(".") else "." + e for e in allowed_exts]

    ts = _dt.datetime.utcnow().strftime("%Y%m%d_%H%M%S")
    suffix = _sanitize_token(args.batch_name) if args.batch_name else _sanitize_token(input_path.stem)
    batch_id = f"{ts}_{suffix}"
    batch_dir = saved_root / batch_id
    batch_dir.mkdir(parents=True, exist_ok=True)

    staged_at_utc = _utc_now_iso()
    target_root = (args.suggest_target_root or "").strip()
    if not target_root.startswith("/Game/"):
        # Keep manifest strict; UE side will also re-validate.
        target_root = "/Game/Temp/Ingest"
    suggested_unreal_target_root = f"{target_root.rstrip('/')}/{batch_id}"

    files = _iter_input_files(input_path)
    warnings: list[str] = []
    errors: list[str] = []
    manifest_files: list[dict] = []

    max_bytes = int(args.max_file_mb) * 1024 * 1024

    for src in files:
        ext = src.suffix.lower()
        if ext not in allowed_exts:
            warnings.append(f"skip_unsupported_ext:{src.name}")
            continue

        try:
            size = src.stat().st_size
        except Exception as e:
            errors.append(f"stat_failed:{src.name}:{e.__class__.__name__}")
            continue

        if size > max_bytes:
            errors.append(f"file_too_large:{src.name}:{size}")
            continue

        ok_sig, sig_reason = _preflight_signature(src)
        if not ok_sig:
            errors.append(f"signature_invalid:{src.name}:{sig_reason}")
            continue

        # Preserve relative structure under input directory when possible.
        rel = None
        if input_path.is_dir():
            try:
                rel = src.relative_to(input_path)
            except Exception:
                rel = src.name
        else:
            rel = src.name

        rel_path = Path(str(rel)).as_posix()
        rel_path = rel_path.lstrip("/").replace("..", "_")

        dst = batch_dir / rel_path
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)

        kind = _guess_kind(ext)
        try:
            sha1 = _sha1_file(dst)
        except Exception as e:
            errors.append(f"hash_failed:{src.name}:{e.__class__.__name__}")
            continue

        manifest_files.append(
            {
                "relative_path": rel_path,
                "kind": kind,
                "size_bytes": int(size),
                "sha1": sha1,
                "suggested_asset_name": _suggest_asset_name(src.name, kind),
            }
        )

    preflight_ok = len(errors) == 0 and len(manifest_files) > 0
    if not manifest_files:
        errors.append("no_files_staged")
        preflight_ok = False

    manifest = {
        "schema_version": 1,
        "batch_id": batch_id,
        "source_app": (args.source_app or "unknown").strip()[:32],
        "staged_at_utc": staged_at_utc,
        "suggested_unreal_target_root": suggested_unreal_target_root,
        "files": manifest_files,
        "preflight": {
            "ok": preflight_ok,
            "warnings": warnings,
            "errors": errors,
        },
    }

    manifest_path = batch_dir / "manifest.hci.json"
    with manifest_path.open("w", encoding="utf-8") as f:
        json.dump(manifest, f, ensure_ascii=True, indent=2)

    print(f"[hci_ingest_stage] ok={str(preflight_ok).lower()} batch_id={batch_id}")
    print(f"[hci_ingest_stage] out_dir={batch_dir}")
    print(f"[hci_ingest_stage] manifest={manifest_path}")
    if warnings:
        print(f"[hci_ingest_stage] warnings={len(warnings)}")
    if errors:
        print(f"[hci_ingest_stage] errors={len(errors)}")

    return 0 if preflight_ok else 3


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

