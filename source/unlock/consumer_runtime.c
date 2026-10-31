/* SPDX-License-Identifier: GPL-2.0-only */
#include "consumer_release.h"
#include "runtime.h"
#include "sha256.h"
#include "topology.h"
#include "app_io.h"
#include <Protocol/SimpleTextOut.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <Protocol/EdidDiscovered.h>
#include "run0009_profiles.h"
#include "experimental_mode.h"
#include "native_gop.h"
/* No reference-board GUID/offset is implicitly trusted for BIOS 3607.
 * Reviewed local identities must be supplied before either profile is usable. */
#ifdef GU_HOST_APP_TEST
#include "tests/app_host_model.h"
static const BOOLEAN ConsumerProfilesReviewed=TRUE;
static const BOOLEAN OwnershipGraphReviewed=TRUE;
#elif GU_EXPERIMENTAL_ENABLED
/* Authorizes the reviewed dynamic checks, not a claim of Stop safety. */
static const BOOLEAN ConsumerProfilesReviewed=TRUE;
static const BOOLEAN OwnershipGraphReviewed=TRUE;
#else
static const BOOLEAN ConsumerProfilesReviewed=FALSE;
static const BOOLEAN OwnershipGraphReviewed=FALSE;
#endif
/* RUN0009 measured identifiers. Review/entry gates remain closed. */
static const R4_DRIVER_PROFILE *const Profiles=&Run0009Drivers[1];
static EFI_GUID EdidGuid=EFI_EDID_DISCOVERED_PROTOCOL_GUID;
static EFI_GUID GopGuid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,TextGuid=EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID,
 BindingGuid=EFI_DRIVER_BINDING_PROTOCOL_GUID,LoadedGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
static R4_PLAN Plan;
static R4_GRAPH Observed,Current,RestoredExpected;
static R4_GRAPH RestorePrevious;
static BOOLEAN RestorePreviousValid;
static R4_GRAPH NativeEdges,FirstNativeEdges;
static GU_RELEASE_OWNER FirstNativeOwner;
static BOOLEAN NativeObserved;
static BOOLEAN LegacyIsolated;
static EFI_HANDLE IsolatedLegacyAgent;
static GU_RELEASE_OWNER Parent;
static EFI_SYSTEM_TABLE *Table;
static EFI_HANDLE Volume;
static EFI_HANDLE Console[3];static VOID *Interfaces[3];
static UINT32 Config[64];
static BOOLEAN Prepared;
static BOOLEAN Passive(UINT32 a){return a==1||a==2||a==4;}
#define R4_HANDLE_MAX 2048U
static EFI_STATUS SnapshotFailure(const char *stage,const char *reason,UINTN hc,UINTN hi,EFI_HANDLE h,UINTN pc,UINTN pi,UINTN ec,UINTN edges,UINTN actual,UINTN limit,EFI_STATUS api,EFI_STATUS result){
 GuLog("consumer-snapshot failure_stage=%s reason=%s handles=%zu handle_index=%zu handle=%#llx protocols=%zu protocol_index=%zu opens=%zu edges=%zu actual=%zu limit=%zu api=%#llx result=%#llx\n",stage,reason,(size_t)hc,(size_t)hi,(unsigned long long)(UINTN)h,(size_t)pc,(size_t)pi,(size_t)ec,(size_t)edges,(size_t)actual,(size_t)limit,(unsigned long long)api,(unsigned long long)result);return result;
}
static EFI_STATUS Snapshot(VOID *context,R4_GRAPH *out){
 EFI_BOOT_SERVICES *bs=Gu.bs;EFI_HANDLE *handles=NULL;UINTN hc=0,hi=0,pc=0,pi=0,ec=0;EFI_HANDLE h=NULL;(void)context;out->count=0;
 EFI_STATUS status=bs->LocateHandleBuffer(AllHandles,NULL,NULL,&hc,&handles);
#define REJECT(stage,reason,actual,limit,api,result) SnapshotFailure(stage,reason,hc,hi,h,pc,pi,ec,out->count,actual,limit,api,result)
 if(EFI_ERROR(status))status=REJECT("LocateHandleBuffer","API_FAILURE",hc,R4_HANDLE_MAX,status,status);
 else if(hc>R4_HANDLE_MAX)status=REJECT("LocateHandleBuffer","HANDLE_LIMIT",hc,R4_HANDLE_MAX,EFI_SUCCESS,EFI_BAD_BUFFER_SIZE);
 else if(!handles||!hc)status=REJECT("LocateHandleBuffer","MISSING_HANDLES",hc,R4_HANDLE_MAX,EFI_SUCCESS,EFI_BAD_BUFFER_SIZE);
 for(hi=0;!EFI_ERROR(status)&&hi<hc;hi++){
  EFI_GUID **protocols=NULL;pc=pi=ec=0;h=handles[hi];
  status=bs->ProtocolsPerHandle(h,&protocols,&pc);
  if(EFI_ERROR(status))status=REJECT("ProtocolsPerHandle","API_FAILURE",pc,128,status,status);
  else if(pc>128)status=REJECT("ProtocolsPerHandle","PROTOCOL_LIMIT",pc,128,EFI_SUCCESS,EFI_BAD_BUFFER_SIZE);
  else if(!protocols||!pc)status=REJECT("ProtocolsPerHandle","MISSING_PROTOCOLS",pc,128,EFI_SUCCESS,EFI_BAD_BUFFER_SIZE);
  for(pi=0;!EFI_ERROR(status)&&pi<pc;pi++){
   EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL;ec=0;
   if(!protocols[pi]){status=REJECT("ProtocolsPerHandle","NULL_GUID",pi,pc,EFI_SUCCESS,EFI_COMPROMISED_DATA);break;}
   status=bs->OpenProtocolInformation(h,protocols[pi],&entries,&ec);
   if(EFI_ERROR(status))status=REJECT("OpenProtocolInformation","API_FAILURE",ec,512,status,status);
   else if(ec>512)status=REJECT("OpenProtocolInformation","OPEN_LIMIT",ec,512,EFI_SUCCESS,EFI_BAD_BUFFER_SIZE);
   else if(ec&&!entries)status=REJECT("OpenProtocolInformation","MISSING_ENTRIES",ec,512,EFI_SUCCESS,EFI_BAD_BUFFER_SIZE);
   for(UINTN k=0;!EFI_ERROR(status)&&k<ec;k++){
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e=&entries[k];if(Passive(e->Attributes))continue;
    if(out->count==R4_EDGE_MAX||!e->AgentHandle||!e->OpenCount){status=REJECT("ActiveEdge","LIMIT_OR_INVALID_ENTRY",out->count,R4_EDGE_MAX,EFI_SUCCESS,EFI_COMPROMISED_DATA);break;}
    out->edges[out->count++]=(R4_EDGE){h,e->AgentHandle,e->ControllerHandle,*protocols[pi],e->Attributes,e->OpenCount};
   }
   if(entries){EFI_STATUS f=bs->FreePool(entries);if(EFI_ERROR(f)){GuLog("consumer-snapshot cleanup=entries api=%#llx\n",(unsigned long long)f);if(!EFI_ERROR(status))status=REJECT("FreePool","API_FAILURE",ec,512,f,f);}}
  }
  if(protocols){EFI_STATUS f=bs->FreePool(protocols);if(EFI_ERROR(f)){GuLog("consumer-snapshot cleanup=protocols api=%#llx\n",(unsigned long long)f);if(!EFI_ERROR(status))status=REJECT("FreePool","API_FAILURE",pc,128,f,f);}}
 }
 if(handles){EFI_STATUS f=bs->FreePool(handles);if(EFI_ERROR(f)){GuLog("consumer-snapshot cleanup=handles api=%#llx\n",(unsigned long long)f);if(!EFI_ERROR(status))status=REJECT("FreePool","API_FAILURE",hc,R4_HANDLE_MAX,f,f);}}
#undef REJECT
 return status;
}
static BOOLEAN FileGuidMatches(EFI_DEVICE_PATH_PROTOCOL *p,const EFI_GUID *expected){
 UINTN bytes=0;BOOLEAN found=FALSE;if(!p)return FALSE;
 for(UINTN i=0;i<128&&bytes<=4092;i++){
  EFI_DEVICE_PATH_PROTOCOL *n=(VOID*)((UINT8*)p+bytes);UINTN len=n->Length[0]+((UINTN)n->Length[1]<<8);
  if(len<4||len>4096-bytes)return FALSE;
  if(n->Type==MEDIA_DEVICE_PATH&&n->SubType==MEDIA_PIWG_FW_FILE_DP){
   if(found||len!=4+sizeof(EFI_GUID)||memcmp((UINT8*)n+4,expected,sizeof(*expected)))return FALSE;found=TRUE;
  }
  if(n->Type==END_DEVICE_PATH_TYPE)return found&&n->SubType==END_ENTIRE_DEVICE_PATH_SUBTYPE&&len==4;
  bytes+=len;
 }
 return FALSE;
}
static EFI_STATUS Driver(EFI_HANDLE agent,UINTN role){
 EFI_DRIVER_BINDING_PROTOCOL *binding=NULL;EFI_LOADED_IMAGE_PROTOCOL *image=NULL;UINT8 digest[32];
 if(!ConsumerProfilesReviewed||role>3)return EFI_NOT_READY;
 const R4_DRIVER_PROFILE *p=role==3?&Run0009Drivers[0]:&Profiles[role];
#if defined(GU_HOST_APP_TEST) && !defined(GU_MEASURED_PROFILE_TEST)
 R4_DRIVER_PROFILE model;
 EFI_STATUS supplied=GuHostConsumerProfile(role,&model.file,&model.version,&model.size,&model.stop,model.stop_hash,&model.code_type);
 if(EFI_ERROR(supplied))return supplied;p=&model;
#endif
 EFI_STATUS status=Gu.bs->HandleProtocol(agent,&BindingGuid,(VOID**)&binding);
 if(EFI_ERROR(status)||!binding||!binding->Stop||binding->DriverBindingHandle!=agent||!binding->ImageHandle||binding->ImageHandle==Gu.image)return EFI_UNSUPPORTED;
 status=Gu.bs->HandleProtocol(binding->ImageHandle,&LoadedGuid,(VOID**)&image);
 if(EFI_ERROR(status)||!image||!image->ImageBase||image->ImageCodeType!=p->code_type||
  image->ImageSize!=p->size||p->size>64U*1024U*1024U||p->size<64||p->stop>p->size-64||
  (UINTN)image->ImageBase>MAX_UINTN-(UINTN)p->size||binding->Version!=p->version||
  (UINTN)binding->Stop!=(UINTN)image->ImageBase+(UINTN)p->stop||!FileGuidMatches(image->FilePath,&p->file))return EFI_UNSUPPORTED;
 GuSha256((UINT8*)image->ImageBase+p->stop,64,digest);
 return memcmp(digest,p->stop_hash,32)?EFI_SECURITY_VIOLATION:EFI_SUCCESS;
}
static EFI_STATUS ReadConfig(UINT32 *config){
 for(UINTN i=0;i<64;i++){EFI_STATUS status=GuConfigRead(EfiPciIoWidthUint32,(UINT32)i*4,&config[i]);if(EFI_ERROR(status))return status;}
 return EFI_SUCCESS;
}
/* Bounded public-protocol/RAM evidence only. No Driver Start/Stop or PCI writes.
 * This logger does not approve a profile and does not bypass the EFI entry lock. */
EFI_STATUS GuRecordOwnershipEvidence(EFI_SYSTEM_TABLE *table,const GU_RELEASE_OWNER *owner){
 if(!table||!owner)return EFI_INVALID_PARAMETER;
 EFI_STATUS status=Snapshot(NULL,&Current);if(EFI_ERROR(status))return status;
 GuLog("ownership-evidence-only console=%#llx stderr=%#llx input=%#llx conout=%#llx err=%#llx conin=%#llx\n",
  (unsigned long long)(UINTN)table->ConsoleOutHandle,(unsigned long long)(UINTN)table->StandardErrorHandle,
  (unsigned long long)(UINTN)table->ConsoleInHandle,(unsigned long long)(UINTN)table->ConOut,
  (unsigned long long)(UINTN)table->StdErr,(unsigned long long)(UINTN)table->ConIn);
 EFI_HANDLE agents[16];UINTN count=0;
 for(UINTN i=0;i<Current.count;i++){
  R4_EDGE *e=&Current.edges[i];EFI_GUID *g=&e->protocol;
  GuLog("active-edge handle=%#llx agent=%#llx controller=%#llx attrs=%#x count=%u guid=%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
   (unsigned long long)(UINTN)e->handle,(unsigned long long)(UINTN)e->agent,(unsigned long long)(UINTN)e->controller,e->attributes,e->count,
   g->Data1,g->Data2,g->Data3,g->Data4[0],g->Data4[1],g->Data4[2],g->Data4[3],g->Data4[4],g->Data4[5],g->Data4[6],g->Data4[7]);
  BOOLEAN child=FALSE;for(UINTN j=0;j<owner->child_count;j++)if(owner->children[j].handle==e->handle)child=TRUE;
  if(!child)continue;BOOLEAN seen=FALSE;for(UINTN j=0;j<count;j++)if(agents[j]==e->agent)seen=TRUE;
  if(!seen){if(count==16)return EFI_BAD_BUFFER_SIZE;agents[count++]=e->agent;}
 }
 for(UINTN i=0;i<count;i++){
  EFI_DRIVER_BINDING_PROTOCOL *binding=NULL;EFI_LOADED_IMAGE_PROTOCOL *image=NULL;
  status=Gu.bs->HandleProtocol(agents[i],&BindingGuid,(VOID**)&binding);
  GuLog("consumer-binding agent=%#llx status=%#llx\n",(unsigned long long)(UINTN)agents[i],(unsigned long long)status);
  if(EFI_ERROR(status)||!binding)continue;
  status=Gu.bs->HandleProtocol(binding->ImageHandle,&LoadedGuid,(VOID**)&image);
  GuLog("consumer-image binding=%#llx image=%#llx version=%#x query=%#llx\n",(unsigned long long)(UINTN)binding->DriverBindingHandle,(unsigned long long)(UINTN)binding->ImageHandle,binding->Version,(unsigned long long)status);
  if(EFI_ERROR(status)||!image)continue;
  UINTN base=(UINTN)image->ImageBase,stop=(UINTN)binding->Stop;
  GuLog("consumer-live base=%#llx size=%#llx code_type=%u stop=%#llx filepath=%#llx\n",(unsigned long long)base,(unsigned long long)image->ImageSize,(unsigned)image->ImageCodeType,(unsigned long long)stop,(unsigned long long)(UINTN)image->FilePath);
  if(!base||image->ImageSize<64||image->ImageSize>64U*1024U*1024U||base>MAX_UINTN-image->ImageSize||stop<base||stop-base>image->ImageSize-64||
   (image->ImageCodeType!=EfiBootServicesCode&&image->ImageCodeType!=EfiRuntimeServicesCode))continue;
  UINT8 digest[32];char hex[65];static const char digits[]="0123456789abcdef";GuSha256((VOID*)stop,64,digest);
  for(UINTN j=0;j<32;j++){hex[2*j]=digits[digest[j]>>4];hex[2*j+1]=digits[digest[j]&15];}hex[64]=0;
  GuLog("consumer-stop rva=%#llx bytes=64 sha256=%s\n",(unsigned long long)(stop-base),hex);
  for(UINTN role=0;role<3;role++)GuLog("consumer-file role=%u guid_match=%u\n",(unsigned)role,(unsigned)FileGuidMatches(image->FilePath,&Profiles[role].file));
 }
 return GuLogReady()?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
static EFI_STATUS Validate(VOID *context,const R4_PLAN *plan,UINTN stage){
 (void)context;(void)stage;UINT32 now[64];
 if(!Table||Table->ConsoleOutHandle!=Console[0]||Table->StandardErrorHandle!=Console[1]||Table->ConsoleInHandle!=Console[2]||
  Table->ConOut!=Interfaces[0]||Table->StdErr!=Interfaces[1]||Table->ConIn!=Interfaces[2])return EFI_MEDIA_CHANGED;
 EFI_STATUS status=ReadConfig(now);if(EFI_ERROR(status))return status;
 if(memcmp(Config,now,sizeof(Config)))return EFI_MEDIA_CHANGED;
 status=Rev3TopologyGate(Gu.handle,Volume,Gu.image);if(EFI_ERROR(status))return status;
 for(UINTN i=0;i<plan->steps;i++){status=Driver(plan->step[i].agent,i);if(EFI_ERROR(status))return status;}
 return EFI_SUCCESS;
}
static EFI_STATUS Note(VOID *ctx,const char *event,UINTN stage,EFI_STATUS status){
 (void)ctx;GuLog("%s stage=%zu status=%#llx\n",event,(size_t)stage,(unsigned long long)status);
 return GuLogReady()?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
static EFI_STATUS Disconnect(VOID *ctx,EFI_HANDLE target,EFI_HANDLE agent){
 (void)ctx;if(target==Gu.handle||!target||!agent)return EFI_ACCESS_DENIED;
 return Gu.bs->DisconnectController(target,agent,NULL);
}
static EFI_STATUS TextState(VOID *ctx,EFI_HANDLE child,BOOLEAN present){
 (void)ctx;VOID *text=NULL;EFI_STATUS status=Gu.bs->HandleProtocol(child,&TextGuid,&text);
 if(present)return !EFI_ERROR(status)&&text?EFI_SUCCESS:EFI_NOT_READY;
 return (status==EFI_UNSUPPORTED||status==EFI_NOT_FOUND)&&!text?EFI_SUCCESS:EFI_NOT_READY;
}
EFI_STATUS GuPrepareConsumerRelease(EFI_SYSTEM_TABLE *table,const GU_RELEASE_OWNER *owner,EFI_HANDLE volume){
 Prepared=FALSE;RestorePreviousValid=FALSE;NativeObserved=FALSE;LegacyIsolated=FALSE;IsolatedLegacyAgent=NULL;memset(&Plan,0,sizeof(Plan));if(!table||!owner||!volume)return EFI_INVALID_PARAMETER;
 Table=table;Volume=volume;Parent=*owner;
 Console[0]=table->ConsoleOutHandle;Console[1]=table->StandardErrorHandle;Console[2]=table->ConsoleInHandle;
 Interfaces[0]=table->ConOut;Interfaces[1]=table->StdErr;Interfaces[2]=table->ConIn;
 EFI_STATUS status=Snapshot(NULL,&Observed);if(EFI_ERROR(status))return status;
 EFI_HANDLE child=NULL,out=NULL,gfx=NULL,edid=NULL;UINTN active=0;
 for(UINTN i=0;i<owner->child_count;i++){
  if(child&&child!=owner->children[i].handle){GuLogText("BLOCKED_OWNERSHIP: unreviewed multi-child consumer graph\n");return EFI_UNSUPPORTED;}
  child=owner->children[i].handle;
 }
 if(!child)return EFI_NOT_FOUND;
 for(UINTN i=0;i<Observed.count;i++)if(Observed.edges[i].handle==child){
  R4_EDGE *e=&Observed.edges[i];active++;
  GuLog("consumer observed protocol=%08x agent=%#llx attributes=%#x controller=%#llx\n",e->protocol.Data1,(unsigned long long)(UINTN)e->agent,e->attributes,(unsigned long long)(UINTN)e->controller);
  if(!memcmp(&e->protocol,&TextGuid,sizeof(TextGuid))&&e->attributes==EFI_OPEN_PROTOCOL_BY_DRIVER)out=e->agent;
  if(!memcmp(&e->protocol,&GopGuid,sizeof(GopGuid))&&e->attributes==EFI_OPEN_PROTOCOL_BY_DRIVER)gfx=e->agent;
  if(!memcmp(&e->protocol,&EdidGuid,sizeof(EdidGuid))&&e->attributes==EFI_OPEN_PROTOCOL_BY_DRIVER)edid=e->agent;
 }
 status=ReadConfig(Config);if(EFI_ERROR(status))return status;
 #if GU_EXPERIMENTAL_ENABLED || defined(GU_MEASURED_PROFILE_TEST)
 EFI_STATUS parent_profile=Driver(owner->agent,3);if(EFI_ERROR(parent_profile))return parent_profile;
#endif
 if(!OwnershipGraphReviewed){GuLogText("BLOCKED_OWNERSHIP: local ownership graph has not been reviewed\n");return EFI_NOT_READY;}
 if(!active){Plan.gpu=Gu.handle;Plan.child=child;Plan.original=Observed;Plan.ready=TRUE;Prepared=TRUE;return EFI_SUCCESS;}
 if(!ConsumerProfilesReviewed){GuLogText("BLOCKED_OWNERSHIP: consumer identities and graph are not reviewed for BIOS 3607\n");return EFI_NOT_READY;}
 status=Driver(out,0);if(!EFI_ERROR(status))status=Driver(gfx,1);if(EFI_ERROR(status))return status;
 status=Driver(edid,2);if(EFI_ERROR(status))return status;
 status=R4BuildEdid(&Plan,&Observed,Gu.handle,child,table->ConsoleOutHandle,out,gfx,edid,TRUE);
 if(!EFI_ERROR(status))Prepared=TRUE;return status;
}
EFI_STATUS GuReleaseConsumers(BOOLEAN *attempted){
 if(!attempted)return EFI_INVALID_PARAMETER;*attempted=FALSE;
 if(!Prepared)return EFI_NOT_READY;
 R4_OPS ops={NULL,Snapshot,Validate,Note,Disconnect,TextState};
 EFI_STATUS status=R4Execute(&Plan,&ops);*attempted=Plan.attempted;return status;
}
static BOOLEAN SameParent(const GU_RELEASE_OWNER *a,const GU_RELEASE_OWNER *b){
 GU_RELEASE_OWNER x=*a,y=*b;
 for(UINTN i=0;i<x.child_count;i++)memset(x.children[i].consumer_sha256,0,32);
 for(UINTN i=0;i<y.child_count;i++)memset(y.children[i].consumer_sha256,0,32);
 return GuSameReleaseOwner(&x,&y);
}
EFI_STATUS GuConsumerParentReady(EFI_SYSTEM_TABLE *table,GU_RELEASE_OWNER *fresh){
 if(!Prepared||!Plan.finished)return EFI_NOT_READY;
#if GU_EXPERIMENTAL_ENABLED || defined(GU_MEASURED_PROFILE_TEST)
 EFI_STATUS parent_profile=Driver(Parent.agent,3);if(EFI_ERROR(parent_profile))return parent_profile;
#endif
 EFI_STATUS status=Validate(NULL,&Plan,Plan.steps);if(EFI_ERROR(status))return status;
 status=Snapshot(NULL,&Current);if(EFI_ERROR(status))return status;
 if(!R4Expected(&Plan,&Current,Plan.steps))return EFI_MEDIA_CHANGED;
 status=GuFindReleaseOwner(table,fresh,FALSE);if(EFI_ERROR(status))return status;
 return SameParent(&Parent,fresh)?EFI_SUCCESS:EFI_MEDIA_CHANGED;
}
EFI_STATUS GuLegacyIsolationCheck(VOID){
 if(!LegacyIsolated||!IsolatedLegacyAgent)return EFI_NOT_READY;
 VOID *binding=NULL;EFI_STATUS s=Gu.bs->HandleProtocol(IsolatedLegacyAgent,&BindingGuid,&binding);
 if(binding||(s!=EFI_NOT_FOUND&&s!=EFI_UNSUPPORTED))return EFI_ACCESS_DENIED;
 EFI_HANDLE *handles=NULL;UINTN count=0;
 s=Gu.bs->LocateHandleBuffer(ByProtocol,&BindingGuid,NULL,&count,&handles);
 if(!EFI_ERROR(s)&&(!handles||!count||count>2048))s=EFI_COMPROMISED_DATA;
 for(UINTN i=0;!EFI_ERROR(s)&&i<count;i++){
  EFI_DRIVER_BINDING_PROTOCOL *b=NULL;s=Gu.bs->HandleProtocol(handles[i],&BindingGuid,(VOID**)&b);
  if(!EFI_ERROR(s)&&(!b||b->ImageHandle==Parent.image))s=EFI_ACCESS_DENIED;
 }
 GuFree(handles);return s;
}
static EFI_STATUS IsolateStoppedBinding(EFI_DRIVER_BINDING_PROTOCOL *binding){
 /* Protocol-handler services are legal at TPL_NOTIFY. Keep the final open
  * check and uninstall in one interval without event callbacks or file I/O.
  * With only passive opens, standard uninstall has no driver to disconnect
  * and no remaining active open that could trigger its reconnect path. */
 EFI_TPL previous=Gu.bs->RaiseTPL(TPL_NOTIFY);
 EFI_STATUS s=previous==TPL_APPLICATION?EFI_SUCCESS:EFI_UNSUPPORTED;
 EFI_DRIVER_BINDING_PROTOCOL *fresh=NULL;
 if(!EFI_ERROR(s))s=Gu.bs->HandleProtocol(Parent.agent,&BindingGuid,(VOID**)&fresh);
 if(!EFI_ERROR(s)&&fresh!=binding)s=EFI_MEDIA_CHANGED;
 EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL;UINTN count=0;
 if(!EFI_ERROR(s))s=Gu.bs->OpenProtocolInformation(Parent.agent,&BindingGuid,&entries,&count);
 if(!EFI_ERROR(s)&&(count>512||(count&&!entries)))s=EFI_COMPROMISED_DATA;
 for(UINTN i=0;!EFI_ERROR(s)&&i<count;i++)if(!Passive(entries[i].Attributes))s=EFI_ACCESS_DENIED;
 if(entries){EFI_STATUS freed=Gu.bs->FreePool(entries);if(EFI_ERROR(freed))s=freed;}
 if(!EFI_ERROR(s))s=Gu.bs->UninstallProtocolInterface(Parent.agent,&BindingGuid,binding);
 Gu.bs->RestoreTPL(previous);return s;
}
EFI_STATUS GuIsolateLegacyBinding(VOID){
 /* Standard driver selection uses the protocol database. Remove only the
  * reviewed, fully stopped binding before publishing the native image.
  * Do not restore it on failure: that could trigger the forbidden fallback. */
 if(!Prepared||!Plan.finished||LegacyIsolated||!GuNativeEnabled()||!Gu.owned||
    !Gu.quiesced||Gu.flrs!=1||Gu.allocations||Gu.dma_count||Gu.fault||!Gu.bs->UninstallProtocolInterface)return EFI_NOT_READY;
 EFI_STATUS s=Snapshot(NULL,&Current);if(EFI_ERROR(s))return s;
 EFI_HANDLE *bindings=NULL;UINTN bindingCount=0;
 s=Gu.bs->LocateHandleBuffer(ByProtocol,&BindingGuid,NULL,&bindingCount,&bindings);
 if(!EFI_ERROR(s)&&(!bindings||!bindingCount||bindingCount>2048))s=EFI_COMPROMISED_DATA;
 for(UINTN i=0;!EFI_ERROR(s)&&i<bindingCount;i++){
  EFI_DRIVER_BINDING_PROTOCOL *b=NULL;s=Gu.bs->HandleProtocol(bindings[i],&BindingGuid,(VOID**)&b);
  if(!EFI_ERROR(s)&&(!b||(b->ImageHandle==Parent.image&&bindings[i]!=Parent.agent)))s=EFI_ACCESS_DENIED;
 }
 GuFree(bindings);if(EFI_ERROR(s))return s;
 for(UINTN i=0;i<Current.count;i++){
  const R4_EDGE *e=&Current.edges[i];
  if(e->agent==Parent.agent||e->handle==Parent.agent||e->controller==Parent.agent){
   GuLogText("legacy isolation refused: binding still has an active reference or managed controller\n");return EFI_ACCESS_DENIED;
  }
 }
#if GU_EXPERIMENTAL_ENABLED || defined(GU_MEASURED_PROFILE_TEST)
 s=Driver(Parent.agent,3);if(EFI_ERROR(s))return s;
#endif
 EFI_DRIVER_BINDING_PROTOCOL *binding=NULL;
 s=Gu.bs->HandleProtocol(Parent.agent,&BindingGuid,(VOID**)&binding);
 if(EFI_ERROR(s)||!binding||binding->DriverBindingHandle!=Parent.agent||binding->ImageHandle!=Parent.image||
    binding->Version!=Parent.version||binding->Stop!=Parent.stop)return EFI_SECURITY_VIOLATION;
 GuLog("stage=before_LegacyBindingIsolation agent=%#llx image=%#llx active_refs=0 owned=%u\n",
  (unsigned long long)(UINTN)Parent.agent,(unsigned long long)(UINTN)Parent.image,Gu.owned);
 if(!GuLogReady())return EFI_DEVICE_ERROR;
 s=IsolateStoppedBinding(binding);
 /* An error may follow partial application. Never reinstall or retry here. */
 if(!EFI_ERROR(s)){IsolatedLegacyAgent=Parent.agent;LegacyIsolated=TRUE;}
 GuLog("stage=after_LegacyBindingIsolation status=%#llx isolated=%u\n",(unsigned long long)s,LegacyIsolated);
 if(EFI_ERROR(s))return s;if(!GuLogReady())return EFI_DEVICE_ERROR;
 s=GuLegacyIsolationCheck();if(EFI_ERROR(s))return s;
 /* Observed is scratch after release: Plan.original retains the baseline.
  * Removing a stopped binding must not alter any active ownership edge. */
 s=Snapshot(NULL,&Observed);if(EFI_ERROR(s))return s;
 if(Current.count!=Observed.count)return EFI_MEDIA_CHANGED;
 for(UINTN i=0;i<Current.count;i++){
  UINTN matches=0;for(UINTN j=0;j<Observed.count;j++)if(R4EdgeEqual(&Current.edges[i],&Observed.edges[j]))matches++;
  if(matches!=1)return EFI_MEDIA_CHANGED;
 }
 GuLogText("stage=legacy_isolation_graph_verified; all active relationships unchanged; no runtime legacy reinstall\n");
 return GuLogReady()?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
static VOID RestoreEdgeLog(const char *event,const R4_EDGE *e){
 const EFI_GUID *g=&e->protocol;
 GuLog("restore-graph %s handle=%#llx agent=%#llx controller=%#llx attrs=%#x count=%u guid=%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
  event,(unsigned long long)(UINTN)e->handle,(unsigned long long)(UINTN)e->agent,(unsigned long long)(UINTN)e->controller,e->attributes,e->count,
  g->Data1,g->Data2,g->Data3,g->Data4[0],g->Data4[1],g->Data4[2],g->Data4[3],g->Data4[4],g->Data4[5],g->Data4[6],g->Data4[7]);
}
static BOOLEAN HasEdge(const R4_GRAPH *graph,const R4_EDGE *edge){
 for(UINTN i=0;i<graph->count;i++)if(R4EdgeEqual(&graph->edges[i],edge))return TRUE;
 return FALSE;
}
static EFI_STATUS RestoreReject(const char *reason,EFI_STATUS status){
 GuLog("restore-verification reason=%s status=%#llx\n",reason,(unsigned long long)status);return status;
}
EFI_STATUS GuInspectRestoredDisplay(EFI_SYSTEM_TABLE *table,EFI_HANDLE *outChild,BOOLEAN *ready,BOOLEAN requireDisplay){
 if(!outChild||!ready)return EFI_INVALID_PARAMETER;*outChild=NULL;*ready=FALSE;
 if(!Prepared||!Plan.finished||!table)return EFI_NOT_READY;
 /* Record parent-only GOP publication without treating it as a valid child. */
 VOID *parentGop=NULL;EFI_STATUS parentGopStatus=Gu.bs->HandleProtocol(Gu.handle,&GopGuid,&parentGop);
 GuLog("restore-GOP-location parent=%#llx parent_gop_status=%#llx parent_gop=%#llx\n",
  (unsigned long long)(UINTN)Gu.handle,(unsigned long long)parentGopStatus,(unsigned long long)(UINTN)parentGop);
 BOOLEAN native=GuNativeEnabled();
 if(native){EFI_STATUS isolated=GuLegacyIsolationCheck();if(EFI_ERROR(isolated))return RestoreReject("LEGACY_BINDING_NOT_ISOLATED",isolated);}
 GU_RELEASE_OWNER fresh;EFI_STATUS status=native?GuNativeInspect(&fresh,&NativeEdges,requireDisplay):GuFindReleaseOwner(table,&fresh,TRUE);
 if(EFI_ERROR(status))return RestoreReject("PARENT_OR_CHILD_PROTOCOL",status);
 if(!native&&(fresh.agent!=Parent.agent||fresh.image!=Parent.image||fresh.stop!=Parent.stop||fresh.base!=Parent.base||
  fresh.size!=Parent.size||fresh.version!=Parent.version))return RestoreReject("PARENT_IDENTITY",EFI_MEDIA_CHANGED);
 if(native){
  if(fresh.agent==Parent.agent)return RestoreReject("LEGACY_PARENT_REBOUND",EFI_ACCESS_DENIED);
  if(NativeObserved){
   /* Driver/ancestry is identity; framebuffer geometry is mutable SetMode
    * state. GuNativeInspect checks it strictly in the final phase. */
   GU_RELEASE_OWNER a=FirstNativeOwner,b=fresh;
   for(UINTN i=0;i<a.child_count;i++){a.children[i].framebuffer=0;a.children[i].framebuffer_size=0;}
   for(UINTN i=0;i<b.child_count;i++){b.children[i].framebuffer=0;b.children[i].framebuffer_size=0;}
   if(!GuSameReleaseOwner(&a,&b)||NativeEdges.count!=FirstNativeEdges.count)return RestoreReject("NATIVE_PARENT_CHANGED",EFI_MEDIA_CHANGED);
   for(UINTN i=0;i<NativeEdges.count;i++)if(!HasEdge(&FirstNativeEdges,&NativeEdges.edges[i]))return RestoreReject("NATIVE_EDGES_CHANGED",EFI_MEDIA_CHANGED);
  }else {FirstNativeOwner=fresh;FirstNativeEdges=NativeEdges;NativeObserved=TRUE;}
 }
#if GU_EXPERIMENTAL_ENABLED || defined(GU_MEASURED_PROFILE_TEST)
 if(!native){status=Driver(fresh.agent,3);if(EFI_ERROR(status))return RestoreReject("PARENT_PROFILE",status);}
#endif
 GuLog("stage=parent_binding_verified parent=%#llx agent=%#llx image=%#llx version=%#x stop=%#llx\n",
  (unsigned long long)(UINTN)Gu.handle,(unsigned long long)(UINTN)fresh.agent,(unsigned long long)(UINTN)fresh.image,
  fresh.version,(unsigned long long)(UINTN)fresh.stop);
 if(!fresh.child_count||(native?fresh.child_count!=1:fresh.child_count!=Parent.child_count))return RestoreReject("CHILD_COUNT",EFI_NOT_FOUND);
 EFI_HANDLE child=NULL;
 for(UINTN i=0;i<fresh.child_count;i++){
  const GU_DISPLAY_CHILD *c=&fresh.children[i];UINTN matches=0;
  for(UINTN j=0;j<Parent.child_count;j++)if(!memcmp(&c->protocol,&Parent.children[j].protocol,sizeof(EFI_GUID))&&
    !memcmp(c->path_sha256,Parent.children[j].path_sha256,32))matches++;
  if(!native&&matches!=1)return RestoreReject("CHILD_PATH_OR_PROTOCOL",EFI_MEDIA_CHANGED);
  if(child&&child!=c->handle)return RestoreReject("AMBIGUOUS_CHILD",EFI_UNSUPPORTED);child=c->handle;
 }
 if(!child||child==Gu.handle||child==Gu.image)return RestoreReject("CHILD_HANDLE",EFI_UNSUPPORTED);
 char pathHash[65];static const char digits[]="0123456789abcdef";
 for(UINTN i=0;i<32;i++){pathHash[2*i]=digits[fresh.children[0].path_sha256[i]>>4];pathHash[2*i+1]=digits[fresh.children[0].path_sha256[i]&15];}pathHash[64]=0;
 GuLog("stage=new_display_child_verified parent=%#llx child=%#llx old_child=%#llx handle_reused=%u path_sha256=%s GOP=%#llx gop_guid=%08x\n",
  (unsigned long long)(UINTN)Gu.handle,(unsigned long long)(UINTN)child,(unsigned long long)(UINTN)Plan.child,
  child==Plan.child,pathHash,(unsigned long long)(UINTN)fresh.children[0].gop,GopGuid.Data1);
 BOOLEAN optional[R4_EDGE_MAX];memset(optional,0,sizeof(optional));
 RestoredExpected.count=0;
 for(UINTN i=0;i<Plan.original.count;i++){
  R4_EDGE e=Plan.original.edges[i];
  /* Replace only this GPU's old parent edges. No unrelated agent, shared
   * console edge, or input edge is remapped to the newly loaded driver. */
  if(native&&e.handle==Gu.handle&&e.agent==Parent.agent)continue;
  if(e.handle==Plan.child)e.handle=child;if(e.controller==Plan.child)e.controller=child;
  UINTN j=RestoredExpected.count;RestoredExpected.edges[j]=e;RestoredExpected.count++;
  for(UINTN s=0;s<Plan.steps;s++)for(UINTN k=0;k<Plan.step[s].removed_count;k++)if(Plan.step[s].removed[k]==i)optional[j]=TRUE;
 }
 if(native){
  if(NativeEdges.count>R4_EDGE_MAX-RestoredExpected.count)return EFI_BAD_BUFFER_SIZE;
  for(UINTN i=0;i<NativeEdges.count;i++)RestoredExpected.edges[RestoredExpected.count++]=NativeEdges.edges[i];
 }
 status=Snapshot(NULL,&Current);if(EFI_ERROR(status))return RestoreReject("GLOBAL_SNAPSHOT",status);
 if(RestorePreviousValid){
  for(UINTN i=0;i<Current.count;i++)if(!HasEdge(&RestorePrevious,&Current.edges[i]))RestoreEdgeLog("added",&Current.edges[i]);
  for(UINTN i=0;i<RestorePrevious.count;i++)if(!HasEdge(&Current,&RestorePrevious.edges[i]))RestoreEdgeLog("removed",&RestorePrevious.edges[i]);
 }
 RestorePrevious=Current;RestorePreviousValid=TRUE;
 for(UINTN s=0;s<Plan.steps;s++){
  status=Driver(Plan.step[s].agent,s);if(EFI_ERROR(status))return RestoreReject("CONSUMER_PROFILE",status);
 }
 /* Only the previously released target edges may still be absent. All other
  * original edges must exist exactly once; no new active edge is approved. */
 UINTN present=0,required=0;
 for(UINTN i=0;i<Current.count;i++){
  UINTN matches=0;for(UINTN j=0;j<RestoredExpected.count;j++)if(R4EdgeEqual(&Current.edges[i],&RestoredExpected.edges[j]))matches++;
  for(UINTN j=0;j<i;j++)if(R4EdgeEqual(&Current.edges[i],&Current.edges[j]))matches++;
  if(matches!=1){RestoreEdgeLog("unexpected",&Current.edges[i]);return RestoreReject("UNEXPECTED_GRAPH_EDGE",EFI_MEDIA_CHANGED);}
 }
 for(UINTN i=0;i<RestoredExpected.count;i++){
  BOOLEAN exists=HasEdge(&Current,&RestoredExpected.edges[i]);
  if(optional[i]){required++;if(exists)present++;RestoreEdgeLog(exists?"consumer_present":"consumer_missing",&RestoredExpected.edges[i]);}
  else if(!exists){RestoreEdgeLog("required_missing",&RestoredExpected.edges[i]);return RestoreReject("REQUIRED_GRAPH_EDGE",EFI_MEDIA_CHANGED);}
 }
 if(Table->ConsoleOutHandle!=Console[0]||Table->StandardErrorHandle!=Console[1]||Table->ConsoleInHandle!=Console[2]||
  Table->ConOut!=Interfaces[0]||Table->StdErr!=Interfaces[1]||Table->ConIn!=Interfaces[2])return RestoreReject("CONSOLE_IDENTITY",EFI_MEDIA_CHANGED);
 *outChild=child;*ready=present==required;
 GuLog("restore-graph verified child=%#llx edges=%zu expected_complete=%zu consumer_edges=%zu/%zu ready=%u\n",
  (unsigned long long)(UINTN)child,(size_t)Current.count,(size_t)RestoredExpected.count,(size_t)present,(size_t)required,*ready);
 return GuLogReady()?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
EFI_STATUS GuVerifyConsumersRestored(EFI_SYSTEM_TABLE *table){
 EFI_HANDLE child=NULL;BOOLEAN ready=FALSE;
 EFI_STATUS status=GuInspectRestoredDisplay(table,&child,&ready,TRUE);
 return EFI_ERROR(status)?status:ready?EFI_SUCCESS:RestoreReject("CONSUMERS_INCOMPLETE",EFI_NOT_READY);
}

