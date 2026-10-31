/* Local-ROM native NVIDIA GOP recovery; no ROM or firmware modification. */
#pragma once
#include "ownership.h"
#include "consumer_release.h"
#if defined(GU_NATIVE_GOP_TEST) && (!defined(GU_HOST_APP_TEST) || defined(GU_FIRMWARE_BUILD))
#error Native GOP host model is forbidden in firmware builds
#endif
VOID GuNativeReset(VOID);
VOID GuNativeFree(VOID);
BOOLEAN GuNativeEnabled(VOID);
EFI_STATUS GuNativePrepare(VOID);
EFI_STATUS GuNativeLoad(EFI_HANDLE *Driver);
EFI_STATUS GuNativeConnect(VOID);
EFI_STATUS GuNativeInspect(GU_RELEASE_OWNER *Owner,R4_GRAPH *Edges,BOOLEAN RequireDisplay);
EFI_STATUS GuNativeInventory(const char *Stage);
