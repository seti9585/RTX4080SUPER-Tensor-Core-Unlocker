#ifndef REV3_POLICY_H
#define REV3_POLICY_H
/* Locked release: no firmware services may run before this check. */
#include "experimental_mode.h"
static volatile const unsigned char Rev3ExecutionApproved=GU_EXPERIMENTAL_ENABLED;
/* PROM/chip are measured; initial VRAM/selectors and ownership remain incomplete. */
#if defined(GU_HOST_APP_TEST) || GU_EXPERIMENTAL_ENABLED
#ifdef GU_HOST_APP_TEST
#include "tests/app_host_model.h"
#endif
static volatile const unsigned char Rev3HardwareProfileReady=1;
#else
static volatile const unsigned char Rev3HardwareProfileReady=0;
#endif
static inline int Rev3SelectorsMatch(unsigned int ss0,unsigned int ss1) {
 return ss0==0x88888888U && ss1==8U;
}
static inline int Rev3PreFlrCommand(unsigned short command) {
 return command!=0xffffU && (command&2U) && !(command&4U);
}
static inline int Rev3RestoredCommand(unsigned short command,unsigned short initial) {
 return initial!=0xffffU && command==initial;
}
#endif

