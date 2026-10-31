#ifndef GU_PCI_SNAPSHOT_H
#define GU_PCI_SNAPSHOT_H
#include "port.h"
#if defined(GU_VGA_RESTORE) && (!defined(GU_PCI_SNAPSHOT) || defined(GU_PCI_SNAPSHOT_NORMAL))
#error VGA restoration requires experimental snapshot integration
#endif
#ifdef GU_VGA_RESTORE
EFI_STATUS GuRestoreVga(BOOLEAN verifyOnly);
#endif
#if defined(GU_PCI_SNAPSHOT_NORMAL) && !defined(GU_PCI_SNAPSHOT)
#error Normal snapshot requires the read-only snapshot module
#endif
#if defined(GU_PCI_SNAPSHOT)
VOID GuSnapshotReset(VOID);
EFI_STATUS GuSnapshotInit(VOID);
VOID GuSnapshotTake(unsigned stage);
BOOLEAN GuSnapshotFlush(BOOLEAN normal);
#else
static inline VOID GuSnapshotReset(VOID){}
static inline EFI_STATUS GuSnapshotInit(VOID){return EFI_SUCCESS;}
static inline VOID GuSnapshotTake(unsigned stage){(void)stage;}
static inline BOOLEAN GuSnapshotFlush(BOOLEAN normal){(void)normal;return TRUE;}
#endif
#endif
