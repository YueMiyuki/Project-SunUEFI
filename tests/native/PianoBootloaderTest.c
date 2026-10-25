// SPDX-License-Identifier: BSD-2-Clause-Patent
#include "../../android/native/piano-bootloader.h"
#include <assert.h>
#include <linux/input.h>
#include <stdlib.h>
#include <stdio.h>

int main(void) {
  struct piano_bootloader_keys keys = {0};
  assert(piano_bootloader_key(&keys, KEY_VOLUMEDOWN, 0) == 0 && !keys.count);
  assert(piano_bootloader_key(&keys, KEY_VOLUMEDOWN, 2) == 0 && !keys.down);
  for (unsigned i = 0; i < 3; ++i) {
    assert(piano_bootloader_key(&keys, KEY_VOLUMEDOWN, 1) == 0);
    assert(piano_bootloader_key(&keys, KEY_VOLUMEDOWN, 1) == 0);
    assert(piano_bootloader_key(&keys, KEY_VOLUMEDOWN, 2) == 0 && keys.count == i);
    assert(piano_bootloader_key(&keys, KEY_VOLUMEDOWN, 0) == (i == 2));
  }
  keys = (struct piano_bootloader_keys){.down = -1};
  assert(!piano_bootloader_key(&keys, KEY_VOLUMEDOWN, 2));
  assert(!piano_bootloader_key(&keys, KEY_VOLUMEDOWN, 1));
  assert(!piano_bootloader_key(&keys, KEY_VOLUMEDOWN, 0) && !keys.count);
  assert(piano_bootloader_key(&keys, KEY_VOLUMEUP, 1) == -1);
  assert(piano_bootloader_key(&keys, KEY_VOLUMEUP, 2) == -1);
  /* The installer may still be holding Up from choosing Install. Only a new
   * press after release cancels this separate risk confirmation. */
  keys = (struct piano_bootloader_keys){.up = -1};
  assert(!piano_bootloader_key(&keys, KEY_VOLUMEUP, 1));
  assert(!piano_bootloader_key(&keys, KEY_VOLUMEUP, 2));
  assert(!piano_bootloader_key(&keys, KEY_VOLUMEUP, 0));
  assert(piano_bootloader_key(&keys, KEY_VOLUMEUP, 1) == -1);
  unsetenv("PIANO_CONFIRMED_UNVERIFIED_BOOTLOADER");
  assert(piano_bootloader_write_allowed("0"));
  assert(!piano_bootloader_write_allowed("1"));
  assert(!piano_bootloader_write_allowed(""));
  setenv("PIANO_CONFIRMED_UNVERIFIED_BOOTLOADER", "0", 1);
  assert(!piano_bootloader_write_allowed("1"));
  setenv("PIANO_CONFIRMED_UNVERIFIED_BOOTLOADER", "1", 1);
  assert(piano_bootloader_write_allowed("1"));
  unsetenv("PIANO_CONFIRMED_UNVERIFIED_BOOTLOADER");
  assert(!piano_bootloader_write_allowed("1"));
  puts("Actual bootloader confirmation: press/release, held/repeat/cancel and operation-only override passed.");
  return 0;
}
