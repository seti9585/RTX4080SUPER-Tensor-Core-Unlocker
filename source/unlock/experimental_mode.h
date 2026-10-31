/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef GU_EXPERIMENTAL_MODE_H
#define GU_EXPERIMENTAL_MODE_H
#if defined(GU_EXPERIMENTAL_ARM)
#if defined(GU_HOST_APP_TEST) && defined(GU_FIRMWARE_BUILD)
#error Host application model is forbidden in experimental firmware
#endif
#include "experimental_verified.h"
#if GU_EXPERIMENTAL_EVIDENCE_VERIFIED != 1
#error Experimental evidence verification is required
#endif
#define GU_EXPERIMENTAL_ENABLED 1
#else
#define GU_EXPERIMENTAL_ENABLED 0
#endif
#endif
