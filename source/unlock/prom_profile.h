#ifndef GU_PROM_PROFILE_H
#define GU_PROM_PROFILE_H
/* Live BAR0 capture: Rev2 v0.2.2 RUN0001, 2026-10-28.
 * Distinct measured ranges; full-VBIOS offset mapping is NOT established. */
#define GU_PROM_BAR_OFFSET 0x300000U
#define GU_PROM_CAPTURE_BYTES 0x100000U
#define GU_PROM_CAPTURE_SHA256 "dd5a21fc1434583d3660de35f60799c2cce3b72cd5330dd1d7e5b042a0785f6f"
#define GU_PROM_PREFIX_BYTES 0x9a200U
#define GU_PROM_PREFIX_SHA256 "efe79bd0be7cde01fb565f1667ad807f2a38d82736f42b429d4ceeb96fb6e9a5"
#define GU_PROM_VIEW_BYTES 0xd6c00U
#define GU_PROM_VIEW_SHA256 "525e8559268e3610de1149677c8f7a62f382438dc1354b7e4acd719bf76717f3"
#define GU_CAPTURE_CHIP 0x193000a1U
#define GU_CAPTURE_PCI_SHA256 "cbac9280b9b9a863886bf10853945ff75a162d62d4f7113b2560f618a19f309f"
#include "experimental_mode.h"
/* WPC RUN0004: separate RAM RomImage and measured initial registers. */
#define GU_UEFI_ROM_PROFILE_READY GU_EXPERIMENTAL_ENABLED
#define GU_UEFI_ROM_BYTES 0x24c00U
#define GU_UEFI_ROM_SHA256 "d9bfb45194747db9130d96d9d377de7ba7de2e0687d9feac9e9e424eb0c700d8"
/* Unset initial register values cannot become an approval by reusing a reference. */
#define GU_COLD_SS_PROFILE_READY GU_EXPERIMENTAL_ENABLED
#define GU_COLD_VRAM_PROFILE_READY GU_EXPERIMENTAL_ENABLED
#define GU_COLD_SS0 0x46326143U
#define GU_COLD_SS1 0x00000002U
#define GU_COLD_VRAM 0x00003ff8U
#ifdef GU_MEASURED_PROFILE_TEST
#if !defined(GU_HOST_APP_TEST) || defined(GU_FIRMWARE_BUILD)
#error Measured application model is forbidden in firmware builds
#endif
#undef GU_UEFI_ROM_PROFILE_READY
#undef GU_COLD_SS_PROFILE_READY
#undef GU_COLD_VRAM_PROFILE_READY
#define GU_UEFI_ROM_PROFILE_READY 1
#define GU_COLD_SS_PROFILE_READY 1
#define GU_COLD_VRAM_PROFILE_READY 1
#endif
#if defined(GU_HOST_APP_TEST) && !defined(GU_MEASURED_PROFILE_TEST)
#include "tests/app_host_model.h"
#undef GU_UEFI_ROM_PROFILE_READY
#undef GU_UEFI_ROM_SHA256
#undef GU_COLD_SS_PROFILE_READY
#undef GU_COLD_VRAM_PROFILE_READY
#undef GU_COLD_SS0
#undef GU_COLD_SS1
#undef GU_COLD_VRAM
#define GU_UEFI_ROM_PROFILE_READY 1
#define GU_UEFI_ROM_SHA256 GuHostRomSha256
#define GU_COLD_SS_PROFILE_READY 1
#define GU_COLD_VRAM_PROFILE_READY 1
#define GU_COLD_SS0 0x31322012U
#define GU_COLD_SS1 7U
#define GU_COLD_VRAM 0x4000U
#endif
#endif

