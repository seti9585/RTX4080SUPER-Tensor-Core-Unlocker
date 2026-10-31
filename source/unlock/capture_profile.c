/* SPDX-License-Identifier: GPL-2.0-only */
#include "capture_profile.h"
#include "prom_profile.h"
#include "sha256.h"
#include "runtime.h"
static BOOLEAN Match(const VOID *data,UINTN size,const char *hex){
 static const char digits[]="0123456789abcdef";UINT8 hash[32];char actual[65];
 GuSha256(data,size,hash);
 for(UINTN i=0;i<32;i++){actual[2*i]=digits[hash[i]>>4];actual[2*i+1]=digits[hash[i]&15];}actual[64]=0;
 return !memcmp(actual,hex,65);
}
BOOLEAN GuCapturedPromMatches(const VOID *data,UINTN size){
 return data&&size==GU_PROM_CAPTURE_BYTES&&Match(data,size,GU_PROM_CAPTURE_SHA256)&&
  Match(data,GU_PROM_PREFIX_BYTES,GU_PROM_PREFIX_SHA256)&&Match(data,GU_PROM_VIEW_BYTES,GU_PROM_VIEW_SHA256);
}
BOOLEAN GuCapturedPciMatches(const VOID *data,UINTN size){return data&&size==256&&Match(data,size,GU_CAPTURE_PCI_SHA256);}
