#ifndef GU_CAPTURE_PROFILE_H
#define GU_CAPTURE_PROFILE_H
#include <Uefi.h>
BOOLEAN GuCapturedPromMatches(const VOID*,UINTN);
BOOLEAN GuCapturedPciMatches(const VOID*,UINTN);
#endif
