#!/usr/bin/env python3
"""
Vulkan Feature Test Script for ERMINE-ENGINE
============================================
Tests all Vulkan features required by the renderer BEFORE any implementation runs.
Run this script before building to validate machine capabilities.

Usage:
    python test_vulkan_features.py [--verbose] [--json-output path]

Exit codes:
    0 - All required features present
    1 - Missing required features (renderer cannot initialize)
    2 - Missing optional features (reduced quality mode)
    3 - Vulkan not available
"""

import subprocess
import sys
import json
import os
import struct
import ctypes
from dataclasses import dataclass, field, asdict
from typing import Optional, List, Dict, Tuple
from enum import Enum

# ─────────────────────────────────────────────────────────────────────────────
# Feature requirement levels
# ─────────────────────────────────────────────────────────────────────────────

class Requirement(Enum):
    REQUIRED  = "REQUIRED"    # Engine cannot run without this
    PREFERRED = "PREFERRED"   # Reduces quality or performance if absent
    OPTIONAL  = "OPTIONAL"    # Nice-to-have, graceful degradation


@dataclass
class FeatureTest:
    name: str
    requirement: Requirement
    description: str
    result: Optional[bool] = None
    actual_value: Optional[str] = None
    reason: Optional[str] = None


@dataclass
class CapabilityReport:
    device_name: str = ""
    driver_version: str = ""
    vulkan_api_version: str = ""
    vendor_id: int = 0
    device_type: str = ""

    # Adaptive buffer sizes (derived from device limits)
    max_uniform_buffer_range: int = 0
    max_storage_buffer_range: int = 0
    max_push_constants_size: int = 0
    max_memory_allocation_count: int = 0
    max_per_stage_descriptor_storage_buffers: int = 0
    max_per_stage_descriptor_sampled_images: int = 0
    max_bound_descriptor_sets: int = 0
    max_framebuffer_width: int = 0
    max_framebuffer_height: int = 0
    max_image_dimension_2d: int = 0
    max_image_array_layers: int = 0
    timestamp_compute_and_graphics: bool = False
    timestamp_period: float = 0.0

    # Memory heaps
    device_local_memory_mb: int = 0
    host_visible_memory_mb: int = 0

    # Computed adaptive sizes
    recommended_ubo_size: int = 0           # For lights, materials, etc.
    recommended_ssbo_size: int = 0          # For draw commands, skeletal data
    recommended_staging_buffer_mb: int = 0  # For upload batching
    recommended_descriptor_pool_size: int = 0

    features: List[FeatureTest] = field(default_factory=list)
    passed: bool = False
    warnings: List[str] = field(default_factory=list)


# ─────────────────────────────────────────────────────────────────────────────
# Vulkan SDK detection
# ─────────────────────────────────────────────────────────────────────────────

def find_vulkan_sdk() -> Optional[str]:
    """Locate the Vulkan SDK installation."""
    sdk_var = os.environ.get("VULKAN_SDK") or os.environ.get("VK_SDK_PATH")
    if sdk_var and os.path.isdir(sdk_var):
        return sdk_var

    # Common install locations
    candidates = [
        r"C:\VulkanSDK",
        r"C:\Program Files\VulkanSDK",
        "/usr/share/vulkan",
        "/usr/local",
    ]
    for base in candidates:
        if os.path.isdir(base):
            # Pick latest version sub-directory
            try:
                versions = sorted(os.listdir(base), reverse=True)
                for v in versions:
                    candidate = os.path.join(base, v)
                    if os.path.isdir(candidate):
                        return candidate
            except PermissionError:
                pass
    return None


def find_vulkaninfo() -> Optional[str]:
    """Find the vulkaninfo executable."""
    sdk = find_vulkan_sdk()
    if sdk:
        for sub in ["Bin", "bin", "bin32", "Bin32"]:
            exe = os.path.join(sdk, sub, "vulkaninfo.exe")
            if os.path.isfile(exe):
                return exe
            exe = os.path.join(sdk, sub, "vulkaninfo")
            if os.path.isfile(exe):
                return exe

    # Try PATH
    import shutil
    found = shutil.which("vulkaninfo")
    return found


# ─────────────────────────────────────────────────────────────────────────────
# Parse vulkaninfo JSON output
# ─────────────────────────────────────────────────────────────────────────────

def run_vulkaninfo() -> Optional[dict]:
    """Run vulkaninfo --json and parse the output."""
    exe = find_vulkaninfo()
    if not exe:
        print("[ERROR] vulkaninfo not found. Install the Vulkan SDK.")
        return None

    try:
        result = subprocess.run(
            [exe, "--json"],
            capture_output=True, text=True, timeout=30
        )
        # vulkaninfo may exit non-zero but still produce valid JSON
        raw = result.stdout.strip()
        if not raw:
            raw = result.stderr.strip()

        # Find JSON object in output (vulkaninfo may prefix with text)
        start = raw.find('{')
        if start > 0:
            raw = raw[start:]

        return json.loads(raw)
    except subprocess.TimeoutExpired:
        print("[ERROR] vulkaninfo timed out.")
        return None
    except json.JSONDecodeError as e:
        print(f"[ERROR] Failed to parse vulkaninfo output: {e}")
        return None
    except FileNotFoundError:
        print(f"[ERROR] vulkaninfo executable not found at: {exe}")
        return None


def parse_version(v: int) -> str:
    """Decode a packed Vulkan version integer."""
    major = (v >> 22) & 0x3FF
    minor = (v >> 12) & 0x3FF
    patch = v & 0xFFF
    return f"{major}.{minor}.{patch}"


# ─────────────────────────────────────────────────────────────────────────────
# Feature test definitions
# ─────────────────────────────────────────────────────────────────────────────

REQUIRED_EXTENSIONS = [
    "VK_KHR_swapchain",
    "VK_KHR_maintenance1",         # Negative viewport heights
    "VK_KHR_multiview",            # Used for shadow cubemap rendering
    "VK_EXT_descriptor_indexing",  # Bindless textures (like OpenGL's bindless handles)
]

PREFERRED_EXTENSIONS = [
    "VK_KHR_dynamic_rendering",    # Modern render pass approach
    "VK_EXT_memory_budget",        # Accurate VRAM reporting
    "VK_NV_mesh_shader",           # Future: mesh shaders
    "VK_EXT_mesh_shader",          # Future: mesh shaders (cross-vendor)
    "VK_KHR_ray_tracing_pipeline", # Future: hardware ray tracing
    "VK_KHR_acceleration_structure",
    "VK_EXT_shader_atomic_float",  # Needed for GI voxelization
    "VK_KHR_shader_float16_int8",  # Half-precision math
    "VK_KHR_buffer_device_address",# For indirect draw + GPU pointers
    "VK_EXT_calibrated_timestamps",# Accurate GPU timing
]

REQUIRED_FEATURES = [
    "geometryShader",
    "tessellationShader",
    "multiDrawIndirect",           # Core to the indirect draw architecture
    "drawIndirectFirstInstance",
    "fragmentStoresAndAtomics",    # Used in picking pass
    "shaderStorageImageWriteWithoutFormat",
    "samplerAnisotropy",           # Texture quality
    "depthClamp",                  # Needed for shadow maps
    "fillModeNonSolid",            # Debug wireframe rendering
]

PREFERRED_FEATURES = [
    "shaderFloat64",               # Double precision shaders
    "shaderInt64",                 # 64-bit integers
    "shaderInt16",
    "multiViewport",               # Editor split-view
    "shaderSampledImageArrayDynamicIndexing",  # Bindless textures
    "shaderStorageBufferArrayDynamicIndexing",
    "shaderStorageImageArrayDynamicIndexing",
    "robustBufferAccess",          # Safety: out-of-bounds protection
    "pipelineStatisticsQuery",     # GPU profiler
    "inheritedQueries",
    "wideLines",                   # Debug line rendering
    "largePoints",
    "alphaToOne",
    "independentBlend",            # Per-attachment blend states for G-buffer
    "logicOp",
    "depthBiasClamp",              # Better shadow acne reduction
    "depthBounds",
    "shaderResourceResidency",
    "sparseBinding",               # Virtual texturing potential
]

# Minimum limits required by the renderer
LIMIT_REQUIREMENTS: List[Tuple[str, int, Requirement, str]] = [
    # (limit_name, minimum_value, requirement, description)
    ("maxBoundDescriptorSets",           4,        Requirement.REQUIRED,  "G-buffer, materials, textures, lights"),
    ("maxDescriptorSetUniformBuffers",   8,        Requirement.REQUIRED,  "Camera, lights, probes, shadow matrices"),
    ("maxDescriptorSetStorageBuffers",   6,        Requirement.REQUIRED,  "Draw cmds, draw info, skeletal, materials, textures, shadow views"),
    ("maxDescriptorSetSampledImages",    16,       Requirement.REQUIRED,  "G-buffer textures + shadow maps + material textures"),
    ("maxPushConstantsSize",             64,       Requirement.REQUIRED,  "Per-draw push constants (model matrix index)"),
    ("maxStorageBufferRange",            134217728,Requirement.REQUIRED,  "128 MB SSBO for large meshes + skeletal data"),
    ("maxUniformBufferRange",            65536,    Requirement.REQUIRED,  "64 KB UBO for lights and probes"),
    ("maxImageDimension2D",              4096,     Requirement.REQUIRED,  "Minimum texture size"),
    ("maxImageArrayLayers",              256,      Requirement.REQUIRED,  "Shadow map array layers"),
    ("maxFramebufferWidth",              2048,     Requirement.REQUIRED,  "Minimum framebuffer resolution"),
    ("maxFramebufferHeight",             2048,     Requirement.REQUIRED,  "Minimum framebuffer resolution"),
    ("maxColorAttachments",              4,        Requirement.REQUIRED,  "G-buffer needs 4 MRTs"),
    ("maxSamplerAnisotropy",             4.0,      Requirement.PREFERRED, "Texture filtering quality"),
    ("maxImageDimension2D",              8192,     Requirement.PREFERRED, "4K texture support"),
    ("maxStorageBufferRange",            536870912,Requirement.PREFERRED, "512 MB SSBO for large scenes"),
    ("maxDrawIndirectCount",             1048576,  Requirement.PREFERRED, "Large indirect draw batches"),
    ("maxPerStageDescriptorSampledImages", 128,    Requirement.PREFERRED, "Large bindless texture arrays"),
]


# ─────────────────────────────────────────────────────────────────────────────
# Adaptive buffer sizing logic
# ─────────────────────────────────────────────────────────────────────────────

def compute_adaptive_sizes(report: CapabilityReport) -> None:
    """
    Compute recommended buffer sizes based on actual device capabilities.
    These values will be written to a header/config file for the engine to use.
    """
    max_ssbo = report.max_storage_buffer_range
    max_ubo  = report.max_uniform_buffer_range
    vram_mb  = report.device_local_memory_mb

    # UBO: lights, probes, shadow matrices
    # Clamp to sensible range: 4KB minimum, 256KB maximum, prefer 64KB
    report.recommended_ubo_size = min(max_ubo, max(4096, min(262144, max_ubo // 4)))

    # SSBO: draw commands + draw info + skeletal + materials + textures + shadow views
    # Scale with available VRAM: low=64MB, mid=256MB, high=512MB
    if vram_mb < 2048:        # < 2 GB VRAM (integrated / low-end)
        ssbo_budget = 64  * 1024 * 1024
    elif vram_mb < 6144:      # 2-6 GB VRAM (mid-range)
        ssbo_budget = 256 * 1024 * 1024
    else:                     # > 6 GB VRAM (high-end)
        ssbo_budget = 512 * 1024 * 1024
    report.recommended_ssbo_size = min(max_ssbo, ssbo_budget)

    # Staging buffer: used for texture uploads, mesh uploads
    if vram_mb < 2048:
        report.recommended_staging_buffer_mb = 64
    elif vram_mb < 6144:
        report.recommended_staging_buffer_mb = 256
    else:
        report.recommended_staging_buffer_mb = 512

    # Descriptor pool size
    if report.max_per_stage_descriptor_sampled_images >= 128:
        report.recommended_descriptor_pool_size = 1024
    elif report.max_per_stage_descriptor_sampled_images >= 64:
        report.recommended_descriptor_pool_size = 512
    else:
        report.recommended_descriptor_pool_size = 256


# ─────────────────────────────────────────────────────────────────────────────
# Main test runner
# ─────────────────────────────────────────────────────────────────────────────

def run_tests(data: dict, verbose: bool = False) -> CapabilityReport:
    report = CapabilityReport()
    all_required_pass = True

    # ── Locate device info ──────────────────────────────────────────────────
    # vulkaninfo JSON schema: top-level key "devices" or legacy flat
    devices = data.get("devices", [])
    if not devices:
        # Try legacy format
        device_info = data
    else:
        # Pick discrete GPU if available, else first device
        device_info = devices[0]
        for d in devices:
            dtype = d.get("properties", {}).get("deviceType", "")
            if "DISCRETE" in dtype.upper():
                device_info = d
                break

    props = device_info.get("properties", device_info.get("VkPhysicalDeviceProperties", {}))
    report.device_name    = props.get("deviceName", "Unknown")
    report.driver_version = parse_version(props.get("driverVersion", 0))
    report.vulkan_api_version = parse_version(props.get("apiVersion", 0))
    report.vendor_id      = props.get("vendorID", 0)
    device_type_raw       = props.get("deviceType", 0)
    type_map = {1: "INTEGRATED", 2: "DISCRETE", 3: "VIRTUAL", 4: "CPU"}
    report.device_type    = type_map.get(device_type_raw, str(device_type_raw))

    limits = props.get("limits", {})

    # ── Capture key limits ───────────────────────────────────────────────────
    report.max_uniform_buffer_range            = limits.get("maxUniformBufferRange", 0)
    report.max_storage_buffer_range            = limits.get("maxStorageBufferRange", 0)
    report.max_push_constants_size             = limits.get("maxPushConstantsSize", 0)
    report.max_memory_allocation_count         = limits.get("maxMemoryAllocationCount", 0)
    report.max_per_stage_descriptor_storage_buffers = limits.get("maxPerStageDescriptorStorageBuffers", 0)
    report.max_per_stage_descriptor_sampled_images  = limits.get("maxPerStageDescriptorSampledImages", 0)
    report.max_bound_descriptor_sets           = limits.get("maxBoundDescriptorSets", 0)
    report.max_framebuffer_width               = limits.get("maxFramebufferWidth", 0)
    report.max_framebuffer_height              = limits.get("maxFramebufferHeight", 0)
    report.max_image_dimension_2d              = limits.get("maxImageDimension2D", 0)
    report.max_image_array_layers              = limits.get("maxImageArrayLayers", 0)
    report.timestamp_compute_and_graphics      = bool(limits.get("timestampComputeAndGraphics", False))
    report.timestamp_period                    = float(limits.get("timestampPeriod", 0.0))

    # ── Memory heaps ─────────────────────────────────────────────────────────
    mem_props = device_info.get("memoryProperties", device_info.get("VkPhysicalDeviceMemoryProperties", {}))
    heaps = mem_props.get("memoryHeaps", [])
    for heap in heaps:
        flags = heap.get("flags", 0)
        size_bytes = heap.get("size", 0)
        size_mb = size_bytes // (1024 * 1024)
        if flags & 1:  # VK_MEMORY_HEAP_DEVICE_LOCAL_BIT
            report.device_local_memory_mb += size_mb
        else:
            report.host_visible_memory_mb += size_mb

    # ── Compute adaptive buffer sizes ────────────────────────────────────────
    compute_adaptive_sizes(report)

    # ── Extension tests ──────────────────────────────────────────────────────
    ext_list_raw = device_info.get("extensions", [])
    if isinstance(ext_list_raw, list):
        ext_names = set(
            e.get("extensionName", e) if isinstance(e, dict) else str(e)
            for e in ext_list_raw
        )
    elif isinstance(ext_list_raw, dict):
        ext_names = set(ext_list_raw.keys())
    else:
        ext_names = set()

    for ext in REQUIRED_EXTENSIONS:
        t = FeatureTest(
            name=ext,
            requirement=Requirement.REQUIRED,
            description=f"Required device extension: {ext}",
        )
        t.result = ext in ext_names
        t.actual_value = "present" if t.result else "absent"
        if not t.result:
            t.reason = "Extension not available on this device."
            all_required_pass = False
        report.features.append(t)

    for ext in PREFERRED_EXTENSIONS:
        t = FeatureTest(
            name=ext,
            requirement=Requirement.PREFERRED,
            description=f"Preferred device extension: {ext}",
        )
        t.result = ext in ext_names
        t.actual_value = "present" if t.result else "absent"
        if not t.result:
            t.reason = "Extension not available; feature disabled."
        report.features.append(t)

    # ── Device feature tests ─────────────────────────────────────────────────
    features_raw = device_info.get("features", device_info.get("VkPhysicalDeviceFeatures", {}))

    for feat in REQUIRED_FEATURES:
        t = FeatureTest(
            name=feat,
            requirement=Requirement.REQUIRED,
            description=f"Required device feature: {feat}",
        )
        val = features_raw.get(feat, False)
        t.result = bool(val)
        t.actual_value = str(val)
        if not t.result:
            t.reason = f"Device feature '{feat}' not supported."
            all_required_pass = False
        report.features.append(t)

    for feat in PREFERRED_FEATURES:
        t = FeatureTest(
            name=feat,
            requirement=Requirement.PREFERRED,
            description=f"Preferred device feature: {feat}",
        )
        val = features_raw.get(feat, False)
        t.result = bool(val)
        t.actual_value = str(val)
        if not t.result:
            t.reason = f"Feature '{feat}' not available; running in reduced mode."
        report.features.append(t)

    # ── Limit tests ──────────────────────────────────────────────────────────
    seen_limit_tests: Dict[str, bool] = {}
    for limit_name, min_val, req, desc in LIMIT_REQUIREMENTS:
        actual = limits.get(limit_name, 0)
        if isinstance(actual, (int, float)):
            passed_limit = actual >= min_val
        else:
            passed_limit = False

        key = f"{limit_name}>={min_val}"
        t = FeatureTest(
            name=f"limit:{limit_name}>={min_val}",
            requirement=req,
            description=desc,
            result=passed_limit,
            actual_value=str(actual),
        )
        if not passed_limit:
            t.reason = f"Device reports {actual}, need >= {min_val}."
            if req == Requirement.REQUIRED:
                all_required_pass = False
                report.warnings.append(f"REQUIRED limit {limit_name} too low: {actual} < {min_val}")
            else:
                report.warnings.append(f"PREFERRED limit {limit_name}: {actual} (prefer >= {min_val})")
        report.features.append(t)

    # ── Triple buffering check ───────────────────────────────────────────────
    # We test for Mailbox present mode availability (needed for efficient triple buffering).
    # vulkaninfo doesn't always expose this directly; we note it as a runtime check.
    t = FeatureTest(
        name="presentMode:MAILBOX_KHR",
        requirement=Requirement.PREFERRED,
        description="VK_PRESENT_MODE_MAILBOX_KHR for tear-free triple buffering",
        result=None,
        reason="This is a runtime check - test in VulkanSwapchain::Init().",
        actual_value="runtime"
    )
    report.features.append(t)

    # ── Vulkan API version check ─────────────────────────────────────────────
    api_ver = props.get("apiVersion", 0)
    major = (api_ver >> 22) & 0x3FF
    minor = (api_ver >> 12) & 0x3FF
    t = FeatureTest(
        name="apiVersion>=1.2",
        requirement=Requirement.REQUIRED,
        description="Vulkan API 1.2 for descriptor indexing and timeline semaphores",
        result=(major > 1) or (major == 1 and minor >= 2),
        actual_value=f"{major}.{minor}",
    )
    if not t.result:
        t.reason = f"Found Vulkan {major}.{minor}, need >= 1.2."
        all_required_pass = False
    report.features.append(t)

    report.passed = all_required_pass
    return report


# ─────────────────────────────────────────────────────────────────────────────
# Config header generation
# ─────────────────────────────────────────────────────────────────────────────

def generate_capability_header(report: CapabilityReport, output_path: str) -> None:
    """
    Write VulkanCapabilities_Generated.h with compile-time constants derived
    from the device's actual limits. Consumed by VulkanDeviceCapabilities.h.
    """
    lines = [
        "// AUTO-GENERATED by test_vulkan_features.py - DO NOT EDIT",
        f"// Device: {report.device_name}",
        f"// Driver: {report.driver_version}",
        f"// Vulkan: {report.vulkan_api_version}",
        "",
        "#pragma once",
        "",
        "// ──────────────────────────────────────────────────────────────────────────",
        "// Adaptive buffer sizes (computed from device limits at last capability scan)",
        "// ──────────────────────────────────────────────────────────────────────────",
        "",
        f"#define VK_ADAPTIVE_UBO_SIZE              {report.recommended_ubo_size}u",
        f"#define VK_ADAPTIVE_SSBO_SIZE             {report.recommended_ssbo_size}u",
        f"#define VK_ADAPTIVE_STAGING_BUFFER_MB     {report.recommended_staging_buffer_mb}u",
        f"#define VK_ADAPTIVE_DESCRIPTOR_POOL_SIZE  {report.recommended_descriptor_pool_size}u",
        "",
        "// Device limits (for runtime assertions)",
        f"#define VK_DEVICE_MAX_UBO_RANGE           {report.max_uniform_buffer_range}u",
        f"#define VK_DEVICE_MAX_SSBO_RANGE          {report.max_storage_buffer_range}u",
        f"#define VK_DEVICE_MAX_PUSH_CONSTANTS      {report.max_push_constants_size}u",
        f"#define VK_DEVICE_MAX_IMAGE_DIM2D         {report.max_image_dimension_2d}u",
        f"#define VK_DEVICE_MAX_IMAGE_ARRAY_LAYERS  {report.max_image_array_layers}u",
        f"#define VK_DEVICE_LOCAL_MEMORY_MB         {report.device_local_memory_mb}u",
        "",
        "// Optional feature availability",
    ]

    # Add extension flags
    ext_flags = {
        "VK_KHR_dynamic_rendering":     "VK_HAS_DYNAMIC_RENDERING",
        "VK_EXT_descriptor_indexing":   "VK_HAS_DESCRIPTOR_INDEXING",
        "VK_KHR_buffer_device_address": "VK_HAS_BUFFER_DEVICE_ADDRESS",
        "VK_EXT_mesh_shader":           "VK_HAS_MESH_SHADERS",
        "VK_KHR_ray_tracing_pipeline":  "VK_HAS_RAY_TRACING",
        "VK_EXT_shader_atomic_float":   "VK_HAS_SHADER_ATOMIC_FLOAT",
    }
    feature_map = {f.name: f.result for f in report.features}
    for ext, macro in ext_flags.items():
        present = feature_map.get(ext, False)
        lines.append(f"#define {macro} {'1' if present else '0'}")

    lines += ["", "// ── end auto-generated ──────────────────────────────────────────────────"]

    with open(output_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"[OK] Wrote capability header: {output_path}")


# ─────────────────────────────────────────────────────────────────────────────
# Reporting
# ─────────────────────────────────────────────────────────────────────────────

PASS  = "\033[92m[PASS]\033[0m"
FAIL  = "\033[91m[FAIL]\033[0m"
WARN  = "\033[93m[WARN]\033[0m"
INFO  = "\033[94m[INFO]\033[0m"
SKIP  = "\033[90m[SKIP]\033[0m"


def print_report(report: CapabilityReport, verbose: bool) -> None:
    bar = "=" * 70
    print(bar)
    print(f"  ERMINE-ENGINE Vulkan Capability Report")
    print(bar)
    print(f"  Device  : {report.device_name}")
    print(f"  Type    : {report.device_type}")
    print(f"  Driver  : {report.driver_version}")
    print(f"  Vulkan  : {report.vulkan_api_version}")
    print(f"  VRAM    : {report.device_local_memory_mb} MB (device local)")
    print(bar)
    print()
    print("  Adaptive Buffer Sizes")
    print(f"    UBO size            : {report.recommended_ubo_size // 1024} KB")
    print(f"    SSBO size           : {report.recommended_ssbo_size // (1024*1024)} MB")
    print(f"    Staging buffer      : {report.recommended_staging_buffer_mb} MB")
    print(f"    Descriptor pool     : {report.recommended_descriptor_pool_size}")
    print()

    required_failures = []
    preferred_failures = []

    for t in report.features:
        if t.result is None:
            tag = SKIP
        elif t.result:
            tag = PASS
        elif t.requirement == Requirement.REQUIRED:
            tag = FAIL
            required_failures.append(t)
        else:
            tag = WARN
            preferred_failures.append(t)

        if verbose or not t.result:
            suffix = f"  ({t.actual_value})" if t.actual_value else ""
            print(f"  {tag} [{t.requirement.value:9s}] {t.name}{suffix}")
            if t.reason and not t.result:
                print(f"             └─ {t.reason}")

    print()
    print(bar)
    if report.passed:
        print(f"  {PASS} All REQUIRED features present. Vulkan renderer CAN initialize.")
    else:
        print(f"  {FAIL} REQUIRED features missing. Vulkan renderer CANNOT initialize.")
        for t in required_failures:
            print(f"        ✗ {t.name}: {t.reason or 'not available'}")

    if preferred_failures:
        print(f"\n  {WARN} {len(preferred_failures)} PREFERRED feature(s) unavailable (reduced quality):")
        for t in preferred_failures[:5]:
            print(f"        ~ {t.name}")
        if len(preferred_failures) > 5:
            print(f"        ... and {len(preferred_failures) - 5} more")

    if report.warnings:
        print()
        for w in report.warnings:
            print(f"  {WARN} {w}")

    print(bar)


# ─────────────────────────────────────────────────────────────────────────────
# Entry point
# ─────────────────────────────────────────────────────────────────────────────

def main():
    import argparse

    parser = argparse.ArgumentParser(description="ERMINE-ENGINE Vulkan Feature Test")
    parser.add_argument("--verbose", action="store_true", help="Show all tests including passing ones")
    parser.add_argument("--json-output", metavar="PATH", help="Write JSON report to file")
    parser.add_argument("--header-output", metavar="PATH",
                        help="Write VulkanCapabilities_Generated.h",
                        default=None)
    args = parser.parse_args()

    print("[INFO] Running vulkaninfo...")
    data = run_vulkaninfo()

    if data is None:
        print("[ERROR] Could not obtain Vulkan device info.")
        print("        Ensure the Vulkan SDK is installed and VULKAN_SDK is set.")
        sys.exit(3)

    report = run_tests(data, verbose=args.verbose)
    print_report(report, verbose=args.verbose)

    if args.json_output:
        with open(args.json_output, "w") as f:
            # Convert dataclass to dict for JSON serialization
            def to_dict(obj):
                if isinstance(obj, FeatureTest):
                    return {
                        "name": obj.name,
                        "requirement": obj.requirement.value,
                        "description": obj.description,
                        "result": obj.result,
                        "actual_value": obj.actual_value,
                        "reason": obj.reason,
                    }
                return asdict(obj)
            d = asdict(report)
            d["features"] = [to_dict(f) for f in report.features]
            json.dump(d, f, indent=2)
        print(f"[INFO] JSON report written to: {args.json_output}")

    header_out = args.header_output
    if header_out is None:
        # Default: write next to this script in the engine include tree
        script_dir = os.path.dirname(os.path.abspath(__file__))
        engine_root = os.path.dirname(script_dir)
        header_out = os.path.join(
            engine_root, "Ermine-Engine", "include", "Vulkan",
            "VulkanCapabilities_Generated.h"
        )
    generate_capability_header(report, header_out)

    if not report.passed:
        sys.exit(1)

    warnings_only = any(
        not f.result and f.requirement != Requirement.REQUIRED
        for f in report.features
        if f.result is not None
    )
    sys.exit(2 if warnings_only else 0)


if __name__ == "__main__":
    main()
