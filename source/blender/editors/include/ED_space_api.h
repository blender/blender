/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct ARegionType;
struct bContext;

/* Only called once on startup. storage is global in BKE kernel listbase. */
void ED_spacetypes_init(void);
void ED_spacemacros_init(void);

/* The plugin-able API for export to editors. */

/* -------------------------------------------------------------------- */
/** \name Calls for registering default spaces
 *
 * Calls for registering default spaces, only called once, from #ED_spacetypes_init
 * \{ */

void ED_spacetype_outliner(void);
void ED_spacetype_view3d(void);
void ED_spacetype_ipo(void);
void ED_spacetype_image(void);
void ED_spacetype_node(void);
void ED_spacetype_buttons(void);
void ED_spacetype_info(void);
void ED_spacetype_file(void);
void ED_spacetype_action(void);
void ED_spacetype_nla(void);
void ED_spacetype_script(void);
void ED_spacetype_text(void);
void ED_spacetype_sequencer(void);
void ED_spacetype_logic(void);
void ED_spacetype_console(void);
void ED_spacetype_userpref(void);
void ED_spacetype_clip(void);
void ED_spacetype_statusbar(void);
void ED_spacetype_topbar(void);
void ED_spacetype_spreadsheet(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space-type Static Data
 * Calls for instancing and freeing space-type static data called in #WM_init_exit
 * \{ */

void ED_file_init(void);
void ED_file_exit(void);

/** \} */

#define REGION_DRAW_POST_VIEW 0
#define REGION_DRAW_POST_PIXEL 1
#define REGION_DRAW_PRE_VIEW 2
#define REGION_DRAW_BACKDROP 3

void *ED_region_draw_cb_activate(struct ARegionType *art,
                                 void (*draw)(const struct bContext *, struct ARegion *, void *),
                                 void *customdata,
                                 int type);
void ED_region_draw_cb_draw(const struct bContext *C, struct ARegion *region, int type);
void ED_region_surface_draw_cb_draw(struct ARegionType *art, int type);
bool ED_region_draw_cb_exit(struct ARegionType *art, void *handle);
void ED_region_draw_cb_remove_by_type(struct ARegionType *art,
                                      void *draw_fn,
                                      void (*free)(void *));

#ifdef __cplusplus
}
#endif
