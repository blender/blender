/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Share between interface_region_*.cc files.
 */

#pragma once

/* interface_region_menu_popup.cc */

uint ui_popup_menu_hash(const char *str);

/* interface_regions.cc */

ARegion *ui_region_temp_add(bScreen *screen);
void ui_region_temp_remove(bContext *C, bScreen *screen, ARegion *region);
