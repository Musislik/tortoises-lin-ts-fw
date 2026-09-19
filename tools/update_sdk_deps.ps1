<#
.SYNOPSIS
    Script to update/copy necessary parts of MSPM0 SDK to the local project folder.
#>

$SdkDir = "D:\mspm0_sdk_2_10_00_04"
$ProjectDir = Split-Path -Path $PSScriptRoot -Parent

$SdkDestDir = Join-Path $ProjectDir "sdk"
$StartupDestDir = Join-Path $ProjectDir "startup"

function Copy-SdkItem {
    param (
        [string]$Source,
        [string]$Destination
    )
    
    if (Test-Path $Source) {
        $DestParent = Split-Path $Destination -Parent
        if (-not (Test-Path $DestParent)) {
            New-Item -ItemType Directory -Path $DestParent -Force | Out-Null
        }
        
        Write-Host "Copying: $Source -> $Destination"
        if ((Get-Item $Source).PSIsContainer) {
            if (-not (Test-Path $Destination)) {
                New-Item -ItemType Directory -Path $Destination -Force | Out-Null
            }
            Copy-Item -Path "$Source\*" -Destination $Destination -Recurse -Force
        } else {
            Copy-Item -Path $Source -Destination $Destination -Force
        }
    } else {
        Write-Warning "Source path not found: $Source"
    }
}

Write-Host "Starting copying of dependencies from: $SdkDir"

# 1. Driverlib
Copy-SdkItem -Source (Join-Path $SdkDir "source\ti\driverlib") -Destination (Join-Path $SdkDestDir "ti\driverlib")

# 2. Device definitions
Copy-SdkItem -Source (Join-Path $SdkDir "source\ti\devices") -Destination (Join-Path $SdkDestDir "ti\devices")

# 3. CMSIS headers
Copy-SdkItem -Source (Join-Path $SdkDir "source\third_party\CMSIS\Core\Include") -Destination (Join-Path $SdkDestDir "third_party\CMSIS\Core\Include")

# 4. Startup file
$StartupSource = Join-Path $SdkDir "source\ti\devices\msp\m0p\startup_system_files\ticlang\startup_mspm0c110x_ticlang.c"
if (Test-Path $StartupSource) {
    Copy-SdkItem -Source $StartupSource -Destination (Join-Path $StartupDestDir "startup_mspm0c110x_ticlang.c")
} else {
    Write-Warning "Startup file not found."
}

# 5. Linker scripts
$LinkerSource1103 = Join-Path $SdkDir "source\ti\devices\msp\m0p\linker_files\ticlang\mspm0c1103.cmd"
if (Test-Path $LinkerSource1103) {
    Copy-SdkItem -Source $LinkerSource1103 -Destination (Join-Path $StartupDestDir "mspm0c1103.cmd")
} else {
    Write-Warning "Linker script mspm0c1103.cmd not found."
}

$LinkerSource1104 = Join-Path $SdkDir "source\ti\devices\msp\m0p\linker_files\ticlang\mspm0c1104.cmd"
if (Test-Path $LinkerSource1104) {
    Copy-SdkItem -Source $LinkerSource1104 -Destination (Join-Path $StartupDestDir "mspm0c1104.cmd")
} else {
    Write-Warning "Linker script mspm0c1104.cmd not found."
}

Write-Host "Performing cleanup of unnecessary files..."

# Cleanup redundant templates and metadata from local sdk folder to save space
$RedundantLinkerDir = Join-Path $SdkDestDir "ti\devices\msp\m0p\linker_files"
$RedundantStartupDir = Join-Path $SdkDestDir "ti\devices\msp\m0p\startup_system_files"
$RedundantMetaDir = Join-Path $SdkDestDir "ti\driverlib\.meta"

if (Test-Path $RedundantLinkerDir) { Remove-Item -Path $RedundantLinkerDir -Recurse -Force -ErrorAction SilentlyContinue }
if (Test-Path $RedundantStartupDir) { Remove-Item -Path $RedundantStartupDir -Recurse -Force -ErrorAction SilentlyContinue }
if (Test-Path $RedundantMetaDir) { Remove-Item -Path $RedundantMetaDir -Recurse -Force -ErrorAction SilentlyContinue }

# Cleanup unused toolchain precompiled libraries (we only use TI Arm Clang - ticlang)
$LibDir = Join-Path $SdkDestDir "ti\driverlib\lib"
if (Test-Path $LibDir) {
    Remove-Item -Path (Join-Path $LibDir "gcc"), (Join-Path $LibDir "iar"), (Join-Path $LibDir "keil") -Recurse -Force -ErrorAction SilentlyContinue
    
    # Keep only the specific chip family library (mspm0c110x) for ticlang
    $TiclangM0pDir = Join-Path $LibDir "ticlang\m0p"
    if (Test-Path $TiclangM0pDir) {
        Get-ChildItem -Path $TiclangM0pDir -Directory | Where-Object { $_.Name -notmatch 'mspm0c110x' } | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# Cleanup sysctl (keep only mspm0c110x)
$SysctlHwDir = Join-Path $SdkDestDir "ti\devices\msp\peripherals\m0p\sysctl"
if (Test-Path $SysctlHwDir) {
    Get-ChildItem -Path $SysctlHwDir -File | Where-Object { $_.Name -notmatch 'mspm0c110x' } | Remove-Item -Force -ErrorAction SilentlyContinue
}

$SysctlDlDir = Join-Path $SdkDestDir "ti\driverlib\m0p\sysctl"
if (Test-Path $SysctlDlDir) {
    Get-ChildItem -Path $SysctlDlDir -File | Where-Object { $_.Name -notmatch 'mspm0c110x' } | Remove-Item -Force -ErrorAction SilentlyContinue
}

# Cleanup devices headers (keep only mspm0c110x)
$M0pDevicesDir = Join-Path $SdkDestDir "ti\devices\msp\m0p"
if (Test-Path $M0pDevicesDir) {
    Get-ChildItem -Path $M0pDevicesDir -File | Where-Object { $_.Name -notmatch 'mspm0c110x' } | Remove-Item -Force -ErrorAction SilentlyContinue
}

# Cleanup CMSIS headers (keep only core_cm0plus and compiler headers)
$CmsisDir = Join-Path $SdkDestDir "third_party\CMSIS\Core\Include"
if (Test-Path $CmsisDir) {
    Get-ChildItem -Path $CmsisDir -File | Where-Object { 
        $_.Name -notmatch 'core_cm0plus' -and 
        $_.Name -notmatch 'cmsis_compiler' -and 
        $_.Name -notmatch 'cmsis_version' -and 
        $_.Name -notmatch 'cmsis_ticlang' -and
        $_.Name -notmatch 'cmsis_gcc' -and
        $_.Name -notmatch 'cmsis_armclang' -and
        $_.Name -notmatch 'mpu_armv' -and
        $_.Name -notmatch 'cachel1_armv' -and
        $_.Name -notmatch 'tz_context'
    } | Remove-Item -Force -ErrorAction SilentlyContinue
}

Write-Host "Copying finished! Files should now be in the 'sdk' and 'startup' folders."
