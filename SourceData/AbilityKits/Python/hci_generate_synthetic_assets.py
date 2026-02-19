#!/usr/bin/env python3
"""Generate synthetic AbilityKit metadata for large-scale audit testing.

Backward compatibility with importer is preserved via:
- schema_version
- id
- display_name
- params.damage

Extended fields for retrieval/audit pressure tests:
- name, type, tags, description, virtual_path, params.*
"""

from __future__ import annotations

import argparse
import json
import random
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

ASSET_TYPES: Tuple[str, ...] = ("StaticMesh", "Sound", "VFX")

THEMES: Tuple[str, ...] = ("fire", "ice", "forest")

THEME_KEYWORDS: Dict[str, Tuple[str, ...]] = {
    "fire": ("fire", "flame", "burn", "ember", "inferno"),
    "ice": ("ice", "frost", "chill", "blizzard", "glacier"),
    "forest": ("forest", "nature", "jungle", "grove", "vine"),
}

CORE_KEYWORDS: Tuple[str, ...] = tuple(
    keyword for theme in THEMES for keyword in THEME_KEYWORDS[theme]
)

UE_LONG_OBJECT_PATH_RE = re.compile(r"^/Game(?:/[A-Za-z0-9_]+)+\.[A-Za-z0-9_]+$")

THEME_ALIASES: Dict[str, Tuple[str, ...]] = {
    "fire": ("Red Lotus", "Crimson Bloom", "Solar Petal", "Ash Crown", "Scarlet Arc"),
    "ice": ("Silent Mirror", "Pale Prism", "Moon Glass", "White Shard", "Crystal Halo"),
    "forest": ("Verdant Echo", "Green Ward", "Canopy Whisper", "Leaf Oath", "Woodland Pulse"),
}

NAME_PATTERNS_BY_THEME: Dict[str, Tuple[str, ...]] = {
    "fire": ("{kw}_{noun}", "{kw} {noun}", "{noun} of {kw}"),
    "ice": ("{kw}_{noun}", "{kw} {noun}", "{noun} of {kw}"),
    "forest": ("{kw}_{noun}", "{kw} {noun}", "{noun} of {kw}"),
}

NOUNS_BY_TYPE: Dict[str, Tuple[str, ...]] = {
    "StaticMesh": ("Blade", "Totem", "Shrine", "Orb", "Spear", "Crown"),
    "Sound": ("Pulse", "Chime", "Echo", "Hum", "Roar", "Resonance"),
    "VFX": ("Burst", "Nova", "Trail", "Wave", "Field", "Spark"),
}

DESCRIPTION_PATTERNS: Tuple[str, ...] = (
    "Designed for {scene} encounters, this asset channels {kw} behavior in controlled bursts.",
    "Used in combat prototypes, it emits {kw} cues and supports layered gameplay feedback.",
    "The implementation emphasizes {kw} readability and stable tuning for production maps.",
    "It is authored for client-side toolchain tests and references {kw} semantics explicitly.",
)

SCENES_BY_THEME: Dict[str, Tuple[str, ...]] = {
    "fire": ("boss phase two", "arena", "volcanic gate"),
    "ice": ("frozen keep", "control corridor", "defense lane"),
    "forest": ("forest map", "jungle route", "woodland mission"),
}

THEME_TAGS: Dict[str, Tuple[str, ...]] = {
    "fire": ("element:fire", "status:burn", "theme:aggressive"),
    "ice": ("element:ice", "status:slow", "theme:control"),
    "forest": ("element:forest", "status:root", "theme:sustain"),
}

TYPE_TAGS: Dict[str, Tuple[str, ...]] = {
    "StaticMesh": ("class:mesh", "pipeline:render"),
    "Sound": ("class:audio", "pipeline:acoustics"),
    "VFX": ("class:vfx", "pipeline:fx"),
}

SEMANTIC_TRAP_TEMPLATES: Tuple[Tuple[str, str, str], ...] = (
    (
        "Ice_Sword",
        "A physical sword with a cold look, deals physical damage and is not elemental.",
        "style:cold",
    ),
    (
        "Fire_Bark_Shield",
        "A wooden defensive prop from forest kits, not elemental, tuned for physical collisions.",
        "style:hot",
    ),
    (
        "Forest_Frost_Bell",
        "An audio cue named for art direction only; it is not elemental and drives neutral ambience.",
        "style:organic",
    ),
)

PERFORMANCE_TRAP_TEMPLATES: Tuple[Tuple[str, str, str, int], ...] = (
    (
        "Small_Rock",
        "A tiny rock prop intended for low-poly set dressing, but this version is over-detailed.",
        "/Game/Art/Env/LowPoly/Rocks/",
        100000,
    ),
    (
        "Pebble_Cluster",
        "A pebble cluster designed for distant background usage, but geometry budget is abnormally high.",
        "/Game/Art/Env/LowPoly/Pebbles/",
        85000,
    ),
    (
        "Sapling_LowPoly",
        "A sapling expected to be cheap filler in low-poly biome sets, but triangle count is extreme.",
        "/Game/Art/Env/LowPoly/Trees/",
        120000,
    ),
)


@dataclass(frozen=True)
class Config:
    count: int
    seed: int
    semantic_gap_ratio: float
    trap_ratio: float


def sanitize_id(text: str) -> str:
    lowered = text.lower()
    cleaned = re.sub(r"[^a-z0-9]+", "_", lowered).strip("_")
    return cleaned or "asset"


def load_seed_mesh_manifest(manifest_file: Path) -> List[str]:
    data = json.loads(manifest_file.read_text(encoding="utf-8"))
    entries = data.get("entries", [])
    if not isinstance(entries, list):
        raise ValueError("seed mesh manifest entries must be a list")

    object_paths: List[str] = []
    seen = set()
    for index, item in enumerate(entries):
        if not isinstance(item, dict):
            raise ValueError(f"manifest entries[{index}] must be an object")
        object_path = str(item.get("object_path", ""))
        if not UE_LONG_OBJECT_PATH_RE.match(object_path):
            raise ValueError(
                "manifest object_path must be UE long object path "
                f"(/Game/.../Asset.Asset), got: {object_path}"
            )
        if object_path not in seen:
            seen.add(object_path)
            object_paths.append(object_path)
    return object_paths


def _pick_asset_type(rng: random.Random) -> str:
    return rng.choices(ASSET_TYPES, weights=(0.35, 0.2, 0.45), k=1)[0]


def _pick_theme(rng: random.Random) -> str:
    return rng.choices(THEMES, weights=(0.38, 0.3, 0.32), k=1)[0]


def _normal_name(theme: str, asset_type: str, rng: random.Random) -> str:
    keyword = rng.choice(THEME_KEYWORDS[theme])
    noun = rng.choice(NOUNS_BY_TYPE[asset_type])
    pattern = rng.choice(NAME_PATTERNS_BY_THEME[theme])
    return pattern.format(kw=keyword.title(), noun=noun)


def _semantic_gap_name(theme: str, rng: random.Random) -> str:
    alias = rng.choice(THEME_ALIASES[theme])
    suffix = rng.choice(("Prime", "Core", "MKII", "Variant", "Node"))
    return f"{alias} {suffix}"


def _build_description(theme: str, rng: random.Random) -> str:
    keyword = rng.choice(THEME_KEYWORDS[theme])
    sentence_a = rng.choice(DESCRIPTION_PATTERNS).format(
        kw=keyword, scene=rng.choice(SCENES_BY_THEME[theme])
    )
    sentence_b = (
        f"Tags and params are curated for retrieval and audit benchmarks where {keyword} meaning "
        "must be inferred from long-form text."
    )
    return f"{sentence_a} {sentence_b}"


def _build_virtual_path(theme: str, asset_type: str, rng: random.Random, force_lowpoly: bool = False) -> str:
    type_segment = {
        "StaticMesh": "Meshes",
        "Sound": "Audio",
        "VFX": "VFX",
    }[asset_type]
    quality = "LowPoly" if force_lowpoly else rng.choice(("Gameplay", "Hero", "Shared"))
    return f"/Game/Art/{theme.title()}/{quality}/{type_segment}/"


def _triangle_count(asset_type: str, rng: random.Random, override: int | None = None) -> int:
    if override is not None:
        return override
    if asset_type == "Sound":
        return 0
    if asset_type == "VFX":
        return int(round(rng.uniform(80, 3500)))
    return int(round(rng.uniform(300, 18000)))


def _build_params(
    theme: str,
    asset_type: str,
    rng: random.Random,
    trap: bool,
    triangle_override: int | None = None,
) -> Dict[str, float | int]:
    if trap:
        damage = round(rng.uniform(60.0, 180.0), 2)
    else:
        theme_damage_ranges = {
            "fire": (180.0, 520.0),
            "ice": (120.0, 340.0),
            "forest": (70.0, 260.0),
        }
        low, high = theme_damage_ranges[theme]
        damage = round(rng.uniform(low, high), 2)

    triangle_count_lod0 = _triangle_count(asset_type, rng, override=triangle_override)
    size_base = {"StaticMesh": 2.4, "Sound": 1.2, "VFX": 1.9}[asset_type]
    return {
        "damage": damage,
        "triangle_count_lod0": triangle_count_lod0,
        "size": round(rng.uniform(size_base * 0.5, size_base * 1.6), 2),
        "radius": round(rng.uniform(90.0, 360.0), 2),
        "duration": round(rng.uniform(0.3, 4.0), 2),
        "cooldown": round(rng.uniform(1.0, 15.0), 2),
        "pitch": round(rng.uniform(0.75, 1.35), 2),
        "loudness": round(rng.uniform(-14.0, -1.0), 2),
    }


def _build_tags(
    theme: str,
    asset_type: str,
    gap: bool,
    trap: bool,
    trap_kind: str | None,
    extra_tags: Sequence[str] = (),
) -> List[str]:
    tags = list(THEME_TAGS[theme]) + list(TYPE_TAGS[asset_type])
    if gap:
        tags.append("semantic_gap")
    if trap:
        tags.append("trap")
        if trap_kind:
            tags.append(f"trap:{trap_kind}")
    tags.extend(extra_tags)
    return sorted(set(tags))


def _create_semantic_trap_asset(index: int, rng: random.Random) -> Dict[str, object]:
    asset_type = _pick_asset_type(rng)
    trap_name, trap_description, style_tag = rng.choice(SEMANTIC_TRAP_TEMPLATES)
    name = f"{trap_name}_{index:05d}"
    asset_id = sanitize_id(f"hci_trap_sem_{name}")
    params = _build_params(theme="forest", asset_type=asset_type, rng=rng, trap=True)
    tags = _build_tags(
        "forest",
        asset_type,
        gap=False,
        trap=True,
        trap_kind="semantic",
        extra_tags=("misleading_name", "actual:physical", style_tag),
    )

    return {
        "schema_version": 1,
        "id": asset_id,
        "name": name,
        "display_name": name,
        "type": asset_type,
        "virtual_path": _build_virtual_path("forest", asset_type, rng),
        "tags": tags,
        "description": trap_description,
        "params": params,
    }


def _create_performance_trap_asset(index: int, rng: random.Random) -> Dict[str, object]:
    trap_name, template_desc, virtual_path, triangle_count = rng.choice(PERFORMANCE_TRAP_TEMPLATES)
    name = f"{trap_name}_{index:05d}"
    asset_id = sanitize_id(f"hci_trap_perf_{name}")
    params = _build_params(
        theme="forest",
        asset_type="StaticMesh",
        rng=rng,
        trap=True,
        triangle_override=triangle_count,
    )
    description = (
        f"{template_desc} It is categorized as low-poly, but LOD0 triangle count is {triangle_count}, "
        "which should trigger performance mismatch auditing."
    )
    tags = _build_tags(
        "forest",
        "StaticMesh",
        gap=False,
        trap=True,
        trap_kind="performance",
        extra_tags=("expected:lowpoly", "actual:highpoly"),
    )

    return {
        "schema_version": 1,
        "id": asset_id,
        "name": name,
        "display_name": name,
        "type": "StaticMesh",
        "virtual_path": virtual_path,
        "tags": tags,
        "description": description,
        "params": params,
    }


def _create_regular_asset(index: int, rng: random.Random, semantic_gap: bool) -> Dict[str, object]:
    theme = _pick_theme(rng)
    asset_type = _pick_asset_type(rng)

    if semantic_gap:
        name = _semantic_gap_name(theme, rng)
        neutral_stub = f"hci_gap_asset_{index:05d}_{rng.randint(100, 999)}"
        asset_id = sanitize_id(neutral_stub)
    else:
        name = _normal_name(theme, asset_type, rng)
        asset_id = sanitize_id(f"hci_{theme}_{asset_type}_{index:05d}")

    description = _build_description(theme, rng)
    params = _build_params(theme, asset_type, rng, trap=False)
    tags = _build_tags(theme, asset_type, gap=semantic_gap, trap=False, trap_kind=None)

    return {
        "schema_version": 1,
        "id": asset_id,
        "name": name,
        "display_name": name,
        "type": asset_type,
        "virtual_path": _build_virtual_path(theme, asset_type, rng),
        "tags": tags,
        "description": description,
        "params": params,
    }


def build_dataset(
    count: int,
    seed: int = 42,
    semantic_gap_ratio: float = 0.3,
    trap_ratio: float = 0.12,
    representing_mesh_pool: Sequence[str] | None = None,
) -> List[Dict[str, object]]:
    if count <= 0:
        raise ValueError("count must be > 0")
    if not 0.0 <= semantic_gap_ratio <= 1.0:
        raise ValueError("semantic_gap_ratio must be within [0,1]")
    if not 0.0 <= trap_ratio <= 1.0:
        raise ValueError("trap_ratio must be within [0,1]")
    if semantic_gap_ratio + trap_ratio > 1.0:
        raise ValueError("semantic_gap_ratio + trap_ratio must be <= 1")

    rng = random.Random(seed)
    indices = list(range(count))
    rng.shuffle(indices)

    semantic_gap_target = int(round(count * semantic_gap_ratio))
    trap_target = int(round(count * trap_ratio))

    semantic_gap_set = set(indices[:semantic_gap_target])
    remaining = [idx for idx in indices if idx not in semantic_gap_set]
    trap_indices = remaining[:trap_target]

    perf_trap_target = trap_target // 2
    performance_trap_set = set(trap_indices[:perf_trap_target])
    semantic_trap_set = set(trap_indices[perf_trap_target:])
    mesh_pool = list(representing_mesh_pool or [])

    records: List[Dict[str, object]] = []
    for i in range(count):
        if i in performance_trap_set:
            record = _create_performance_trap_asset(i, rng)
        elif i in semantic_trap_set:
            record = _create_semantic_trap_asset(i, rng)
        else:
            record = _create_regular_asset(i, rng, semantic_gap=(i in semantic_gap_set))
        if mesh_pool:
            record["representing_mesh"] = rng.choice(mesh_pool)
        records.append(record)
    return records


def semantic_gap_flags(records: Sequence[Dict[str, object]]) -> List[bool]:
    return ["semantic_gap" in record.get("tags", []) for record in records]


def trap_flags(records: Sequence[Dict[str, object]]) -> List[bool]:
    return ["trap" in record.get("tags", []) for record in records]


def performance_trap_flags(records: Sequence[Dict[str, object]]) -> List[bool]:
    return ["trap:performance" in record.get("tags", []) for record in records]


def write_dataset(records: Sequence[Dict[str, object]], output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    for record in records:
        filename = output_dir / f"{record['id']}.hciabilitykit"
        filename.write_text(
            json.dumps(record, ensure_ascii=False, indent=2),
            encoding="utf-8",
        )


def write_report(
    records: Sequence[Dict[str, object]],
    output_dir: Path,
    seed: int,
    semantic_gap_ratio: float,
    trap_ratio: float,
    seed_mesh_pool_size: int = 0,
) -> None:
    type_histogram: Dict[str, int] = {t: 0 for t in ASSET_TYPES}
    high_triangle_count = 0
    for record in records:
        type_histogram[str(record["type"])] += 1
        triangles = int(record.get("params", {}).get("triangle_count_lod0", 0))
        if triangles >= 50000:
            high_triangle_count += 1

    report = {
        "count": len(records),
        "seed": seed,
        "semantic_gap_ratio_target": semantic_gap_ratio,
        "semantic_gap_count": sum(semantic_gap_flags(records)),
        "trap_ratio_target": trap_ratio,
        "trap_count": sum(trap_flags(records)),
        "performance_trap_count": sum(performance_trap_flags(records)),
        "high_triangle_asset_count": high_triangle_count,
        "seed_mesh_pool_size": seed_mesh_pool_size,
        "representing_mesh_assigned_count": sum(
            1 for record in records if "representing_mesh" in record
        ),
        "asset_types": type_histogram,
        "core_vocab": {
            "fire": list(THEME_KEYWORDS["fire"]),
            "ice": list(THEME_KEYWORDS["ice"]),
            "forest": list(THEME_KEYWORDS["forest"]),
        },
    }
    (output_dir / "generation_report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate synthetic .hciabilitykit assets.")
    parser.add_argument("--count", type=int, default=10000, help="How many assets to generate.")
    parser.add_argument("--seed", type=int, default=42, help="Deterministic random seed.")
    parser.add_argument(
        "--semantic-gap-ratio",
        type=float,
        default=0.3,
        help="Share of assets where name lacks core keywords but description contains them.",
    )
    parser.add_argument(
        "--trap-ratio",
        type=float,
        default=0.12,
        help="Share of trap assets (semantic + performance mismatches).",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("SourceData/AbilityKits/Generated"),
        help="Directory to write .hciabilitykit files.",
    )
    parser.add_argument(
        "--seed-mesh-manifest",
        type=Path,
        default=None,
        help="Optional seed mesh manifest JSON. When set, each record gets representing_mesh.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    seed_mesh_pool: List[str] = []
    if args.seed_mesh_manifest is not None:
        seed_mesh_pool = load_seed_mesh_manifest(args.seed_mesh_manifest)
        if not seed_mesh_pool:
            raise ValueError(
                f"seed mesh manifest has no entries: {args.seed_mesh_manifest}"
            )

    config = Config(
        count=args.count,
        seed=args.seed,
        semantic_gap_ratio=args.semantic_gap_ratio,
        trap_ratio=args.trap_ratio,
    )
    dataset = build_dataset(
        count=config.count,
        seed=config.seed,
        semantic_gap_ratio=config.semantic_gap_ratio,
        trap_ratio=config.trap_ratio,
        representing_mesh_pool=seed_mesh_pool,
    )
    write_dataset(dataset, args.output_dir)
    write_report(
        dataset,
        args.output_dir,
        config.seed,
        config.semantic_gap_ratio,
        config.trap_ratio,
        seed_mesh_pool_size=len(seed_mesh_pool),
    )
    print(
        f"Generated {len(dataset)} assets into {args.output_dir} "
        f"(semantic_gap={sum(semantic_gap_flags(dataset))}, trap={sum(trap_flags(dataset))}, "
        f"performance_trap={sum(performance_trap_flags(dataset))}, "
        f"representing_mesh_assigned={sum(1 for item in dataset if 'representing_mesh' in item)})."
    )


if __name__ == "__main__":
    main()
