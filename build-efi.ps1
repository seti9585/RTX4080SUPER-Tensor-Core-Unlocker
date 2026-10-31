param([Parameter(Mandatory=$true)][string]$Zig)
$ErrorActionPreference='Stop'
$base=$PSScriptRoot
New-Item -ItemType Directory -Path "$base\build" -Force | Out-Null
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $base 'cache\global'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $base 'cache\local'
$common=@('cc','-std=c11','-ffreestanding','-fno-builtin','-fno-stack-protector','-mno-red-zone','-fshort-wchar','-Os','-g0','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-variable','-D__KERNEL__','-DGU_UEFI_PORT','-DGU_TARGET_PCI_ID=0x270210deU','-DGU_PCI_SNAPSHOT','-DGU_VGA_RESTORE','-Iunlock/compat','-Ideps/edk2/MdePkg/Include','-Ideps/edk2/MdePkg/Include/X64')
$inputs=@('app','app_io','pci_snapshot','native_gop','extract_gop','topology','ownership','consumer_engine','consumer_runtime','capture_profile','format','runtime','sha256','checks','port','compat','vendor/unlock_610','vendor/prep_610','vendor/layout_610','vendor/vbios_610','vendor/gfw_eval')|ForEach-Object {'unlock/'+$_+'.c'}
Push-Location "$base\source"
try{
 & $Zig @common '-DGU_FIRMWARE_BUILD' '-DGU_EXPERIMENTAL_ARM' '-target' 'x86_64-uefi' '-nostdlib' @inputs '-o' "$base\build\DYNAMIC-GOP-CANDIDATE.EFI"
 if($LASTEXITCODE){throw 'EFI build failed'}
}finally{Pop-Location}
