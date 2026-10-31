/* SPDX-License-Identifier: GPL-2.0-only
 * Real Rev4 application and AD103 core; all EFI services and cold state synthetic.
 * PROM, PCI fixture and firmware bytes are measured/pinned inputs, not mocks.
 */
#define main transaction_fixture_main
#define GuLog FixtureLog
#define GuLogText FixtureLogText
#include "test_efi_transaction.c"
#undef main
#undef GuLog
#undef GuLogText
#include "app_host_model.h"
#include "../app.h"
#include "../sha256.h"
#include "../app_io.h"
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/EdidDiscovered.h>
#include <Guid/FileInfo.h>
#include <IndustryStandard/Acpi.h>
#include <setjmp.h>
#define H(n) ((EFI_HANDLE)(UINTN)(n))
#ifdef GU_NATIVE_GOP_TEST
#define NATIVE_CASES(X) X(DYNAMIC_NO_PROTOCOL) X(DYNAMIC_INFO_FAIL) X(DYNAMIC_SIZE_FAIL) X(DYNAMIC_DECODE_FAIL) X(DYNAMIC_HASH_FAIL) X(NATIVE_LOAD_ERROR) X(NATIVE_CODE) X(NATIVE_TYPE) X(NATIVE_DEVICE) X(NATIVE_START_ERROR) X(NATIVE_NO_BINDING) X(NATIVE_DUP_BINDING) X(NATIVE_VERSION) X(NATIVE_START_POINTER) X(NATIVE_LATE_CODE) X(NATIVE_INACTIVE_OUTPUT) X(NATIVE_FOREIGN_EDGE) X(NATIVE_WRONG_ADR) X(NATIVE_PARENT_CHANGED) \
 X(NATIVE_SUPPORTED_ERROR) X(NATIVE_SUPPORTED_ALREADY) X(NATIVE_BIND_START_ERROR) X(NATIVE_ISOLATION_ERROR) X(NATIVE_SHARED_LEGACY) X(NATIVE_ISOLATION_NOOP) X(NATIVE_NOTIFY_REINSTALL) X(NATIVE_MODE_CHANGE) X(NATIVE_MODE_INVALID) X(NATIVE_SUPPORTED_DIRTY) X(NATIVE_PARENT_TPL) X(NATIVE_SUPPORTED_LOG) X(NATIVE_DRIVER_START_LOG) X(NATIVE_LEGACY_ALIAS) X(NATIVE_ISOLATION_LATE_OWNER) X(NATIVE_ISOLATION_LOST_EDGE) X(NATIVE_ISOLATION_EXTRA_EDGE) X(NATIVE_CONSOLE_INITIALIZES) X(NATIVE_CONSOLE_MODE_FAIL) X(NATIVE_CONSOLE_FB_ZERO) X(NATIVE_CONSOLE_OWNER_FAIL) X(NATIVE_CONSOLE_FB_OVERFLOW) X(NATIVE_MODE_POINTER_NULL) X(NATIVE_INFO_POINTER_NULL) X(NATIVE_CONSOLE_MODE_OOB) X(NATIVE_CONSOLE_INFO_SIZE) X(NATIVE_CONSOLE_DMA_REAPPEARS) X(NATIVE_COMMAND_7_TO_3) X(NATIVE_NO_IO) X(NATIVE_NO_MEMORY) X(NATIVE_COMMAND_EXTRA) X(NATIVE_BME_ON) X(NATIVE_ATTR_BME) X(NATIVE_ATTR_NO_IO) X(NATIVE_ATTR_NO_MEMORY) X(NATIVE_ATTR_ERROR) X(NATIVE_BEFORE_START_COMMAND) X(NATIVE_BEFORE_START_BAR) X(NATIVE_BEFORE_START_ATTR)
#include <Protocol/Decompress.h>
#include "Decompress.h"
static UINT8 NativeGopImage[164608];
static int DynamicMode;
static EFI_STATUS EFIAPI DynamicInfo(EFI_DECOMPRESS_PROTOCOL *d,VOID *p,UINT32 n,UINT32 *o,UINT32 *s){(void)d;if(DynamicMode==2)return EFI_DEVICE_ERROR;EFI_STATUS r=EfiGetInfo(p,n,o,s);if(DynamicMode==3)*o=1;return r;}
static EFI_STATUS EFIAPI DynamicDecode(EFI_DECOMPRESS_PROTOCOL *d,VOID *p,UINT32 n,VOID *o,UINT32 z,VOID *s,UINT32 t){(void)d;if(DynamicMode==4)return EFI_DEVICE_ERROR;EFI_STATUS r=EfiDecompress(p,n,o,z,s,t);if(DynamicMode==5)((UINT8*)o)[0]^=1;return r;}
static EFI_DECOMPRESS_PROTOCOL DynamicProtocol={DynamicInfo,DynamicDecode};
static EFI_STATUS EFIAPI DynamicLocate(EFI_GUID *g,VOID *registration,VOID **out){
 EFI_GUID expected=EFI_DECOMPRESS_PROTOCOL_GUID;
 CHECK(!memcmp(g,&expected,sizeof(expected))&&!registration);if(DynamicMode==1){*out=NULL;return EFI_NOT_FOUND;}*out=&DynamicProtocol;return EFI_SUCCESS;
}
static UINT8 native_code[0x27ac0];
static EFI_LOADED_IMAGE_PROTOCOL native_image;
static EFI_DRIVER_BINDING_PROTOCOL native_binding;
static UINTN native_loads,native_starts;
static UINTN native_supported,native_bind_starts,legacy_dispatches,notification_attempts,isolation_calls;
static BOOLEAN legacy_registered,native_dispatch;
static UINT8 inactive_path[36];
#else
#define NATIVE_CASES(X)
#endif
#ifdef GU_VGA_RESTORE
#define VGA_CASES(X) X(VGA_API_ERROR) X(VGA_NO_EFFECT) X(VGA_EXTRA_CHANGE) X(VGA_BEFORE_START_LOST) X(VGA_ROOT_CHANGE)
#else
#define VGA_CASES(X)
#endif
#define APP_CASES(X) \
 X(OK) X(ESCAPE) X(TIMEOUT) X(SECURE_BOOT) X(NO_USB) \
 X(BOOT_MISSING) X(BOOT_MULTI) X(BOOT_READ_ERROR) X(BOOT_SHORT) X(BOOT_BAD_MZ) X(BOOT_CLOSE) X(ROOT_CLOSE) \
 X(LOAD_ERROR) X(LOAD_WRONG_VOLUME) X(LOAD_WRONG_TYPE) \
 X(ASSET_SIZE) X(ASSET_READ_ERROR) X(ASSET_SHORT) X(ASSET_HASH) X(ASSET_CLOSE) \
 X(ROM_SIZE) X(ROM_HASH) X(PROM_HASH) X(INITIAL_SS0) X(INITIAL_SS1) X(INITIAL_VRAM) \
 X(CONSUMER_VERSION) X(CONSUMER_HASH) X(CONSUMER_TYPE) X(CONSUMER_GUID) \
 X(AUDIO_BME) X(AUDIO_OWNER) X(UNKNOWN_FUNCTION) X(CONSOLE_RACE) X(GRAPH_RACE) \
 X(OUT_ERROR) X(GFX_ERROR) X(EDID_ERROR) X(EDID_DROPS_INPUT) X(CONSUMER_LOG_ERROR) \
 X(PARENT_ERROR) X(PARENT_NOOP) X(MISSING_PCI) X(REPLACED_PCI) X(NEW_OWNER) \
 X(DECODE_ERROR_MATCH) X(MASTER_ENABLE_ERROR) X(MASTER_DISABLE_ERROR) \
 X(GOP_ERROR) X(GOP_NOOP) X(GOP_COMMAND) X(GOP_BAR) X(GOP_SS0) X(GOP_SS1) X(GOP_EDID) X(GOP_CONSOLE) \
 X(REPORT_SHORT) X(REPORT_FLUSH) X(REPORT_CLOSE) X(START_RETURNS) X(CLOSE_EXCLUSIVE_ERROR) \
 X(RUN03_SS) X(RUN04_SS) X(ZERO_SS) X(RUN03_UNSTABLE) X(RUN04_UNSTABLE) \
 X(RUN03_READ_ERROR) X(RUN04_READ_ERROR) X(SS0_BADF) X(SS1_FFFF) X(POST_PROM_CHANGED) \
 X(CONFIRM_SS_CHANGED) X(GOP_PCI_MISSING) X(BEFORE_START_SS) \
 X(RESTORE_WRONG_OWNER) X(RESTORE_NO_CHILD) X(RESTORE_NO_GOP) X(RESTORE_MULTI_CHILD) \
 X(RESTORE_WRONG_PATH) X(RESTORE_WRONG_GPU) X(RESTORE_PARENT_GOP) X(CHILD_ERROR) \
 X(CHILD_NOT_FOUND_COMPLETE) X(CHILD_NOT_FOUND_MISSING) X(CONSUMERS_EARLY) X(REUSE_CHILD) \
 X(PARENT_NO_RETURN) X(CHILD_NO_RETURN) X(PARENT_LOG_WRITE) X(CHILD_LOG_WRITE) \
 X(PARENT_LOG_FLUSH) X(CHILD_LOG_FLUSH) X(RESTORE_EXTRA_EDGE) X(RESTORE_LOST_INPUT) NATIVE_CASES(X) VGA_CASES(X)
#define DECLARE(n) A_##n,
enum app_case {APP_CASES(DECLARE) A_COUNT};
#undef DECLARE
#define NAME(n) #n,
static const char *names[]={APP_CASES(NAME)};
#undef NAME
static enum app_case app_mode;
static enum scenario requested_core;
static EFI_SYSTEM_TABLE st;
static EFI_RUNTIME_SERVICES rt;
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL conout;
static EFI_SIMPLE_TEXT_INPUT_PROTOCOL conin;
static EFI_LOADED_IMAGE_PROTOCOL loaded,windows_image,owner_image,consumer_images[3];
static EFI_DRIVER_BINDING_PROTOCOL owner_binding,consumer_bindings[3];
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL fs,windows_fs;
static EFI_GRAPHICS_OUTPUT_PROTOCOL gop;
static EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE gop_mode;
static EFI_GRAPHICS_OUTPUT_MODE_INFORMATION gop_info;
static EFI_PCI_IO_PROTOCOL other_pci[5],replacement_pci;
static EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL root_pci;
static UINT32 other_config[5][64];
#ifdef GU_MEASURED_PROFILE_TEST
#include "../run0009_profiles.h"
static UINT8 owner_code[0x4800],consumer_code[3][0x9000],rom_image[0x24c00];
static UINT8 measured_rom[0x24c00],measured_stop[4][64];
static struct {EFI_DEVICE_PATH_PROTOCOL node;EFI_GUID file;EFI_DEVICE_PATH_PROTOCOL end;} owner_path;
#else
static UINT8 owner_code[0x4800],consumer_code[3][0x2000],rom_image[0x24c00];
#endif
static UINT8 expected_stop_hash[3][32];
char GuHostRomSha256[65];
static EFI_GUID pg=EFI_PCI_IO_PROTOCOL_GUID,lg=EFI_LOADED_IMAGE_PROTOCOL_GUID,dg=EFI_DEVICE_PATH_PROTOCOL_GUID,
 fg=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID,gg=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,ig=EFI_FILE_INFO_ID,
 bg=EFI_DRIVER_BINDING_PROTOCOL_GUID,tg=EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID,
 eg=EFI_EDID_DISCOVERED_PROTOCOL_GUID,rg=EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_GUID,ing=EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID;
static const EFI_GUID consumer_guids[3]={
 {0x628a497d,0x2bf6,0x4264,{0x87,0x41,0x06,0x9d,0xbd,0x33,0x99,0xd6}},
 {0x43e7abdd,0xe352,0x4cfb,{0xa2,0x30,0x4c,0xdc,0x1d,0x35,0x0e,0x5c}},
 {0xc7a7030c,0xc3d8,0x45ee,{0xbe,0xd9,0x5d,0x9e,0x76,0x76,0x29,0x53}}};
static struct {EFI_DEVICE_PATH_PROTOCOL node;EFI_GUID file;EFI_DEVICE_PATH_PROTOCOL end;} consumer_paths[3];
static UINT8 rtx_path[]={2,1,12,0,0xd0,0x41,3,0xa,0,0,0,0,1,1,6,0,1,3,1,1,6,0,0,0,0x7f,0xff,4,0};
static UINT8 boot_path[]={2,1,12,0,0xd0,0x41,3,0xa,0,0,0,0,1,1,6,0,1,1,1,1,6,0,0,0,0x7f,0xff,4,0};
static UINT8 usb_path[]={3,5,6,0,1,0,0x7f,0xff,4,0};
typedef struct {EFI_FILE_PROTOCOL protocol;int kind;UINTN position;BOOLEAN open;} FILE_FAKE;
static FILE_FAKE files[12];
static GU_ASSET originals[5];
static char report_text[1048576],console_text[1048576];
static UINTN report_bytes,console_bytes,consumer_calls,parent_calls,connect_calls,start_calls,transfer_calls,
 exclusive_calls,unload_calls,load_calls,non_gpu_writes;
static BOOLEAN bound,roles[3],shared_input,graph_race,exclusive,prompt,key_sent,key_reset,run_created,loaded_windows;
static EFI_TPL current_tpl;
static UINTN cold_ss_reads,flushed_bytes;
static BOOLEAN prom_changed;
static EFI_HANDLE DisplayHandle;
static UINTN child_connect_calls;
static BOOLEAN stage_flush_error;
#ifdef GU_NATIVE_GOP_TEST
static UINT8 restored_path[sizeof(rtx_path)+8];
#else
static UINT8 restored_path[sizeof(rtx_path)];
#endif
static jmp_buf escape_halt;
EFI_STATUS EFIAPI EfiMain(EFI_HANDLE,EFI_SYSTEM_TABLE*);
static int guid(const EFI_GUID *a,const EFI_GUID *b){return !memcmp(a,b,sizeof(*a));}
static void narrow(char *d,UINTN n,const CHAR16 *s){UINTN i=0;while(i+1<n&&s[i]){d[i]=(char)s[i];i++;}d[i]=0;}
EFI_STATUS GuHostConsumerProfile(UINTN role,EFI_GUID *file,UINT32 *version,UINT64 *size,UINT64 *stop,UINT8 *hash,EFI_MEMORY_TYPE *type){
 CHECK(role<3);*file=consumer_guids[role];*version=(UINT32)(0x100+role);*size=0x2000;*stop=0x20;
 memcpy(hash,expected_stop_hash[role],32);*type=role==2?EfiRuntimeServicesCode:EfiBootServicesCode;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI output(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *p,CHAR16 *s){
 CHECK(p==&conout&&!GuScreenDetached);char text[2048];narrow(text,sizeof(text),s);UINTN n=strlen(text);
 CHECK(console_bytes+n<sizeof(console_text));memcpy(console_text+console_bytes,text,n+1);console_bytes+=n;
 if(strstr(console_text,"Press U once"))prompt=TRUE;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI input_reset(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *p,BOOLEAN extended){CHECK(p==&conin&&!extended);key_reset=TRUE;key_sent=FALSE;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI key(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *p,EFI_INPUT_KEY *k){
 CHECK(p==&conin);if(!prompt||key_sent||app_mode==A_TIMEOUT)return EFI_NOT_READY;CHECK(key_reset);key_sent=TRUE;
 k->ScanCode=app_mode==A_ESCAPE?0x17:0;k->UnicodeChar=app_mode==A_ESCAPE?0:'U';
 if(app_mode==A_CONFIRM_SS_CHANGED)registers[SS0/4]^=1;
 if(app_mode==A_CONSOLE_RACE)st.ConsoleOutHandle=H(99);if(app_mode==A_GRAPH_RACE)graph_race=TRUE;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI app_memory_read(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH width,
 UINT8 bar,UINT64 offset,UINTN count,VOID *value){
 if(!Gu.started&&offset==SS0){
  cold_ss_reads++;
  if((app_mode==A_RUN03_READ_ERROR||app_mode==A_RUN04_READ_ERROR)&&cold_ss_reads==2)return EFI_DEVICE_ERROR;
  if((app_mode==A_RUN03_UNSTABLE||app_mode==A_RUN04_UNSTABLE)&&cold_ss_reads==3)registers[SS0/4]^=0x40000;
 }
 if(!Gu.started&&!prom_changed&&app_mode==A_POST_PROM_CHANGED&&offset>=0x300000&&offset<0x400000){registers[SS0/4]^=1;prom_changed=TRUE;}
 return read_memory(p,width,bar,offset,count,value);
}
static EFI_STATUS EFIAPI app_stall(UINTN n){if(Gu.retained)longjmp(escape_halt,1);return stall(n);}
static EFI_STATUS EFIAPI watchdog(UINTN timeout,UINT64 code,UINTN bytes,CHAR16 *data){CHECK(!timeout&&!code&&!bytes&&!data);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI variable(CHAR16 *name,EFI_GUID *g,UINT32 *a,UINTN *n,VOID *data){
 char text[64];narrow(text,sizeof(text),name);CHECK(!strcmp(text,"SecureBoot")&&g&&*n==1);if(a)*a=6;*(UINT8*)data=app_mode==A_SECURE_BOOT;*n=1;return EFI_SUCCESS;
}
static EFI_TPL EFIAPI raise_tpl(EFI_TPL t){EFI_TPL old=current_tpl;current_tpl=t;
#ifdef GU_NATIVE_GOP_TEST
 if(t==TPL_HIGH_LEVEL&&app_mode==A_NATIVE_PARENT_TPL&&native_starts)return TPL_CALLBACK;
#endif
 return old;}
static VOID EFIAPI restore_tpl(EFI_TPL t){current_tpl=t;
#ifdef GU_NATIVE_GOP_TEST
 /* Inject only the observed caller TPL; keep host halt logging executable. */
 if(app_mode==A_NATIVE_PARENT_TPL&&t==TPL_CALLBACK)current_tpl=TPL_APPLICATION;
#endif
}
static EFI_STATUS list(const EFI_HANDLE *items,UINTN count,UINTN *n,EFI_HANDLE **out){
 *n=count;*out=NULL;if(!count)return EFI_NOT_FOUND;EFI_STATUS s=allocate_pool(EfiLoaderData,count*sizeof(**out),(VOID**)out);if(!EFI_ERROR(s))memcpy(*out,items,count*sizeof(**out));return s;
}
static const EFI_HANDLE pci_handles[]={H(3),H(20),H(21),H(22),H(23),H(24)};
static EFI_STATUS EFIAPI locate(EFI_LOCATE_SEARCH_TYPE type,EFI_GUID *g,VOID *k,UINTN *n,EFI_HANDLE **out){
 CHECK(!k);
 if(type==AllHandles){EFI_HANDLE all[]={H(1),H(2),H(3),H(6),H(8),H(9),DisplayHandle,H(12),H(14),H(20),H(21),H(22),H(23),H(24),H(30),H(31),H(32),H(40),H(50)};return list(all,sizeof(all)/sizeof(*all),n,out);}
 CHECK(type==ByProtocol&&g);
 if(guid(g,&pg))return list(pci_handles,6,n,out);
 if(guid(g,&fg)){EFI_HANDLE a[]={H(12),H(13)};return list(a,app_mode==A_BOOT_MULTI?2:1,n,out);}
 if(guid(g,&gg)){EFI_HANDLE a[]={DisplayHandle};return list(a,bound?1:0,n,out);}
#ifdef GU_NATIVE_GOP_TEST
 if(guid(g,&bg)){EFI_HANDLE a[6];UINTN c=0;if(legacy_registered)a[c++]=H(8);a[c++]=H(30);a[c++]=H(31);a[c++]=H(32);
  if(app_mode!=A_NATIVE_NO_BINDING)a[c++]=H(60);if(app_mode==A_NATIVE_DUP_BINDING||app_mode==A_NATIVE_LEGACY_ALIAS)a[c++]=H(62);return list(a,c,n,out);}
#endif
 CHECK(0);return EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI locate_fixed(EFI_LOCATE_SEARCH_TYPE type,EFI_GUID *g,VOID *k,UINTN *bytes,EFI_HANDLE *out){
 CHECK(type==ByProtocol&&!k);if(guid(g,&pg)){CHECK(*bytes>=sizeof(pci_handles));memcpy(out,pci_handles,sizeof(pci_handles));*bytes=sizeof(pci_handles);return EFI_SUCCESS;}
 CHECK(guid(g,&rg)&&*bytes>=sizeof(EFI_HANDLE));*out=H(50);*bytes=sizeof(EFI_HANDLE);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI handle(EFI_HANDLE h,EFI_GUID *g,VOID **p){
 *p=NULL;
#ifdef GU_PCI_SNAPSHOT
 if(guid(g,&dg)&&(h==H(50)||((UINTN)h>=20&&(UINTN)h<=24))){
  static UINT8 diag_paths[6][28];UINTN k=h==H(50)?5:(UINTN)h-20;
  UINTN end=k==5?12:k==0?18:24;memcpy(diag_paths[k],rtx_path,end);
  if(k>0&&k<5)diag_paths[k][17]=(UINT8)(10+k);
  UINT8 tail[]={127,255,4,0};memcpy(diag_paths[k]+end,tail,4);*p=diag_paths[k];return EFI_SUCCESS;
 }
#endif
#ifdef GU_NATIVE_GOP_TEST
 if(h==H(8)&&guid(g,&bg)&&!legacy_registered)return EFI_UNSUPPORTED;
 if(h==H(61)&&guid(g,&lg)){*p=&native_image;return EFI_SUCCESS;}
 if((h==H(60)||h==H(62))&&guid(g,&bg)){*p=h==H(62)&&app_mode==A_NATIVE_LEGACY_ALIAS?&owner_binding:&native_binding;return EFI_SUCCESS;}
 if(connect_calls&&h==H(77)&&guid(g,&dg)){*p=inactive_path;return EFI_SUCCESS;}
 if(connect_calls&&h==H(77)&&guid(g,&gg)&&app_mode==A_RESTORE_MULTI_CHILD){*p=&gop;return EFI_SUCCESS;}
#endif
 if(guid(g,&pg)){
  if(h==H(3)){if((parent_calls&&app_mode==A_MISSING_PCI)||(connect_calls&&app_mode==A_GOP_PCI_MISSING))return EFI_NOT_FOUND;*p=parent_calls&&app_mode==A_REPLACED_PCI?&replacement_pci:&pci;}
  for(UINTN i=0;i<5;i++)if(h==H(20+i))*p=&other_pci[i];
 }else if(guid(g,&lg)){
  if(h==H(1))*p=&loaded;if(h==H(9))*p=&owner_image;if(h==H(14))*p=&windows_image;
  for(UINTN i=0;i<3;i++)if(h==H(30+i))*p=&consumer_images[i];
 }else if(guid(g,&bg)){
  if(h==H(8))*p=&owner_binding;for(UINTN i=0;i<3;i++)if(h==H(30+i))*p=&consumer_bindings[i];
 }else if(guid(g,&fg)){if(h==H(2))*p=&fs;if(h==H(12)||h==H(13))*p=&windows_fs;}
 else if(guid(g,&gg)){if(bound&&h==DisplayHandle&&!(connect_calls&&(app_mode==A_RESTORE_NO_GOP||app_mode==A_RESTORE_PARENT_GOP)))*p=&gop;if(connect_calls&&h==H(3)&&app_mode==A_RESTORE_PARENT_GOP)*p=&gop;}
 else if(guid(g,&tg)){if(h==H(6)||(bound&&h==DisplayHandle&&roles[1]))*p=&conout;}
 else if(guid(g,&eg)){if(bound&&h==DisplayHandle)*p=&roles[2];}
 else if(guid(g,&ing)){if(h==H(40))*p=&conin;}
 else if(guid(g,&rg)){if(h==H(50))*p=&root_pci;}
 else if(guid(g,&dg)){
  if(h==H(2)&&app_mode!=A_NO_USB)*p=usb_path;
  if(h==H(12)||h==H(13))*p=boot_path;if(h==H(3)||(bound&&h==DisplayHandle))*p=connect_calls&&h==DisplayHandle?restored_path:rtx_path;
 }
 return *p?EFI_SUCCESS:EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI protocols(EFI_HANDLE h,EFI_GUID ***out,UINTN *n){
 EFI_GUID *a[6];UINTN c=0;
 if(h==H(3)){a[c++]=&pg;a[c++]=&dg;}
 else if(h==DisplayHandle&&bound){a[c++]=&gg;a[c++]=&eg;a[c++]=&dg;if(roles[1])a[c++]=&tg;}
 else if(h==H(40))a[c++]=&ing;
 else if((UINTN)h>=20&&(UINTN)h<=24)a[c++]=&pg;
 else a[c++]=&dg;
 CHECK(!EFI_ERROR(allocate_pool(EfiLoaderData,c*sizeof(**out),(VOID**)out)));memcpy(*out,a,c*sizeof(**out));*n=c;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI open_info(EFI_HANDLE h,EFI_GUID *g,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **out,UINTN *n){
 EFI_OPEN_PROTOCOL_INFORMATION_ENTRY a[4];UINTN c=0;
 UINTN parentAgent=8;
#ifdef GU_NATIVE_GOP_TEST
 if(connect_calls)parentAgent=60;if(child_connect_calls&&app_mode==A_NATIVE_CONSOLE_OWNER_FAIL)parentAgent=99;
#endif
#define EDGE(agent,controller,attr) a[c++]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){H(agent),controller,attr,1}
#ifdef GU_NATIVE_GOP_TEST
 if(h==H(8)&&guid(g,&bg)&&current_tpl==TPL_NOTIFY&&app_mode==A_NATIVE_ISOLATION_LATE_OWNER){EDGE(99,H(8),EFI_OPEN_PROTOCOL_BY_DRIVER);}
#endif
 if(h==H(3)&&guid(g,&pg)){
  if(bound){EDGE(connect_calls&&app_mode==A_RESTORE_WRONG_OWNER?99:parentAgent,H(3),EFI_OPEN_PROTOCOL_BY_DRIVER);
   if(!(connect_calls&&app_mode==A_RESTORE_NO_CHILD)){EDGE(parentAgent,DisplayHandle,EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER);}
   if(connect_calls&&app_mode==A_RESTORE_MULTI_CHILD){EDGE(parentAgent,H(77),EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER);}
#ifdef GU_NATIVE_GOP_TEST
   if(connect_calls&&app_mode==A_NATIVE_INACTIVE_OUTPUT){EDGE(parentAgent,H(77),EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER);}
   if(connect_calls&&app_mode==A_NATIVE_FOREIGN_EDGE){EDGE(99,H(77),EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER);}
#endif
  }
  if(exclusive){EDGE(1,NULL,EFI_OPEN_PROTOCOL_EXCLUSIVE);}
  if(parent_calls&&app_mode==A_NEW_OWNER){EDGE(99,H(3),EFI_OPEN_PROTOCOL_BY_DRIVER);}
 }
 if(h==H(3)&&bound&&guid(g,&dg)&&!(connect_calls&&app_mode==A_RESTORE_NO_CHILD)){EDGE(parentAgent,DisplayHandle,EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER);}
 if(h==DisplayHandle&&bound){
  if(guid(g,&tg)&&roles[0]){EDGE(30,DisplayHandle,EFI_OPEN_PROTOCOL_BY_DRIVER);EDGE(30,H(6),EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER);}
  if(guid(g,&gg)&&roles[1]){EDGE(31,DisplayHandle,EFI_OPEN_PROTOCOL_BY_DRIVER);}
  if(guid(g,&eg)&&roles[2]){EDGE(32,DisplayHandle,EFI_OPEN_PROTOCOL_BY_DRIVER);}
 }
 if(h==H(40)&&guid(g,&ing)){
  if(shared_input){EDGE(32,H(40),EFI_OPEN_PROTOCOL_BY_DRIVER);}
  if(graph_race){EDGE(99,H(40),EFI_OPEN_PROTOCOL_BY_DRIVER);}
  if(connect_calls&&app_mode==A_RESTORE_EXTRA_EDGE){EDGE(98,H(40),EFI_OPEN_PROTOCOL_BY_DRIVER);}
#ifdef GU_NATIVE_GOP_TEST
  if(parent_calls&&app_mode==A_NATIVE_SHARED_LEGACY){EDGE(8,H(40),EFI_OPEN_PROTOCOL_BY_DRIVER);}
#endif
 }
 if(h==H(24)&&app_mode==A_AUDIO_OWNER&&guid(g,&pg)){EDGE(99,H(24),EFI_OPEN_PROTOCOL_BY_DRIVER);}
#undef EDGE
 *out=NULL;*n=c;if(c){CHECK(!EFI_ERROR(allocate_pool(EfiLoaderData,c*sizeof(**out),(VOID**)out)));memcpy(*out,a,c*sizeof(**out));}return EFI_SUCCESS;
}
#ifdef GU_NATIVE_GOP_TEST
static EFI_STATUS EFIAPI uninstall_binding(EFI_HANDLE h,EFI_GUID *g,VOID *interface){
 CHECK(h==H(8)&&guid(g,&bg)&&interface==&owner_binding&&legacy_registered&&exclusive&&Gu.owned&&Gu.quiesced&&fake.flrs==1);
 CHECK(current_tpl==TPL_NOTIFY);
 CHECK(flushed_bytes==report_bytes&&strstr(report_text,"stage=before_LegacyBindingIsolation"));isolation_calls++;
 if(app_mode==A_NATIVE_ISOLATION_ERROR)return EFI_ACCESS_DENIED;
 if(app_mode!=A_NATIVE_ISOLATION_NOOP)legacy_registered=FALSE;
 if(app_mode==A_NATIVE_ISOLATION_LOST_EDGE)shared_input=FALSE;
 if(app_mode==A_NATIVE_ISOLATION_EXTRA_EDGE)graph_race=TRUE;
 return EFI_SUCCESS;
}
static void notification_selection(void){
 /* Model an entry/Console notification using the standard binding database.
  * It would invoke legacy Supported/Start if the binding were still exposed. */
 notification_attempts++;VOID *b=NULL;
 if(!EFI_ERROR(handle(H(8),&bg,&b))&&b)legacy_dispatches++;
 CHECK(!legacy_dispatches);
}
#endif
static EFI_STATUS EFIAPI open_protocol(EFI_HANDLE h,EFI_GUID *g,VOID **p,EFI_HANDLE agent,EFI_HANDLE controller,UINT32 attrs){
 CHECK(agent==H(1)&&!controller);
 if(attrs==EFI_OPEN_PROTOCOL_GET_PROTOCOL)return handle(h,g,p);
 CHECK(h==H(3)&&guid(g,&pg)&&attrs==EFI_OPEN_PROTOCOL_EXCLUSIVE&&!bound&&!exclusive&&consumer_calls==3);
 CHECK(!roles[0]&&!roles[1]&&!roles[2]);exclusive=TRUE;exclusive_calls++;*p=&pci;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI close_protocol(EFI_HANDLE h,EFI_GUID *g,EFI_HANDLE agent,EFI_HANDLE controller){
 CHECK(flushed_bytes==report_bytes);
 CHECK(h==H(3)&&guid(g,&pg)&&agent==H(1)&&!controller&&exclusive&&GuSafeToReturn()&&!Gu.allocations);CHECK(strstr(report_text,"stage=before_CloseProtocol"));if(app_mode==A_CLOSE_EXCLUSIVE_ERROR)return EFI_DEVICE_ERROR;exclusive=FALSE;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI location(EFI_PCI_IO_PROTOCOL *p,UINTN *s,UINTN *b,UINTN *d,UINTN *f){
 *s=*d=*f=0;if(p==&pci||p==&replacement_pci){*b=11;return EFI_SUCCESS;}
 UINTN i=(UINTN)(p-other_pci);CHECK(i<5);static const UINTN buses[]={0,4,1,7,11};*b=buses[i];if(!i){*d=3;*f=1;}if(i==4)*f=1;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI other_read(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 off,UINTN n,VOID *data){
 UINTN i=(UINTN)(p-other_pci);CHECK(i<5&&w==EfiPciIoWidthUint32&&off+n*4<=256);memcpy(data,(UINT8*)other_config[i]+off,n*4);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI other_write(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 off,UINTN n,VOID *data){non_gpu_writes++;CHECK(0);return EFI_ACCESS_DENIED;}
static EFI_STATUS EFIAPI app_config_read(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 off,UINTN n,VOID *data){
 CHECK(p==&pci||p==&replacement_pci);UINTN width=(UINTN)1<<w;CHECK(w<=EfiPciIoWidthUint32);
 for(UINTN i=0;i<n;i++){EFI_STATUS s=read_config(&pci,w,off+(UINT32)(i*width),1,(UINT8*)data+i*width);if(EFI_ERROR(s))return s;}return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI root_read(EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *p,EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH w,UINT64 address,UINTN n,VOID *data){
#ifdef GU_PCI_SNAPSHOT
 UINTN db=(address>>24)&255,dd=(address>>16)&255,df=(address>>8)&255,off=address&255;
 if((db==11&&dd==0&&df==0)||(db==0&&dd==3&&df==1)){
  CHECK(w==EfiPciWidthUint32&&n==1&&off<64&&off%4==0);
  memcpy(data,(db==11?config:(UINT8*)other_config[0])+off,4);return EFI_SUCCESS;
 }
#endif
 CHECK(p==&root_pci&&w==EfiPciWidthUint32&&n==1&&(address>>24)==11);UINTN d=(address>>16)&255,f=(address>>8)&255;
 *(UINT32*)data=d?0xffffffffU:f==0?0x270210deU:f==1?0x22bb10deU:f==2&&app_mode==A_UNKNOWN_FUNCTION?0x12345678U:0xffffffffU;return EFI_SUCCESS;
}
#ifdef GU_PCI_SNAPSHOT
static EFI_STATUS EFIAPI diag_configuration(EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *p,VOID **out){
 static UINT8 r[48];(void)p;memset(r,0,sizeof(r));r[0]=0x8a;r[1]=43;r[3]=2;r[22]=255;r[39]=1;r[46]=0x79;
#ifdef GU_VGA_RESTORE
 if(app_mode==A_VGA_ROOT_CHANGE&&(fake.pci_attributes&0x18)==0x18&&connect_calls)r[14]=1;
#endif
 *out=r;return EFI_SUCCESS;
}
#endif
static EFI_STATUS EFIAPI locate_path(EFI_GUID *g,EFI_DEVICE_PATH_PROTOCOL **path,EFI_HANDLE *h){CHECK(guid(g,&pg)&&path&&*path);*h=*path==(VOID*)boot_path?H(22):*path==(VOID*)usb_path?H(23):connect_calls&&*path==(VOID*)restored_path&&app_mode==A_RESTORE_WRONG_GPU?H(99):H(3);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI disconnect(EFI_HANDLE h,EFI_HANDLE agent,EFI_HANDLE child){
 CHECK(loaded_windows&&GuScreenDetached&&!child&&!exclusive&&!fake.mmio_writes&&!fake.flrs&&!fake.maps);
 if(h==H(10)){
  CHECK(bound&&agent==H(30+consumer_calls)&&consumer_calls<3);UINTN role=consumer_calls++;roles[role]=FALSE;
  if(role==2&&app_mode==A_EDID_DROPS_INPUT)shared_input=FALSE;
  return (app_mode==A_OUT_ERROR&&role==0)||(app_mode==A_GFX_ERROR&&role==1)||(app_mode==A_EDID_ERROR&&role==2)?EFI_DEVICE_ERROR:EFI_SUCCESS;
 }
 CHECK(h==H(3)&&agent==H(8)&&bound&&consumer_calls==3&&!parent_calls);parent_calls++;
 if(app_mode==A_PARENT_NOOP)return EFI_SUCCESS;
#ifdef GU_VGA_RESTORE
 other_config[0][15]&=~0x80000U;
#endif
 bound=FALSE;config[4]&=(UINT8)~7U;fake.pci_attributes=0;
 return app_mode==A_PARENT_ERROR?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI attributes(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op,UINT64 mask,UINT64 *out){
 CHECK(p==&pci);
#ifdef GU_VGA_RESTORE
 if(op==EfiPciIoAttributeOperationGet&&!Gu.owned&&!connect_calls){*out=fake.pci_attributes;return EFI_SUCCESS;}
 if(!exclusive&&connect_calls&&op==EfiPciIoAttributeOperationSupported){*out=0x718;return EFI_SUCCESS;}
 if(!exclusive&&connect_calls&&op==EfiPciIoAttributeOperationEnable){CHECK(mask==0x18&&!out);fake.pci_attributes|=mask;
 if(app_mode!=A_VGA_NO_EFFECT)other_config[0][15]|=0x80000U;
 if(app_mode==A_VGA_EXTRA_CHANGE)other_config[0][8]^=16;
 return app_mode==A_VGA_API_ERROR?EFI_DEVICE_ERROR:EFI_SUCCESS;}
#endif
#ifdef GU_NATIVE_GOP_TEST
 if(op==EfiPciIoAttributeOperationGet&&!exclusive&&connect_calls){
  if(app_mode==A_NATIVE_ATTR_ERROR)return EFI_DEVICE_ERROR;
  *out=fake.pci_attributes;
  if(app_mode==A_NATIVE_ATTR_BME)*out|=EFI_PCI_IO_ATTRIBUTE_BUS_MASTER;
  if(app_mode==A_NATIVE_ATTR_NO_IO)*out&=~EFI_PCI_IO_ATTRIBUTE_IO;
  if(app_mode==A_NATIVE_ATTR_NO_MEMORY)*out&=~EFI_PCI_IO_ATTRIBUTE_MEMORY;
  return EFI_SUCCESS;
 }
#endif
 CHECK(exclusive&&Gu.owned);
 if(mask==EFI_PCI_IO_ATTRIBUTE_BUS_MASTER){
  if(op==EfiPciIoAttributeOperationDisable&&app_mode==A_MASTER_DISABLE_ERROR)return EFI_DEVICE_ERROR;
  EFI_STATUS s=master_attributes(p,op,mask,out);
  return op==EfiPciIoAttributeOperationEnable&&app_mode==A_MASTER_ENABLE_ERROR?EFI_DEVICE_ERROR:s;
 }
 if(op==EfiPciIoAttributeOperationSupported||op==EfiPciIoAttributeOperationGet)return master_attributes(p,op,mask,out);
 CHECK(op==EfiPciIoAttributeOperationEnable&&mask&&!(mask&~(EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY))&&!out&&!(config[4]&4));
 if(mask&EFI_PCI_IO_ATTRIBUTE_IO)config[4]|=1;if(mask&EFI_PCI_IO_ATTRIBUTE_MEMORY)config[4]|=2;fake.pci_attributes|=mask;
 return app_mode==A_DECODE_ERROR_MATCH?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI bar_info(EFI_PCI_IO_PROTOCOL *p,UINT8 bar,UINT64 *support,VOID **out){
 CHECK(p==&pci&&!bar);if(support)*support=0;EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *r;
 CHECK(!EFI_ERROR(allocate_pool(EfiLoaderData,sizeof(*r)+2,(VOID**)&r)));r->Desc=ACPI_ADDRESS_SPACE_DESCRIPTOR;r->Len=sizeof(*r)-3;
 r->ResType=0;r->AddrSpaceGranularity=32;r->AddrRangeMin=0xfb000000;r->AddrRangeMax=0xffffff;r->AddrLen=0x1000000;
 ((UINT8*)(r+1))[0]=ACPI_END_TAG_DESCRIPTOR;((UINT8*)(r+1))[1]=0;*out=r;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI file_close(EFI_FILE_PROTOCOL *f){FILE_FAKE *v=(VOID*)f;CHECK(v->open&&!Gu.retained);v->open=FALSE;
 return (v->kind==10&&app_mode==A_BOOT_CLOSE)||(v->kind==9&&app_mode==A_ROOT_CLOSE)||
 (v->kind==4&&app_mode==A_ASSET_CLOSE)||(v->kind==3&&app_mode==A_REPORT_CLOSE)?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI file_open(EFI_FILE_PROTOCOL *f,EFI_FILE_PROTOCOL **out,CHAR16 *name,UINT64 mode,UINT64 attr){
 FILE_FAKE *v=(VOID*)f;char path[128];narrow(path,sizeof(path),name);CHECK(v->open);*out=NULL;int target=-1;
 if(v->kind==9&&!strcmp(path,"\\EFI\\Microsoft\\Boot\\bootmgfw.efi")){if(app_mode==A_BOOT_MISSING)return EFI_NOT_FOUND;target=10;}
 else if(v->kind==0&&!strcmp(path,"\\RTX4080-EXPERIMENT\\RUN00\\REPORT.TXT")){CHECK(mode==(EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE));target=3;}
 else if(v->kind==0&&!strcmp(path,"\\RTX4080-EXPERIMENT"))target=1;
 else if(v->kind==1&&!strncmp(path,"RUN",3)){
  if(!(mode&EFI_FILE_MODE_CREATE))return EFI_NOT_FOUND;CHECK(attr==EFI_FILE_DIRECTORY&&!run_created);run_created=TRUE;target=2;
 }else if(v->kind==2&&!strcmp(path,"REPORT.TXT")){
  if(!(mode&EFI_FILE_MODE_CREATE))return EFI_NOT_FOUND;CHECK(run_created&&!report_bytes);target=3;
 }else if(v->kind==2&&!strcmp(path,"PROM.BIN")){
  if(!(mode&EFI_FILE_MODE_CREATE))return EFI_NOT_FOUND;target=11;
 }else if(v->kind==1){for(int i=0;i<5;i++){const char *base=strrchr(originals[i].name,'/')+1;if(!strncmp(path,"ASSETS\\",7)&&!strcmp(path+7,base))target=4+i;}}
 CHECK(target>=0&&!files[target].open);files[target].open=TRUE;files[target].position=0;*out=&files[target].protocol;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI file_info(EFI_FILE_PROTOCOL *f,EFI_GUID *g,UINTN *n,VOID *data){
 FILE_FAKE *v=(VOID*)f;CHECK(guid(g,&ig)&&v->open&&*n>=sizeof(EFI_FILE_INFO));memset(data,0,sizeof(EFI_FILE_INFO));EFI_FILE_INFO *i=data;i->Size=sizeof(*i);
 if(v->kind<3||v->kind==9)i->Attribute=EFI_FILE_DIRECTORY;
 if(v->kind>=4&&v->kind<=8)i->FileSize=originals[v->kind-4].firmware.size+(v->kind==4&&app_mode==A_ASSET_SIZE?1:0);
 *n=sizeof(*i);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI file_read(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *data){
 FILE_FAKE *v=(VOID*)f;CHECK(v->open);
 if(v->kind==10){CHECK(*n==2);if(app_mode==A_BOOT_READ_ERROR)return EFI_DEVICE_ERROR;memcpy(data,app_mode==A_BOOT_BAD_MZ?"XX":"MZ",2);if(app_mode==A_BOOT_SHORT)*n=1;return EFI_SUCCESS;}
 CHECK(v->kind>=4&&v->kind<=8);const struct firmware *asset=&originals[v->kind-4].firmware;UINTN remain=asset->size-v->position;
 if(v->kind==4&&app_mode==A_ASSET_READ_ERROR)return EFI_DEVICE_ERROR;if(*n>remain)*n=remain;
 if(v->kind==4&&app_mode==A_ASSET_SHORT&&*n)(*n)--;
 memcpy(data,asset->data+v->position,*n);if(v->kind==4&&app_mode==A_ASSET_HASH&&!v->position&&*n)((UINT8*)data)[0]^=1;v->position+=*n;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI file_write(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *data){
 FILE_FAKE *v=(VOID*)f;CHECK(v->open&&current_tpl==TPL_APPLICATION);
 if(v->kind==11){CHECK(*n==sizeof(prom)&&!Gu.started);return EFI_SUCCESS;}
 CHECK(v->kind==3&&data);
#ifdef GU_NATIVE_GOP_TEST
 if((app_mode==A_NATIVE_SUPPORTED_LOG&&*n>=30&&!memcmp(data,"stage=before_NativeGOP_Supported",30))||
    (app_mode==A_NATIVE_DRIVER_START_LOG&&*n>=32&&!memcmp(data,"stage=before_NativeGOP_DriverStart",32)))return EFI_DEVICE_ERROR;
#endif
 const char *parentStage="stage=before_ConnectParentNonRecursive",*childStage="stage=before_ConnectDisplayChild";
 BOOLEAN parentBoundary=*n>=strlen(parentStage)&&!memcmp(data,parentStage,strlen(parentStage));
 BOOLEAN childBoundary=*n>=strlen(childStage)&&!memcmp(data,childStage,strlen(childStage));
 if((app_mode==A_PARENT_LOG_WRITE&&parentBoundary)||(app_mode==A_CHILD_LOG_WRITE&&childBoundary))return EFI_DEVICE_ERROR;
 if((app_mode==A_PARENT_LOG_FLUSH&&parentBoundary)||(app_mode==A_CHILD_LOG_FLUSH&&childBoundary))stage_flush_error=TRUE;
 /* Core fault injection begins at the real transaction boundary. Earlier
  * profile/topology reads must not accidentally consume a core fault. */
 if(*n>=18&&!memcmp(data,"stage=transaction\n",18))fake.mode=requested_core;
#ifdef GU_NATIVE_GOP_TEST
 if(*n==12&&!memcmp(data,"stage=start\n",12)){
  if(app_mode==A_NATIVE_BEFORE_START_COMMAND)config[5]^=1;
  if(app_mode==A_NATIVE_BEFORE_START_BAR)config[0x10]^=0x10;
  if(app_mode==A_NATIVE_BEFORE_START_ATTR)fake.pci_attributes|=EFI_PCI_IO_ATTRIBUTE_BUS_MASTER;
 }
#endif
#ifdef GU_VGA_RESTORE
 if(app_mode==A_VGA_BEFORE_START_LOST&&*n==12&&!memcmp(data,"stage=start\n",12))other_config[0][15]&=~0x80000U;
#endif
 if(app_mode==A_BEFORE_START_SS&&*n==12&&!memcmp(data,"stage=start\n",12))registers[SS0/4]^=1;
 if(app_mode==A_CONSUMER_LOG_ERROR&&consumer_calls)return EFI_DEVICE_ERROR;
 if(app_mode==A_REPORT_SHORT&&*n)(*n)--;CHECK(report_bytes+*n<sizeof(report_text));memcpy(report_text+report_bytes,data,*n);report_bytes+=*n;report_text[report_bytes]=0;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI file_position(EFI_FILE_PROTOCOL *f,UINT64 position){CHECK(f==&files[3].protocol&&files[3].open&&position==~(UINT64)0);files[3].position=report_bytes;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI file_flush(EFI_FILE_PROTOCOL *f){CHECK(((FILE_FAKE*)f)->open);if(app_mode==A_REPORT_FLUSH||stage_flush_error)return EFI_DEVICE_ERROR;flushed_bytes=report_bytes;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI volume(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *p,EFI_FILE_PROTOCOL **root){CHECK(p==&fs||p==&windows_fs);int k=p==&fs?0:9;CHECK(!files[k].open);files[k].open=TRUE;*root=&files[k].protocol;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI load_image(BOOLEAN policy,EFI_HANDLE parent,EFI_DEVICE_PATH_PROTOCOL *path,VOID *src,UINTN n,EFI_HANDLE *out){
#ifdef GU_NATIVE_GOP_TEST
 if(src){
  CHECK(!policy&&parent==H(1)&&path&&n==sizeof(NativeGopImage)&&!memcmp(src,NativeGopImage,n));
  CHECK(!exclusive&&!Gu.owned&&Gu.quiesced&&fake.flrs==1&&!Gu.allocations&&!fake.live_maps&&!bound);
  CHECK(!memcmp(path,rtx_path,sizeof(rtx_path)-4));UINT8 *node=(UINT8*)path+sizeof(rtx_path)-4;
  CHECK(node[0]==MEDIA_DEVICE_PATH&&node[1]==MEDIA_RELATIVE_OFFSET_RANGE_DP&&node[2]==24&&node[24]==127);
  UINT64 begin,end;memcpy(&begin,node+8,8);memcpy(&end,node+16,8);CHECK(begin==0xfc00&&end==0x24bff);
  CHECK(!legacy_registered&&isolation_calls==1);notification_selection();
  CHECK(native_loads++==0&&flushed_bytes==report_bytes);if(app_mode==A_NATIVE_LOAD_ERROR)return EFI_SECURITY_VIOLATION;
  *out=H(61);return EFI_SUCCESS;
 }
#endif
 CHECK(!policy&&parent==H(1)&&path&&!src&&!n&&!consumer_calls&&!parent_calls&&!fake.mmio_writes);load_calls++;
 /* Verify the actual constructed file-path node, not only the volume handle. */
 CHECK(!memcmp(path,boot_path,sizeof(boot_path)-4));UINT8 *node=(UINT8*)path+sizeof(boot_path)-4;CHECK(node[0]==MEDIA_DEVICE_PATH&&node[1]==MEDIA_FILEPATH_DP);
 char text[128];narrow(text,sizeof(text),(CHAR16*)(node+4));CHECK(!strcmp(text,"\\EFI\\Microsoft\\Boot\\bootmgfw.efi"));
 UINTN length=node[2]+((UINTN)node[3]<<8);CHECK(node[length]==END_DEVICE_PATH_TYPE&&node[length+1]==END_ENTIRE_DEVICE_PATH_SUBTYPE);
 if(app_mode==A_LOAD_ERROR)return EFI_LOAD_ERROR;loaded_windows=TRUE;*out=H(14);windows_image.DeviceHandle=app_mode==A_LOAD_WRONG_VOLUME?H(99):H(12);
 windows_image.ImageCodeType=app_mode==A_LOAD_WRONG_TYPE?EfiBootServicesCode:EfiLoaderCode;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI unload_image(EFI_HANDLE h){CHECK(h==H(14)&&loaded_windows&&!consumer_calls);unload_calls++;loaded_windows=FALSE;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI connect(EFI_HANDLE h,EFI_HANDLE *drivers,EFI_DEVICE_PATH_PROTOCOL *path,BOOLEAN recursive){
 CHECK(flushed_bytes==report_bytes);
 CHECK(!path&&!recursive&&loaded_windows&&!exclusive&&Gu.quiesced&&!Gu.allocations&&fake.flrs==1&&GuScreenDetached);
 CHECK(registers[SS0/4]==0x88888888&&registers[SS1/4]==8&&!fake.live_maps&&!fake.live_buffers);
 if(h==H(3)){
#ifdef GU_NATIVE_GOP_TEST
  /* Parent connections in this suite must come only from our direct Start
   * service model, never from BS.ConnectController candidate enumeration. */
  CHECK(native_dispatch&&native_supported==1&&native_bind_starts==1&&!legacy_registered);
#endif
  CHECK(strstr(report_text,"stage=after_CloseProtocol status=0")&&strstr(report_text,"stage=before_ConnectParentNonRecursive"));
  CHECK(drivers&&drivers[0]==
#ifdef GU_NATIVE_GOP_TEST
   H(60)&&native_loads==1&&native_starts==1
#else
   H(8)
#endif
   &&!drivers[1]&&!connect_calls&&!child_connect_calls);connect_calls++;
  if(app_mode==A_PARENT_NO_RETURN){Gu.retained=TRUE;longjmp(escape_halt,3);}
  if(app_mode==A_GOP_NOOP)return EFI_SUCCESS;
  bound=TRUE;DisplayHandle=app_mode==A_REUSE_CHILD?H(10):H(11);
  roles[0]=roles[1]=roles[2]=app_mode==A_CONSUMERS_EARLY;
  config[4]|=7;fake.pci_attributes=EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY|EFI_PCI_IO_ATTRIBUTE_BUS_MASTER;
  if(app_mode==A_RESTORE_WRONG_PATH)restored_path[12]^=1;
#ifdef GU_NATIVE_GOP_TEST
  if(app_mode==A_NATIVE_LATE_CODE)native_code[0x280]^=1;
  if(app_mode==A_NATIVE_WRONG_ADR)restored_path[24]=MEDIA_DEVICE_PATH;
  if(app_mode==A_OK||app_mode>=A_NATIVE_CONSOLE_INITIALIZES){gop_mode.FrameBufferBase=gop_mode.FrameBufferSize=0;gop_mode.Mode=15;gop_mode.MaxMode=1;gop.Mode=&gop_mode;}
  if(app_mode==A_NATIVE_INFO_POINTER_NULL){gop_mode.Info=NULL;gop_mode.SizeOfInfo=0;}
  if(app_mode==A_NATIVE_MODE_POINTER_NULL)gop.Mode=NULL;
#endif
  if(app_mode==A_RESTORE_LOST_INPUT)shared_input=FALSE;
#ifdef GU_NATIVE_GOP_TEST
  config[4]&=(UINT8)~4U;fake.pci_attributes&=~EFI_PCI_IO_ATTRIBUTE_BUS_MASTER;
  if(app_mode==A_GOP_COMMAND||app_mode==A_NATIVE_BME_ON)config[4]|=4;
  if(app_mode==A_NATIVE_NO_IO)config[4]&=(UINT8)~1U;
  if(app_mode==A_NATIVE_NO_MEMORY)config[4]&=(UINT8)~2U;
  if(app_mode==A_NATIVE_COMMAND_EXTRA)config[5]^=1;
#else
  if(app_mode==A_GOP_COMMAND)config[4]&=(UINT8)~4U;
#endif
 if(app_mode==A_GOP_BAR)config[0x10]^=0x10;
  return app_mode==A_GOP_ERROR?EFI_DEVICE_ERROR:EFI_SUCCESS;
 }
 CHECK(h==DisplayHandle&&!drivers&&connect_calls==1&&!child_connect_calls&&bound);
#ifdef GU_NATIVE_GOP_TEST
 notification_selection();
#endif
 CHECK(strstr(report_text,"stage=new_display_child_verified")&&strstr(report_text,"stage=before_ConnectDisplayChild"));
 CHECK(app_mode==A_REUSE_CHILD||h!=H(10));child_connect_calls++;
 if(app_mode==A_CHILD_NO_RETURN){Gu.retained=TRUE;longjmp(escape_halt,3);}
 roles[0]=roles[1]=roles[2]=TRUE;
#ifdef GU_NATIVE_GOP_TEST
 if(app_mode==A_NATIVE_PARENT_CHANGED)native_binding.Version++;
 if(app_mode==A_OK||app_mode>=A_NATIVE_CONSOLE_INITIALIZES){
  CHECK(strstr(report_text,"phase=STRUCTURE")&&strstr(report_text,"state=UNINITIALIZED"));
  if(app_mode!=A_NATIVE_CONSOLE_MODE_FAIL){gop_mode.Mode=0;gop_mode.FrameBufferBase=0x4000000000ULL;gop_mode.FrameBufferSize=0x300000;gop_mode.Info=&gop_info;gop_mode.SizeOfInfo=sizeof(gop_info);}
 }
 if(app_mode==A_NATIVE_CONSOLE_FB_ZERO)gop_mode.FrameBufferBase=gop_mode.FrameBufferSize=0;
 if(app_mode==A_NATIVE_CONSOLE_FB_OVERFLOW){gop_mode.FrameBufferBase=MAX_UINT64-8;gop_mode.FrameBufferSize=0x300000;}
 if(app_mode==A_NATIVE_CONSOLE_MODE_OOB)gop_mode.Mode=gop_mode.MaxMode;
 if(app_mode==A_NATIVE_CONSOLE_INFO_SIZE)gop_mode.SizeOfInfo=4;
 if(app_mode==A_NATIVE_CONSOLE_DMA_REAPPEARS)Gu.dma_count=1;
 if(app_mode==A_NATIVE_MODE_CHANGE){gop_mode.FrameBufferBase+=0x100000;gop_mode.FrameBufferSize=0x200000;gop_info.HorizontalResolution=gop_info.PixelsPerScanLine=800;gop_info.VerticalResolution=600;}
 if(app_mode==A_NATIVE_MODE_INVALID)gop_mode.FrameBufferSize=4;
#endif
 if(app_mode==A_GOP_SS0)registers[SS0/4]=0x88888889;if(app_mode==A_GOP_SS1)registers[SS1/4]=9;
 if(app_mode==A_GOP_EDID||app_mode==A_CHILD_NOT_FOUND_MISSING)roles[2]=FALSE;if(app_mode==A_GOP_CONSOLE)st.ConsoleOutHandle=H(99);
 if(app_mode==A_CHILD_ERROR)return EFI_DEVICE_ERROR;
 return app_mode==A_CHILD_NOT_FOUND_COMPLETE||app_mode==A_CHILD_NOT_FOUND_MISSING?EFI_NOT_FOUND:EFI_SUCCESS;
}
#ifdef GU_NATIVE_GOP_TEST
EFI_STATUS GuHostNativeSupported(EFI_DRIVER_BINDING_PROTOCOL *b,EFI_HANDLE h,EFI_DEVICE_PATH_PROTOCOL *path){
 CHECK(b==&native_binding&&h==H(3)&&!path&&!native_supported++&&!native_bind_starts&&!legacy_registered&&current_tpl==TPL_APPLICATION);
 CHECK(flushed_bytes==report_bytes&&strstr(report_text,"stage=before_NativeGOP_Supported"));notification_selection();
 if(app_mode==A_NATIVE_SUPPORTED_DIRTY){exclusive=TRUE;return EFI_SUCCESS;}
 return app_mode==A_NATIVE_SUPPORTED_ERROR?EFI_UNSUPPORTED:app_mode==A_NATIVE_SUPPORTED_ALREADY?EFI_ALREADY_STARTED:EFI_SUCCESS;
}
EFI_STATUS GuHostNativeStart(EFI_DRIVER_BINDING_PROTOCOL *b,EFI_HANDLE h,EFI_DEVICE_PATH_PROTOCOL *path){
 CHECK(b==&native_binding&&h==H(3)&&!path&&native_supported==1&&!native_bind_starts++&&!legacy_registered&&current_tpl==TPL_APPLICATION);
 CHECK(flushed_bytes==report_bytes&&strstr(report_text,"stage=before_NativeGOP_DriverStart"));notification_selection();
 if(app_mode==A_NATIVE_BIND_START_ERROR)return EFI_DEVICE_ERROR;
 EFI_HANDLE drivers[]={H(60),NULL};native_dispatch=TRUE;EFI_STATUS s=connect(h,drivers,NULL,FALSE);native_dispatch=FALSE;return s;
}
#endif
static EFI_STATUS EFIAPI start_image(EFI_HANDLE h,UINTN *bytes,CHAR16 **data){
#ifdef GU_PCI_SNAPSHOT_NORMAL
 CHECK(h==H(14)&&bytes&&data&&loaded_windows&&!consumer_calls&&!parent_calls&&!connect_calls&&!fake.flrs&&!fake.mmio_writes);
 CHECK(strstr(report_text,"PCI-END complete=1 expected_stages=2"));start_calls++;transfer_calls++;longjmp(escape_halt,2);
#endif
#ifdef GU_NATIVE_GOP_TEST
 if(h==H(61)){CHECK(native_loads==1&&native_starts++==0&&!exclusive&&Gu.quiesced&&GuScreenDetached&&flushed_bytes==report_bytes&&!legacy_registered);
  notification_selection();if(app_mode==A_NATIVE_NOTIFY_REINSTALL)legacy_registered=TRUE;
  *bytes=0;*data=NULL;return app_mode==A_NATIVE_START_ERROR?EFI_DEVICE_ERROR:EFI_SUCCESS;}
#endif
 CHECK(h==H(14)&&bytes&&data&&loaded_windows&&bound&&!exclusive&&!GuScreenDetached&&connect_calls==1&&roles[0]&&roles[1]&&roles[2]);
#ifdef GU_NATIVE_GOP_TEST
#ifdef GU_VGA_RESTORE
 CHECK(config[4]==3&&fake.pci_attributes==0x318&&(other_config[0][15]>>16)==0x18);
#else
 CHECK(config[4]==3&&fake.pci_attributes==(EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY));
#endif
#else
 CHECK(config[4]==7);
#endif
 CHECK(registers[SS0/4]==0x88888888&&registers[SS1/4]==8&&!Gu.allocations&&!fake.live_maps&&!fake.live_buffers&&Gu.quiesced&&!files[3].open);
 CHECK(strstr(report_text,"stage=before_StartImage")&&strstr(report_text,"POST-GOP SS0="));start_calls++;
 if(app_mode==A_START_RETURNS)return EFI_ABORTED;transfer_calls++;longjmp(escape_halt,2);
}
static void hex_hash(const VOID *data,UINTN bytes,char out[65]){UINT8 digest[32];static const char digits[]="0123456789abcdef";GuSha256(data,bytes,digest);for(UINTN i=0;i<32;i++){out[2*i]=digits[digest[i]>>4];out[2*i+1]=digits[digest[i]&15];}out[64]=0;}
static void app_setup(enum app_case mode,enum scenario core){
#ifdef GU_NATIVE_GOP_TEST
 DynamicMode=mode==A_DYNAMIC_NO_PROTOCOL?1:mode==A_DYNAMIC_INFO_FAIL?2:mode==A_DYNAMIC_SIZE_FAIL?3:mode==A_DYNAMIC_DECODE_FAIL?4:mode==A_DYNAMIC_HASH_FAIL?5:0;
#endif
 setup(SUCCESS);requested_core=core;app_mode=mode;memcpy(config,captured_config,256);GuHostResetTopology();memset(&st,0,sizeof(st));memset(&rt,0,sizeof(rt));memset(files,0,sizeof(files));
 report_bytes=console_bytes=consumer_calls=parent_calls=connect_calls=child_connect_calls=start_calls=transfer_calls=exclusive_calls=unload_calls=load_calls=non_gpu_writes=0;
 DisplayHandle=H(10);memcpy(restored_path,rtx_path,sizeof(rtx_path));
#ifdef GU_NATIVE_GOP_TEST
 native_loads=native_starts=native_supported=native_bind_starts=legacy_dispatches=notification_attempts=isolation_calls=0;legacy_registered=TRUE;native_dispatch=FALSE;memcpy(native_code,NativeGopImage,sizeof(native_code));
 native_image=(EFI_LOADED_IMAGE_PROTOCOL){0};native_image.ImageBase=native_code;native_image.ImageSize=sizeof(native_code);native_image.DeviceHandle=H(3);native_image.ImageCodeType=EfiBootServicesCode;
 native_binding=(EFI_DRIVER_BINDING_PROTOCOL){0};native_binding.DriverBindingHandle=H(60);native_binding.ImageHandle=H(61);native_binding.Version=0x70011;
 native_binding.Supported=(EFI_DRIVER_BINDING_SUPPORTED)(UINTN)(native_code+0x3c8);native_binding.Start=(EFI_DRIVER_BINDING_START)(UINTN)(native_code+0x704);native_binding.Stop=(EFI_DRIVER_BINDING_STOP)(UINTN)(native_code+0x6ec);
 UINT8 tail[]={ACPI_DEVICE_PATH,ACPI_ADR_DP,8,0,0,3,1,0x80,127,255,4,0};memcpy(restored_path+sizeof(rtx_path)-4,tail,sizeof(tail));memcpy(inactive_path,restored_path,sizeof(inactive_path));inactive_path[28]=0x20;
 if(mode==A_NATIVE_CODE)native_code[0x280]^=1;if(mode==A_NATIVE_TYPE)native_image.ImageCodeType=EfiLoaderCode;
 if(mode==A_NATIVE_DEVICE)native_image.DeviceHandle=H(99);if(mode==A_NATIVE_VERSION)native_binding.Version++;
 if(mode==A_NATIVE_START_POINTER)native_binding.Start=(EFI_DRIVER_BINDING_START)(UINTN)(native_code+0x705);
#endif
 cold_ss_reads=flushed_bytes=0;prom_changed=stage_flush_error=FALSE;
 report_text[0]=console_text[0]=0;bound=shared_input=TRUE;roles[0]=roles[1]=roles[2]=TRUE;graph_race=exclusive=prompt=key_sent=key_reset=run_created=loaded_windows=FALSE;current_tpl=TPL_APPLICATION;
 st.BootServices=&bs;st.RuntimeServices=&rt;st.ConOut=st.StdErr=&conout;st.ConIn=&conin;st.ConsoleOutHandle=st.StandardErrorHandle=H(6);st.ConsoleInHandle=H(40);
 conout.OutputString=output;conin.Reset=input_reset;conin.ReadKeyStroke=key;rt.GetVariable=variable;
 bs.Stall=app_stall;bs.SetWatchdogTimer=watchdog;bs.HandleProtocol=handle;bs.LocateHandleBuffer=locate;bs.LocateHandle=locate_fixed;
 bs.ProtocolsPerHandle=protocols;bs.OpenProtocolInformation=open_info;bs.OpenProtocol=open_protocol;bs.CloseProtocol=close_protocol;bs.RaiseTPL=raise_tpl;bs.RestoreTPL=restore_tpl;
 bs.DisconnectController=disconnect;bs.ConnectController=connect;bs.LoadImage=load_image;bs.UnloadImage=unload_image;bs.StartImage=start_image;bs.LocateDevicePath=locate_path;
#ifdef GU_NATIVE_GOP_TEST
 bs.UninstallProtocolInterface=uninstall_binding;bs.LocateProtocol=DynamicLocate;
#endif
 pci.GetLocation=location;pci.Attributes=attributes;pci.GetBarAttributes=bar_info;pci.Pci.Read=app_config_read;pci.Mem.Read=app_memory_read;replacement_pci=pci;
 for(UINTN i=0;i<sizeof(rom_image);i++)rom_image[i]=(UINT8)(i*17+53);hex_hash(rom_image,sizeof(rom_image),GuHostRomSha256);
#ifdef GU_MEASURED_PROFILE_TEST
 memcpy(rom_image,measured_rom,sizeof(rom_image));
 registers[SS0/4]=0x46326143;registers[SS1/4]=2;registers[0x1183a4/4]=0x3ff8;
 registers[0x1fa7cc/4]=0x4cb8f;registers[0x9a0148/4]=0xffffff0f;registers[0x1fa7c4/4]=0x4cb8f;
#endif
 pci.RomImage=rom_image;pci.RomSize=sizeof(rom_image);loaded.DeviceHandle=H(2);fs.OpenVolume=windows_fs.OpenVolume=volume;
 memset(&owner_image,0,sizeof(owner_image));memset(&owner_binding,0,sizeof(owner_binding));
 owner_binding.DriverBindingHandle=H(8);owner_binding.ImageHandle=H(9);owner_binding.Version=0x24;owner_binding.Stop=(EFI_DRIVER_BINDING_STOP)(UINTN)(owner_code+0x7c4);
 owner_image.ImageBase=owner_code;owner_image.ImageSize=sizeof(owner_code);owner_image.ImageCodeType=EfiBootServicesCode;
 for(UINTN i=0;i<3;i++){
  for(UINTN j=0;j<sizeof(consumer_code[i]);j++)consumer_code[i][j]=(UINT8)(j+29*i);
  GuSha256(consumer_code[i]+0x20,64,expected_stop_hash[i]);
  consumer_paths[i].node=(EFI_DEVICE_PATH_PROTOCOL){MEDIA_DEVICE_PATH,MEDIA_PIWG_FW_FILE_DP,{20,0}};consumer_paths[i].file=consumer_guids[i];consumer_paths[i].end=(EFI_DEVICE_PATH_PROTOCOL){END_DEVICE_PATH_TYPE,END_ENTIRE_DEVICE_PATH_SUBTYPE,{4,0}};
  consumer_bindings[i]=(EFI_DRIVER_BINDING_PROTOCOL){0};consumer_bindings[i].DriverBindingHandle=consumer_bindings[i].ImageHandle=H(30+i);consumer_bindings[i].Version=(UINT32)(0x100+i);consumer_bindings[i].Stop=(EFI_DRIVER_BINDING_STOP)(UINTN)(consumer_code[i]+0x20);
  consumer_images[i]=(EFI_LOADED_IMAGE_PROTOCOL){0};consumer_images[i].ImageBase=consumer_code[i];consumer_images[i].ImageSize=sizeof(consumer_code[i]);consumer_images[i].ImageCodeType=i==2?EfiRuntimeServicesCode:EfiBootServicesCode;consumer_images[i].FilePath=&consumer_paths[i].node;
 }
#ifdef GU_MEASURED_PROFILE_TEST
 const R4_DRIVER_PROFILE *parent=&Run0009Drivers[0];
 memcpy(owner_code+parent->stop,measured_stop[0],64);
 owner_path.node=(EFI_DEVICE_PATH_PROTOCOL){MEDIA_DEVICE_PATH,MEDIA_PIWG_FW_FILE_DP,{20,0}};owner_path.file=parent->file;owner_path.end=(EFI_DEVICE_PATH_PROTOCOL){127,255,{4,0}};owner_image.FilePath=&owner_path.node;
 for(UINTN i=0;i<3;i++){
  const R4_DRIVER_PROFILE *p=&Run0009Drivers[i+1];memcpy(consumer_code[i]+p->stop,measured_stop[i+1],64);
  consumer_paths[i].file=p->file;consumer_bindings[i].Version=p->version;consumer_bindings[i].Stop=(EFI_DRIVER_BINDING_STOP)(UINTN)(consumer_code[i]+p->stop);
  consumer_images[i].ImageSize=p->size;consumer_images[i].ImageCodeType=p->code_type;
 }
#endif
 memset(other_config,0,sizeof(other_config));
 UINT32 ids[]={0x14831022,0x02859005,0x22681d79,0x12341022,0x22bb10de},subs[]={0,0x02d59005,0x22681d79,0,0x470219da},classes[]={0x06040000,0x01040009,0x01080203,0x0c033000,0x040300a1};
 for(UINTN i=0;i<5;i++){other_pci[i]=(EFI_PCI_IO_PROTOCOL){0};other_pci[i].GetLocation=location;other_pci[i].Pci.Read=other_read;other_pci[i].Pci.Write=other_write;other_config[i][0]=ids[i];other_config[i][1]=i==4?0:7;other_config[i][2]=classes[i];other_config[i][11]=subs[i];}
 other_config[0][3]=0x10000;other_config[0][6]=0x000b0b00;other_config[0][15]=0x00180000;root_pci=(EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL){0};root_pci.Pci.Read=root_read;
#ifdef GU_PCI_SNAPSHOT
 root_pci.Configuration=diag_configuration;
#ifdef GU_VGA_RESTORE
 fake.pci_attributes=0x718;other_config[0][7]=0xf1f1;other_config[0][8]=0xfc00fb00;other_config[0][9]=0xe1f1d001;
#endif
#endif
 for(int i=0;i<12;i++){files[i].kind=i;files[i].protocol.Open=file_open;files[i].protocol.Close=file_close;files[i].protocol.GetInfo=file_info;files[i].protocol.Read=file_read;files[i].protocol.Write=file_write;files[i].protocol.Flush=file_flush;files[i].protocol.SetPosition=file_position;}
 gop=(EFI_GRAPHICS_OUTPUT_PROTOCOL){0};gop_mode=(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE){0};gop_info=(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION){0};gop.Mode=&gop_mode;gop_mode.Info=&gop_info;gop_mode.SizeOfInfo=sizeof(gop_info);gop_mode.MaxMode=1;
 gop_mode.FrameBufferBase=0x4000000000ULL;gop_mode.FrameBufferSize=0x300000;gop_info.HorizontalResolution=gop_info.PixelsPerScanLine=1024;gop_info.VerticalResolution=768;gop_info.PixelFormat=PixelBlueGreenRedReserved8BitPerColor;
 if(mode==A_ROM_SIZE)pci.RomSize--;if(mode==A_ROM_HASH)rom_image[0]^=1;if(mode==A_PROM_HASH)registers[0x300000/4]^=1;
 if(mode==A_INITIAL_SS0)registers[SS0/4]=0xffffffff;if(mode==A_INITIAL_SS1)registers[SS1/4]=0xbadf5620;if(mode==A_INITIAL_VRAM)registers[0x1183a4/4]^=1;
 if(mode==A_RUN03_SS||mode==A_RUN03_UNSTABLE||mode==A_RUN03_READ_ERROR){registers[SS0/4]=0x46326143;registers[SS1/4]=2;}
 if(mode==A_RUN04_SS||mode==A_RUN04_UNSTABLE||mode==A_RUN04_READ_ERROR){registers[SS0/4]=0x46366143;registers[SS1/4]=2;}
 if(mode==A_ZERO_SS)registers[SS0/4]=registers[SS1/4]=0;
 if(mode==A_SS0_BADF)registers[SS0/4]=0xbadf0001;if(mode==A_SS1_FFFF)registers[SS1/4]=0xffffffff;
 if(mode==A_CONSUMER_VERSION)consumer_bindings[0].Version++;if(mode==A_CONSUMER_HASH)consumer_code[1][
#ifdef GU_MEASURED_PROFILE_TEST
 Run0009Drivers[2].stop
#else
 0x20
#endif
 ]^=1;
 if(mode==A_CONSUMER_TYPE)consumer_images[2].ImageCodeType=EfiBootServicesCode;if(mode==A_CONSUMER_GUID)consumer_paths[2].file.Data1++;
 if(mode==A_AUDIO_BME)other_config[4][1]=6;
}
int main(int argc,char **argv){
 setvbuf(stdout,NULL,_IONBF,0);
 CHECK(argc==4||argc==5||argc==6||argc==7);load_fixture_assets(argv[1],argv[2]);for(UINTN i=0;i<5;i++)originals[i]=GuAssets[i];
 size_t bytes;UINT8 *data=read_file(argv[3],&bytes);CHECK(bytes==256&&GuHashMatches(data,bytes,GU_CAPTURE_PCI_SHA256));memcpy(captured_config,data,bytes);free(data);captured_config_valid=TRUE;
#ifdef GU_MEASURED_PROFILE_TEST
 CHECK(argc>=6);size_t rom_bytes;UINT8 *rom=read_file(argv[4],&rom_bytes);CHECK(rom_bytes==sizeof(measured_rom)&&GuHashMatches(rom,rom_bytes,GU_UEFI_ROM_SHA256));memcpy(measured_rom,rom,rom_bytes);free(rom);
#ifdef GU_NATIVE_GOP_TEST
 UINT32 decoded=0,scratchBytes=0;CHECK(!EFI_ERROR(EfiGetInfo(measured_rom+0xfc50,0x14fb0,&decoded,&scratchBytes))&&decoded==sizeof(NativeGopImage));
 VOID *scratch=malloc(scratchBytes);CHECK(scratch);CHECK(!EFI_ERROR(EfiDecompress(measured_rom+0xfc50,0x14fb0,NativeGopImage,decoded,scratch,scratchBytes)));free(scratch);
#endif
 const char *roles[]={"parent","text","gfx","edid"};for(UINTN i=0;i<4;i++){char path[1024];snprintf(path,sizeof(path),"%s/%s-stop64.bin",argv[5],roles[i]);size_t n;UINT8 *data=read_file(path,&n);CHECK(n==64);memcpy(measured_stop[i],data,64);free(data);}
#endif
 const enum scenario injected[]={FWSEC_TIMEOUT,SEC2_RUN_TIMEOUT,MAP_FAIL,MAP_PARTIAL,BME_READBACK_FAIL,FLR_WRITE_FAIL,SELECTOR_LOST,AD_SS0_REJECT,AD_SS1_REJECT,UNMAP_FAIL};
#ifdef GU_PCI_SNAPSHOT_NORMAL
 const enum app_case normal_cases[]={A_OK,A_BOOT_MISSING,A_BOOT_MULTI,A_LOAD_ERROR,A_LOAD_WRONG_VOLUME,A_REPORT_SHORT,A_REPORT_FLUSH};
 for(unsigned ni=0;ni<sizeof(normal_cases)/sizeof(*normal_cases);ni++){
  app_setup(normal_cases[ni],SUCCESS);int normal_jump=setjmp(escape_halt);
  volatile EFI_STATUS normal_status=EFI_SUCCESS;
  if(!normal_jump)normal_status=GuHostApplicationRun(H(1),&st);
  if(ni==0)CHECK(normal_jump==2&&transfer_calls==1);
  else CHECK(normal_jump!=2&&!transfer_calls&&EFI_ERROR(normal_status));
  CHECK(!fake.flrs&&!fake.mmio_writes&&!parent_calls&&!consumer_calls&&!connect_calls);
 }
 printf("PASS measured-normal application 7 scenarios; no unlock/reset/GOP dispatch\n");return 0;
#endif
 unsigned total=A_COUNT+sizeof(injected)/sizeof(*injected),executed=0;
 for(unsigned i=0;i<total;i++){
  if(argc==5&&i!=(unsigned)atoi(argv[4]))continue;BOOLEAN injected_case=i>=A_COUNT;enum scenario core=injected_case?injected[i-A_COUNT]:SUCCESS;enum app_case mode=injected_case?A_OK:(enum app_case)i;
  #ifdef GU_NATIVE_GOP_TEST
  if(argc==7&&mode!=A_OK&&(mode<A_DYNAMIC_NO_PROTOCOL||mode>A_DYNAMIC_HASH_FAIL))continue;
#endif
  app_setup(mode,core);CHECK(EfiMain(H(1),&st)==EFI_ACCESS_DENIED);CHECK(!consumer_calls&&!parent_calls&&!load_calls&&!fake.mmio_writes);
  volatile EFI_STATUS result=EFI_ABORTED;int jumped=setjmp(escape_halt);if(!jumped)result=GuHostApplicationRun(H(1),&st);
  BOOLEAN success=(mode==A_OK||mode==A_AUDIO_BME||mode==A_RUN03_SS||mode==A_RUN04_SS||mode==A_ZERO_SS||mode==A_CHILD_NOT_FOUND_COMPLETE||mode==A_CONSUMERS_EARLY||mode==A_REUSE_CHILD)&&!injected_case;
#ifdef GU_NATIVE_GOP_TEST
  if(mode==A_DYNAMIC_INFO_FAIL)CHECK(strstr(report_text,"extract_stage=decompress_get_info"));
  if(mode==A_DYNAMIC_SIZE_FAIL)CHECK(strstr(report_text,"extract_stage=decompress_sizes"));
  if(mode==A_DYNAMIC_DECODE_FAIL)CHECK(strstr(report_text,"extract_stage=decompress"));
  if(mode==A_DYNAMIC_HASH_FAIL)CHECK(strstr(report_text,"extract_stage=gop_sha256_pe"));
  if(mode==A_DYNAMIC_NO_PROTOCOL)CHECK(strstr(report_text,"native-GOP dynamic: LocateProtocol status="));
  if(mode==A_OK)CHECK(strstr(report_text,"extract_stage=complete"));
  if(mode>=A_DYNAMIC_NO_PROTOCOL&&mode<=A_DYNAMIC_HASH_FAIL)CHECK(!prompt&&!key_sent&&!consumer_calls&&!parent_calls&&!native_loads&&!native_starts&&!fake.mmio_writes&&!fake.flrs&&!transfer_calls&&!loaded_windows);
  if((mode==A_NATIVE_INACTIVE_OUTPUT||mode==A_NATIVE_MODE_CHANGE||mode==A_NATIVE_CONSOLE_INITIALIZES||mode==A_NATIVE_COMMAND_7_TO_3)&&!injected_case)success=TRUE;
  CHECK(native_loads<=1&&native_starts<=1);
  CHECK(native_supported<=1&&native_bind_starts<=1&&!legacy_dispatches);
  if(success)CHECK(native_loads==1&&native_starts==1&&native_supported==1&&native_bind_starts==1&&isolation_calls==1&&!legacy_registered&&notification_attempts>=4&&strstr(report_text,"stage=native_GOP_binding_verified"));
  if(mode>=A_NATIVE_LOAD_ERROR&&mode<=A_NATIVE_START_POINTER)CHECK(!connect_calls&&!start_calls);
  if(mode==A_NATIVE_SUPPORTED_ERROR||mode==A_NATIVE_SUPPORTED_ALREADY||mode==A_NATIVE_SUPPORTED_DIRTY)CHECK(native_supported==1&&!native_bind_starts&&!connect_calls&&!start_calls);
  if(mode==A_NATIVE_BIND_START_ERROR)CHECK(native_supported==1&&native_bind_starts==1&&!connect_calls&&!start_calls);
  if(mode==A_NATIVE_ISOLATION_ERROR||mode==A_NATIVE_SHARED_LEGACY||mode==A_NATIVE_ISOLATION_NOOP||mode==A_NATIVE_LEGACY_ALIAS)CHECK(!native_loads&&!native_supported&&!native_bind_starts&&!start_calls);
  if(mode==A_NATIVE_ISOLATION_LATE_OWNER)CHECK(!isolation_calls&&!native_loads&&!native_supported&&!start_calls&&legacy_registered);
  if(mode==A_NATIVE_ISOLATION_LOST_EDGE||mode==A_NATIVE_ISOLATION_EXTRA_EDGE)CHECK(isolation_calls==1&&!native_loads&&!native_supported&&!start_calls&&!legacy_registered);
  if(mode==A_NATIVE_NOTIFY_REINSTALL)CHECK(native_starts==1&&!native_supported&&!start_calls);
  if(mode==A_NATIVE_MODE_INVALID)CHECK(child_connect_calls==1&&!start_calls);
  if(mode>=A_NATIVE_CONSOLE_INITIALIZES&&mode!=A_NATIVE_CONSOLE_INITIALIZES&&mode!=A_NATIVE_COMMAND_7_TO_3){
   CHECK(!start_calls&&!transfer_calls);
   if(mode==A_NATIVE_MODE_POINTER_NULL)CHECK(!child_connect_calls&&strstr(report_text,"reason=MODE_POINTER"));
   else if(mode==A_NATIVE_INFO_POINTER_NULL)CHECK(!child_connect_calls&&strstr(report_text,"reason=MODE_INFO_POINTER"));
   else CHECK(child_connect_calls==1);
  }
  if(mode==A_NATIVE_CONSOLE_MODE_FAIL||mode==A_NATIVE_CONSOLE_MODE_OOB)CHECK(strstr(report_text,"reason=MODE_UNSET"));
  if(mode==A_NATIVE_CONSOLE_FB_ZERO)CHECK(strstr(report_text,"reason=FRAMEBUFFER_ZERO"));
  if(mode==A_NATIVE_CONSOLE_FB_OVERFLOW)CHECK(strstr(report_text,"reason=FRAMEBUFFER_OVERFLOW"));
  if(mode==A_NATIVE_CONSOLE_INFO_SIZE)CHECK(strstr(report_text,"reason=MODE_INFO_SIZE"));
  if(mode==A_NATIVE_CONSOLE_DMA_REAPPEARS)CHECK(strstr(report_text,"stage=POST_GOP_DMA")&&Gu.dma_count==1&&!start_calls);
  if(mode==A_NATIVE_COMMAND_7_TO_3)CHECK(strstr(report_text,"actual=0x3 initial=0x7 expected=0x3 BME=0")&&strstr(report_text,"stage=BEFORE_START_ATTRIBUTES")&&transfer_calls==1);
  if(mode>=A_NATIVE_NO_IO)CHECK(!start_calls&&!transfer_calls);
  if(mode==A_NATIVE_NO_IO||mode==A_NATIVE_NO_MEMORY)CHECK(strstr(report_text,"phase=POST_GOP reason=DECODE_MISSING"));
  if(mode==A_NATIVE_COMMAND_EXTRA)CHECK(strstr(report_text,"phase=POST_GOP reason=UNEXPECTED_COMMAND_BITS"));
  if(mode==A_NATIVE_BME_ON)CHECK(strstr(report_text,"phase=POST_GOP reason=BME_ENABLED"));
  if(mode==A_NATIVE_ATTR_BME||mode==A_NATIVE_ATTR_NO_IO||mode==A_NATIVE_ATTR_NO_MEMORY||mode==A_NATIVE_ATTR_ERROR)CHECK(strstr(report_text,"phase=POST_GOP reason=ATTRIBUTES_INCONSISTENT"));
  if(mode==A_NATIVE_BEFORE_START_COMMAND)CHECK(strstr(report_text,"phase=BEFORE_START reason=UNEXPECTED_COMMAND_BITS"));
  if(mode==A_NATIVE_BEFORE_START_ATTR)CHECK(strstr(report_text,"phase=BEFORE_START reason=ATTRIBUTES_INCONSISTENT"));
  if(mode==A_NATIVE_BEFORE_START_BAR)CHECK(strstr(report_text,"stage=BEFORE_START_BAR offset=0x10"));

  if(mode==A_NATIVE_PARENT_TPL||mode==A_NATIVE_SUPPORTED_LOG)CHECK(!native_supported&&!native_bind_starts&&!start_calls);
  if(mode==A_NATIVE_DRIVER_START_LOG)CHECK(native_supported==1&&!native_bind_starts&&!start_calls);
#endif
  if(success&&jumped!=2){fprintf(stderr,"APP unexpected stop status=%llx jumped=%d\n%s\n",(unsigned long long)result,jumped,report_text);}
  if(success){CHECK(jumped==2&&transfer_calls==1&&!Gu.retained&&consumer_calls==3&&parent_calls==1&&connect_calls==1&&fake.flrs==1&&fake.sec2_starts==1);
#ifdef GU_PCI_SNAPSHOT
   CHECK(strstr(report_text,"PCI-END complete=1 expected_stages=8"));
#endif
  }
  else {CHECK(jumped!=2&&!transfer_calls);if(mode==A_PARENT_NO_RETURN||mode==A_CHILD_NO_RETURN){CHECK(jumped==3&&Gu.retained);}else if(consumer_calls||parent_calls){CHECK(jumped==1&&Gu.retained);}else{CHECK(EFI_ERROR(result)&&!fake.mmio_writes&&!fake.maps&&!fake.flrs&&!loaded_windows);}}
  CHECK(!non_gpu_writes&&consumer_calls<=3&&parent_calls<=1&&connect_calls<=1&&child_connect_calls<=1&&start_calls<=1&&fake.gsp_starts<=1&&fake.sec2_starts<=1&&fake.flrs<=1);
  if(mode==A_ESCAPE||mode==A_TIMEOUT)CHECK(load_calls==1&&unload_calls==1&&!consumer_calls);
  if(mode==A_OUT_ERROR||mode==A_GFX_ERROR||mode==A_EDID_ERROR||mode==A_EDID_DROPS_INPUT||mode==A_CONSUMER_LOG_ERROR)CHECK(!parent_calls&&!exclusive_calls&&!fake.maps&&!fake.mmio_writes);
  if(mode>=A_PARENT_ERROR&&mode<=A_MASTER_DISABLE_ERROR)CHECK(consumer_calls==3&&parent_calls==1&&!start_calls);
  if(mode>=A_GOP_ERROR&&mode<=A_GOP_CONSOLE)CHECK(fake.flrs==1&&connect_calls==1&&!start_calls);
  if(mode==A_CLOSE_EXCLUSIVE_ERROR)CHECK(!connect_calls&&!start_calls&&strstr(report_text,"stage=after_CloseProtocol status=0x8000000000000007"));
  if(mode==A_GOP_ERROR)CHECK(strstr(report_text,"stage=after_ConnectParentNonRecursive status=0x8000000000000007")&&!child_connect_calls);
  if(mode==A_GOP_PCI_MISSING)CHECK(!start_calls&&strstr(report_text,"stage=after_GOP_PCI_refresh status=0x800000000000000e"));
  if(mode==A_START_RETURNS)CHECK(strstr(report_text,"stage=after_Windows_StartImage status=0x8000000000000015 returned=1"));
  if(mode==A_RUN03_UNSTABLE||mode==A_RUN04_UNSTABLE)CHECK(!consumer_calls&&strstr(report_text,"reason=UNSTABLE_REPEAT"));
  if(mode==A_RUN03_READ_ERROR||mode==A_RUN04_READ_ERROR)CHECK(!consumer_calls&&strstr(report_text,"reason=READ_ERROR"));
  if(mode==A_POST_PROM_CHANGED||mode==A_CONFIRM_SS_CHANGED)CHECK(!consumer_calls&&strstr(report_text,"reason=CHANGED_WITHIN_BOOT"));
  if(mode>=A_RESTORE_WRONG_OWNER&&mode<=A_RESTORE_PARENT_GOP)CHECK(!child_connect_calls&&!start_calls);
  if(mode==A_CHILD_NOT_FOUND_COMPLETE)CHECK(child_connect_calls==1&&transfer_calls==1&&strstr(report_text,"child EFI_NOT_FOUND accepted"));
  if(mode==A_CHILD_NOT_FOUND_MISSING||mode==A_CHILD_ERROR)CHECK(child_connect_calls==1&&!start_calls);
  if(mode==A_CONSUMERS_EARLY)CHECK(!child_connect_calls&&strstr(report_text,"stage=display_consumers_already_restored"));
  if(mode==A_PARENT_NO_RETURN)CHECK(strstr(report_text,"stage=before_ConnectParentNonRecursive")&&!strstr(report_text,"stage=after_ConnectParentNonRecursive"));
  if(mode==A_CHILD_NO_RETURN)CHECK(strstr(report_text,"stage=after_ConnectParentNonRecursive")&&strstr(report_text,"stage=before_ConnectDisplayChild")&&!strstr(report_text,"stage=after_ConnectDisplayChild"));
  if(mode==A_PARENT_LOG_WRITE||mode==A_PARENT_LOG_FLUSH)CHECK(!connect_calls&&!child_connect_calls&&!start_calls);
  if(mode==A_CHILD_LOG_WRITE||mode==A_CHILD_LOG_FLUSH)CHECK(connect_calls==1&&!child_connect_calls&&!start_calls);
  if(mode==A_RESTORE_EXTRA_EDGE||mode==A_RESTORE_LOST_INPUT)CHECK(!child_connect_calls&&!start_calls);
  if(injected_case)CHECK(parent_calls==1&&exclusive_calls==1&&!start_calls);
  printf("AD103 10de:2702 APP %02u %s core=%u consumers=%llu parent=%llu FLR=%u GOP-parent=%llu GOP-child=%llu StartImage=%llu transfer=%llu halt=%u PASS\n",i,names[mode],(unsigned)core,(unsigned long long)consumer_calls,(unsigned long long)parent_calls,(unsigned)fake.flrs,(unsigned long long)connect_calls,(unsigned long long)child_connect_calls,(unsigned long long)start_calls,(unsigned long long)transfer_calls,Gu.retained);
  dispose_host_fixture();executed++;
 }
 for(UINTN i=0;i<5;i++)free((VOID*)originals[i].firmware.data);
#ifdef GU_MEASURED_PROFILE_TEST
 printf("%u AD103 10de:2702 measured-profile application + core scenarios PASS; real PROM/PCI/assets/RAM ROM and WPC initial values; modeled EFI/PCI/MMIO/DMA/Windows actions; test EFI entry LOCKED.\n",executed);
 puts("MEASURED PROFILE PASS: WPC initial registers and real RAM ROM; RUN0009 pinned driver identities/Stop windows. EFI services and hardware actions remain models; application graph model is separate from full RUN0009 replay.");
#else
 printf("%u AD103 10de:2702 application + core scenarios PASS; real PROM/PCI/assets; synthetic cold/ROM/driver identities and EFI/PCI/MMIO/DMA/Windows services; actual EFI entry LOCKED.\n",executed);
#endif
return 0;
}

