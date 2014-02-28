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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: some of this file.
 *
 * Contributor(s): Jens Ole Wund (bjornmose), Campbell Barton (ideasman42)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/sculpt_paint/paint_image.c
 *  \ingroup edsculpt
 *  \brief Functions to paint images in 2D and 3D.
 */

#include <float.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_memarena.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"

#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_idprop.h"
#include "BKE_brush.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_colortools.h"

#include "BKE_editmesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_view2d.h"

#include "ED_image.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_uvedit.h"
#include "ED_view3d.h"
#include "ED_mesh.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "GPU_draw.h"

#include "IMB_colormanagement.h"

#include "paint_intern.h"

typedef struct UndoImageTile {
	struct UndoImageTile *next, *prev;

	char idname[MAX_ID_NAME];  /* name instead of pointer*/
	char ibufname[IB_FILENAME_SIZE];

	union {
		float        *fp;
		unsigned int *uint;
		void         *pt;
	} rect;

	unsigned short *mask;

	int x, y;

	short source, use_float;
	char gen_type;
} UndoImageTile;

/* this is a static resource for non-globality,
 * Maybe it should be exposed as part of the
 * paint operation, but for now just give a public interface */
static ImagePaintPartialRedraw imapaintpartial = {0, 0, 0, 0, 0};

ImagePaintPartialRedraw *get_imapaintpartial(void)
{
	return &imapaintpartial;
}

void set_imapaintpartial(struct ImagePaintPartialRedraw *ippr)
{
	imapaintpartial = *ippr;
}

/* UNDO */

static void undo_copy_tile(UndoImageTile *tile, ImBuf *tmpibuf, ImBuf *ibuf, int restore)
{
	/* copy or swap contents of tile->rect and region in ibuf->rect */
	IMB_rectcpy(tmpibuf, ibuf, 0, 0, tile->x * IMAPAINT_TILE_SIZE,
	            tile->y * IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE);

	if (ibuf->rect_float) {
		SWAP(float *, tmpibuf->rect_float, tile->rect.fp);
	}
	else {
		SWAP(unsigned int *, tmpibuf->rect, tile->rect.uint);
	}
	
	if (restore)
		IMB_rectcpy(ibuf, tmpibuf, tile->x * IMAPAINT_TILE_SIZE,
		            tile->y * IMAPAINT_TILE_SIZE, 0, 0, IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE);
}

void *image_undo_find_tile(Image *ima, ImBuf *ibuf, int x_tile, int y_tile, unsigned short **mask)
{
	ListBase *lb = undo_paint_push_get_list(UNDO_PAINT_IMAGE);
	UndoImageTile *tile;
	short use_float = ibuf->rect_float ? 1 : 0;

	for (tile = lb->first; tile; tile = tile->next) {
		if (tile->x == x_tile && tile->y == y_tile && ima->gen_type == tile->gen_type && ima->source == tile->source) {
			if (tile->use_float == use_float) {
				if (strcmp(tile->idname, ima->id.name) == 0 && strcmp(tile->ibufname, ibuf->name) == 0) {
					if (mask) {
						/* allocate mask if requested */
						if (!tile->mask) {
							tile->mask = MEM_callocN(sizeof(unsigned short) * IMAPAINT_TILE_SIZE * IMAPAINT_TILE_SIZE,
							                         "UndoImageTile.mask");
						}

						*mask = tile->mask;
					}

					return tile->rect.pt;
				}
			}
		}
	}
	
	return NULL;
}

void *image_undo_push_tile(Image *ima, ImBuf *ibuf, ImBuf **tmpibuf, int x_tile, int y_tile)
{
	ListBase *lb = undo_paint_push_get_list(UNDO_PAINT_IMAGE);
	UndoImageTile *tile;
	int allocsize;
	short use_float = ibuf->rect_float ? 1 : 0;
	void *data;

	/* check if tile is already pushed */
	data = image_undo_find_tile(ima, ibuf, x_tile, y_tile, NULL);
	if (data)
		return data;
	
	if (*tmpibuf == NULL)
		*tmpibuf = IMB_allocImBuf(IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, 32, IB_rectfloat | IB_rect);
	
	tile = MEM_callocN(sizeof(UndoImageTile), "UndoImageTile");
	BLI_strncpy(tile->idname, ima->id.name, sizeof(tile->idname));
	tile->x = x_tile;
	tile->y = y_tile;

	allocsize = IMAPAINT_TILE_SIZE * IMAPAINT_TILE_SIZE * 4;
	allocsize *= (ibuf->rect_float) ? sizeof(float) : sizeof(char);
	tile->rect.pt = MEM_mapallocN(allocsize, "UndeImageTile.rect");

	BLI_strncpy(tile->ibufname, ibuf->name, sizeof(tile->ibufname));

	tile->gen_type = ima->gen_type;
	tile->source = ima->source;
	tile->use_float = use_float;

	undo_copy_tile(tile, *tmpibuf, ibuf, 0);
	undo_paint_push_count_alloc(UNDO_PAINT_IMAGE, allocsize);

	BLI_addtail(lb, tile);
	
	return tile->rect.pt;
}

void image_undo_remove_masks(void)
{
	ListBase *lb = undo_paint_push_get_list(UNDO_PAINT_IMAGE);
	UndoImageTile *tile;

	for (tile = lb->first; tile; tile = tile->next) {
		if (tile->mask) {
			MEM_freeN(tile->mask);
			tile->mask = NULL;
		}
	}
}

void ED_image_undo_restore(bContext *C, ListBase *lb)
{
	Main *bmain = CTX_data_main(C);
	Image *ima = NULL;
	ImBuf *ibuf, *tmpibuf;
	UndoImageTile *tile;

	tmpibuf = IMB_allocImBuf(IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, 32,
	                         IB_rectfloat | IB_rect);

	for (tile = lb->first; tile; tile = tile->next) {
		short use_float;

		/* find image based on name, pointer becomes invalid with global undo */
		if (ima && strcmp(tile->idname, ima->id.name) == 0) {
			/* ima is valid */
		}
		else {
			ima = BLI_findstring(&bmain->image, tile->idname, offsetof(ID, name));
		}

		ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);

		if (ima && ibuf && strcmp(tile->ibufname, ibuf->name) != 0) {
			/* current ImBuf filename was changed, probably current frame
			 * was changed when paiting on image sequence, rather than storing
			 * full image user (which isn't so obvious, btw) try to find ImBuf with
			 * matched file name in list of already loaded images */

			BKE_image_release_ibuf(ima, ibuf, NULL);

			ibuf = BKE_image_get_ibuf_with_name(ima, tile->ibufname);
		}

		if (!ima || !ibuf || !(ibuf->rect || ibuf->rect_float)) {
			BKE_image_release_ibuf(ima, ibuf, NULL);
			continue;
		}

		if (ima->gen_type != tile->gen_type || ima->source != tile->source) {
			BKE_image_release_ibuf(ima, ibuf, NULL);
			continue;
		}

		use_float = ibuf->rect_float ? 1 : 0;

		if (use_float != tile->use_float) {
			BKE_image_release_ibuf(ima, ibuf, NULL);
			continue;
		}

		undo_copy_tile(tile, tmpibuf, ibuf, 1);

		GPU_free_image(ima); /* force OpenGL reload */
		if (ibuf->rect_float)
			ibuf->userflags |= IB_RECT_INVALID; /* force recreate of char rect */
		if (ibuf->mipmap[0])
			ibuf->userflags |= IB_MIPMAP_INVALID;  /* force mipmap recreatiom */
		ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;

		BKE_image_release_ibuf(ima, ibuf, NULL);
	}

	IMB_freeImBuf(tmpibuf);
}

void ED_image_undo_free(ListBase *lb)
{
	UndoImageTile *tile;

	for (tile = lb->first; tile; tile = tile->next)
		MEM_freeN(tile->rect.pt);
}

/* Imagepaint Partial Redraw & Dirty Region */

void ED_imapaint_clear_partial_redraw(void)
{
	memset(&imapaintpartial, 0, sizeof(imapaintpartial));
}

void imapaint_region_tiles(ImBuf *ibuf, int x, int y, int w, int h, int *tx, int *ty, int *tw, int *th)
{
	int srcx = 0, srcy = 0;

	IMB_rectclip(ibuf, NULL, &x, &y, &srcx, &srcy, &w, &h);

	*tw = ((x + w - 1) >> IMAPAINT_TILE_BITS);
	*th = ((y + h - 1) >> IMAPAINT_TILE_BITS);
	*tx = (x >> IMAPAINT_TILE_BITS);
	*ty = (y >> IMAPAINT_TILE_BITS);
}

void ED_imapaint_dirty_region(Image *ima, ImBuf *ibuf, int x, int y, int w, int h)
{
	ImBuf *tmpibuf = NULL;
	int tilex, tiley, tilew, tileh, tx, ty;
	int srcx = 0, srcy = 0;

	IMB_rectclip(ibuf, NULL, &x, &y, &srcx, &srcy, &w, &h);

	if (w == 0 || h == 0)
		return;
	
	if (!imapaintpartial.enabled) {
		imapaintpartial.x1 = x;
		imapaintpartial.y1 = y;
		imapaintpartial.x2 = x + w;
		imapaintpartial.y2 = y + h;
		imapaintpartial.enabled = 1;
	}
	else {
		imapaintpartial.x1 = min_ii(imapaintpartial.x1, x);
		imapaintpartial.y1 = min_ii(imapaintpartial.y1, y);
		imapaintpartial.x2 = max_ii(imapaintpartial.x2, x + w);
		imapaintpartial.y2 = max_ii(imapaintpartial.y2, y + h);
	}

	imapaint_region_tiles(ibuf, x, y, w, h, &tilex, &tiley, &tilew, &tileh);

	for (ty = tiley; ty <= tileh; ty++)
		for (tx = tilex; tx <= tilew; tx++)
			image_undo_push_tile(ima, ibuf, &tmpibuf, tx, ty);

	ibuf->userflags |= IB_BITMAPDIRTY;
	
	if (tmpibuf)
		IMB_freeImBuf(tmpibuf);
}

void imapaint_image_update(SpaceImage *sima, Image *image, ImBuf *ibuf, short texpaint)
{
	if (imapaintpartial.x1 != imapaintpartial.x2 &&
	    imapaintpartial.y1 != imapaintpartial.y2)
	{
		IMB_partial_display_buffer_update_delayed(ibuf, imapaintpartial.x1, imapaintpartial.y1,
		                                          imapaintpartial.x2, imapaintpartial.y2);
	}
	
	if (ibuf->mipmap[0])
		ibuf->userflags |= IB_MIPMAP_INVALID;

	/* todo: should set_tpage create ->rect? */
	if (texpaint || (sima && sima->lock)) {
		int w = imapaintpartial.x2 - imapaintpartial.x1;
		int h = imapaintpartial.y2 - imapaintpartial.y1;
		/* Testing with partial update in uv editor too */
		GPU_paint_update_image(image, imapaintpartial.x1, imapaintpartial.y1, w, h); //!texpaint);
	}
}

/************************ image paint poll ************************/

static Brush *image_paint_brush(bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *settings = scene->toolsettings;

	return BKE_paint_brush(&settings->imapaint.paint);
}

static int image_paint_poll(bContext *C)
{
	Object *obact = CTX_data_active_object(C);

	if (!image_paint_brush(C))
		return 0;

	if ((obact && obact->mode & OB_MODE_TEXTURE_PAINT) && CTX_wm_region_view3d(C)) {
		return 1;
	}
	else {
		SpaceImage *sima = CTX_wm_space_image(C);

		if (sima) {
			ARegion *ar = CTX_wm_region(C);

			if ((sima->mode == SI_MODE_PAINT) && ar->regiontype == RGN_TYPE_WINDOW) {
				return 1;
			}
		}
	}

	return 0;
}

static int image_paint_2d_clone_poll(bContext *C)
{
	Brush *brush = image_paint_brush(C);

	if (!CTX_wm_region_view3d(C) && image_paint_poll(C))
		if (brush && (brush->imagepaint_tool == PAINT_TOOL_CLONE))
			if (brush->clone.image)
				return 1;
	
	return 0;
}

/************************ paint operator ************************/
typedef enum TexPaintMode {
	PAINT_MODE_2D,
	PAINT_MODE_3D_PROJECT
} TexPaintMode;

typedef struct PaintOperation {
	TexPaintMode mode;

	void *custom_paint;

	float prevmouse[2];
	double starttime;

	ViewContext vc;
} PaintOperation;

void paint_brush_init_tex(Brush *brush)
{
	/* init mtex nodes */
	if (brush) {
		MTex *mtex = &brush->mtex;
		if (mtex->tex && mtex->tex->nodetree)
			ntreeTexBeginExecTree(mtex->tex->nodetree);  /* has internal flag to detect it only does it once */
		mtex = &brush->mask_mtex;
		if (mtex->tex && mtex->tex->nodetree)
			ntreeTexBeginExecTree(mtex->tex->nodetree);
	}
}

void paint_brush_exit_tex(Brush *brush)
{
	if (brush) {
		MTex *mtex = &brush->mtex;
		if (mtex->tex && mtex->tex->nodetree)
			ntreeTexEndExecTree(mtex->tex->nodetree->execdata);
		mtex = &brush->mask_mtex;
		if (mtex->tex && mtex->tex->nodetree)
			ntreeTexEndExecTree(mtex->tex->nodetree->execdata);
	}
}


static PaintOperation *texture_paint_init(bContext *C, wmOperator *op, float mouse[2])
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *settings = scene->toolsettings;
	PaintOperation *pop = MEM_callocN(sizeof(PaintOperation), "PaintOperation"); /* caller frees */
	int mode = RNA_enum_get(op->ptr, "mode");
	view3d_set_viewcontext(C, &pop->vc);

	pop->prevmouse[0] = mouse[0];
	pop->prevmouse[1] = mouse[1];

	/* initialize from context */
	if (CTX_wm_region_view3d(C)) {
		pop->mode = PAINT_MODE_3D_PROJECT;
		pop->custom_paint = paint_proj_new_stroke(C, OBACT, pop->prevmouse, mode);
	}
	else {
		pop->mode = PAINT_MODE_2D;
		pop->custom_paint = paint_2d_new_stroke(C, op);
	}

	if (!pop->custom_paint) {
		MEM_freeN(pop);
		return NULL;
	}

	settings->imapaint.flag |= IMAGEPAINT_DRAWING;
	ED_undo_paint_push_begin(UNDO_PAINT_IMAGE, op->type->name,
	                      ED_image_undo_restore, ED_image_undo_free);

	{
		UnifiedPaintSettings *ups = &settings->unified_paint_settings;
		ups->stroke_active = true;
	}

	return pop;
}

static void paint_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr)
{
	PaintOperation *pop = paint_stroke_mode_data(stroke);
	Scene *scene = CTX_data_scene(C);
	Brush *brush = BKE_paint_brush(&scene->toolsettings->imapaint.paint);

	/* initial brush values. Maybe it should be considered moving these to stroke system */
	float startsize = (float)BKE_brush_size_get(scene, brush);
	float startalpha = BKE_brush_alpha_get(scene, brush);

	float mouse[2];
	float pressure;
	int eraser;

	RNA_float_get_array(itemptr, "mouse", mouse);
	pressure = RNA_float_get(itemptr, "pressure");
	eraser = RNA_boolean_get(itemptr, "pen_flip");

	if (BKE_brush_use_alpha_pressure(scene, brush))
		BKE_brush_alpha_set(scene, brush, max_ff(0.0f, startalpha * pressure));
	if (BKE_brush_use_size_pressure(scene, brush))
		BKE_brush_size_set(scene, brush, (int)max_ff(1.0f, startsize * pressure));

	if (pop->mode == PAINT_MODE_3D_PROJECT) {
		paint_proj_stroke(C, pop->custom_paint, pop->prevmouse, mouse);
	}
	else {
		paint_2d_stroke(pop->custom_paint, pop->prevmouse, mouse, eraser);
	}

	pop->prevmouse[0] = mouse[0];
	pop->prevmouse[1] = mouse[1];

	/* restore brush values */
	BKE_brush_alpha_set(scene, brush, startalpha);
	BKE_brush_size_set(scene, brush, startsize);
}

static void paint_stroke_redraw(const bContext *C, struct PaintStroke *stroke, bool final)
{
	PaintOperation *pop = paint_stroke_mode_data(stroke);

	if (pop->mode == PAINT_MODE_3D_PROJECT) {
		paint_proj_redraw(C, pop->custom_paint, final);
	}
	else {
		paint_2d_redraw(C, pop->custom_paint, final);
	}
}

static void paint_stroke_done(const bContext *C, struct PaintStroke *stroke)
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *settings = scene->toolsettings;
	PaintOperation *pop = paint_stroke_mode_data(stroke);

	settings->imapaint.flag &= ~IMAGEPAINT_DRAWING;

	if (pop->mode == PAINT_MODE_3D_PROJECT) {
		paint_proj_stroke_done(pop->custom_paint);
	}
	else {
		paint_2d_stroke_done(pop->custom_paint);
	}

	ED_undo_paint_push_end(UNDO_PAINT_IMAGE);

	/* duplicate warning, see texpaint_init */
#if 0
	if (pop->s.warnmultifile)
		BKE_reportf(op->reports, RPT_WARNING, "Image requires 4 color channels to paint: %s", pop->s.warnmultifile);
	if (pop->s.warnpackedfile)
		BKE_reportf(op->reports, RPT_WARNING, "Packed MultiLayer files cannot be painted: %s", pop->s.warnpackedfile);
#endif
	MEM_freeN(pop);

	{
		UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
		ups->stroke_active = false;
	}
}

static bool paint_stroke_test_start(bContext *UNUSED(C), wmOperator *UNUSED(op), const float UNUSED(mouse[2]))
{
	return true;
}


static int paint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	PaintOperation *pop;
	float mouse[2];
	int retval;

	/* TODO Should avoid putting this here. Instead, last position should be requested
	 * from stroke system. */
	mouse[0] = event->mval[0];
	mouse[1] = event->mval[1];

	if (!(pop = texture_paint_init(C, op, mouse))) {
		return OPERATOR_CANCELLED;
	}

	op->customdata = paint_stroke_new(C, NULL, paint_stroke_test_start,
	                                  paint_stroke_update_step,
	                                  paint_stroke_redraw,
	                                  paint_stroke_done, event->type);
	paint_stroke_set_mode_data(op->customdata, pop);
	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	retval = op->type->modal(C, op, event);
	OPERATOR_RETVAL_CHECK(retval);
	BLI_assert(retval == OPERATOR_RUNNING_MODAL);

	return OPERATOR_RUNNING_MODAL;
}

static int paint_exec(bContext *C, wmOperator *op)
{
	PaintOperation *pop;
	PropertyRNA *strokeprop;
	PointerRNA firstpoint;
	float mouse[2];

	strokeprop = RNA_struct_find_property(op->ptr, "stroke");

	if (!RNA_property_collection_lookup_int(op->ptr, strokeprop, 0, &firstpoint))
		return OPERATOR_CANCELLED;

	RNA_float_get_array(&firstpoint, "mouse", mouse);

	if (!(pop = texture_paint_init(C, op, mouse))) {
		return OPERATOR_CANCELLED;
	}

	op->customdata = paint_stroke_new(C, NULL, paint_stroke_test_start,
	                                  paint_stroke_update_step,
	                                  paint_stroke_redraw,
	                                  paint_stroke_done, 0);
	paint_stroke_set_mode_data(op->customdata, pop);

	/* frees op->customdata */
	paint_stroke_exec(C, op);

	return OPERATOR_FINISHED;
}

void PAINT_OT_image_paint(wmOperatorType *ot)
{
	static EnumPropertyItem stroke_mode_items[] = {
		{BRUSH_STROKE_NORMAL, "NORMAL", 0, "Normal", "Apply brush normally"},
		{BRUSH_STROKE_INVERT, "INVERT", 0, "Invert", "Invert action of brush for duration of stroke"},
		{0}
	};

	/* identifiers */
	ot->name = "Image Paint";
	ot->idname = "PAINT_OT_image_paint";
	ot->description = "Paint a stroke into the image";

	/* api callbacks */
	ot->invoke = paint_invoke;
	ot->modal = paint_stroke_modal;
	ot->exec = paint_exec;
	ot->poll = image_paint_poll;
	ot->cancel = paint_stroke_cancel;

	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;

	RNA_def_enum(ot->srna, "mode", stroke_mode_items, BRUSH_STROKE_NORMAL,
	             "Paint Stroke Mode",
	             "Action taken when a paint stroke is made");

	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
}


int get_imapaint_zoom(bContext *C, float *zoomx, float *zoomy)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);

	if (!rv3d) {
		SpaceImage *sima = CTX_wm_space_image(C);
		ARegion *ar = CTX_wm_region(C);

		if (sima->mode == SI_MODE_PAINT) {
			ED_space_image_get_zoom(sima, ar, zoomx, zoomy);

			return 1;
		}
	}

	*zoomx = *zoomy = 1;

	return 0;
}

/************************ cursor drawing *******************************/

void brush_drawcursor_texpaint_uvsculpt(bContext *C, int x, int y, void *UNUSED(customdata))
{
#define PX_SIZE_FADE_MAX 12.0f
#define PX_SIZE_FADE_MIN 4.0f

	Scene *scene = CTX_data_scene(C);
	//Brush *brush = image_paint_brush(C);
	Paint *paint = BKE_paint_get_active_from_context(C);
	Brush *brush = BKE_paint_brush(paint);

	if (paint && brush && paint->flags & PAINT_SHOW_BRUSH) {
		float zoomx, zoomy;
		const float size = (float)BKE_brush_size_get(scene, brush);
		short use_zoom;
		float pixel_size;
		float alpha = 0.5f;

		use_zoom = get_imapaint_zoom(C, &zoomx, &zoomy);

		if (use_zoom) {
			pixel_size = size * max_ff(zoomx, zoomy);
		}
		else {
			pixel_size = size;
		}

		/* fade out the brush (cheap trick to work around brush interfering with sampling [#])*/
		if (pixel_size < PX_SIZE_FADE_MIN) {
			return;
		}
		else if (pixel_size < PX_SIZE_FADE_MAX) {
			alpha *= (pixel_size - PX_SIZE_FADE_MIN) / (PX_SIZE_FADE_MAX - PX_SIZE_FADE_MIN);
		}

		glPushMatrix();

		glTranslatef((float)x, (float)y, 0.0f);

		/* No need to scale for uv sculpting, on the contrary it might be useful to keep un-scaled */
		if (use_zoom)
			glScalef(zoomx, zoomy, 1.0f);

		glColor4f(brush->add_col[0], brush->add_col[1], brush->add_col[2], alpha);
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_BLEND);
		{
			UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
			/* hrmf, duplicate paint_draw_cursor logic here */
			if (ups->stroke_active && BKE_brush_use_size_pressure(scene, brush)) {
				/* inner at full alpha */
				glutil_draw_lined_arc(0, (float)(M_PI * 2.0), size * ups->pressure_value, 40);
				/* outer at half alpha */
				glColor4f(brush->add_col[0], brush->add_col[1], brush->add_col[2], alpha * 0.5f);
			}
		}
		glutil_draw_lined_arc(0, (float)(M_PI * 2.0), size, 40);
		glDisable(GL_BLEND);
		glDisable(GL_LINE_SMOOTH);

		glPopMatrix();
	}
#undef PX_SIZE_FADE_MAX
#undef PX_SIZE_FADE_MIN
}

static void toggle_paint_cursor(bContext *C, int enable)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *settings = scene->toolsettings;

	if (settings->imapaint.paintcursor && !enable) {
		WM_paint_cursor_end(wm, settings->imapaint.paintcursor);
		settings->imapaint.paintcursor = NULL;
		paint_cursor_delete_textures();
	}
	else if (enable)
		paint_cursor_start(C, image_paint_poll);
}

/* enable the paint cursor if it isn't already.
 *
 * purpose is to make sure the paint cursor is shown if paint
 * mode is enabled in the image editor. the paint poll will
 * ensure that the cursor is hidden when not in paint mode */
void ED_space_image_paint_update(wmWindowManager *wm, ToolSettings *settings)
{
	wmWindow *win;
	ScrArea *sa;
	ImagePaintSettings *imapaint = &settings->imapaint;
	int enabled = FALSE;

	for (win = wm->windows.first; win; win = win->next)
		for (sa = win->screen->areabase.first; sa; sa = sa->next)
			if (sa->spacetype == SPACE_IMAGE)
				if (((SpaceImage *)sa->spacedata.first)->mode == SI_MODE_PAINT)
					enabled = TRUE;

	if (enabled) {
		BKE_paint_init(&imapaint->paint, PAINT_CURSOR_TEXTURE_PAINT);

		paint_cursor_start_explicit(&imapaint->paint, wm, image_paint_poll);
	}
	else {
		paint_cursor_delete_textures();
	}
}

/************************ grab clone operator ************************/

typedef struct GrabClone {
	float startoffset[2];
	int startx, starty;
} GrabClone;

static void grab_clone_apply(bContext *C, wmOperator *op)
{
	Brush *brush = image_paint_brush(C);
	float delta[2];

	RNA_float_get_array(op->ptr, "delta", delta);
	add_v2_v2(brush->clone.offset, delta);
	ED_region_tag_redraw(CTX_wm_region(C));
}

static int grab_clone_exec(bContext *C, wmOperator *op)
{
	grab_clone_apply(C, op);

	return OPERATOR_FINISHED;
}

static int grab_clone_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Brush *brush = image_paint_brush(C);
	GrabClone *cmv;

	cmv = MEM_callocN(sizeof(GrabClone), "GrabClone");
	copy_v2_v2(cmv->startoffset, brush->clone.offset);
	cmv->startx = event->x;
	cmv->starty = event->y;
	op->customdata = cmv;

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int grab_clone_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	Brush *brush = image_paint_brush(C);
	ARegion *ar = CTX_wm_region(C);
	GrabClone *cmv = op->customdata;
	float startfx, startfy, fx, fy, delta[2];
	int xmin = ar->winrct.xmin, ymin = ar->winrct.ymin;

	switch (event->type) {
		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE: // XXX hardcoded
			MEM_freeN(op->customdata);
			return OPERATOR_FINISHED;
		case MOUSEMOVE:
			/* mouse moved, so move the clone image */
			UI_view2d_region_to_view(&ar->v2d, cmv->startx - xmin, cmv->starty - ymin, &startfx, &startfy);
			UI_view2d_region_to_view(&ar->v2d, event->x - xmin, event->y - ymin, &fx, &fy);

			delta[0] = fx - startfx;
			delta[1] = fy - startfy;
			RNA_float_set_array(op->ptr, "delta", delta);

			copy_v2_v2(brush->clone.offset, cmv->startoffset);

			grab_clone_apply(C, op);
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static void grab_clone_cancel(bContext *UNUSED(C), wmOperator *op)
{
	MEM_freeN(op->customdata);
}

void PAINT_OT_grab_clone(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Grab Clone";
	ot->idname = "PAINT_OT_grab_clone";
	ot->description = "Move the clone source image";
	
	/* api callbacks */
	ot->exec = grab_clone_exec;
	ot->invoke = grab_clone_invoke;
	ot->modal = grab_clone_modal;
	ot->cancel = grab_clone_cancel;
	ot->poll = image_paint_2d_clone_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* properties */
	RNA_def_float_vector(ot->srna, "delta", 2, NULL, -FLT_MAX, FLT_MAX, "Delta", "Delta offset of clone image in 0.0..1.0 coordinates", -1.0f, 1.0f);
}

/******************** sample color operator ********************/
typedef struct {
	bool show_cursor;
	short event_type;
} SampleColorData;

static int sample_color_exec(bContext *C, wmOperator *op)
{
	Paint *paint = BKE_paint_get_active_from_context(C);
	Brush *brush = BKE_paint_brush(paint);
	ARegion *ar = CTX_wm_region(C);
	wmWindow *win = CTX_wm_window(C);
	bool show_cursor = ((paint->flags & PAINT_SHOW_BRUSH) != 0);
	int location[2];

	paint->flags &= ~PAINT_SHOW_BRUSH;

	/* force redraw without cursor */
	WM_paint_cursor_tag_redraw(win, ar);
	WM_redraw_windows(C);

	RNA_int_get_array(op->ptr, "location", location);
	paint_sample_color(C, ar, location[0], location[1]);

	if (show_cursor) {
		paint->flags |= PAINT_SHOW_BRUSH;
	}

	WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);
	
	return OPERATOR_FINISHED;
}

static int sample_color_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Paint *paint = BKE_paint_get_active_from_context(C);
	Brush *brush = BKE_paint_brush(paint);
	SampleColorData *data = MEM_mallocN(sizeof(SampleColorData), "sample color custom data");
	ARegion *ar = CTX_wm_region(C);
	wmWindow *win = CTX_wm_window(C);

	data->event_type = event->type;
	data->show_cursor = ((paint->flags & PAINT_SHOW_BRUSH) != 0);
	op->customdata = data;
	paint->flags &= ~PAINT_SHOW_BRUSH;

	/* force redraw without cursor */
	WM_paint_cursor_tag_redraw(win, ar);
	WM_redraw_windows(C);

	RNA_int_set_array(op->ptr, "location", event->mval);
	paint_sample_color(C, ar, event->mval[0], event->mval[1]);

	WM_event_add_modal_handler(C, op);
	WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);

	return OPERATOR_RUNNING_MODAL;
}

static int sample_color_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	SampleColorData *data = op->customdata;
	Paint *paint = BKE_paint_get_active_from_context(C);
	Brush *brush = BKE_paint_brush(paint);

	if ((event->type == data->event_type) && (event->val == KM_RELEASE)) {
		if (data->show_cursor) {
			paint->flags |= PAINT_SHOW_BRUSH;
		}

		MEM_freeN(data);
		return OPERATOR_FINISHED;
	}

	switch (event->type) {
		case MOUSEMOVE:
		{
			ARegion *ar = CTX_wm_region(C);
			RNA_int_set_array(op->ptr, "location", event->mval);
			paint_sample_color(C, ar, event->mval[0], event->mval[1]);
			WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);
			break;
		}
	}

	return OPERATOR_RUNNING_MODAL;
}

static int sample_color_poll(bContext *C)
{
	return (image_paint_poll(C) || vertex_paint_poll(C));
}

void PAINT_OT_sample_color(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Sample Color";
	ot->idname = "PAINT_OT_sample_color";
	ot->description = "Use the mouse to sample a color in the image";
	
	/* api callbacks */
	ot->exec = sample_color_exec;
	ot->invoke = sample_color_invoke;
	ot->modal = sample_color_modal;
	ot->poll = sample_color_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_int_vector(ot->srna, "location", 2, NULL, 0, INT_MAX, "Location", "Cursor location in region coordinates", 0, 16384);
}

/******************** texture paint toggle operator ********************/

static int texture_paint_toggle_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	if (ob == NULL || ob->type != OB_MESH)
		return 0;
	if (!ob->data || ((ID *)ob->data)->lib)
		return 0;
	if (CTX_data_edit_object(C))
		return 0;

	return 1;
}

static int texture_paint_toggle_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	const int mode_flag = OB_MODE_TEXTURE_PAINT;
	const bool is_mode_set = (ob->mode & mode_flag) != 0;
	Mesh *me;

	if (!is_mode_set) {
		if (!ED_object_mode_compat_set(C, ob, mode_flag, op->reports)) {
			return OPERATOR_CANCELLED;
		}
	}

	me = BKE_mesh_from_object(ob);

	if (ob->mode & mode_flag) {
		ob->mode &= ~mode_flag;

		if (U.glreslimit != 0)
			GPU_free_images();
		GPU_paint_set_mipmap(1);

		toggle_paint_cursor(C, 0);
	}
	else {
		ob->mode |= mode_flag;

		if (me->mtface == NULL)
			me->mtface = CustomData_add_layer(&me->fdata, CD_MTFACE, CD_DEFAULT,
			                                  NULL, me->totface);

		BKE_paint_init(&scene->toolsettings->imapaint.paint, PAINT_CURSOR_TEXTURE_PAINT);

		if (U.glreslimit != 0)
			GPU_free_images();
		GPU_paint_set_mipmap(0);

		toggle_paint_cursor(C, 1);
	}

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_SCENE | ND_MODE, scene);

	return OPERATOR_FINISHED;
}

void PAINT_OT_texture_paint_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Texture Paint Toggle";
	ot->idname = "PAINT_OT_texture_paint_toggle";
	ot->description = "Toggle texture paint mode in 3D view";
	
	/* api callbacks */
	ot->exec = texture_paint_toggle_exec;
	ot->poll = texture_paint_toggle_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int texture_paint_poll(bContext *C)
{
	if (texture_paint_toggle_poll(C))
		if (CTX_data_active_object(C)->mode & OB_MODE_TEXTURE_PAINT)
			return 1;
	
	return 0;
}

int image_texture_paint_poll(bContext *C)
{
	return (texture_paint_poll(C) || image_paint_poll(C));
}

int facemask_paint_poll(bContext *C)
{
	return BKE_paint_select_face_test(CTX_data_active_object(C));
}

int vert_paint_poll(bContext *C)
{
	return BKE_paint_select_vert_test(CTX_data_active_object(C));
}

int mask_paint_poll(bContext *C)
{
	return BKE_paint_select_elem_test(CTX_data_active_object(C));
}

