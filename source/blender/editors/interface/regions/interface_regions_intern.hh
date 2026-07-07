/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Share between interface_region_*.cc files.
 */

#pragma once

#include "BLI_string_ref.hh"

namespace blender {

struct ARegion;
struct bContext;
struct bScreen;

namespace ui {

/* interface_region_menu_popup.cc */

uint ui_popup_menu_hash(StringRef str);

/* interface_regions.cc */

ARegion *region_temp_add(bScreen *screen);
void region_temp_remove(bContext *C, bScreen *screen, ARegion *region);

}  // namespace ui
}  // namespace blender
