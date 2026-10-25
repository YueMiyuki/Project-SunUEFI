// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

void piano_bootmenu_sort(char **names);
// Takes ownership of the optional activity string returned by confd.
void piano_bootmenu_enter(void *activity);
