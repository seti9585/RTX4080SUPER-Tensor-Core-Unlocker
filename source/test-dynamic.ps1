param([Parameter(Mandatory=$true)][string]$Zig,[Parameter(Mandatory=$true)][string]$Fixtures,[switch]$PreflightOnly)
$ErrorActionPreference='Stop'
$base=Split-Path $PSScriptRoot
New-Item -ItemType Directory -Path "$base\build" -Force | Out-Null
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path (Split-Path $base) 'phase2-gop\zig-global-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $base 'zig-cache'
$common=@('cc','-std=c11','-ffreestanding','-fno-builtin','-fno-stack-protector','-mno-red-zone','-fshort-wchar','-Os','-g0','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-variable','-D__KERNEL__','-DGU_UEFI_PORT','-DGU_TARGET_PCI_ID=0x270210deU','-DGU_PCI_SNAPSHOT','-DGU_VGA_RESTORE','-Iunlock/compat','-Ideps/edk2/MdePkg/Include','-Ideps/edk2/MdePkg/Include/X64')
$inputs=@('app','app_io','pci_snapshot','native_gop','extract_gop','topology','ownership','consumer_engine','consumer_runtime','capture_profile','format','runtime','sha256','checks','port','compat','vendor/unlock_610','vendor/prep_610','vendor/layout_610','vendor/vbios_610','vendor/gfw_eval')|ForEach-Object {'unlock/'+$_+'.c'}
Push-Location "$base\source"
try{
 & $Zig @common '-O1' '-Iunlock/tests' '-Wno-macro-redefined' '-DGU_HOST_APP_TEST' '-DGU_MEASURED_PROFILE_TEST' '-DGU_NATIVE_GOP_TEST' 'unlock/tests/test_ad103_app.c' 'unlock/tests/Decompress.c' @inputs '-o' "$base\build\test-dynamic.exe" *> "$base\build\host-build.log"
 if($LASTEXITCODE){throw "Host build failed: $base/build/host-build.log"}
 $testArgs=@("$Fixtures\fixtures\PROM-1M.BIN","$Fixtures\assets","$Fixtures\fixtures\GPU-PCI-CONFIG-256-FROM-REPORT.BIN","$Fixtures\experimental-evidence\wpc\UEFI-ROM.BIN","$Fixtures\run0009");if($PreflightOnly){$testArgs+="preflight-only"}
 & "$base\build\test-dynamic.exe" @testArgs *> "$base\build\host-results.txt"
 if($LASTEXITCODE){throw "Host tests failed: $base/build/host-results.txt"}
 & $Zig @common '-DGU_FIRMWARE_BUILD' '-DGU_EXPERIMENTAL_ARM' '-target' 'x86_64-uefi' '-nostdlib' @inputs '-o' "$base\build\DYNAMIC-GOP-CANDIDATE.EFI" *> "$base\build\efi-build.log"
 if($LASTEXITCODE){throw "EFI build failed: $base/build/efi-build.log"}
 Get-Content "$base\build\host-results.txt" -Tail 2
}finally{Pop-Location}
