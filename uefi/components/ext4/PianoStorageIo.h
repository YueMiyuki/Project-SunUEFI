// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once
#include <Uefi.h>
// Private partition-scoped file driver IO. Factory parent BlockIO stays RO.
typedef struct {
  UINT8 Lun;
  UINT32 BlockBytes,MediaId;
  UINT64 First,Blocks;
  EFI_GUID PartitionGuid;
  VOID *Identity;
} PIANO_STORAGE_PARTITION;
EFI_STATUS PianoStorageOpen(CONST CHAR16 *Name,PIANO_STORAGE_PARTITION *Partition);
EFI_STATUS PianoStorageRead(CONST PIANO_STORAGE_PARTITION *,UINT64 Block,UINTN Bytes,VOID *Buffer);
EFI_STATUS PianoStorageWrite(CONST PIANO_STORAGE_PARTITION *,UINT64 Block,UINTN Bytes,CONST VOID *Buffer);
EFI_STATUS PianoStorageFlush(CONST PIANO_STORAGE_PARTITION *);
// At application TPL, close filesystem drivers before parent disk teardown.
EFI_STATUS PianoStorageCloseFilesystems(VOID);
