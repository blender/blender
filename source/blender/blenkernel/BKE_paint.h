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
struct MeshElemMap;
struct GridPaintMask;
struct Main;
struct Mesh;
struct MLoop;
struct MLoopTri;
struct MFace;
struct MVert;
struct Object;
struct Paint;
struct PaintCurve;
struct Palette;
struct PaletteColor;
struct PBVH;
struct ReportList;
struct Scene;
struct ViewLayer;
struct Sculpt;
struct StrokeCache;
struct Tex;
struct ImagePool;
struct UnifiedPaintSettings;
struct Depsgraph;

enum eOverlayFlags;

#include "DNA_object_enums.h"

extern const char PAINT_CURSOR_SCULPT[3];
extern const char PAINT_CURSOR_VERTEX_PAINT[3];
extern const char PAINT_CURSOR_WEIGHT_PAINT[3];
extern const char PAINT_CURSOR_TEXTURE_PAINT[3];

typedef enum ePaintMode {
	ePaintSculpt = 0,
	ePaintVertex = 1,
	ePaintWeight = 2,
	ePaintTextureProjective = 3,
	ePaintTexture2D = 4,
	ePaintSculptUV = 5,
	ePaintInvalid = 6,
	ePaintGpencil = 7
} ePaintMode;

/* overlay invalidation */
typedef enum eOverlayControlFlags {
	PAINT_INVALID_OVERLAY_TEXTURE_PRIMARY = 1,
	PAINT_INVALID_OVERLAY_TEXTURE_SECONDARY = (1 << 2),
	PAINT_INVALID_OVERLAY_CURVE = (1 << 3),
	PAINT_OVERLAY_OVERRIDE_CURSOR = (1 << 4),
	PAINT_OVERLAY_OVERRIDE_PRIMARY = (1 << 5),
	PAINT_OVERLAY_OVERRIDE_SECONDARY = (1 << 6)
} eOverlayControlFlags;

#define PAINT_OVERRIDE_MASK (PAINT_OVERLAY_OVERRIDE_SECONDARY | \
						     PAINT_OVERLAY_OVERRIDE_PRIMARY | \
						     PAINT_OVERLAY_OVERRIDE_CURSOR)

void BKE_paint_invalidate_overlay_tex(struct Scene *scene, struct ViewLayer *view_layer, const struct Tex *tex);
void BKE_paint_invalidate_cursor_overlay(struct Scene *scene, struct ViewLayer *view_layer, struct CurveMapping *curve);
void BKE_paint_invalidate_overlay_all(void);
eOverlayControlFlags BKE_paint_get_overlay_flags(void);
void BKE_paint_reset_overlay_invalid(eOverlayControlFlags flag);
void BKE_paint_set_overlay_override(enum eOverlayFlags flag);

/* palettes */
void                 BKE_palette_free(struct Palette *palette);
struct Palette      *BKE_palette_add(struct Main *bmain, const char *name);
void BKE_palette_copy_data(
        struct Main *bmain, struct Palette *palette_dst, const struct Palette *palette_src, const int flag);
struct Palette      *BKE_palette_copy(struct Main *bmain, const struct Palette *palette);
void                 BKE_palette_make_local(struct Main *bmain, struct Palette *palette, const bool lib_local);
struct PaletteColor *BKE_palette_color_add(struct Palette *palette);
bool                 BKE_palette_is_empty(const struct Palette *palette);
void                 BKE_palette_color_remove(struct Palette *palette, struct PaletteColor *color);
void                 BKE_palette_clear(struct Palette *palette);

/* paint curves */
struct PaintCurve *BKE_paint_curve_add(struct Main *bmain, const char *name);
void BKE_paint_curve_free(struct PaintCurve *pc);
void BKE_paint_curve_copy_data(
        struct Main *bmain, struct PaintCurve *pc_dst, const struct PaintCurve *pc_src, const int flag);
struct PaintCurve *BKE_paint_curve_copy(struct Main *bmain, const struct PaintCurve *pc);
void               BKE_paint_curve_make_local(struct Main *bmain, struct PaintCurve *pc, const bool lib_local);

void BKE_paint_init(struct Main *bmain, struct Scene *sce, ePaintMode mode, const char col[3]);
void BKE_paint_free(struct Paint *p);
void BKE_paint_copy(struct Paint *src, struct Paint *tar, const int flag);

void BKE_paint_cavity_curve_preset(struct Paint *p, int preset);

eObjectMode BKE_paint_object_mode_from_paint_mode(ePaintMode mode);
struct Paint *BKE_paint_get_active_from_paintmode(struct Scene *sce, ePaintMode mode);
struct Paint *BKE_paint_get_active(struct Scene *sce, struct ViewLayer *view_layer);
struct Paint *BKE_paint_get_active_from_context(const struct bContext *C);
ePaintMode BKE_paintmode_get_active_from_context(const struct bContext *C);
struct Brush *BKE_paint_brush(struct Paint *paint);
void BKE_paint_brush_set(struct Paint *paint, struct Brush *br);
struct Palette *BKE_paint_palette(struct Paint *paint);
void BKE_paint_palette_set(struct Paint *p, struct Palette *palette);
void BKE_paint_curve_set(struct Brush *br, struct PaintCurve *pc);
void BKE_paint_curve_clamp_endpoint_add_index(struct PaintCurve *pc, const int add_index);

void BKE_paint_data_warning(struct ReportList *reports, bool uvs, bool mat, bool tex, bool stencil);
bool BKE_paint_proj_mesh_data_check(struct Scene *scene, struct Object *ob, bool *uvs, bool *mat, bool *tex, bool *stencil);

/* testing face select mode
 * Texture paint could be removed since selected faces are not used
 * however hiding faces is useful */
bool BKE_paint_select_face_test(struct Object *ob);
bool BKE_paint_select_vert_test(struct Object *ob);
bool BKE_paint_select_elem_test(struct Object *ob);

/* partial visibility */
bool paint_is_face_hidden(const struct MLoopTri *lt, const struct MVert *mvert, const struct MLoop *mloop);
bool paint_is_grid_face_hidden(const unsigned int *grid_hidden,
                               int gridsize, int x, int y);
bool paint_is_bmesh_face_hidden(struct BMFace *f);

/* paint masks */
float paint_grid_paint_mask(const struct GridPaintMask *gpm, unsigned level,
                            unsigned x, unsigned y);

/* stroke related */
bool paint_calculate_rake_rotation(struct UnifiedPaintSettings *ups, struct Brush *brush, const float mouse_pos[2]);
void paint_update_brush_rake_rotation(struct UnifiedPaintSettings *ups, struct Brush *brush, float rotation);

void BKE_paint_stroke_get_average(struct Scene *scene, struct Object *ob, float stroke[3]);

/* Used for both vertex color and weight paint */
struct SculptVertexPaintGeomMap {
	int *vert_map_mem;
	struct MeshElemMap *vert_to_loop;
	int *poly_map_mem;
	struct MeshElemMap *vert_to_poly;
};

/* Session data (mode-specific) */

typedef struct SculptSession {
	/* Mesh data (not copied) can come either directly from a Mesh, or from a MultiresDM */
	struct MultiresModifierData *multires; /* Special handling for multires meshes */
	struct MVert *mvert;
	struct MPoly *mpoly;
	struct MLoop *mloop;
	int totvert, totpoly;
	struct KeyBlock *kb;
	float *vmask;

	/* Mesh connectivity */
	struct MeshElemMap *pmap;
	int *pmap_mem;

	/* BMesh for dynamic topology sculpting */
	struct BMesh *bm;
	int cd_vert_node_offset;
	int cd_face_node_offset;
	bool bm_smooth_shading;
	/* Undo/redo log for dynamic topology sculpting */
	struct BMLog *bm_log;

	/* PBVH acceleration structure */
	struct PBVH *pbvh;
	bool show_diffuse_color;
	bool show_mask;

	/* Painting on deformed mesh */
	bool modifiers_active; /* object is deformed with some modifiers */
	float (*orig_cos)[3]; /* coords of undeformed mesh */
	float (*deform_cos)[3]; /* coords of deformed mesh but without stroke displacement */
	float (*deform_imats)[3][3]; /* crazyspace deformation matrices */

	/* Partial redraw */
	bool partial_redraw;

	/* Used to cache the render of the active texture */
	unsigned int texcache_side, *texcache, texcache_actual;
	struct ImagePool *tex_pool;

	/* Layer brush persistence between strokes */
	float (*layer_co)[3]; /* Copy of the mesh vertices' locations */

	struct StrokeCache *cache;

	union {
		struct {
			struct SculptVertexPaintGeomMap gmap;

			/* For non-airbrush painting to re-apply from the original (MLoop aligned). */
			unsigned int *previous_color;
		} vpaint;

		struct {
			struct SculptVertexPaintGeomMap gmap;
			/* Keep track of how much each vertex has been painted (non-airbrush only). */
			float *alpha_weight;

			/* Needed to continuously re-apply over the same weights (BRUSH_ACCUMULATE disabled).
			 * Lazy initialize as needed (flag is set to 1 to tag it as uninitialized). */
			struct MDeformVert *dvert_prev;
		} wpaint;

		/* TODO: identify sculpt-only fields */
		// struct { ... } sculpt;
	} mode;
	int mode_type;

	/* This flag prevents PBVH from being freed when creating the vp_handle for texture paint. */
	bool building_vp_handle;
} SculptSession;

void BKE_sculptsession_free(struct Object *ob);
void BKE_sculptsession_free_deformMats(struct SculptSession *ss);
void BKE_sculptsession_free_vwpaint_data(struct SculptSession *ss);
void BKE_sculptsession_bm_to_me(struct Object *ob, bool reorder);
void BKE_sculptsession_bm_to_me_for_render(struct Object *object);
void BKE_sculpt_update_mesh_elements(
        struct Depsgraph *depsgraph, struct Scene *scene, struct Sculpt *sd, struct Object *ob,
        bool need_pmap, bool need_mask);
struct MultiresModifierData *BKE_sculpt_multires_active(struct Scene *scene, struct Object *ob);
int BKE_sculpt_mask_layers_ensure(struct Object *ob,
                                  struct MultiresModifierData *mmd);
void BKE_sculpt_toolsettings_data_ensure(struct Scene *scene);

struct PBVH *BKE_sculpt_object_pbvh_ensure(struct Object *ob, struct Mesh *me_eval_deform);

enum {
	SCULPT_MASK_LAYER_CALC_VERT = (1 << 0),
	SCULPT_MASK_LAYER_CALC_LOOP = (1 << 1)
};
#endif
