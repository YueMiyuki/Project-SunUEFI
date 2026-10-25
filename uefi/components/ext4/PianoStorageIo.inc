// SPDX-License-Identifier: BSD-2-Clause-Patent
// Included by the sole UFS owner; no second SMMU domain or DMA allocation.
STATIC PIANO_STORAGE_PARTITION mFilePartitions[3];
STATIC EFI_GUID mPartitionIoGuid=PIANO_PARTITION_IO_PROTOCOL_GUID;
STATIC EFI_STATUS PartitionFromHandle(EFI_HANDLE Handle,PIANO_STORAGE_PARTITION *P){
  EFI_PARTITION_INFO_PROTOCOL *Info=NULL;EFI_STATUS S=gBS->HandleProtocol(Handle,&gEfiPartitionInfoProtocolGuid,(VOID **)&Info);
  if(S!=EFI_SUCCESS||!Info||Info->Type!=PARTITION_TYPE_GPT)return EFI_ACCESS_DENIED;
  CHAR16 Name[37];CopyMem(Name,Info->Info.Gpt.PartitionName,72);Name[36]=0;
  S=PianoStorageOpen(Name,P);if(S!=EFI_SUCCESS)return S;
  return CompareGuid(&P->PartitionGuid,&Info->Info.Gpt.UniquePartitionGUID)&&P->First==Info->Info.Gpt.StartingLBA&&
    P->Blocks==Info->Info.Gpt.EndingLBA-Info->Info.Gpt.StartingLBA+1?EFI_SUCCESS:EFI_MEDIA_CHANGED;
}
STATIC EFI_STATUS EFIAPI FileWrite(PIANO_PARTITION_IO_PROTOCOL *This,EFI_HANDLE Handle,UINT64 At,UINTN Bytes,CONST VOID *Data);
STATIC EFI_STATUS EFIAPI FileFlush(PIANO_PARTITION_IO_PROTOCOL *This,EFI_HANDLE Handle);
STATIC PIANO_PARTITION_IO_PROTOCOL mPartitionIo={1,FileWrite,FileFlush};
EFI_STATUS PianoStorageCloseFilesystems(VOID){
  EFI_TPL Tpl=gBS->RaiseTPL(TPL_HIGH_LEVEL);gBS->RestoreTPL(Tpl);
  if(Tpl!=TPL_APPLICATION)return EFI_UNSUPPORTED;
  EFI_HANDLE *Handles=NULL;UINTN Count=0;
  EFI_STATUS S=gBS->LocateHandleBuffer(ByProtocol,&gEfiSimpleFileSystemProtocolGuid,NULL,&Count,&Handles);
  if(S==EFI_NOT_FOUND)return EFI_SUCCESS;if(S!=EFI_SUCCESS)return S;
  for(UINTN I=0;I<Count;++I){
    EFI_DEVICE_PATH_PROTOCOL *Path=NULL;EFI_GUID Vendor={0xA8675600,0x87D0,0x4A29,{0x9B,0x40,0x60,0,0,0,0,1}};
    if(gBS->HandleProtocol(Handles[I],&gEfiDevicePathProtocolGuid,(VOID **)&Path)!=EFI_SUCCESS||!Path||
       Path->Type!=HARDWARE_DEVICE_PATH||Path->SubType!=HW_VENDOR_DP||DevicePathNodeLength(Path)!=sizeof(VENDOR_DEVICE_PATH)||
       !CompareGuid(&((VENDOR_DEVICE_PATH *)Path)->Guid,&Vendor))continue;
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *Info=NULL;UINTN Drivers=0;
    S=gBS->OpenProtocolInformation(Handles[I],&gEfiDiskIoProtocolGuid,&Info,&Drivers);
    if(S!=EFI_SUCCESS)break;
    // Disconnect the filesystem itself first, not DiskIo. Otherwise removing
    // parent protocols recursively calls Stop at TPL_NOTIFY, too late for IO.
    for(UINTN J=0;J<Drivers;++J)if(Info[J].Attributes&EFI_OPEN_PROTOCOL_BY_DRIVER){
      S=gBS->DisconnectController(Handles[I],Info[J].AgentHandle,NULL);
      if(S!=EFI_SUCCESS)break;
    }
    if(Info)gBS->FreePool(Info);
    VOID *Remaining=NULL;
    if(S==EFI_SUCCESS&&gBS->HandleProtocol(Handles[I],&gEfiSimpleFileSystemProtocolGuid,&Remaining)!=EFI_UNSUPPORTED)S=EFI_ACCESS_DENIED;
    DEBUG((DEBUG_WARN,"PIANO_FILESYSTEM_CLOSE handle=%p status=%r\n",Handles[I],S));
    if(S!=EFI_SUCCESS)break;
  }
  gBS->FreePool(Handles);return S;
}
STATIC EFI_STATUS PartitionGate(VOID){
  if(!mPartitionExecuting||!mBlockBusy||!mBlockLive||mExitRetained||mResetReport.Prepared||mResetReport.Failed||mPartitionFailed||
     !mInstalled||!mContext.Attached||!mContext.Verified||mContext.TableMemory.Quarantined)return EFI_NOT_READY;
  return Read(0x50)==(UINT32)mTrl.DeviceAddress&&Read(0x54)==(UINT32)(mTrl.DeviceAddress>>32)&&
    !Read(0x58)&&!Read(0x78)?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
STATIC BOOLEAN FilePartition(CONST PIANO_STORAGE_PARTITION *P){
  for(UINTN I=0;I<ARRAY_SIZE(mFilePartitions);++I){
    if(P&&P->Identity==&mFilePartitions[I]&&!CompareMem(P,&mFilePartitions[I],sizeof(*P)))return TRUE;
  }
  return FALSE;
}
EFI_STATUS PianoStorageOpen(CONST CHAR16 *Name,PIANO_STORAGE_PARTITION *P){
  if(!Name||!P||!mBlockLive||mBlockBusy||mResetReport.Prepared||mResetReport.Failed||mPartitionFailed)return EFI_NOT_READY;
  STATIC CONST CHAR16 *Names[]={L"persist",L"sunuefi_esp",L"sunuefi_root"};UINTN Slot=0;
  while(Slot<ARRAY_SIZE(Names)&&StrCmp(Name,Names[Slot]))++Slot;
  if(Slot==ARRAY_SIZE(Names))return EFI_ACCESS_DENIED;
  if(mFilePartitions[Slot].Identity){*P=mFilePartitions[Slot];return EFI_SUCCESS;}
  EFI_HANDLE *Handles=NULL;UINTN Count=0,Matches=0;EFI_STATUS S=gBS->LocateHandleBuffer(ByProtocol,&gEfiPartitionInfoProtocolGuid,NULL,&Count,&Handles);
  if(S!=EFI_SUCCESS)return S;
  PIANO_STORAGE_PARTITION Found={0};
  for(UINTN I=0;I<Count;++I){
    EFI_PARTITION_INFO_PROTOCOL *Info=NULL;EFI_DEVICE_PATH_PROTOCOL *Path=NULL;EFI_BLOCK_IO_PROTOCOL *Block=NULL;
    if(gBS->HandleProtocol(Handles[I],&gEfiPartitionInfoProtocolGuid,(VOID **)&Info)!=EFI_SUCCESS||!Info||
       Info->Type!=PARTITION_TYPE_GPT||StrnCmp(Info->Info.Gpt.PartitionName,Name,36))continue;
    if(gBS->HandleProtocol(Handles[I],&gEfiDevicePathProtocolGuid,(VOID **)&Path)!=EFI_SUCCESS||!Path||
       Path->Type!=HARDWARE_DEVICE_PATH||Path->SubType!=HW_VENDOR_DP||DevicePathNodeLength(Path)!=sizeof(VENDOR_DEVICE_PATH))continue;
    EFI_GUID Vendor={0xA8675600,0x87D0,0x4A29,{0x9B,0x40,0x60,0,0,0,0,1}};
    if(!CompareGuid(&((VENDOR_DEVICE_PATH *)Path)->Guid,&Vendor))continue;
    UFS_DEVICE_PATH *U=(VOID *)((UINT8 *)Path+sizeof(VENDOR_DEVICE_PATH));
    if(U->Header.Type!=MESSAGING_DEVICE_PATH||U->Header.SubType!=MSG_UFS_DP||U->Pun||U->Lun>7)continue;
    HARDDRIVE_DEVICE_PATH *Hd=(VOID *)((UINT8 *)U+sizeof(*U));
    if(Hd->Header.Type!=MEDIA_DEVICE_PATH||Hd->Header.SubType!=MEDIA_HARDDRIVE_DP||
       Hd->PartitionStart!=Info->Info.Gpt.StartingLBA||Hd->PartitionSize!=Info->Info.Gpt.EndingLBA-Info->Info.Gpt.StartingLBA+1)continue;
    if(gBS->HandleProtocol(Handles[I],&gEfiBlockIoProtocolGuid,(VOID **)&Block)!=EFI_SUCCESS||!Block||!Block->Media||Block->Media->BlockSize!=4096)continue;
    Found=(PIANO_STORAGE_PARTITION){U->Lun,4096,Block->Media->MediaId,Hd->PartitionStart,Hd->PartitionSize,Info->Info.Gpt.UniquePartitionGUID,&mFilePartitions[Slot]};++Matches;
  }
  gBS->FreePool(Handles);if(Matches!=1)return Matches?EFI_COMPROMISED_DATA:EFI_NOT_FOUND;
  if(!Found.Blocks||Found.First>MAX_UINT32||Found.Blocks-1>MAX_UINT32-Found.First)return EFI_UNSUPPORTED;
  mFilePartitions[Slot]=Found;*P=Found;return EFI_SUCCESS;
}
STATIC EFI_STATUS PartitionTransfer(CONST PIANO_STORAGE_PARTITION *P,UINT64 Block,UINTN Bytes,VOID *Buffer,BOOLEAN Writing){
  if(!FilePartition(P)||!Buffer||!Bytes||Bytes%4096||Block>=P->Blocks||Bytes/4096>P->Blocks-Block)return EFI_INVALID_PARAMETER;
  if(!mBlockLive||mBlockBusy||mResetReport.Prepared||mResetReport.Failed||mExitRetained||mPartitionFailed)return EFI_NOT_READY;
  UINTN Index=0;while(Index<mLunCount&&mLuns[Index].Id!=P->Lun)++Index;if(Index==mLunCount)return EFI_NO_MEDIA;
  if(Writing&&(mPermanentWpStatus!=EFI_SUCCESS||mPowerOnWpStatus!=EFI_SUCCESS||mPermanentWp||mPowerOnWp||
     mCapabilities[Index].CacheStatus!=EFI_SUCCESS||mCapabilities[Index].UnitStatus!=EFI_SUCCESS||
     mCapabilities[Index].WriteProtected||mCapabilities[Index].UnitWriteProtect||!mCapabilities[Index].Fua))return EFI_WRITE_PROTECTED;
  EFI_TPL Old=gBS->RaiseTPL(TPL_CALLBACK);mBlockBusy=mPartitionExecuting=mProductExecuting=TRUE;
  mPartitionLun=P->Lun;mPartitionFirst=P->First;mPartitionBlocks=P->Blocks;
  EFI_STATUS S=PartitionGate();if(S==EFI_SUCCESS)S=ProductSmmuGuard(TRUE);
  for(UINTN At=0;S==EFI_SUCCESS&&At<Bytes;At+=4096){
    UINT8 Tag=mServiceTag++;UINT32 Lba=(UINT32)(P->First+Block+At/4096);
    S=PianoUfsBuildReadCommand(mTrl.Cpu,mTrl.Bytes,mUcd.Cpu,mUcd.Bytes,mUcd.DeviceAddress,mData.DeviceAddress,4096,Tag,P->Lun,PianoUfsReadLba10,Lba,1);
    if(S!=EFI_SUCCESS)break;
    if(Writing){((UINT8 *)mTrl.Cpu)[3]=0x13;((UINT8 *)mUcd.Cpu)[1]=0x20;((UINT8 *)mUcd.Cpu)[16]=0x2A;((UINT8 *)mUcd.Cpu)[17]=8;CopyMem(mData.Cpu,(UINT8 *)Buffer+At,4096);}
    S=Submit(Writing?"PARTITION_WRITE_FUA":"PARTITION_READ",Tag,FALSE);
    if(S==EFI_SUCCESS&&mTransferred!=4096)S=EFI_BAD_BUFFER_SIZE;
    if(S==EFI_SUCCESS&&!Writing)CopyMem((UINT8 *)Buffer+At,mData.Cpu,4096);
  }
  if(S!=EFI_SUCCESS&&Writing)mPartitionFailed=TRUE;
  if(S==EFI_SUCCESS)S=ProductSmmuGuard(FALSE);
  mPartitionExecuting=mBlockBusy=mProductExecuting=FALSE;gBS->RestoreTPL(Old);return S;
}
EFI_STATUS PianoStorageRead(CONST PIANO_STORAGE_PARTITION *P,UINT64 Block,UINTN Bytes,VOID *Buffer){return PartitionTransfer(P,Block,Bytes,Buffer,FALSE);}
EFI_STATUS PianoStorageWrite(CONST PIANO_STORAGE_PARTITION *P,UINT64 Block,UINTN Bytes,CONST VOID *Buffer){return PartitionTransfer(P,Block,Bytes,(VOID *)Buffer,TRUE);}
EFI_STATUS PianoStorageFlush(CONST PIANO_STORAGE_PARTITION *P){
  if(!FilePartition(P))return EFI_INVALID_PARAMETER;
  if(!mBlockLive||mBlockBusy||mResetReport.Prepared||mResetReport.Failed||mPartitionFailed)return EFI_NOT_READY;
  EFI_TPL Old=gBS->RaiseTPL(TPL_CALLBACK);mBlockBusy=mPartitionExecuting=mProductExecuting=TRUE;
  mPartitionLun=P->Lun;mPartitionFirst=P->First;mPartitionBlocks=P->Blocks;UINT8 Tag=mServiceTag++;
  ZeroMem(mTrl.Cpu,mTrl.Bytes);ZeroMem(mUcd.Cpu,mUcd.Bytes);UINT8 *T=mTrl.Cpu,*U=mUcd.Cpu;
  T[3]=0x11;T[8]=15;for(UINTN I=0;I<8;++I)T[16+I]=(UINT8)(mUcd.DeviceAddress>>(8*I));T[24]=16;T[26]=16;
  U[0]=1;U[2]=P->Lun;U[3]=Tag;U[16]=0x35;EFI_STATUS S=ProductSmmuGuard(TRUE);
  if(S==EFI_SUCCESS)S=Submit("PARTITION_SYNC",Tag,FALSE);
  if(S==EFI_SUCCESS)S=ProductSmmuGuard(FALSE);
  if(S!=EFI_SUCCESS)mPartitionFailed=TRUE;
  mPartitionExecuting=mBlockBusy=mProductExecuting=FALSE;gBS->RestoreTPL(Old);return S;
}
STATIC EFI_STATUS EFIAPI FileWrite(PIANO_PARTITION_IO_PROTOCOL *This,EFI_HANDLE Handle,UINT64 At,UINTN Bytes,CONST VOID *Data){
  if(This!=&mPartitionIo)return EFI_INVALID_PARAMETER;PIANO_STORAGE_PARTITION P;EFI_STATUS S=PartitionFromHandle(Handle,&P);
  return S==EFI_SUCCESS?PianoStorageWrite(&P,At,Bytes,Data):S;
}
STATIC EFI_STATUS EFIAPI FileFlush(PIANO_PARTITION_IO_PROTOCOL *This,EFI_HANDLE Handle){
  if(This!=&mPartitionIo)return EFI_INVALID_PARAMETER;PIANO_STORAGE_PARTITION P;EFI_STATUS S=PartitionFromHandle(Handle,&P);
  return S==EFI_SUCCESS?PianoStorageFlush(&P):S;
}
