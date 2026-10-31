/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef GU_APP_HOST_MODEL_H
#define GU_APP_HOST_MODEL_H
#if !defined(GU_HOST_APP_TEST) || defined(GU_FIRMWARE_BUILD) || !defined(_WIN32_WINNT)
#error Host application model is forbidden in firmware builds
#endif
#include <Uefi.h>
/* Synthetic preconditions only; the firmware entry execution lock stays zero. */
extern char GuHostRomSha256[65];
EFI_STATUS GuHostApplicationRun(EFI_HANDLE,EFI_SYSTEM_TABLE*);
VOID GuHostResetTopology(VOID);
EFI_STATUS GuHostConsumerProfile(UINTN,EFI_GUID*,UINT32*,UINT64*,UINT64*,UINT8*,EFI_MEMORY_TYPE*);
#endif
