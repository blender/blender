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
#include "BLI_utildefines.h"
#include "BLI_threads.h"

#include "PIL_time.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_brush.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_texture.h"
#include "BKE_colortools.h"

#include "BKE_editmesh.h"

#include "UI_view2d.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_paint.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "GPU_draw.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

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

	Image *ima;
	short source, use_float;
	char gen_type;
	bool valid;
} UndoImageTile;

/* this is a static resource for non-globality,
 * Maybe it should be exposed as part of the
 * paint operation, but for now just give a public interface */
static ImagePaintPartialRedraw imapaintpartial = {0, 0, 0, 0, 0};
static SpinLock undolock;

void image_undo_init_locks(void)
{
	BLI_spin_init(&undolock);
}

void image_undo_end_locks(void)
{
	BLI_spin_end(&undolock);
}

ImagePaintPartialRedraw *get_imapaintpartial(void)
{
	return &imapaintpartial;
}

void set_imapaintpartial(struct ImagePaintPartialRedraw *ippr)
{
	imapaintpartial = *ippr;
}

/* UNDO */
typedef enum {
	COPY = 0,
	RESTORE = 1,
	RESTORE_COPY = 2
} CopyMode;

static void undo_copy_tile(UndoImageTile *tile, ImBuf *tmpibuf, ImBuf *ibuf, CopyMode mode)
{
	if (mode == COPY) {
		/* copy or swap contents of tile->rect and region in ibuf->rect */
		IMB_rectcpy(tmpibuf, ibuf, 0, 0, tile->x * IMAPAINT_TILE_SIZE,
		            tile->y * IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE);

		if (ibuf->rect_float) {
			SWAP(float *, tmpibuf->rect_float, tile->rect.fp);
		}
		else {
			SWAP(unsigned int *, tmpibuf->rect, tile->rect.uint);
		}
	}
	else {
		if (mode == RESTORE_COPY)
			IMB_rectcpy(tmpibuf, ibuf, 0, 0, tile->x * IMAPAINT_TILE_SIZE,
		                tile->y * IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE);
		/* swap to the tmpbuf for easy copying */
		if (ibuf->rect_float) {
			SWAP(float *, tmpibuf->rect_float, tile->rect.fp);
		}
		else {
			SWAP(unsigned int *, tmpibuf->rect, tile->rect.uint);
		}

		IMB_rectcpy(ibuf, tmpibuf, tile->x * IMAPAINT_TILE_SIZE,
		            tile->y * IMAPAINT_TILE_SIZE, 0, 0, IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE);

		if (mode == RESTORE) {
			if (ibuf->rect_float) {
				SWAP(float *, tmpibuf->rect_float, tile->rect.fp);
			}
			else {
				SWAP(unsigned int *, tmpibuf->rect, tile->rect.uint);
			}
		}
	}
}

void *image_undo_find_tile(Image *ima, ImBuf *ibuf, int x_tile, int y_tile, unsigned short **mask, bool validate)
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
					if (validate)
						tile->valid = true;

					return tile->rect.pt;
				}
			}
		}
	}
	
	return NULL;
}

void *image_undo_push_tile(Image *ima, ImBuf *ibuf, ImBuf **tmpibuf, int x_tile, int y_tile, unsigned short **mask, bool **valid, bool proj)
{
	ListBase *lb = undo_paint_push_get_list(UNDO_PAINT_IMAGE);
	UndoImageTile *tile;
	int allocsize;
	short use_float = ibuf->rect_float ? 1 : 0;
	void *data;

	/* check if tile is already pushed */

	/* in projective painting we keep accounting of tiles, so if we need one pushed, just push! */
	if (!proj) {
		data = image_undo_find_tile(ima, ibuf, x_tile, y_tile, mask, true);
		if (data)
			return data;
	}

	if (*tmpibuf == NULL)
		*tmpibuf = IMB_allocImBuf(IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, 32, IB_rectfloat | IB_rect);
	
	tile = MEM_callocN(sizeof(UndoImageTile), "UndoImageTile");
	BLI_strncpy(tile->idname, ima->id.name, sizeof(tile->idname));
	tile->x = x_tile;
	tile->y = y_tile;

	/* add mask explicitly here */
	if (mask)
		*mask = tile->mask = MEM_callocN(sizeof(unsigned short) * IMAPAINT_TILE_SIZE * IMAPAINT_TILE_SIZE,
		                         "UndoImageTile.mask");

	allocsize = IMAPAINT_TILE_SIZE * IMAPAINT_TILE_SIZE * 4;
	allocsize *= (ibuf->rect_float) ? sizeof(float) : sizeof(char);
	tile->rect.pt = MEM_mapallocN(allocsize, "UndeImageTile.rect");

	BLI_strncpy(tile->ibufname, ibuf->name, sizeof(tile->ibufname));

	tile->gen_type = ima->gen_type;
	tile->source = ima->source;
	tile->use_float = use_float;
	tile->valid = true;
	tile->ima = ima;

	if (valid)
		*valid = &tile->valid;

	undo_copy_tile(tile, *tmpibuf, ibuf, COPY);

	if (proj)
		BLI_spin_lock(&undolock);

	undo_paint_push_count_alloc(UNDO_PAINT_IMAGE, allocsize);
	BLI_addtail(lb, tile);

	if (proj)
		BLI_spin_unlock(&undolock);

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

static void image_undo_restore_runtime(ListBase *lb)
{
	ImBuf *ibuf, *tmpibuf;
	UndoImageTile *tile;

	tmpibuf = IMB_allocImBuf(IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, 32,
	                         IB_rectfloat | IB_rect);

	for (tile = lb->first; tile; tile = tile->next) {
		Image *ima = tile->ima;
		ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);

		undo_copy_tile(tile, tmpibuf, ibuf, RESTORE);

		GPU_free_image(ima); /* force OpenGL reload (maybe partial update will operate better?) */
		if (ibuf->rect_float)
			ibuf->userflags |= IB_RECT_INVALID; /* force recreate of char rect */
		if (ibuf->mipmap[0])
			ibuf->userflags |= IB_MIPMAP_INVALID;  /* force mipmap recreatiom */
		ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;

		BKE_image_release_ibuf(ima, ibuf, NULL);
	}

	IMB_freeImBuf(tmpibuf);
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
			 * was changed when painting on image sequence, rather than storing
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

		undo_copy_tile(tile, tmpibuf, ibuf, RESTORE_COPY);

		GPU_free_image(ima); /* force OpenGL reload */
		if (ibuf->rect_float)
			ibuf->userflags |= IB_RECT_INVALID; /* force recreate of char rect */
		if (ibuf->mipmap[0])
			ibuf->userflags |= IB_MIPMAP_INVALID;  /* force mipmap recreatiom */
		ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;

		DAG_id_tag_update(&ima->id, 0);

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

static void image_undo_end(void)
{
	ListBase *lb = undo_paint_push_get_list(UNDO_PAINT_IMAGE);
	UndoImageTile *tile;
	int deallocsize = 0;
	int allocsize = IMAPAINT_TILE_SIZE * IMAPAINT_TILE_SIZE * 4;

	/* first dispose of invalid tiles (may happen due to drag dot for instance) */
	for (tile = lb->first; tile;) {
		if (!tile->valid) {
			UndoImageTile *tmp_tile = tile->next;
			deallocsize += allocsize * ((tile->use_float) ? sizeof(float) : sizeof(char));
			MEM_freeN(tile->rect.pt);
			BLI_freelinkN (lb, tile);
			tile = tmp_tile;
		}
		else {
			tile = tile->next;
		}
	}

	/* don't forget to remove the size of deallocated tiles */
	undo_paint_push_count_alloc(UNDO_PAINT_IMAGE, -deallocsize);

	ED_undo_paint_push_end(UNDO_PAINT_IMAGE);
}

static void image_undo_invalidate(void)
{
	UndoImageTile *tile;
	ListBase *lb = undo_paint_push_get_list(UNDO_PAINT_IMAGE);

	for (tile = lb->first; tile; tile = tile->next)
		tile->valid = false;
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
			image_undo_push_tile(ima, ibuf, &tmpibuf, tx, ty, NULL, NULL, false);

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

/* paint blur kernels. Projective painting enforces use of a 2x2 kernel due to lagging */
BlurKernel *paint_new_blur_kernel(Brush *br, bool proj)
{
	int i, j;
	BlurKernel *kernel = MEM_mallocN(sizeof(BlurKernel), "blur kernel");
	float radius;
	int side;
	BlurKernelType type = br->blur_mode;

	if (proj) {
		radius = 0.5f;
		
		side = kernel->side = 2;
		kernel->side_squared = kernel->side * kernel->side;
		kernel->wdata = MEM_mallocN(sizeof(float) * kernel->side_squared, "blur kernel data");
		kernel->pixel_len = radius;
	}
	else {
		radius = br->blur_kernel_radius;
		
		side = kernel->side = radius * 2 + 1;
		kernel->side_squared = kernel->side * kernel->side;
		kernel->wdata = MEM_mallocN(sizeof(float) * kernel->side_squared, "blur kernel data");
		kernel->pixel_len = br->blur_kernel_radius;
	}
	
	switch (type) {
		case KERNEL_BOX:
			for (i = 0; i < kernel->side_squared; i++)
				kernel->wdata[i] = 1.0;
			break;

		case KERNEL_GAUSSIAN:
		{
			/* at standard deviation of 3.0 kernel is at about zero */			
			float standard_dev = radius / 3.0f; 
			
			/* make the necessary adjustment to the value for use in the normal distribution formula */
			standard_dev = standard_dev * standard_dev * 2;

			for (i = 0; i < side; i++) {
				for (j = 0; j < side; j++) {
					float idist = radius - i;
					float jdist = radius - j;
					float value = exp((idist * idist + jdist * jdist) / standard_dev);
					
					kernel->wdata[i + j * side] = value;
				}
			}

			break;
		}

		default:
			printf("unidentified kernel type, aborting\n");
			MEM_freeN(kernel->wdata);
			MEM_freeN(kernel);
			return NULL;
			break;
	}

	return kernel;
}

void paint_delete_blur_kernel(BlurKernel *kernel)
{
	if (kernel->wdata)
		MEM_freeN(kernel->wdata);
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
	float startmouse[2];
	double starttime;

	void *cursor;
	ViewContext vc;
} PaintOperation;

bool paint_use_opacity_masking(Brush *brush)
{
	return (brush->flag & BRUSH_AIRBRUSH) ||
	       (brush->flag & BRUSH_DRAG_DOT) ||
	       (brush->flag & BRUSH_ANCHORED) ||
	       (brush->imagepaint_tool == PAINT_TOOL_SMEAR) ||
	       (brush->imagepaint_tool == PAINT_TOOL_SOFTEN) ||
	       (brush->imagepaint_tool == PAINT_TOOL_FILL) ||
	       (brush->flag & BRUSH_USE_GRADIENT) ||
		   (brush->mtex.tex && !ELEM(brush->mtex.brush_map_mode, MTEX_MAP_MODE_TILED, MTEX_MAP_MODE_STENCIL, MTEX_MAP_MODE_3D)) ?
	            false : true;
}

void paint_brush_color_get(struct Scene *scene, struct Brush *br, bool color_correction, bool invert, float distance,
                           float pressure, float color[3], struct ColorManagedDisplay *display)
{
	if (invert)
		copy_v3_v3(color, BKE_brush_secondary_color_get(scene, br));
	else {
		if (br->flag & BRUSH_USE_GRADIENT) {
			switch (br->gradient_stroke_mode) {
				case BRUSH_GRADIENT_PRESSURE:
					do_colorband(br->gradient, pressure, color);
					break;
				case BRUSH_GRADIENT_SPACING_REPEAT:
				{
					float coord = fmod(distance / br->gradient_spacing, 1.0);
					do_colorband(br->gradient, coord, color);
					break;
				}
				case BRUSH_GRADIENT_SPACING_CLAMP:
				{
					do_colorband(br->gradient, distance / br->gradient_spacing, color);
					break;
				}
			}
		}
		else
			copy_v3_v3(color, BKE_brush_color_get(scene, br));
	}
	if (color_correction)
		IMB_colormanagement_display_to_scene_linear_v3(color, display);
}

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

static void gradient_draw_line(bContext *UNUSED(C), int x, int y, void *customdata)
{
	PaintOperation *pop = (PaintOperation *)customdata;

	if (pop) {
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_BLEND);

		glLineWidth(4.0);
		glColor4ub(0, 0, 0, 255);
		sdrawline(x, y, pop->startmouse[0], pop->startmouse[1]);
		glLineWidth(2.0);
		glColor4ub(255, 255, 255, 255);
		sdrawline(x, y, pop->startmouse[0], pop->startmouse[1]);
		glLineWidth(1.0);

		glDisable(GL_BLEND);
		glDisable(GL_LINE_SMOOTH);
	}
}


static PaintOperation *texture_paint_init(bContext *C, wmOperator *op, const float mouse[2])
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *settings = scene->toolsettings;
	PaintOperation *pop = MEM_callocN(sizeof(PaintOperation), "PaintOperation"); /* caller frees */
	Brush *brush = BKE_paint_brush(&settings->imapaint.paint);
	int mode = RNA_enum_get(op->ptr, "mode");
	view3d_set_viewcontext(C, &pop->vc);

	copy_v2_v2(pop->prevmouse, mouse);
	copy_v2_v2(pop->startmouse, mouse);

	if ((brush->imagepaint_tool == PAINT_TOOL_FILL) && (brush->flag & BRUSH_USE_GRADIENT)) {
		pop->cursor = WM_paint_cursor_activate(CTX_wm_manager(C), image_paint_poll, gradient_draw_line, pop);
	}

	/* initialize from context */
	if (CTX_wm_region_view3d(C)) {
		Object *ob = OBACT;
		paint_proj_mesh_data_ensure(C, ob, op);
		pop->mode = PAINT_MODE_3D_PROJECT;
		pop->custom_paint = paint_proj_new_stroke(C, ob, mouse, mode);
	}
	else {
		pop->mode = PAINT_MODE_2D;
		pop->custom_paint = paint_2d_new_stroke(C, op, mode);
	}

	if (!pop->custom_paint) {
		MEM_freeN(pop);
		return NULL;
	}

	settings->imapaint.flag |= IMAGEPAINT_DRAWING;
	ED_undo_paint_push_begin(UNDO_PAINT_IMAGE, op->type->name,
	                         ED_image_undo_restore, ED_image_undo_free, NULL);

	return pop;
}

/* restore painting image to previous state. Used for anchored and drag-dot style brushes*/
static void paint_stroke_restore(void)
{
	ListBase *lb = undo_paint_push_get_list(UNDO_PAINT_IMAGE);
	image_undo_restore_runtime(lb);
	image_undo_invalidate();
}

static void paint_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr)
{
	PaintOperation *pop = paint_stroke_mode_data(stroke);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *toolsettings = CTX_data_tool_settings(C);
	UnifiedPaintSettings *ups = &toolsettings->unified_paint_settings;
	Brush *brush = BKE_paint_brush(&toolsettings->imapaint.paint);

	float alphafac = (brush->flag & BRUSH_ACCUMULATE) ? ups->overlap_factor : 1.0f;

	/* initial brush values. Maybe it should be considered moving these to stroke system */
	float startalpha = BKE_brush_alpha_get(scene, brush);

	float mouse[2];
	float pressure;
	float size;
	float distance = paint_stroke_distance_get(stroke);
	int eraser;

	RNA_float_get_array(itemptr, "mouse", mouse);
	pressure = RNA_float_get(itemptr, "pressure");
	eraser = RNA_boolean_get(itemptr, "pen_flip");
	size = max_ff(1.0f, RNA_float_get(itemptr, "size"));

	/* stroking with fill tool only acts on stroke end */
	if (brush->imagepaint_tool == PAINT_TOOL_FILL) {
		copy_v2_v2(pop->prevmouse, mouse);
		return;
	}

	if (BKE_brush_use_alpha_pressure(scene, brush))
		BKE_brush_alpha_set(scene, brush, max_ff(0.0f, startalpha * pressure * alphafac));
	else
		BKE_brush_alpha_set(scene, brush, max_ff(0.0f, startalpha * alphafac));

	if ((brush->flag & BRUSH_DRAG_DOT) || (brush->flag & BRUSH_ANCHORED)) {
		paint_stroke_restore();
	}

	if (pop->mode == PAINT_MODE_3D_PROJECT) {
		paint_proj_stroke(C, pop->custom_paint, pop->prevmouse, mouse, eraser, pressure, distance, size);
	}
	else {
		paint_2d_stroke(pop->custom_paint, pop->prevmouse, mouse, eraser, pressure, distance, size);
	}

	copy_v2_v2(pop->prevmouse, mouse);

	/* restore brush values */
	BKE_brush_alpha_set(scene, brush, startalpha);
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
	ToolSettings *toolsettings = scene->toolsettings;
	PaintOperation *pop = paint_stroke_mode_data(stroke);
	Brush *brush = BKE_paint_brush(&toolsettings->imapaint.paint);

	toolsettings->imapaint.flag &= ~IMAGEPAINT_DRAWING;

	if (brush->imagepaint_tool == PAINT_TOOL_FILL) {
		if (brush->flag & BRUSH_USE_GRADIENT) {
			if (pop->mode == PAINT_MODE_2D) {
				paint_2d_gradient_fill(C, brush, pop->startmouse, pop->prevmouse, pop->custom_paint);
			}
			else {
				paint_proj_stroke(C, pop->custom_paint, pop->startmouse, pop->prevmouse, paint_stroke_flipped(stroke),
				                  1.0, 0.0, BKE_brush_size_get(scene, brush));
				/* two redraws, one for GPU update, one for notification */
				paint_proj_redraw(C, pop->custom_paint, false);
				paint_proj_redraw(C, pop->custom_paint, true);
			}
		}
		else {
			if (pop->mode == PAINT_MODE_2D) {
				float color[3];

				srgb_to_linearrgb_v3_v3(color, BKE_brush_color_get(scene, brush));
				paint_2d_bucket_fill(C, color, brush, pop->prevmouse, pop->custom_paint);
			}
			else {
				paint_proj_stroke(C, pop->custom_paint, pop->startmouse, pop->prevmouse, paint_stroke_flipped(stroke),
				                  1.0, 0.0, BKE_brush_size_get(scene, brush));
				/* two redraws, one for GPU update, one for notification */
				paint_proj_redraw(C, pop->custom_paint, false);
				paint_proj_redraw(C, pop->custom_paint, true);
			}
		}
	}
	if (pop->mode == PAINT_MODE_3D_PROJECT) {
		paint_proj_stroke_done(pop->custom_paint);
	}
	else {
		paint_2d_stroke_done(pop->custom_paint);
	}

	if (pop->cursor) {
		WM_paint_cursor_end(CTX_wm_manager(C), pop->cursor);
	}

	image_undo_end();

	/* duplicate warning, see texpaint_init */
#if 0
	if (pop->s.warnmultifile)
		BKE_reportf(op->reports, RPT_WARNING, "Image requires 4 color channels to paint: %s", pop->s.warnmultifile);
	if (pop->s.warnpackedfile)
		BKE_reportf(op->reports, RPT_WARNING, "Packed MultiLayer files cannot be painted: %s", pop->s.warnpackedfile);
#endif
	MEM_freeN(pop);
}

static bool paint_stroke_test_start(bContext *C, wmOperator *op, const float mouse[2])
{
	PaintOperation *pop;

	/* TODO Should avoid putting this here. Instead, last position should be requested
	 * from stroke system. */

	if (!(pop = texture_paint_init(C, op, mouse))) {
		return false;
	}

	paint_stroke_set_mode_data(op->customdata, pop);

	return true;
}


static int paint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	int retval;

	op->customdata = paint_stroke_new(C, op, NULL, paint_stroke_test_start,
	                                  paint_stroke_update_step,
	                                  paint_stroke_redraw,
	                                  paint_stroke_done, event->type);

	if ((retval = op->type->modal(C, op, event)) == OPERATOR_FINISHED) {
		paint_stroke_data_free(op);
		return OPERATOR_FINISHED;
	}
	/* add modal handler */
	WM_event_add_modal_handler(C, op);

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

	op->customdata = paint_stroke_new(C, op, NULL, paint_stroke_test_start,
	                                  paint_stroke_update_step,
	                                  paint_stroke_redraw,
	                                  paint_stroke_done, 0);
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
	ot->flag = OPTYPE_BLOCKING;

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

		if (sima->mode == SI_MODE_PAINT) {
			ARegion *ar = CTX_wm_region(C);
			ED_space_image_get_zoom(sima, ar, zoomx, zoomy);

			return 1;
		}
	}

	*zoomx = *zoomy = 1;

	return 0;
}

/************************ cursor drawing *******************************/

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
	bool enabled = false;

	for (win = wm->windows.first; win; win = win->next)
		for (sa = win->screen->areabase.first; sa; sa = sa->next)
			if (sa->spacetype == SPACE_IMAGE)
				if (((SpaceImage *)sa->spacedata.first)->mode == SI_MODE_PAINT)
					enabled = true;

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
	float initcolor[3];
	bool sample_palette;
}	SampleColorData;


static void sample_color_update_header(SampleColorData *data, bContext *C)
{
#define HEADER_LENGTH 150
	char msg[HEADER_LENGTH];
	ScrArea *sa = CTX_wm_area(C);

	if (sa) {
		BLI_snprintf(msg, HEADER_LENGTH,
		             "Sample color for %s",
		             !data->sample_palette ?
		             "Brush. Use Left Click to sample for palette instead" :
		             "Palette. Use Left Click to sample more colors");
		ED_area_headerprint(sa, msg);
	}

#undef HEADER_LENGTH
}

static int sample_color_exec(bContext *C, wmOperator *op)
{
	Paint *paint = BKE_paint_get_active_from_context(C);
	Brush *brush = BKE_paint_brush(paint);
	PaintMode mode = BKE_paintmode_get_active_from_context(C);
	ARegion *ar = CTX_wm_region(C);
	wmWindow *win = CTX_wm_window(C);
	bool show_cursor = ((paint->flags & PAINT_SHOW_BRUSH) != 0);
	int location[2];
	bool use_palette;
	paint->flags &= ~PAINT_SHOW_BRUSH;

	/* force redraw without cursor */
	WM_paint_cursor_tag_redraw(win, ar);
	WM_redraw_windows(C);

	RNA_int_get_array(op->ptr, "location", location);
	use_palette = RNA_boolean_get(op->ptr, "palette");

	paint_sample_color(C, ar, location[0], location[1], mode == PAINT_TEXTURE_PROJECTIVE, use_palette);

	if (show_cursor) {
		paint->flags |= PAINT_SHOW_BRUSH;
	}

	WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);
	
	return OPERATOR_FINISHED;
}

static int sample_color_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Scene *scene = CTX_data_scene(C);
	Paint *paint = BKE_paint_get_active_from_context(C);
	PaintMode mode = BKE_paintmode_get_active_from_context(C);
	Brush *brush = BKE_paint_brush(paint);
	SampleColorData *data = MEM_mallocN(sizeof(SampleColorData), "sample color custom data");
	ARegion *ar = CTX_wm_region(C);
	wmWindow *win = CTX_wm_window(C);

	data->event_type = event->type;
	data->show_cursor = ((paint->flags & PAINT_SHOW_BRUSH) != 0);
	copy_v3_v3(data->initcolor, BKE_brush_color_get(scene, brush));
	data->sample_palette = false;
	op->customdata = data;
	paint->flags &= ~PAINT_SHOW_BRUSH;

	sample_color_update_header(data, C);

	WM_event_add_modal_handler(C, op);

	/* force redraw without cursor */
	WM_paint_cursor_tag_redraw(win, ar);
	WM_redraw_windows(C);

	RNA_int_set_array(op->ptr, "location", event->mval);

	paint_sample_color(C, ar, event->mval[0], event->mval[1], mode == PAINT_TEXTURE_PROJECTIVE, false);
	WM_cursor_modal_set(win, BC_EYEDROPPER_CURSOR);

	WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);

	return OPERATOR_RUNNING_MODAL;
}

static int sample_color_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	Scene *scene = CTX_data_scene(C);
	SampleColorData *data = op->customdata;
	Paint *paint = BKE_paint_get_active_from_context(C);
	Brush *brush = BKE_paint_brush(paint);
	PaintMode mode = BKE_paintmode_get_active_from_context(C);

	if ((event->type == data->event_type) && (event->val == KM_RELEASE)) {
		ScrArea *sa = CTX_wm_area(C);

		if (data->show_cursor) {
			paint->flags |= PAINT_SHOW_BRUSH;
		}

		if (data->sample_palette) {
			BKE_brush_color_set(scene, brush, data->initcolor);
			RNA_boolean_set(op->ptr, "palette", true);
		}
		WM_cursor_modal_restore(CTX_wm_window(C));
		MEM_freeN(data);
		ED_area_headerprint(sa, NULL);

		return OPERATOR_FINISHED;
	}

	switch (event->type) {
		case MOUSEMOVE:
		{
			ARegion *ar = CTX_wm_region(C);
			RNA_int_set_array(op->ptr, "location", event->mval);
			paint_sample_color(C, ar, event->mval[0], event->mval[1], mode == PAINT_TEXTURE_PROJECTIVE, false);
			WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);
			break;
		}

		case LEFTMOUSE:
			if (event->val == KM_PRESS) {
				ARegion *ar = CTX_wm_region(C);
				RNA_int_set_array(op->ptr, "location", event->mval);
				paint_sample_color(C, ar, event->mval[0], event->mval[1], mode == PAINT_TEXTURE_PROJECTIVE, true);
				if (!data->sample_palette) {
					data->sample_palette = true;
					sample_color_update_header(data, C);
				}
			}
			break;
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
	RNA_def_boolean(ot->srna, "palette", 0, "Palette", "Add color to palette");
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


/* Make sure that active object has a material, and assign UVs and image layers if they do not exist */
void paint_proj_mesh_data_ensure(bContext *C, Object *ob, wmOperator *op)
{
	Mesh *me;
	int layernum;
	ImagePaintSettings *imapaint = &(CTX_data_tool_settings(C)->imapaint);
	bScreen *sc;
	Scene *scene = CTX_data_scene(C);
	Main *bmain = CTX_data_main(C);
	Brush *br = BKE_paint_brush(&imapaint->paint);

	if (imapaint->mode == IMAGEPAINT_MODE_MATERIAL) {
		/* no material, add one */
		if (ob->totcol == 0) {
			Material *ma = BKE_material_add(CTX_data_main(C), "Material");
			/* no material found, just assign to first slot */
			assign_material(ob, ma, 1, BKE_MAT_ASSIGN_USERPREF);
			proj_paint_add_slot(C, ma, NULL);
		}
		else {
			/* there may be material slots but they may be empty, check */
			int i;
			
			for (i = 1; i < ob->totcol + 1; i++) {
				Material *ma = give_current_material(ob, i);
				if (ma) {
					if (!ma->texpaintslot) {
						/* refresh here just in case */
						BKE_texpaint_slot_refresh_cache(scene, ma);				
						
						/* if still no slots, we have to add */
						if (!ma->texpaintslot) {
							proj_paint_add_slot(C, ma, NULL);
							
							if (ma->texpaintslot) {
								for (sc = bmain->screen.first; sc; sc = sc->id.next) {
									ScrArea *sa;
									for (sa = sc->areabase.first; sa; sa = sa->next) {
										SpaceLink *sl;
										for (sl = sa->spacedata.first; sl; sl = sl->next) {
											if (sl->spacetype == SPACE_IMAGE) {
												SpaceImage *sima = (SpaceImage *)sl;
												
												ED_space_image_set(sima, scene, scene->obedit, ma->texpaintslot[0].ima);
											}
										}
									}
								}
							}							
						}
					}
				}
				else {
					Material *ma = BKE_material_add(CTX_data_main(C), "Material");
					/* no material found, just assign to first slot */
					assign_material(ob, ma, i, BKE_MAT_ASSIGN_USERPREF);
					proj_paint_add_slot(C, ma, NULL);
				}
			}
		}
	}
	else if (imapaint->mode == IMAGEPAINT_MODE_IMAGE) {
		if (imapaint->canvas == NULL) {
			int width;
			int height;
			Main *bmain = CTX_data_main(C);
			float color[4] = {0.0, 0.0, 0.0, 1.0};

			width = 1024;
			height = 1024;
			imapaint->canvas = BKE_image_add_generated(bmain, width, height, "Canvas", 32, false, IMA_GENTYPE_BLANK, color);
			
			for (sc = bmain->screen.first; sc; sc = sc->id.next) {
				ScrArea *sa;
				for (sa = sc->areabase.first; sa; sa = sa->next) {
					SpaceLink *sl;
					for (sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_IMAGE) {
							SpaceImage *sima = (SpaceImage *)sl;
							
							ED_space_image_set(sima, scene, scene->obedit, imapaint->canvas);
						}
					}
				}
			}			
		}		
	}
		
	me = BKE_mesh_from_object(ob);
	layernum = CustomData_number_of_layers(&me->pdata, CD_MTEXPOLY);

	if (layernum == 0) {
		BKE_reportf(op->reports, RPT_WARNING, "Object did not have UV map, manual unwrap recommended");

		ED_mesh_uv_texture_add(me, "UVMap", true);
	}

	/* Make sure we have a stencil to paint on! */
	if (br && br->imagepaint_tool == PAINT_TOOL_MASK) {
		imapaint->flag |= IMAGEPAINT_PROJECT_LAYER_STENCIL;

		if (imapaint->stencil == NULL) {
			int width;
			int height;
			Main *bmain = CTX_data_main(C);
			float color[4] = {0.0, 0.0, 0.0, 1.0};

			width = 1024;
			height = 1024;
			imapaint->stencil = BKE_image_add_generated(bmain, width, height, "Stencil", 32, false, IMA_GENTYPE_BLANK, color);
		}
	}
}

static int texture_paint_toggle_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	const int mode_flag = OB_MODE_TEXTURE_PAINT;
	const bool is_mode_set = (ob->mode & mode_flag) != 0;

	if (!is_mode_set) {
		if (!ED_object_mode_compat_set(C, ob, mode_flag, op->reports)) {
			return OPERATOR_CANCELLED;
		}
	}

	if (ob->mode & mode_flag) {
		ob->mode &= ~mode_flag;

		if (U.glreslimit != 0)
			GPU_free_images();
		GPU_paint_set_mipmap(1);

		toggle_paint_cursor(C, 0);
	}
	else {
		bScreen *sc;
		Main *bmain = CTX_data_main(C);
		Image *ima = NULL;
		ImagePaintSettings *imapaint = &scene->toolsettings->imapaint;

		/* This has to stay here to regenerate the texture paint
		 * cache in case we are loading a file */
		BKE_texpaint_slots_refresh_object(scene, ob);

		paint_proj_mesh_data_ensure(C, ob, op);

		/* entering paint mode also sets image to editors */
		if (imapaint->mode == IMAGEPAINT_MODE_MATERIAL) {
			Material *ma = give_current_material(ob, ob->actcol); /* set the current material active paint slot on image editor */

			if (ma->texpaintslot)
				ima = ma->texpaintslot[ma->paint_active_slot].ima;
		}
		else if (imapaint->mode == IMAGEPAINT_MODE_MATERIAL) {
			ima = imapaint->canvas;
		}	
		
		for (sc = bmain->screen.first; sc; sc = sc->id.next) {
			ScrArea *sa;
			for (sa = sc->areabase.first; sa; sa = sa->next) {
				SpaceLink *sl;
				for (sl = sa->spacedata.first; sl; sl = sl->next) {
					if (sl->spacetype == SPACE_IMAGE) {
						SpaceImage *sima = (SpaceImage *)sl;
						
						ED_space_image_set(sima, scene, scene->obedit, ima);
					}
				}
			}
		}
		
		ob->mode |= mode_flag;

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


static int brush_colors_flip_exec(bContext *C, wmOperator *UNUSED(op))
{
	UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
	Brush *br = image_paint_brush(C);
	if (ups->flag & UNIFIED_PAINT_COLOR) {
		swap_v3_v3(ups->rgb, ups->secondary_rgb);
	}
	else if (br) {
		swap_v3_v3(br->rgb, br->secondary_rgb);
	}
	WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, br);

	return OPERATOR_FINISHED;
}

static int brush_colors_flip_poll(bContext *C)
{
	if (image_paint_poll(C)) {
		Brush *br = image_paint_brush(C);
		if (br->imagepaint_tool == PAINT_TOOL_DRAW)
			return 1;
	}

	return 0;
}

void PAINT_OT_brush_colors_flip(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Brush Colors Flip";
	ot->idname = "PAINT_OT_brush_colors_flip";
	ot->description = "Toggle foreground and background brush colors";

	/* api callbacks */
	ot->exec = brush_colors_flip_exec;
	ot->poll = brush_colors_flip_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


void ED_imapaint_bucket_fill(struct bContext *C, float color[3], wmOperator *op)
{
	ED_undo_paint_push_begin(UNDO_PAINT_IMAGE, op->type->name,
	                      ED_image_undo_restore, ED_image_undo_free, NULL);

	paint_2d_bucket_fill(C, color, NULL, NULL, NULL);

	ED_undo_paint_push_end(UNDO_PAINT_IMAGE);
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

