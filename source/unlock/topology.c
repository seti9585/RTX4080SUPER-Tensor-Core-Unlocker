#include "port.h"
#include "topology.h"
#include "app_io.h"
#include <Protocol/LoadedImage.h>
#include <Protocol/PciRootBridgeIo.h>
static REV3_NODE Nodes[REV3_TOPOLOGY_MAX];
static EFI_HANDLE Handles[REV3_TOPOLOGY_MAX];
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID,PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID;
static REV3_NODE Baseline[REV3_TOPOLOGY_MAX];
static UINTN BaselineCount;
#ifdef GU_HOST_APP_TEST
#include "tests/app_host_model.h"
VOID GuHostResetTopology(VOID){BaselineCount=0;}
#endif
static EFI_GUID LoadedGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID RootGuid=EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_GUID;
static EFI_STATUS AudioPassive(EFI_BOOT_SERVICES *bs,EFI_HANDLE handle){
 EFI_GUID **protocols=NULL;UINTN count=0;
 if(!bs->ProtocolsPerHandle||!bs->OpenProtocolInformation||!bs->FreePool)return EFI_UNSUPPORTED;
 EFI_STATUS status=bs->ProtocolsPerHandle(handle,&protocols,&count);
 if(!EFI_ERROR(status)&&(!protocols||count>128))status=EFI_COMPROMISED_DATA;
 for(UINTN i=0;!EFI_ERROR(status)&&i<count;i++){
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL;UINTN n=0;
  if(!protocols[i]){status=EFI_COMPROMISED_DATA;break;}
  status=bs->OpenProtocolInformation(handle,protocols[i],&entries,&n);
  if(!EFI_ERROR(status)&&(n>512||(n&&!entries)))status=EFI_COMPROMISED_DATA;
  for(UINTN j=0;!EFI_ERROR(status)&&j<n;j++){
   UINT32 a=entries[j].Attributes;
   if(a!=EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL&&a!=EFI_OPEN_PROTOCOL_GET_PROTOCOL&&a!=EFI_OPEN_PROTOCOL_TEST_PROTOCOL){GuLog("BLOCKED_IDENTITY condition=AUDIO_ACTIVE_OPEN BDF=0:11:0.1 field=attributes actual=%#x expected=passive_1_or_2_or_4\n",a);status=EFI_ACCESS_DENIED;}
  }
  if(entries&&EFI_ERROR(bs->FreePool(entries)))status=EFI_DEVICE_ERROR;
 }
 if(protocols&&EFI_ERROR(bs->FreePool(protocols)))status=EFI_DEVICE_ERROR;
 return status;
}
/* Reconcile all 256 conventional function addresses against PCI IO inventory.
 * Any hidden/unpublished function or failed config read rejects the snapshot. */
static EFI_STATUS FullBusCoverage(EFI_BOOT_SERVICES *bs,EFI_HANDLE image,UINTN count) {
 EFI_HANDLE roots[16];UINTN bytes=sizeof(roots),i,d,f,rootcount=0;
 EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *root=NULL;EFI_STATUS status;
 status=bs->LocateHandle(ByProtocol,&RootGuid,NULL,&bytes,roots);
 if(EFI_ERROR(status)||bytes>sizeof(roots)||bytes%sizeof(EFI_HANDLE))return EFI_UNSUPPORTED;
 for(i=0;i<bytes/sizeof(EFI_HANDLE);i++) {
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *p=NULL;
  status=bs->OpenProtocol(roots[i],&RootGuid,(VOID**)&p,image,NULL,EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  if(EFI_ERROR(status)||!p)return EFI_DEVICE_ERROR;
  if(p->SegmentNumber==0){root=p;rootcount++;}
 }
 if(rootcount!=1||!root||!root->Pci.Read)return EFI_UNSUPPORTED;
 for(d=0;d<32;d++)for(f=0;f<8;f++) {
  UINT32 id=0;UINTN matches=0;
  status=root->Pci.Read(root,EfiPciWidthUint32,((UINT64)11<<24)|((UINT64)d<<16)|((UINT64)f<<8),1,&id);
  if(EFI_ERROR(status))return status;
  for(i=0;i<count;i++)if(Nodes[i].segment==0&&Nodes[i].bus==11&&Nodes[i].device==d&&Nodes[i].function==f){
   matches++;if(Nodes[i].id!=id){GuLog("BLOCKED_IDENTITY condition=BUS_COVERAGE_ID BDF=0:11:%zu.%zu actual=%#x expected=%#x\n",(size_t)d,(size_t)f,id,Nodes[i].id);return EFI_MEDIA_CHANGED;}}
  if(id==0xffffffffU){if(matches)return EFI_MEDIA_CHANGED;}
  else if(!id||matches!=1){GuLog("BLOCKED_IDENTITY condition=BUS_COVERAGE_FUNCTION BDF=0:11:%zu.%zu id=%#x actual_matches=%zu expected=1\n",(size_t)d,(size_t)f,id,(size_t)matches);return EFI_ACCESS_DENIED;}
 }
 return EFI_SUCCESS;
}
EFI_STATUS Rev3TopologyGate(EFI_HANDLE gpu,EFI_HANDLE volume,EFI_HANDLE image) {
 EFI_STATUS status;UINTN bytes=sizeof(Handles),count,i;EFI_HANDLE boot=NULL;
 EFI_DEVICE_PATH_PROTOCOL *path=NULL,*remaining;EFI_BOOT_SERVICES *bs=Gu.bs;
 if(!bs||!bs->LocateHandle||!bs->OpenProtocol||!bs->LocateDevicePath||!volume)return EFI_UNSUPPORTED;
 status=bs->OpenProtocol(volume,&PathGuid,(VOID**)&path,image,NULL,EFI_OPEN_PROTOCOL_GET_PROTOCOL);
 if(EFI_ERROR(status)||!path)return EFI_NOT_FOUND;remaining=path;
 status=bs->LocateDevicePath(&PciGuid,&remaining,&boot);
 if(EFI_ERROR(status)||!boot)return EFI_NOT_FOUND;
 status=bs->LocateHandle(ByProtocol,&PciGuid,NULL,&bytes,Handles);
 if(EFI_ERROR(status)||!bytes||bytes%sizeof(EFI_HANDLE)||bytes>sizeof(Handles))return EFI_BAD_BUFFER_SIZE;
 count=bytes/sizeof(EFI_HANDLE);
 for(i=0;i<count;i++) {
  EFI_PCI_IO_PROTOCOL *pci=NULL;UINT32 config[16];REV3_NODE *n=&Nodes[i];
  status=bs->OpenProtocol(Handles[i],&PciGuid,(VOID**)&pci,image,NULL,EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  if(EFI_ERROR(status)||!pci||!pci->GetLocation||!pci->Pci.Read)return EFI_DEVICE_ERROR;
  status=pci->GetLocation(pci,&n->segment,&n->bus,&n->device,&n->function);if(EFI_ERROR(status))return status;
  status=pci->Pci.Read(pci,EfiPciIoWidthUint32,0,16,config);if(EFI_ERROR(status))return status;
  n->handle=Handles[i];n->id=config[0];n->subsystem=config[11];n->classrev=config[2];n->buses=config[6];
  n->command=(UINT16)config[1];n->control=(UINT16)(config[15]>>16);n->header=(UINT8)(config[3]>>16);
 }
 REV3_REJECTION rejected={0};
 if(!Rev3TopologyExplain(Nodes,count,gpu,boot,&rejected)){
  const REV3_NODE *n=rejected.node;
  GuLog("BLOCKED_IDENTITY condition=%s field=%s actual=%#llx expected=%#llx node_present=%u BDF=%llu:%llu:%llu.%llu\n",rejected.condition,rejected.field,(unsigned long long)rejected.actual,(unsigned long long)rejected.expected,(unsigned)(n!=NULL),(unsigned long long)(n?n->segment:0),(unsigned long long)(n?n->bus:0),(unsigned long long)(n?n->device:0),(unsigned long long)(n?n->function:0));
  for(UINTN j=0;j<count;j++){n=&Nodes[j];GuLog("topology-node BDF=%llu:%llu:%llu.%llu id=%#x subsystem=%#x classrev=%#x header=%#x command=%#x control=%#x buses=%#x gpu=%u boot=%u\n",(unsigned long long)n->segment,(unsigned long long)n->bus,(unsigned long long)n->device,(unsigned long long)n->function,n->id,n->subsystem,n->classrev,n->header,n->command,n->control,n->buses,(unsigned)(n->handle==gpu),(unsigned)(n->handle==boot));}
  return EFI_ACCESS_DENIED;
 }
 for(i=0;i<count;i++)if(Rev4Audio(&Nodes[i])){
  status=AudioPassive(bs,Nodes[i].handle);
  GuLog("audio-protection BDF=0000:0b:00.1 command=%#x BME=%u passive_status=%#llx baseline_recheck=%u audio_writes=0 DMA_idle_proven=0\n",Nodes[i].command,(unsigned)((Nodes[i].command&4)!=0),(unsigned long long)status,(unsigned)(BaselineCount!=0));
  if(EFI_ERROR(status)){GuLogText("BLOCKED_IDENTITY condition=AUDIO_PASSIVE_CHECK expected=no_active_owners_and_readable_protocol_inventory\n");return status;}
  if(!GuLogReady())return EFI_DEVICE_ERROR;
 }
 EFI_LOADED_IMAGE_PROTOCOL *loaded=NULL;EFI_HANDLE logController=NULL;
 status=bs->OpenProtocol(image,&LoadedGuid,(VOID**)&loaded,image,NULL,EFI_OPEN_PROTOCOL_GET_PROTOCOL);
 if(EFI_ERROR(status)||!loaded||!loaded->DeviceHandle)return EFI_NOT_FOUND;
 status=bs->OpenProtocol(loaded->DeviceHandle,&PathGuid,(VOID**)&path,image,NULL,EFI_OPEN_PROTOCOL_GET_PROTOCOL);
 if(EFI_ERROR(status)||!path)return EFI_NOT_FOUND;remaining=path;
 status=bs->LocateDevicePath(&PciGuid,&remaining,&logController);
 if(EFI_ERROR(status)||!logController||logController==gpu)return EFI_ACCESS_DENIED;
 BOOLEAN logSafe=FALSE;for(i=0;i<count;i++)if(Nodes[i].handle==logController){
 UINT32 cls=Nodes[i].classrev>>16;logSafe=(Nodes[i].segment!=0||Nodes[i].bus!=11)&&(cls==0x0c03||((cls>>8)==1&&cls!=0x0104));}
 if(!logSafe)return EFI_ACCESS_DENIED;
 if(BaselineCount){if(count!=BaselineCount)return EFI_MEDIA_CHANGED;
 for(i=0;i<count;i++){UINTN j;for(j=0;j<count;j++)if(Nodes[i].handle==Baseline[j].handle)break;
 if(j==count||Nodes[i].segment!=Baseline[j].segment||Nodes[i].bus!=Baseline[j].bus||Nodes[i].device!=Baseline[j].device||Nodes[i].function!=Baseline[j].function||Nodes[i].id!=Baseline[j].id||Nodes[i].subsystem!=Baseline[j].subsystem||Nodes[i].classrev!=Baseline[j].classrev||Nodes[i].buses!=Baseline[j].buses||Nodes[i].command!=Baseline[j].command||Nodes[i].control!=Baseline[j].control||Nodes[i].header!=Baseline[j].header)return EFI_MEDIA_CHANGED;}}
 status=FullBusCoverage(bs,image,count);
 if(!EFI_ERROR(status)&&!BaselineCount){for(i=0;i<count;i++)Baseline[i]=Nodes[i];BaselineCount=count;}
 return status;
}

