/* Modified 2026-10-21: experimental AD103 32GB GA102-pair port; see PORT-NOTES.txt. */
/* Derived from the v0.3 handoff path exercised on this user's single AD103. */
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
static EFI_GUID FsGuid=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID,LoadedGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
static const CHAR16 WindowsPath[]=L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";
static EFI_HANDLE WindowsImage,WindowsVolume;
static EFI_DEVICE_PATH_PROTOCOL *WindowsDevicePath;
static EFI_STATUS MakeWindowsPath(VOID) {
  EFI_DEVICE_PATH_PROTOCOL *base=NULL;
  EFI_STATUS s=Gu.bs->HandleProtocol(WindowsVolume,&PathGuid,(VOID**)&base);
  if(EFI_ERROR(s)||!base)return EFI_NOT_FOUND;
  UINTN off=0;BOOLEAN end=FALSE;
  for(UINTN i=0;i<128 && off<=4092;i++) {
    EFI_DEVICE_PATH_PROTOCOL *n=(VOID*)((UINT8*)base+off);
    UINTN len=n->Length[0]+((UINTN)n->Length[1]<<8);
    if(len<4||len>4096-off)return EFI_COMPROMISED_DATA;
    if(n->Type==END_DEVICE_PATH_TYPE) {
      if(n->SubType!=END_ENTIRE_DEVICE_PATH_SUBTYPE||len!=4)return EFI_UNSUPPORTED;
      end=TRUE;break;
    }
    off+=len;
  }
  if(!end)return EFI_COMPROMISED_DATA;
  UINTN node=4+sizeof(WindowsPath),total=off+node+4;
  UINT8 *b=GuAlloc(total);if(!b)return EFI_OUT_OF_RESOURCES;
  memcpy(b,base,off);b[off]=MEDIA_DEVICE_PATH;b[off+1]=MEDIA_FILEPATH_DP;
  b[off+2]=(UINT8)node;b[off+3]=(UINT8)(node>>8);memcpy(b+off+4,WindowsPath,sizeof(WindowsPath));
  b[off+node]=END_DEVICE_PATH_TYPE;b[off+node+1]=END_ENTIRE_DEVICE_PATH_SUBTYPE;b[off+node+2]=4;
  WindowsDevicePath=(VOID*)b;return EFI_SUCCESS;
}
static EFI_STATUS DiscoverWindows(VOID) {
  EFI_HANDLE *h=NULL;UINTN n=0,found=0;
  EFI_STATUS s=Gu.bs->LocateHandleBuffer(ByProtocol,&FsGuid,NULL,&n,&h);
  if(EFI_ERROR(s))return s;
  if(!h||n>256){GuFree(h);return EFI_COMPROMISED_DATA;}
  for(UINTN i=0;i<n;i++) {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;EFI_FILE_PROTOCOL *root=NULL,*file=NULL;
    EFI_STATUS x=Gu.bs->HandleProtocol(h[i],&FsGuid,(VOID**)&fs);
    if(!EFI_ERROR(x)&&fs)x=fs->OpenVolume(fs,&root);
    if(EFI_ERROR(x)||!root){s=EFI_DEVICE_ERROR;break;}
    x=root->Open(root,&file,(CHAR16*)WindowsPath,EFI_FILE_MODE_READ,0);
    if(!EFI_ERROR(x)&&file) {
      UINT8 mz[2];UINTN bytes=2;EFI_STATUS y=file->Read(file,&bytes,mz);
      EFI_STATUS closed=file->Close(file);
      if(EFI_ERROR(y)||EFI_ERROR(closed)||bytes!=2||mz[0]!='M'||mz[1]!='Z')s=EFI_COMPROMISED_DATA;
      else {found++;WindowsVolume=h[i];}
    } else if(x!=EFI_NOT_FOUND)s=EFI_DEVICE_ERROR;
    x=root->Close(root);if(EFI_ERROR(x))s=x;
    if(EFI_ERROR(s))break;
  }
  GuFree(h);GuLog("Windows candidates=%zu status=%#llx\n",(size_t)found,(unsigned long long)s);
  if(EFI_ERROR(s))return s;
  if(found!=1)return found?EFI_NO_MAPPING:EFI_NOT_FOUND;
  s=MakeWindowsPath();if(EFI_ERROR(s))return s;
  return EFI_SUCCESS;
}
static EFI_STATUS LoadDiscoveredWindows(VOID) {
  if(!WindowsVolume||!WindowsDevicePath||WindowsImage)return EFI_NOT_READY;
  EFI_STATUS s=Gu.bs->LoadImage(FALSE,Gu.image,WindowsDevicePath,NULL,0,&WindowsImage);
  GuLog("Windows LoadImage=%#llx\n",(unsigned long long)s);
  if(EFI_ERROR(s))return s;
  EFI_LOADED_IMAGE_PROTOCOL *li=NULL;
  s=Gu.bs->HandleProtocol(WindowsImage,&LoadedGuid,(VOID**)&li);
  if(EFI_ERROR(s)||!li||li->DeviceHandle!=WindowsVolume||li->ImageCodeType!=EfiLoaderCode)return EFI_COMPROMISED_DATA;
  return GuLogReady()?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
static EFI_STATUS VerifySelectors(const char *phase) {
  UINT32 ss0=GuRead32(0x82381c),ss1=GuRead32(0x823820);
  GuLog("%s SS0=0x%08x SS1=0x%08x\n",phase,ss0,ss1);
  GuLog("%s unlock_matched=%u; handoff checks GPU accessibility separately\n",phase,ss0==0x88888888 && ss1==8);
  return !Gu.fault && GuLogReady() && Rev3SelectorsMatch(ss0,ss1)?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
/* Native GOP preserves decode, but must not regain DMA permission.
 * Compare every other COMMAND bit with the boot snapshot. Read only. */
static EFI_STATUS VerifyRestoredPci(const char *phase) {
  UINT16 command=0,initial=(UINT16)InitialConfig[1];UINT32 value=0;
  EFI_STATUS s=GuConfigRead(EfiPciIoWidthUint16,4,&command);
  GuLog("stage=%s_COMMAND policy=%s status=%#llx actual=%#x initial=%#x expected=%#x BME=%u\n",
    phase,GuNativeEnabled()?"NATIVE_DECODE_NO_DMA":"LEGACY_EXACT",(unsigned long long)s,
    command,initial,GuNativeEnabled()?(UINT16)(initial&~4U):initial,(command&4)!=0);
  if(EFI_ERROR(s))return s;
  if(GuNativeEnabled()) {
    const char *reason=NULL;
    if(initial==0xffffU||(initial&3U)!=3U)reason="INITIAL_DECODE";
    else if((command&3U)!=3U)reason="DECODE_MISSING";
    else if(command&4U)reason="BME_ENABLED";
    else if((command&~7U)!=(initial&~7U))reason="UNEXPECTED_COMMAND_BITS";
    if(reason){GuLog("native-pci-reject phase=%s reason=%s\n",phase,reason);return EFI_DEVICE_ERROR;}
    UINT64 attributes=0;
    s=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationGet,0,&attributes);
    UINT64 mask=EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY|EFI_PCI_IO_ATTRIBUTE_BUS_MASTER;
    UINT64 expected=EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY;
    GuLog("stage=%s_ATTRIBUTES status=%#llx actual=%#llx mask=%#llx expected=%#llx\n",
      phase,(unsigned long long)s,(unsigned long long)attributes,(unsigned long long)mask,(unsigned long long)expected);
    if(EFI_ERROR(s)||(attributes&mask)!=expected){GuLog("native-pci-reject phase=%s reason=ATTRIBUTES_INCONSISTENT\n",phase);return EFI_DEVICE_ERROR;}
  }else if(!Rev3RestoredCommand(command,initial))return EFI_DEVICE_ERROR;
  for(UINT32 offset=0x10;offset<=0x24;offset+=4) {
    s=GuConfigRead(EfiPciIoWidthUint32,offset,&value);
    GuLog("stage=%s_BAR offset=%#x status=%#llx actual=%#x expected=%#x\n",
      phase,offset,(unsigned long long)s,value,InitialConfig[offset/4]);
    if(EFI_ERROR(s)||value!=InitialConfig[offset/4])return EFI_DEVICE_ERROR;
  }
  return GuLogReady()?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
static EFI_STATUS RestoreDisplayForWindows(EFI_HANDLE Driver) {
  if(!WindowsImage || !Driver || !GuScreenDetached || !GuSafeToReturn() || !Gu.quiesced ||
     Gu.flrs!=1 || Gu.allocations || Gu.dma_count || !Gu.owned || Gu.fault)return EFI_NOT_READY;
  EFI_STATUS s=VerifySelectors("POST-FLR");if(EFI_ERROR(s))return s;
  GuLogText("stage=before_GOP_reconnect; transaction quiesced, DMA released, PCI restored\n");
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  if(GuNativeEnabled()){s=GuIsolateLegacyBinding();if(EFI_ERROR(s))return s;}
  GuLog("stage=before_CloseProtocol gpu=%#llx agent=%#llx controller=NULL owned=%u detached=%u\n",(unsigned long long)(UINTN)Gu.handle,(unsigned long long)(UINTN)Gu.image,Gu.owned,GuScreenDetached);
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  s=Gu.bs->CloseProtocol(Gu.handle,&PciGuid,Gu.image,NULL);
  /* Reflect a successful close before any logging failure can return. */
  if(!EFI_ERROR(s))Gu.owned=FALSE;
  GuLog("stage=after_CloseProtocol status=%#llx gpu=%#llx agent=%#llx owned=%u detached=%u\n",(unsigned long long)s,(unsigned long long)(UINTN)Gu.handle,(unsigned long long)(UINTN)Gu.image,Gu.owned,GuScreenDetached);
  if(EFI_ERROR(s))return s;
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  if(GuNativeEnabled()){
    EFI_HANDLE legacy=Driver;s=GuNativeLoad(&Driver);
    GuLog("stage=native_GOP_recovery_selected status=%#llx legacy=%#llx native=%#llx\n",
      (unsigned long long)s,(unsigned long long)(UINTN)legacy,(unsigned long long)(UINTN)Driver);
    if(EFI_ERROR(s))return s;if(!GuLogReady()||!Driver||Driver==legacy)return EFI_SECURITY_VIOLATION;
  }
  EFI_HANDLE drivers[2]={Driver,NULL};
  /* Closing an open record is not a license to retain an old interface. */
  s=GuRefreshTargetPci();if(EFI_ERROR(s))return s;
  UINT16 beforeCommand=0;s=GuConfigRead(EfiPciIoWidthUint16,4,&beforeCommand);if(EFI_ERROR(s))return s;
  GuLog("stage=before_ConnectParentNonRecursive gpu=%#llx driver=%#llx recursive=0 owned=%u detached=%u command=%#x\n",
    (unsigned long long)(UINTN)Gu.handle,(unsigned long long)(UINTN)Driver,Gu.owned,GuScreenDetached,beforeCommand);
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  s=GuNativeEnabled()?GuNativeConnect():Gu.bs->ConnectController(Gu.handle,drivers,NULL,FALSE);
  /* Flush the API return before any protocol access that might itself fail. */
  GuLog("stage=after_ConnectParentNonRecursive status=%#llx gpu=%#llx driver=%#llx recursive=0 owned=%u detached=%u pre_command=%#x\n",
    (unsigned long long)s,(unsigned long long)(UINTN)Gu.handle,(unsigned long long)(UINTN)Driver,Gu.owned,GuScreenDetached,beforeCommand);
  if(EFI_ERROR(s))return s;
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  GuLogText("stage=before_GOP_PCI_refresh\n");if(!GuLogReady())return EFI_DEVICE_ERROR;
  s=GuRefreshTargetPci();
  GuLog("stage=after_GOP_PCI_refresh status=%#llx pci=%#llx\n",(unsigned long long)s,(unsigned long long)(UINTN)Gu.pci);
  if(EFI_ERROR(s))return s;if(!GuLogReady())return EFI_DEVICE_ERROR;
  UINT16 parentCommand=0;s=GuConfigRead(EfiPciIoWidthUint16,4,&parentCommand);
  GuLog("stage=after_parent_PCI_snapshot status=%#llx command=%#x BME=%u owned=%u\n",
    (unsigned long long)s,parentCommand,(parentCommand&4)!=0,Gu.owned);
  if(EFI_ERROR(s))return s;if(!GuLogReady())return EFI_DEVICE_ERROR;
  EFI_HANDLE child=NULL;BOOLEAN ready=FALSE;
  GuSnapshotTake(5);
  GuLogText("stage=before_parent_child_verification\n");if(!GuLogReady())return EFI_DEVICE_ERROR;
  s=GuInspectRestoredDisplay(Table,&child,&ready,FALSE);
  GuLog("stage=after_parent_child_verification status=%#llx child=%#llx consumers_ready=%u\n",
    (unsigned long long)s,(unsigned long long)(UINTN)child,ready);
  if(EFI_ERROR(s))return s;if(!GuLogReady())return EFI_DEVICE_ERROR;
  if(!ready){
    GuLog("stage=before_ConnectDisplayChild child=%#llx parent=%#llx driver_list=NULL recursive=0 owned=%u detached=%u\n",
      (unsigned long long)(UINTN)child,(unsigned long long)(UINTN)Gu.handle,Gu.owned,GuScreenDetached);
    if(!GuLogReady())return EFI_DEVICE_ERROR;
    if(GuNativeEnabled()){s=GuNativeInventory("before_console_connect");if(EFI_ERROR(s))return s;}
    EFI_STATUS connected=Gu.bs->ConnectController(child,NULL,NULL,FALSE);
    GuLog("stage=after_ConnectDisplayChild status=%#llx child=%#llx parent=%#llx recursive=0 owned=%u detached=%u\n",
      (unsigned long long)connected,(unsigned long long)(UINTN)child,(unsigned long long)(UINTN)Gu.handle,Gu.owned,GuScreenDetached);
    if(!GuLogReady())return EFI_DEVICE_ERROR;
    if(GuNativeEnabled()){s=GuNativeInventory("after_console_connect");if(EFI_ERROR(s))return s;}
    EFI_HANDLE actual=NULL;BOOLEAN complete=FALSE;
    s=GuInspectRestoredDisplay(Table,&actual,&complete,TRUE);
    GuLog("stage=after_child_graph_verification status=%#llx child=%#llx expected_child=%#llx ready=%u connect_status=%#llx\n",
      (unsigned long long)s,(unsigned long long)(UINTN)actual,(unsigned long long)(UINTN)child,complete,(unsigned long long)connected);
    if(!GuLogReady())return EFI_DEVICE_ERROR;
    if(EFI_ERROR(connected)&&connected!=EFI_NOT_FOUND)return connected;
    if(EFI_ERROR(s))return s;
    if(!complete||actual!=child)return EFI_NOT_READY;
    if(connected==EFI_NOT_FOUND)GuLogText("child EFI_NOT_FOUND accepted: exact complete ownership graph independently verified\n");
  }else GuLog("stage=display_consumers_already_restored child=%#llx; duplicate connection skipped after exact graph check\n",(unsigned long long)(UINTN)child);
  GuSnapshotTake(6);
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  /* A complete consumer graph alone does not prove a valid display mode.
   * Keep the strict display gate even when notifications connected it early. */
  EFI_HANDLE finalChild=NULL;BOOLEAN finalReady=FALSE;
  s=GuInspectRestoredDisplay(Table,&finalChild,&finalReady,TRUE);
  if(EFI_ERROR(s))return s;
  if(!finalReady||finalChild!=child)return EFI_NOT_READY;
  GuLog("stage=POST_GOP_DMA quiesced=%u allocations=%zu DMA=%zu owned=%u fault=%u safe=%u\n",
    Gu.quiesced,(size_t)Gu.allocations,(size_t)Gu.dma_count,Gu.owned,Gu.fault,GuSafeToReturn());
  if(!GuLogReady()||!Gu.quiesced||Gu.allocations||Gu.dma_count||Gu.owned||Gu.fault||!GuSafeToReturn())return EFI_DEVICE_ERROR;
  /* Child connection may replace interfaces or initiate synchronous notices. */
  s=GuRefreshTargetPci();if(EFI_ERROR(s))return s;
  ExpectDetached=FALSE;
  GuLogText("stage=before_GOP_display_check\n");if(!GuLogReady())return EFI_DEVICE_ERROR;
  BOOLEAN display=DisplayCheck();
  GuLog("stage=after_GOP_display_check matched=%u detached=%u\n",display,GuScreenDetached);
  if(!display||!GuLogReady())return EFI_DEVICE_ERROR;
  s=VerifyRestoredPci("POST_GOP");if(EFI_ERROR(s))return s;
  s=VerifySelectors("POST-GOP");if(EFI_ERROR(s))return s;
  GuLogText("stage=before_StartImage; POST-GOP selectors and PCI BAR/command verified\n");
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  /* Keep console output suppressed until the ownership graph also matches. */
  return EFI_SUCCESS;
}


