// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once
#include <Uefi.h>
#define PIANO_NV_FILE_PROTOCOL_GUID {0x8e65d9c0,0x9943,0x4130,{0x9c,0x24,0x4e,0x56,0x46,0x49,0x4c,0x45}}
typedef struct {
  UINT64 Revision;
  volatile UINT64 Changes;
  volatile BOOLEAN Frozen;
} PIANO_NV_FILE_PROTOCOL;
