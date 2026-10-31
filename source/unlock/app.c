/* RTX 4080 SUPER AD103 16GB Rev4 integration; upstream port history: PORT-NOTES.txt. */
#include "app.h"
#include "rev3_policy.h"
#include "topology.h"
#include "control.h"
#include "app_io.h"
#include "compat/compat.h"
#include "checks.h"
#include "sha256.h"
#include "prom_profile.h"
#include "capture_profile.h"
#include "ownership.h"
#include "consumer_release.h"
#include "native_gop.h"
#include "pci_snapshot.h"
#include "vendor/unlock_610.h"
#include "vendor/gfw_eval.h"
#include <Protocol/GraphicsOutput.h>
#include <IndustryStandard/Acpi.h>
#include <Guid/GlobalVariable.h>
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID,GopGuid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,
  PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID,GlobalGuid=EFI_GLOBAL_VARIABLE;
static EFI_SYSTEM_TABLE *Table;
static UINT32 InitialConfig[64];
/* BAR0 base as assigned by platform firmware on this boot. Locked on first
   ConfigCheck(Save=TRUE); every later check and the restore path must match. */
static UINT32 InitialBar0;
static const char *Stage;
static BOOLEAN ExpectDetached;
static GU_RELEASE_OWNER PendingOwner;
static EFI_STATUS Step(EFI_STATUS Status) {
  GuLog("CHECK stage=%s status=%#llx\n",Stage,(unsigned long long)Status);
  return !GuLogReady() && !EFI_ERROR(Status)?EFI_DEVICE_ERROR:Status;
}
static VOID ConfigLog(const UINT32 *Config) {
  for(UINTN i=0;i<64;i+=4)GuLog("config[%02x]=%08x %08x %08x %08x\n",
    (unsigned)(i*4),Config[i],Config[i+1],Config[i+2],Config[i+3]);
}
static BOOLEAN DisplayCheck(VOID) {
  EFI_HANDLE *handles=NULL;UINTN count=0,physical=0,matched=0;
  EFI_STATUS status=Gu.bs->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&count,&handles);
  if(status==EFI_NOT_FOUND && ExpectDetached)return GuLogReady();
  if(EFI_ERROR(status) || count>128 || (count && !handles)){if(handles)GuFree(handles);return FALSE;}
  for(UINTN i=0;i<count;i++) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *g=NULL;EFI_DEVICE_PATH_PROTOCOL *path=NULL;
    status=Gu.bs->HandleProtocol(handles[i],&GopGuid,(VOID **)&g);
    if(EFI_ERROR(status) || !g || !g->Mode){status=EFI_COMPROMISED_DATA;break;}
    if(!g->Mode->FrameBufferBase && !g->Mode->FrameBufferSize)continue;
    physical++;
    status=Gu.bs->HandleProtocol(handles[i],&PathGuid,(VOID **)&path);
    if(EFI_ERROR(status) || !path){status=EFI_UNSUPPORTED;break;}
    EFI_HANDLE parent=NULL;status=Gu.bs->LocateDevicePath(&PciGuid,&path,&parent);
    if(EFI_ERROR(status) || parent!=Gu.handle){status=EFI_UNSUPPORTED;break;}
    if(!g->Mode->Info || g->Mode->SizeOfInfo<sizeof(*g->Mode->Info) ||
       !g->Mode->FrameBufferSize || !g->Mode->Info->VerticalResolution ||
       !g->Mode->Info->HorizontalResolution || g->Mode->Info->PixelFormat>PixelBlueGreenRedReserved8BitPerColor ||
       g->Mode->Info->PixelsPerScanLine<g->Mode->Info->HorizontalResolution ||
       (UINT64)g->Mode->Info->PixelsPerScanLine*g->Mode->Info->VerticalResolution>g->Mode->FrameBufferSize/4) {
      status=EFI_COMPROMISED_DATA;break;
    }
    matched++;
  }
  GuFree(handles);
  GuLog("GOP physical=%zu target=%zu expect_detached=%u status=%#llx\n",(size_t)physical,(size_t)matched,
    ExpectDetached,(unsigned long long)status);
  return !EFI_ERROR(status) && physical==matched && matched==(ExpectDetached?0U:1U) && GuLogReady();
}
#include "handoff.inc"
static EFI_STATUS ReadConfig(UINT32 *Config) {
  for(UINTN i=0;i<64;i++)if(EFI_ERROR(GuConfigRead(EfiPciIoWidthUint32,(UINT32)i*4,&Config[i]))) {
    GuLog("PCI config read failed: offset=%#x valid_bytes=%u; snapshot incomplete\n",(unsigned)(i*4),(unsigned)(i*4));
    return EFI_DEVICE_ERROR;
  }
  return EFI_SUCCESS;
}
static EFI_STATUS ConfigCheck(BOOLEAN Save) {
  UINT32 config[64];GU_CAPS caps;UINTN segment,bus,device,function;
  EFI_STATUS status=Gu.pci->GetLocation(Gu.pci,&segment,&bus,&device,&function);
  GuLog("target location: status=%#llx\n",(unsigned long long)status);
  if(EFI_ERROR(status))return status;
  status=ReadConfig(config);if(EFI_ERROR(status))return status;
  BOOLEAN checked=GuCheckConfig(config,segment,bus,device,function,&caps);
  /* BAR0 base is assigned by platform firmware, not a GPU identity constant.
     Accept any valid 32-bit memory BAR; the exact base is validated against the
     UEFI resource descriptor in BarCheck(), and locked once via InitialBar0. */
  UINT32 bar0=config[0x10/4];
  BOOLEAN bar_ok=(bar0!=0 && bar0!=0xffffffffU && !(bar0&0xfu) &&
                  ((bar0&0x6u)==0x0u || (bar0&0x6u)==0x4u));
  BOOLEAN valid=checked && config[2]==0x030000a1 && config[0xc8/4]==0x00050011 &&
    bar_ok && (InitialBar0==0 || bar0==InitialBar0) &&
    !(config[(caps.pcie+8)/4]&0x00200000U);
  UINT32 expected[64];memcpy(expected,InitialConfig,sizeof(expected));
  if(ExpectDetached)expected[1]&=~4U;
  BOOLEAN same=Save || !memcmp(config,expected,sizeof(config));
  if(ExpectDetached && (config[1]&4))return EFI_ACCESS_DENIED;
  if(Save || !valid || !same)ConfigLog(config);
  GuLog("target BDF=%zu:%zu:%zu.%zu checks=%u pm=%#x pcie=%#x msi=%#x msix=%#x\n",
    (size_t)segment,(size_t)bus,(size_t)device,(size_t)function,checked,
    (unsigned)caps.pm,(unsigned)caps.pcie,(unsigned)caps.msi,(unsigned)caps.msix);
  if(!valid)return EFI_UNSUPPORTED;
  if(!same)return EFI_NOT_READY;
  if(Save){memcpy(InitialConfig,config,sizeof(config));InitialBar0=config[0x10/4];}
  Gu.pm_cap=caps.pm;Gu.pcie_cap=caps.pcie;Gu.pcie_version=caps.pcie_version;Gu.msi_cap=caps.msi;Gu.msix_cap=caps.msix;
  return EFI_SUCCESS;
}
static EFI_STATUS RestoreDecode(VOID) {
  UINT32 current[64],normalized[64],after[64],expected[64];
  memcpy(expected,InitialConfig,sizeof(expected));expected[1]&=~4U;UINTN segment,bus,device,function;
  if(!Gu.pci || !Gu.owned || !Gu.armed || !Gu.display_ok || Gu.started || Gu.allocations ||
    Gu.dma_count || Gu.fault || Gu.retained)return EFI_ACCESS_DENIED;
  EFI_STATUS status=Gu.pci->GetLocation(Gu.pci,&segment,&bus,&device,&function);
  if(EFI_ERROR(status))return status;
  status=ReadConfig(current);if(EFI_ERROR(status))return status;
  GuLogText("config before decode restore (256 valid bytes):\n");ConfigLog(current);
  if(!GuTargetConfig(current,segment,bus,device,function))return EFI_UNSUPPORTED;
  UINT32 missing=InitialConfig[1]&~current[1]&3U;
  memcpy(normalized,current,sizeof(normalized));normalized[1]|=missing;
  /* Restore only cleared, previously enabled IO/MEM bits. Every other bit,
     including BME, status, BAR, ROM and PM/capability state must be unchanged. */
  if(memcmp(normalized,expected,sizeof(normalized))) {
    GuLogText("decode restore refused: config differs beyond cleared original IO/MEM bits\n");
    return EFI_UNSUPPORTED;
  }
  if(!missing){GuLogText("decode restore not needed\n");return EFI_SUCCESS;}
  if(!Gu.pci->Attributes)return EFI_UNSUPPORTED;
  UINT64 needed=(missing&1?EFI_PCI_IO_ATTRIBUTE_IO:0)|(missing&2?EFI_PCI_IO_ATTRIBUTE_MEMORY:0);
  UINT64 supported=0,attributes=0;
  status=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationSupported,0,&supported);
  GuLog("decode supported attributes: status=%#llx mask=%#llx\n",(unsigned long long)status,(unsigned long long)supported);
  if(EFI_ERROR(status))return status;
  if((supported&needed)!=needed)return EFI_UNSUPPORTED;
  status=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationGet,0,&attributes);
  GuLog("decode current attributes: status=%#llx mask=%#llx\n",(unsigned long long)status,(unsigned long long)attributes);
  if(EFI_ERROR(status))return status;
  GuLog("decode restore intent: saved_command=%#x current_command=%#x missing=%#x attributes=%#llx\n",
    InitialConfig[1]&0xffff,current[1]&0xffff,missing,(unsigned long long)needed);
  if(!GuLogReady() || Gu.fault)return EFI_DEVICE_ERROR;
  /* Never use EFI_PCI_DEVICE_ENABLE: it would also enable bus mastering. */
  EFI_STATUS enabled=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationEnable,needed,NULL);
  GuLog("decode enable status=%#llx\n",(unsigned long long)enabled);
  if(!GuLogReady() || Gu.fault)return EFI_ERROR(enabled)?enabled:EFI_DEVICE_ERROR;
  /* Even an error may follow a partial hardware effect. Capture it if readable,
     but never turn an API error into success merely because readback matches. */
  status=ReadConfig(after);
  BOOLEAN matched=FALSE;
  if(!EFI_ERROR(status)) {
    GuLogText("config after decode restore (256 valid bytes):\n");ConfigLog(after);
    matched=!memcmp(after,expected,sizeof(after));GuLog("decode readback matched=%u\n",matched);
  }
  if(EFI_ERROR(enabled))return enabled;
  if(EFI_ERROR(status))return status;
  if(!GuLogReady() || Gu.fault)return EFI_DEVICE_ERROR;
  return matched?EFI_SUCCESS:EFI_COMPROMISED_DATA;
}
static EFI_STATUS FindTarget(VOID) {
  EFI_HANDLE *handles=NULL;UINTN count=0,found=0;
  EFI_STATUS status=Gu.bs->LocateHandleBuffer(ByProtocol,&PciGuid,NULL,&count,&handles);
  GuLog("PCI enumeration: status=%#llx handles=%zu\n",(unsigned long long)status,(size_t)count);
  if(EFI_ERROR(status) || !handles || !count || count>256){if(handles)GuFree(handles);return EFI_NOT_FOUND;}
  for(UINTN i=0;i<count;i++) {
    EFI_PCI_IO_PROTOCOL *pci=NULL;UINT32 id=0;
    status=Gu.bs->HandleProtocol(handles[i],&PciGuid,(VOID **)&pci);
    if(!EFI_ERROR(status) && pci)status=pci->Pci.Read(pci,EfiPciIoWidthUint32,0,1,&id);
    if(EFI_ERROR(status) || !pci){GuLog("PCI scan failed: index=%zu status=%#llx\n",(size_t)i,(unsigned long long)status);status=EFI_DEVICE_ERROR;break;}
    if(id==GU_BOARD_ID){found++;Gu.handle=handles[i];Gu.pci=pci;}
  }
  GuFree(handles);GuLog("PCI target matches=%zu status=%#llx\n",(size_t)found,(unsigned long long)status);
  if(EFI_ERROR(status) || found!=1)return EFI_UNSUPPORTED;
  return ConfigCheck(TRUE);
}
static EFI_STATUS BarCheck(VOID) {
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *r=NULL;
  EFI_STATUS status=Gu.pci->GetBarAttributes(Gu.pci,0,NULL,(VOID **)&r);
  GuLog("bar0 query: status=%#llx\n",(unsigned long long)status);
  if(!EFI_ERROR(status)) {
    if(r && r->Desc==ACPI_ADDRESS_SPACE_DESCRIPTOR && r->Len==sizeof(*r)-3)
      GuLog("bar0 descriptor: type=%u granularity=%llu min=%#llx max=%#llx length=%#llx translation=%#llx\n",
        (unsigned)r->ResType,(unsigned long long)r->AddrSpaceGranularity,(unsigned long long)r->AddrRangeMin,
        (unsigned long long)r->AddrRangeMax,(unsigned long long)r->AddrLen,(unsigned long long)r->AddrTranslationOffset);
    /* BAR0 base must match the value locked from PCI config on first check;
       when running before that lock, accept any valid memory BAR base. */
    if(!r || r->Desc!=ACPI_ADDRESS_SPACE_DESCRIPTOR || r->Len!=sizeof(*r)-3 || r->ResType!=0 ||
      (InitialBar0!=0 && r->AddrRangeMin!=InitialBar0) ||
      r->AddrLen!=0x1000000 || r->AddrTranslationOffset ||
      /* UEFI describes an inclusive end; pinned EDK2 PciBusDxe instead returns
         PciBar.Alignment. Both representations describe this exact 16 MiB BAR. */
      !(r->AddrRangeMax==0x00ffffffULL ||
        (InitialBar0!=0 && r->AddrRangeMax==(UINT64)InitialBar0+0xffffffULL)) ||
      r->AddrSpaceGranularity!=32 || ((UINT8 *)(r+1))[0]!=ACPI_END_TAG_DESCRIPTOR)status=EFI_UNSUPPORTED;
    else {
      UINT8 *raw=(UINT8 *)r,sum=0;
      for(UINTN i=0;i<sizeof(*r)+2;i++)sum=(UINT8)(sum+raw[i]);
      if(raw[sizeof(*r)+1] && sum)status=EFI_COMPROMISED_DATA;
      else {Gu.bar_base=r->AddrRangeMin;Gu.bar_size=r->AddrLen;}
    }
  }
  if(r)GuFree(r);return status;
}
/* Selectors are boot state, not board identity. Pin only within this run. */
static BOOLEAN ColdSelectorsRecorded;
static UINT32 ColdSs0,ColdSs1;
static BOOLEAN ColdSelectorReadable(UINT32 Value) {
  return Value!=0xffffffffU && (Value&0xffff0000U)!=0xbadf0000U;
}
static EFI_STATUS ColdSelectorsCheck(const char *Phase) {
  UINT32 a=GuRead32(GU_FEAT_SS0_OFFSET),b=GuRead32(GU_FEAT_SS1_OFFSET);
  UINT32 c=GuRead32(GU_FEAT_SS0_OFFSET),d=GuRead32(GU_FEAT_SS1_OFFSET);
  const char *reason=Gu.fault?"READ_ERROR":
    !ColdSelectorReadable(a)||!ColdSelectorReadable(b)||!ColdSelectorReadable(c)||!ColdSelectorReadable(d)?"INVALID_SENTINEL":
    a!=c||b!=d?"UNSTABLE_REPEAT":
    ColdSelectorsRecorded&&(a!=ColdSs0||b!=ColdSs1)?"CHANGED_WITHIN_BOOT":"PASS";
  GuLog("cold selectors phase=%s reason=%s SS0=%#x/%#x SS1=%#x/%#x baseline=%#x/%#x recorded=%u\n",
    Phase,reason,a,c,b,d,ColdSs0,ColdSs1,ColdSelectorsRecorded);
  if(strcmp(reason,"PASS"))return Gu.fault?EFI_DEVICE_ERROR:EFI_NOT_READY;
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  ColdSs0=a;ColdSs1=b;ColdSelectorsRecorded=TRUE;return EFI_SUCCESS;
}
static EFI_STATUS ColdCheck(VOID) {
  if(!Rev3HardwareProfileReady||!GU_COLD_SS_PROFILE_READY||!GU_COLD_VRAM_PROFILE_READY)return EFI_NOT_READY;
  UINT32 chip=GuRead32(0);GuLog("chip: actual=%#x expected=%#x\n",chip,GU_BOARD_CHIP);
  if(chip!=GU_BOARD_CHIP)return EFI_UNSUPPORTED;
  struct gpu_unlock_gfw_sample s={.config_ok=true,.plm=GuRead32(GU_GFW_PLM_OFFSET),
    .boot=GuRead32(GU_GFW_BOOT_OFFSET),.cpuctl=GuRead32(GU_GFW_CPUCTL_OFFSET)};
  UINT32 hi=GuRead32(0x1fa828),plm=GuRead32(GU_FEAT_PLM_OFFSET),ss0=GuRead32(GU_FEAT_SS0_OFFSET),ss1=GuRead32(GU_FEAT_SS1_OFFSET);
  GuLog("cold state: chip=%#x plm=%#x gfw=%#x/%#x/%#x WPR2_HI=%#x SS0=%#x SS1=%#x\n",
    GU_BOARD_CHIP,plm,s.plm,s.boot,s.cpuctl,hi,ss0,ss1);
  UINT32 wpr_cfg=GuRead32(0x1fa7cc),fbpa=GuRead32(0x9a0148),wpr=GuRead32(0x1fa7c4);
  GuLog("cold PLMs: WPR_CFG=%#x FBPA=%#x WPR=%#x FEAT=%#x\n",wpr_cfg,fbpa,wpr,plm);
  UINT32 vram=GuRead32(0x1183a4);
  GuLog("VRAM register=%#x (%u MiB); required=%#x\n",vram,vram,GU_COLD_VRAM);
  const char *reason=Gu.fault?"READ_ERROR":gpu_unlock_gfw_assess(&s)?"GFW":
    hi>>4?"WPR2_HI":plm!=0xffffff8f?"FEAT_PLM":vram!=GU_COLD_VRAM?"VRAM":"PASS";
  GuLog("cold independent reason=%s\n",reason);
  if(strcmp(reason,"PASS"))return Gu.fault?EFI_DEVICE_ERROR:EFI_NOT_READY;
  EFI_STATUS status=ColdSelectorsCheck("cold-repeat");
  if(!EFI_ERROR(status)&&(ss0!=ColdSs0||ss1!=ColdSs1)) {
    GuLogText("cold selectors reason=UNSTABLE_FIRST_SAMPLE\n");return EFI_NOT_READY;
  }
  return status;
}
static VOID LogHash(const char *Name,const VOID *Data,UINTN Bytes) {
  static const char digits[]="0123456789abcdef";
  UINT8 digest[32];char hex[65];GuSha256(Data,Bytes,digest);
  for(UINTN i=0;i<32;i++){hex[2*i]=digits[digest[i]>>4];hex[2*i+1]=digits[digest[i]&15];}
  hex[64]=0;GuLog("%s bytes=%zu SHA256=%s\n",Name,(size_t)Bytes,hex);
}
static EFI_STATUS PromCheck(VOID) {
  if(!Rev3HardwareProfileReady)return EFI_NOT_READY;
  if(!GU_UEFI_ROM_PROFILE_READY||sizeof(GU_UEFI_ROM_SHA256)!=65)return EFI_NOT_READY;
  if(!Gu.pci->RomImage || Gu.pci->RomSize!=GU_UEFI_ROM_BYTES)return EFI_CRC_ERROR;
  if(!GuHashMatches(Gu.pci->RomImage,GU_UEFI_ROM_BYTES,GU_UEFI_ROM_SHA256))
    return EFI_CRC_ERROR;
  VOID *prom=GuAlloc(1048576);if(!prom)return EFI_OUT_OF_RESOURCES;
  GuCopyFromIo(prom,(VOID *)(UINTN)(Gu.bar_base+GU_PROM_BAR_OFFSET),1048576);
  BOOLEAN good=!Gu.fault && GuCapturedPromMatches(prom,GU_PROM_CAPTURE_BYTES);
  GuLog("PCI RomImage hash matched=1; PROM read width=32 io_fault=%u\n",(unsigned)Gu.fault);
  LogHash("live PROM",prom,1048576);
  LogHash("PROM verified view [0,0xd6c00)",prom,GU_PROM_VIEW_BYTES);
  GuLog("live PROM view hash matched=%u bar_offset=%#x bytes=%#x expected=%s\n",
    (unsigned)good,GU_PROM_BAR_OFFSET,GU_PROM_VIEW_BYTES,GU_PROM_VIEW_SHA256);
  if(!good) {
    UINT32 *p=prom;
    GuLog("PROM prefix: %08x %08x %08x %08x %08x %08x %08x %08x\n",p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7]);
    LogHash("PROM first 0x24c00 bytes; not UEFI RomImage",prom,0x24c00);
    EFI_STATUS saved=GuSavePromDump(prom,1048576);
    GuLog("PROM.BIN save status=%#llx bytes=1048576 complete_read=%u; mismatch remains blocking\n",
      (unsigned long long)saved,(unsigned)!Gu.fault);
  }
  GuFree(prom);
  if(!good)return Gu.fault?EFI_DEVICE_ERROR:EFI_CRC_ERROR;
  return ColdSelectorsCheck("post-PROM");
}
static EFI_STATUS Arm(VOID) {
  if(!Table->ConIn || !Table->ConIn->Reset || !Table->ConIn->ReadKeyStroke)return EFI_UNSUPPORTED;
  EFI_STATUS status=Table->ConIn->Reset(Table->ConIn,FALSE);if(EFI_ERROR(status))return status;
  GuLogText("READY: RTX 4080 SUPER AD103 / 10de:2702 / Windows preloaded.\n");
  if(PendingOwner.agent)GuLogText("U will release the RTX firmware driver, restore verified access settings if needed, then attempt WPR_CFG -> FBPA -> WPR -> FEAT in ONE SEC2 launch, then SS0/SS1 if required gates pass; log mismatches and recover.\n");
  GuLogText("Press U once. One SEC2 fire containing four ordered PLM writes, then selectors/FLR/Windows on full success. The last frame may remain visible. Power OFF on STOP. Esc/30s cancels.\n");
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  for(UINTN i=0;i<300;i++) {
    EFI_INPUT_KEY key;status=Table->ConIn->ReadKeyStroke(Table->ConIn,&key);
    if(!EFI_ERROR(status)) {
      if(key.ScanCode==0x17 || key.UnicodeChar==27)return EFI_ABORTED;
      if(key.UnicodeChar=='u' || key.UnicodeChar=='U')return EFI_SUCCESS;
    } else if(status!=EFI_NOT_READY)return status;
    GuStall(100000);
  }
  return EFI_TIMEOUT;
}
static VOID PausePreflightFailure(VOID) {
  if(GuScreenDetached || Gu.started || !Table->ConIn || !Table->ConIn->ReadKeyStroke)return;
  if(Table->ConIn->Reset)Table->ConIn->Reset(Table->ConIn,FALSE);
  GuConsole("Preflight stopped before unlock. See the FINAL stage above and REPORT.TXT.\n");
  GuConsole("Press Enter or Esc to return to firmware; automatic return in 60 seconds. U cannot start an attempt here.\n");
  for(UINTN i=0;i<600;i++) {
    EFI_INPUT_KEY key;EFI_STATUS s=Table->ConIn->ReadKeyStroke(Table->ConIn,&key);
    if(!EFI_ERROR(s) && (key.ScanCode==0x17 || key.UnicodeChar==27 || key.UnicodeChar=='\r'))break;
    if(EFI_ERROR(s) && s!=EFI_NOT_READY)break;
    Gu.bs->Stall(100000);
  }
}
static VOID Halt(VOID) {
  Gu.retained=TRUE;
  GuLogText("STOP: cleanup cannot be proven. Resources remain reserved.\nPOWER OFF using the physical power button; do not continue to Windows.\n");
  /* No ResetSystem, ExitBootServices, retry, protocol close or memory release. */
  for(;;)Gu.bs->Stall(1000000);
}
static VOID LogContainment(VOID) {
  /* Called only after any TPL_NOTIFY containment section has ended. */
  GuLog("BME containment: attribute_status=%#llx hardware_status=%#llx command=%#x cached=%#llx pending=%u\n",
    (unsigned long long)Gu.master_attribute_status,(unsigned long long)Gu.master_hardware_status,
    Gu.master_last_command,(unsigned long long)Gu.master_last_attributes,Gu.master_attributes_pending);
}
static VOID ReleaseHalt(EFI_STATUS Status,BOOLEAN Attempted,BOOLEAN ParentAttempted) {
  /* Console release does not grant PCI ownership. Never clear BME through the
   * still-owned GPU merely because a consumer detach failed. */
  if(!ParentAttempted){
    GuLog("FINAL status=%#llx stage=%s consumer_release=1 parent_attempted=0 clean=0\n",
      (unsigned long long)Status,Stage);Halt();return;
  }
  /* A failed claim may follow interface removal/replacement during logging.
     Resolve, validate and clear BME without a callback window or stale access. */
  EFI_TPL old=Gu.bs->RaiseTPL(TPL_NOTIFY);
  EFI_STATUS refreshed=GuRefreshTargetPci();
  EFI_STATUS contained=EFI_ERROR(refreshed)?refreshed:GuClearMaster();
  Gu.bs->RestoreTPL(old);
  LogContainment();
  GuLog("FINAL status=%#llx stage=%s attempted=%u driver_release=1 clean=0 containment=%#llx pci_available=%u\n",
    (unsigned long long)Status,Stage,Attempted,(unsigned long long)contained,(unsigned)(Gu.pci!=NULL));
  Halt();
}

typedef struct {EFI_HANDLE image;EFI_SYSTEM_TABLE *table;BOOLEAN attempted,released,parent_attempted;} R3_CONTEXT;
static EFI_STATUS ProductAction(VOID *opaque,R3_STAGE phase) {
 R3_CONTEXT *ctx=opaque;EFI_STATUS status=EFI_SUCCESS;
 static const char *names[]={"identify","observe","boot-discover","topology","profile","assets","boot-load","consumer-plan","confirm","recheck","consumer-release","disconnect","refresh","exclusive","post-claim","decode","post-decode","transaction","restore","start"};
 Stage=names[phase];GuLog("stage=%s\n",Stage);if(!GuLogReady())return EFI_DEVICE_ERROR;
 switch(phase){
 case R3_IDENTIFY:return FindTarget();
 case R3_OBSERVE:
  Gu.display_ok=DisplayCheck();if(!Gu.display_ok)return EFI_UNSUPPORTED;
  status=GuFindReleaseOwner(ctx->table,&PendingOwner,TRUE);
  return EFI_ERROR(status)?status:GuRecordOwnershipEvidence(ctx->table,&PendingOwner);
 case R3_BOOT_DISCOVER:return DiscoverWindows();
 case R3_TOPOLOGY:return Rev3TopologyGate(Gu.handle,WindowsVolume,ctx->image);
 case R3_PROFILE:
  status=ConfigCheck(FALSE);if(!EFI_ERROR(status))status=BarCheck();
  if(!EFI_ERROR(status))status=ColdCheck();if(!EFI_ERROR(status))status=PromCheck();return status;
 case R3_ASSETS:
  status=GuLoadAssets();if(!EFI_ERROR(status))status=GuNativePrepare();return status;
 case R3_BOOT_LOAD:return LoadDiscoveredWindows();
 case R4_CONSUMER_PLAN:return GuPrepareConsumerRelease(ctx->table,&PendingOwner,WindowsVolume);
 case R3_CONFIRM:
  status=Gu.bs->SetWatchdogTimer(0,0,0,NULL);return EFI_ERROR(status)?status:Arm();
 case R3_RECHECK: {
  GU_RELEASE_OWNER current;
  status=ConfigCheck(FALSE);if(!EFI_ERROR(status))status=BarCheck();
  if(!EFI_ERROR(status))status=ColdCheck();if(!EFI_ERROR(status))status=PromCheck();
  if(!EFI_ERROR(status))status=Rev3TopologyGate(Gu.handle,WindowsVolume,ctx->image);
  if(!EFI_ERROR(status))status=GuFindReleaseOwner(ctx->table,&current,FALSE);
  if(!EFI_ERROR(status)&&!GuSameReleaseOwner(&PendingOwner,&current))status=EFI_ABORTED;
  return status;
 }
 case R4_CONSUMER_RELEASE: {
  BOOLEAN attempted=FALSE;GuScreenDetached=TRUE;
  status=GuReleaseConsumers(&attempted);
  if(attempted)ctx->released=TRUE;else GuScreenDetached=FALSE;
  return status;
 }
 case R3_DISCONNECT: {
  GU_RELEASE_OWNER fresh;
  status=ConfigCheck(FALSE);if(!EFI_ERROR(status))status=BarCheck();
  if(!EFI_ERROR(status))status=ColdCheck();if(!EFI_ERROR(status))status=PromCheck();
  if(!EFI_ERROR(status))status=GuConsumerParentReady(ctx->table,&fresh);
  if(EFI_ERROR(status))return status;PendingOwner=fresh;
  ctx->released=TRUE;ctx->parent_attempted=TRUE;Gu.armed=TRUE;GuScreenDetached=TRUE;ExpectDetached=TRUE;
  return Gu.bs->DisconnectController(Gu.handle,PendingOwner.agent,NULL);
 }
 case R3_REFRESH: {
  EFI_PCI_IO_PROTOCOL *prior=Gu.pci;Gu.pci=NULL;status=GuRefreshTargetPci();
  return !EFI_ERROR(status)&&Gu.pci!=prior?EFI_COMPROMISED_DATA:status;
 }
 case R3_CLAIM:return GuClaimController();
 case R3_POST_CLAIM:
  Gu.display_ok=DisplayCheck();return Gu.display_ok?EFI_SUCCESS:EFI_UNSUPPORTED;
 case R3_DECODE:return RestoreDecode();
 case R3_POST_DECODE:
  status=ConfigCheck(FALSE);if(!EFI_ERROR(status))status=BarCheck();
  if(!EFI_ERROR(status))status=ColdCheck();if(!EFI_ERROR(status))status=PromCheck();return status;
 case R3_TRANSACTION: {
  ctx->attempted=TRUE;Gu.armed=TRUE;
  UINT64 mask=0xffffffffULL;
  struct pci_dev device={.dev={.dma_mask=&mask,.coherent_dma_mask=0xffffffffULL},.pm_cap=Gu.pm_cap,.refs=1};
  struct gpu_unlock_610_shared shared={0};
  int result=gpu_unlock_610_load_shared(&device,&shared);
  if(!result)result=gpu_unlock_610_unlock(&device,&shared);
  EFI_STATUS contained=GuClearMaster();LogContainment();
  if(EFI_ERROR(contained)||!GuSafeToReturn()||Gu.allocations||(Gu.started&&result))Halt();
  gpu_unlock_610_release_shared(&shared);Gu.armed=FALSE;
  return result?EFI_DEVICE_ERROR:EFI_SUCCESS;
 }
 case R3_RESTORE:
  status=RestoreDisplayForWindows(PendingOwner.agent);
  if(!EFI_ERROR(status)) {
    GuLogText("stage=before_restored_ownership_check\n");
    if(!GuLogReady())return EFI_DEVICE_ERROR;
    status=GuVerifyConsumersRestored(ctx->table);
    GuLog("stage=after_restored_ownership_check status=%#llx\n",(unsigned long long)status);
    if(!GuLogReady())status=EFI_DEVICE_ERROR;
  }
#ifdef GU_VGA_RESTORE
  if(!EFI_ERROR(status))status=GuRestoreVga(FALSE);
#endif
  GuScreenDetached=EFI_ERROR(status);
  return status;
 case R3_START: {
  if(GuNativeEnabled()) {
    if(GuScreenDetached||Gu.owned||Gu.fault||!Gu.quiesced||Gu.allocations||Gu.dma_count||!GuSafeToReturn())return EFI_NOT_READY;
    status=GuRefreshTargetPci();if(EFI_ERROR(status))return status;
    status=VerifyRestoredPci("BEFORE_START");if(EFI_ERROR(status))return status;
  }
#ifdef GU_VGA_RESTORE
  status=GuRestoreVga(TRUE);if(EFI_ERROR(status))return status;
#endif
  status=VerifySelectors("BEFORE-START");if(EFI_ERROR(status))return status;
  GuSnapshotTake(7);(void)GuSnapshotFlush(FALSE);
  GuLog("stage=before_Windows_StartImage image=%#llx gpu=%#llx detached=%u owned=%u\n",
    (unsigned long long)(UINTN)WindowsImage,(unsigned long long)(UINTN)Gu.handle,GuScreenDetached,Gu.owned);
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  status=GuIoClose();if(EFI_ERROR(status))return status;
  GuReleaseAssets();UINTN bytes=0;CHAR16 *data=NULL;
  status=Gu.bs->StartImage(WindowsImage,&bytes,&data);
  GuScreenDetached=TRUE;
  if(!EFI_ERROR(GuIoReopenReport()))
    GuLog("stage=after_Windows_StartImage status=%#llx returned=1\n",(unsigned long long)status);
  /* Never return to firmware after a hardware attempt, even if Windows returns. */
  (void)status;(void)data;GuScreenDetached=TRUE;Halt();return EFI_ABORTED;
 }
 default:return EFI_UNSUPPORTED;
 }
}
static EFI_STATUS ProductStep(VOID *ctx,R3_STAGE phase){
 if(phase==R4_CONSUMER_RELEASE||phase==R3_DISCONNECT||phase==R3_TRANSACTION||phase==R3_RESTORE||phase==R3_START){
  GuLog("STAGE-BEFORE phase=%u gpu=%#llx agent=%#llx owned=%u armed=%u last_command=%#x allocations=%zu\n",
   (unsigned)phase,(unsigned long long)(UINTN)Gu.handle,(unsigned long long)(UINTN)PendingOwner.agent,Gu.owned,Gu.armed,Gu.master_last_command,(size_t)Gu.allocations);
  if(!GuLogReady())return EFI_DEVICE_ERROR;
 }
 EFI_STATUS status=Step(ProductAction(ctx,phase));
 if(!EFI_ERROR(status)) {
  if(phase==R3_IDENTIFY)status=GuSnapshotInit();
  else if(phase==R4_CONSUMER_RELEASE)GuSnapshotTake(1);
  else if(phase==R3_DISCONNECT)GuSnapshotTake(2);
  else if(phase==R3_POST_DECODE)GuSnapshotTake(3);
  else if(phase==R3_TRANSACTION)GuSnapshotTake(4);
 }
 return status;
}
#ifdef GU_PCI_SNAPSHOT_NORMAL
static EFI_STATUS SnapshotNormal(VOID){
 EFI_STATUS s=FindTarget();if(EFI_ERROR(s))return s;
 s=GuSnapshotInit();if(EFI_ERROR(s))return s;
 s=DiscoverWindows();if(EFI_ERROR(s))return s;
 s=LoadDiscoveredWindows();if(EFI_ERROR(s))return s;
 GuSnapshotTake(7);if(!GuSnapshotFlush(TRUE))return EFI_DEVICE_ERROR;
 s=GuIoClose();if(EFI_ERROR(s))return s;
 UINTN size=0;CHAR16 *data=NULL;
 return Gu.bs->StartImage(WindowsImage,&size,&data);
}
#endif
static EFI_STATUS ApplicationRun(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St) {
 if(!St||!St->BootServices||!St->RuntimeServices)return EFI_INVALID_PARAMETER;
 WindowsImage=NULL;WindowsVolume=NULL;WindowsDevicePath=NULL;GuScreenDetached=FALSE;ExpectDetached=FALSE;
 ColdSelectorsRecorded=FALSE;ColdSs0=ColdSs1=0;
 GuNativeReset();
 GuSnapshotReset();
 Table=St;GuInit(St->BootServices,NULL);Gu.image=Image;Gu.display_check=DisplayCheck;
 memset(&PendingOwner,0,sizeof(PendingOwner));
 Stage="usb-log";EFI_STATUS status=GuIoOpen(Image,St);
 R3_CONTEXT ctx={Image,St,FALSE,FALSE,FALSE};R3_FLOW flow={R3_IDENTIFY,FALSE};
 if(!EFI_ERROR(status)){
  UINT8 secure=0xff;UINTN bytes=1;
  status=St->RuntimeServices->GetVariable(L"SecureBoot",&GlobalGuid,NULL,&bytes,&secure);
  if(!EFI_ERROR(status)&&(bytes!=1||secure))status=EFI_SECURITY_VIOLATION;
 }
 if(!EFI_ERROR(status)) {
#ifdef GU_PCI_SNAPSHOT_NORMAL
  status=SnapshotNormal();
#else
  status=Rev3ControlRun(&ctx,ProductStep,&flow);
#endif
 }
 if(ctx.released){(void)GuSnapshotFlush(FALSE);ReleaseHalt(status,ctx.attempted,ctx.parent_attempted);return EFI_ABORTED;}
 if(GuLogReady())GuLog("FINAL status=%#llx stage=%s mutation=0\n",(unsigned long long)status,Stage);
 if(WindowsImage)Gu.bs->UnloadImage(WindowsImage);
 GuFree(WindowsDevicePath);WindowsDevicePath=NULL;GuNativeFree();GuReleaseAssets();
 EFI_STATUS closed=GuIoClose();if(EFI_ERROR(closed))status=closed;
 if(EFI_ERROR(status)&&status!=EFI_ABORTED&&status!=EFI_TIMEOUT)PausePreflightFailure();
 return status;
}
EFI_STATUS EFIAPI EfiMain(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St) {
 if(!Rev3ExecutionApproved)return EFI_ACCESS_DENIED;
 static BOOLEAN entered=FALSE;if(entered)return EFI_ALREADY_STARTED;entered=TRUE;
 return ApplicationRun(Image,St);
}
#ifdef GU_HOST_APP_TEST
/* Windows host tests call the shared body only with synthetic EFI vtables. */
EFI_STATUS GuHostApplicationRun(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St){return ApplicationRun(Image,St);}
#endif



