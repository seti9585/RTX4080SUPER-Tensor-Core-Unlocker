#include <Uefi.h>
#include <Protocol/Decompress.h>
#include "sha256.h"
static CHAR16 *Stage=L"not_started";
CHAR16 *GopExtractStage(VOID){return Stage;}
#define ROM_BYTES 150528U
#define GOP_BYTES 164608U
static UINT16 R16(const UINT8 *p){return (UINT16)(p[0]|((UINT16)p[1]<<8));}
static UINT32 R32(const UINT8 *p){return (UINT32)R16(p)|((UINT32)R16(p+2)<<16);}
static BOOLEAN PeValid(const UINT8 *p){
 if(R16(p)!=0x5a4d)return FALSE;
 UINT32 pe=R32(p+0x3c);
 if(pe>GOP_BYTES-264||R32(p+pe)!=0x4550||R16(p+pe+4)!=0x8664||R16(p+pe+20)<112)return FALSE;
 const UINT8 *o=p+pe+24;
 return R16(o)==0x20b&&R32(o+16)==0x308&&R32(o+56)==0x27ac0&&R16(o+68)==11;
}
/* This module only processes a firmware-owned ROM copy and allocates memory.
 * The caller must retain the verified output until its consumer is finished. */
EFI_STATUS ExtractGop(EFI_BOOT_SERVICES *bs,EFI_DECOMPRESS_PROTOCOL *d,
 const VOID *rom,UINTN bytes,VOID **image){
 UINT8 *copy=NULL,*out=NULL;VOID *scratch=NULL;UINT32 dst=0,tmp=0;
 EFI_STATUS s=EFI_SECURITY_VIOLATION;
 Stage=L"arguments"; if(!image)return EFI_INVALID_PARAMETER;*image=NULL;
 if(!bs||!bs->AllocatePool||!bs->FreePool||!d||!d->GetInfo||!d->Decompress)return EFI_UNSUPPORTED;
 Stage=L"rom_presence_size"; if(!rom||bytes!=ROM_BYTES)return s;
 Stage=L"rom_copy_allocation"; s=bs->AllocatePool(EfiBootServicesData,ROM_BYTES,(VOID**)&copy);
 if(EFI_ERROR(s)||!copy)return EFI_OUT_OF_RESOURCES;
 for(UINTN i=0;i<bytes;i++)copy[i]=((const UINT8*)rom)[i];
 s=EFI_SECURITY_VIOLATION;
 Stage=L"rom_sha256"; if(!GuHashMatches(copy,bytes,"d9bfb45194747db9130d96d9d377de7ba7de2e0687d9feac9e9e424eb0c700d8"))goto done;
 Stage=L"rom_headers"; const UINTN start=0xfc00,payload=0xfc50;
 if(R16(copy+start)!=0xaa55||R32(copy+start+4)!=0xef1||R16(copy+start+8)!=11||
 R16(copy+start+10)!=0x8664||R16(copy+start+12)!=1||R16(copy+start+22)!=0x50)goto done;
 UINTN pcir=start+R16(copy+start+24);
 if(pcir>bytes-24||R32(copy+pcir)!=0x52494350||copy[pcir+20]!=3)goto done;
 UINT32 compressed=R32(copy+payload);
 if(compressed>bytes-payload-8||R32(copy+payload+4)!=GOP_BYTES)goto done;
 /* Include the compression header, exclude trailing ROM padding. */
 UINT32 sourceBytes=compressed+8;
 Stage=L"decompress_get_info"; s=d->GetInfo(d,copy+payload,sourceBytes,&dst,&tmp);if(EFI_ERROR(s))goto done;
 Stage=L"decompress_sizes"; s=EFI_SECURITY_VIOLATION;if(dst!=GOP_BYTES||!tmp||tmp>16U*1024U*1024U)goto done;
 Stage=L"output_allocation"; s=bs->AllocatePool(EfiBootServicesData,dst,(VOID**)&out);
 if(EFI_ERROR(s)||!out){s=EFI_OUT_OF_RESOURCES;goto done;}
 Stage=L"scratch_allocation"; s=bs->AllocatePool(EfiBootServicesData,tmp,&scratch);
 if(EFI_ERROR(s)||!scratch){s=EFI_OUT_OF_RESOURCES;goto done;}
 Stage=L"decompress"; s=d->Decompress(d,copy+payload,sourceBytes,out,dst,scratch,tmp);if(EFI_ERROR(s))goto done;
 s=EFI_SECURITY_VIOLATION;
 Stage=L"gop_sha256_pe"; if(!GuHashMatches(out,dst,"b69af88e4f6238933379922b27a6a2cd392f361df862014c40bea92e79483e80")||!PeValid(out))goto done;
 *image=out;out=NULL;s=EFI_SUCCESS;Stage=L"complete";
 done:
 if(scratch)bs->FreePool(scratch);if(out)bs->FreePool(out);bs->FreePool(copy);return s;
}
