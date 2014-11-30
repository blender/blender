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

struct ID;
struct ListBase;
struct bContext;
struct bScreen;
struct ScrArea;
struct ARegion;
struct View3D;
struct SpaceNode;
struct SpaceSeq;
struct Object;
struct bGPdata;
struct bGPDlayer;
struct bGPDframe;
struct PointerRNA;
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
	float time;             /* Time relative to stroke start (used when converting to path) */
} tGPspoint;


/* Check if 'sketching sessions' are enabled */
#define GPENCIL_SKETCH_SESSIONS_ON(scene) ((scene)->toolsettings->gpencil_flags & GP_TOOL_FLAG_PAINTSESSIONS_ON)

/* ----------- Grease Pencil Tools/Context ------------- */

/* Context-dependent */
struct bGPdata **ED_gpencil_data_get_pointers(const struct bContext *C, struct PointerRNA *ptr);
struct bGPdata  *ED_gpencil_data_get_active(const struct bContext *C);

/* Context independent (i.e. each required part is passed in instead) */
struct bGPdata **ED_gpencil_data_get_pointers_direct(struct ID *screen_id, struct Scene *scene,
                                                     struct ScrArea *sa, struct Object *ob,
                                                     struct PointerRNA *ptr);
struct bGPdata *ED_gpencil_data_get_active_direct(struct ID *screen_id, struct Scene *scene,
                                                  struct ScrArea *sa, struct Object *ob);

/* 3D View */
struct bGPdata  *ED_gpencil_data_get_active_v3d(struct Scene *scene, struct View3D *v3d);

/* ----------- Grease Pencil Operators ----------------- */

void ED_keymap_gpencil(struct wmKeyConfig *keyconf);

void ED_operatortypes_gpencil(void);
void ED_operatormacros_gpencil(void);

/* ------------ Grease-Pencil Drawing API ------------------ */
/* drawgpencil.c */

void ED_gpencil_draw_2dimage(const struct bContext *C);
void ED_gpencil_draw_view2d(const struct bContext *C, bool onlyv2d);
void ED_gpencil_draw_view3d(struct Scene *scene, struct View3D *v3d, struct ARegion *ar, bool only3d);
void ED_gpencil_draw_ex(struct bGPdata *gpd, int winx, int winy, const int cfra);

/* ----------- Grease-Pencil AnimEdit API ------------------ */
bool  ED_gplayer_frames_looper(struct bGPDlayer *gpl, struct Scene *scene,
                               short (*gpf_cb)(struct bGPDframe *, struct Scene *));
void ED_gplayer_make_cfra_list(struct bGPDlayer *gpl, ListBase *elems, bool onlysel);

bool  ED_gplayer_frame_select_check(struct bGPDlayer *gpl);
void  ED_gplayer_frame_select_set(struct bGPDlayer *gpl, short mode);
void  ED_gplayer_frames_select_border(struct bGPDlayer *gpl, float min, float max, short select_mode);
void  ED_gpencil_select_frames(struct bGPDlayer *gpl, short select_mode);
void  ED_gpencil_select_frame(struct bGPDlayer *gpl, int selx, short select_mode);

bool  ED_gplayer_frames_delete(struct bGPDlayer *gpl);
void  ED_gplayer_frames_duplicate(struct bGPDlayer *gpl);

void ED_gplayer_frames_keytype_set(struct bGPDlayer *gpl, short type);

void  ED_gplayer_snap_frames(struct bGPDlayer *gpl, struct Scene *scene, short mode);

#if 0
void free_gpcopybuf(void);
void copy_gpdata(void);
void paste_gpdata(void);

void mirror_gplayer_frames(struct bGPDlayer *gpl, short mode);
#endif

/* ------------ Grease-Pencil Undo System ------------------ */
int ED_gpencil_session_active(void);
int ED_undo_gpencil_step(struct bContext *C, int step, const char *name);

#endif /*  __ED_GPENCIL_H__ */
