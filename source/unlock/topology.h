#ifndef REV3_TOPOLOGY_H
#define REV3_TOPOLOGY_H
#include <Uefi.h>
#define REV3_TOPOLOGY_MAX 512
typedef struct {
 UINTN segment,bus,device,function;
 UINT32 id,subsystem,classrev,buses;
 UINT16 command,control;
 UINT8 header;
 EFI_HANDLE handle;
} REV3_NODE;
/* IDs/BDF from 2026-10-28 Windows PnP/storage evidence; each UEFI snapshot
 * must independently match. Audio identity, initial COMMAND policy and active ownership are independent. */
static inline BOOLEAN Rev4Audio(const REV3_NODE *n){return n->segment==0&&n->bus==11&&n->device==0&&n->function==1&&
 n->id==0x22bb10deU&&n->subsystem==0x470219daU&&n->classrev==0x040300a1U&&(n->header&127)==0;}
typedef struct {const char *condition,*field;const REV3_NODE *node;UINT64 actual,expected;} REV3_REJECTION;
static inline BOOLEAN Rev3Reject(REV3_REJECTION *r,const char *why,const REV3_NODE *n,const char *field,UINT64 actual,UINT64 expected){
 if(r){r->condition=why;r->node=n;r->field=field;r->actual=actual;r->expected=expected;}return FALSE;
}
static inline BOOLEAN Rev3TopologyExplain(const REV3_NODE *nodes,UINTN count,EFI_HANDLE gpu,EFI_HANDLE boot,REV3_REJECTION *r){
 UINTN parents=0,targets=0,boots=0,raids=0;const REV3_NODE *parent=NULL,*target=NULL;
#define FAIL(why,n,f,a,e) return Rev3Reject(r,why,n,f,(UINT64)(a),(UINT64)(e))
#define EXACT(n,f,e,why) do{if((n)->f!=(e))FAIL(why,n,#f,(n)->f,e);}while(0)
 if(!nodes||!count||count>REV3_TOPOLOGY_MAX||!gpu||!boot||gpu==boot)FAIL("INVALID_INPUT",NULL,"count",count,REV3_TOPOLOGY_MAX);
 for(UINTN i=0;i<count;i++){
  const REV3_NODE *n=&nodes[i];
  if(n->bus>255||n->device>31||n->function>7||!n->handle||!n->id||n->id==0xffffffffU)FAIL("INVALID_NODE",n,"id",n->id,0);
  for(UINTN j=0;j<i;j++)if(n->handle==nodes[j].handle||(n->segment==nodes[j].segment&&n->bus==nodes[j].bus&&n->device==nodes[j].device&&n->function==nodes[j].function))FAIL("DUPLICATE_NODE",n,"index",i,j);
  if(n->handle==gpu){targets++;target=n;EXACT(n,id,0x270210deU,"GPU_ID");EXACT(n,subsystem,0x470219daU,"GPU_SUBSYSTEM");if((n->classrev>>24)!=3)FAIL("GPU_CLASS",n,"class",n->classrev>>24,3);if(n->header&127)FAIL("GPU_HEADER",n,"header",n->header&127,0);}
  if(n->handle==boot){boots++;EXACT(n,segment,0,"BOOT_SEGMENT");EXACT(n,bus,1,"BOOT_BUS");EXACT(n,device,0,"BOOT_DEVICE");EXACT(n,function,0,"BOOT_FUNCTION");EXACT(n,id,0x22681d79U,"BOOT_ID");EXACT(n,subsystem,0x22681d79U,"BOOT_SUBSYSTEM");EXACT(n,classrev,0x01080203U,"BOOT_CLASSREV");if(n->header&127)FAIL("BOOT_HEADER",n,"header",n->header&127,0);}
  if(n->segment==0&&n->bus==4&&!n->device&&!n->function&&n->id==0x02859005U&&n->subsystem==0x02d59005U&&n->classrev==0x01040009U)raids++;
  if(n->segment==0&&n->bus==0&&n->device==3&&n->function==1){parents++;parent=n;}
 }
 if(targets!=1)FAIL("GPU_COUNT",NULL,"count",targets,1);
 if(boots!=1)FAIL("BOOT_COUNT",NULL,"count",boots,1);
 if(parents!=1)FAIL("PARENT_COUNT",NULL,"count",parents,1);
 if(!raids)FAIL("RAID_NOT_MATCHED",NULL,"count",raids,1);
 EXACT(target,segment,0,"GPU_SEGMENT");EXACT(target,bus,11,"GPU_BUS");EXACT(target,device,0,"GPU_DEVICE");EXACT(target,function,0,"GPU_FUNCTION");
 EXACT(parent,id,0x14831022U,"PARENT_ID");EXACT(parent,classrev,0x06040000U,"PARENT_CLASSREV");
 if((parent->header&127)!=1)FAIL("PARENT_HEADER",parent,"header",parent->header&127,1);
 if((parent->buses&0xffffff)!=0x0b0b00)FAIL("PARENT_BUS_WINDOW",parent,"primary_secondary_subordinate",parent->buses&0xffffff,0x0b0b00);
 EXACT(parent,command,7,"PARENT_COMMAND");EXACT(parent,control,0x18,"PARENT_CONTROL");
 for(UINTN i=0;i<count;i++){
  const REV3_NODE *n=&nodes[i];
  if(n->segment==0&&n->bus==11&&n->handle!=gpu){
   EXACT(n,device,0,"GPU_BUS_EXTRA_DEVICE");EXACT(n,function,1,"GPU_BUS_EXTRA_FUNCTION");EXACT(n,id,0x22bb10deU,"AUDIO_ID");EXACT(n,subsystem,0x470219daU,"AUDIO_SUBSYSTEM");EXACT(n,classrev,0x040300a1U,"AUDIO_CLASSREV");
   if(n->header&127)FAIL("AUDIO_HEADER",n,"header",n->header&127,0);
   /* Only RUN02 measured COMMAND=6 may enter the passive-owner gate
    * with BME set. This is not a DMA-idle assertion or an audio write. */
   if((n->command&4)&&n->command!=6)FAIL("AUDIO_INITIAL_COMMAND",n,"command",n->command,6);
  }
  if((n->header&127)==1&&n!=parent&&n->segment==0){
   UINT32 secondary=(n->buses>>8)&255,subordinate=(n->buses>>16)&255;
   /* An unassigned 0..0 window cannot route bus 11 (or NVMe 1 / RAID 4).
    * Do not relax malformed, active overlapping, or the pinned parent window. */
   if(!secondary&&!subordinate)continue;
   if(!secondary||secondary>subordinate||n->bus>=secondary)FAIL("OTHER_BRIDGE_MALFORMED_WINDOW",n,"bus_window",n->buses&0xffffff,0);
   if(secondary<=11&&subordinate>=11)FAIL("OTHER_BRIDGE_OVERLAPS_GPU",n,"secondary_subordinate",n->buses&0xffff00,0x0b0b00);
  }
 }
#undef EXACT
#undef FAIL
 return TRUE;
}
static inline BOOLEAN Rev3TopologyValid(const REV3_NODE *n,UINTN count,EFI_HANDLE gpu,EFI_HANDLE boot){return Rev3TopologyExplain(n,count,gpu,boot,NULL);}
EFI_STATUS Rev3TopologyGate(EFI_HANDLE gpu,EFI_HANDLE volume,EFI_HANDLE image);
#endif

