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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_gpencil.h
 *  \ingroup editors
 */

#ifndef __ED_GPENCIL_H__
#define __ED_GPENCIL_H__

struct ListBase;
struct bContext;
struct bScreen;
struct ScrArea;
struct ARegion;
struct View3D;
struct SpaceNode;
struct SpaceSeq;
struct bGPdata;
struct bGPDlayer;
struct bGPDframe;
struct PointerRNA;
struct Panel;
struct ImBuf;
struct wmKeyConfig;


/* ------------- Grease-Pencil Helpers ---------------- */

/* Temporary 'Stroke Point' data 
 *
 * Used as part of the 'stroke cache' used during drawing of new strokes
 */
typedef struct tGPspoint {
	int x, y;               /* x and y coordinates of cursor (in relative to area) */
	float pressure;         /* pressure of tablet at this point */
} tGPspoint;


/* Check if 'sketching sessions' are enabled */
#define GPENCIL_SKETCH_SESSIONS_ON(scene) ((scene)->toolsettings->gpencil_flags & GP_TOOL_FLAG_PAINTSESSIONS_ON)

/* ----------- Grease Pencil Tools/Context ------------- */

struct bGPdata **gpencil_data_get_pointers(struct bContext *C, struct PointerRNA *ptr);
struct bGPdata *gpencil_data_get_active(struct bContext *C);
struct bGPdata *gpencil_data_get_active_v3d(struct Scene *scene); /* for offscreen rendering */

/* ----------- Grease Pencil Operators ----------------- */

void ED_keymap_gpencil(struct wmKeyConfig *keyconf);
void ED_operatortypes_gpencil(void);

/* ------------ Grease-Pencil Drawing API ------------------ */
/* drawgpencil.c */

void draw_gpencil_2dimage(struct bContext *C, struct ImBuf *ibuf);
void draw_gpencil_view2d(struct bContext *C, short onlyv2d);
void draw_gpencil_view3d(struct Scene *scene, struct View3D *v3d, struct ARegion *ar, short only3d);

void gpencil_panel_standard(const struct bContext *C, struct Panel *pa);

/* ----------- Grease-Pencil AnimEdit API ------------------ */
short gplayer_frames_looper(struct bGPDlayer *gpl, struct Scene *scene, short (*gpf_cb)(struct bGPDframe *, struct Scene *));
void gplayer_make_cfra_list(struct bGPDlayer *gpl, ListBase *elems, short onlysel);

short is_gplayer_frame_selected(struct bGPDlayer *gpl);
void set_gplayer_frame_selection(struct bGPDlayer *gpl, short mode);
void select_gpencil_frames(struct bGPDlayer *gpl, short select_mode);
void select_gpencil_frame(struct bGPDlayer *gpl, int selx, short select_mode);
void borderselect_gplayer_frames(struct bGPDlayer *gpl, float min, float max, short select_mode);

void delete_gplayer_frames(struct bGPDlayer *gpl);
void duplicate_gplayer_frames(struct bGPDlayer *gpd);

void free_gpcopybuf(void);
void copy_gpdata(void);
void paste_gpdata(void);

void snap_gplayer_frames(struct bGPDlayer *gpl, short mode);
void mirror_gplayer_frames(struct bGPDlayer *gpl, short mode);

/* ------------ Grease-Pencil Undo System ------------------ */
int ED_gpencil_session_active(void);
int ED_undo_gpencil_step(struct bContext *C, int step, const char *name);

#endif /*  __ED_GPENCIL_H__ */
