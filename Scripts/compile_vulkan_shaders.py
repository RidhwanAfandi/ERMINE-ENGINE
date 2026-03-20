#!/usr/bin/env python3
"""
ERMINE-ENGINE Vulkan Shader Compilation Script
===============================================
Compiles all GLSL shaders in Resources/Shaders/Vulkan/ to SPIR-V binaries
using glslc from the Vulkan SDK.

Tests shaders for correctness BEFORE the engine loads them (hardened approach).

Usage:
    python compile_vulkan_shaders.py [--verbose] [--force] [--validate-only]

Exit codes:
    0 - All shaders compiled successfully
    1 - One or more shaders failed to compile
    2 - glslc not found
"""

import os
import sys
import subprocess
import hashlib
import json
import time
import shutil
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional, List, Dict

# ─────────────────────────────────────────────────────────────────────────────
# Configuration
# ─────────────────────────────────────────────────────────────────────────────

SCRIPT_DIR   = Path(__file__).parent.resolve()
REPO_ROOT    = SCRIPT_DIR.parent
SHADER_DIR   = REPO_ROOT / "Resources" / "Shaders" / "Vulkan"
SPIRV_DIR    = REPO_ROOT / "Resources" / "Shaders" / "Vulkan" / "SPIRV"
CACHE_FILE   = SPIRV_DIR / "shader_cache.json"

GLSL_EXTENSIONS = {
    ".vert": "vert",
    ".frag": "frag",
    ".comp": "comp",
    ".geom": "geom",
    ".tesc": "tesc",
    ".tese": "tese",
    # Also support .glsl with stage suffix in filename
}

STAGE_SUFFIXES = {
    "_vertex.glsl":   "vert",
    "_fragment.glsl": "frag",
    "_compute.glsl":  "comp",
    "_geom.glsl":     "geom",
    "_vert.glsl":     "vert",
    "_frag.glsl":     "frag",
    "_comp.glsl":     "comp",
}

# Vulkan-specific compile flags
GLSLC_FLAGS = [
    "--target-env=vulkan1.2",
    "-O",                       # Optimize
    "-Werror",                  # Treat warnings as errors
    "-I", str(SHADER_DIR),      # Include directory for shared headers
]

# ─────────────────────────────────────────────────────────────────────────────
# glslc discovery
# ─────────────────────────────────────────────────────────────────────────────

def find_glslc() -> Optional[str]:
    sdk = os.environ.get("VULKAN_SDK") or os.environ.get("VK_SDK_PATH")
    if sdk:
        for sub in ["Bin", "bin", "Bin32", "bin32"]:
            exe = Path(sdk) / sub / "glslc.exe"
            if exe.exists(): return str(exe)
            exe = Path(sdk) / sub / "glslc"
            if exe.exists(): return str(exe)

    found = shutil.which("glslc")
    return found


# ─────────────────────────────────────────────────────────────────────────────
# Shader compilation
# ─────────────────────────────────────────────────────────────────────────────

@dataclass
class ShaderResult:
    source_path: str
    output_path: str
    stage: str
    success: bool
    cached: bool = False
    error: str = ""
    compile_time_ms: float = 0.0
    spirv_size_bytes: int = 0


def file_hash(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        h.update(f.read())
    return h.hexdigest()


def load_cache(cache_path: Path) -> Dict[str, str]:
    if cache_path.exists():
        with open(cache_path) as f:
            return json.load(f)
    return {}


def save_cache(cache_path: Path, cache: Dict[str, str]) -> None:
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    with open(cache_path, "w") as f:
        json.dump(cache, f, indent=2)


def detect_stage(path: Path) -> Optional[str]:
    """Determine shader stage from filename."""
    name = path.name.lower()
    for suffix, stage in STAGE_SUFFIXES.items():
        if name.endswith(suffix):
            return stage
    ext = path.suffix.lower()
    return GLSL_EXTENSIONS.get(ext)


def compile_shader(
    glslc: str,
    source: Path,
    output: Path,
    stage: str,
    verbose: bool,
    cache: Dict[str, str],
    force: bool,
) -> ShaderResult:
    result = ShaderResult(
        source_path=str(source),
        output_path=str(output),
        stage=stage,
        success=False,
    )

    # Check cache
    src_hash = file_hash(str(source))
    cache_key = str(source.relative_to(SHADER_DIR)) if SHADER_DIR in source.parents else source.name
    if not force and cache.get(cache_key) == src_hash and output.exists():
        result.success = True
        result.cached = True
        result.spirv_size_bytes = output.stat().st_size
        return result

    output.parent.mkdir(parents=True, exist_ok=True)

    cmd = [glslc, f"-fshader-stage={stage}"] + GLSLC_FLAGS + [str(source), "-o", str(output)]

    if verbose:
        print(f"    $ {' '.join(cmd)}")

    t0 = time.monotonic()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        result.compile_time_ms = (time.monotonic() - t0) * 1000

        if proc.returncode == 0:
            result.success = True
            result.spirv_size_bytes = output.stat().st_size if output.exists() else 0
            cache[cache_key] = src_hash
        else:
            result.error = (proc.stderr or proc.stdout).strip()
    except subprocess.TimeoutExpired:
        result.error = "Compilation timed out (>60s)"
    except FileNotFoundError:
        result.error = f"glslc not found at: {glslc}"

    return result


def validate_spirv(spirv_path: Path) -> tuple[bool, str]:
    """
    Basic SPIR-V validation: check magic number and minimum size.
    Full validation requires spirv-val from the SDK.
    """
    if not spirv_path.exists():
        return False, "SPIR-V file does not exist"

    with open(spirv_path, "rb") as f:
        data = f.read(4)

    if len(data) < 4:
        return False, "SPIR-V file too small"

    SPIRV_MAGIC = b'\x03\x02\x23\x07'
    SPIRV_MAGIC_LE = b'\x07\x23\x02\x03'
    if data not in (SPIRV_MAGIC, SPIRV_MAGIC_LE):
        return False, f"Invalid SPIR-V magic: {data.hex()}"

    return True, "OK"


# ─────────────────────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────────────────────

PASS = "\033[92m[PASS]\033[0m"
FAIL = "\033[91m[FAIL]\033[0m"
WARN = "\033[93m[WARN]\033[0m"
SKIP = "\033[90m[SKIP]\033[0m"
INFO = "\033[94m[INFO]\033[0m"


def main():
    import argparse
    parser = argparse.ArgumentParser(description="ERMINE-ENGINE Vulkan Shader Compiler")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--force", action="store_true", help="Recompile even if unchanged")
    parser.add_argument("--validate-only", action="store_true", help="Only validate existing SPIR-V")
    args = parser.parse_args()

    print("=" * 70)
    print("  ERMINE-ENGINE Vulkan Shader Compilation")
    print("=" * 70)

    glslc = find_glslc()
    if not glslc:
        print(f"{FAIL} glslc not found. Install the Vulkan SDK.")
        print("      Set VULKAN_SDK environment variable.")
        sys.exit(2)

    print(f"{INFO} glslc: {glslc}")
    print(f"{INFO} Shader source: {SHADER_DIR}")
    print(f"{INFO} SPIR-V output: {SPIRV_DIR}")
    print()

    if not SHADER_DIR.exists():
        print(f"{WARN} Shader directory not found: {SHADER_DIR}")
        print(f"      Creating directory...")
        SHADER_DIR.mkdir(parents=True, exist_ok=True)
        print(f"{INFO} No shaders to compile yet.")
        sys.exit(0)

    # Discover shaders
    sources = []
    for f in SHADER_DIR.rglob("*"):
        if f.is_file() and f.suffix in (".glsl", ".vert", ".frag", ".comp", ".geom", ".tesc", ".tese"):
            # Skip the SPIRV subdirectory
            if "SPIRV" in f.parts:
                continue
            stage = detect_stage(f)
            if stage:
                sources.append((f, stage))
            else:
                print(f"{WARN} Cannot determine stage for: {f.name} — skipping")

    if not sources:
        print(f"{INFO} No GLSL shaders found in {SHADER_DIR}")
        print(f"      Add .glsl or .vert/.frag/.comp files to compile.")
        sys.exit(0)

    print(f"{INFO} Found {len(sources)} shader(s) to process")
    print()

    cache = load_cache(CACHE_FILE)
    results: List[ShaderResult] = []
    failed = 0
    cached_count = 0

    if args.validate_only:
        # Only validate existing SPIR-V files
        for spirv in SPIRV_DIR.rglob("*.spv"):
            ok, msg = validate_spirv(spirv)
            if ok:
                print(f"  {PASS} {spirv.name} ({spirv.stat().st_size} bytes)")
            else:
                print(f"  {FAIL} {spirv.name}: {msg}")
                failed += 1
    else:
        for source, stage in sorted(sources, key=lambda x: x[0].name):
            rel = source.relative_to(SHADER_DIR)
            out_name = str(rel).replace(os.sep, "_").replace("/", "_") + ".spv"
            output = SPIRV_DIR / out_name

            r = compile_shader(glslc, source, output, stage, args.verbose, cache, args.force)
            results.append(r)

            if r.cached:
                cached_count += 1
                if args.verbose:
                    print(f"  {SKIP} [cached] {source.name}")
            elif r.success:
                ok, val_msg = validate_spirv(output)
                if ok:
                    print(f"  {PASS} [{stage:4s}] {source.name} → {r.spirv_size_bytes} bytes ({r.compile_time_ms:.0f}ms)")
                else:
                    print(f"  {FAIL} [{stage:4s}] {source.name}: SPIR-V validation failed: {val_msg}")
                    r.success = False
                    failed += 1
            else:
                print(f"  {FAIL} [{stage:4s}] {source.name}")
                print(f"        Error: {r.error}")
                failed += 1

        save_cache(CACHE_FILE, cache)

    print()
    print("=" * 70)
    compiled = len(results) - cached_count
    print(f"  Compiled: {compiled}  Cached: {cached_count}  Failed: {failed}")
    if failed == 0:
        print(f"  {PASS} All shaders compiled and validated successfully.")
    else:
        print(f"  {FAIL} {failed} shader(s) failed. Fix errors before running the engine.")
    print("=" * 70)

    sys.exit(1 if failed > 0 else 0)


if __name__ == "__main__":
    main()
