#ifndef REV3_CONTROL_H
#define REV3_CONTROL_H
#include <Uefi.h>
typedef enum {
 R3_IDENTIFY,R3_OBSERVE,R3_BOOT_DISCOVER,R3_TOPOLOGY,R3_PROFILE,R3_ASSETS,
 R3_BOOT_LOAD,R4_CONSUMER_PLAN,R3_CONFIRM,R3_RECHECK,R4_CONSUMER_RELEASE,R3_DISCONNECT,R3_REFRESH,R3_CLAIM,
 R3_POST_CLAIM,R3_DECODE,R3_POST_DECODE,R3_TRANSACTION,R3_RESTORE,R3_START,
 R3_STAGE_COUNT
} R3_STAGE;
typedef EFI_STATUS (*R3_ACTION)(VOID*,R3_STAGE);
typedef struct {R3_STAGE stage;BOOLEAN mutation_possible;} R3_FLOW;
/* Both the product and host mock execute this exact ordered state machine.
 * Stop on the first error, including errors with apparently matching readback. */
static inline EFI_STATUS Rev3ControlRun(VOID *context,R3_ACTION action,R3_FLOW *flow){
 EFI_STATUS status;UINTN i;
 if(!action||!flow)return EFI_INVALID_PARAMETER;
 flow->mutation_possible=FALSE;
 for(i=0;i<R3_STAGE_COUNT;i++){
  flow->stage=(R3_STAGE)i;
  if(i==R4_CONSUMER_RELEASE)flow->mutation_possible=TRUE;
  status=action(context,(R3_STAGE)i);
  if(EFI_ERROR(status))return status;
 }
 return EFI_SUCCESS;
}
#endif
