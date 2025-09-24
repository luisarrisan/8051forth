#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "emu8051.h"
#include <assert.h>

int uart_data;
int trace = 0;

static void finish(struct em8051 *aCPU)
{
  fprintf(stderr, "Cycles: %lu, %.3f ms\n", aCPU->cycles, 1000 * aCPU->cycles / 28e6);
  FILE *dump = fopen("dump.bin", "wb");
  fwrite(aCPU->mCodeMem, 32768, 1, dump);
  fclose(dump);
  exit(0);
}

static int
emu_sfrread(struct em8051 *aCPU, int aRegister)
{
  if ((aRegister == 0x86) ||      
      ((uart_data == 0xf9) && (aRegister == 0xf8)) //cc2541.asm uses UART1 -> U1CSR.RX_BYTE
     ) { 
    return 4;  // Pretend UART receive byte always ready
  }
  if (aRegister == uart_data) {
    int c = getchar();
    if (c < 0)
      finish(aCPU);
    return (c == '\n') ? 0x0d : c;
  }
  return aCPU->mSFR[aRegister - 0x80];
}

static void
emu_sfrwrite(struct em8051 *aCPU, int aRegister)
{  
  if (aRegister == uart_data) {
    if(trace!=0) printf("sfr_write aRegister=%02x\n", aRegister);      
    putchar(aCPU->mSFR[uart_data - 0x80]);
    if (uart_data == 0xf9) {      
      aCPU->mSFR[0xe8 - 0x80] = 4; //cc2541.asm uses UART1
    } else {
      aCPU->mSFR[0xe8 - 0x80] = 2;
    }
  }
  //If writing to MPAGE copy the value to P2
  //    so movx through @r0 works as expected in code written for the cc2541
  if (aRegister == 0x93) {
    if(trace!=0) printf("Writing to MPAGE!!!!!!!\n", aRegister);      
    aCPU->mSFR[0xa0 - 0x80]=aCPU->mSFR[0x93 - 0x80];
  }
}

static void
emu_exception(struct em8051 *aCPU, int aCode)
{
  if (aCode == EXCEPTION_ILLEGAL_OPCODE)
    finish(aCPU);
  //assert(0);
}

int main(int argc, char *argv[])
{
  unsigned char* iMem;

  struct em8051 emu;
  memset(&emu, 0, sizeof(emu));

  emu.mCodeMem     = malloc(65536);
  emu.mCodeMemSize = 65536;
  emu.mExtData     = emu.mCodeMem;
  // emu.mExtData     = malloc(65536);
  emu.mExtDataSize = 65536;
  emu.mSFR   = malloc(128);

  if (strcmp(argv[1], "cc1110") == 0) {
    // For TI CC1110:
    emu.mLowerData   = emu.mExtData + 0xff00;
    emu.mUpperData   = emu.mExtData + 0xff80;
    uart_data        = 0xc1;
  } else if (strcmp(argv[1], "generic") == 0) {
    emu.mLowerData   = calloc(128, 1);
    emu.mUpperData   = calloc(128, 1);
    uart_data        = 0x99;
  } else if (strcmp(argv[1], "cc2541") == 0) {
    // For TI CC2541 (HM-10 module):
    free(emu.mCodeMem);
    iMem             = malloc(0x18000);
    emu.mCodeMem     = iMem;
    emu.mCodeMemSize = 0x10000;
    memset(emu.mCodeMem, 0xff, emu.mCodeMemSize);
    emu.mExtData     = iMem + 0x8000;
    emu.mExtDataSize = 0x10000;
	//iData = emu.mExtData + (8*1024) - 256;
    emu.mLowerData   = emu.mExtData + 0x1f00;
    emu.mUpperData   = emu.mExtData + 0x1f80;
    //emu.mLowerData   = emu.mExtData + 0xff00;
    //emu.mUpperData   = emu.mExtData + 0xff80;  
    free(emu.mSFR);
	emu.mSFR         = emu.mExtData + 0x7080;
    uart_data        = 0xf9;
  } else {
    fprintf(stderr, "usage: emu8051 [generic|cc1110] <hexfile>\n");
    exit(1);
  }

  emu.except       = &emu_exception;
  emu.sfrread      = &emu_sfrread;
  emu.sfrwrite     = &emu_sfrwrite;
  emu.xread = NULL;
  emu.xwrite = NULL;
  reset(&emu, 1);
  if (strcmp(argv[1], "cc2541") != 0)
    memset(emu.mCodeMem, 0xff, emu.mCodeMemSize);  

    static const uint8_t df00_block[] = {
0xD3, 0x91, 0xFF, 0x04, 0x45, 0x00, 0x00, 0x0F, 0x00, 0x1E, 0xC4, 0xEC, 0x8C, 0x22, 0x02, 0x22, 
0xF8, 0x47, 0x07, 0x30, 0x04, 0x76, 0x6C, 0x03, 0x40, 0x91, 0x56, 0x10, 0xA9, 0x0A, 0x20, 0x0D, 
0x59, 0x3F, 0x3F, 0x88, 0x11, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x00, 0x7F, 0x80, 0x01, 0x00, 0x94, 0x00, 0x00, 
0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x93, 0xE2, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC7, 0x00, 0x00, 0x0C, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x03, 0x70, 0x6B, 0x87, 0x08, 0x44, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x12, 0x00, 0x0F, 
0x00, 0x00, 0x00, 0x08, 0x33, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x68, 0x90, 
0x00, 0x0A, 0x22, 0x00, 0x02, 0x0C, 0x88, 0x01, 0x00, 0x00, 0x00, 0x08, 0x40, 0x00, 0x40, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x08, 0x40, 0x00, 0x40, 0x00, 
0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00};
    memcpy(emu.mExtData + 0xdf00, df00_block, 0x100);

  int rc = load_obj(&emu, argv[2]);
  if (rc < 0) {
    fprintf(stderr, "load obj error %d\n", rc);
    exit(1);
  }

  int i, inst_bytes;
  unsigned char disasm_buf[50];
  for (i = 0; emu.mPC != 0xffff; i++) {
    // trace |= (emu.mPC == 0xe009);
    if (trace) {
	//if (emu.mPC>0x0c00 && emu.mPC<0x0c52) {
      unsigned int r0 = emu.mLowerData[0];
      unsigned int p2 = emu.mSFR[REG_P2];
      unsigned int tos = emu.mSFR[REG_DPH0] * 256 + emu.mSFR[REG_DPL0];  
	  unsigned int tosm1h = emu.mExtData[p2 * 256 + r0];  
	  unsigned int tosm1l = emu.mLowerData[r0];    
	  unsigned int tosm1 = tosm1h*256 + tosm1l;  
	  unsigned int tosm1o = emu.mExtData[p2 * 256 + r0+1] * 256 + 
	                        emu.mExtData[p2 * 256 + r0];  
      inst_bytes = emu.dec[emu.mCodeMem[emu.mPC]](&emu, emu.mPC, disasm_buf);
      printf("%04x: ", emu.mPC);
      for (int j = 0; j<inst_bytes;   j++){printf("%02x", emu.mCodeMem[emu.mPC+j]);}
      for (int j = 0; j<3-inst_bytes; j++){printf("  ");}
      printf("\t\t%-25s\t\t", disasm_buf);
      printf("DS[r0]=%04x tos=%04x sp=%02x r0=%02x ACC=%02x PSW=%08b P2=%04x\n", 
	          tosm1, tos, emu.mSFR[REG_SP], r0, emu.mSFR[REG_ACC], emu.mSFR[REG_PSW], p2);      
    }
    tick(&emu);
  }

  exit(0);
}
