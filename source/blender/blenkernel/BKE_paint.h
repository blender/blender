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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */ 

#ifndef __BKE_PAINT_H__
#define __BKE_PAINT_H__

/** \file BKE_paint.h
 *  \ingroup bke
 */

struct bContext;
struct BMesh;
struct BMFace;
struct Brush;
struct CurveMapping;
struct MDisps;
struct MeshElemMap;
struct GridPaintMask;
struct MFace;
struct MultireModifierData;
struct MVert;
struct Object;
struct Paint;
struct PBVH;
struct Scene;
struct StrokeCache;
struct Tex;
struct ImagePool;
struct UnifiedPaintSettings;

enum OverlayFlags;

extern const char PAINT_CURSOR_SCULPT[3];
extern const char PAINT_CURSOR_VERTEX_PAINT[3];
extern const char PAINT_CURSOR_WEIGHT_PAINT[3];
extern const char PAINT_CURSOR_TEXTURE_PAINT[3];

typedef enum PaintMode {
	PAINT_SCULPT = 0,
	PAINT_VERTEX = 1,
	PAINT_WEIGHT = 2,
	PAINT_TEXTURE_PROJECTIVE = 3,
	PAINT_TEXTURE_2D = 4,
	PAINT_SCULPT_UV = 5,
	PAINT_INVALID = 6
} PaintMode;

/* overlay invalidation */
typedef enum OverlayControlFlags {
	PAINT_INVALID_OVERLAY_TEXTURE_PRIMARY = 1,
	PAINT_INVALID_OVERLAY_TEXTURE_SECONDARY = (1 << 2),
	PAINT_INVALID_OVERLAY_CURVE = (1 << 3),
	PAINT_OVERLAY_OVERRIDE_CURSOR = (1 << 4),
	PAINT_OVERLAY_OVERRIDE_PRIMARY = (1 << 5),
	PAINT_OVERLAY_OVERRIDE_SECONDARY = (1 << 6)
} OverlayControlFlags;

#define PAINT_OVERRIDE_MASK (PAINT_OVERLAY_OVERRIDE_SECONDARY | \
						     PAINT_OVERLAY_OVERRIDE_PRIMARY | \
						     PAINT_OVERLAY_OVERRIDE_CURSOR)

void BKE_paint_invalidate_overlay_tex(struct Scene *scene, const struct Tex *tex);
void BKE_paint_invalidate_cursor_overlay(struct Scene *scene, struct CurveMapping *curve);
void BKE_paint_invalidate_overlay_all(void);
OverlayControlFlags BKE_paint_get_overlay_flags(void);
void BKE_paint_reset_overlay_invalid(OverlayControlFlags flag);
void BKE_paint_set_overlay_override(enum OverlayFlags flag);

void BKE_paint_init(struct Paint *p, const char col[3]);
void BKE_paint_free(struct Paint *p);
void BKE_paint_copy(struct Paint *src, struct Paint *tar);

struct Paint *BKE_paint_get_active(struct Scene *sce);
struct Paint *BKE_paint_get_active_from_context(const struct bContext *C);
PaintMode BKE_paintmode_get_active_from_context(const struct bContext *C);
struct Brush *BKE_paint_brush(struct Paint *paint);
void BKE_paint_brush_set(struct Paint *paint, struct Brush *br);

/* testing face select mode
 * Texture paint could be removed since selected faces are not used
 * however hiding faces is useful */
bool BKE_paint_select_face_test(struct Object *ob);
bool BKE_paint_select_vert_test(struct Object *ob);
bool BKE_paint_select_elem_test(struct Object *ob);

/* partial visibility */
bool paint_is_face_hidden(const struct MFace *f, const struct MVert *mvert);
bool paint_is_grid_face_hidden(const unsigned int *grid_hidden,
                               int gridsize, int x, int y);
bool paint_is_bmesh_face_hidden(struct BMFace *f);

/* paint masks */
float paint_grid_paint_mask(const struct GridPaintMask *gpm, unsigned level,
                            unsigned x, unsigned y);
void paint_calculate_rake_rotation(struct UnifiedPaintSettings *ups, const float mouse_pos[2]);
/* Session data (mode-specific) */

typedef struct SculptSession {
	/* Mesh data (not copied) can come either directly from a Mesh, or from a MultiresDM */
	struct MultiresModifierData *multires; /* Special handling for multires meshes */
	struct MVert *mvert;
	struct MPoly *mpoly;
	struct MLoop *mloop;
	int totvert, totpoly;
	float *face_normals;
	struct KeyBlock *kb;
	float *vmask;
	
	/* Mesh connectivity */
	const struct MeshElemMap *pmap;

	/* BMesh for dynamic topology sculpting */
	struct BMesh *bm;
	int bm_smooth_shading;
	/* Undo/redo log for dynamic topology sculpting */
	struct BMLog *bm_log;

	/* PBVH acceleration structure */
	struct PBVH *pbvh;
	bool show_diffuse_color;

	/* Paiting on deformed mesh */
	int modifiers_active; /* object is deformed with some modifiers */
	float (*orig_cos)[3]; /* coords of undeformed mesh */
	float (*deform_cos)[3]; /* coords of deformed mesh but without stroke displacement */
	float (*deform_imats)[3][3]; /* crazyspace deformation matrices */

	/* Partial redraw */
	int partial_redraw;
	
	/* Used to cache the render of the active texture */
	unsigned int texcache_side, *texcache, texcache_actual;
	struct ImagePool *tex_pool;

	/* Layer brush persistence between strokes */
	float (*layer_co)[3]; /* Copy of the mesh vertices' locations */

	struct SculptStroke *stroke;
	struct StrokeCache *cache;

	/* last paint/sculpt stroke location */
	int last_stroke_valid;
	float last_stroke[3];

	float average_stroke_accum[3];
	int average_stroke_counter;
} SculptSession;

void free_sculptsession(struct Object *ob);
void free_sculptsession_deformMats(struct SculptSession *ss);
void sculptsession_bm_to_me(struct Object *ob, int reorder);
void sculptsession_bm_to_me_for_render(struct Object *object);
#endif
