/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct ARegionType;
struct bContext;

/* Only called once on startup. storage is global in BKE kernel listbase. */
void ED_spacetypes_init();
void ED_spacemacros_init();

/* The plugin-able API for export to editors. */

/* -------------------------------------------------------------------- */
/** \name Calls for registering default spaces
 *
 * Calls for registering default spaces, only called once, from #ED_spacetypes_init
 * \{ */

void ED_spacetype_outliner();
void ED_spacetype_view3d();
void ED_spacetype_ipo();
void ED_spacetype_image();
void ED_spacetype_node();
void ED_spacetype_buttons();
void ED_spacetype_info();
void ED_spacetype_file();
void ED_spacetype_action();
void ED_spacetype_nla();
void ED_spacetype_script();
void ED_spacetype_text();
void ED_spacetype_sequencer();
void ED_spacetype_logic();
void ED_spacetype_console();
void ED_spacetype_userpref();
void ED_spacetype_clip();
void ED_spacetype_statusbar();
void ED_spacetype_topbar();
void ED_spacetype_spreadsheet();

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space-type Static Data
 * Calls for instancing and freeing space-type static data called in #WM_init_exit
 * \{ */

void ED_file_init();
void ED_file_exit();

/** \} */

#define REGION_DRAW_POST_VIEW 0
#define REGION_DRAW_POST_PIXEL 1
#define REGION_DRAW_PRE_VIEW 2
#define REGION_DRAW_BACKDROP 3

void *ED_region_draw_cb_activate(ARegionType *art,
                                 void (*draw)(const bContext *, ARegion *, void *),
                                 void *customdata,
                                 int type);
void ED_region_draw_cb_draw(const bContext *C, ARegion *region, int type);
void ED_region_surface_draw_cb_draw(ARegionType *art, int type);
bool ED_region_draw_cb_exit(ARegionType *art, void *handle);
void ED_region_draw_cb_remove_by_type(ARegionType *art, void *draw_fn, void (*free)(void *));
