/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/screen/screen_intern.h
 *  \ingroup edscr
 */

#ifndef __SCREEN_INTERN_H__
#define __SCREEN_INTERN_H__

struct bContext;
struct bContextDataResult;
struct Main;

/* internal exports only */

#define AZONESPOT       (0.6f * U.widget_unit)
#define AZONEFADEIN     (5.0f * U.widget_unit) /* when azone is totally visible */
#define AZONEFADEOUT    (6.5f * U.widget_unit) /* when we start seeing the azone */

/* area.c */
void        ED_area_data_copy(ScrArea *sa_dst, ScrArea *sa_src, const bool do_free);
void        ED_area_data_swap(ScrArea *sa1, ScrArea *sa2);
void        screen_area_update_region_sizes(wmWindowManager *wm, wmWindow *win, ScrArea *area);
void        region_toggle_hidden(struct bContext *C, ARegion *ar, const bool do_fade);

/* screen_edit.c */
bScreen    *screen_add(const char *name, const rcti *rect);
void        screen_data_copy(bScreen *to, bScreen *from);
void        screen_new_activate_prepare(const wmWindow *win, bScreen *screen_new);
void        screen_change_update(struct bContext *C, wmWindow *win, bScreen *sc);
bScreen    *screen_change_prepare(bScreen *screen_old, bScreen *screen_new, struct Main *bmain, struct bContext *C, wmWindow *win);
ScrArea    *area_split(bScreen *sc, ScrArea *sa, char dir, float fac, int merge);
int         screen_area_join(struct bContext *C, bScreen *scr, ScrArea *sa1, ScrArea *sa2);
int         area_getorientation(ScrArea *sa, ScrArea *sb);
void        select_connected_scredge(const wmWindow *win, ScrEdge *edge);

bool        scredge_is_horizontal(ScrEdge *se);
ScrEdge     *screen_area_map_find_active_scredge(
        const struct ScrAreaMap *area_map,
        const rcti *bounds_rect,
        const int mx, const int my);
ScrEdge    *screen_find_active_scredge(
        const wmWindow *win, const bScreen *screen,
        const int mx, const int my);

struct AZone *is_in_area_actionzone(ScrArea *sa, const int xy[2]);

/* screen_context.c */
int ed_screen_context(
        const struct bContext *C, const char *member, struct bContextDataResult *result);

extern const char *screen_context_dir[]; /* doc access */

/* screendump.c */
void	SCREEN_OT_screenshot(struct wmOperatorType *ot);
void	SCREEN_OT_screencast(struct wmOperatorType *ot);

/* screen_ops.c */
void	region_blend_start(struct bContext *C, struct ScrArea *sa, struct ARegion *ar);

/* workspace_layout_edit.c */
bool workspace_layout_set_poll(const struct WorkSpaceLayout *layout);


#endif /* __SCREEN_INTERN_H__ */
