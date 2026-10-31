/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef R4_CONSUMER_RELEASE_H
#define R4_CONSUMER_RELEASE_H
#include "ownership.h"
#include <Protocol/GraphicsOutput.h>
#define R4_EDGE_MAX 2048
#define R4_STEP_MAX 16
typedef struct {
 EFI_HANDLE handle,agent,controller;EFI_GUID protocol;UINT32 attributes,count;
} R4_EDGE;
typedef struct {UINTN count;R4_EDGE edges[R4_EDGE_MAX];} R4_GRAPH;
typedef struct {
 EFI_HANDLE target,agent;
 UINTN removed_count,removed[R4_EDGE_MAX];
 BOOLEAN text_present_after;
} R4_STEP;
typedef struct {
 BOOLEAN ready,attempted,finished;
 EFI_HANDLE gpu,child;
 UINTN steps,completed;
 R4_GRAPH original;
 R4_STEP step[R4_STEP_MAX];
} R4_PLAN;
typedef struct {
 VOID *context;
 EFI_STATUS (*snapshot)(VOID*,R4_GRAPH*);
 EFI_STATUS (*validate)(VOID*,const R4_PLAN*,UINTN);
 EFI_STATUS (*note)(VOID*,const char*,UINTN,EFI_STATUS);
 EFI_STATUS (*disconnect)(VOID*,EFI_HANDLE,EFI_HANDLE);
 EFI_STATUS (*text_state)(VOID*,EFI_HANDLE,BOOLEAN);
} R4_OPS;
BOOLEAN R4EdgeEqual(const R4_EDGE*,const R4_EDGE*);
BOOLEAN R4Expected(const R4_PLAN*,const R4_GRAPH*,UINTN);
EFI_STATUS R4BuildSimple(R4_PLAN*,const R4_GRAPH*,EFI_HANDLE,EFI_HANDLE,
 EFI_HANDLE,EFI_HANDLE,EFI_HANDLE,BOOLEAN);
EFI_STATUS R4BuildEdid(R4_PLAN*,const R4_GRAPH*,EFI_HANDLE,EFI_HANDLE,
 EFI_HANDLE,EFI_HANDLE,EFI_HANDLE,EFI_HANDLE,BOOLEAN);
EFI_STATUS R4Execute(R4_PLAN*,const R4_OPS*);
EFI_STATUS GuPrepareConsumerRelease(EFI_SYSTEM_TABLE*,const GU_RELEASE_OWNER*,EFI_HANDLE);
EFI_STATUS GuRecordOwnershipEvidence(EFI_SYSTEM_TABLE*,const GU_RELEASE_OWNER*);
EFI_STATUS GuReleaseConsumers(BOOLEAN*);
EFI_STATUS GuConsumerParentReady(EFI_SYSTEM_TABLE*,GU_RELEASE_OWNER*);
EFI_STATUS GuVerifyConsumersRestored(EFI_SYSTEM_TABLE*);
/* Re-enumerate the restored parent/child and permit only a subset of the
 * four reviewed consumer edges while local connection is still in progress. */
EFI_STATUS GuInspectRestoredDisplay(EFI_SYSTEM_TABLE*,EFI_HANDLE*,BOOLEAN*,BOOLEAN RequireDisplay);
EFI_STATUS GuIsolateLegacyBinding(VOID);
EFI_STATUS GuLegacyIsolationCheck(VOID);
#endif
