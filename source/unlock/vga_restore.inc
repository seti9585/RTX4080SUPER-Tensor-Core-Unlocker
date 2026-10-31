/* Standard Attributes only; no direct bridge writes or retry/fallback. */
static EFI_STATUS VgaRead(SNAP *s){
 memset(s,0,sizeof(*s));
 for(UINTN i=0;i<target_count;i++){
  EFI_STATUS e=Header(&targets[i],s->cfg[i]);if(EFI_ERROR(e))return e;
  UINT32 again[16];e=Header(&targets[i],again);if(EFI_ERROR(e))return e;
  again[1]=(again[1]&0xffff)|(s->cfg[i][1]&0xffff0000U);
  if(memcmp(again,s->cfg[i],sizeof(again)))return EFI_NOT_READY;
 }
 return Resources(s->resources,&s->resource_len);
}
static BOOLEAN VgaSame(const SNAP *a,const SNAP *b,BOOLEAN initial,BOOLEAN restored){
 if(a->resource_len!=b->resource_len||memcmp(a->resources,b->resources,a->resource_len))return FALSE;
 for(UINTN i=0;i<2;i++)for(UINTN j=0;j<16;j++){
  UINT32 expected=a->cfg[i][j],actual=b->cfg[i][j];
  if(j==1){expected&=0xffff;actual&=0xffff;if(initial&&i==0)expected&=~4U;}
  if(i==1&&j==15){if(restored)expected|=0x80000U;else if(initial)expected&=~0x80000U;}
  if(actual!=expected){GuLog("VGA mismatch node=%zu offset=%x expected=%x actual=%x\n",(size_t)i,(unsigned)(j*4),expected,actual);return FALSE;}
 }
 return TRUE;
}
static VOID VgaLogSnapshot(const char *phase,const SNAP *s){
 for(UINTN i=0;i<target_count;i++)for(UINTN j=0;j<16;j++)
  GuLog("VGA-REG phase=%s node=%zu offset=%02x value=%08x\n",phase,(size_t)i,(unsigned)(j*4),s->cfg[i][j]);
 for(UINTN n=0;n<s->resource_len;n+=16){
  static const char digits[]="0123456789abcdef";char hex[33];UINTN count=s->resource_len-n;if(count>16)count=16;
  for(UINTN k=0;k<count;k++){hex[2*k]=digits[s->resources[n+k]>>4];hex[2*k+1]=digits[s->resources[n+k]&15];}hex[2*count]=0;
  GuLog("VGA-ROOT phase=%s offset=%zu data=%s\n",phase,(size_t)n,hex);
 }
}
EFI_STATUS GuRestoreVga(BOOLEAN verifyOnly){
 /* Application-only sequential gate; avoid EFI stack probing for buffers. */
 static SNAP before,after;UINT64 attrs=0,supported=0;EFI_STATUS s;
 if(!root||target_count!=2||invalid||!snaps[0].taken||EFI_ERROR(snaps[0].status)||
 targets[0].segment||targets[0].bus!=11||targets[0].device||targets[0].function||
 targets[1].segment||targets[1].bus||targets[1].device!=3||targets[1].function!=1||targets[1].id!=0x14831022||
 snaps[0].cfg[0][0]!=GU_BOARD_ID||(snaps[0].cfg[0][1]&0xffff)!=7||
 (snaps[0].cfg[1][1]&0xffff)!=7||(snaps[0].cfg[1][15]>>16)!=0x18||
 snaps[0].cfg[1][6]!=0x000b0b00||snaps[0].cfg[1][8]!=0xfc00fb00||snaps[0].cfg[1][9]!=0xe1f1d001||
 !Gu.quiesced||Gu.owned||Gu.allocations||Gu.dma_count||Gu.fault||Gu.master_attributes_pending||Gu.retained)return EFI_NOT_READY;
 if(verifyOnly&&!vga_restored)return EFI_NOT_READY;
 s=VgaRead(&before);if(EFI_ERROR(s))return s;
 if(!VgaSame(&snaps[0],&before,TRUE,verifyOnly))return EFI_COMPROMISED_DATA;
 s=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationGet,0,&attrs);if(EFI_ERROR(s))return s;
 if((attrs&0x700)!=0x300||(attrs&~0x18ULL)!=(initial_vga_attributes&~0x418ULL))return EFI_COMPROMISED_DATA;
 if(verifyOnly)return (attrs&0x18)==0x18&&GuLogReady()?EFI_SUCCESS:EFI_DEVICE_ERROR;
 s=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationSupported,0,&supported);if(EFI_ERROR(s)||(supported&0x18)!=0x18)return EFI_UNSUPPORTED;
 GuLog("stage=before_VGA_restore mask=0x18 command=%x bridge_control=%x attributes=%llx\n",before.cfg[0][1]&0xffff,before.cfg[1][15]>>16,(unsigned long long)attrs);
 VgaLogSnapshot("before",&before);
 if(!GuLogReady())return EFI_DEVICE_ERROR;
 EFI_STATUS enabled=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationEnable,0x18,NULL);
 s=VgaRead(&after);
 EFI_STATUS got=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationGet,0,&attrs);
 GuLog("stage=after_VGA_restore api=%llx read=%llx get=%llx command=%x bridge_control=%x attributes=%llx\n",(unsigned long long)enabled,(unsigned long long)s,(unsigned long long)got,after.cfg[0][1]&0xffff,after.cfg[1][15]>>16,(unsigned long long)attrs);
 if(!EFI_ERROR(s))VgaLogSnapshot("after",&after);
 if(EFI_ERROR(enabled)||EFI_ERROR(s)||EFI_ERROR(got)||!GuLogReady())return EFI_DEVICE_ERROR;
 if(!VgaSame(&before,&after,FALSE,TRUE)||attrs!=(initial_vga_attributes&~0x400ULL))return EFI_COMPROMISED_DATA;
 vga_restored=TRUE;return EFI_SUCCESS;
}
