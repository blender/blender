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

#include "ED_numinput.h"

struct ID;
struct ListBase;
struct bContext;
struct ScrArea;
struct ARegion;
struct View3D;
struct Object;
struct bGPdata;
struct bGPDlayer;
struct bGPDframe;
struct bGPDstroke;
struct bGPDpalette;
struct bGPDpalettecolor;
struct bAnimContext;
struct KeyframeEditData;
struct PointerRNA;
struct wmWindowManager;
struct wmKeyConfig;


/* ------------- Grease-Pencil Helpers ---------------- */
typedef struct tGPDinterpolate_layer {
	struct tGPDinterpolate_layer *next, *prev;

	struct bGPDlayer *gpl;            /* layer */
	struct bGPDframe *prevFrame;      /* frame before current frame (interpolate-from) */
	struct bGPDframe *nextFrame;      /* frame after current frame (interpolate-to) */
	struct bGPDframe *interFrame;     /* interpolated frame */
	float factor;                     /* interpolate factor */

} tGPDinterpolate_layer;

/* Temporary interpolate operation data */
typedef struct tGPDinterpolate {
	struct Scene *scene;       /* current scene from context */
	struct ScrArea *sa;        /* area where painting originated */
	struct ARegion *ar;        /* region where painting originated */
	struct bGPdata *gpd;       /* current GP datablock */

	int cframe;                /* current frame number */
	ListBase ilayers;   /* (tGPDinterpolate_layer) layers to be interpolated */
	float shift;        /* value for determining the displacement influence */
	float init_factor;  /* initial interpolation factor for active layer */
	float low_limit;    /* shift low limit (-100%) */
	float high_limit;   /* shift upper limit (200%) */
	int flag;           /* flag from toolsettings */

	NumInput num;       /* numeric input */
	void *draw_handle_3d; /* handle for drawing strokes while operator is running 3d stuff */
	void *draw_handle_screen; /* handle for drawing strokes while operator is running screen stuff */
} tGPDinterpolate;

/* Temporary 'Stroke Point' data 
 *
 * Used as part of the 'stroke cache' used during drawing of new strokes
 */
typedef struct tGPspoint {
	int x, y;               /* x and y coordinates of cursor (in relative to area) */
	float pressure;         /* pressure of tablet at this point */
	float strength;         /* pressure of tablet at this point for alpha factor */
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

bool ED_gpencil_has_keyframe_v3d(struct Scene *scene, struct Object *ob, int cfra);

/* ----------- Stroke Editing Utilities ---------------- */

bool ED_gpencil_stroke_can_use_direct(const struct ScrArea *sa, const struct bGPDstroke *gps);
bool ED_gpencil_stroke_can_use(const struct bContext *C, const struct bGPDstroke *gps);
bool ED_gpencil_stroke_color_use(const struct bGPDlayer *gpl, const struct bGPDstroke *gps);

struct bGPDpalettecolor *ED_gpencil_stroke_getcolor(struct bGPdata *gpd, struct bGPDstroke *gps);

bool ED_gpencil_stroke_minmax(
        const struct bGPDstroke *gps, const bool use_select,
        float r_min[3], float r_max[3]);

/* ----------- Grease Pencil Operators ----------------- */

void ED_keymap_gpencil(struct wmKeyConfig *keyconf);

void ED_operatortypes_gpencil(void);
void ED_operatormacros_gpencil(void);

/* ------------- Copy-Paste Buffers -------------------- */

/* Strokes copybuf */
void ED_gpencil_strokes_copybuf_free(void);


/* ------------ Grease-Pencil Drawing API ------------------ */
/* drawgpencil.c */

void ED_gpencil_draw_2dimage(const struct bContext *C);
void ED_gpencil_draw_view2d(const struct bContext *C, bool onlyv2d);
void ED_gpencil_draw_view3d(struct wmWindowManager *wm, struct Scene *scene, struct View3D *v3d, struct ARegion *ar, bool only3d);
void ED_gpencil_draw_ex(struct Scene *scene, struct bGPdata *gpd, int winx, int winy,
                        const int cfra, const char spacetype);
void ED_gp_draw_interpolation(struct tGPDinterpolate *tgpi, const int type);

/* ----------- Grease-Pencil AnimEdit API ------------------ */
bool  ED_gplayer_frames_looper(struct bGPDlayer *gpl, struct Scene *scene,
                               short (*gpf_cb)(struct bGPDframe *, struct Scene *));
void ED_gplayer_make_cfra_list(struct bGPDlayer *gpl, ListBase *elems, bool onlysel);

bool  ED_gplayer_frame_select_check(struct bGPDlayer *gpl);
void  ED_gplayer_frame_select_set(struct bGPDlayer *gpl, short mode);
void  ED_gplayer_frames_select_border(struct bGPDlayer *gpl, float min, float max, short select_mode);
void  ED_gplayer_frames_select_region(struct KeyframeEditData *ked, struct bGPDlayer *gpl, short tool, short select_mode);
void  ED_gpencil_select_frames(struct bGPDlayer *gpl, short select_mode);
void  ED_gpencil_select_frame(struct bGPDlayer *gpl, int selx, short select_mode);

bool  ED_gplayer_frames_delete(struct bGPDlayer *gpl);
void  ED_gplayer_frames_duplicate(struct bGPDlayer *gpl);

void ED_gplayer_frames_keytype_set(struct bGPDlayer *gpl, short type);

void  ED_gplayer_snap_frames(struct bGPDlayer *gpl, struct Scene *scene, short mode);
void  ED_gplayer_mirror_frames(struct bGPDlayer *gpl, struct Scene *scene, short mode);

void ED_gpencil_anim_copybuf_free(void);
bool ED_gpencil_anim_copybuf_copy(struct bAnimContext *ac);
bool ED_gpencil_anim_copybuf_paste(struct bAnimContext *ac, const short copy_mode);


/* ------------ Grease-Pencil Undo System ------------------ */
int ED_gpencil_session_active(void);
int ED_undo_gpencil_step(struct bContext *C, int step, const char *name);

/* ------------ Transformation Utilities ------------ */

/* get difference matrix using parent */
void ED_gpencil_parent_location(struct bGPDlayer *gpl, float diff_mat[4][4]);
/* reset parent matrix for all layers */
void ED_gpencil_reset_layers_parent(struct bGPdata *gpd);


#endif /*  __ED_GPENCIL_H__ */
