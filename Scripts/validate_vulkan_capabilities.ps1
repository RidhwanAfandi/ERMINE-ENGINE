# ============================================================================
# ERMINE-ENGINE Vulkan Capability Validation Script (PowerShell)
# ============================================================================
# Quick pre-build check for Vulkan SDK and device support.
# Run from the repository root:
#   .\Scripts\validate_vulkan_capabilities.ps1
#
# Also invoked by the Python test script via subprocess for Windows-native
# checks (registry, driver info, DXGI enumeration).
# ============================================================================

param(
    [switch]$Verbose,
    [switch]$GenerateHeader,
    [string]$HeaderOutput = ""
)

$ErrorActionPreference = "Continue"
$script:failures = @()
$script:warnings = @()

function Write-Pass   { param($msg) Write-Host "  [PASS] $msg" -ForegroundColor Green  }
function Write-Fail   { param($msg) Write-Host "  [FAIL] $msg" -ForegroundColor Red; $script:failures += $msg }
function Write-Warn   { param($msg) Write-Host "  [WARN] $msg" -ForegroundColor Yellow; $script:warnings += $msg }
function Write-Info   { param($msg) Write-Host "  [INFO] $msg" -ForegroundColor Cyan  }
function Write-Header { param($msg) Write-Host "`n$("=" * 70)`n  $msg`n$("=" * 70)" -ForegroundColor White }

# ─────────────────────────────────────────────────────────────────────────────
# 1. Check Vulkan SDK installation
# ─────────────────────────────────────────────────────────────────────────────
Write-Header "ERMINE-ENGINE Vulkan Capability Validation"

$vulkanSdk = $env:VULKAN_SDK
if (-not $vulkanSdk) {
    $vulkanSdk = $env:VK_SDK_PATH
}

if ($vulkanSdk -and (Test-Path $vulkanSdk)) {
    Write-Pass "Vulkan SDK found at: $vulkanSdk"
} else {
    # Try registry
    $regPath = "HKLM:\SOFTWARE\LunarG\VulkanSDK"
    $sdkVersions = @()
    if (Test-Path $regPath) {
        $sdkVersions = Get-ChildItem $regPath -ErrorAction SilentlyContinue |
                       Sort-Object Name -Descending
    }
    if ($sdkVersions.Count -gt 0) {
        $vulkanSdk = (Get-ItemProperty $sdkVersions[0].PSPath).InstallPath
        Write-Pass "Vulkan SDK found via registry: $vulkanSdk"
    } else {
        Write-Fail "Vulkan SDK not found. Install from https://vulkan.lunarg.com"
        Write-Info "Set VULKAN_SDK environment variable after installation."
        exit 3
    }
}

# ─────────────────────────────────────────────────────────────────────────────
# 2. Locate vulkaninfo
# ─────────────────────────────────────────────────────────────────────────────

$vulkanInfoExe = $null
foreach ($subdir in @("Bin", "bin", "Bin32", "bin32")) {
    $candidate = Join-Path $vulkanSdk "$subdir\vulkaninfo.exe"
    if (Test-Path $candidate) {
        $vulkanInfoExe = $candidate
        break
    }
}

if (-not $vulkanInfoExe) {
    $vulkanInfoExe = (Get-Command vulkaninfo -ErrorAction SilentlyContinue)?.Source
}

if ($vulkanInfoExe) {
    Write-Pass "vulkaninfo found: $vulkanInfoExe"
} else {
    Write-Fail "vulkaninfo.exe not found in SDK. SDK installation may be corrupt."
    exit 3
}

# ─────────────────────────────────────────────────────────────────────────────
# 3. Run vulkaninfo summary
# ─────────────────────────────────────────────────────────────────────────────

Write-Info "Running vulkaninfo --summary..."
try {
    $summary = & $vulkanInfoExe --summary 2>&1
    if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne $null) {
        Write-Warn "vulkaninfo exited with code $LASTEXITCODE (may still be valid)"
    }

    $summaryText = $summary -join "`n"
    if ($Verbose) { Write-Host $summaryText }

    # Parse device name from summary
    $deviceLine = $summary | Where-Object { $_ -match "GPU id" } | Select-Object -First 1
    if ($deviceLine) {
        Write-Pass "GPU detected: $($deviceLine.Trim())"
    }

    # Check for Vulkan 1.2+
    $apiLine = $summary | Where-Object { $_ -match "apiVersion" } | Select-Object -First 1
    if ($apiLine -match "(\d+\.\d+\.\d+)") {
        $verStr = $matches[1]
        $verParts = $verStr.Split('.')
        $major = [int]$verParts[0]
        $minor = [int]$verParts[1]
        if ($major -gt 1 -or ($major -eq 1 -and $minor -ge 2)) {
            Write-Pass "Vulkan API version: $verStr (>= 1.2 required)"
        } else {
            Write-Fail "Vulkan API version $verStr is too old. Need >= 1.2"
        }
    }
} catch {
    Write-Fail "Failed to run vulkaninfo: $_"
    exit 3
}

# ─────────────────────────────────────────────────────────────────────────────
# 4. Check glslc / glslangValidator for shader compilation
# ─────────────────────────────────────────────────────────────────────────────

Write-Info "Checking shader compilation tools..."

$glslc = $null
foreach ($subdir in @("Bin", "bin")) {
    $candidate = Join-Path $vulkanSdk "$subdir\glslc.exe"
    if (Test-Path $candidate) { $glslc = $candidate; break }
}
if (-not $glslc) {
    $glslc = (Get-Command glslc -ErrorAction SilentlyContinue)?.Source
}

if ($glslc) {
    Write-Pass "glslc found: $glslc"
} else {
    Write-Warn "glslc not found. SPIR-V shader compilation will require manual setup."
}

$glslang = $null
foreach ($subdir in @("Bin", "bin")) {
    $candidate = Join-Path $vulkanSdk "$subdir\glslangValidator.exe"
    if (Test-Path $candidate) { $glslang = $candidate; break }
}
if (-not $glslang) {
    $glslang = (Get-Command glslangValidator -ErrorAction SilentlyContinue)?.Source
}

if ($glslang) {
    Write-Pass "glslangValidator found: $glslang"
} else {
    Write-Warn "glslangValidator not found. Install Vulkan SDK for shader compilation."
}

# ─────────────────────────────────────────────────────────────────────────────
# 5. Check DXGI for GPU memory info (Windows-specific)
# ─────────────────────────────────────────────────────────────────────────────

Write-Info "Checking GPU memory via DXGI..."
try {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public class DXGI {
    [DllImport("dxgi.dll")]
    public static extern int CreateDXGIFactory1(ref Guid riid, out IntPtr ppFactory);

    public static Guid IID_IDXGIFactory1 = new Guid("770aae78-f26f-4dba-a829-253c83d1b387");
}
"@ -ErrorAction Stop

    # This is a simplified check; actual memory query needs COM interop
    Write-Info "DXGI available (full memory query requires COM interop)"
} catch {
    Write-Warn "DXGI type definition failed (non-critical): $_"
}

# WMI GPU query as fallback
try {
    $gpuInfo = Get-WmiObject -Class Win32_VideoController -ErrorAction Stop |
               Select-Object Name, AdapterRAM, DriverVersion |
               Select-Object -First 3
    foreach ($gpu in $gpuInfo) {
        $vramMB = if ($gpu.AdapterRAM) { [math]::Round($gpu.AdapterRAM / 1MB) } else { 0 }
        Write-Pass "GPU: $($gpu.Name) | VRAM: ${vramMB}MB | Driver: $($gpu.DriverVersion)"
        if ($vramMB -lt 2048 -and $vramMB -gt 0) {
            Write-Warn "GPU has < 2 GB VRAM. Renderer will use reduced buffer sizes."
        }
    }
} catch {
    Write-Warn "WMI GPU query failed (non-critical): $_"
}

# ─────────────────────────────────────────────────────────────────────────────
# 6. Check Python availability for the full feature test
# ─────────────────────────────────────────────────────────────────────────────

Write-Info "Checking Python availability for full feature test..."
$python = $null
foreach ($cmd in @("python", "python3", "py")) {
    $found = Get-Command $cmd -ErrorAction SilentlyContinue
    if ($found) { $python = $found.Source; break }
}

if ($python) {
    Write-Pass "Python found: $python"
    Write-Info "Run '.\Scripts\test_vulkan_features.py --verbose' for full feature report."
} else {
    Write-Warn "Python not found. Cannot run full Vulkan feature test script."
    Write-Info "Install Python from https://python.org to enable full feature testing."
}

# ─────────────────────────────────────────────────────────────────────────────
# 7. Test required Vulkan instance extensions (via vulkaninfo JSON)
# ─────────────────────────────────────────────────────────────────────────────

Write-Info "Checking required extensions via vulkaninfo..."
try {
    $jsonRaw = & $vulkanInfoExe --json 2>&1
    $jsonText = ($jsonRaw -join "`n")
    $start = $jsonText.IndexOf('{')
    if ($start -ge 0) { $jsonText = $jsonText.Substring($start) }
    $jsonData = $jsonText | ConvertFrom-Json -ErrorAction Stop

    # Find first device
    $device = $null
    if ($jsonData.devices -and $jsonData.devices.Count -gt 0) {
        $device = $jsonData.devices[0]
        # Prefer discrete GPU
        foreach ($d in $jsonData.devices) {
            if ($d.properties.deviceType -eq 2) { $device = $d; break }
        }
    }

    if ($device) {
        $extNames = @()
        if ($device.extensions -is [array]) {
            $extNames = $device.extensions | ForEach-Object {
                if ($_ -is [string]) { $_ } else { $_.extensionName }
            }
        } elseif ($device.extensions -is [System.Management.Automation.PSCustomObject]) {
            $extNames = $device.extensions.PSObject.Properties.Name
        }

        $required = @(
            "VK_KHR_swapchain",
            "VK_KHR_maintenance1",
            "VK_EXT_descriptor_indexing"
        )

        foreach ($ext in $required) {
            if ($extNames -contains $ext) {
                Write-Pass "Extension: $ext"
            } else {
                Write-Fail "Extension missing: $ext"
            }
        }
    }
} catch {
    Write-Warn "Could not parse vulkaninfo JSON: $_"
}

# ─────────────────────────────────────────────────────────────────────────────
# 8. Generate capability header if requested
# ─────────────────────────────────────────────────────────────────────────────

if ($GenerateHeader) {
    if ($python) {
        $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
        $pyScript = Join-Path $scriptDir "test_vulkan_features.py"
        $outHeader = if ($HeaderOutput) { $HeaderOutput } else {
            Join-Path (Split-Path $scriptDir -Parent) `
                "Ermine-Engine\include\Vulkan\VulkanCapabilities_Generated.h"
        }
        Write-Info "Generating capability header via Python..."
        & $python $pyScript --header-output $outHeader
        if ($LASTEXITCODE -eq 0) {
            Write-Pass "Capability header written: $outHeader"
        } else {
            Write-Warn "Python feature test exited with code $LASTEXITCODE"
        }
    } else {
        Write-Warn "Cannot generate header: Python not available."
    }
}

# ─────────────────────────────────────────────────────────────────────────────
# Summary
# ─────────────────────────────────────────────────────────────────────────────

Write-Host "`n$("=" * 70)"
if ($script:failures.Count -eq 0) {
    Write-Host "  [PASS] All required Vulkan checks passed." -ForegroundColor Green
    if ($script:warnings.Count -gt 0) {
        Write-Host "  [WARN] $($script:warnings.Count) warning(s) - some features will run in reduced mode." -ForegroundColor Yellow
    }
    exit 0
} else {
    Write-Host "  [FAIL] $($script:failures.Count) required check(s) failed:" -ForegroundColor Red
    foreach ($f in $script:failures) {
        Write-Host "         x $f" -ForegroundColor Red
    }
    exit 1
}
