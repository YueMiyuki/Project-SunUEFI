// SPDX-License-Identifier: BSD-2-Clause-Patent
#include "PianoProductBootLog.h"
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

#ifndef PIANO_PRODUCT_DISPLAY_TEST
#define PIANO_PRODUCT_DISPLAY_TEST 0
#endif
#if PIANO_PRODUCT_DISPLAY_TEST
#include <Library/BaseLib.h>
#include "PianoDisplayClockObserve.h"
STATIC struct {CONST CHAR8 *Name;UINT32 Stage;EFI_STATUS Status,GccStatus;UINT32 Gcc[2];} mCheckpoints[32];
STATIC UINT32 mCheckpointCount,mCheckpointReplayCalls;
#endif

STATIC CONST CHAR8 *mNames[]={"DISPLAY","PAYLOAD","SMEM","INPUT","UFS","USB","MENU"};
STATIC CONST UINT8 mLetters[26][5]={
  {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},
  {0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},{0x7f,0x09,0x09,0x09,0x01},
  {0x3e,0x41,0x49,0x49,0x7a},{0x7f,0x08,0x08,0x08,0x7f},{0x00,0x41,0x7f,0x41,0x00},
  {0x20,0x40,0x41,0x3f,0x01},{0x7f,0x08,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},
  {0x7f,0x02,0x0c,0x02,0x7f},{0x7f,0x04,0x08,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},
  {0x7f,0x09,0x09,0x09,0x06},{0x3e,0x41,0x51,0x21,0x5e},{0x7f,0x09,0x19,0x29,0x46},
  {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7f,0x01,0x01},{0x3f,0x40,0x40,0x40,0x3f},
  {0x1f,0x20,0x40,0x20,0x1f},{0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,0x08,0x14,0x63},
  {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}
};
STATIC CONST UINT8 mDigits[10][5]={
  {0x3e,0x51,0x49,0x45,0x3e},{0x00,0x42,0x7f,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},
  {0x21,0x41,0x45,0x4b,0x31},{0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},
  {0x3c,0x4a,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},
  {0x06,0x49,0x49,0x29,0x1e}
};
STATIC struct {
  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
  PIANO_PRODUCT_BOOT_LOG_ALIVE Alive;
  UINT32 Width,Height,X,Y,Scale,Pitch,PanelWidth;
  EFI_STATUS Fault;
} mCanvas;

STATIC EFI_GRAPHICS_OUTPUT_BLT_PIXEL Color(UINT8 R,UINT8 G,UINT8 B){
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel={B,G,R,0};return Pixel;
}
STATIC EFI_STATUS Rect(EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel,UINT32 X,UINT32 Y,UINT32 W,UINT32 H){
  if(mCanvas.Fault!=EFI_SUCCESS)return mCanvas.Fault;
  if(!mCanvas.Alive())return mCanvas.Fault=EFI_ABORTED;
  if(!W||!H||X>=mCanvas.Width||Y>=mCanvas.Height||W>mCanvas.Width-X||H>mCanvas.Height-Y)
    return mCanvas.Fault=EFI_BAD_BUFFER_SIZE;
  EFI_STATUS Status=mCanvas.Gop->Blt(mCanvas.Gop,&Pixel,EfiBltVideoFill,0,0,X,Y,W,H,0);
  if(!mCanvas.Alive())return mCanvas.Fault=EFI_ABORTED;
  if(Status!=EFI_SUCCESS)mCanvas.Fault=EFI_ERROR(Status)?Status:EFI_DEVICE_ERROR;
  return mCanvas.Fault;
}
STATIC CONST UINT8 *Glyph(CHAR8 Ch){
  STATIC CONST UINT8 Space[5]={0},Dash[5]={8,8,8,8,8};
  if(Ch>='a'&&Ch<='z')Ch=(CHAR8)(Ch-'a'+'A');
  if(Ch>='A'&&Ch<='Z')return mLetters[Ch-'A'];
  if(Ch>='0'&&Ch<='9')return mDigits[Ch-'0'];
  return Ch=='-'?Dash:Space;
}
STATIC EFI_STATUS Text(CONST CHAR8 *String,UINT32 X,UINT32 Y,EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel){
  UINT32 Scale=mCanvas.Scale;
  for(UINT32 I=0;String[I];++I){
    CONST UINT8 *G=Glyph(String[I]);
    for(UINT32 Column=0;Column<5;++Column){
      UINT32 Row=0;
      while(Row<7){
        if(!(G[Column]&(1U<<Row))){++Row;continue;}
        UINT32 First=Row;
        while(Row<7&&(G[Column]&(1U<<Row)))++Row;
        EFI_STATUS Status=Rect(Pixel,X+(I*6+Column)*Scale,Y+First*Scale,Scale,(Row-First)*Scale);
        if(Status!=EFI_SUCCESS)return Status;
      }
    }
  }
  return EFI_SUCCESS;
}
EFI_STATUS PianoProductDisplayCheckpoint(CONST CHAR8 *Name,UINT32 Stage){
#if PIANO_PRODUCT_DISPLAY_TEST
  if(!mCanvas.Gop||!mCanvas.Alive||!mCanvas.Alive())return EFI_NOT_READY;
  UINT32 Gcc[2]={0};
  EFI_STATUS GccStatus=PianoDisplayClockSampleGcc(mCanvas.Alive,Gcc);
  if(PianoDisplayClockRetained()||!mCanvas.Alive()){CpuDeadLoop();return EFI_ABORTED;}
  STATIC CONST UINT8 Colors[][3]={{32,64,160},{0,130,100},{180,80,0},{130,40,160},{20,110,170}};
  UINT32 Index=Stage%(sizeof(Colors)/sizeof(Colors[0]));
  EFI_STATUS Status=Rect(Color(Colors[Index][0],Colors[Index][1],Colors[Index][2]),0,0,mCanvas.Width,mCanvas.Height);
  if(Status==EFI_SUCCESS)Status=Rect(Color(245,245,245),mCanvas.Width/8,mCanvas.Height/4,mCanvas.Width*3/4,mCanvas.Height/2);
  UINT32 SavedScale=mCanvas.Scale,Number=mCheckpointCount+1;
  CHAR8 NumberText[3]={(CHAR8)('0'+Number/10),(CHAR8)('0'+Number%10),0};
  mCanvas.Scale=mCanvas.Width/80;
  if(Status==EFI_SUCCESS)Status=Text(NumberText,mCanvas.Width/6,mCanvas.Height/3,Color(20,20,20));
  mCanvas.Scale=mCanvas.Width/180;
  if(Status==EFI_SUCCESS)Status=Text(Name,mCanvas.Width/6,mCanvas.Height/2,Color(20,20,20));
  mCanvas.Scale=SavedScale;
  DEBUG((DEBUG_WARN,"PIANO_DISPLAY_CHECKPOINT name=%a stage=%u status=%r gop=%p base=%lx mode=%u\n",
    Name,Stage,Status,mCanvas.Gop,mCanvas.Gop->Mode->FrameBufferBase,mCanvas.Gop->Mode->Mode));
  if(mCheckpointCount<ARRAY_SIZE(mCheckpoints)){
    mCheckpoints[mCheckpointCount].Name=Name;
    mCheckpoints[mCheckpointCount].Stage=Stage;
    mCheckpoints[mCheckpointCount].GccStatus=GccStatus;
    mCheckpoints[mCheckpointCount].Gcc[0]=Gcc[0];mCheckpoints[mCheckpointCount].Gcc[1]=Gcc[1];
    mCheckpoints[mCheckpointCount++].Status=Status;
  }
  if(Status==EFI_SUCCESS && mCanvas.Alive())Status=gBS->Stall(3000000);
  return Status;
#else
  (VOID)Name;(VOID)Stage;return EFI_SUCCESS;
#endif
}
VOID PianoProductDisplayCheckpointReplay(VOID){
#if PIANO_PRODUCT_DISPLAY_TEST
  // GUI file reads can overwrite the early RAM ring. Reemit cached results
  // through the existing GUI pump; no timer, display call or new owner.
  if((++mCheckpointReplayCalls%1024)!=1)return;
  for(UINT32 I=0;I<mCheckpointCount;++I)
    DEBUG((DEBUG_WARN,"PIANO_DISPLAY_CHECKPOINT_SAVED id=%u name=%a stage=%u status=%r gcc=%r ahb=%08x axi=%08x\n",
      I+1,mCheckpoints[I].Name,mCheckpoints[I].Stage,mCheckpoints[I].Status,
      mCheckpoints[I].GccStatus,mCheckpoints[I].Gcc[0],mCheckpoints[I].Gcc[1]));
#endif
}
STATIC BOOLEAN Equal(CONST CHAR8 *A,CONST CHAR8 *B){
  while(*A&&*A==*B){++A;++B;}return *A==*B;
}
STATIC CONST CHAR8 *State(EFI_STATUS Status){
  if(Status==EFI_SUCCESS)return "OK";
  if(Status==EFI_NOT_STARTED)return "WAIT";
  if(Status==EFI_NOT_READY)return "NOT READY";
  if(Status==EFI_NOT_FOUND||Status==EFI_UNSUPPORTED)return "UNAVAILABLE";
  return "FAILED";
}
STATIC VOID Decimal(UINT64 Value,CHAR8 Out[24]){
  CHAR8 Reverse[20];UINT32 Count=0;
  do{Reverse[Count++]=(CHAR8)('0'+Value%10);Value/=10;}while(Value);
  for(UINT32 I=0;I<Count;++I)Out[I]=Reverse[Count-I-1];
  Out[Count++]=' ';Out[Count++]='M';Out[Count++]='S';Out[Count]=0;
}
STATIC VOID Hex(EFI_STATUS Value,CHAR8 Out[19]){
  STATIC CONST CHAR8 Digits[]="0123456789ABCDEF";
  UINT64 Bits=(UINT64)Value;Out[0]='0';Out[1]='X';
  for(UINT32 I=0;I<16;++I)Out[2+I]=Digits[(Bits>>((15-I)*4))&15];
  Out[18]=0;
}
STATIC UINT32 Length(CONST CHAR8 *String){UINT32 Count=0;while(String[Count])++Count;return Count;}
STATIC EFI_STATUS DrawRow(UINT32 Row,CONST CHAR8 *Label,CONST CHAR8 *Elapsed,CONST CHAR8 *Code){
  UINT32 Scale=mCanvas.Scale,Y=mCanvas.Y+Row*mCanvas.Pitch;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL White=Color(255,255,255),Ink=Color(48,48,48),Muted=Color(100,100,100),Orange=Color(238,121,0);
  EFI_STATUS Status=Rect(White,mCanvas.X,Y,mCanvas.PanelWidth,mCanvas.Pitch);
  if(Status!=EFI_SUCCESS)return Status;
  Status=Rect(Orange,mCanvas.X,Y+2*Scale,3*Scale,3*Scale);if(Status!=EFI_SUCCESS)return Status;
  UINT32 X=mCanvas.X+6*Scale;
  Status=Text(mNames[Row],X,Y,Ink);if(Status!=EFI_SUCCESS)return Status;
  Status=Text(Label,X+9*6*Scale,Y,Ink);if(Status!=EFI_SUCCESS)return Status;
  Status=Text(Elapsed,X+23*6*Scale,Y,Muted);if(Status!=EFI_SUCCESS)return Status;
  UINT32 ElapsedColumns=Length(Elapsed);if(ElapsedColumns<8)ElapsedColumns=8;
  return Text(Code,X+(23+ElapsedColumns+2)*6*Scale,Y,Muted);
}
EFI_STATUS PianoProductBootLogInitialize(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop,PIANO_PRODUCT_BOOT_LOG_ALIVE Alive){
  if(!Gop||!Gop->Blt||!Gop->Mode||!Gop->Mode->Info||!Alive)return EFI_INVALID_PARAMETER;
  if(!Alive())return EFI_ABORTED;
  UINT32 Width=Gop->Mode->Info->HorizontalResolution,Height=Gop->Mode->Info->VerticalResolution;
  if(Width<480||Height<320||Width>8192||Height>8192)return EFI_UNSUPPORTED;
  UINT32 Short=Width<Height?Width:Height,Scale=Short/400;
  if(!Scale)Scale=1;
  // Seven 35px rows at the native 3200x2136 mode, with no overlap with the
  // splash wordmark above or its bottom-left setup/key hints below.
  UINT32 Top=Height*60/100,Bottom=Height*79/100;
  if(Bottom-Top<70)Top=Bottom-70;
  while(Scale>1&&(402*Scale>Width-32||70*Scale>Bottom-Top))--Scale;
  mCanvas.Gop=Gop;mCanvas.Alive=Alive;mCanvas.Width=Width;mCanvas.Height=Height;
  mCanvas.Scale=Scale;mCanvas.Pitch=10*Scale;mCanvas.PanelWidth=402*Scale;
  mCanvas.X=Width*6/100;mCanvas.Y=Top;mCanvas.Fault=EFI_SUCCESS;
  for(UINT32 Row=0;Row<ARRAY_SIZE(mNames);++Row){
    EFI_STATUS Status=DrawRow(Row,"--","-- MS","--");if(Status!=EFI_SUCCESS)return Status;
  }
  return EFI_SUCCESS;
}
EFI_STATUS PianoProductBootLogStage(CONST CHAR8 *Name,EFI_STATUS Status,UINT64 ElapsedMs){
  if(!Name)return EFI_INVALID_PARAMETER;
  if(!mCanvas.Gop||!mCanvas.Alive)return EFI_NOT_STARTED;
  if(mCanvas.Fault!=EFI_SUCCESS)return mCanvas.Fault;
  CONST CHAR8 *Label=State(Status);
  UINT32 Row=0;while(Row<ARRAY_SIZE(mNames)&&!Equal(Name,mNames[Row]))++Row;
  // Reuse the UFS row for a precise failure phase; do not add screen clutter.
  STATIC CONST struct {CONST CHAR8 *Name,*Label;} UfsFailures[]={
    {"UFS LINK","LINK FAILED"},{"UFS SMMU","SMMU FAILED"},
    {"UFS DMA","DMA FAILED"},{"UFS NOP","NOP FAILED"},
    {"UFS QUERY","QUERY FAILED"},{"UFS CAPACITY","CAPACITY FAIL"},
    {"UFS GPT","GPT FAILED"},{"UFS BLOCKIO","BLOCKIO FAIL"},
    {"UFS VOLUME","VOLUME FAIL"}};
  if(Row==ARRAY_SIZE(mNames))for(UINTN I=0;I<ARRAY_SIZE(UfsFailures);++I) {
    if(Equal(Name,UfsFailures[I].Name)){Row=4;Label=UfsFailures[I].Label;break;}
  }
  if(Row==ARRAY_SIZE(mNames))return EFI_INVALID_PARAMETER;
  CHAR8 Elapsed[24],Code[19];Decimal(ElapsedMs,Elapsed);Hex(Status,Code);
  return DrawRow(Row,Label,Elapsed,
    Status==EFI_SUCCESS||Status==EFI_NOT_STARTED?"--":Code);
}
