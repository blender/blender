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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* note: the original vertex color stuff is now just used for
 * getting info on the layers themselves, accessing the data is
 * done through the (not yet written) mpoly interfaces.*/

/** \file blender/makesrna/intern/rna_mesh.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math_base.h"
#include "BLI_math_rotation.h"
#include "BLI_utildefines.h"

#include "BKE_editmesh.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "WM_types.h"

EnumPropertyItem mesh_delimit_mode_items[] = {
	{BMO_DELIM_NORMAL, "NORMAL", 0, "Normal", "Delimit by face directions"},
	{BMO_DELIM_MATERIAL, "MATERIAL", 0, "Material", "Delimit by face material"},
	{BMO_DELIM_SEAM, "SEAM", 0, "Seam", "Delimit by edge seams"},
	{0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME

#include "DNA_scene_types.h"

#include "BLI_math.h"

#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_report.h"

#include "ED_mesh.h" /* XXX Bad level call */

#include "WM_api.h"
#include "WM_types.h"

#include "rna_mesh_utils.h"


/* -------------------------------------------------------------------- */
/* Generic helpers */

static Mesh *rna_mesh(PointerRNA *ptr)
{
	Mesh *me = (Mesh *)ptr->id.data;
	return me;
}

static CustomData *rna_mesh_vdata_helper(Mesh *me)
{
	return (me->edit_btmesh) ? &me->edit_btmesh->bm->vdata : &me->vdata;
}

static CustomData *rna_mesh_edata_helper(Mesh *me)
{
	return (me->edit_btmesh) ? &me->edit_btmesh->bm->edata : &me->edata;
}

static CustomData *rna_mesh_pdata_helper(Mesh *me)
{
	return (me->edit_btmesh) ? &me->edit_btmesh->bm->pdata : &me->pdata;
}

static CustomData *rna_mesh_ldata_helper(Mesh *me)
{
	return (me->edit_btmesh) ? &me->edit_btmesh->bm->ldata : &me->ldata;
}

static CustomData *rna_mesh_fdata_helper(Mesh *me)
{
	return (me->edit_btmesh) ? NULL : &me->fdata;
}

static CustomData *rna_mesh_vdata(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return rna_mesh_vdata_helper(me);
}
#if 0
static CustomData *rna_mesh_edata(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return rna_mesh_edata_helper(me);
}
#endif
static CustomData *rna_mesh_pdata(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return rna_mesh_pdata_helper(me);
}

static CustomData *rna_mesh_ldata(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return rna_mesh_ldata_helper(me);
}

static CustomData *rna_mesh_fdata(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return rna_mesh_fdata_helper(me);
}


/* -------------------------------------------------------------------- */
/* Generic CustomData Layer Functions */

static void rna_cd_layer_name_set(CustomData *cdata, CustomDataLayer *cdl, const char *value)
{
	BLI_strncpy_utf8(cdl->name, value, sizeof(cdl->name));
	CustomData_set_layer_unique_name(cdata, cdl - cdata->layers);
}

/* avoid using where possible!, ideally the type is known */
static CustomData *rna_cd_from_layer(PointerRNA *ptr, CustomDataLayer *cdl)
{
	/* find out where we come from by */
	Mesh *me = ptr->id.data;
	CustomData *cd;

	/* rely on negative values wrapping */
#define TEST_CDL(cmd) if ((void)(cd = cmd(me)), ARRAY_HAS_ITEM(cdl, cd->layers, cd->totlayer)) return cd

	TEST_CDL(rna_mesh_vdata_helper);
	TEST_CDL(rna_mesh_edata_helper);
	TEST_CDL(rna_mesh_pdata_helper);
	TEST_CDL(rna_mesh_ldata_helper);
	TEST_CDL(rna_mesh_fdata_helper);

#undef TEST_CDL

	/* should _never_ happen */
	return NULL;
}

static void rna_MeshVertexLayer_name_set(PointerRNA *ptr, const char *value)
{
	rna_cd_layer_name_set(rna_mesh_vdata(ptr), (CustomDataLayer *)ptr->data, value);
}
#if 0
static void rna_MeshEdgeLayer_name_set(PointerRNA *ptr, const char *value)
{
	rna_cd_layer_name_set(rna_mesh_edata(ptr), (CustomDataLayer *)ptr->data, value);
}
#endif
#if 0
static void rna_MeshPolyLayer_name_set(PointerRNA *ptr, const char *value)
{
	rna_cd_layer_name_set(rna_mesh_pdata(ptr), (CustomDataLayer *)ptr->data, value);
}
#endif
static void rna_MeshLoopLayer_name_set(PointerRNA *ptr, const char *value)
{
	rna_cd_layer_name_set(rna_mesh_ldata(ptr), (CustomDataLayer *)ptr->data, value);
}
#if 0
static void rna_MeshTessfaceLayer_name_set(PointerRNA *ptr, const char *value)
{
	rna_cd_layer_name_set(rna_mesh_fdata(ptr), (CustomDataLayer *)ptr->data, value);
}
#endif
/* only for layers shared between types */
static void rna_MeshAnyLayer_name_set(PointerRNA *ptr, const char *value)
{
	CustomData *cd = rna_cd_from_layer(ptr, (CustomDataLayer *)ptr->data);
	rna_cd_layer_name_set(cd, (CustomDataLayer *)ptr->data, value);
}


/* -------------------------------------------------------------------- */
/* Update Callbacks */

static void rna_Mesh_update_data(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	ID *id = ptr->id.data;

	/* cheating way for importers to avoid slow updates */
	if (id->us > 0) {
		DAG_id_tag_update(id, 0);
		WM_main_add_notifier(NC_GEOM | ND_DATA, id);
	}
}

static void rna_Mesh_update_data_edit_color(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	rna_Mesh_update_data(bmain, scene, ptr);
	if (me->edit_btmesh) {
		BKE_editmesh_color_free(me->edit_btmesh);
	}
}

static void rna_Mesh_update_select(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	ID *id = ptr->id.data;
	/* cheating way for importers to avoid slow updates */
	if (id->us > 0) {
		WM_main_add_notifier(NC_GEOM | ND_SELECT, id);
	}
}

void rna_Mesh_update_draw(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	ID *id = ptr->id.data;
	/* cheating way for importers to avoid slow updates */
	if (id->us > 0) {
		WM_main_add_notifier(NC_GEOM | ND_DATA, id);
	}
}


static void rna_Mesh_update_vertmask(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Mesh *me = ptr->data;
	if ((me->editflag & ME_EDIT_PAINT_VERT_SEL) && (me->editflag & ME_EDIT_PAINT_FACE_SEL)) {
		me->editflag &= ~ME_EDIT_PAINT_FACE_SEL;
	}
	rna_Mesh_update_draw(bmain, scene, ptr);
}

static void rna_Mesh_update_facemask(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Mesh *me = ptr->data;
	if ((me->editflag & ME_EDIT_PAINT_VERT_SEL) && (me->editflag & ME_EDIT_PAINT_FACE_SEL)) {
		me->editflag &= ~ME_EDIT_PAINT_VERT_SEL;
	}
	rna_Mesh_update_draw(bmain, scene, ptr);
}


/* -------------------------------------------------------------------- */
/* Property get/set Callbacks  */

static void rna_MeshVertex_normal_get(PointerRNA *ptr, float *value)
{
	MVert *mvert = (MVert *)ptr->data;
	normal_short_to_float_v3(value, mvert->no);
}

static void rna_MeshVertex_normal_set(PointerRNA *ptr, const float *value)
{
	MVert *mvert = (MVert *)ptr->data;
	float no[3];

	copy_v3_v3(no, value);
	normalize_v3(no);
	normal_float_to_short_v3(mvert->no, no);
}

static float rna_MeshVertex_bevel_weight_get(PointerRNA *ptr)
{
	MVert *mvert = (MVert *)ptr->data;
	return mvert->bweight / 255.0f;
}

static void rna_MeshVertex_bevel_weight_set(PointerRNA *ptr, float value)
{
	MVert *mvert = (MVert *)ptr->data;
	mvert->bweight = (char)(CLAMPIS(value * 255.0f, 0, 255));
}

static float rna_MEdge_bevel_weight_get(PointerRNA *ptr)
{
	MEdge *medge = (MEdge *)ptr->data;
	return medge->bweight / 255.0f;
}

static void rna_MEdge_bevel_weight_set(PointerRNA *ptr, float value)
{
	MEdge *medge = (MEdge *)ptr->data;
	medge->bweight = (char)(CLAMPIS(value * 255.0f, 0, 255));
}

static float rna_MEdge_crease_get(PointerRNA *ptr)
{
	MEdge *medge = (MEdge *)ptr->data;
	return medge->crease / 255.0f;
}

static void rna_MEdge_crease_set(PointerRNA *ptr, float value)
{
	MEdge *medge = (MEdge *)ptr->data;
	medge->crease = (char)(CLAMPIS(value * 255.0f, 0, 255));
}

static void rna_MeshLoop_normal_get(PointerRNA *ptr, float *values)
{
	Mesh *me = rna_mesh(ptr);
	MLoop *ml = (MLoop *)ptr->data;
	const float (*vec)[3] = CustomData_get(&me->ldata, (int)(ml - me->mloop), CD_NORMAL);

	if (!vec) {
		zero_v3(values);
	}
	else {
		copy_v3_v3(values, (const float *)vec);
	}
}

static void rna_MeshLoop_tangent_get(PointerRNA *ptr, float *values)
{
	Mesh *me = rna_mesh(ptr);
	MLoop *ml = (MLoop *)ptr->data;
	const float (*vec)[4] = CustomData_get(&me->ldata, (int)(ml - me->mloop), CD_MLOOPTANGENT);

	if (!vec) {
		zero_v3(values);
	}
	else {
		copy_v3_v3(values, (const float *)vec);
	}
}

static float rna_MeshLoop_bitangent_sign_get(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	MLoop *ml = (MLoop *)ptr->data;
	const float (*vec)[4] = CustomData_get(&me->ldata, (int)(ml - me->mloop), CD_MLOOPTANGENT);

	return (vec) ? (*vec)[3] : 0.0f;
}

static void rna_MeshLoop_bitangent_get(PointerRNA *ptr, float *values)
{
	Mesh *me = rna_mesh(ptr);
	MLoop *ml = (MLoop *)ptr->data;
	const float (*nor)[3] = CustomData_get(&me->ldata, (int)(ml - me->mloop), CD_NORMAL);
	const float (*vec)[4] = CustomData_get(&me->ldata, (int)(ml - me->mloop), CD_MLOOPTANGENT);

	if (nor && vec) {
		cross_v3_v3v3(values, (const float *)nor, (const float *)vec);
		mul_v3_fl(values, (*vec)[3]);
	}
	else {
		zero_v3(values);
	}
}

static void rna_MeshPolygon_normal_get(PointerRNA *ptr, float *values)
{
	Mesh *me = rna_mesh(ptr);
	MPoly *mp = (MPoly *)ptr->data;

	BKE_mesh_calc_poly_normal(mp, me->mloop + mp->loopstart, me->mvert, values);
}

static void rna_MeshPolygon_center_get(PointerRNA *ptr, float *values)
{
	Mesh *me = rna_mesh(ptr);
	MPoly *mp = (MPoly *)ptr->data;

	BKE_mesh_calc_poly_center(mp, me->mloop + mp->loopstart, me->mvert, values);
}

static float rna_MeshPolygon_area_get(PointerRNA *ptr)
{
	Mesh *me = (Mesh *)ptr->id.data;
	MPoly *mp = (MPoly *)ptr->data;

	return BKE_mesh_calc_poly_area(mp, me->mloop + mp->loopstart, me->mvert, NULL);
}

static void rna_MeshTessFace_normal_get(PointerRNA *ptr, float *values)
{
	Mesh *me = rna_mesh(ptr);
	MFace *mface = (MFace *)ptr->data;

	if (mface->v4)
		normal_quad_v3(values, me->mvert[mface->v1].co, me->mvert[mface->v2].co,
		               me->mvert[mface->v3].co, me->mvert[mface->v4].co);
	else
		normal_tri_v3(values, me->mvert[mface->v1].co, me->mvert[mface->v2].co, me->mvert[mface->v3].co);
}

static float rna_MeshTessFace_area_get(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	MFace *mface = (MFace *)ptr->data;

	if (mface->v4)
		return area_quad_v3(me->mvert[mface->v1].co, me->mvert[mface->v2].co, me->mvert[mface->v3].co,
		                    me->mvert[mface->v4].co);
	else
		return area_tri_v3(me->mvert[mface->v1].co, me->mvert[mface->v2].co, me->mvert[mface->v3].co);
}

static void rna_MeshTextureFace_uv1_get(PointerRNA *ptr, float *values)
{
	MTFace *mtface = (MTFace *)ptr->data;

	values[0] = mtface->uv[0][0];
	values[1] = mtface->uv[0][1];
}

static void rna_MeshTextureFace_uv1_set(PointerRNA *ptr, const float *values)
{
	MTFace *mtface = (MTFace *)ptr->data;

	mtface->uv[0][0] = values[0];
	mtface->uv[0][1] = values[1];
}

static void rna_MeshTextureFace_uv2_get(PointerRNA *ptr, float *values)
{
	MTFace *mtface = (MTFace *)ptr->data;

	values[0] = mtface->uv[1][0];
	values[1] = mtface->uv[1][1];
}

static void rna_MeshTextureFace_uv2_set(PointerRNA *ptr, const float *values)
{
	MTFace *mtface = (MTFace *)ptr->data;

	mtface->uv[1][0] = values[0];
	mtface->uv[1][1] = values[1];
}

static void rna_MeshTextureFace_uv3_get(PointerRNA *ptr, float *values)
{
	MTFace *mtface = (MTFace *)ptr->data;

	values[0] = mtface->uv[2][0];
	values[1] = mtface->uv[2][1];
}

static void rna_MeshTextureFace_uv3_set(PointerRNA *ptr, const float *values)
{
	MTFace *mtface = (MTFace *)ptr->data;

	mtface->uv[2][0] = values[0];
	mtface->uv[2][1] = values[1];
}

static void rna_MeshTextureFace_uv4_get(PointerRNA *ptr, float *values)
{
	MTFace *mtface = (MTFace *)ptr->data;

	values[0] = mtface->uv[3][0];
	values[1] = mtface->uv[3][1];
}

static void rna_MeshTextureFace_uv4_set(PointerRNA *ptr, const float *values)
{
	MTFace *mtface = (MTFace *)ptr->data;

	mtface->uv[3][0] = values[0];
	mtface->uv[3][1] = values[1];
}

static int rna_CustomDataData_numverts(PointerRNA *ptr, int type)
{
	Mesh *me = rna_mesh(ptr);
	CustomData *fdata = rna_mesh_fdata(ptr);
	CustomDataLayer *cdl;
	int a, b;

	for (cdl = fdata->layers, a = 0; a < fdata->totlayer; cdl++, a++) {
		if (cdl->type == type) {
			b = ((char *)ptr->data - ((char *)cdl->data)) / CustomData_sizeof(type);
			if (b >= 0 && b < me->totface) {
				return (me->mface[b].v4 ? 4 : 3);
			}
		}
	}

	return 0;
}

static int rna_MeshTextureFace_uv_get_length(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
	length[0] = rna_CustomDataData_numverts(ptr, CD_MTFACE);
	length[1] = 2;
	return length[0] * length[1];
}

static void rna_MeshTextureFace_uv_get(PointerRNA *ptr, float *values)
{
	MTFace *mtface = (MTFace *)ptr->data;
	int totvert = rna_CustomDataData_numverts(ptr, CD_MTFACE);

	memcpy(values, mtface->uv, totvert * 2 * sizeof(float));
}

static void rna_MeshTextureFace_uv_set(PointerRNA *ptr, const float *values)
{
	MTFace *mtface = (MTFace *)ptr->data;
	int totvert = rna_CustomDataData_numverts(ptr, CD_MTFACE);

	memcpy(mtface->uv, values, totvert * 2 * sizeof(float));
}

/* notice red and blue are swapped */
static void rna_MeshColor_color1_get(PointerRNA *ptr, float *values)
{
	MCol *mcol = (MCol *)ptr->data;

	values[2] = (&mcol[0].r)[0] / 255.0f;
	values[1] = (&mcol[0].r)[1] / 255.0f;
	values[0] = (&mcol[0].r)[2] / 255.0f;
}

static void rna_MeshColor_color1_set(PointerRNA *ptr, const float *values)
{
	MCol *mcol = (MCol *)ptr->data;

	(&mcol[0].r)[2] = (char)(CLAMPIS(values[0] * 255.0f, 0, 255));
	(&mcol[0].r)[1] = (char)(CLAMPIS(values[1] * 255.0f, 0, 255));
	(&mcol[0].r)[0] = (char)(CLAMPIS(values[2] * 255.0f, 0, 255));
}

static void rna_MeshColor_color2_get(PointerRNA *ptr, float *values)
{
	MCol *mcol = (MCol *)ptr->data;

	values[2] = (&mcol[1].r)[0] / 255.0f;
	values[1] = (&mcol[1].r)[1] / 255.0f;
	values[0] = (&mcol[1].r)[2] / 255.0f;
}

static void rna_MeshColor_color2_set(PointerRNA *ptr, const float *values)
{
	MCol *mcol = (MCol *)ptr->data;

	(&mcol[1].r)[2] = (char)(CLAMPIS(values[0] * 255.0f, 0, 255));
	(&mcol[1].r)[1] = (char)(CLAMPIS(values[1] * 255.0f, 0, 255));
	(&mcol[1].r)[0] = (char)(CLAMPIS(values[2] * 255.0f, 0, 255));
}

static void rna_MeshColor_color3_get(PointerRNA *ptr, float *values)
{
	MCol *mcol = (MCol *)ptr->data;

	values[2] = (&mcol[2].r)[0] / 255.0f;
	values[1] = (&mcol[2].r)[1] / 255.0f;
	values[0] = (&mcol[2].r)[2] / 255.0f;
}

static void rna_MeshColor_color3_set(PointerRNA *ptr, const float *values)
{
	MCol *mcol = (MCol *)ptr->data;

	(&mcol[2].r)[2] = (char)(CLAMPIS(values[0] * 255.0f, 0, 255));
	(&mcol[2].r)[1] = (char)(CLAMPIS(values[1] * 255.0f, 0, 255));
	(&mcol[2].r)[0] = (char)(CLAMPIS(values[2] * 255.0f, 0, 255));
}

static void rna_MeshColor_color4_get(PointerRNA *ptr, float *values)
{
	MCol *mcol = (MCol *)ptr->data;

	values[2] = (&mcol[3].r)[0] / 255.0f;
	values[1] = (&mcol[3].r)[1] / 255.0f;
	values[0] = (&mcol[3].r)[2] / 255.0f;
}

static void rna_MeshColor_color4_set(PointerRNA *ptr, const float *values)
{
	MCol *mcol = (MCol *)ptr->data;

	(&mcol[3].r)[2] = (char)(CLAMPIS(values[0] * 255.0f, 0, 255));
	(&mcol[3].r)[1] = (char)(CLAMPIS(values[1] * 255.0f, 0, 255));
	(&mcol[3].r)[0] = (char)(CLAMPIS(values[2] * 255.0f, 0, 255));
}

static void rna_MeshLoopColor_color_get(PointerRNA *ptr, float *values)
{
	MLoopCol *mcol = (MLoopCol *)ptr->data;

	values[0] = (&mcol->r)[0] / 255.0f;
	values[1] = (&mcol->r)[1] / 255.0f;
	values[2] = (&mcol->r)[2] / 255.0f;
}

static void rna_MeshLoopColor_color_set(PointerRNA *ptr, const float *values)
{
	MLoopCol *mcol = (MLoopCol *)ptr->data;

	(&mcol->r)[0] = (char)(CLAMPIS(values[0] * 255.0f, 0, 255));
	(&mcol->r)[1] = (char)(CLAMPIS(values[1] * 255.0f, 0, 255));
	(&mcol->r)[2] = (char)(CLAMPIS(values[2] * 255.0f, 0, 255));
}

static int rna_Mesh_texspace_editable(PointerRNA *ptr)
{
	Mesh *me = (Mesh *)ptr->data;
	return (me->texflag & ME_AUTOSPACE) ? 0 : PROP_EDITABLE;
}

static void rna_Mesh_texspace_size_get(PointerRNA *ptr, float values[3])
{
	Mesh *me = (Mesh *)ptr->data;

	if (me->bb == NULL || (me->bb->flag & BOUNDBOX_DIRTY)) {
		BKE_mesh_texspace_calc(me);
	}

	copy_v3_v3(values, me->size);
}

static void rna_Mesh_texspace_loc_get(PointerRNA *ptr, float values[3])
{
	Mesh *me = (Mesh *)ptr->data;

	if (me->bb == NULL || (me->bb->flag & BOUNDBOX_DIRTY)) {
		BKE_mesh_texspace_calc(me);
	}

	copy_v3_v3(values, me->loc);
}

static void rna_MeshVertex_groups_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);

	if (me->dvert) {
		MVert *mvert = (MVert *)ptr->data;
		MDeformVert *dvert = me->dvert + (mvert - me->mvert);

		rna_iterator_array_begin(iter, (void *)dvert->dw, sizeof(MDeformWeight), dvert->totweight, 0, NULL);
	}
	else
		rna_iterator_array_begin(iter, NULL, 0, 0, 0, NULL);
}

static void rna_MeshVertex_undeformed_co_get(PointerRNA *ptr, float values[3])
{
	Mesh *me = rna_mesh(ptr);
	MVert *mvert = (MVert *)ptr->data;
	float (*orco)[3] =  CustomData_get_layer(&me->vdata, CD_ORCO);

	if (orco) {
		/* orco is normalized to 0..1, we do inverse to match mvert->co */
		float loc[3], size[3];

		BKE_mesh_texspace_get(me->texcomesh ? me->texcomesh : me, loc, NULL, size);
		madd_v3_v3v3v3(values, loc, orco[(mvert - me->mvert)], size);
	}
	else
		copy_v3_v3(values, mvert->co);
}

static int rna_CustomDataLayer_active_get(PointerRNA *ptr, CustomData *data, int type, int render)
{
	int n = ((CustomDataLayer *)ptr->data) - data->layers;

	if (render) return (n == CustomData_get_render_layer_index(data, type));
	else return (n == CustomData_get_active_layer_index(data, type));
}

static int rna_CustomDataLayer_clone_get(PointerRNA *ptr, CustomData *data, int type)
{
	int n = ((CustomDataLayer *)ptr->data) - data->layers;

	return (n == CustomData_get_clone_layer_index(data, type));
}

static void rna_CustomDataLayer_active_set(PointerRNA *ptr, CustomData *data, int value, int type, int render)
{
	Mesh *me = ptr->id.data;
	int n = (((CustomDataLayer *)ptr->data) - data->layers) - CustomData_get_layer_index(data, type);

	if (value == 0)
		return;

	if (render) CustomData_set_layer_render(data, type, n);
	else CustomData_set_layer_active(data, type, n);

	/* sync loop layer */
	if (type == CD_MTEXPOLY) {
		CustomData *ldata = rna_mesh_ldata(ptr);
		if (render) CustomData_set_layer_render(ldata, CD_MLOOPUV, n);
		else CustomData_set_layer_active(ldata, CD_MLOOPUV, n);
	}

	BKE_mesh_update_customdata_pointers(me, true);
}

static void rna_CustomDataLayer_clone_set(PointerRNA *ptr, CustomData *data, int value, int type)
{
	int n = ((CustomDataLayer *)ptr->data) - data->layers;

	if (value == 0)
		return;

	CustomData_set_layer_clone_index(data, type, n);
}

/* Generic UV rename! */
static void rna_MeshUVLayer_name_set(PointerRNA *ptr, const char *name)
{
	char buf[MAX_CUSTOMDATA_LAYER_NAME];
	BLI_strncpy_utf8(buf, name, MAX_CUSTOMDATA_LAYER_NAME);
	BKE_mesh_uv_cdlayer_rename(rna_mesh(ptr), ((CustomDataLayer *)ptr->data)->name, buf, true);
}

/* uv_layers */

DEFINE_CUSTOMDATA_LAYER_COLLECTION(uv_layer, ldata, CD_MLOOPUV)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(uv_layer, ldata, CD_MLOOPUV, active, MeshUVLoopLayer)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(uv_layer, ldata, CD_MLOOPUV, clone, MeshUVLoopLayer)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(uv_layer, ldata, CD_MLOOPUV, stencil, MeshUVLoopLayer)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(uv_layer, ldata, CD_MLOOPUV, render, MeshUVLoopLayer)

/* MeshUVLoopLayer */

static char *rna_MeshUVLoopLayer_path(PointerRNA *ptr)
{
	CustomDataLayer *cdl = ptr->data;
	char name_esc[sizeof(cdl->name) * 2];
	BLI_strescape(name_esc, cdl->name, sizeof(name_esc));
	return BLI_sprintfN("uv_layers[\"%s\"]", name_esc);
}

static void rna_MeshUVLoopLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
	rna_iterator_array_begin(iter, layer->data, sizeof(MLoopUV), (me->edit_btmesh) ? 0 : me->totloop, 0, NULL);
}

static int rna_MeshUVLoopLayer_data_length(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return (me->edit_btmesh) ? 0 : me->totloop;
}

/* face uv_textures */

DEFINE_CUSTOMDATA_LAYER_COLLECTION(tessface_uv_texture, fdata, CD_MTFACE)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(tessface_uv_texture, fdata, CD_MTFACE, active, MeshTextureFaceLayer)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(tessface_uv_texture, fdata, CD_MTFACE, clone, MeshTextureFaceLayer)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(tessface_uv_texture, fdata, CD_MTFACE, stencil, MeshTextureFaceLayer)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(tessface_uv_texture, fdata, CD_MTFACE, render, MeshTextureFaceLayer)

static void rna_MeshTextureFaceLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
	rna_iterator_array_begin(iter, layer->data, sizeof(MTFace), (me->edit_btmesh) ? 0 : me->totface, 0, NULL);
}

static int rna_MeshTextureFaceLayer_data_length(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return (me->edit_btmesh) ? 0 : me->totface;
}

static int rna_MeshTextureFaceLayer_active_render_get(PointerRNA *ptr)
{
	return rna_CustomDataLayer_active_get(ptr, rna_mesh_fdata(ptr), CD_MTFACE, 1);
}

static int rna_MeshTextureFaceLayer_active_get(PointerRNA *ptr)
{
	return rna_CustomDataLayer_active_get(ptr, rna_mesh_fdata(ptr), CD_MTFACE, 0);
}

static int rna_MeshTextureFaceLayer_clone_get(PointerRNA *ptr)
{
	return rna_CustomDataLayer_clone_get(ptr, rna_mesh_fdata(ptr), CD_MTFACE);
}

static void rna_MeshTextureFaceLayer_active_render_set(PointerRNA *ptr, int value)
{
	rna_CustomDataLayer_active_set(ptr, rna_mesh_fdata(ptr), value, CD_MTFACE, 1);
}

static void rna_MeshTextureFaceLayer_active_set(PointerRNA *ptr, int value)
{
	rna_CustomDataLayer_active_set(ptr, rna_mesh_fdata(ptr), value, CD_MTFACE, 0);
}

static void rna_MeshTextureFaceLayer_clone_set(PointerRNA *ptr, int value)
{
	rna_CustomDataLayer_clone_set(ptr, rna_mesh_fdata(ptr), value, CD_MTFACE);
}

/* poly uv_textures */

DEFINE_CUSTOMDATA_LAYER_COLLECTION(uv_texture, pdata, CD_MTEXPOLY)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(uv_texture, pdata, CD_MTEXPOLY, active, MeshTexturePolyLayer)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(uv_texture, pdata, CD_MTEXPOLY, clone, MeshTexturePolyLayer)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(uv_texture, pdata, CD_MTEXPOLY, stencil, MeshTexturePolyLayer)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(uv_texture, pdata, CD_MTEXPOLY, render, MeshTexturePolyLayer)

static void rna_MeshTexturePolyLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
	rna_iterator_array_begin(iter, layer->data, sizeof(MTexPoly), (me->edit_btmesh) ? 0 : me->totpoly, 0, NULL);
}

static int rna_MeshTexturePolyLayer_data_length(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return (me->edit_btmesh) ? 0 : me->totpoly;
}

static int rna_MeshTexturePolyLayer_active_render_get(PointerRNA *ptr)
{
	return rna_CustomDataLayer_active_get(ptr, rna_mesh_pdata(ptr), CD_MTEXPOLY, 1);
}

static int rna_MeshTexturePolyLayer_active_get(PointerRNA *ptr)
{
	return rna_CustomDataLayer_active_get(ptr, rna_mesh_pdata(ptr), CD_MTEXPOLY, 0);
}

static int rna_MeshTexturePolyLayer_clone_get(PointerRNA *ptr)
{
	return rna_CustomDataLayer_clone_get(ptr, rna_mesh_pdata(ptr), CD_MTEXPOLY);
}

static void rna_MeshTexturePolyLayer_active_render_set(PointerRNA *ptr, int value)
{
	rna_CustomDataLayer_active_set(ptr, rna_mesh_pdata(ptr), value, CD_MTEXPOLY, 1);
}

static void rna_MeshTexturePolyLayer_active_set(PointerRNA *ptr, int value)
{
	rna_CustomDataLayer_active_set(ptr, rna_mesh_pdata(ptr), value, CD_MTEXPOLY, 0);
}

static void rna_MeshTexturePolyLayer_clone_set(PointerRNA *ptr, int value)
{
	rna_CustomDataLayer_clone_set(ptr, rna_mesh_pdata(ptr), value, CD_MTEXPOLY);
}

/* vertex_color_layers */

DEFINE_CUSTOMDATA_LAYER_COLLECTION(tessface_vertex_color, fdata, CD_MCOL)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(tessface_vertex_color, fdata, CD_MCOL, active, MeshColorLayer)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(tessface_vertex_color, fdata, CD_MCOL, render, MeshColorLayer)

static void rna_MeshColorLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
	rna_iterator_array_begin(iter, layer->data, sizeof(MCol) * 4, me->totface, 0, NULL);
}

static int rna_MeshColorLayer_data_length(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return me->totface;
}

static int rna_MeshColorLayer_active_render_get(PointerRNA *ptr)
{
	return rna_CustomDataLayer_active_get(ptr, rna_mesh_fdata(ptr), CD_MCOL, 1);
}

static int rna_MeshColorLayer_active_get(PointerRNA *ptr)
{
	return rna_CustomDataLayer_active_get(ptr, rna_mesh_fdata(ptr), CD_MCOL, 0);
}

static void rna_MeshColorLayer_active_render_set(PointerRNA *ptr, int value)
{
	rna_CustomDataLayer_active_set(ptr, rna_mesh_fdata(ptr), value, CD_MCOL, 1);
}

static void rna_MeshColorLayer_active_set(PointerRNA *ptr, int value)
{
	rna_CustomDataLayer_active_set(ptr, rna_mesh_fdata(ptr), value, CD_MCOL, 0);
}

DEFINE_CUSTOMDATA_LAYER_COLLECTION(vertex_color, ldata, CD_MLOOPCOL)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(vertex_color, ldata, CD_MLOOPCOL, active, MeshLoopColorLayer)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(vertex_color, ldata, CD_MLOOPCOL, render, MeshLoopColorLayer)

static void rna_MeshLoopColorLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
	rna_iterator_array_begin(iter, layer->data, sizeof(MLoopCol), (me->edit_btmesh) ? 0 : me->totloop, 0, NULL);
}

static int rna_MeshLoopColorLayer_data_length(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return (me->edit_btmesh) ? 0 : me->totloop;
}

static int rna_MeshLoopColorLayer_active_render_get(PointerRNA *ptr)
{
	return rna_CustomDataLayer_active_get(ptr, rna_mesh_ldata(ptr), CD_MLOOPCOL, 1);
}

static int rna_MeshLoopColorLayer_active_get(PointerRNA *ptr)
{
	return rna_CustomDataLayer_active_get(ptr, rna_mesh_ldata(ptr), CD_MLOOPCOL, 0);
}

static void rna_MeshLoopColorLayer_active_render_set(PointerRNA *ptr, int value)
{
	rna_CustomDataLayer_active_set(ptr, rna_mesh_ldata(ptr), value, CD_MLOOPCOL, 1);
}

static void rna_MeshLoopColorLayer_active_set(PointerRNA *ptr, int value)
{
	rna_CustomDataLayer_active_set(ptr, rna_mesh_ldata(ptr), value, CD_MLOOPCOL, 0);
}

static void rna_MeshFloatPropertyLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
	rna_iterator_array_begin(iter, layer->data, sizeof(MFloatProperty), me->totpoly, 0, NULL);
}

static int rna_MeshFloatPropertyLayer_data_length(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return me->totpoly;
}

static int rna_float_layer_check(CollectionPropertyIterator *UNUSED(iter), void *data)
{
	CustomDataLayer *layer = (CustomDataLayer *)data;
	return (layer->type != CD_PROP_FLT);
}

static void rna_Mesh_polygon_float_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	CustomData *pdata = rna_mesh_pdata(ptr);
	rna_iterator_array_begin(iter, (void *)pdata->layers, sizeof(CustomDataLayer), pdata->totlayer, 0,
	                         rna_float_layer_check);
}

static int rna_Mesh_polygon_float_layers_length(PointerRNA *ptr)
{
	return CustomData_number_of_layers(rna_mesh_pdata(ptr), CD_PROP_FLT);
}

static int rna_int_layer_check(CollectionPropertyIterator *UNUSED(iter), void *data)
{
	CustomDataLayer *layer = (CustomDataLayer *)data;
	return (layer->type != CD_PROP_INT);
}

static void rna_MeshIntPropertyLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
	rna_iterator_array_begin(iter, layer->data, sizeof(MIntProperty), me->totpoly, 0, NULL);
}

static int rna_MeshIntPropertyLayer_data_length(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return me->totpoly;
}

static void rna_Mesh_polygon_int_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	CustomData *pdata = rna_mesh_pdata(ptr);
	rna_iterator_array_begin(iter, (void *)pdata->layers, sizeof(CustomDataLayer), pdata->totlayer, 0,
	                         rna_int_layer_check);
}

static int rna_Mesh_polygon_int_layers_length(PointerRNA *ptr)
{
	return CustomData_number_of_layers(rna_mesh_pdata(ptr), CD_PROP_INT);
}

static int rna_string_layer_check(CollectionPropertyIterator *UNUSED(iter), void *data)
{
	CustomDataLayer *layer = (CustomDataLayer *)data;
	return (layer->type != CD_PROP_STR);
}

static void rna_MeshStringPropertyLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
	rna_iterator_array_begin(iter, layer->data, sizeof(MStringProperty), me->totpoly, 0, NULL);
}

static int rna_MeshStringPropertyLayer_data_length(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return me->totpoly;
}

static void rna_Mesh_polygon_string_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	CustomData *pdata = rna_mesh_pdata(ptr);
	rna_iterator_array_begin(iter, (void *)pdata->layers, sizeof(CustomDataLayer), pdata->totlayer, 0,
	                         rna_string_layer_check);
}

static int rna_Mesh_polygon_string_layers_length(PointerRNA *ptr)
{
	return CustomData_number_of_layers(rna_mesh_pdata(ptr), CD_PROP_STR);
}

/* Skin vertices */
DEFINE_CUSTOMDATA_LAYER_COLLECTION(skin_vertice, vdata, CD_MVERT_SKIN)

static char *rna_MeshSkinVertexLayer_path(PointerRNA *ptr)
{
	CustomDataLayer *cdl = ptr->data;
	char name_esc[sizeof(cdl->name) * 2];
	BLI_strescape(name_esc, cdl->name, sizeof(name_esc));
	return BLI_sprintfN("skin_vertices[\"%s\"]", name_esc);
}

static char *rna_VertCustomData_data_path(PointerRNA *ptr, const char *collection, int type);
static char *rna_MeshSkinVertex_path(PointerRNA *ptr)
{
	return rna_VertCustomData_data_path(ptr, "skin_vertices", CD_MVERT_SKIN);
}

static void rna_MeshSkinVertexLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
	rna_iterator_array_begin(iter, layer->data, sizeof(MVertSkin), me->totvert, 0, NULL);
}

static int rna_MeshSkinVertexLayer_data_length(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return me->totvert;
}

/* End skin vertices */

static void rna_TexturePoly_image_set(PointerRNA *ptr, PointerRNA value)
{
	MTexPoly *tf = (MTexPoly *)ptr->data;
	ID *id = value.data;

	if (id) {
		/* special exception here, individual faces don't count
		 * as reference, but we do ensure the refcount is not zero */
		if (id->us == 0)
			id_us_plus(id);
		else
			id_lib_extern(id);
	}

	tf->tpage = (struct Image *)id;
}

/* while this is supposed to be readonly,
 * keep it to support importers that only make tessfaces */
static void rna_TextureFace_image_set(PointerRNA *ptr, PointerRNA value)
{
	MTFace *tf = (MTFace *)ptr->data;
	ID *id = value.data;

	if (id) {
		/* special exception here, individual faces don't count
		 * as reference, but we do ensure the refcount is not zero */
		if (id->us == 0)
			id_us_plus(id);
		else
			id_lib_extern(id);
	}

	tf->tpage = (struct Image *)id;
}

static void rna_Mesh_auto_smooth_angle_set(PointerRNA *ptr, float value)
{
	Mesh *me = rna_mesh(ptr);
	value = RAD2DEGF(value);
	CLAMP(value, 1.0f, 80.0f);
	me->smoothresh = (int)value;
}

static float rna_Mesh_auto_smooth_angle_get(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return DEG2RADF((float)me->smoothresh);
}

static int rna_MeshTessFace_verts_get_length(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
	MFace *face = (MFace *)ptr->data;

	if (face)
		length[0] = (face->v4) ? 4 : 3;
	else
		length[0] = 4;  /* XXX rna_raw_access wants the length of a dummy face. this needs fixing. - Campbell */

	return length[0];
}

static void rna_MeshTessFace_verts_get(PointerRNA *ptr, int *values)
{
	MFace *face = (MFace *)ptr->data;
	memcpy(values, &face->v1, (face->v4 ? 4 : 3) * sizeof(int));
}

static void rna_MeshTessFace_verts_set(PointerRNA *ptr, const int *values)
{
	MFace *face = (MFace *)ptr->data;
	memcpy(&face->v1, values, (face->v4 ? 4 : 3) * sizeof(int));
}

/* poly.vertices - this is faked loop access for convenience */
static int rna_MeshPoly_vertices_get_length(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
	MPoly *mp = (MPoly *)ptr->data;
	/* note, raw access uses dummy item, this _could_ crash, watch out for this, mface uses it but it cant work here */
	return (length[0] = mp->totloop);
}

static void rna_MeshPoly_vertices_get(PointerRNA *ptr, int *values)
{
	Mesh *me = rna_mesh(ptr);
	MPoly *mp = (MPoly *)ptr->data;
	MLoop *ml = &me->mloop[mp->loopstart];
	unsigned int i;
	for (i = mp->totloop; i > 0; i--, values++, ml++) {
		*values = ml->v;
	}
}

static void rna_MeshPoly_vertices_set(PointerRNA *ptr, const int *values)
{
	Mesh *me = rna_mesh(ptr);
	MPoly *mp = (MPoly *)ptr->data;
	MLoop *ml = &me->mloop[mp->loopstart];
	unsigned int i;
	for (i = mp->totloop; i > 0; i--, values++, ml++) {
		ml->v = *values;
	}
}

/* disabling, some importers don't know the total material count when assigning materials */
#if 0
static void rna_MeshPoly_material_index_range(PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
	Mesh *me = rna_mesh(ptr);
	*min = 0;
	*max = max_ii(0, me->totcol - 1);
}
#endif

static int rna_MeshVertex_index_get(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	MVert *vert = (MVert *)ptr->data;
	return (int)(vert - me->mvert);
}

static int rna_MeshEdge_index_get(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	MEdge *edge = (MEdge *)ptr->data;
	return (int)(edge - me->medge);
}

static int rna_MeshTessFace_index_get(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	MFace *face = (MFace *)ptr->data;
	return (int)(face - me->mface);
}

static int rna_MeshPolygon_index_get(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	MPoly *mpoly = (MPoly *)ptr->data;
	return (int)(mpoly - me->mpoly);
}

static int rna_MeshLoop_index_get(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	MLoop *mloop = (MLoop *)ptr->data;
	return (int)(mloop - me->mloop);
}

/* path construction */

static char *rna_VertexGroupElement_path(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr); /* XXX not always! */
	MDeformWeight *dw = (MDeformWeight *)ptr->data;
	MDeformVert *dvert;
	int a, b;

	for (a = 0, dvert = me->dvert; a < me->totvert; a++, dvert++)
		for (b = 0; b < dvert->totweight; b++)
			if (dw == &dvert->dw[b])
				return BLI_sprintfN("vertices[%d].groups[%d]", a, b);

	return NULL;
}

static char *rna_MeshPolygon_path(PointerRNA *ptr)
{
	return BLI_sprintfN("polygons[%d]", (int)((MPoly *)ptr->data - rna_mesh(ptr)->mpoly));
}

static char *rna_MeshTessFace_path(PointerRNA *ptr)
{
	return BLI_sprintfN("tessfaces[%d]", (int)((MFace *)ptr->data - rna_mesh(ptr)->mface));
}

static char *rna_MeshEdge_path(PointerRNA *ptr)
{
	return BLI_sprintfN("edges[%d]", (int)((MEdge *)ptr->data - rna_mesh(ptr)->medge));
}

static char *rna_MeshLoop_path(PointerRNA *ptr)
{
	return BLI_sprintfN("loops[%d]", (int)((MLoop *)ptr->data - rna_mesh(ptr)->mloop));
}


static char *rna_MeshVertex_path(PointerRNA *ptr)
{
	return BLI_sprintfN("vertices[%d]", (int)((MVert *)ptr->data - rna_mesh(ptr)->mvert));
}

static char *rna_MeshTextureFaceLayer_path(PointerRNA *ptr)
{
	CustomDataLayer *cdl = ptr->data;
	char name_esc[sizeof(cdl->name) * 2];
	BLI_strescape(name_esc, cdl->name, sizeof(name_esc));
	return BLI_sprintfN("tessface_uv_textures[\"%s\"]", name_esc);
}

static char *rna_MeshTexturePolyLayer_path(PointerRNA *ptr)
{
	CustomDataLayer *cdl = ptr->data;
	char name_esc[sizeof(cdl->name) * 2];
	BLI_strescape(name_esc, cdl->name, sizeof(name_esc));
	return BLI_sprintfN("uv_textures[\"%s\"]", name_esc);
}

static char *rna_VertCustomData_data_path(PointerRNA *ptr, const char *collection, int type)
{
	CustomDataLayer *cdl;
	Mesh *me = rna_mesh(ptr);
	CustomData *vdata = rna_mesh_vdata(ptr);
	int a, b, totvert = (me->edit_btmesh) ? 0 : me->totvert;

	for (cdl = vdata->layers, a = 0; a < vdata->totlayer; cdl++, a++) {
		if (cdl->type == type) {
			b = ((char *)ptr->data - ((char *)cdl->data)) / CustomData_sizeof(type);
			if (b >= 0 && b < totvert) {
				char name_esc[sizeof(cdl->name) * 2];
				BLI_strescape(name_esc, cdl->name, sizeof(name_esc));
				return BLI_sprintfN("%s[\"%s\"].data[%d]", collection, name_esc, b);
			}
		}
	}

	return NULL;
}

static char *rna_PolyCustomData_data_path(PointerRNA *ptr, const char *collection, int type)
{
	CustomDataLayer *cdl;
	Mesh *me = rna_mesh(ptr);
	CustomData *pdata = rna_mesh_pdata(ptr);
	int a, b, totpoly = (me->edit_btmesh) ? 0 : me->totpoly;

	for (cdl = pdata->layers, a = 0; a < pdata->totlayer; cdl++, a++) {
		if (cdl->type == type) {
			b = ((char *)ptr->data - ((char *)cdl->data)) / CustomData_sizeof(type);
			if (b >= 0 && b < totpoly) {
				char name_esc[sizeof(cdl->name) * 2];
				BLI_strescape(name_esc, cdl->name, sizeof(name_esc));
				return BLI_sprintfN("%s[\"%s\"].data[%d]", collection, name_esc, b);
			}
		}
	}

	return NULL;
}

static char *rna_LoopCustomData_data_path(PointerRNA *ptr, const char *collection, int type)
{
	CustomDataLayer *cdl;
	Mesh *me = rna_mesh(ptr);
	CustomData *ldata = rna_mesh_ldata(ptr);
	int a, b, totloop = (me->edit_btmesh) ? 0 : me->totloop;

	for (cdl = ldata->layers, a = 0; a < ldata->totlayer; cdl++, a++) {
		if (cdl->type == type) {
			b = ((char *)ptr->data - ((char *)cdl->data)) / CustomData_sizeof(type);
			if (b >= 0 && b < totloop) {
				char name_esc[sizeof(cdl->name) * 2];
				BLI_strescape(name_esc, cdl->name, sizeof(name_esc));
				return BLI_sprintfN("%s[\"%s\"].data[%d]", collection, name_esc, b);
			}
		}
	}

	return NULL;
}

static char *rna_FaceCustomData_data_path(PointerRNA *ptr, const char *collection, int type)
{
	CustomDataLayer *cdl;
	Mesh *me = rna_mesh(ptr);
	CustomData *fdata = rna_mesh_fdata(ptr);
	int a, b, totloop = (me->edit_btmesh) ? 0 : me->totloop;

	for (cdl = fdata->layers, a = 0; a < fdata->totlayer; cdl++, a++) {
		if (cdl->type == type) {
			b = ((char *)ptr->data - ((char *)cdl->data)) / CustomData_sizeof(type);
			if (b >= 0 && b < totloop) {
				char name_esc[sizeof(cdl->name) * 2];
				BLI_strescape(name_esc, cdl->name, sizeof(name_esc));
				return BLI_sprintfN("%s[\"%s\"].data[%d]", collection, name_esc, b);
			}
		}
	}

	return NULL;
}




static char *rna_MeshUVLoop_path(PointerRNA *ptr)
{
	return rna_LoopCustomData_data_path(ptr, "uv_layers", CD_MLOOPUV);
}

static char *rna_MeshTextureFace_path(PointerRNA *ptr)
{
	return rna_FaceCustomData_data_path(ptr, "tessface_uv_textures", CD_MTFACE);
}

static char *rna_MeshTexturePoly_path(PointerRNA *ptr)
{
	return rna_PolyCustomData_data_path(ptr, "uv_textures", CD_MTEXPOLY);
}

static char *rna_MeshColorLayer_path(PointerRNA *ptr)
{
	CustomDataLayer *cdl = ptr->data;
	char name_esc[sizeof(cdl->name) * 2];
	BLI_strescape(name_esc, cdl->name, sizeof(name_esc));
	return BLI_sprintfN("tessface_vertex_colors[\"%s\"]", name_esc);
}

static char *rna_MeshLoopColorLayer_path(PointerRNA *ptr)
{
	CustomDataLayer *cdl = ptr->data;
	char name_esc[sizeof(cdl->name) * 2];
	BLI_strescape(name_esc, cdl->name, sizeof(name_esc));
	return BLI_sprintfN("vertex_colors[\"%s\"]", name_esc);
}

static char *rna_MeshColor_path(PointerRNA *ptr)
{
	return rna_LoopCustomData_data_path(ptr, "vertex_colors", CD_MLOOPCOL);
}

static char *rna_MeshIntPropertyLayer_path(PointerRNA *ptr)
{
	CustomDataLayer *cdl = ptr->data;
	char name_esc[sizeof(cdl->name) * 2];
	BLI_strescape(name_esc, cdl->name, sizeof(name_esc));
	return BLI_sprintfN("int_layers[\"%s\"]", name_esc);
}

static char *rna_MeshIntProperty_path(PointerRNA *ptr)
{
	return rna_PolyCustomData_data_path(ptr, "layers_int", CD_PROP_INT);
}

static char *rna_MeshFloatPropertyLayer_path(PointerRNA *ptr)
{
	CustomDataLayer *cdl = ptr->data;
	char name_esc[sizeof(cdl->name) * 2];
	BLI_strescape(name_esc, cdl->name, sizeof(name_esc));
	return BLI_sprintfN("float_layers[\"%s\"]", name_esc);
}

static char *rna_MeshFloatProperty_path(PointerRNA *ptr)
{
	return rna_PolyCustomData_data_path(ptr, "layers_float", CD_PROP_FLT);
}

static char *rna_MeshStringPropertyLayer_path(PointerRNA *ptr)
{
	CustomDataLayer *cdl = ptr->data;
	char name_esc[sizeof(cdl->name) * 2];
	BLI_strescape(name_esc, cdl->name, sizeof(name_esc));
	return BLI_sprintfN("string_layers[\"%s\"]", name_esc);
}

static char *rna_MeshStringProperty_path(PointerRNA *ptr)
{
	return rna_PolyCustomData_data_path(ptr, "layers_string", CD_PROP_STR);
}

/* XXX, we dont have propper byte string support yet, so for now use the (bytes + 1)
 * bmesh API exposes correct python/bytestring access */
void rna_MeshStringProperty_s_get(PointerRNA *ptr, char *value)
{
	MStringProperty *ms = (MStringProperty *)ptr->data;
	BLI_strncpy(value, ms->s, (int)ms->s_len + 1);
}

int rna_MeshStringProperty_s_length(PointerRNA *ptr)
{
	MStringProperty *ms = (MStringProperty *)ptr->data;
	return (int)ms->s_len + 1;
}

void rna_MeshStringProperty_s_set(PointerRNA *ptr, const char *value)
{
	MStringProperty *ms = (MStringProperty *)ptr->data;
	BLI_strncpy(ms->s, value, sizeof(ms->s));
}

static int rna_Mesh_tot_vert_get(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return me->edit_btmesh ? me->edit_btmesh->bm->totvertsel : 0;
}
static int rna_Mesh_tot_edge_get(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return me->edit_btmesh ? me->edit_btmesh->bm->totedgesel : 0;
}
static int rna_Mesh_tot_face_get(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return me->edit_btmesh ? me->edit_btmesh->bm->totfacesel : 0;
}

static PointerRNA rna_Mesh_vertex_color_new(struct Mesh *me, const char *name)
{
	PointerRNA ptr;
	CustomData *ldata;
	CustomDataLayer *cdl = NULL;
	int index = ED_mesh_color_add(me, name, false);

	if (index != -1) {
		ldata = rna_mesh_ldata_helper(me);
		cdl = &ldata->layers[CustomData_get_layer_index_n(ldata, CD_MLOOPCOL, index)];
	}

	RNA_pointer_create(&me->id, &RNA_MeshLoopColorLayer, cdl, &ptr);
	return ptr;
}

static void rna_Mesh_vertex_color_remove(struct Mesh *me, ReportList *reports, CustomDataLayer *layer)
{
	if (ED_mesh_color_remove_named(me, layer->name) == false) {
		BKE_reportf(reports, RPT_ERROR, "Vertex color '%s' not found", layer->name);
	}
}

static PointerRNA rna_Mesh_tessface_vertex_color_new(struct Mesh *me, ReportList *reports, const char *name)
{
	PointerRNA ptr;
	CustomData *fdata;
	CustomDataLayer *cdl = NULL;
	int index;

	if (me->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Cannot add tessface colors in edit mode");
		return PointerRNA_NULL;
	}

	if (me->mpoly) {
		BKE_report(reports, RPT_ERROR, "Cannot add tessface colors when MPoly's exist");
		return PointerRNA_NULL;
	}

	index = ED_mesh_color_add(me, name, false);

	if (index != -1) {
		fdata = rna_mesh_fdata_helper(me);
		cdl = &fdata->layers[CustomData_get_layer_index_n(fdata, CD_MCOL, index)];
	}

	RNA_pointer_create(&me->id, &RNA_MeshColorLayer, cdl, &ptr);
	return ptr;
}

static PointerRNA rna_Mesh_polygon_int_property_new(struct Mesh *me, const char *name)
{
	PointerRNA ptr;
	CustomDataLayer *cdl = NULL;
	int index;

	CustomData_add_layer_named(&me->pdata, CD_PROP_INT, CD_DEFAULT, NULL, me->totpoly, name);
	index = CustomData_get_named_layer_index(&me->pdata, CD_PROP_INT, name);

	cdl = (index == -1) ? NULL : &(me->pdata.layers[index]);

	RNA_pointer_create(&me->id, &RNA_MeshIntPropertyLayer, cdl, &ptr);
	return ptr;
}

static PointerRNA rna_Mesh_polygon_float_property_new(struct Mesh *me, const char *name)
{
	PointerRNA ptr;
	CustomDataLayer *cdl = NULL;
	int index;

	CustomData_add_layer_named(&me->pdata, CD_PROP_FLT, CD_DEFAULT, NULL, me->totpoly, name);
	index = CustomData_get_named_layer_index(&me->pdata, CD_PROP_FLT, name);

	cdl = (index == -1) ? NULL : &(me->pdata.layers[index]);

	RNA_pointer_create(&me->id, &RNA_MeshFloatPropertyLayer, cdl, &ptr);
	return ptr;
}

static PointerRNA rna_Mesh_polygon_string_property_new(struct Mesh *me, const char *name)
{
	PointerRNA ptr;
	CustomDataLayer *cdl = NULL;
	int index;

	CustomData_add_layer_named(&me->pdata, CD_PROP_STR, CD_DEFAULT, NULL, me->totpoly, name);
	index = CustomData_get_named_layer_index(&me->pdata, CD_PROP_STR, name);

	cdl = (index == -1) ? NULL : &(me->pdata.layers[index]);

	RNA_pointer_create(&me->id, &RNA_MeshStringPropertyLayer, cdl, &ptr);
	return ptr;
}

static PointerRNA rna_Mesh_uv_texture_new(struct Mesh *me, const char *name)
{
	PointerRNA ptr;
	CustomData *pdata;
	CustomDataLayer *cdl = NULL;
	int index = ED_mesh_uv_texture_add(me, name, false);

	if (index != -1) {
		pdata = rna_mesh_pdata_helper(me);
		cdl = &pdata->layers[CustomData_get_layer_index_n(pdata, CD_MTEXPOLY, index)];
	}

	RNA_pointer_create(&me->id, &RNA_MeshTexturePolyLayer, cdl, &ptr);
	return ptr;
}

static void rna_Mesh_uv_texture_layers_remove(struct Mesh *me, ReportList *reports, CustomDataLayer *layer)
{
	if (ED_mesh_uv_texture_remove_named(me, layer->name) == false) {
		BKE_reportf(reports, RPT_ERROR, "Texture layer '%s' not found", layer->name);
	}
}

/* while this is supposed to be readonly,
 * keep it to support importers that only make tessfaces */

static PointerRNA rna_Mesh_tessface_uv_texture_new(struct Mesh *me, ReportList *reports, const char *name)
{
	PointerRNA ptr;
	CustomData *fdata;
	CustomDataLayer *cdl = NULL;
	int index;

	if (me->edit_btmesh) {
		BKE_report(reports, RPT_ERROR, "Cannot add tessface uv's in edit mode");
		return PointerRNA_NULL;
	}

	if (me->mpoly) {
		BKE_report(reports, RPT_ERROR, "Cannot add tessface uv's when MPoly's exist");
		return PointerRNA_NULL;
	}

	index = ED_mesh_uv_texture_add(me, name, false);

	if (index != -1) {
		fdata = rna_mesh_fdata_helper(me);
		cdl = &fdata->layers[CustomData_get_layer_index_n(fdata, CD_MTFACE, index)];
	}

	RNA_pointer_create(&me->id, &RNA_MeshTextureFaceLayer, cdl, &ptr);
	return ptr;
}


static int rna_Mesh_is_editmode_get(PointerRNA *ptr)
{
	Mesh *me = rna_mesh(ptr);
	return (me->edit_btmesh != NULL);
}

/* only to quiet warnings */
static void UNUSED_FUNCTION(rna_mesh_unused)(void)
{
	/* unused functions made by macros */
	(void)rna_Mesh_skin_vertice_index_range;
	(void)rna_Mesh_tessface_uv_texture_active_set;
	(void)rna_Mesh_tessface_uv_texture_clone_get;
	(void)rna_Mesh_tessface_uv_texture_clone_index_get;
	(void)rna_Mesh_tessface_uv_texture_clone_index_set;
	(void)rna_Mesh_tessface_uv_texture_clone_set;
	(void)rna_Mesh_tessface_uv_texture_index_range;
	(void)rna_Mesh_tessface_uv_texture_render_get;
	(void)rna_Mesh_tessface_uv_texture_render_index_get;
	(void)rna_Mesh_tessface_uv_texture_render_index_set;
	(void)rna_Mesh_tessface_uv_texture_render_set;
	(void)rna_Mesh_tessface_uv_texture_stencil_get;
	(void)rna_Mesh_tessface_uv_texture_stencil_index_get;
	(void)rna_Mesh_tessface_uv_texture_stencil_index_set;
	(void)rna_Mesh_tessface_uv_texture_stencil_set;
	(void)rna_Mesh_tessface_vertex_color_active_set;
	(void)rna_Mesh_tessface_vertex_color_index_range;
	(void)rna_Mesh_tessface_vertex_color_render_get;
	(void)rna_Mesh_tessface_vertex_color_render_index_get;
	(void)rna_Mesh_tessface_vertex_color_render_index_set;
	(void)rna_Mesh_tessface_vertex_color_render_set;
	(void)rna_Mesh_uv_layer_render_get;
	(void)rna_Mesh_uv_layer_render_index_get;
	(void)rna_Mesh_uv_layer_render_index_set;
	(void)rna_Mesh_uv_layer_render_set;
	(void)rna_Mesh_uv_texture_render_get;
	(void)rna_Mesh_uv_texture_render_index_get;
	(void)rna_Mesh_uv_texture_render_index_set;
	(void)rna_Mesh_uv_texture_render_set;
	(void)rna_Mesh_vertex_color_render_get;
	(void)rna_Mesh_vertex_color_render_index_get;
	(void)rna_Mesh_vertex_color_render_index_set;
	(void)rna_Mesh_vertex_color_render_set;
	/* end unused function block */
}

#else

static void rna_def_mvert_group(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "VertexGroupElement", NULL);
	RNA_def_struct_sdna(srna, "MDeformWeight");
	RNA_def_struct_path_func(srna, "rna_VertexGroupElement_path");
	RNA_def_struct_ui_text(srna, "Vertex Group Element", "Weight value of a vertex in a vertex group");
	RNA_def_struct_ui_icon(srna, ICON_GROUP_VERTEX);

	/* we can't point to actual group, it is in the object and so
	 * there is no unique group to point to, hence the index */
	prop = RNA_def_property(srna, "group", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "def_nr");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Group Index", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Weight", "Vertex Weight");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
}

static void rna_def_mvert(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MeshVertex", NULL);
	RNA_def_struct_sdna(srna, "MVert");
	RNA_def_struct_ui_text(srna, "Mesh Vertex", "Vertex in a Mesh datablock");
	RNA_def_struct_path_func(srna, "rna_MeshVertex_path");
	RNA_def_struct_ui_icon(srna, ICON_VERTEXSEL);

	prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_ui_text(prop, "Location", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "normal", PROP_FLOAT, PROP_DIRECTION);
	/* RNA_def_property_float_sdna(prop, NULL, "no"); */
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_float_funcs(prop, "rna_MeshVertex_normal_get", "rna_MeshVertex_normal_set", NULL);
	RNA_def_property_ui_text(prop, "Normal", "Vertex Normal");

	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SELECT);
	RNA_def_property_ui_text(prop, "Select", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_HIDE);
	RNA_def_property_ui_text(prop, "Hide", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

	prop = RNA_def_property(srna, "bevel_weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_MeshVertex_bevel_weight_get", "rna_MeshVertex_bevel_weight_set", NULL);
	RNA_def_property_ui_text(prop, "Bevel Weight", "Weight used by the Bevel modifier 'Only Vertices' option");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "groups", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_MeshVertex_groups_begin", "rna_iterator_array_next",
	                                  "rna_iterator_array_end", "rna_iterator_array_get", NULL, NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "VertexGroupElement");
	RNA_def_property_ui_text(prop, "Groups", "Weights for the vertex groups this vertex is member of");

	prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_MeshVertex_index_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Index", "Index of this vertex");

	prop = RNA_def_property(srna, "undeformed_co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Undeformed Location",
	                         "For meshes with modifiers applied, the coordinate of the vertex with no deforming "
	                         "modifiers applied, as used for generated texture coordinates");
	RNA_def_property_float_funcs(prop, "rna_MeshVertex_undeformed_co_get", NULL, NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_medge(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MeshEdge", NULL);
	RNA_def_struct_sdna(srna, "MEdge");
	RNA_def_struct_ui_text(srna, "Mesh Edge", "Edge in a Mesh datablock");
	RNA_def_struct_path_func(srna, "rna_MeshEdge_path");
	RNA_def_struct_ui_icon(srna, ICON_EDGESEL);

	prop = RNA_def_property(srna, "vertices", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "v1");
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_text(prop, "Vertices", "Vertex indices");
	/* XXX allows creating invalid meshes */

	prop = RNA_def_property(srna, "crease", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_MEdge_crease_get", "rna_MEdge_crease_set", NULL);
	RNA_def_property_ui_text(prop, "Crease", "Weight used by the Subsurf modifier for creasing");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "bevel_weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_MEdge_bevel_weight_get", "rna_MEdge_bevel_weight_set", NULL);
	RNA_def_property_ui_text(prop, "Bevel Weight", "Weight used by the Bevel modifier");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SELECT);
	RNA_def_property_ui_text(prop, "Select", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_HIDE);
	RNA_def_property_ui_text(prop, "Hide", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

	prop = RNA_def_property(srna, "use_seam", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_SEAM);
	RNA_def_property_ui_text(prop, "Seam", "Seam edge for UV unwrapping");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

	prop = RNA_def_property(srna, "use_edge_sharp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_SHARP);
	RNA_def_property_ui_text(prop, "Sharp", "Sharp edge for the EdgeSplit modifier");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "is_loose", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_LOOSEEDGE);
	RNA_def_property_ui_text(prop, "Loose", "Loose edge");

	prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_MeshEdge_index_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Index", "Index of this edge");
}

static void rna_def_mface(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MeshTessFace", NULL);
	RNA_def_struct_sdna(srna, "MFace");
	RNA_def_struct_ui_text(srna, "Mesh TessFace", "TessFace in a Mesh datablock");
	RNA_def_struct_path_func(srna, "rna_MeshTessFace_path");
	RNA_def_struct_ui_icon(srna, ICON_FACESEL);

	/* XXX allows creating invalid meshes */
	prop = RNA_def_property(srna, "vertices", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_array(prop, 4);
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_dynamic_array_funcs(prop, "rna_MeshTessFace_verts_get_length");
	RNA_def_property_int_funcs(prop, "rna_MeshTessFace_verts_get", "rna_MeshTessFace_verts_set", NULL);
	RNA_def_property_ui_text(prop, "Vertices", "Vertex indices");

	/* leaving this fixed size array for foreach_set used in import scripts */
	prop = RNA_def_property(srna, "vertices_raw", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "v1");
	RNA_def_property_array(prop, 4);
	RNA_def_property_ui_text(prop, "Vertices", "Fixed size vertex indices array");

	prop = RNA_def_property(srna, "material_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "mat_nr");
	RNA_def_property_ui_text(prop, "Material Index", "");
#if 0
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_MeshPoly_material_index_range"); /* reuse for tessface is ok */
#endif
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_FACE_SEL);
	RNA_def_property_ui_text(prop, "Select", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_HIDE);
	RNA_def_property_ui_text(prop, "Hide", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

	prop = RNA_def_property(srna, "use_smooth", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_SMOOTH);
	RNA_def_property_ui_text(prop, "Smooth", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "normal", PROP_FLOAT, PROP_DIRECTION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_MeshTessFace_normal_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Face Normal", "Local space unit length normal vector for this face");

	prop = RNA_def_property(srna, "area", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_MeshTessFace_area_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Face Area", "Read only area of this face");

	prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_MeshTessFace_index_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Index", "Index of this face");
}


static void rna_def_mloop(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MeshLoop", NULL);
	RNA_def_struct_sdna(srna, "MLoop");
	RNA_def_struct_ui_text(srna, "Mesh Loop", "Loop in a Mesh datablock");
	RNA_def_struct_path_func(srna, "rna_MeshLoop_path");
	RNA_def_struct_ui_icon(srna, ICON_EDGESEL);

	prop = RNA_def_property(srna, "vertex_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "v");
	RNA_def_property_ui_text(prop, "Vertex", "Vertex index");

	prop = RNA_def_property(srna, "edge_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "e");
	RNA_def_property_ui_text(prop, "Edge", "Edge index");

	prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_MeshLoop_index_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Index", "Index of this loop");

	prop = RNA_def_property(srna, "normal", PROP_FLOAT, PROP_DIRECTION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_MeshLoop_normal_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Normal",
	                         "Local space unit length split normal vector of this vertex for this polygon "
	                         "(must be computed beforehand using calc_normals_split or calc_tangents)");

	prop = RNA_def_property(srna, "tangent", PROP_FLOAT, PROP_DIRECTION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_MeshLoop_tangent_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Tangent",
	                         "Local space unit length tangent vector of this vertex for this polygon "
	                         "(must be computed beforehand using calc_tangents)");

	prop = RNA_def_property(srna, "bitangent_sign", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_MeshLoop_bitangent_sign_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Bitangent Sign",
	                         "Sign of the bitangent vector of this vertex for this polygon (must be computed "
	                         "beforehand using calc_tangents, bitangent = bitangent_sign * cross(normal, tangent))");

	prop = RNA_def_property(srna, "bitangent", PROP_FLOAT, PROP_DIRECTION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_MeshLoop_bitangent_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Bitangent",
	                         "Bitangent vector of this vertex for this polygon (must be computed beforehand using "
	                         "calc_tangents, *use it only if really needed*, slower access than bitangent_sign)");
}

static void rna_def_mpolygon(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MeshPolygon", NULL);
	RNA_def_struct_sdna(srna, "MPoly");
	RNA_def_struct_ui_text(srna, "Mesh Polygon", "Polygon in a Mesh datablock");
	RNA_def_struct_path_func(srna, "rna_MeshPolygon_path");
	RNA_def_struct_ui_icon(srna, ICON_FACESEL);

	/* Faked, actually access to loop vertex values, don't this way because manually setting up
	 * vertex/edge per loop is very low level.
	 * Instead we setup poly sizes, assign indices, then calc edges automatic when creating
	 * meshes from rna/py. */
	prop = RNA_def_property(srna, "vertices", PROP_INT, PROP_UNSIGNED);
	/* Eek, this is still used in some cases but in fact we don't want to use it at all here. */
	RNA_def_property_array(prop, 3);
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_dynamic_array_funcs(prop, "rna_MeshPoly_vertices_get_length");
	RNA_def_property_int_funcs(prop, "rna_MeshPoly_vertices_get", "rna_MeshPoly_vertices_set", NULL);
	RNA_def_property_ui_text(prop, "Vertices", "Vertex indices");

	/* these are both very low level access */
	prop = RNA_def_property(srna, "loop_start", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "loopstart");
	RNA_def_property_ui_text(prop, "Loop Start", "Index of the first loop of this polygon");
	/* also low level */
	prop = RNA_def_property(srna, "loop_total", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "totloop");
	RNA_def_property_ui_text(prop, "Loop Total", "Number of loops used by this polygon");

	prop = RNA_def_property(srna, "material_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "mat_nr");
	RNA_def_property_ui_text(prop, "Material Index", "");
#if 0
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_MeshPoly_material_index_range");
#endif
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_FACE_SEL);
	RNA_def_property_ui_text(prop, "Select", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_HIDE);
	RNA_def_property_ui_text(prop, "Hide", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

	prop = RNA_def_property(srna, "use_smooth", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_SMOOTH);
	RNA_def_property_ui_text(prop, "Smooth", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "normal", PROP_FLOAT, PROP_DIRECTION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_MeshPolygon_normal_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Polygon Normal", "Local space unit length normal vector for this polygon");

	prop = RNA_def_property(srna, "center", PROP_FLOAT, PROP_NONE);
	RNA_def_property_array(prop, 3);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_MeshPolygon_center_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Polygon Center", "Center of this polygon");

	prop = RNA_def_property(srna, "area", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_MeshPolygon_area_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Polygon Area", "Read only area of this polygon");

	prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_MeshPolygon_index_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Index", "Index of this polygon");
}

/* mesh.loop_uvs */
static void rna_def_mloopuv(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MeshUVLoopLayer", NULL);
	RNA_def_struct_sdna(srna, "CustomDataLayer");
	RNA_def_struct_path_func(srna, "rna_MeshUVLoopLayer_path");

	prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshUVLoop");
	RNA_def_property_collection_funcs(prop, "rna_MeshUVLoopLayer_data_begin", "rna_iterator_array_next",
	                                  "rna_iterator_array_end", "rna_iterator_array_get",
	                                  "rna_MeshUVLoopLayer_data_length", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshUVLayer_name_set");
	RNA_def_property_ui_text(prop, "Name", "Name of UV map");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	srna = RNA_def_struct(brna, "MeshUVLoop", NULL);
	RNA_def_struct_sdna(srna, "MLoopUV");
	RNA_def_struct_path_func(srna, "rna_MeshUVLoop_path");

	prop = RNA_def_property(srna, "uv", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "pin_uv", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MLOOPUV_PINNED);
	RNA_def_property_ui_text(prop, "UV Pinned", "");

	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MLOOPUV_VERTSEL);
	RNA_def_property_ui_text(prop, "UV Select", "");

	prop = RNA_def_property(srna, "select_edge", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MLOOPUV_EDGESEL);
	RNA_def_property_ui_text(prop, "UV Edge Select", "");
}

static void rna_def_mtface(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	const int uv_dim[] = {4, 2};

	srna = RNA_def_struct(brna, "MeshTextureFaceLayer", NULL);
	RNA_def_struct_ui_text(srna, "Mesh UV Map", "UV map with assigned image textures in a Mesh datablock");
	RNA_def_struct_sdna(srna, "CustomDataLayer");
	RNA_def_struct_path_func(srna, "rna_MeshTextureFaceLayer_path");
	RNA_def_struct_ui_icon(srna, ICON_GROUP_UVS);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshUVLayer_name_set");
	RNA_def_property_ui_text(prop, "Name", "Name of UV map");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_MeshTextureFaceLayer_active_get", "rna_MeshTextureFaceLayer_active_set");
	RNA_def_property_ui_text(prop, "Active", "Set the map as active for display and editing");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "active_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "active_rnd", 0);
	RNA_def_property_boolean_funcs(prop, "rna_MeshTextureFaceLayer_active_render_get",
	                               "rna_MeshTextureFaceLayer_active_render_set");
	RNA_def_property_ui_text(prop, "Active Render", "Set the map as active for rendering");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "active_clone", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "active_clone", 0);
	RNA_def_property_boolean_funcs(prop, "rna_MeshTextureFaceLayer_clone_get", "rna_MeshTextureFaceLayer_clone_set");
	RNA_def_property_ui_text(prop, "Active Clone", "Set the map as active for cloning");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshTextureFace");
	RNA_def_property_ui_text(prop, "Data", "");
	RNA_def_property_collection_funcs(prop, "rna_MeshTextureFaceLayer_data_begin", "rna_iterator_array_next",
	                                  "rna_iterator_array_end", "rna_iterator_array_get",
	                                  "rna_MeshTextureFaceLayer_data_length", NULL, NULL, NULL);

	srna = RNA_def_struct(brna, "MeshTextureFace", NULL);
	RNA_def_struct_sdna(srna, "MTFace");
	RNA_def_struct_ui_text(srna, "Mesh UV Map Face", "UV map and image texture for a face");
	RNA_def_struct_path_func(srna, "rna_MeshTextureFace_path");
	RNA_def_struct_ui_icon(srna, ICON_FACESEL_HLT);

	prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tpage");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_TextureFace_image_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Image", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	/* these are for editing only, access at loops now */
#if 0
	prop = RNA_def_property(srna, "select_uv", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TF_SEL1);
	RNA_def_property_array(prop, 4);
	RNA_def_property_ui_text(prop, "UV Selected", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

	prop = RNA_def_property(srna, "pin_uv", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "unwrap", TF_PIN1);
	RNA_def_property_array(prop, 4);
	RNA_def_property_ui_text(prop, "UV Pinned", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_select");
#endif

	prop = RNA_def_property(srna, "uv1", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_funcs(prop, "rna_MeshTextureFace_uv1_get", "rna_MeshTextureFace_uv1_set", NULL);
	RNA_def_property_ui_text(prop, "UV 1", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "uv2", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_funcs(prop, "rna_MeshTextureFace_uv2_get", "rna_MeshTextureFace_uv2_set", NULL);
	RNA_def_property_ui_text(prop, "UV 2", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "uv3", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_funcs(prop, "rna_MeshTextureFace_uv3_get", "rna_MeshTextureFace_uv3_set", NULL);
	RNA_def_property_ui_text(prop, "UV 3", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "uv4", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_funcs(prop, "rna_MeshTextureFace_uv4_get", "rna_MeshTextureFace_uv4_set", NULL);
	RNA_def_property_ui_text(prop, "UV 4", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "uv", PROP_FLOAT, PROP_NONE);
	RNA_def_property_multi_array(prop, 2, uv_dim);
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_dynamic_array_funcs(prop, "rna_MeshTextureFace_uv_get_length");
	RNA_def_property_float_funcs(prop, "rna_MeshTextureFace_uv_get", "rna_MeshTextureFace_uv_set", NULL);
	RNA_def_property_ui_text(prop, "UV", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "uv_raw", PROP_FLOAT, PROP_NONE);
	RNA_def_property_multi_array(prop, 2, uv_dim);
	RNA_def_property_float_sdna(prop, NULL, "uv");
	RNA_def_property_ui_text(prop, "UV Raw", "Fixed size UV coordinates array");

}

static void rna_def_mtexpoly(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
#if 0  /* BMESH_TODO: needed later when do another todo */
	int uv_dim[] = {4, 2};
#endif

	srna = RNA_def_struct(brna, "MeshTexturePolyLayer", NULL);
	RNA_def_struct_ui_text(srna, "Mesh UV Map", "UV map with assigned image textures in a Mesh datablock");
	RNA_def_struct_sdna(srna, "CustomDataLayer");
	RNA_def_struct_path_func(srna, "rna_MeshTexturePolyLayer_path");
	RNA_def_struct_ui_icon(srna, ICON_GROUP_UVS);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshUVLayer_name_set");
	RNA_def_property_ui_text(prop, "Name", "Name of UV map");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_MeshTexturePolyLayer_active_get", "rna_MeshTexturePolyLayer_active_set");
	RNA_def_property_ui_text(prop, "Active", "Set the map as active for display and editing");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "active_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "active_rnd", 0);
	RNA_def_property_boolean_funcs(prop, "rna_MeshTexturePolyLayer_active_render_get",
	                               "rna_MeshTexturePolyLayer_active_render_set");
	RNA_def_property_ui_text(prop, "Active Render", "Set the map as active for rendering");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "active_clone", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "active_clone", 0);
	RNA_def_property_boolean_funcs(prop, "rna_MeshTexturePolyLayer_clone_get", "rna_MeshTexturePolyLayer_clone_set");
	RNA_def_property_ui_text(prop, "Active Clone", "Set the map as active for cloning");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshTexturePoly");
	RNA_def_property_ui_text(prop, "Data", "");
	RNA_def_property_collection_funcs(prop, "rna_MeshTexturePolyLayer_data_begin", "rna_iterator_array_next",
	                                  "rna_iterator_array_end", "rna_iterator_array_get",
	                                  "rna_MeshTexturePolyLayer_data_length", NULL, NULL, NULL);

	srna = RNA_def_struct(brna, "MeshTexturePoly", NULL);
	RNA_def_struct_sdna(srna, "MTexPoly");
	RNA_def_struct_ui_text(srna, "Mesh UV Map Face", "UV map and image texture for a face");
	RNA_def_struct_path_func(srna, "rna_MeshTexturePoly_path");
	RNA_def_struct_ui_icon(srna, ICON_FACESEL_HLT);

	prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tpage");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_TexturePoly_image_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Image", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

#if 0 /* moved to MeshUVLoopLayer */
	prop = RNA_def_property(srna, "select_uv", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TF_SEL1);
	RNA_def_property_array(prop, 4);
	RNA_def_property_ui_text(prop, "UV Selected", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

	prop = RNA_def_property(srna, "pin_uv", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "unwrap", TF_PIN1);
	RNA_def_property_array(prop, 4);
	RNA_def_property_ui_text(prop, "UV Pinned", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_select");

	prop = RNA_def_property(srna, "uv_raw", PROP_FLOAT, PROP_NONE);
	RNA_def_property_multi_array(prop, 2, uv_dim);
	RNA_def_property_float_sdna(prop, NULL, "uv");
	RNA_def_property_ui_text(prop, "UV", "Fixed size UV coordinates array");
#endif
}

static void rna_def_mcol(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MeshColorLayer", NULL);
	RNA_def_struct_ui_text(srna, "Mesh Vertex Color Layer", "Layer of vertex colors in a Mesh datablock");
	RNA_def_struct_sdna(srna, "CustomDataLayer");
	RNA_def_struct_path_func(srna, "rna_MeshColorLayer_path");
	RNA_def_struct_ui_icon(srna, ICON_GROUP_VCOL);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_string_funcs(prop, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Name", "Name of Vertex color layer");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_MeshColorLayer_active_get", "rna_MeshColorLayer_active_set");
	RNA_def_property_ui_text(prop, "Active", "Sets the layer as active for display and editing");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "active_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "active_rnd", 0);
	RNA_def_property_boolean_funcs(prop, "rna_MeshColorLayer_active_render_get",
	                               "rna_MeshColorLayer_active_render_set");
	RNA_def_property_ui_text(prop, "Active Render", "Sets the layer as active for rendering");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshColor");
	RNA_def_property_ui_text(prop, "Data", "");
	RNA_def_property_collection_funcs(prop, "rna_MeshColorLayer_data_begin", "rna_iterator_array_next",
	                                  "rna_iterator_array_end", "rna_iterator_array_get",
	                                  "rna_MeshColorLayer_data_length", NULL, NULL, NULL);

	srna = RNA_def_struct(brna, "MeshColor", NULL);
	RNA_def_struct_sdna(srna, "MCol");
	RNA_def_struct_ui_text(srna, "Mesh Vertex Color", "Vertex colors for a face in a Mesh");
	RNA_def_struct_path_func(srna, "rna_MeshColor_path");

	prop = RNA_def_property(srna, "color1", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_float_funcs(prop, "rna_MeshColor_color1_get", "rna_MeshColor_color1_set", NULL);
	RNA_def_property_ui_text(prop, "Color 1", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "color2", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_float_funcs(prop, "rna_MeshColor_color2_get", "rna_MeshColor_color2_set", NULL);
	RNA_def_property_ui_text(prop, "Color 2", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "color3", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_float_funcs(prop, "rna_MeshColor_color3_get", "rna_MeshColor_color3_set", NULL);
	RNA_def_property_ui_text(prop, "Color 3", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "color4", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_float_funcs(prop, "rna_MeshColor_color4_get", "rna_MeshColor_color4_set", NULL);
	RNA_def_property_ui_text(prop, "Color 4", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
}

static void rna_def_mloopcol(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MeshLoopColorLayer", NULL);
	RNA_def_struct_ui_text(srna, "Mesh Vertex Color Layer", "Layer of vertex colors in a Mesh datablock");
	RNA_def_struct_sdna(srna, "CustomDataLayer");
	RNA_def_struct_path_func(srna, "rna_MeshLoopColorLayer_path");
	RNA_def_struct_ui_icon(srna, ICON_GROUP_VCOL);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshLoopLayer_name_set");
	RNA_def_property_ui_text(prop, "Name", "Name of Vertex color layer");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_MeshLoopColorLayer_active_get", "rna_MeshLoopColorLayer_active_set");
	RNA_def_property_ui_text(prop, "Active", "Sets the layer as active for display and editing");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "active_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "active_rnd", 0);
	RNA_def_property_boolean_funcs(prop, "rna_MeshLoopColorLayer_active_render_get",
	                               "rna_MeshLoopColorLayer_active_render_set");
	RNA_def_property_ui_text(prop, "Active Render", "Sets the layer as active for rendering");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshLoopColor");
	RNA_def_property_ui_text(prop, "Data", "");
	RNA_def_property_collection_funcs(prop, "rna_MeshLoopColorLayer_data_begin", "rna_iterator_array_next",
	                                  "rna_iterator_array_end", "rna_iterator_array_get",
	                                  "rna_MeshLoopColorLayer_data_length", NULL, NULL, NULL);


	srna = RNA_def_struct(brna, "MeshLoopColor", NULL);
	RNA_def_struct_sdna(srna, "MLoopCol");
	RNA_def_struct_ui_text(srna, "Mesh Vertex Color", "Vertex loop colors in a Mesh");
	RNA_def_struct_path_func(srna, "rna_MeshColor_path");

	prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_float_funcs(prop, "rna_MeshLoopColor_color_get", "rna_MeshLoopColor_color_set", NULL);
	RNA_def_property_ui_text(prop, "Color", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
}

static void rna_def_mproperties(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* Float */
	srna = RNA_def_struct(brna, "MeshFloatPropertyLayer", NULL);
	RNA_def_struct_sdna(srna, "CustomDataLayer");
	RNA_def_struct_ui_text(srna, "Mesh Float Property Layer", "User defined layer of floating point number values");
	RNA_def_struct_path_func(srna, "rna_MeshFloatPropertyLayer_path");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshAnyLayer_name_set");
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshFloatProperty");
	RNA_def_property_ui_text(prop, "Data", "");
	RNA_def_property_collection_funcs(prop, "rna_MeshFloatPropertyLayer_data_begin", "rna_iterator_array_next",
	                                  "rna_iterator_array_end", "rna_iterator_array_get",
	                                  "rna_MeshFloatPropertyLayer_data_length", NULL, NULL, NULL);

	srna = RNA_def_struct(brna, "MeshFloatProperty", NULL);
	RNA_def_struct_sdna(srna, "MFloatProperty");
	RNA_def_struct_ui_text(srna, "Mesh Float Property",
	                       "User defined floating point number value in a float properties layer");
	RNA_def_struct_path_func(srna, "rna_MeshFloatProperty_path");

	prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f");
	RNA_def_property_ui_text(prop, "Value", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	/* Int */
	srna = RNA_def_struct(brna, "MeshIntPropertyLayer", NULL);
	RNA_def_struct_sdna(srna, "CustomDataLayer");
	RNA_def_struct_ui_text(srna, "Mesh Int Property Layer", "User defined layer of integer number values");
	RNA_def_struct_path_func(srna, "rna_MeshIntPropertyLayer_path");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshAnyLayer_name_set");
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshIntProperty");
	RNA_def_property_ui_text(prop, "Data", "");
	RNA_def_property_collection_funcs(prop, "rna_MeshIntPropertyLayer_data_begin", "rna_iterator_array_next",
	                                  "rna_iterator_array_end", "rna_iterator_array_get",
	                                  "rna_MeshIntPropertyLayer_data_length", NULL, NULL, NULL);

	srna = RNA_def_struct(brna, "MeshIntProperty", NULL);
	RNA_def_struct_sdna(srna, "MIntProperty");
	RNA_def_struct_ui_text(srna, "Mesh Int Property",
	                       "User defined integer number value in an integer properties layer");
	RNA_def_struct_path_func(srna, "rna_MeshIntProperty_path");

	prop = RNA_def_property(srna, "value", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "i");
	RNA_def_property_ui_text(prop, "Value", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	/* String */
	srna = RNA_def_struct(brna, "MeshStringPropertyLayer", NULL);
	RNA_def_struct_sdna(srna, "CustomDataLayer");
	RNA_def_struct_ui_text(srna, "Mesh String Property Layer", "User defined layer of string text values");
	RNA_def_struct_path_func(srna, "rna_MeshStringPropertyLayer_path");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshAnyLayer_name_set");
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshStringProperty");
	RNA_def_property_ui_text(prop, "Data", "");
	RNA_def_property_collection_funcs(prop, "rna_MeshStringPropertyLayer_data_begin", "rna_iterator_array_next",
	                                  "rna_iterator_array_end", "rna_iterator_array_get",
	                                  "rna_MeshStringPropertyLayer_data_length", NULL, NULL, NULL);

	srna = RNA_def_struct(brna, "MeshStringProperty", NULL);
	RNA_def_struct_sdna(srna, "MStringProperty");
	RNA_def_struct_ui_text(srna, "Mesh String Property",
	                       "User defined string text value in a string properties layer");
	RNA_def_struct_path_func(srna, "rna_MeshStringProperty_path");

	/* low level mesh data access, treat as bytes */
	prop = RNA_def_property(srna, "value", PROP_STRING, PROP_BYTESTRING);
	RNA_def_property_string_sdna(prop, NULL, "s");
	RNA_def_property_string_funcs(prop, "rna_MeshStringProperty_s_get", "rna_MeshStringProperty_s_length", "rna_MeshStringProperty_s_set");
	RNA_def_property_ui_text(prop, "Value", "");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
}

void rna_def_texmat_common(StructRNA *srna, const char *texspace_editable)
{
	PropertyRNA *prop;

	/* texture space */
	prop = RNA_def_property(srna, "auto_texspace", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "texflag", ME_AUTOSPACE);
	RNA_def_property_ui_text(prop, "Auto Texture Space",
	                         "Adjust active object's texture space automatically when transforming object");

	prop = RNA_def_property(srna, "texspace_location", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "loc");
	RNA_def_property_ui_text(prop, "Texture Space Location", "Texture space location");
	RNA_def_property_float_funcs(prop, "rna_Mesh_texspace_loc_get", NULL, NULL);
	RNA_def_property_editable_func(prop, texspace_editable);
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "texspace_size", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_flag(prop, PROP_PROPORTIONAL);
	RNA_def_property_ui_text(prop, "Texture Space Size", "Texture space size");
	RNA_def_property_float_funcs(prop, "rna_Mesh_texspace_size_get", NULL, NULL);
	RNA_def_property_editable_func(prop, texspace_editable);
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	/* not supported yet */
#if 0
	prop = RNA_def_property(srna, "texspace_rot", PROP_FLOAT, PROP_EULER);
	RNA_def_property_float(prop, NULL, "rot");
	RNA_def_property_ui_text(prop, "Texture Space Rotation", "Texture space rotation");
	RNA_def_property_editable_func(prop, texspace_editable);
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
#endif

	/* materials */
	prop = RNA_def_property(srna, "materials", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "mat", "totcol");
	RNA_def_property_struct_type(prop, "Material");
	RNA_def_property_ui_text(prop, "Materials", "");
	RNA_def_property_srna(prop, "IDMaterials"); /* see rna_ID.c */
	RNA_def_property_collection_funcs(prop, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "rna_IDMaterials_assign_int");
}


/* scene.objects */
/* mesh.vertices */
static void rna_def_mesh_vertices(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
/*	PropertyRNA *prop; */

	FunctionRNA *func;
/*	PropertyRNA *parm; */

	RNA_def_property_srna(cprop, "MeshVertices");
	srna = RNA_def_struct(brna, "MeshVertices", NULL);
	RNA_def_struct_sdna(srna, "Mesh");
	RNA_def_struct_ui_text(srna, "Mesh Vertices", "Collection of mesh vertices");

	func = RNA_def_function(srna, "add", "ED_mesh_vertices_add");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_int(func, "count", 0, 0, INT_MAX, "Count", "Number of vertices to add", 0, INT_MAX);
#if 0 /* BMESH_TODO Remove until BMesh merge */
	func = RNA_def_function(srna, "remove", "ED_mesh_vertices_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_int(func, "count", 0, 0, INT_MAX, "Count", "Number of vertices to remove", 0, INT_MAX);
#endif
}

/* mesh.edges */
static void rna_def_mesh_edges(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
/*	PropertyRNA *prop; */

	FunctionRNA *func;
/*	PropertyRNA *parm; */

	RNA_def_property_srna(cprop, "MeshEdges");
	srna = RNA_def_struct(brna, "MeshEdges", NULL);
	RNA_def_struct_sdna(srna, "Mesh");
	RNA_def_struct_ui_text(srna, "Mesh Edges", "Collection of mesh edges");

	func = RNA_def_function(srna, "add", "ED_mesh_edges_add");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_int(func, "count", 0, 0, INT_MAX, "Count", "Number of edges to add", 0, INT_MAX);
#if 0 /* BMESH_TODO Remove until BMesh merge */
	func = RNA_def_function(srna, "remove", "ED_mesh_edges_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_int(func, "count", 0, 0, INT_MAX, "Count", "Number of edges to remove", 0, INT_MAX);
#endif
}

/* mesh.faces */
static void rna_def_mesh_tessfaces(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
/*	PropertyRNA *parm; */

	RNA_def_property_srna(cprop, "MeshTessFaces");
	srna = RNA_def_struct(brna, "MeshTessFaces", NULL);
	RNA_def_struct_sdna(srna, "Mesh");
	RNA_def_struct_ui_text(srna, "Mesh Faces", "Collection of mesh faces");

	prop = RNA_def_property(srna, "active", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "act_face");
	RNA_def_property_ui_text(prop, "Active Face", "The active face for this mesh");

	func = RNA_def_function(srna, "add", "ED_mesh_tessfaces_add");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_int(func, "count", 0, 0, INT_MAX, "Count", "Number of faces to add", 0, INT_MAX);
#if 0 /* BMESH_TODO Remove until BMesh merge */
	func = RNA_def_function(srna, "remove", "ED_mesh_faces_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_int(func, "count", 0, 0, INT_MAX, "Count", "Number of faces to remove", 0, INT_MAX);
#endif
}

/* mesh.loops */
static void rna_def_mesh_loops(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	/*PropertyRNA *prop;*/

	FunctionRNA *func;
	/*PropertyRNA *parm;*/

	RNA_def_property_srna(cprop, "MeshLoops");
	srna = RNA_def_struct(brna, "MeshLoops", NULL);
	RNA_def_struct_sdna(srna, "Mesh");
	RNA_def_struct_ui_text(srna, "Mesh Loops", "Collection of mesh loops");

	func = RNA_def_function(srna, "add", "ED_mesh_loops_add");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_int(func, "count", 0, 0, INT_MAX, "Count", "Number of loops to add", 0, INT_MAX);
}

/* mesh.polygons */
static void rna_def_mesh_polygons(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	PropertyRNA *prop;

	FunctionRNA *func;
	/* PropertyRNA *parm; */

	RNA_def_property_srna(cprop, "MeshPolygons");
	srna = RNA_def_struct(brna, "MeshPolygons", NULL);
	RNA_def_struct_sdna(srna, "Mesh");
	RNA_def_struct_ui_text(srna, "Mesh Polygons", "Collection of mesh polygons");

	prop = RNA_def_property(srna, "active", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "act_face");
	RNA_def_property_ui_text(prop, "Active Polygon", "The active polygon for this mesh");

	func = RNA_def_function(srna, "add", "ED_mesh_polys_add");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_int(func, "count", 0, 0, INT_MAX, "Count", "Number of polygons to add", 0, INT_MAX);
}


/* mesh.vertex_colors */
static void rna_def_tessface_vertex_colors(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "VertexColors");
	srna = RNA_def_struct(brna, "VertexColors", NULL);
	RNA_def_struct_sdna(srna, "Mesh");
	RNA_def_struct_ui_text(srna, "Vertex Colors", "Collection of vertex colors");

	/* eventually deprecate this */
	func = RNA_def_function(srna, "new", "rna_Mesh_tessface_vertex_color_new");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Add a vertex color layer to Mesh");
	RNA_def_string(func, "name", "Col", 0, "", "Vertex color name");
	parm = RNA_def_pointer(func, "layer", "MeshColorLayer", "", "The newly created layer");
	RNA_def_property_flag(parm, PROP_RNAPTR);
	RNA_def_function_return(func, parm);

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshColorLayer");
	RNA_def_property_pointer_funcs(prop, "rna_Mesh_tessface_vertex_color_active_get",
	                               "rna_Mesh_tessface_vertex_color_active_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Vertex Color Layer", "Active vertex color layer");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_Mesh_tessface_vertex_color_active_index_get",
	                           "rna_Mesh_tessface_vertex_color_active_index_set", "rna_Mesh_vertex_color_index_range");
	RNA_def_property_ui_text(prop, "Active Vertex Color Index", "Active vertex color index");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
}

static void rna_def_loop_colors(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "LoopColors");
	srna = RNA_def_struct(brna, "LoopColors", NULL);
	RNA_def_struct_sdna(srna, "Mesh");
	RNA_def_struct_ui_text(srna, "Loop Colors", "Collection of vertex colors");

	func = RNA_def_function(srna, "new", "rna_Mesh_vertex_color_new");
	RNA_def_function_ui_description(func, "Add a vertex color layer to Mesh");
	RNA_def_string(func, "name", "Col", 0, "", "Vertex color name");
	parm = RNA_def_pointer(func, "layer", "MeshLoopColorLayer", "", "The newly created layer");
	RNA_def_property_flag(parm, PROP_RNAPTR);
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_Mesh_vertex_color_remove");
	RNA_def_function_ui_description(func, "Remove a vertex color layer");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "layer", "MeshLoopColorLayer", "", "The layer to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshLoopColorLayer");
	RNA_def_property_pointer_funcs(prop, "rna_Mesh_vertex_color_active_get",
	                               "rna_Mesh_vertex_color_active_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "Active Vertex Color Layer", "Active vertex color layer");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_Mesh_vertex_color_active_index_get",
	                           "rna_Mesh_vertex_color_active_index_set", "rna_Mesh_vertex_color_index_range");
	RNA_def_property_ui_text(prop, "Active Vertex Color Index", "Active vertex color index");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
}

static void rna_def_uv_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* FunctionRNA *func; */
	/* PropertyRNA *parm; */

	RNA_def_property_srna(cprop, "UVLoopLayers");
	srna = RNA_def_struct(brna, "UVLoopLayers", NULL);
	RNA_def_struct_sdna(srna, "Mesh");
	RNA_def_struct_ui_text(srna, "UV Loop Layers", "Collection of uv loop layers");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshUVLoopLayer");
	RNA_def_property_pointer_funcs(prop, "rna_Mesh_uv_layer_active_get",
	                               "rna_Mesh_uv_layer_active_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "Active UV loop layer", "Active UV loop layer");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_Mesh_uv_layer_active_index_get",
	                           "rna_Mesh_uv_layer_active_index_set", "rna_Mesh_uv_layer_index_range");
	RNA_def_property_ui_text(prop, "Active UV loop layer Index", "Active UV loop layer index");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
}

/* mesh int layers */
static void rna_def_polygon_int_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "IntProperties");
	srna = RNA_def_struct(brna, "IntProperties", NULL);
	RNA_def_struct_sdna(srna, "Mesh");
	RNA_def_struct_ui_text(srna, "Int Properties", "Collection of int properties");

	func = RNA_def_function(srna, "new", "rna_Mesh_polygon_int_property_new");
	RNA_def_function_ui_description(func, "Add a integer property layer to Mesh");
	RNA_def_string(func, "name", "Int Prop", 0, "",  "Int property name");
	parm = RNA_def_pointer(func, "layer", "MeshIntPropertyLayer", "", "The newly created layer");
	RNA_def_property_flag(parm, PROP_RNAPTR);
	RNA_def_function_return(func, parm);
}

/* mesh float layers */
static void rna_def_polygon_float_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "FloatProperties");
	srna = RNA_def_struct(brna, "FloatProperties", NULL);
	RNA_def_struct_sdna(srna, "Mesh");
	RNA_def_struct_ui_text(srna, "Float Properties", "Collection of float properties");

	func = RNA_def_function(srna, "new", "rna_Mesh_polygon_float_property_new");
	RNA_def_function_ui_description(func, "Add a float property layer to Mesh");
	RNA_def_string(func, "name", "Float Prop", 0, "", "Float property name");
	parm = RNA_def_pointer(func, "layer", "MeshFloatPropertyLayer", "", "The newly created layer");
	RNA_def_property_flag(parm, PROP_RNAPTR);
	RNA_def_function_return(func, parm);
}

/* mesh string layers */
static void rna_def_polygon_string_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "StringProperties");
	srna = RNA_def_struct(brna, "StringProperties", NULL);
	RNA_def_struct_sdna(srna, "Mesh");
	RNA_def_struct_ui_text(srna, "String Properties", "Collection of string properties");

	func = RNA_def_function(srna, "new", "rna_Mesh_polygon_string_property_new");
	RNA_def_function_ui_description(func, "Add a string property layer to Mesh");
	RNA_def_string(func, "name", "String Prop", 0, "", "String property name");
	parm = RNA_def_pointer(func, "layer", "MeshStringPropertyLayer", "", "The newly created layer");
	RNA_def_property_flag(parm, PROP_RNAPTR);
	RNA_def_function_return(func, parm);
}

/* mesh.tessface_uv_layers */
static void rna_def_tessface_uv_textures(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "TessfaceUVTextures");
	srna = RNA_def_struct(brna, "TessfaceUVTextures", NULL);
	RNA_def_struct_sdna(srna, "Mesh");
	RNA_def_struct_ui_text(srna, "UV Maps", "Collection of UV maps for tessellated faces");

	/* eventually deprecate this */
	func = RNA_def_function(srna, "new", "rna_Mesh_tessface_uv_texture_new");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Add a UV tessface-texture layer to Mesh (only for meshes with no polygons)");
	RNA_def_string(func, "name", "UVMap", 0, "", "UV map name");
	parm = RNA_def_pointer(func, "layer", "MeshTextureFaceLayer", "", "The newly created layer");
	RNA_def_property_flag(parm, PROP_RNAPTR);
	RNA_def_function_return(func, parm);


	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshTextureFaceLayer");
	RNA_def_property_pointer_funcs(prop, "rna_Mesh_tessface_uv_texture_active_get",
	                               "rna_Mesh_tessface_uv_texture_active_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active UV Map", "Active UV Map");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_Mesh_tessface_uv_texture_active_index_get",
	                           "rna_Mesh_tessface_uv_texture_active_index_set", "rna_Mesh_uv_texture_index_range");
	RNA_def_property_ui_text(prop, "Active UV Map Index", "Active UV Map index");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
}


static void rna_def_uv_textures(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "UVTextures");
	srna = RNA_def_struct(brna, "UVTextures", NULL);
	RNA_def_struct_sdna(srna, "Mesh");
	RNA_def_struct_ui_text(srna, "UV Maps", "Collection of UV maps");

	func = RNA_def_function(srna, "new", "rna_Mesh_uv_texture_new");
	RNA_def_function_ui_description(func, "Add a UV map layer to Mesh");
	RNA_def_string(func, "name", "UVMap", 0, "", "UV map name");
	parm = RNA_def_pointer(func, "layer", "MeshTexturePolyLayer", "", "The newly created layer");
	RNA_def_property_flag(parm, PROP_RNAPTR);
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_Mesh_uv_texture_layers_remove");
	RNA_def_function_ui_description(func, "Remove a vertex color layer");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "layer", "MeshTexturePolyLayer", "", "The layer to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshTexturePolyLayer");
	RNA_def_property_pointer_funcs(prop, "rna_Mesh_uv_texture_active_get",
	                               "rna_Mesh_uv_texture_active_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "Active UV Map", "Active UV Map");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_Mesh_uv_texture_active_index_get",
	                           "rna_Mesh_uv_texture_active_index_set", "rna_Mesh_uv_texture_index_range");
	RNA_def_property_ui_text(prop, "Active UV Map Index", "Active UV Map index");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
}

static void rna_def_skin_vertices(BlenderRNA *brna, PropertyRNA *UNUSED(cprop))
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MeshSkinVertexLayer", NULL);
	RNA_def_struct_ui_text(srna, "Mesh Skin Vertex Layer", "Per-vertex skin data for use with the Skin modifier");
	RNA_def_struct_sdna(srna, "CustomDataLayer");
	RNA_def_struct_path_func(srna, "rna_MeshSkinVertexLayer_path");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshVertexLayer_name_set");
	RNA_def_property_ui_text(prop, "Name", "Name of skin layer");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshSkinVertex");
	RNA_def_property_ui_text(prop, "Data", "");
	RNA_def_property_collection_funcs(prop, "rna_MeshSkinVertexLayer_data_begin", "rna_iterator_array_next",
	                                  "rna_iterator_array_end", "rna_iterator_array_get",
	                                  "rna_MeshSkinVertexLayer_data_length", NULL, NULL, NULL);

	/* SkinVertex struct */
	srna = RNA_def_struct(brna, "MeshSkinVertex", NULL);
	RNA_def_struct_sdna(srna, "MVertSkin");
	RNA_def_struct_ui_text(srna, "Skin Vertex", "Per-vertex skin data for use with the Skin modifier");
	RNA_def_struct_path_func(srna, "rna_MeshSkinVertex_path");

	prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_range(prop, 0.001, 100, 1, 3);
	RNA_def_property_ui_text(prop, "Radius", "Radius of the skin");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	/* Flags */

	prop = RNA_def_property(srna, "use_root", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MVERT_SKIN_ROOT);
	RNA_def_property_ui_text(prop, "Root", "Vertex is a root for rotation calculations and armature generation");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
	
	prop = RNA_def_property(srna, "use_loose", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MVERT_SKIN_LOOSE);
	RNA_def_property_ui_text(prop, "Loose", "If vertex has multiple adjacent edges, it is hulled to them directly");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
}

static void rna_def_mesh(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Mesh", "ID");
	RNA_def_struct_ui_text(srna, "Mesh", "Mesh datablock defining geometric surfaces");
	RNA_def_struct_ui_icon(srna, ICON_MESH_DATA);

	prop = RNA_def_property(srna, "vertices", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "mvert", "totvert");
	RNA_def_property_struct_type(prop, "MeshVertex");
	RNA_def_property_ui_text(prop, "Vertices", "Vertices of the mesh");
	rna_def_mesh_vertices(brna, prop);

	prop = RNA_def_property(srna, "edges", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "medge", "totedge");
	RNA_def_property_struct_type(prop, "MeshEdge");
	RNA_def_property_ui_text(prop, "Edges", "Edges of the mesh");
	rna_def_mesh_edges(brna, prop);

	prop = RNA_def_property(srna, "tessfaces", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "mface", "totface");
	RNA_def_property_struct_type(prop, "MeshTessFace");
	RNA_def_property_ui_text(prop, "TessFaces", "Tessellated faces of the mesh (derived from polygons)");
	rna_def_mesh_tessfaces(brna, prop);

	prop = RNA_def_property(srna, "loops", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "mloop", "totloop");
	RNA_def_property_struct_type(prop, "MeshLoop");
	RNA_def_property_ui_text(prop, "Loops", "Loops of the mesh (polygon corners)");
	rna_def_mesh_loops(brna, prop);

	prop = RNA_def_property(srna, "polygons", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "mpoly", "totpoly");
	RNA_def_property_struct_type(prop, "MeshPolygon");
	RNA_def_property_ui_text(prop, "Polygons", "Polygons of the mesh");
	rna_def_mesh_polygons(brna, prop);

	/* TODO, should this be allowed to be its self? */
	prop = RNA_def_property(srna, "texture_mesh", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "texcomesh");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_ui_text(prop, "Texture Mesh",
	                         "Use another mesh for texture indices (vertex indices must be aligned)");

	/* UV loop layers */
	prop = RNA_def_property(srna, "uv_layers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "ldata.layers", "ldata.totlayer");
	RNA_def_property_collection_funcs(prop, "rna_Mesh_uv_layers_begin", NULL, NULL, NULL,
	                                  "rna_Mesh_uv_layers_length", NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "MeshUVLoopLayer");
	RNA_def_property_ui_text(prop, "UV Loop Layers", "All UV loop layers");
	rna_def_uv_layers(brna, prop);

	prop = RNA_def_property(srna, "uv_layer_clone", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshUVLoopLayer");
	RNA_def_property_pointer_funcs(prop, "rna_Mesh_uv_layer_clone_get",
	                               "rna_Mesh_uv_layer_clone_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Clone UV loop layer", "UV loop layer to be used as cloning source");

	prop = RNA_def_property(srna, "uv_layer_clone_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_Mesh_uv_layer_clone_index_get",
	                           "rna_Mesh_uv_layer_clone_index_set", "rna_Mesh_uv_layer_index_range");
	RNA_def_property_ui_text(prop, "Clone UV loop layer Index", "Clone UV loop layer index");

	prop = RNA_def_property(srna, "uv_layer_stencil", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshUVLoopLayer");
	RNA_def_property_pointer_funcs(prop, "rna_Mesh_uv_layer_stencil_get",
	                               "rna_Mesh_uv_layer_stencil_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mask UV loop layer", "UV loop layer to mask the painted area");

	prop = RNA_def_property(srna, "uv_layer_stencil_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_Mesh_uv_layer_stencil_index_get",
	                           "rna_Mesh_uv_layer_stencil_index_set", "rna_Mesh_uv_layer_index_range");
	RNA_def_property_ui_text(prop, "Mask UV loop layer Index", "Mask UV loop layer index");

	/* Tessellated face UV maps - used by renderers */
	prop = RNA_def_property(srna, "tessface_uv_textures", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "fdata.layers", "fdata.totlayer");
	RNA_def_property_collection_funcs(prop, "rna_Mesh_tessface_uv_textures_begin", NULL, NULL, NULL,
	                                  "rna_Mesh_tessface_uv_textures_length", NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "MeshTextureFaceLayer");
	RNA_def_property_ui_text(prop, "Tessellated Face UV Maps",
	                         "All UV maps for tessellated faces (read-only, for use by renderers)");
	rna_def_tessface_uv_textures(brna, prop);

	/* UV maps */
	prop = RNA_def_property(srna, "uv_textures", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "pdata.layers", "pdata.totlayer");
	RNA_def_property_collection_funcs(prop, "rna_Mesh_uv_textures_begin", NULL, NULL, NULL,
	                                  "rna_Mesh_uv_textures_length", NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "MeshTexturePolyLayer");
	RNA_def_property_ui_text(prop, "UV Maps", "All UV maps");
	rna_def_uv_textures(brna, prop);

	prop = RNA_def_property(srna, "uv_texture_clone", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshTexturePolyLayer");
	RNA_def_property_pointer_funcs(prop, "rna_Mesh_uv_texture_clone_get",
	                               "rna_Mesh_uv_texture_clone_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Clone UV Map", "UV map to be used as cloning source");

	prop = RNA_def_property(srna, "uv_texture_clone_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_Mesh_uv_texture_clone_index_get",
	                           "rna_Mesh_uv_texture_clone_index_set", "rna_Mesh_uv_texture_index_range");
	RNA_def_property_ui_text(prop, "Clone UV Map Index", "Clone UV map index");

	prop = RNA_def_property(srna, "uv_texture_stencil", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MeshTexturePolyLayer");
	RNA_def_property_pointer_funcs(prop, "rna_Mesh_uv_texture_stencil_get",
	                               "rna_Mesh_uv_texture_stencil_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mask UV Map", "UV map to mask the painted area");

	prop = RNA_def_property(srna, "uv_texture_stencil_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_Mesh_uv_texture_stencil_index_get",
	                           "rna_Mesh_uv_texture_stencil_index_set", "rna_Mesh_uv_texture_index_range");
	RNA_def_property_ui_text(prop, "Mask UV Map Index", "Mask UV map index");

	/* Tessellated face colors - used by renderers */

	prop = RNA_def_property(srna, "tessface_vertex_colors", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "fdata.layers", "fdata.totlayer");
	RNA_def_property_collection_funcs(prop, "rna_Mesh_tessface_vertex_colors_begin", NULL, NULL, NULL,
	                                  "rna_Mesh_tessface_vertex_colors_length", NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "MeshColorLayer");
	RNA_def_property_ui_text(prop, "Tessellated Face Colors",
	                         "All tessellated face colors (read-only, for use by renderers)");
	rna_def_tessface_vertex_colors(brna, prop);

	/* Vertex colors */

	prop = RNA_def_property(srna, "vertex_colors", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "ldata.layers", "ldata.totlayer");
	RNA_def_property_collection_funcs(prop, "rna_Mesh_vertex_colors_begin", NULL, NULL, NULL,
	                                  "rna_Mesh_vertex_colors_length", NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "MeshLoopColorLayer");
	RNA_def_property_ui_text(prop, "Vertex Colors", "All vertex colors");
	rna_def_loop_colors(brna, prop);

	/* TODO, vertex, edge customdata layers (bmesh py api can access already) */
	prop = RNA_def_property(srna, "polygon_layers_float", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "pdata.layers", "pdata.totlayer");
	RNA_def_property_collection_funcs(prop, "rna_Mesh_polygon_float_layers_begin", NULL, NULL, NULL,
	                                  "rna_Mesh_polygon_float_layers_length", NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "MeshFloatPropertyLayer");
	RNA_def_property_ui_text(prop, "Float Property Layers", "");
	rna_def_polygon_float_layers(brna, prop);

	prop = RNA_def_property(srna, "polygon_layers_int", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "pdata.layers", "pdata.totlayer");
	RNA_def_property_collection_funcs(prop, "rna_Mesh_polygon_int_layers_begin", NULL, NULL, NULL,
	                                  "rna_Mesh_polygon_int_layers_length", NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "MeshIntPropertyLayer");
	RNA_def_property_ui_text(prop, "Int Property Layers", "");
	rna_def_polygon_int_layers(brna, prop);

	prop = RNA_def_property(srna, "polygon_layers_string", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "pdata.layers", "pdata.totlayer");
	RNA_def_property_collection_funcs(prop, "rna_Mesh_polygon_string_layers_begin", NULL, NULL, NULL,
	                                  "rna_Mesh_polygon_string_layers_length", NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "MeshStringPropertyLayer");
	RNA_def_property_ui_text(prop, "String Property Layers", "");
	rna_def_polygon_string_layers(brna, prop);

	/* Skin vertices */
	prop = RNA_def_property(srna, "skin_vertices", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "vdata.layers", "vdata.totlayer");
	RNA_def_property_collection_funcs(prop, "rna_Mesh_skin_vertices_begin", NULL, NULL, NULL,
	                                  "rna_Mesh_skin_vertices_length", NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "MeshSkinVertexLayer");
	RNA_def_property_ui_text(prop, "Skin Vertices", "All skin vertices");
	rna_def_skin_vertices(brna, prop);
	/* End skin vertices */

	prop = RNA_def_property(srna, "use_auto_smooth", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_AUTOSMOOTH);
	RNA_def_property_ui_text(prop, "Auto Smooth",
	                         "Treat all set-smoothed faces with angles less than the specified angle "
	                         "as 'smooth' during render");

#if 1 /* expose as radians */
	prop = RNA_def_property(srna, "auto_smooth_angle", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_funcs(prop, "rna_Mesh_auto_smooth_angle_get", "rna_Mesh_auto_smooth_angle_set", NULL);
	RNA_def_property_ui_range(prop, DEG2RAD(1.0), DEG2RAD(80), 1.0, 1);
#else
	prop = RNA_def_property(srna, "auto_smooth_angle", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "smoothresh");
	RNA_def_property_range(prop, 1, 80);
#endif
	RNA_def_property_ui_text(prop, "Auto Smooth Angle",
	                         "Maximum angle between face normals that 'Auto Smooth' will operate on");

	prop = RNA_def_property(srna, "show_double_sided", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_TWOSIDED);
	RNA_def_property_ui_text(prop, "Double Sided", "Render/display the mesh with double or single sided lighting");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

	prop = RNA_def_property(srna, "texco_mesh", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "texcomesh");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Texture Space Mesh", "Derive texture coordinates from another mesh");

	prop = RNA_def_property(srna, "shape_keys", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "key");
	RNA_def_property_ui_text(prop, "Shape Keys", "");

	/* texture space */
	prop = RNA_def_property(srna, "use_auto_texspace", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "texflag", ME_AUTOSPACE);
	RNA_def_property_ui_text(prop, "Auto Texture Space",
	                         "Adjust active object's texture space automatically when transforming object");

#if 0
	prop = RNA_def_property(srna, "texspace_location", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Texture Space Location", "Texture space location");
	RNA_def_property_editable_func(prop, "rna_Mesh_texspace_editable");
	RNA_def_property_float_funcs(prop, "rna_Mesh_texspace_loc_get", "rna_Mesh_texspace_loc_set", NULL);
	RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");
#endif

	/* not supported yet */
#if 0
	prop = RNA_def_property(srna, "texspace_rot", PROP_FLOAT, PROP_EULER);
	RNA_def_property_float(prop, NULL, "rot");
	RNA_def_property_ui_text(prop, "Texture Space Rotation", "Texture space rotation");
	RNA_def_property_editable_func(prop, texspace_editable);
	RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");
#endif

	/* Mesh Draw Options for Edit Mode*/

	prop = RNA_def_property(srna, "show_edges", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "drawflag", ME_DRAWEDGES);
	RNA_def_property_ui_text(prop, "Draw Edges",
	                         "Display selected edges using highlights in the 3D view and UV editor");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

	prop = RNA_def_property(srna, "show_faces", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "drawflag", ME_DRAWFACES);
	RNA_def_property_ui_text(prop, "Draw Faces", "Display all faces as shades in the 3D view and UV editor");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

	prop = RNA_def_property(srna, "show_normal_face", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "drawflag", ME_DRAWNORMALS);
	RNA_def_property_ui_text(prop, "Draw Normals", "Display face normals as lines");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

	prop = RNA_def_property(srna, "show_normal_vertex", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "drawflag", ME_DRAW_VNORMALS);
	RNA_def_property_ui_text(prop, "Draw Vertex Normals", "Display vertex normals as lines");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

	prop = RNA_def_property(srna, "show_weight", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "drawflag", ME_DRAWEIGHT);
	RNA_def_property_ui_text(prop, "Show Weights", "Draw weights in editmode");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data_edit_color");  /* needs to rebuild 'dm' */

	prop = RNA_def_property(srna, "show_edge_crease", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "drawflag", ME_DRAWCREASES);
	RNA_def_property_ui_text(prop, "Draw Creases", "Display creases created for subsurf weighting");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

	prop = RNA_def_property(srna, "show_edge_bevel_weight", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "drawflag", ME_DRAWBWEIGHTS);
	RNA_def_property_ui_text(prop, "Draw Bevel Weights", "Display weights created for the Bevel modifier");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

	prop = RNA_def_property(srna, "show_edge_seams", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "drawflag", ME_DRAWSEAMS);
	RNA_def_property_ui_text(prop, "Draw Seams", "Display UV unwrapping seams");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

	prop = RNA_def_property(srna, "show_edge_sharp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "drawflag", ME_DRAWSHARP);
	RNA_def_property_ui_text(prop, "Draw Sharp", "Display sharp edges, used with the EdgeSplit modifier");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

	prop = RNA_def_property(srna, "show_freestyle_edge_marks", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "drawflag", ME_DRAW_FREESTYLE_EDGE);
	RNA_def_property_ui_text(prop, "Draw Freestyle Edge Marks", "Display Freestyle edge marks, used with the Freestyle renderer");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

	prop = RNA_def_property(srna, "show_freestyle_face_marks", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "drawflag", ME_DRAW_FREESTYLE_FACE);
	RNA_def_property_ui_text(prop, "Draw Freestyle Face Marks", "Display Freestyle face marks, used with the Freestyle renderer");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

	prop = RNA_def_property(srna, "show_statvis", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "drawflag", ME_DRAW_STATVIS);
	RNA_def_property_ui_text(prop, "Stat Vis", "Display statistical information about the mesh");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_data_edit_color");

	prop = RNA_def_property(srna, "show_extra_edge_length", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "drawflag", ME_DRAWEXTRA_EDGELEN);
	RNA_def_property_ui_text(prop, "Edge Length",
	                         "Display selected edge lengths, using global values when set in the transform panel");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

	prop = RNA_def_property(srna, "show_extra_edge_angle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "drawflag", ME_DRAWEXTRA_EDGEANG);
	RNA_def_property_ui_text(prop, "Edge Angle",
	                         "Display selected edge angle, using global values when set in the transform panel");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

	prop = RNA_def_property(srna, "show_extra_face_angle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "drawflag", ME_DRAWEXTRA_FACEANG);
	RNA_def_property_ui_text(prop, "Face Angles",
	                         "Display the angles in the selected edges, "
	                         "using global values when set in the transform panel");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

	prop = RNA_def_property(srna, "show_extra_face_area", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "drawflag", ME_DRAWEXTRA_FACEAREA);
	RNA_def_property_ui_text(prop, "Face Area",
	                         "Display the area of selected faces, "
	                         "using global values when set in the transform panel");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

	prop = RNA_def_property(srna, "show_extra_indices", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "drawflag", ME_DRAWEXTRA_INDICES);
	RNA_def_property_ui_text(prop, "Indices", "Display the index numbers of selected vertices, edges, and faces");
	RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

	/* editflag */
	prop = RNA_def_property(srna, "use_mirror_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "editflag", ME_EDIT_MIRROR_X);
	RNA_def_property_ui_text(prop, "X Mirror", "X Axis mirror editing");

#if 0
	prop = RNA_def_property(srna, "use_mirror_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "editflag", ME_EDIT_MIRROR_Y);
	RNA_def_property_ui_text(prop, "Y Mirror", "Y Axis mirror editing");

	prop = RNA_def_property(srna, "use_mirror_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "editflag", ME_EDIT_MIRROR_Z);
	RNA_def_property_ui_text(prop, "Z Mirror", "Z Axis mirror editing");
#endif

	prop = RNA_def_property(srna, "use_mirror_topology", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "editflag", ME_EDIT_MIRROR_TOPO);
	RNA_def_property_ui_text(prop, "Topology Mirror",
	                         "Use topology based mirroring "
	                         "(for when both sides of mesh have matching, unique topology)");

	prop = RNA_def_property(srna, "use_paint_mask", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "editflag", ME_EDIT_PAINT_FACE_SEL);
	RNA_def_property_ui_text(prop, "Paint Mask", "Face selection masking for painting");
	RNA_def_property_ui_icon(prop, ICON_FACESEL_HLT, 0);
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_Mesh_update_facemask");

	prop = RNA_def_property(srna, "use_paint_mask_vertex", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "editflag", ME_EDIT_PAINT_VERT_SEL);
	RNA_def_property_ui_text(prop, "Vertex Selection", "Vertex selection masking for painting (weight paint only)");
	RNA_def_property_ui_icon(prop, ICON_VERTEXSEL, 0);
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_Mesh_update_vertmask");



	/* customdata flags */
	prop = RNA_def_property(srna, "use_customdata_vertex_bevel", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "cd_flag", ME_CDFLAG_VERT_BWEIGHT);
	RNA_def_property_ui_text(prop, "Store Vertex Bevel Weight", "");

	prop = RNA_def_property(srna, "use_customdata_edge_bevel", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "cd_flag", ME_CDFLAG_EDGE_BWEIGHT);
	RNA_def_property_ui_text(prop, "Store Edge Bevel Weight", "");

	prop = RNA_def_property(srna, "use_customdata_edge_crease", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "cd_flag", ME_CDFLAG_EDGE_CREASE);
	RNA_def_property_ui_text(prop, "Store Edge Crease", "");


	/* readonly editmesh info - use for extrude menu */
	prop = RNA_def_property(srna, "total_vert_sel", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_Mesh_tot_vert_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Selected Vert Total", "Selected vertex count in editmode");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "total_edge_sel", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_Mesh_tot_edge_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Selected Edge Total", "Selected edge count in editmode");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "total_face_sel", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_Mesh_tot_face_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Selected Face Total", "Selected face count in editmode");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "is_editmode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_Mesh_is_editmode_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Is Editmode", "True when used in editmode");

	/* pointers */
	rna_def_animdata_common(srna);
	rna_def_texmat_common(srna, "rna_Mesh_texspace_editable");

	RNA_api_mesh(srna);
}

void RNA_def_mesh(BlenderRNA *brna)
{
	rna_def_mesh(brna);
	rna_def_mvert(brna);
	rna_def_mvert_group(brna);
	rna_def_medge(brna);
	rna_def_mface(brna);
	rna_def_mloop(brna);
	rna_def_mpolygon(brna);
	rna_def_mloopuv(brna);
	rna_def_mtface(brna);
	rna_def_mtexpoly(brna);
	rna_def_mcol(brna);
	rna_def_mloopcol(brna);
	rna_def_mproperties(brna);
}

#endif
