/* SPDX-License-Identifier: GPL-2.0-only */
#include "consumer_release.h"
#include "runtime.h"
#include <Protocol/SimpleTextOut.h>
#include <Protocol/EdidDiscovered.h>
static const EFI_GUID TextGuid=EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID,GopGuid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static const EFI_GUID EdidGuid=EFI_EDID_DISCOVERED_PROTOCOL_GUID;
static BOOLEAN GuidEqual(const EFI_GUID *a,const EFI_GUID *b){return !memcmp(a,b,sizeof(*a));}
BOOLEAN R4EdgeEqual(const R4_EDGE *a,const R4_EDGE *b){return a->handle==b->handle&&a->agent==b->agent&&a->controller==b->controller&&GuidEqual(&a->protocol,&b->protocol)&&a->attributes==b->attributes&&a->count==b->count;}
static BOOLEAN Removed(const R4_PLAN *p,UINTN edge,UINTN stage){
 for(UINTN i=0;i<stage;i++)for(UINTN j=0;j<p->step[i].removed_count;j++)if(p->step[i].removed[j]==edge)return TRUE;
 return FALSE;
}
BOOLEAN R4Expected(const R4_PLAN *p,const R4_GRAPH *now,UINTN stage){
 UINTN expected=0;
 if(!p||!now||p->steps>R4_STEP_MAX||p->original.count>R4_EDGE_MAX||now->count>R4_EDGE_MAX||stage>p->steps)return FALSE;
 for(UINTN i=0;i<p->steps;i++){
  if(p->step[i].removed_count>R4_EDGE_MAX)return FALSE;
  for(UINTN j=0;j<p->step[i].removed_count;j++)if(p->step[i].removed[j]>=p->original.count)return FALSE;
 }
 for(UINTN i=0;i<now->count;i++)for(UINTN j=0;j<i;j++)if(R4EdgeEqual(&now->edges[i],&now->edges[j]))return FALSE;
 for(UINTN i=0;i<p->original.count;i++){
  if(Removed(p,i,stage))continue;expected++;UINTN matches=0;
  for(UINTN j=0;j<now->count;j++)if(R4EdgeEqual(&p->original.edges[i],&now->edges[j]))matches++;
  if(matches!=1)return FALSE;
 }
 return expected==now->count;
}
static EFI_STATUS Build(R4_PLAN *p,const R4_GRAPH *graph,EFI_HANDLE gpu,EFI_HANDLE child,
 EFI_HANDLE console,EFI_HANDLE out,EFI_HANDLE gfx,EFI_HANDLE edid,BOOLEAN reviewed){
 UINTN out_driver=0,out_child=0,gfx_driver=0,edid_driver=0;
 if(!p||!graph||graph->count>R4_EDGE_MAX)return EFI_INVALID_PARAMETER;
 memset(p,0,sizeof(*p));
 if(!gpu||!child||gpu==child||!console||child==console||!out||!gfx||out==gfx||out==gpu||gfx==gpu||out==child||gfx==child)return EFI_ACCESS_DENIED;
 if(edid&&(edid==out||edid==gfx||edid==gpu||edid==child||edid==console))return EFI_ACCESS_DENIED;
 /* No registration order or guessed driver identity grants release authority. */
 if(!reviewed)return EFI_NOT_READY;
 p->gpu=gpu;p->child=child;p->steps=edid?3:2;p->original=*graph;
 p->step[0].target=p->step[1].target=child;p->step[0].agent=out;p->step[1].agent=gfx;
 p->step[0].text_present_after=TRUE;p->step[1].text_present_after=FALSE;
 if(edid){p->step[2].target=child;p->step[2].agent=edid;p->step[2].text_present_after=FALSE;}
 for(UINTN i=0;i<graph->count;i++) {
  const R4_EDGE *e=&graph->edges[i];
  if(!e->agent||!e->count)return EFI_COMPROMISED_DATA;
  for(UINTN j=0;j<i;j++)if(R4EdgeEqual(e,&graph->edges[j]))return EFI_COMPROMISED_DATA;
  if(e->handle!=child)continue;
  UINTN step;
  if(GuidEqual(&e->protocol,&TextGuid)&&e->agent==out&&e->count==1&&
    e->attributes==EFI_OPEN_PROTOCOL_BY_DRIVER&&e->controller==child){out_driver++;step=0;}
  else if(GuidEqual(&e->protocol,&TextGuid)&&e->agent==out&&e->count==1&&
    e->attributes==EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER&&e->controller==console){out_child++;step=0;}
  else if(GuidEqual(&e->protocol,&GopGuid)&&e->agent==gfx&&e->count==1&&
    e->attributes==EFI_OPEN_PROTOCOL_BY_DRIVER&&e->controller==child){gfx_driver++;step=1;}
  else if(edid&&GuidEqual(&e->protocol,&EdidGuid)&&e->agent==edid&&e->count==1&&
    e->attributes==EFI_OPEN_PROTOCOL_BY_DRIVER&&e->controller==child){edid_driver++;step=2;}
  else return EFI_ACCESS_DENIED;
  p->step[step].removed[p->step[step].removed_count++]=i;
 }
 if(out_driver!=1||out_child!=1||gfx_driver!=1||edid_driver!=(edid?1U:0U))return EFI_ACCESS_DENIED;
 /* A target agent must have no additional child relationship on this target. */
 for(UINTN i=0;i<graph->count;i++)if((graph->edges[i].agent==out||graph->edges[i].agent==gfx||(edid&&graph->edges[i].agent==edid))&&
  graph->edges[i].controller==child&&graph->edges[i].handle!=child)return EFI_ACCESS_DENIED;
 p->ready=TRUE;return EFI_SUCCESS;
}
EFI_STATUS R4BuildSimple(R4_PLAN *p,const R4_GRAPH *g,EFI_HANDLE gpu,EFI_HANDLE child,EFI_HANDLE console,EFI_HANDLE out,EFI_HANDLE gfx,BOOLEAN reviewed){return Build(p,g,gpu,child,console,out,gfx,NULL,reviewed);}
EFI_STATUS R4BuildEdid(R4_PLAN *p,const R4_GRAPH *g,EFI_HANDLE gpu,EFI_HANDLE child,EFI_HANDLE console,EFI_HANDLE out,EFI_HANDLE gfx,EFI_HANDLE edid,BOOLEAN reviewed){
 if(!edid){if(p)memset(p,0,sizeof(*p));return EFI_ACCESS_DENIED;}
 return Build(p,g,gpu,child,console,out,gfx,edid,reviewed);
}
EFI_STATUS R4Execute(R4_PLAN *p,const R4_OPS *ops){
 static R4_GRAPH current;
 if(!p||!ops||!ops->snapshot||!ops->validate||!ops->note||!ops->disconnect||!ops->text_state)return EFI_INVALID_PARAMETER;
 if(!p->ready||p->attempted||p->completed||p->finished||p->steps>R4_STEP_MAX)return EFI_NOT_READY;
 for(UINTN i=0;i<p->steps;i++){
  if(!p->step[i].target||!p->step[i].agent||p->step[i].target==p->gpu)return EFI_ACCESS_DENIED;
  EFI_STATUS status=ops->note(ops->context,"consumer-intent",i,EFI_SUCCESS);if(EFI_ERROR(status))return status;
  status=ops->validate(ops->context,p,i);if(EFI_ERROR(status))return status;
  status=ops->snapshot(ops->context,&current);if(EFI_ERROR(status))return status;
  if(!R4Expected(p,&current,i))return EFI_MEDIA_CHANGED;
  /* Set before the call: an error can follow partial firmware effects. */
  p->attempted=TRUE;
  EFI_STATUS api=ops->disconnect(ops->context,p->step[i].target,p->step[i].agent);
  status=ops->snapshot(ops->context,&current);if(EFI_ERROR(status))return status;
  BOOLEAN matches=R4Expected(p,&current,i+1);
  if(EFI_ERROR(api)){ops->note(ops->context,matches?"consumer-api-error-matching-graph":"consumer-api-error-unexpected-graph",i,api);return api;}
  if(!matches)return EFI_MEDIA_CHANGED;
  status=ops->text_state(ops->context,p->step[i].target,p->step[i].text_present_after);if(EFI_ERROR(status))return status;
  status=ops->validate(ops->context,p,i+1);if(EFI_ERROR(status))return status;
  p->completed=i+1;
  status=ops->note(ops->context,"consumer-verified",i,EFI_SUCCESS);if(EFI_ERROR(status))return status;
 }
 p->finished=TRUE;return EFI_SUCCESS;
}
