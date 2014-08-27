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
 * The Original Code is Copyright (C) 2004 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_bake_api.c
 *  \ingroup edobj
 */


#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_material_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_fileops.h"
#include "BLI_math_geom.h"
#include "BLI_path_util.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_modifier.h"
#include "BKE_mesh.h"
#include "BKE_screen.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_colormanagement.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_uvedit.h"

#include "GPU_draw.h"

#include "object_intern.h"


typedef struct BakeAPIRender {
	Object *ob;
	Main *main;
	Scene *scene;
	ReportList *reports;
	ListBase selected_objects;

	ScenePassType pass_type;
	int margin;

	int save_mode;

	bool is_clear;
	bool is_split_materials;
	bool is_automatic_name;
	bool is_selected_to_active;
	bool is_cage;

	float cage_extrusion;
	int normal_space;
	BakeNormalSwizzle normal_swizzle[3];

	char uv_layer[MAX_CUSTOMDATA_LAYER_NAME];
	char custom_cage[MAX_NAME];
	char filepath[FILE_MAX];

	int width;
	int height;
	const char *identifier;

	int result;
	bool ready;

	/* callbacks */
	Render *render;
	float *progress;
	short *do_update;

	/* for redrawing */
	ScrArea *sa;
} BakeAPIRender;

/* callbacks */

static void bake_progress_update(void *bjv, float progress)
{
	BakeAPIRender *bj = bjv;

	if (bj->progress && *bj->progress != progress) {
		*bj->progress = progress;

		/* make jobs timer to send notifier */
		*(bj->do_update) = true;
	}
}

/* catch esc */
static int bake_modal(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	/* no running blender, remove handler and pass through */
	if (0 == WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C), WM_JOB_TYPE_OBJECT_BAKE))
		return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;

	/* running render */
	switch (event->type) {
		case ESCKEY:
		{
			G.is_break = true;
			return OPERATOR_RUNNING_MODAL;
		}
	}
	return OPERATOR_PASS_THROUGH;
}

/* for exec() when there is no render job
 * note: this wont check for the escape key being pressed, but doing so isnt threadsafe */
static int bake_break(void *UNUSED(rjv))
{
	if (G.is_break)
		return 1;
	return 0;
}


static void bake_update_image(ScrArea *sa, Image *image)
{
	if (sa && sa->spacetype == SPACE_IMAGE) { /* in case the user changed while baking */
		SpaceImage *sima = sa->spacedata.first;
		if (sima)
			sima->image = image;
	}
}

static bool write_internal_bake_pixels(
        Image *image, BakePixel pixel_array[], float *buffer,
        const int width, const int height, const int margin,
        const bool is_clear, const bool is_noncolor)
{
	ImBuf *ibuf;
	void *lock;
	bool is_float;
	char *mask_buffer = NULL;
	const size_t num_pixels = (size_t)width * (size_t)height;

	ibuf = BKE_image_acquire_ibuf(image, NULL, &lock);

	if (!ibuf)
		return false;

	if (margin > 0 || !is_clear) {
		mask_buffer = MEM_callocN(sizeof(char) * num_pixels, "Bake Mask");
		RE_bake_mask_fill(pixel_array, num_pixels, mask_buffer);
	}

	is_float = (ibuf->flags & IB_rectfloat);

	/* colormanagement conversions */
	if (!is_noncolor) {
		const char *from_colorspace;
		const char *to_colorspace;

		from_colorspace = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_SCENE_LINEAR);

		if (is_float)
			to_colorspace = IMB_colormanagement_get_float_colorspace(ibuf);
		else
			to_colorspace = IMB_colormanagement_get_rect_colorspace(ibuf);

		if (from_colorspace != to_colorspace)
			IMB_colormanagement_transform(buffer, ibuf->x, ibuf->y, ibuf->channels, from_colorspace, to_colorspace, false);
	}

	/* populates the ImBuf */
	if (is_clear) {
		if (is_float) {
			IMB_buffer_float_from_float(
			        ibuf->rect_float, buffer, ibuf->channels,
			        IB_PROFILE_LINEAR_RGB, IB_PROFILE_LINEAR_RGB, false,
			        ibuf->x, ibuf->y, ibuf->x, ibuf->x);
		}
		else {
			IMB_buffer_byte_from_float(
			        (unsigned char *) ibuf->rect, buffer, ibuf->channels, ibuf->dither,
			        IB_PROFILE_SRGB, IB_PROFILE_SRGB,
			        false, ibuf->x, ibuf->y, ibuf->x, ibuf->x);
		}
	}
	else {
		if (is_float) {
			IMB_buffer_float_from_float_mask(
			        ibuf->rect_float, buffer, ibuf->channels,
			        ibuf->x, ibuf->y, ibuf->x, ibuf->x, mask_buffer);
		}
		else {
			IMB_buffer_byte_from_float_mask(
			        (unsigned char *) ibuf->rect, buffer, ibuf->channels, ibuf->dither,
			        false, ibuf->x, ibuf->y, ibuf->x, ibuf->x, mask_buffer);
		}
	}

	/* margins */
	if (margin > 0)
		RE_bake_margin(ibuf, mask_buffer, margin);

	ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID | IB_BITMAPDIRTY;

	if (ibuf->rect_float)
		ibuf->userflags |= IB_RECT_INVALID;

	/* force mipmap recalc */
	if (ibuf->mipmap[0]) {
		ibuf->userflags |= IB_MIPMAP_INVALID;
		imb_freemipmapImBuf(ibuf);
	}

	BKE_image_release_ibuf(image, ibuf, NULL);

	if (mask_buffer)
		MEM_freeN(mask_buffer);

	return true;
}

/* force OpenGL reload */
static void reset_images_gpu(BakeImages *bake_images)
{
	int i;
	for (i = 0; i < bake_images->size; i++) {
		Image *ima = bake_images->data[i].image;
		if (ima->ok == IMA_OK_LOADED) {
			GPU_free_image(ima);
		}
	}
}

static bool write_external_bake_pixels(
        const char *filepath, BakePixel pixel_array[], float *buffer,
        const int width, const int height, const int margin,
        ImageFormatData *im_format, const bool is_noncolor)
{
	ImBuf *ibuf = NULL;
	bool ok = false;
	bool is_float;

	is_float = im_format->depth > 8;

	/* create a new ImBuf */
	ibuf = IMB_allocImBuf(width, height, im_format->planes, (is_float ? IB_rectfloat : IB_rect));

	if (!ibuf)
		return false;

	/* populates the ImBuf */
	if (is_float) {
		IMB_buffer_float_from_float(
		        ibuf->rect_float, buffer, ibuf->channels,
		        IB_PROFILE_LINEAR_RGB, IB_PROFILE_LINEAR_RGB, false,
		        ibuf->x, ibuf->y, ibuf->x, ibuf->x);
	}
	else {
		if (!is_noncolor) {
			const char *from_colorspace = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_SCENE_LINEAR);
			const char *to_colorspace = IMB_colormanagement_get_rect_colorspace(ibuf);
			IMB_colormanagement_transform(buffer, ibuf->x, ibuf->y, ibuf->channels, from_colorspace, to_colorspace, false);
		}

		IMB_buffer_byte_from_float(
		        (unsigned char *) ibuf->rect, buffer, ibuf->channels, ibuf->dither,
		        IB_PROFILE_SRGB, IB_PROFILE_SRGB,
		        false, ibuf->x, ibuf->y, ibuf->x, ibuf->x);
	}

	/* margins */
	if (margin > 0) {
		char *mask_buffer = NULL;
		const size_t num_pixels = (size_t)width * (size_t)height;

		mask_buffer = MEM_callocN(sizeof(char) * num_pixels, "Bake Mask");
		RE_bake_mask_fill(pixel_array, num_pixels, mask_buffer);
		RE_bake_margin(ibuf, mask_buffer, margin);

		if (mask_buffer)
			MEM_freeN(mask_buffer);
	}

	if ((ok = BKE_imbuf_write(ibuf, filepath, im_format))) {
#ifndef WIN32
		chmod(filepath, S_IRUSR | S_IWUSR);
#endif
		//printf("%s saving bake map: '%s'\n", __func__, filepath);
	}

	/* garbage collection */
	IMB_freeImBuf(ibuf);

	return ok;
}

static bool is_noncolor_pass(ScenePassType pass_type)
{
	return ELEM(pass_type,
	            SCE_PASS_Z,
	            SCE_PASS_NORMAL,
	            SCE_PASS_VECTOR,
	            SCE_PASS_INDEXOB,
	            SCE_PASS_UV,
	            SCE_PASS_RAYHITS,
	            SCE_PASS_INDEXMA);
}

/* if all is good tag image and return true */
static bool bake_object_check(Object *ob, ReportList *reports)
{
	Image *image;
	void *lock;
	int i;

	if (ob->type != OB_MESH) {
		BKE_reportf(reports, RPT_ERROR, "Object \"%s\" is not a mesh", ob->id.name + 2);
		return false;
	}
	else {
		Mesh *me = (Mesh *)ob->data;

		if (CustomData_get_active_layer_index(&me->ldata, CD_MLOOPUV) == -1) {
			BKE_reportf(reports, RPT_ERROR,
			            "No active UV layer found in the object \"%s\"", ob->id.name + 2);
			return false;
		}
	}

	for (i = 0; i < ob->totcol; i++) {
		bNodeTree *ntree = NULL;
		bNode *node = NULL;
		ED_object_get_active_image(ob, i + 1, &image, NULL, &node, &ntree);

		if (image) {
			ImBuf *ibuf;

			if (node) {
				if (BKE_node_is_connected_to_output(ntree, node)) {
					BKE_reportf(reports, RPT_ERROR,
					            "Circular dependency for image \"%s\" from object \"%s\"",
					            image->id.name + 2, ob->id.name + 2);
				}
			}

			ibuf = BKE_image_acquire_ibuf(image, NULL, &lock);

			if (ibuf) {
				BKE_image_release_ibuf(image, ibuf, lock);
			}
			else {
				BKE_reportf(reports, RPT_ERROR,
				            "Uninitialized image \"%s\" from object \"%s\"",
				            image->id.name + 2, ob->id.name + 2);

				BKE_image_release_ibuf(image, ibuf, lock);
				return false;
			}
		}
		else {
			if (ob->mat[i]) {
				BKE_reportf(reports, RPT_ERROR,
				            "No active image found in material \"%s\" (%d) for object \"%s\"",
				            ob->mat[i]->id.name + 2, i, ob->id.name + 2);
			}
			else if (((Mesh *) ob->data)->mat[i]) {
				BKE_reportf(reports, RPT_ERROR,
				            "No active image found in material \"%s\" (%d) for object \"%s\"",
				            ((Mesh *) ob->data)->mat[i]->id.name + 2, i, ob->id.name + 2);
			}
			else {
				BKE_reportf(reports, RPT_ERROR,
				            "No active image found in material (%d) for object \"%s\"",
				            i, ob->id.name + 2);
			}
			return false;
		}

		image->id.flag |= LIB_DOIT;
	}
	return true;
}

/* before even getting in the bake function we check for some basic errors */
static bool bake_objects_check(Main *bmain, Object *ob, ListBase *selected_objects,
                               ReportList *reports, const bool is_selected_to_active)
{
	CollectionPointerLink *link;

	/* error handling and tag (in case multiple materials share the same image) */
	BKE_main_id_tag_idcode(bmain, ID_IM, false);

	if (is_selected_to_active) {
		int tot_objects = 0;

		if (!bake_object_check(ob, reports))
			return false;

		for (link = selected_objects->first; link; link = link->next) {
			Object *ob_iter = (Object *)link->ptr.data;

			if (ob_iter == ob)
				continue;

			if (ELEM(ob_iter->type, OB_MESH, OB_FONT, OB_CURVE, OB_SURF, OB_MBALL) == false) {
				BKE_reportf(reports, RPT_ERROR, "Object \"%s\" is not a mesh or can't be converted to a mesh (Curve, Text, Surface or Metaball)", ob_iter->id.name + 2);
				return false;
			}
			tot_objects += 1;
		}

		if (tot_objects == 0) {
			BKE_report(reports, RPT_ERROR, "No valid selected objects");
			return false;
		}
	}
	else {
		if (BLI_listbase_is_empty(selected_objects)) {
			BKE_report(reports, RPT_ERROR, "No valid selected objects");
			return false;
		}

		for (link = selected_objects->first; link; link = link->next) {
			if (!bake_object_check(link->ptr.data, reports))
				return false;
		}
	}
	return true;
}

/* it needs to be called after bake_objects_check since the image tagging happens there */
static void bake_images_clear(Main *bmain, const bool is_tangent)
{
	Image *image;
	for (image = bmain->image.first; image; image = image->id.next) {
		if ((image->id.flag & LIB_DOIT) != 0) {
			RE_bake_ibuf_clear(image, is_tangent);
		}
	}
}

static void build_image_lookup(Main *bmain, Object *ob, BakeImages *bake_images)
{
	const int tot_mat = ob->totcol;
	int i, j;
	int tot_images = 0;

	/* error handling and tag (in case multiple materials share the same image) */
	BKE_main_id_tag_idcode(bmain, ID_IM, false);

	for (i = 0; i < tot_mat; i++) {
		Image *image;
		ED_object_get_active_image(ob, i + 1, &image, NULL, NULL, NULL);

		if ((image->id.flag & LIB_DOIT)) {
			for (j = 0; j < i; j++) {
				if (bake_images->data[j].image == image) {
					bake_images->lookup[i] = j;
					break;
				}
			}
		}
		else {
			bake_images->lookup[i] = tot_images;
			bake_images->data[tot_images].image = image;
			image->id.flag |= LIB_DOIT;
			tot_images++;
		}
	}

	bake_images->size = tot_images;
}

/*
 * returns the total number of pixels
 */
static size_t initialize_internal_images(BakeImages *bake_images, ReportList *reports)
{
	int i;
	size_t tot_size = 0;

	for (i = 0; i < bake_images->size; i++) {
		ImBuf *ibuf;
		void *lock;

		BakeImage *bk_image = &bake_images->data[i];
		ibuf = BKE_image_acquire_ibuf(bk_image->image, NULL, &lock);

		if (ibuf) {
			bk_image->width = ibuf->x;
			bk_image->height = ibuf->y;
			bk_image->offset = tot_size;

			tot_size += (size_t)ibuf->x * (size_t)ibuf->y;
		}
		else {
			BKE_image_release_ibuf(bk_image->image, ibuf, lock);
			BKE_reportf(reports, RPT_ERROR, "Uninitialized image %s", bk_image->image->id.name + 2);
			return 0;
		}
		BKE_image_release_ibuf(bk_image->image, ibuf, lock);
	}
	return tot_size;
}

static int bake(
        Render *re, Main *bmain, Scene *scene, Object *ob_low, ListBase *selected_objects, ReportList *reports,
        const ScenePassType pass_type, const int margin,
        const BakeSaveMode save_mode, const bool is_clear, const bool is_split_materials,
        const bool is_automatic_name, const bool is_selected_to_active, const bool is_cage,
        const float cage_extrusion, const int normal_space, const BakeNormalSwizzle normal_swizzle[],
        const char *custom_cage, const char *filepath, const int width, const int height,
        const char *identifier, ScrArea *sa, const char *uv_layer)
{
	int op_result = OPERATOR_CANCELLED;
	bool ok = false;

	Object *ob_cage = NULL;

	BakeHighPolyData *highpoly = NULL;
	int tot_highpoly;

	char restrict_flag_low = ob_low->restrictflag;
	char restrict_flag_cage;

	Mesh *me_low = NULL;
	Mesh *me_cage = NULL;

	float *result = NULL;

	BakePixel *pixel_array_low = NULL;

	const bool is_save_internal = (save_mode == R_BAKE_SAVE_INTERNAL);
	const bool is_noncolor = is_noncolor_pass(pass_type);
	const int depth = RE_pass_depth(pass_type);

	BakeImages bake_images = {NULL};

	size_t num_pixels;
	int tot_materials;
	int i;

	RE_bake_engine_set_engine_parameters(re, bmain, scene);

	if (!RE_bake_has_engine(re)) {
		BKE_report(reports, RPT_ERROR, "Current render engine does not support baking");
		goto cleanup;
	}

	tot_materials = ob_low->totcol;

	if (uv_layer && uv_layer[0] != '\0') {
		Mesh *me = (Mesh *)ob_low->data;
		if (CustomData_get_named_layer(&me->ldata, CD_MLOOPUV, uv_layer) == -1) {
			BKE_reportf(reports, RPT_ERROR,
			            "No UV layer named \"%s\" found in the object \"%s\"", uv_layer, ob_low->id.name + 2);
			goto cleanup;
		}
	}

	if (tot_materials == 0) {
		if (is_save_internal) {
			BKE_report(reports, RPT_ERROR,
			           "No active image found, add a material or bake to an external file");

			goto cleanup;
		}
		else if (is_split_materials) {
			BKE_report(reports, RPT_ERROR,
			           "No active image found, add a material or bake without the Split Materials option");

			goto cleanup;
		}
		else {
			/* baking externally without splitting materials */
			tot_materials = 1;
		}
	}

	/* we overallocate in case there is more materials than images */
	bake_images.data = MEM_callocN(sizeof(BakeImage) * tot_materials, "bake images dimensions (width, height, offset)");
	bake_images.lookup = MEM_callocN(sizeof(int) * tot_materials, "bake images lookup (from material to BakeImage)");

	build_image_lookup(bmain, ob_low, &bake_images);

	if (is_save_internal) {
		num_pixels = initialize_internal_images(&bake_images, reports);

		if (num_pixels == 0) {
			goto cleanup;
		}
	}
	else {
		/* when saving extenally always use the size specified in the UI */

		num_pixels = (size_t)width * (size_t)height * bake_images.size;

		for (i = 0; i < bake_images.size; i++) {
			bake_images.data[i].width = width;
			bake_images.data[i].height = height;
			bake_images.data[i].offset = (is_split_materials ? num_pixels : 0);
			bake_images.data[i].image = NULL;
		}

		if (!is_split_materials) {
			/* saving a single image */
			for (i = 0; i < tot_materials; i++)
				bake_images.lookup[i] = 0;
		}
	}

	if (is_selected_to_active) {
		CollectionPointerLink *link;
		tot_highpoly = 0;

		for (link = selected_objects->first; link; link = link->next) {
			Object *ob_iter = link->ptr.data;

			if (ob_iter == ob_low)
				continue;

			tot_highpoly ++;
		}

		if (is_cage && custom_cage[0] != '\0') {
			ob_cage = BLI_findstring(&bmain->object, custom_cage, offsetof(ID, name) + 2);

			if (ob_cage == NULL || ob_cage->type != OB_MESH) {
				BKE_report(reports, RPT_ERROR, "No valid cage object");
				goto cleanup;
			}
			else {
				restrict_flag_cage = ob_cage->restrictflag;
				ob_cage->restrictflag |= OB_RESTRICT_RENDER;
			}
		}
	}

	pixel_array_low = MEM_callocN(sizeof(BakePixel) * num_pixels, "bake pixels low poly");
	result = MEM_callocN(sizeof(float) * depth * num_pixels, "bake return pixels");

	/* get the mesh as it arrives in the renderer */
	me_low = BKE_mesh_new_from_object(bmain, scene, ob_low, 1, 2, 1, 0);

	/* populate the pixel array with the face data */
	if ((is_selected_to_active && (ob_cage == NULL) && is_cage) == false)
		RE_bake_pixels_populate(me_low, pixel_array_low, num_pixels, &bake_images, uv_layer);
	/* else populate the pixel array with the 'cage' mesh (the smooth version of the mesh)  */

	if (is_selected_to_active) {
		CollectionPointerLink *link;
		ModifierData *md, *nmd;
		ListBase modifiers_tmp, modifiers_original;
		int i = 0;

		/* prepare cage mesh */
		if (ob_cage) {
			me_cage = BKE_mesh_new_from_object(bmain, scene, ob_cage, 1, 2, 1, 0);
			if (me_low->totface != me_cage->totface) {
				BKE_report(reports, RPT_ERROR,
				           "Invalid cage object, the cage mesh must have the same number "
				           "of faces as the active object");
				goto cleanup;
			}
		}
		else if (is_cage) {
			modifiers_original = ob_low->modifiers;
			BLI_listbase_clear(&modifiers_tmp);

			for (md = ob_low->modifiers.first; md; md = md->next) {
				/* Edge Split cannot be applied in the cage,
				 * the cage is supposed to have interpolated normals
				 * between the faces unless the geometry is physically
				 * split. So we create a copy of the low poly mesh without
				 * the eventual edge split.*/

				if (md->type == eModifierType_EdgeSplit)
					continue;

				nmd = modifier_new(md->type);
				BLI_strncpy(nmd->name, md->name, sizeof(nmd->name));
				modifier_copyData(md, nmd);
				BLI_addtail(&modifiers_tmp, nmd);
			}

			/* temporarily replace the modifiers */
			ob_low->modifiers = modifiers_tmp;

			/* get the cage mesh as it arrives in the renderer */
			me_cage = BKE_mesh_new_from_object(bmain, scene, ob_low, 1, 2, 1, 0);
			RE_bake_pixels_populate(me_cage, pixel_array_low, num_pixels, &bake_images, uv_layer);
		}

		highpoly = MEM_callocN(sizeof(BakeHighPolyData) * tot_highpoly, "bake high poly objects");

		/* populate highpoly array */
		for (link = selected_objects->first; link; link = link->next) {
			TriangulateModifierData *tmd;
			Object *ob_iter = link->ptr.data;

			if (ob_iter == ob_low)
				continue;

			/* initialize highpoly_data */
			highpoly[i].ob = ob_iter;
			highpoly[i].me = NULL;
			highpoly[i].tri_mod = NULL;
			highpoly[i].restrict_flag = ob_iter->restrictflag;
			highpoly[i].pixel_array = MEM_callocN(sizeof(BakePixel) * num_pixels, "bake pixels high poly");


			/* triangulating so BVH returns the primitive_id that will be used for rendering */
			highpoly[i].tri_mod = ED_object_modifier_add(
			        reports, bmain, scene, highpoly[i].ob,
			        "TmpTriangulate", eModifierType_Triangulate);
			tmd = (TriangulateModifierData *)highpoly[i].tri_mod;
			tmd->quad_method = MOD_TRIANGULATE_QUAD_FIXED;
			tmd->ngon_method = MOD_TRIANGULATE_NGON_EARCLIP;

			highpoly[i].me = BKE_mesh_new_from_object(bmain, scene, highpoly[i].ob, 1, 2, 1, 0);
			highpoly[i].ob->restrictflag &= ~OB_RESTRICT_RENDER;

			/* lowpoly to highpoly transformation matrix */
			copy_m4_m4(highpoly[i].obmat, highpoly[i].ob->obmat);
			invert_m4_m4(highpoly[i].imat, highpoly[i].obmat);

			/* rotation */
			normalize_m4_m4(highpoly[i].rotmat, highpoly[i].imat);
			zero_v3(highpoly[i].rotmat[3]);
			if (is_negative_m4(highpoly[i].rotmat))
				negate_m3(highpoly[i].rotmat);

			i++;
		}

		BLI_assert(i == tot_highpoly);

		ob_low->restrictflag |= OB_RESTRICT_RENDER;

		/* populate the pixel arrays with the corresponding face data for each high poly object */
		if (!RE_bake_pixels_populate_from_objects(
		            me_low, pixel_array_low, highpoly, tot_highpoly, num_pixels, ob_cage != NULL,
		            cage_extrusion, ob_low->obmat, (ob_cage ? ob_cage->obmat : ob_low->obmat), me_cage))
		{
			BKE_report(reports, RPT_ERROR, "Error handling selected objects");
			goto cage_cleanup;
		}

		/* the baking itself */
		for (i = 0; i < tot_highpoly; i++) {
			ok = RE_bake_engine(re, highpoly[i].ob, highpoly[i].pixel_array, num_pixels,
			                    depth, pass_type, result);
			if (!ok) {
				BKE_reportf(reports, RPT_ERROR, "Error baking from object \"%s\"", highpoly[i].ob->id.name + 2);
				goto cage_cleanup;
			}
		}

cage_cleanup:
		/* reverting data back */
		if ((ob_cage == NULL) && is_cage) {
			ob_low->modifiers = modifiers_original;

			while ((md = BLI_pophead(&modifiers_tmp))) {
				modifier_free(md);
			}
		}

		if (!ok) {
			goto cleanup;
		}
	}
	else {
		/* make sure low poly renders */
		ob_low->restrictflag &= ~OB_RESTRICT_RENDER;

		if (RE_bake_has_engine(re)) {
			ok = RE_bake_engine(re, ob_low, pixel_array_low, num_pixels, depth, pass_type, result);
		}
		else {
			BKE_report(reports, RPT_ERROR, "Current render engine does not support baking");
			goto cleanup;
		}
	}

	/* normal space conversion
	 * the normals are expected to be in world space, +X +Y +Z */
	if (ok && pass_type == SCE_PASS_NORMAL) {
		switch (normal_space) {
			case R_BAKE_SPACE_WORLD:
			{
				/* Cycles internal format */
				if ((normal_swizzle[0] == R_BAKE_POSX) &&
				    (normal_swizzle[1] == R_BAKE_POSY) &&
				    (normal_swizzle[2] == R_BAKE_POSZ))
				{
					break;
				}
				else {
					RE_bake_normal_world_to_world(pixel_array_low, num_pixels,  depth, result, normal_swizzle);
				}
				break;
			}
			case R_BAKE_SPACE_OBJECT:
			{
				RE_bake_normal_world_to_object(pixel_array_low, num_pixels, depth, result, ob_low, normal_swizzle);
				break;
			}
			case R_BAKE_SPACE_TANGENT:
			{
				if (is_selected_to_active) {
					RE_bake_normal_world_to_tangent(pixel_array_low, num_pixels, depth, result, me_low, normal_swizzle, ob_low->obmat);
				}
				else {
					/* from multiresolution */
					Mesh *me_nores = NULL;
					ModifierData *md = NULL;
					int mode;

					md = modifiers_findByType(ob_low, eModifierType_Multires);

					if (md) {
						mode = md->mode;
						md->mode &= ~eModifierMode_Render;
					}

					me_nores = BKE_mesh_new_from_object(bmain, scene, ob_low, 1, 2, 1, 0);
					RE_bake_pixels_populate(me_nores, pixel_array_low, num_pixels, &bake_images, uv_layer);

					RE_bake_normal_world_to_tangent(pixel_array_low, num_pixels, depth, result, me_nores, normal_swizzle, ob_low->obmat);
					BKE_libblock_free(bmain, me_nores);

					if (md)
						md->mode = mode;
				}
				break;
			}
			default:
				break;
		}
	}

	if (!ok) {
		BKE_reportf(reports, RPT_ERROR, "Problem baking object \"%s\"", ob_low->id.name + 2);
		op_result = OPERATOR_CANCELLED;
	}
	else {
		/* save the results */
		for (i = 0; i < bake_images.size; i++) {
			BakeImage *bk_image = &bake_images.data[i];

			if (is_save_internal) {
				ok = write_internal_bake_pixels(
				         bk_image->image,
				         pixel_array_low + bk_image->offset,
				         result + bk_image->offset * depth,
				         bk_image->width, bk_image->height,
				         margin, is_clear, is_noncolor);

				/* might be read by UI to set active image for display */
				bake_update_image(sa, bk_image->image);

				if (!ok) {
					BKE_reportf(reports, RPT_ERROR,
					           "Problem saving the bake map internally for object \"%s\"", ob_low->id.name + 2);
					op_result = OPERATOR_CANCELLED;
				}
				else {
					BKE_report(reports, RPT_INFO,
					           "Baking map saved to internal image, save it externally or pack it");
					op_result = OPERATOR_FINISHED;
				}
			}
			/* save externally */
			else {
				BakeData *bake = &scene->r.bake;
				char name[FILE_MAX];

				BKE_makepicstring_from_type(name, filepath, bmain->name, 0, bake->im_format.imtype, true, false);

				if (is_automatic_name) {
					BLI_path_suffix(name, FILE_MAX, ob_low->id.name + 2, "_");
					BLI_path_suffix(name, FILE_MAX, identifier, "_");
				}

				if (is_split_materials) {
					if (bk_image->image) {
						BLI_path_suffix(name, FILE_MAX, bk_image->image->id.name + 2, "_");
					}
					else {
						if (ob_low->mat[i]) {
							BLI_path_suffix(name, FILE_MAX, ob_low->mat[i]->id.name + 2, "_");
						}
						else if (me_low->mat[i]) {
							BLI_path_suffix(name, FILE_MAX, me_low->mat[i]->id.name + 2, "_");
						}
						else {
							/* if everything else fails, use the material index */
							char tmp[4];
							sprintf(tmp, "%d", i % 1000);
							BLI_path_suffix(name, FILE_MAX, tmp, "_");
						}
					}
				}

				/* save it externally */
				ok = write_external_bake_pixels(
				        name,
				        pixel_array_low + bk_image->offset,
				        result + bk_image->offset * depth,
				        bk_image->width, bk_image->height,
				        margin, &bake->im_format, is_noncolor);

				if (!ok) {
					BKE_reportf(reports, RPT_ERROR, "Problem saving baked map in \"%s\"", name);
					op_result = OPERATOR_CANCELLED;
				}
				else {
					BKE_reportf(reports, RPT_INFO, "Baking map written to \"%s\"", name);
					op_result = OPERATOR_FINISHED;
				}

				if (!is_split_materials) {
					break;
				}
			}
		}
	}

	if (is_save_internal)
		reset_images_gpu(&bake_images);

cleanup:

	if (highpoly) {
		int i;
		for (i = 0; i < tot_highpoly; i++) {
			highpoly[i].ob->restrictflag = highpoly[i].restrict_flag;

			if (highpoly[i].pixel_array)
				MEM_freeN(highpoly[i].pixel_array);

			if (highpoly[i].tri_mod)
				ED_object_modifier_remove(reports, bmain, highpoly[i].ob, highpoly[i].tri_mod);

			if (highpoly[i].me)
				BKE_libblock_free(bmain, highpoly[i].me);
		}
		MEM_freeN(highpoly);
	}

	ob_low->restrictflag = restrict_flag_low;

	if (ob_cage)
		ob_cage->restrictflag = restrict_flag_cage;

	if (pixel_array_low)
		MEM_freeN(pixel_array_low);

	if (bake_images.data)
		MEM_freeN(bake_images.data);

	if (bake_images.lookup)
		MEM_freeN(bake_images.lookup);

	if (result)
		MEM_freeN(result);

	if (me_low)
		BKE_libblock_free(bmain, me_low);

	if (me_cage)
		BKE_libblock_free(bmain, me_cage);

	return op_result;
}

static void bake_init_api_data(wmOperator *op, bContext *C, BakeAPIRender *bkr)
{
	bool is_save_internal;
	bScreen *sc = CTX_wm_screen(C);

	bkr->ob = CTX_data_active_object(C);
	bkr->main = CTX_data_main(C);
	bkr->scene = CTX_data_scene(C);
	bkr->sa = sc ? BKE_screen_find_big_area(sc, SPACE_IMAGE, 10) : NULL;

	bkr->pass_type = RNA_enum_get(op->ptr, "type");
	bkr->margin = RNA_int_get(op->ptr, "margin");

	bkr->save_mode = RNA_enum_get(op->ptr, "save_mode");
	is_save_internal = (bkr->save_mode == R_BAKE_SAVE_INTERNAL);

	bkr->is_clear = RNA_boolean_get(op->ptr, "use_clear");
	bkr->is_split_materials = (!is_save_internal) && RNA_boolean_get(op->ptr, "use_split_materials");
	bkr->is_automatic_name = RNA_boolean_get(op->ptr, "use_automatic_name");
	bkr->is_selected_to_active = RNA_boolean_get(op->ptr, "use_selected_to_active");
	bkr->is_cage = RNA_boolean_get(op->ptr, "use_cage");
	bkr->cage_extrusion = RNA_float_get(op->ptr, "cage_extrusion");

	bkr->normal_space = RNA_enum_get(op->ptr, "normal_space");
	bkr->normal_swizzle[0] = RNA_enum_get(op->ptr, "normal_r");
	bkr->normal_swizzle[1] = RNA_enum_get(op->ptr, "normal_g");
	bkr->normal_swizzle[2] = RNA_enum_get(op->ptr, "normal_b");

	bkr->width = RNA_int_get(op->ptr, "width");
	bkr->height = RNA_int_get(op->ptr, "height");
	bkr->identifier = "";

	RNA_string_get(op->ptr, "uv_layer", bkr->uv_layer);

	RNA_string_get(op->ptr, "cage_object", bkr->custom_cage);

	if ((!is_save_internal) && bkr->is_automatic_name) {
		PropertyRNA *prop = RNA_struct_find_property(op->ptr, "type");
		RNA_property_enum_identifier(C, op->ptr, prop, bkr->pass_type, &bkr->identifier);
	}

	CTX_data_selected_objects(C, &bkr->selected_objects);

	bkr->reports = op->reports;

	bkr->result = OPERATOR_CANCELLED;

	bkr->render = RE_NewRender(bkr->scene->id.name);

	/* XXX hack to force saving to always be internal. Whether (and how) to support
	 * external saving will be addressed later */
	bkr->save_mode = R_BAKE_SAVE_INTERNAL;
}

static int bake_exec(bContext *C, wmOperator *op)
{
	Render *re;
	int result = OPERATOR_CANCELLED;
	BakeAPIRender bkr = {NULL};

	bake_init_api_data(op, C, &bkr);
	re = bkr.render;

	/* setup new render */
	RE_test_break_cb(re, NULL, bake_break);

	if (!bake_objects_check(bkr.main, bkr.ob, &bkr.selected_objects, bkr.reports, bkr.is_selected_to_active))
		return OPERATOR_CANCELLED;

	if (bkr.is_clear) {
		const bool is_tangent = ((bkr.pass_type == SCE_PASS_NORMAL) && (bkr.normal_space == R_BAKE_SPACE_TANGENT));
		bake_images_clear(bkr.main, is_tangent);
	}

	RE_SetReports(re, bkr.reports);

	if (bkr.is_selected_to_active) {
		result = bake(
		        bkr.render, bkr.main, bkr.scene, bkr.ob, &bkr.selected_objects, bkr.reports,
		        bkr.pass_type, bkr.margin, bkr.save_mode,
		        bkr.is_clear, bkr.is_split_materials, bkr.is_automatic_name, true, bkr.is_cage,
		        bkr.cage_extrusion, bkr.normal_space, bkr.normal_swizzle,
		        bkr.custom_cage, bkr.filepath, bkr.width, bkr.height, bkr.identifier, bkr.sa,
		        bkr.uv_layer);
	}
	else {
		CollectionPointerLink *link;
		const bool is_clear = bkr.is_clear && BLI_listbase_is_single(&bkr.selected_objects);
		for (link = bkr.selected_objects.first; link; link = link->next) {
			Object *ob_iter = link->ptr.data;
			result = bake(
			        bkr.render, bkr.main, bkr.scene, ob_iter, NULL, bkr.reports,
			        bkr.pass_type, bkr.margin, bkr.save_mode,
			        is_clear, bkr.is_split_materials, bkr.is_automatic_name, false, bkr.is_cage,
			        bkr.cage_extrusion, bkr.normal_space, bkr.normal_swizzle,
			        bkr.custom_cage, bkr.filepath, bkr.width, bkr.height, bkr.identifier, bkr.sa,
			        bkr.uv_layer);
		}
	}

	RE_SetReports(re, NULL);

	BLI_freelistN(&bkr.selected_objects);
	return result;
}

static void bake_startjob(void *bkv, short *UNUSED(stop), short *do_update, float *progress)
{
	BakeAPIRender *bkr = (BakeAPIRender *)bkv;

	/* setup new render */
	bkr->do_update = do_update;
	bkr->progress = progress;

	RE_SetReports(bkr->render, bkr->reports);

	if (!bake_objects_check(bkr->main, bkr->ob, &bkr->selected_objects, bkr->reports, bkr->is_selected_to_active)) {
		bkr->result = OPERATOR_CANCELLED;
		return;
	}

	if (bkr->is_clear) {
		const bool is_tangent = ((bkr->pass_type == SCE_PASS_NORMAL) && (bkr->normal_space == R_BAKE_SPACE_TANGENT));
		bake_images_clear(bkr->main, is_tangent);
	}

	if (bkr->is_selected_to_active) {
		bkr->result = bake(
		        bkr->render, bkr->main, bkr->scene, bkr->ob, &bkr->selected_objects, bkr->reports,
		        bkr->pass_type, bkr->margin, bkr->save_mode,
		        bkr->is_clear, bkr->is_split_materials, bkr->is_automatic_name, true, bkr->is_cage,
		        bkr->cage_extrusion, bkr->normal_space, bkr->normal_swizzle,
		        bkr->custom_cage, bkr->filepath, bkr->width, bkr->height, bkr->identifier, bkr->sa,
		        bkr->uv_layer);
	}
	else {
		CollectionPointerLink *link;
		const bool is_clear = bkr->is_clear && BLI_listbase_is_single(&bkr->selected_objects);
		for (link = bkr->selected_objects.first; link; link = link->next) {
			Object *ob_iter = link->ptr.data;
			bkr->result = bake(
			        bkr->render, bkr->main, bkr->scene, ob_iter, NULL, bkr->reports,
			        bkr->pass_type, bkr->margin, bkr->save_mode,
			        is_clear, bkr->is_split_materials, bkr->is_automatic_name, false, bkr->is_cage,
			        bkr->cage_extrusion, bkr->normal_space, bkr->normal_swizzle,
			        bkr->custom_cage, bkr->filepath, bkr->width, bkr->height, bkr->identifier, bkr->sa,
			        bkr->uv_layer);

			if (bkr->result == OPERATOR_CANCELLED)
				return;
		}
	}

	RE_SetReports(bkr->render, NULL);
}

static void bake_freejob(void *bkv)
{
	BakeAPIRender *bkr = (BakeAPIRender *)bkv;

	BLI_freelistN(&bkr->selected_objects);
	MEM_freeN(bkr);

	G.is_rendering = false;
}

static void bake_set_props(wmOperator *op, Scene *scene)
{
	PropertyRNA *prop;
	BakeData *bake = &scene->r.bake;

	prop = RNA_struct_find_property(op->ptr, "filepath");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_string_set(op->ptr, prop, bake->filepath);
	}

	prop =  RNA_struct_find_property(op->ptr, "width");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_int_set(op->ptr, prop, bake->width);
	}

	prop =  RNA_struct_find_property(op->ptr, "height");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_int_set(op->ptr, prop, bake->width);
	}

	prop = RNA_struct_find_property(op->ptr, "margin");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_int_set(op->ptr, prop, bake->margin);
	}

	prop = RNA_struct_find_property(op->ptr, "use_selected_to_active");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_boolean_set(op->ptr, prop, (bake->flag & R_BAKE_TO_ACTIVE));
	}

	prop = RNA_struct_find_property(op->ptr, "cage_extrusion");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_float_set(op->ptr, prop, bake->cage_extrusion);
	}

	prop = RNA_struct_find_property(op->ptr, "cage_object");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_string_set(op->ptr, prop, bake->cage);
	}

	prop = RNA_struct_find_property(op->ptr, "normal_space");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_enum_set(op->ptr, prop, bake->normal_space);
	}

	prop = RNA_struct_find_property(op->ptr, "normal_r");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_enum_set(op->ptr, prop, bake->normal_swizzle[0]);
	}

	prop = RNA_struct_find_property(op->ptr, "normal_g");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_enum_set(op->ptr, prop, bake->normal_swizzle[1]);
	}

	prop = RNA_struct_find_property(op->ptr, "normal_b");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_enum_set(op->ptr, prop, bake->normal_swizzle[2]);
	}

	prop = RNA_struct_find_property(op->ptr, "save_mode");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_enum_set(op->ptr, prop, bake->save_mode);
	}

	prop = RNA_struct_find_property(op->ptr, "use_clear");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_boolean_set(op->ptr, prop, (bake->flag & R_BAKE_CLEAR));
	}

	prop = RNA_struct_find_property(op->ptr, "use_cage");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_boolean_set(op->ptr, prop, (bake->flag & R_BAKE_CAGE));
	}

	prop = RNA_struct_find_property(op->ptr, "use_split_materials");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_boolean_set(op->ptr, prop, (bake->flag & R_BAKE_SPLIT_MAT));
	}

	prop = RNA_struct_find_property(op->ptr, "use_automatic_name");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_boolean_set(op->ptr, prop, (bake->flag & R_BAKE_AUTO_NAME));
	}
}

static int bake_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	wmJob *wm_job;
	BakeAPIRender *bkr;
	Render *re;
	Scene *scene = CTX_data_scene(C);

	bake_set_props(op, scene);

	/* only one render job at a time */
	if (WM_jobs_test(CTX_wm_manager(C), scene, WM_JOB_TYPE_OBJECT_BAKE))
		return OPERATOR_CANCELLED;

	bkr = MEM_callocN(sizeof(BakeAPIRender), "render bake");

	/* init bake render */
	bake_init_api_data(op, C, bkr);
	re = bkr->render;

	/* setup new render */
	RE_test_break_cb(re, NULL, bake_break);
	RE_progress_cb(re, bkr, bake_progress_update);

	/* setup job */
	wm_job = WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), scene, "Texture Bake",
	                     WM_JOB_EXCL_RENDER | WM_JOB_PRIORITY | WM_JOB_PROGRESS, WM_JOB_TYPE_OBJECT_BAKE);
	WM_jobs_customdata_set(wm_job, bkr, bake_freejob);
	WM_jobs_timer(wm_job, 0.5, NC_IMAGE, 0); /* TODO - only draw bake image, can we enforce this */
	WM_jobs_callbacks(wm_job, bake_startjob, NULL, NULL, NULL);

	G.is_break = false;
	G.is_rendering = true;

	WM_jobs_start(CTX_wm_manager(C), wm_job);

	WM_cursor_wait(0);

	/* add modal handler for ESC */
	WM_event_add_modal_handler(C, op);

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, scene);
	return OPERATOR_RUNNING_MODAL;
}

void OBJECT_OT_bake(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Bake";
	ot->description = "Bake image textures of selected objects";
	ot->idname = "OBJECT_OT_bake";

	/* api callbacks */
	ot->exec = bake_exec;
	ot->modal = bake_modal;
	ot->invoke = bake_invoke;
	ot->poll = ED_operator_object_active_editable_mesh;

	RNA_def_enum(ot->srna, "type", render_pass_type_items, SCE_PASS_COMBINED, "Type",
	             "Type of pass to bake, some of them may not be supported by the current render engine");
	RNA_def_string_file_path(ot->srna, "filepath", NULL, FILE_MAX, "File Path",
	                         "Image filepath to use when saving externally");
	RNA_def_int(ot->srna, "width", 512, 1, INT_MAX, "Width",
	            "Horizontal dimension of the baking map (external only)", 64, 4096);
	RNA_def_int(ot->srna, "height", 512, 1, INT_MAX, "Height",
	            "Vertical dimension of the baking map (external only)", 64, 4096);
	RNA_def_int(ot->srna, "margin", 16, 0, INT_MAX, "Margin",
	            "Extends the baked result as a post process filter", 0, 64);
	RNA_def_boolean(ot->srna, "use_selected_to_active", false, "Selected to Active",
	                "Bake shading on the surface of selected objects to the active object");
	RNA_def_float(ot->srna, "cage_extrusion", 0.0f, 0.0f, FLT_MAX, "Cage Extrusion",
	              "Distance to use for the inward ray cast when using selected to active", 0.0f, 1.0f);
	RNA_def_string(ot->srna, "cage_object", NULL, MAX_NAME, "Cage Object",
	               "Object to use as cage, instead of calculating the cage from the active object with cage extrusion");
	RNA_def_enum(ot->srna, "normal_space", normal_space_items, R_BAKE_SPACE_TANGENT, "Normal Space",
	             "Choose normal space for baking");
	RNA_def_enum(ot->srna, "normal_r", normal_swizzle_items, R_BAKE_POSX, "R", "Axis to bake in red channel");
	RNA_def_enum(ot->srna, "normal_g", normal_swizzle_items, R_BAKE_POSY, "G", "Axis to bake in green channel");
	RNA_def_enum(ot->srna, "normal_b", normal_swizzle_items, R_BAKE_POSZ, "B", "Axis to bake in blue channel");
	RNA_def_enum(ot->srna, "save_mode", bake_save_mode_items, R_BAKE_SAVE_INTERNAL, "Save Mode",
	             "Choose how to save the baking map");
	RNA_def_boolean(ot->srna, "use_clear", false, "Clear",
	                "Clear Images before baking (only for internal saving)");
	RNA_def_boolean(ot->srna, "use_cage", false, "Cage",
	                "Cast rays to active object from a cage");
	RNA_def_boolean(ot->srna, "use_split_materials", false, "Split Materials",
	                "Split baked maps per material, using material name in output file (external only)");
	RNA_def_boolean(ot->srna, "use_automatic_name", false, "Automatic Name",
	                "Automatically name the output file with the pass type");
	RNA_def_string(ot->srna, "uv_layer", NULL, MAX_CUSTOMDATA_LAYER_NAME, "UV Layer", "UV layer to override active");
}
