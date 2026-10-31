/* Recovery through the unmodified native GOP extracted from this board's ROM.
 * LoadImage keeps platform policy. Native DriverBinding is invoked under its
 * documented calling contract; no BIOS patch, ROM write or extra GPU reset. */
#include "native_gop.h"
#include "runtime.h"
#include "app_io.h"
#include "sha256.h"
#include "experimental_mode.h"
#include <Protocol/LoadedImage.h>
#include <Protocol/Decompress.h>
EFI_STATUS ExtractGop(EFI_BOOT_SERVICES*,EFI_DECOMPRESS_PROTOCOL*,const VOID*,UINTN,VOID**);
CHAR16 *GopExtractStage(VOID);
static VOID *NativeGopBuffer;
#define NATIVE_GOP_BYTES 164608U
static EFI_GUID BindingGuid=EFI_DRIVER_BINDING_PROTOCOL_GUID,LoadedGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID,
 PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID,PciGuid=EFI_PCI_IO_PROTOCOL_GUID,GopGuid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_DEVICE_PATH_PROTOCOL *ImagePath;
static UINTN ParentPathBytes;
static EFI_HANDLE NativeImage,NativeAgent;
static BOOLEAN LoadAttempted;
static BOOLEAN ConnectAttempted;
static const char ImageHash[]="b69af88e4f6238933379922b27a6a2cd392f361df862014c40bea92e79483e80";
static const char CodeHash[]="216c7270fa08ad9e4c3694d34d825e9e904554a1af5ffa2aeead4317c5537b37";
BOOLEAN GuNativeEnabled(VOID){
#if defined(GU_HOST_APP_TEST)
#ifdef GU_NATIVE_GOP_TEST
 return TRUE;
#else
 return FALSE;
#endif
#else
 return GU_EXPERIMENTAL_ENABLED;
#endif
}
VOID GuNativeReset(VOID){NativeGopBuffer=NULL;ImagePath=NULL;ParentPathBytes=0;NativeImage=NativeAgent=NULL;LoadAttempted=ConnectAttempted=FALSE;}
VOID GuNativeFree(VOID){GuFree(NativeGopBuffer);NativeGopBuffer=NULL;GuFree(ImagePath);ImagePath=NULL;ParentPathBytes=0;}
static EFI_STATUS PathBytes(const EFI_DEVICE_PATH_PROTOCOL *path,UINTN *bytes){
 if(!path||!bytes)return EFI_INVALID_PARAMETER;UINTN off=0;
 for(UINTN i=0;i<128&&off<=1020;i++){
  const EFI_DEVICE_PATH_PROTOCOL *n=(const VOID*)((const UINT8*)path+off);
  UINTN len=n->Length[0]+((UINTN)n->Length[1]<<8);
  if(len<4||len>1024-off)return EFI_COMPROMISED_DATA;
  if(n->Type==END_DEVICE_PATH_TYPE){
   if(n->SubType!=END_ENTIRE_DEVICE_PATH_SUBTYPE||len!=4)return EFI_UNSUPPORTED;
   *bytes=off+4;return EFI_SUCCESS;
  }off+=len;
 }return EFI_BAD_BUFFER_SIZE;
}
EFI_STATUS GuNativePrepare(VOID){
 if(!GuNativeEnabled())return EFI_SUCCESS;
 if(ImagePath||LoadAttempted||Gu.started||Gu.owned||!Gu.pci)return EFI_NOT_READY;
 if(NativeGopBuffer||Gu.pci->RomSize!=0x24c00)return EFI_SECURITY_VIOLATION;
 EFI_GUID decompressGuid=EFI_DECOMPRESS_PROTOCOL_GUID;
 EFI_DECOMPRESS_PROTOCOL *decompress=NULL;
 EFI_STATUS prepared=Gu.bs->LocateProtocol(&decompressGuid,NULL,(VOID**)&decompress);
 if(EFI_ERROR(prepared)){GuLog("native-GOP dynamic: LocateProtocol status=%#llx\n",(unsigned long long)prepared);return prepared;}
 prepared=ExtractGop(Gu.bs,decompress,Gu.pci->RomImage,(UINTN)Gu.pci->RomSize,&NativeGopBuffer); /* Stage labels are bounded ASCII strings exposed through the UEFI probe API. */
 const CHAR16 *wideStage=GopExtractStage();char stageText[64];UINTN stageIndex=0;
 if(wideStage){for(;stageIndex<sizeof(stageText)-1&&wideStage[stageIndex];stageIndex++)stageText[stageIndex]=wideStage[stageIndex]<128?(char)wideStage[stageIndex]:'?';}
 stageText[stageIndex]=0;
 GuLog("native-GOP dynamic: extract_stage=%s status=%#llx before_confirmation=1\n",stageText,(unsigned long long)prepared);
 GuLog("native-GOP dynamic: preparation status=%#llx before_confirmation=1\n",(unsigned long long)prepared);
 if(EFI_ERROR(prepared))return prepared;
 EFI_DEVICE_PATH_PROTOCOL *parent=NULL;UINTN bytes=0;
 EFI_STATUS s=Gu.bs->HandleProtocol(Gu.handle,&PathGuid,(VOID**)&parent);if(EFI_ERROR(s))return s;
 s=PathBytes(parent,&bytes);if(EFI_ERROR(s)||bytes<10)return EFI_COMPROMISED_DATA;
 /* PCI bus drivers describe option ROM images with Relative Offset Range.
  * The offsets are in the verified 150528-byte RomImage, not physical MMIO. */
 UINT8 *p=GuAlloc(bytes+24);if(!p)return EFI_OUT_OF_RESOURCES;
 ParentPathBytes=bytes-4;memcpy(p,parent,ParentPathBytes);
 UINT8 *n=p+ParentPathBytes;n[0]=MEDIA_DEVICE_PATH;n[1]=MEDIA_RELATIVE_OFFSET_RANGE_DP;n[2]=24;
 UINT64 first=0xfc00,last=0x24bff;memcpy(n+8,&first,8);memcpy(n+16,&last,8);
 n[24]=END_DEVICE_PATH_TYPE;n[25]=END_ENTIRE_DEVICE_PATH_SUBTYPE;n[26]=4;
 ImagePath=(VOID*)p;
 GuLog("native-GOP prepared: source=runtime_decompressed_verified_ROM raw_bytes=%zu sha256=%s rom_range=fc00:24bff\n",NATIVE_GOP_BYTES,ImageHash);
 return GuLogReady()?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
static EFI_STATUS Identity(EFI_HANDLE agent,GU_RELEASE_OWNER *owner){
 EFI_DRIVER_BINDING_PROTOCOL *b=NULL;EFI_LOADED_IMAGE_PROTOCOL *li=NULL;
 if(!NativeImage||!agent)return EFI_NOT_READY;
 EFI_STATUS s=Gu.bs->HandleProtocol(agent,&BindingGuid,(VOID**)&b);
 if(EFI_ERROR(s)||!b||b->ImageHandle!=NativeImage||b->DriverBindingHandle!=agent||b->Version!=0x70011)return EFI_SECURITY_VIOLATION;
 s=Gu.bs->HandleProtocol(NativeImage,&LoadedGuid,(VOID**)&li);
 if(EFI_ERROR(s)||!li||!li->ImageBase||li->ImageSize!=0x27ac0||li->ImageCodeType!=EfiBootServicesCode||
  li->DeviceHandle!=Gu.handle||(UINTN)li->ImageBase>MAX_UINTN-0x27ac0)return EFI_SECURITY_VIOLATION;
 UINTN base=(UINTN)li->ImageBase;
 if((UINTN)b->Supported!=base+0x3c8||(UINTN)b->Start!=base+0x704||(UINTN)b->Stop!=base+0x6ec||
  !GuHashMatches((UINT8*)li->ImageBase+0x280,0x21bc7,CodeHash))return EFI_SECURITY_VIOLATION;
 if(owner){memset(owner,0,sizeof(*owner));owner->agent=agent;owner->image=NativeImage;owner->base=li->ImageBase;
  owner->size=li->ImageSize;owner->version=b->Version;owner->stop=b->Stop;}
 return EFI_SUCCESS;
}
EFI_STATUS GuNativeLoad(EFI_HANDLE *driver){
 if(!driver)return EFI_INVALID_PARAMETER;*driver=NULL;
 if(!GuNativeEnabled()||!NativeGopBuffer||!ImagePath||LoadAttempted||Gu.owned||!Gu.quiesced||Gu.flrs!=1||Gu.allocations||Gu.dma_count||Gu.fault)return EFI_NOT_READY;
 EFI_STATUS isolated=GuLegacyIsolationCheck();if(EFI_ERROR(isolated))return isolated;
 LoadAttempted=TRUE;
 GuLogText("stage=before_NativeGOP_LoadImage; platform_image_policy_not_overridden; signature_trust_not_asserted\n");if(!GuLogReady())return EFI_DEVICE_ERROR;
 EFI_STATUS s=Gu.bs->LoadImage(FALSE,Gu.image,ImagePath,NativeGopBuffer,NATIVE_GOP_BYTES,&NativeImage);
 GuLog("stage=after_NativeGOP_LoadImage status=%#llx image=%#llx\n",(unsigned long long)s,(unsigned long long)(UINTN)NativeImage);
 if(EFI_ERROR(s))return s;if(!NativeImage||!GuLogReady())return EFI_DEVICE_ERROR;
 s=GuLegacyIsolationCheck();if(EFI_ERROR(s))return s;
 EFI_LOADED_IMAGE_PROTOCOL *li=NULL;s=Gu.bs->HandleProtocol(NativeImage,&LoadedGuid,(VOID**)&li);
 if(EFI_ERROR(s)||!li||!li->ImageBase||li->ImageSize!=0x27ac0||li->ImageCodeType!=EfiBootServicesCode||li->DeviceHandle!=Gu.handle||
  !GuHashMatches((UINT8*)li->ImageBase+0x280,0x21bc7,CodeHash))return EFI_SECURITY_VIOLATION;
 GuLogText("stage=before_NativeGOP_StartImage\n");if(!GuLogReady())return EFI_DEVICE_ERROR;
 UINTN exitBytes=0;CHAR16 *exitData=NULL;s=Gu.bs->StartImage(NativeImage,&exitBytes,&exitData);
 GuLog("stage=after_NativeGOP_StartImage status=%#llx exit_bytes=%zu\n",(unsigned long long)s,(size_t)exitBytes);
 if(exitData)GuFree(exitData);if(EFI_ERROR(s))return s;if(!GuLogReady())return EFI_DEVICE_ERROR;
 s=GuLegacyIsolationCheck();if(EFI_ERROR(s))return s;
 EFI_HANDLE *handles=NULL;UINTN count=0,matches=0;
 s=Gu.bs->LocateHandleBuffer(ByProtocol,&BindingGuid,NULL,&count,&handles);
 if(!EFI_ERROR(s)&&(!handles||!count||count>2048))s=EFI_COMPROMISED_DATA;
 for(UINTN i=0;!EFI_ERROR(s)&&i<count;i++){
  EFI_DRIVER_BINDING_PROTOCOL *b=NULL;EFI_STATUS q=Gu.bs->HandleProtocol(handles[i],&BindingGuid,(VOID**)&b);
  if(EFI_ERROR(q)||!b){s=EFI_COMPROMISED_DATA;break;}
  if(b->ImageHandle==NativeImage){NativeAgent=handles[i];matches++;}
 }
 GuFree(handles);if(EFI_ERROR(s))return s;if(matches!=1)return EFI_NO_MAPPING;
 s=Identity(NativeAgent,NULL);if(EFI_ERROR(s))return s;
 GuLog("stage=native_GOP_binding_verified agent=%#llx image=%#llx version=70011; ConnectController priority is not an exclusion filter\n",
  (unsigned long long)(UINTN)NativeAgent,(unsigned long long)(UINTN)NativeImage);
 *driver=NativeAgent;return GuLogReady()?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
static EFI_STATUS ParentFree(VOID){
 EFI_STATUS s=GuRefreshTargetPci();if(EFI_ERROR(s))return s;
 EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL;UINTN count=0;
 s=Gu.bs->OpenProtocolInformation(Gu.handle,&PciGuid,&entries,&count);
 if(!EFI_ERROR(s)&&(count>32||(count&&!entries)))s=EFI_COMPROMISED_DATA;
 for(UINTN i=0;!EFI_ERROR(s)&&i<count;i++)if(entries[i].Attributes!=1&&entries[i].Attributes!=2&&entries[i].Attributes!=4)s=EFI_ACCESS_DENIED;
 GuFree(entries);return s;
}
#if defined(GU_HOST_APP_TEST) && defined(GU_NATIVE_GOP_TEST)
extern EFI_STATUS GuHostNativeSupported(EFI_DRIVER_BINDING_PROTOCOL*,EFI_HANDLE,EFI_DEVICE_PATH_PROTOCOL*);
extern EFI_STATUS GuHostNativeStart(EFI_DRIVER_BINDING_PROTOCOL*,EFI_HANDLE,EFI_DEVICE_PATH_PROTOCOL*);
#endif
EFI_STATUS GuNativeConnect(VOID){
 if(!GuNativeEnabled()||!NativeAgent||ConnectAttempted||Gu.owned||Gu.fault||
    !Gu.quiesced||Gu.flrs!=1||Gu.allocations||Gu.dma_count)return EFI_NOT_READY;
 EFI_TPL tpl=Gu.bs->RaiseTPL(TPL_HIGH_LEVEL);Gu.bs->RestoreTPL(tpl);
 if(tpl!=TPL_APPLICATION)return EFI_UNSUPPORTED;
 EFI_STATUS s=GuLegacyIsolationCheck();if(!EFI_ERROR(s))s=ParentFree();
 if(!EFI_ERROR(s))s=Identity(NativeAgent,NULL);if(EFI_ERROR(s))return s;
 EFI_DRIVER_BINDING_PROTOCOL *binding=NULL;
 s=Gu.bs->HandleProtocol(NativeAgent,&BindingGuid,(VOID**)&binding);if(EFI_ERROR(s)||!binding)return EFI_NOT_FOUND;
 ConnectAttempted=TRUE;
 GuLog("stage=before_NativeGOP_Supported agent=%#llx parent=%#llx remaining=NULL tpl=APPLICATION\n",
  (unsigned long long)(UINTN)NativeAgent,(unsigned long long)(UINTN)Gu.handle);
 if(!GuLogReady())return EFI_DEVICE_ERROR;
#if defined(GU_HOST_APP_TEST) && defined(GU_NATIVE_GOP_TEST)
 s=GuHostNativeSupported(binding,Gu.handle,NULL);
#else
 s=binding->Supported(binding,Gu.handle,NULL);
#endif
 GuLog("stage=after_NativeGOP_Supported status=%#llx\n",(unsigned long long)s);
 if(s!=EFI_SUCCESS)return EFI_ERROR(s)?s:EFI_DEVICE_ERROR;
 if(!GuLogReady())return EFI_DEVICE_ERROR;
 /* Same controller, This and NULL RemainingDevicePath as Supported(). */
 s=GuLegacyIsolationCheck();if(!EFI_ERROR(s))s=ParentFree();
 if(!EFI_ERROR(s))s=Identity(NativeAgent,NULL);if(EFI_ERROR(s))return s;
 EFI_DRIVER_BINDING_PROTOCOL *fresh=NULL;
 s=Gu.bs->HandleProtocol(NativeAgent,&BindingGuid,(VOID**)&fresh);
 if(EFI_ERROR(s)||fresh!=binding)return EFI_MEDIA_CHANGED;
 GuLogText("stage=before_NativeGOP_DriverStart; Supported=EFI_SUCCESS; no ConnectController fallback\n");
 if(!GuLogReady())return EFI_DEVICE_ERROR;
 s=GuNativeInventory("before_driver_start");if(EFI_ERROR(s))return s;
#if defined(GU_HOST_APP_TEST) && defined(GU_NATIVE_GOP_TEST)
 s=GuHostNativeStart(binding,Gu.handle,NULL);
#else
 s=binding->Start(binding,Gu.handle,NULL);
#endif
 GuLog("stage=after_NativeGOP_DriverStart status=%#llx\n",(unsigned long long)s);
 EFI_STATUS inventory=GuNativeInventory("after_driver_start");
 if(s!=EFI_SUCCESS)return EFI_ERROR(s)?s:EFI_DEVICE_ERROR;
 if(EFI_ERROR(inventory))return inventory;
 if(!GuLogReady())return EFI_DEVICE_ERROR;
 return GuLegacyIsolationCheck();
}
static EFI_STATUS NativeReject(const char *reason,EFI_STATUS status,EFI_HANDLE handle){
 GuLog("native-inspect reject reason=%s handle=%#llx status=%#llx\n",reason,
  (unsigned long long)(UINTN)handle,(unsigned long long)status);return status;
}
static VOID ProtocolLog(const char *stage,EFI_HANDLE h,const EFI_GUID *g,EFI_STATUS status){
 GuLog("native-protocol stage=%s handle=%#llx guid=%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x status=%#llx\n",
  stage,(unsigned long long)(UINTN)h,g->Data1,g->Data2,g->Data3,g->Data4[0],g->Data4[1],g->Data4[2],g->Data4[3],g->Data4[4],g->Data4[5],g->Data4[6],g->Data4[7],(unsigned long long)status);
}
static VOID ModeLog(const char *stage,EFI_HANDLE h,EFI_STATUS status,EFI_GRAPHICS_OUTPUT_PROTOCOL *g){
 EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *m=!EFI_ERROR(status)&&g?g->Mode:NULL;
 EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *i=m&&m->Info&&m->SizeOfInfo>=sizeof(*i)?m->Info:NULL;
 const char *state=!m?"NO_MODE":!i?"INVALID_INFO":(!m->FrameBufferBase&&!m->FrameBufferSize&&(!m->MaxMode||m->Mode>=m->MaxMode))?"UNINITIALIZED":
  (m->MaxMode&&m->Mode<m->MaxMode&&m->FrameBufferBase&&m->FrameBufferSize&&i)?"READY_CANDIDATE":"INCONSISTENT";
 GuLog("native-mode stage=%s handle=%#llx status=%#llx GOP=%#llx ModePtr=%#llx state=%s MaxMode=%u Mode=%u Info=%#llx InfoBytes=%zu FB=%#llx FBBytes=%zu width=%u height=%u stride=%u pixel=%u\n",
  stage,(unsigned long long)(UINTN)h,(unsigned long long)status,(unsigned long long)(UINTN)g,(unsigned long long)(UINTN)m,state,
  m?m->MaxMode:0,m?m->Mode:0,(unsigned long long)(UINTN)(m?m->Info:NULL),(size_t)(m?m->SizeOfInfo:0),
  (unsigned long long)(m?m->FrameBufferBase:0),(size_t)(m?m->FrameBufferSize:0),i?i->HorizontalResolution:0,i?i->VerticalResolution:0,i?i->PixelsPerScanLine:0,i?(UINT32)i->PixelFormat:MAX_UINT32);
}
static VOID PathLog(const char *stage,EFI_HANDLE h){
 EFI_DEVICE_PATH_PROTOCOL *p=NULL;UINTN bytes=0;
 EFI_STATUS s=Gu.bs->HandleProtocol(h,&PathGuid,(VOID**)&p);
 if(!EFI_ERROR(s))s=PathBytes(p,&bytes);
 GuLog("native-path stage=%s handle=%#llx status=%#llx bytes=%zu\n",stage,(unsigned long long)(UINTN)h,(unsigned long long)s,(size_t)bytes);
 if(EFI_ERROR(s))return;
 static const char digits[]="0123456789abcdef";
 for(UINTN off=0;off<bytes;off+=32){
  UINTN n=bytes-off;if(n>32)n=32;char hex[65];
  for(UINTN j=0;j<n;j++){UINT8 v=((UINT8*)p)[off+j];hex[2*j]=digits[v>>4];hex[2*j+1]=digits[v&15];}hex[2*n]=0;
  GuLog("native-path-bytes handle=%#llx offset=%zu data=%s\n",(unsigned long long)(UINTN)h,(size_t)off,hex);
 }
 for(UINTN off=0;off<bytes;){
  EFI_DEVICE_PATH_PROTOCOL *n=(VOID*)((UINT8*)p+off);UINTN len=n->Length[0]+((UINTN)n->Length[1]<<8);UINT32 adr=0;
  if(n->Type==ACPI_DEVICE_PATH&&n->SubType==ACPI_ADR_DP&&len==8)memcpy(&adr,(UINT8*)n+4,4);
  GuLog("native-path-node handle=%#llx offset=%zu type=%u subtype=%u length=%zu adr=%#x\n",(unsigned long long)(UINTN)h,(size_t)off,n->Type,n->SubType,(size_t)len,adr);off+=len;
 }
}
EFI_STATUS GuNativeInventory(const char *stage){
 /* Protocol metadata and RAM fields only: no QueryMode/SetMode/Blt or MMIO. */
 EFI_HANDLE *handles=NULL;UINTN count=0;
 EFI_STATUS s=Gu.bs->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&count,&handles);
 GuLog("native-inventory stage=%s GOP-list-status=%#llx GOP-count=%zu parent=%#llx agent=%#llx\n",stage,(unsigned long long)s,(size_t)count,(unsigned long long)(UINTN)Gu.handle,(unsigned long long)(UINTN)NativeAgent);
 if(s==EFI_NOT_FOUND)s=EFI_SUCCESS;
 if(!EFI_ERROR(s)&&(count>128||(count&&!handles)))s=EFI_BAD_BUFFER_SIZE;
 for(UINTN i=0;!EFI_ERROR(s)&&i<count;i++){
  EFI_GRAPHICS_OUTPUT_PROTOCOL *g=NULL;EFI_STATUS q=Gu.bs->HandleProtocol(handles[i],&GopGuid,(VOID**)&g);
  ModeLog(stage,handles[i],q,g);PathLog(stage,handles[i]);
 }
 GuFree(handles);if(EFI_ERROR(s))return NativeReject("INVENTORY_GOP_LIST",s,Gu.handle);
 PathLog(stage,Gu.handle);
 EFI_GUID **protocols=NULL;count=0;s=Gu.bs->ProtocolsPerHandle(Gu.handle,&protocols,&count);
 if(!EFI_ERROR(s)&&(!protocols||!count||count>32))s=EFI_COMPROMISED_DATA;
 for(UINTN i=0;!EFI_ERROR(s)&&i<count;i++){
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL;UINTN n=0;
  if(!protocols[i]){s=EFI_COMPROMISED_DATA;break;}
  s=Gu.bs->OpenProtocolInformation(Gu.handle,protocols[i],&entries,&n);ProtocolLog(stage,Gu.handle,protocols[i],s);
  if(!EFI_ERROR(s)&&(n>32||(n&&!entries)))s=EFI_COMPROMISED_DATA;
  for(UINTN j=0;!EFI_ERROR(s)&&j<n;j++){
   EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e=&entries[j];
   GuLog("native-open stage=%s parent=%#llx protocol=%08x agent=%#llx controller=%#llx attributes=%#x OpenCount=%u BY_DRIVER=%u BY_CHILD_CONTROLLER=%u\n",
    stage,(unsigned long long)(UINTN)Gu.handle,protocols[i]->Data1,(unsigned long long)(UINTN)e->AgentHandle,(unsigned long long)(UINTN)e->ControllerHandle,e->Attributes,e->OpenCount,
    (e->Attributes&EFI_OPEN_PROTOCOL_BY_DRIVER)!=0,(e->Attributes&EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER)!=0);
   if(e->Attributes==EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER&&e->ControllerHandle){
    EFI_GRAPHICS_OUTPUT_PROTOCOL *g=NULL;EFI_STATUS q=Gu.bs->HandleProtocol(e->ControllerHandle,&GopGuid,(VOID**)&g);
    ModeLog(stage,e->ControllerHandle,q,g);PathLog(stage,e->ControllerHandle);
   }
  }GuFree(entries);
 }GuFree(protocols);
 return EFI_ERROR(s)?NativeReject("INVENTORY_PARENT_PROTOCOLS",s,Gu.handle):GuLogReady()?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
static EFI_STATUS Child(EFI_HANDLE handle,GU_RELEASE_OWNER *owner,BOOLEAN requireDisplay){
 GuLog("native-child phase=%s handle=%#llx\n",requireDisplay?"FINAL":"STRUCTURE",(unsigned long long)(UINTN)handle);
 EFI_DEVICE_PATH_PROTOCOL *p=NULL;UINTN bytes=0;
 EFI_STATUS s=Gu.bs->HandleProtocol(handle,&PathGuid,(VOID**)&p);if(EFI_ERROR(s))return NativeReject("CHILD_PATH_PROTOCOL",s,handle);
 s=PathBytes(p,&bytes);if(EFI_ERROR(s))return NativeReject("CHILD_PATH_FORMAT",s,handle);
 PathLog("inspect",handle);
 if(!ImagePath||bytes!=ParentPathBytes+12||memcmp(p,ImagePath,ParentPathBytes))return NativeReject("CHILD_PATH_PARENT_OR_SIZE",EFI_MEDIA_CHANGED,handle);
 const UINT8 *adr=(const UINT8*)p+ParentPathBytes;
 if(adr[0]!=ACPI_DEVICE_PATH||adr[1]!=ACPI_ADR_DP||adr[2]!=8||adr[3])return NativeReject("CHILD_ADR_NODE",EFI_UNSUPPORTED,handle);
 EFI_DEVICE_PATH_PROTOCOL *remaining=p;EFI_HANDLE parent=NULL;
 s=Gu.bs->LocateDevicePath(&PciGuid,&remaining,&parent);if(EFI_ERROR(s)||parent!=Gu.handle)return NativeReject("CHILD_ANCESTRY",EFI_ACCESS_DENIED,handle);
 EFI_GRAPHICS_OUTPUT_PROTOCOL *g=NULL;s=Gu.bs->HandleProtocol(handle,&GopGuid,(VOID**)&g);
 ModeLog(requireDisplay?"final":"structure",handle,s,g);
 if((s==EFI_UNSUPPORTED||s==EFI_NOT_FOUND)&&!g){
  GuLog("native-GOP inactive output child=%#llx adr=%02x%02x%02x%02x\n",(unsigned long long)(UINTN)handle,adr[7],adr[6],adr[5],adr[4]);return EFI_SUCCESS;
 }
 if(EFI_ERROR(s)||!g)return NativeReject("CHILD_GOP_PROTOCOL",EFI_UNSUPPORTED,handle);
 if(!g->Mode)return NativeReject("MODE_POINTER",EFI_UNSUPPORTED,handle);
 EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *m=g->Mode;
 if(m->Info&&m->SizeOfInfo<sizeof(*m->Info))return NativeReject("MODE_INFO_SIZE",EFI_UNSUPPORTED,handle);
 /* A published mode description must be structurally valid before Console
  * code can dereference it; missing Info is not a mode awaiting SetMode. */
 if(!m->Info)return NativeReject("MODE_INFO_POINTER",EFI_UNSUPPORTED,handle);
 if(requireDisplay){
  if(!m->MaxMode||m->Mode>=m->MaxMode)return NativeReject("MODE_UNSET",EFI_UNSUPPORTED,handle);
  if(!m->FrameBufferBase||!m->FrameBufferSize)return NativeReject("FRAMEBUFFER_ZERO",EFI_UNSUPPORTED,handle);
  if(m->FrameBufferBase>MAX_UINT64-m->FrameBufferSize)return NativeReject("FRAMEBUFFER_OVERFLOW",EFI_UNSUPPORTED,handle);
  if(!m->Info->HorizontalResolution||!m->Info->VerticalResolution)return NativeReject("MODE_RESOLUTION",EFI_UNSUPPORTED,handle);
  if(m->Info->PixelFormat>PixelBlueGreenRedReserved8BitPerColor)return NativeReject("MODE_PIXEL_FORMAT",EFI_UNSUPPORTED,handle);
  if(m->Info->PixelsPerScanLine<m->Info->HorizontalResolution)return NativeReject("MODE_STRIDE",EFI_UNSUPPORTED,handle);
  if((UINT64)m->Info->PixelsPerScanLine*m->Info->VerticalResolution>m->FrameBufferSize/4)return NativeReject("FRAMEBUFFER_SIZE",EFI_UNSUPPORTED,handle);
 }
 if(owner->child_count){return owner->children[0].handle==handle?EFI_SUCCESS:EFI_NO_MAPPING;}
 GU_DISPLAY_CHILD *c=&owner->children[0];owner->child_count=1;c->handle=handle;c->protocol=PathGuid;c->gop=g;
 c->framebuffer=m->FrameBufferBase;c->framebuffer_size=m->FrameBufferSize;GuSha256(p,bytes,c->path_sha256);return EFI_SUCCESS;
}
EFI_STATUS GuNativeInspect(GU_RELEASE_OWNER *owner,R4_GRAPH *edges,BOOLEAN requireDisplay){
 if(!owner||!edges)return EFI_INVALID_PARAMETER;edges->count=0;
 GuLog("native-inspect phase=%s parent=%#llx agent=%#llx\n",requireDisplay?"FINAL":"STRUCTURE",(unsigned long long)(UINTN)Gu.handle,(unsigned long long)(UINTN)NativeAgent);
 EFI_STATUS s=Identity(NativeAgent,owner);if(EFI_ERROR(s))return NativeReject("PARENT_IDENTITY",s,Gu.handle);
 EFI_GUID **protocols=NULL;UINTN count=0,pciOwners=0,children=0;
 s=Gu.bs->ProtocolsPerHandle(Gu.handle,&protocols,&count);
 GuLog("native-parent enumeration-status=%#llx protocols=%zu\n",(unsigned long long)s,(size_t)count);
 if(!EFI_ERROR(s)&&(!protocols||!count||count>32))s=EFI_COMPROMISED_DATA;
 for(UINTN i=0;!EFI_ERROR(s)&&i<count;i++){
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL;UINTN n=0;
  if(!protocols[i]){s=EFI_COMPROMISED_DATA;break;}
  s=Gu.bs->OpenProtocolInformation(Gu.handle,protocols[i],&entries,&n);
  ProtocolLog("parent_open_info",Gu.handle,protocols[i],s);
  if(!EFI_ERROR(s)&&(n>32||(n&&!entries)))s=EFI_COMPROMISED_DATA;
  for(UINTN j=0;!EFI_ERROR(s)&&j<n;j++){
   EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e=&entries[j];
   if(e->Attributes==1||e->Attributes==2||e->Attributes==4)continue;
   BOOLEAN pci=!memcmp(protocols[i],&PciGuid,sizeof(PciGuid));
   if((!pci&&memcmp(protocols[i],&PathGuid,sizeof(PathGuid)))||e->AgentHandle!=NativeAgent||e->OpenCount!=1||edges->count>=32){s=EFI_ACCESS_DENIED;break;}
   if(e->Attributes==EFI_OPEN_PROTOCOL_BY_DRIVER){if(e->ControllerHandle!=Gu.handle){s=EFI_ACCESS_DENIED;break;}if(pci)pciOwners++;}
   else if(e->Attributes==EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER){
    if(!e->ControllerHandle||e->ControllerHandle==Gu.handle||e->ControllerHandle==Gu.image||++children>8){s=EFI_ACCESS_DENIED;break;}
    s=Child(e->ControllerHandle,owner,requireDisplay);if(EFI_ERROR(s))break;
   }else {s=EFI_ACCESS_DENIED;break;}
   edges->edges[edges->count++]=(R4_EDGE){Gu.handle,NativeAgent,e->ControllerHandle,*protocols[i],e->Attributes,1};
  }GuFree(entries);
 }GuFree(protocols);
 if(EFI_ERROR(s))return NativeReject("PARENT_OR_CHILD_SCAN",s,Gu.handle);
 GuLog("native-inspect counts pci_owners=%zu child_records=%zu GOP_children=%zu\n",(size_t)pciOwners,(size_t)children,(size_t)owner->child_count);
 if(pciOwners!=1||!children||owner->child_count!=1)return NativeReject("PARENT_OWNER_OR_GOP_CHILD_COUNT",EFI_NOT_FOUND,Gu.handle);
 return GuLogReady()?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
