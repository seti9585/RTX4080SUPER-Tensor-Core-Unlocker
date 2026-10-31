/* Read-only standard PCI headers. No MMIO, writes, resets or driver dispatch. */
#include "pci_snapshot.h"
#ifdef GU_PCI_SNAPSHOT
#include "app_io.h"
#include "compat/compat.h"
#include <Protocol/PciRootBridgeIo.h>
#define SN 8
#define PN 8
#define PATH_MAX 512
#define RESOURCE_MAX 1474
typedef struct {UINTN segment,bus,device,function,path_len;UINT8 path[PATH_MAX];UINT32 id;UINT8 header;} TARGET;
typedef struct {EFI_STATUS status;UINT32 cfg[PN][16];UINT8 resources[RESOURCE_MAX];UINTN resource_len;BOOLEAN taken;} SNAP;
static TARGET targets[PN];
#ifdef GU_VGA_RESTORE
static UINT64 initial_vga_attributes;
static BOOLEAN vga_restored;
#endif
static UINTN target_count;
static EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *root;
static UINT8 root_path[PATH_MAX];static UINTN root_path_len;
static SNAP snaps[SN];static BOOLEAN invalid,dumped,dump_ok;
static EFI_GUID pci_guid=EFI_PCI_IO_PROTOCOL_GUID,path_guid=EFI_DEVICE_PATH_PROTOCOL_GUID,root_guid=EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_GUID;
static UINTN PathLength(const UINT8 *p){
 if(!p)return 0;
 for(UINTN n=0;n+4<=PATH_MAX;){UINTN z=p[n+2]+((UINTN)p[n+3]<<8);
  if(z<4||z>PATH_MAX-n)return 0;
  if(p[n]==0x7f)return p[n+1]==0xff&&z==4?n+4:0;
  n+=z;
 }return 0;
}
static BOOLEAN Prefix(const UINT8 *a,UINTN an,const UINT8 *b,UINTN bn){return an>=4&&an<=bn&&!memcmp(a,b,an-4);}
static EFI_STATUS Header(const TARGET *t,UINT32 out[16]){
 UINT64 address=((UINT64)t->bus<<24)|((UINT64)t->device<<16)|((UINT64)t->function<<8);
 for(UINTN i=0;i<16;i++){
  EFI_STATUS s=root->Pci.Read(root,EfiPciWidthUint32,address+i*4,1,&out[i]);if(EFI_ERROR(s))return s;
 }return EFI_SUCCESS;
}
/* Configuration() has no length argument. Accept only the specified QWORD
 * descriptors and end tag, with a fixed descriptor/byte budget. */
static EFI_STATUS Resources(UINT8 *out,UINTN *length){
 VOID *buffer=NULL;EFI_STATUS s=root->Configuration(root,&buffer);*length=0;
 if(EFI_ERROR(s))return s;if(!buffer)return EFI_COMPROMISED_DATA;
 const UINT8 *p=buffer;
 for(UINTN n=0;n+2<=RESOURCE_MAX;){
  if(p[n]==0x79){memcpy(out+n,p+n,2);*length=n+2;
   UINT8 sum=0;for(UINTN k=0;k<n+2;k++)sum=(UINT8)(sum+out[k]);
   return !n||(out[n+1]&&sum)?EFI_COMPROMISED_DATA:EFI_SUCCESS;}
  if(n+46>RESOURCE_MAX||p[n]!=0x8a||p[n+1]!=43||p[n+2]!=0)return EFI_COMPROMISED_DATA;
  memcpy(out+n,p+n,46);n+=46;
 }return EFI_BAD_BUFFER_SIZE;
}
static UINT64 U64(const UINT8 *p){UINT64 v=0;for(unsigned i=0;i<8;i++)v|=(UINT64)p[i]<<(8*i);return v;}
static BOOLEAN RootCovers(const UINT8 *p,UINTN len){
 for(UINTN i=0;i<target_count;i++){
  UINTN matches=0;
  for(UINTN n=0;n+46<=len&&p[n]==0x8a;n+=46)
   if(p[n+3]==2&&U64(p+n+14)<=targets[i].bus&&targets[i].bus<=U64(p+n+22))matches++;
  if(matches!=1)return FALSE;
 }return TRUE;
}
VOID GuSnapshotReset(VOID){memset(targets,0,sizeof(targets));memset(snaps,0,sizeof(snaps));root=NULL;target_count=root_path_len=0;invalid=dumped=dump_ok=FALSE;}
EFI_STATUS GuSnapshotInit(VOID){
 EFI_HANDLE handles[256];UINTN bytes=sizeof(handles);EFI_BOOT_SERVICES *bs=Gu.bs;
 UINT8 *gpu_path=NULL;EFI_STATUS s;
 if(!bs||!bs->LocateHandle||!bs->HandleProtocol||!Gu.pci||!Gu.pci->GetLocation)return EFI_UNSUPPORTED;
 s=bs->HandleProtocol(Gu.handle,&path_guid,(VOID**)&gpu_path);if(EFI_ERROR(s))return s;
 UINTN gpu_len=PathLength(gpu_path);if(!gpu_len)return EFI_COMPROMISED_DATA;
 s=bs->LocateHandle(ByProtocol,&pci_guid,NULL,&bytes,handles);if(EFI_ERROR(s))return s;
 if(!bytes||bytes>sizeof(handles)||bytes%sizeof(EFI_HANDLE))return EFI_BAD_BUFFER_SIZE;
 for(UINTN i=0;i<bytes/sizeof(EFI_HANDLE);i++){
  UINT8 *path=NULL;EFI_PCI_IO_PROTOCOL *pci=NULL;
  s=bs->HandleProtocol(handles[i],&path_guid,(VOID**)&path);if(EFI_ERROR(s))return s;
  UINTN len=PathLength(path);if(!len)return EFI_COMPROMISED_DATA;
  if(!Prefix(path,len,gpu_path,gpu_len))continue;
  if(target_count==PN)return EFI_BAD_BUFFER_SIZE;
  s=bs->HandleProtocol(handles[i],&pci_guid,(VOID**)&pci);if(EFI_ERROR(s)||!pci||!pci->GetLocation||!pci->Pci.Read)return EFI_UNSUPPORTED;
  TARGET *t=&targets[target_count++];t->path_len=len;memcpy(t->path,path,len);
  s=pci->GetLocation(pci,&t->segment,&t->bus,&t->device,&t->function);if(EFI_ERROR(s))return s;
  if(t->bus>255||t->device>31||t->function>7)return EFI_COMPROMISED_DATA;
  UINT32 v[16];s=pci->Pci.Read(pci,EfiPciIoWidthUint32,0,16,v);if(EFI_ERROR(s))return s;
  t->id=v[0];t->header=(UINT8)(v[3]>>16)&0x7f;
  if(!t->id||t->id==0xffffffffU||t->header!=(len==gpu_len?0:1))return EFI_COMPROMISED_DATA;
  if(len==gpu_len&&(handles[i]!=Gu.handle||t->id!=GU_BOARD_ID))return EFI_COMPROMISED_DATA;
 }
 if(target_count<2)return EFI_NOT_FOUND;
 for(UINTN i=0;i<target_count;i++)for(UINTN j=i+1;j<target_count;j++)if(targets[j].path_len>targets[i].path_len){TARGET t=targets[i];targets[i]=targets[j];targets[j]=t;}
 if(targets[0].path_len!=gpu_len)return EFI_NOT_FOUND;
 for(UINTN i=1;i<target_count;i++)if(targets[i].segment!=targets[0].segment||targets[i].path_len==targets[i-1].path_len)return EFI_COMPROMISED_DATA;
 bytes=sizeof(handles);s=bs->LocateHandle(ByProtocol,&root_guid,NULL,&bytes,handles);if(EFI_ERROR(s))return s;
 if(!bytes||bytes>sizeof(handles)||bytes%sizeof(EFI_HANDLE))return EFI_BAD_BUFFER_SIZE;
 for(UINTN i=0;i<bytes/sizeof(EFI_HANDLE);i++){
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *r=NULL;UINT8 *path=NULL;
  s=bs->HandleProtocol(handles[i],&root_guid,(VOID**)&r);if(EFI_ERROR(s)||!r)return EFI_UNSUPPORTED;
  if(r->SegmentNumber!=targets[0].segment)continue;
  s=bs->HandleProtocol(handles[i],&path_guid,(VOID**)&path);if(EFI_ERROR(s))return s;
  UINTN len=PathLength(path);if(!len)return EFI_COMPROMISED_DATA;
  if(!Prefix(path,len,targets[target_count-1].path,targets[target_count-1].path_len)||len>=targets[target_count-1].path_len)continue;
  if(root)return EFI_COMPROMISED_DATA;root=r;root_path_len=len;memcpy(root_path,path,len);
 }
 if(!root||!root->Pci.Read||!root->Configuration)return EFI_UNSUPPORTED;
 GuSnapshotTake(0);if(EFI_ERROR(snaps[0].status))return snaps[0].status;
#ifdef GU_VGA_RESTORE
 vga_restored=FALSE;initial_vga_attributes=0;
 if(target_count!=2||targets[0].segment||targets[0].bus!=11||targets[0].device||targets[0].function||
 targets[1].segment||targets[1].bus||targets[1].device!=3||targets[1].function!=1||targets[1].id!=0x14831022||
 (snaps[0].cfg[0][1]&0xffff)!=7||(snaps[0].cfg[1][1]&0xffff)!=7||
 (snaps[0].cfg[1][15]>>16)!=0x18||(snaps[0].cfg[1][7]&0xffff)!=0xf1f1||
 snaps[0].cfg[1][8]!=0xfc00fb00||snaps[0].cfg[1][9]!=0xe1f1d001)return EFI_UNSUPPORTED;
 if(!Gu.pci->Attributes)return EFI_UNSUPPORTED;
 s=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationGet,0,&initial_vga_attributes);
 if(EFI_ERROR(s)||(initial_vga_attributes&0x718)!=0x718)return EFI_UNSUPPORTED;
#endif
 for(UINTN i=1;i<target_count;i++){
  UINT32 buses=snaps[0].cfg[i][6];UINTN primary=buses&255,secondary=(buses>>8)&255,subordinate=(buses>>16)&255;
  if(primary!=targets[i].bus||secondary!=targets[i-1].bus||secondary>subordinate||targets[0].bus>subordinate)return EFI_COMPROMISED_DATA;
 }
 return EFI_SUCCESS;
}
VOID GuSnapshotTake(unsigned stage){
 if(stage>=SN){invalid=TRUE;return;}SNAP *s=&snaps[stage];
 if(s->taken){invalid=TRUE;return;}s->taken=TRUE;s->status=EFI_NOT_READY;
 if(!root||!root->Pci.Read||!root->Configuration)return;
 for(UINTN i=0;i<target_count;i++){
  s->status=Header(&targets[i],s->cfg[i]);if(EFI_ERROR(s->status))return;
  if(s->cfg[i][0]!=targets[i].id||(((s->cfg[i][3]>>16)&0x7f)!=targets[i].header)){s->status=EFI_MEDIA_CHANGED;return;}
  UINT32 again[16];s->status=Header(&targets[i],again);if(EFI_ERROR(s->status))return;
  /* Status bits may change asynchronously; compare standard routing/control. */
  again[1]=(again[1]&0xffff)|(s->cfg[i][1]&0xffff0000U);
  if(memcmp(again,s->cfg[i],sizeof(again))){s->status=EFI_NOT_READY;return;}
 }
 s->status=Resources(s->resources,&s->resource_len);
 if(!EFI_ERROR(s->status)&&!RootCovers(s->resources,s->resource_len))s->status=EFI_COMPROMISED_DATA;
}
BOOLEAN GuSnapshotFlush(BOOLEAN normal){
 if(dumped)return dump_ok&&GuLogReady();dumped=TRUE;
 BOOLEAN complete=!invalid;
 GuLog("PCI-SNAPSHOT schema=1 mode=%s nodes=%zu root_segment=%u\n",normal?"NORMAL":"RUN09",(size_t)target_count,root?root->SegmentNumber:0);
 for(UINTN i=0;i<target_count;i++)GuLog("PCI-TARGET index=%zu segment=%zu bus=%zu device=%zu function=%zu id=%08x header=%u\n",(size_t)i,(size_t)targets[i].segment,(size_t)targets[i].bus,(size_t)targets[i].device,(size_t)targets[i].function,targets[i].id,targets[i].header);
 for(UINTN n=0;n<root_path_len;n++)GuLog("PCI-ROOT-PATH offset=%zu byte=%02x\n",(size_t)n,root_path[n]);
 for(unsigned st=0;st<SN;st++){
  if(normal&&st!=0&&st!=7)continue;SNAP *s=&snaps[st];
  GuLog("PCI-STAGE stage=%u taken=%u status=%#llx\n",st,s->taken,(unsigned long long)s->status);
  if(!s->taken||EFI_ERROR(s->status)){complete=FALSE;continue;}
  for(UINTN i=0;i<target_count;i++)for(UINTN j=0;j<16;j++)GuLog("PCI-REG stage=%u node=%zu offset=%02x value=%08x\n",st,(size_t)i,(unsigned)(j*4),s->cfg[i][j]);
  for(UINTN n=0;n<s->resource_len;n+=16){
   static const char digits[]="0123456789abcdef";char hex[33];UINTN count=s->resource_len-n;if(count>16)count=16;
   for(UINTN k=0;k<count;k++){hex[2*k]=digits[s->resources[n+k]>>4];hex[2*k+1]=digits[s->resources[n+k]&15];}hex[2*count]=0;
   GuLog("PCI-ROOT stage=%u offset=%zu data=%s\n",st,(size_t)n,hex);
  }
 }
 if(!GuLogReady())complete=FALSE;
 GuLog("PCI-END complete=%u expected_stages=%u diagnostic_only=1\n",complete,normal?2:8);
 dump_ok=complete&&GuLogReady();return dump_ok;
}
#ifdef GU_VGA_RESTORE
#include "vga_restore.inc"
#endif
#endif

