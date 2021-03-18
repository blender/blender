/*
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
 */

/* note: the original vertex color stuff is now just used for
 * getting info on the layers themselves, accessing the data is
 * done through the (not yet written) mpoly interfaces.*/

/** \file
 * \ingroup RNA
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
#include "RNA_enum_types.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "WM_types.h"

const EnumPropertyItem rna_enum_mesh_delimit_mode_items[] = {
    {BMO_DELIM_NORMAL, "NORMAL", 0, "Normal", "Delimit by face directions"},
    {BMO_DELIM_MATERIAL, "MATERIAL", 0, "Material", "Delimit by face material"},
    {BMO_DELIM_SEAM, "SEAM", 0, "Seam", "Delimit by edge seams"},
    {BMO_DELIM_SHARP, "SHARP", 0, "Sharp", "Delimit by sharp edges"},
    {BMO_DELIM_UV, "UV", 0, "UVs", "Delimit by UV coordinates"},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_mesh_remesh_mode_items[] = {
    {REMESH_VOXEL, "VOXEL", 0, "Voxel", "Use the voxel remesher"},
    {REMESH_QUAD, "QUAD", 0, "Quad", "Use the quad remesher"},
    {0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME

#  include "DNA_scene_types.h"

#  include "BLI_math.h"

#  include "BKE_customdata.h"
#  include "BKE_main.h"
#  include "BKE_mesh.h"
#  include "BKE_mesh_runtime.h"
#  include "BKE_report.h"

#  include "DEG_depsgraph.h"

#  include "ED_mesh.h" /* XXX Bad level call */

#  include "WM_api.h"

#  include "rna_mesh_utils.h"

/* -------------------------------------------------------------------- */
/* Generic helpers */

static Mesh *rna_mesh(PointerRNA *ptr)
{
  Mesh *me = (Mesh *)ptr->owner_id;
  return me;
}

static CustomData *rna_mesh_vdata_helper(Mesh *me)
{
  return (me->edit_mesh) ? &me->edit_mesh->bm->vdata : &me->vdata;
}

static CustomData *rna_mesh_edata_helper(Mesh *me)
{
  return (me->edit_mesh) ? &me->edit_mesh->bm->edata : &me->edata;
}

static CustomData *rna_mesh_pdata_helper(Mesh *me)
{
  return (me->edit_mesh) ? &me->edit_mesh->bm->pdata : &me->pdata;
}

static CustomData *rna_mesh_ldata_helper(Mesh *me)
{
  return (me->edit_mesh) ? &me->edit_mesh->bm->ldata : &me->ldata;
}

static CustomData *rna_mesh_fdata_helper(Mesh *me)
{
  return (me->edit_mesh) ? NULL : &me->fdata;
}

static CustomData *rna_mesh_vdata(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  return rna_mesh_vdata_helper(me);
}
#  if 0
static CustomData *rna_mesh_edata(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  return rna_mesh_edata_helper(me);
}
#  endif
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
  Mesh *me = (Mesh *)ptr->owner_id;
  CustomData *cd;

  /* rely on negative values wrapping */
#  define TEST_CDL(cmd) \
    if ((void)(cd = cmd(me)), ARRAY_HAS_ITEM(cdl, cd->layers, cd->totlayer)) { \
      return cd; \
    } \
    ((void)0)

  TEST_CDL(rna_mesh_vdata_helper);
  TEST_CDL(rna_mesh_edata_helper);
  TEST_CDL(rna_mesh_pdata_helper);
  TEST_CDL(rna_mesh_ldata_helper);
  TEST_CDL(rna_mesh_fdata_helper);

#  undef TEST_CDL

  /* should _never_ happen */
  return NULL;
}

static void rna_MeshVertexLayer_name_set(PointerRNA *ptr, const char *value)
{
  rna_cd_layer_name_set(rna_mesh_vdata(ptr), (CustomDataLayer *)ptr->data, value);
}
#  if 0
static void rna_MeshEdgeLayer_name_set(PointerRNA *ptr, const char *value)
{
  rna_cd_layer_name_set(rna_mesh_edata(ptr), (CustomDataLayer *)ptr->data, value);
}
#  endif
static void rna_MeshPolyLayer_name_set(PointerRNA *ptr, const char *value)
{
  rna_cd_layer_name_set(rna_mesh_pdata(ptr), (CustomDataLayer *)ptr->data, value);
}
static void rna_MeshLoopLayer_name_set(PointerRNA *ptr, const char *value)
{
  rna_cd_layer_name_set(rna_mesh_ldata(ptr), (CustomDataLayer *)ptr->data, value);
}
/* only for layers shared between types */
static void rna_MeshAnyLayer_name_set(PointerRNA *ptr, const char *value)
{
  CustomData *cd = rna_cd_from_layer(ptr, (CustomDataLayer *)ptr->data);
  rna_cd_layer_name_set(cd, (CustomDataLayer *)ptr->data, value);
}

static bool rna_Mesh_has_custom_normals_get(PointerRNA *ptr)
{
  Mesh *me = ptr->data;
  return BKE_mesh_has_custom_loop_normals(me);
}

/* -------------------------------------------------------------------- */
/* Update Callbacks */

static void rna_Mesh_update_data(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  /* cheating way for importers to avoid slow updates */
  if (id->us > 0) {
    DEG_id_tag_update(id, 0);
    WM_main_add_notifier(NC_GEOM | ND_DATA, id);
  }
}

static void rna_Mesh_update_data_edit_weight(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  BKE_mesh_batch_cache_dirty_tag(rna_mesh(ptr), BKE_MESH_BATCH_DIRTY_ALL);

  rna_Mesh_update_data(bmain, scene, ptr);
}

static void rna_Mesh_update_data_edit_active_color(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  BKE_mesh_batch_cache_dirty_tag(rna_mesh(ptr), BKE_MESH_BATCH_DIRTY_ALL);

  rna_Mesh_update_data(bmain, scene, ptr);
}
static void rna_Mesh_update_select(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  ID *id = ptr->owner_id;
  /* cheating way for importers to avoid slow updates */
  if (id->us > 0) {
    WM_main_add_notifier(NC_GEOM | ND_SELECT, id);
  }
}

void rna_Mesh_update_draw(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  ID *id = ptr->owner_id;
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

  BKE_mesh_batch_cache_dirty_tag(me, BKE_MESH_BATCH_DIRTY_ALL);

  rna_Mesh_update_draw(bmain, scene, ptr);
}

static void rna_Mesh_update_facemask(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Mesh *me = ptr->data;
  if ((me->editflag & ME_EDIT_PAINT_VERT_SEL) && (me->editflag & ME_EDIT_PAINT_FACE_SEL)) {
    me->editflag &= ~ME_EDIT_PAINT_VERT_SEL;
  }

  BKE_mesh_batch_cache_dirty_tag(me, BKE_MESH_BATCH_DIRTY_ALL);

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
  mvert->bweight = round_fl_to_uchar_clamp(value * 255.0f);
}

static float rna_MEdge_bevel_weight_get(PointerRNA *ptr)
{
  MEdge *medge = (MEdge *)ptr->data;
  return medge->bweight / 255.0f;
}

static void rna_MEdge_bevel_weight_set(PointerRNA *ptr, float value)
{
  MEdge *medge = (MEdge *)ptr->data;
  medge->bweight = round_fl_to_uchar_clamp(value * 255.0f);
}

static float rna_MEdge_crease_get(PointerRNA *ptr)
{
  MEdge *medge = (MEdge *)ptr->data;
  return medge->crease / 255.0f;
}

static void rna_MEdge_crease_set(PointerRNA *ptr, float value)
{
  MEdge *medge = (MEdge *)ptr->data;
  medge->crease = round_fl_to_uchar_clamp(value * 255.0f);
}

static void rna_MeshLoop_normal_get(PointerRNA *ptr, float *values)
{
  Mesh *me = rna_mesh(ptr);
  MLoop *ml = (MLoop *)ptr->data;
  const float(*vec)[3] = CustomData_get(&me->ldata, (int)(ml - me->mloop), CD_NORMAL);

  if (!vec) {
    zero_v3(values);
  }
  else {
    copy_v3_v3(values, (const float *)vec);
  }
}

static void rna_MeshLoop_normal_set(PointerRNA *ptr, const float *values)
{
  Mesh *me = rna_mesh(ptr);
  MLoop *ml = (MLoop *)ptr->data;
  float(*vec)[3] = CustomData_get(&me->ldata, (int)(ml - me->mloop), CD_NORMAL);

  if (vec) {
    normalize_v3_v3(*vec, values);
  }
}

static void rna_MeshLoop_tangent_get(PointerRNA *ptr, float *values)
{
  Mesh *me = rna_mesh(ptr);
  MLoop *ml = (MLoop *)ptr->data;
  const float(*vec)[4] = CustomData_get(&me->ldata, (int)(ml - me->mloop), CD_MLOOPTANGENT);

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
  const float(*vec)[4] = CustomData_get(&me->ldata, (int)(ml - me->mloop), CD_MLOOPTANGENT);

  return (vec) ? (*vec)[3] : 0.0f;
}

static void rna_MeshLoop_bitangent_get(PointerRNA *ptr, float *values)
{
  Mesh *me = rna_mesh(ptr);
  MLoop *ml = (MLoop *)ptr->data;
  const float(*nor)[3] = CustomData_get(&me->ldata, (int)(ml - me->mloop), CD_NORMAL);
  const float(*vec)[4] = CustomData_get(&me->ldata, (int)(ml - me->mloop), CD_MLOOPTANGENT);

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
  Mesh *me = (Mesh *)ptr->owner_id;
  MPoly *mp = (MPoly *)ptr->data;

  return BKE_mesh_calc_poly_area(mp, me->mloop + mp->loopstart, me->mvert);
}

static void rna_MeshPolygon_flip(ID *id, MPoly *mp)
{
  Mesh *me = (Mesh *)id;

  BKE_mesh_polygon_flip(mp, me->mloop, &me->ldata);
  BKE_mesh_tessface_clear(me);
  BKE_mesh_runtime_clear_geometry(me);
}

static void rna_MeshLoopTriangle_verts_get(PointerRNA *ptr, int *values)
{
  Mesh *me = rna_mesh(ptr);
  MLoopTri *lt = (MLoopTri *)ptr->data;
  values[0] = me->mloop[lt->tri[0]].v;
  values[1] = me->mloop[lt->tri[1]].v;
  values[2] = me->mloop[lt->tri[2]].v;
}

static void rna_MeshLoopTriangle_normal_get(PointerRNA *ptr, float *values)
{
  Mesh *me = rna_mesh(ptr);
  MLoopTri *lt = (MLoopTri *)ptr->data;
  unsigned int v1 = me->mloop[lt->tri[0]].v;
  unsigned int v2 = me->mloop[lt->tri[1]].v;
  unsigned int v3 = me->mloop[lt->tri[2]].v;

  normal_tri_v3(values, me->mvert[v1].co, me->mvert[v2].co, me->mvert[v3].co);
}

static void rna_MeshLoopTriangle_split_normals_get(PointerRNA *ptr, float *values)
{
  Mesh *me = rna_mesh(ptr);
  const float(*lnors)[3] = CustomData_get_layer(&me->ldata, CD_NORMAL);

  if (!lnors) {
    zero_v3(values + 0);
    zero_v3(values + 3);
    zero_v3(values + 6);
  }
  else {
    MLoopTri *lt = (MLoopTri *)ptr->data;
    copy_v3_v3(values + 0, lnors[lt->tri[0]]);
    copy_v3_v3(values + 3, lnors[lt->tri[1]]);
    copy_v3_v3(values + 6, lnors[lt->tri[2]]);
  }
}

static float rna_MeshLoopTriangle_area_get(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  MLoopTri *lt = (MLoopTri *)ptr->data;
  unsigned int v1 = me->mloop[lt->tri[0]].v;
  unsigned int v2 = me->mloop[lt->tri[1]].v;
  unsigned int v3 = me->mloop[lt->tri[2]].v;

  return area_tri_v3(me->mvert[v1].co, me->mvert[v2].co, me->mvert[v3].co);
}

static void rna_MeshLoopColor_color_get(PointerRNA *ptr, float *values)
{
  MLoopCol *mlcol = (MLoopCol *)ptr->data;

  values[0] = mlcol->r / 255.0f;
  values[1] = mlcol->g / 255.0f;
  values[2] = mlcol->b / 255.0f;
  values[3] = mlcol->a / 255.0f;
}

static void rna_MeshLoopColor_color_set(PointerRNA *ptr, const float *values)
{
  MLoopCol *mlcol = (MLoopCol *)ptr->data;

  mlcol->r = round_fl_to_uchar_clamp(values[0] * 255.0f);
  mlcol->g = round_fl_to_uchar_clamp(values[1] * 255.0f);
  mlcol->b = round_fl_to_uchar_clamp(values[2] * 255.0f);
  mlcol->a = round_fl_to_uchar_clamp(values[3] * 255.0f);
}

static int rna_Mesh_texspace_editable(PointerRNA *ptr, const char **UNUSED(r_info))
{
  Mesh *me = (Mesh *)ptr->data;
  return (me->texflag & ME_AUTOSPACE) ? 0 : PROP_EDITABLE;
}

static void rna_Mesh_texspace_size_get(PointerRNA *ptr, float values[3])
{
  Mesh *me = (Mesh *)ptr->data;

  BKE_mesh_texspace_ensure(me);

  copy_v3_v3(values, me->size);
}

static void rna_Mesh_texspace_loc_get(PointerRNA *ptr, float values[3])
{
  Mesh *me = (Mesh *)ptr->data;

  BKE_mesh_texspace_ensure(me);

  copy_v3_v3(values, me->loc);
}

static void rna_MeshVertex_groups_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);

  if (me->dvert) {
    MVert *mvert = (MVert *)ptr->data;
    MDeformVert *dvert = me->dvert + (mvert - me->mvert);

    rna_iterator_array_begin(
        iter, (void *)dvert->dw, sizeof(MDeformWeight), dvert->totweight, 0, NULL);
  }
  else {
    rna_iterator_array_begin(iter, NULL, 0, 0, 0, NULL);
  }
}

static void rna_MeshVertex_undeformed_co_get(PointerRNA *ptr, float values[3])
{
  Mesh *me = rna_mesh(ptr);
  MVert *mvert = (MVert *)ptr->data;
  float(*orco)[3] = CustomData_get_layer(&me->vdata, CD_ORCO);

  if (orco) {
    /* orco is normalized to 0..1, we do inverse to match mvert->co */
    float loc[3], size[3];

    BKE_mesh_texspace_get(me->texcomesh ? me->texcomesh : me, loc, size);
    madd_v3_v3v3v3(values, loc, orco[(mvert - me->mvert)], size);
  }
  else {
    copy_v3_v3(values, mvert->co);
  }
}

static int rna_CustomDataLayer_active_get(PointerRNA *ptr, CustomData *data, int type, bool render)
{
  int n = ((CustomDataLayer *)ptr->data) - data->layers;

  if (render) {
    return (n == CustomData_get_render_layer_index(data, type));
  }
  else {
    return (n == CustomData_get_active_layer_index(data, type));
  }
}

static int rna_CustomDataLayer_clone_get(PointerRNA *ptr, CustomData *data, int type)
{
  int n = ((CustomDataLayer *)ptr->data) - data->layers;

  return (n == CustomData_get_clone_layer_index(data, type));
}

static void rna_CustomDataLayer_active_set(
    PointerRNA *ptr, CustomData *data, int value, int type, int render)
{
  Mesh *me = (Mesh *)ptr->owner_id;
  int n = (((CustomDataLayer *)ptr->data) - data->layers) - CustomData_get_layer_index(data, type);

  if (value == 0) {
    return;
  }

  if (render) {
    CustomData_set_layer_render(data, type, n);
  }
  else {
    CustomData_set_layer_active(data, type, n);
  }

  BKE_mesh_update_customdata_pointers(me, true);
}

static void rna_CustomDataLayer_clone_set(PointerRNA *ptr, CustomData *data, int value, int type)
{
  int n = ((CustomDataLayer *)ptr->data) - data->layers;

  if (value == 0) {
    return;
  }

  CustomData_set_layer_clone_index(data, type, n);
}

static bool rna_MEdge_freestyle_edge_mark_get(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  MEdge *medge = (MEdge *)ptr->data;
  FreestyleEdge *fed = CustomData_get(&me->edata, (int)(medge - me->medge), CD_FREESTYLE_EDGE);

  return fed && (fed->flag & FREESTYLE_EDGE_MARK) != 0;
}

static void rna_MEdge_freestyle_edge_mark_set(PointerRNA *ptr, bool value)
{
  Mesh *me = rna_mesh(ptr);
  MEdge *medge = (MEdge *)ptr->data;
  FreestyleEdge *fed = CustomData_get(&me->edata, (int)(medge - me->medge), CD_FREESTYLE_EDGE);

  if (!fed) {
    fed = CustomData_add_layer(&me->edata, CD_FREESTYLE_EDGE, CD_CALLOC, NULL, me->totedge);
  }
  if (value) {
    fed->flag |= FREESTYLE_EDGE_MARK;
  }
  else {
    fed->flag &= ~FREESTYLE_EDGE_MARK;
  }
}

static bool rna_MPoly_freestyle_face_mark_get(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  MPoly *mpoly = (MPoly *)ptr->data;
  FreestyleFace *ffa = CustomData_get(&me->pdata, (int)(mpoly - me->mpoly), CD_FREESTYLE_FACE);

  return ffa && (ffa->flag & FREESTYLE_FACE_MARK) != 0;
}

static void rna_MPoly_freestyle_face_mark_set(PointerRNA *ptr, int value)
{
  Mesh *me = rna_mesh(ptr);
  MPoly *mpoly = (MPoly *)ptr->data;
  FreestyleFace *ffa = CustomData_get(&me->pdata, (int)(mpoly - me->mpoly), CD_FREESTYLE_FACE);

  if (!ffa) {
    ffa = CustomData_add_layer(&me->pdata, CD_FREESTYLE_FACE, CD_CALLOC, NULL, me->totpoly);
  }
  if (value) {
    ffa->flag |= FREESTYLE_FACE_MARK;
  }
  else {
    ffa->flag &= ~FREESTYLE_FACE_MARK;
  }
}

/* uv_layers */

DEFINE_CUSTOMDATA_LAYER_COLLECTION(uv_layer, ldata, CD_MLOOPUV)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(uv_layer, ldata, CD_MLOOPUV, active, MeshUVLoopLayer)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(uv_layer, ldata, CD_MLOOPUV, clone, MeshUVLoopLayer)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(
    uv_layer, ldata, CD_MLOOPUV, stencil, MeshUVLoopLayer)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(uv_layer, ldata, CD_MLOOPUV, render, MeshUVLoopLayer)

/* MeshUVLoopLayer */

static char *rna_MeshUVLoopLayer_path(PointerRNA *ptr)
{
  CustomDataLayer *cdl = ptr->data;
  char name_esc[sizeof(cdl->name) * 2];
  BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
  return BLI_sprintfN("uv_layers[\"%s\"]", name_esc);
}

static void rna_MeshUVLoopLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  rna_iterator_array_begin(
      iter, layer->data, sizeof(MLoopUV), (me->edit_mesh) ? 0 : me->totloop, 0, NULL);
}

static int rna_MeshUVLoopLayer_data_length(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  return (me->edit_mesh) ? 0 : me->totloop;
}

static bool rna_MeshUVLoopLayer_active_render_get(PointerRNA *ptr)
{
  return rna_CustomDataLayer_active_get(ptr, rna_mesh_ldata(ptr), CD_MLOOPUV, 1);
}

static bool rna_MeshUVLoopLayer_active_get(PointerRNA *ptr)
{
  return rna_CustomDataLayer_active_get(ptr, rna_mesh_ldata(ptr), CD_MLOOPUV, 0);
}

static bool rna_MeshUVLoopLayer_clone_get(PointerRNA *ptr)
{
  return rna_CustomDataLayer_clone_get(ptr, rna_mesh_ldata(ptr), CD_MLOOPUV);
}

static void rna_MeshUVLoopLayer_active_render_set(PointerRNA *ptr, bool value)
{
  rna_CustomDataLayer_active_set(ptr, rna_mesh_ldata(ptr), value, CD_MLOOPUV, 1);
}

static void rna_MeshUVLoopLayer_active_set(PointerRNA *ptr, bool value)
{
  rna_CustomDataLayer_active_set(ptr, rna_mesh_ldata(ptr), value, CD_MLOOPUV, 0);
}

static void rna_MeshUVLoopLayer_clone_set(PointerRNA *ptr, bool value)
{
  rna_CustomDataLayer_clone_set(ptr, rna_mesh_ldata(ptr), value, CD_MLOOPUV);
}

/* vertex_color_layers */

DEFINE_CUSTOMDATA_LAYER_COLLECTION(vertex_color, ldata, CD_MLOOPCOL)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(
    vertex_color, ldata, CD_MLOOPCOL, active, MeshLoopColorLayer)

static void rna_MeshLoopColorLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  rna_iterator_array_begin(
      iter, layer->data, sizeof(MLoopCol), (me->edit_mesh) ? 0 : me->totloop, 0, NULL);
}

static int rna_MeshLoopColorLayer_data_length(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  return (me->edit_mesh) ? 0 : me->totloop;
}

static bool rna_MeshLoopColorLayer_active_render_get(PointerRNA *ptr)
{
  return rna_CustomDataLayer_active_get(ptr, rna_mesh_ldata(ptr), CD_MLOOPCOL, 1);
}

static bool rna_MeshLoopColorLayer_active_get(PointerRNA *ptr)
{
  return rna_CustomDataLayer_active_get(ptr, rna_mesh_ldata(ptr), CD_MLOOPCOL, 0);
}

static void rna_MeshLoopColorLayer_active_render_set(PointerRNA *ptr, bool value)
{
  rna_CustomDataLayer_active_set(ptr, rna_mesh_ldata(ptr), value, CD_MLOOPCOL, 1);
}

static void rna_MeshLoopColorLayer_active_set(PointerRNA *ptr, bool value)
{
  rna_CustomDataLayer_active_set(ptr, rna_mesh_ldata(ptr), value, CD_MLOOPCOL, 0);
}

/* sculpt_vertex_color_layers */

DEFINE_CUSTOMDATA_LAYER_COLLECTION(sculpt_vertex_color, vdata, CD_PROP_COLOR)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(
    sculpt_vertex_color, vdata, CD_PROP_COLOR, active, MeshVertColorLayer)

static void rna_MeshVertColorLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  rna_iterator_array_begin(
      iter, layer->data, sizeof(MPropCol), (me->edit_mesh) ? 0 : me->totvert, 0, NULL);
}

static int rna_MeshVertColorLayer_data_length(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  return (me->edit_mesh) ? 0 : me->totvert;
}

static bool rna_MeshVertColorLayer_active_render_get(PointerRNA *ptr)
{
  return rna_CustomDataLayer_active_get(ptr, rna_mesh_vdata(ptr), CD_PROP_COLOR, 1);
}

static bool rna_MeshVertColorLayer_active_get(PointerRNA *ptr)
{
  return rna_CustomDataLayer_active_get(ptr, rna_mesh_vdata(ptr), CD_PROP_COLOR, 0);
}

static void rna_MeshVertColorLayer_active_render_set(PointerRNA *ptr, bool value)
{
  rna_CustomDataLayer_active_set(ptr, rna_mesh_vdata(ptr), value, CD_PROP_COLOR, 1);
}

static void rna_MeshVertColorLayer_active_set(PointerRNA *ptr, bool value)
{
  rna_CustomDataLayer_active_set(ptr, rna_mesh_vdata(ptr), value, CD_PROP_COLOR, 0);
}

static int rna_float_layer_check(CollectionPropertyIterator *UNUSED(iter), void *data)
{
  CustomDataLayer *layer = (CustomDataLayer *)data;
  return (layer->type != CD_PROP_FLOAT);
}

static void rna_Mesh_vertex_float_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  CustomData *vdata = rna_mesh_vdata(ptr);
  rna_iterator_array_begin(iter,
                           (void *)vdata->layers,
                           sizeof(CustomDataLayer),
                           vdata->totlayer,
                           0,
                           rna_float_layer_check);
}
static void rna_Mesh_polygon_float_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  CustomData *pdata = rna_mesh_pdata(ptr);
  rna_iterator_array_begin(iter,
                           (void *)pdata->layers,
                           sizeof(CustomDataLayer),
                           pdata->totlayer,
                           0,
                           rna_float_layer_check);
}

static int rna_Mesh_vertex_float_layers_length(PointerRNA *ptr)
{
  return CustomData_number_of_layers(rna_mesh_vdata(ptr), CD_PROP_FLOAT);
}
static int rna_Mesh_polygon_float_layers_length(PointerRNA *ptr)
{
  return CustomData_number_of_layers(rna_mesh_pdata(ptr), CD_PROP_FLOAT);
}

static int rna_int_layer_check(CollectionPropertyIterator *UNUSED(iter), void *data)
{
  CustomDataLayer *layer = (CustomDataLayer *)data;
  return (layer->type != CD_PROP_INT32);
}

static void rna_Mesh_vertex_int_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  CustomData *vdata = rna_mesh_vdata(ptr);
  rna_iterator_array_begin(iter,
                           (void *)vdata->layers,
                           sizeof(CustomDataLayer),
                           vdata->totlayer,
                           0,
                           rna_int_layer_check);
}
static void rna_Mesh_polygon_int_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  CustomData *pdata = rna_mesh_pdata(ptr);
  rna_iterator_array_begin(iter,
                           (void *)pdata->layers,
                           sizeof(CustomDataLayer),
                           pdata->totlayer,
                           0,
                           rna_int_layer_check);
}

static int rna_Mesh_vertex_int_layers_length(PointerRNA *ptr)
{
  return CustomData_number_of_layers(rna_mesh_vdata(ptr), CD_PROP_INT32);
}
static int rna_Mesh_polygon_int_layers_length(PointerRNA *ptr)
{
  return CustomData_number_of_layers(rna_mesh_pdata(ptr), CD_PROP_INT32);
}

static int rna_string_layer_check(CollectionPropertyIterator *UNUSED(iter), void *data)
{
  CustomDataLayer *layer = (CustomDataLayer *)data;
  return (layer->type != CD_PROP_STRING);
}

static void rna_Mesh_vertex_string_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  CustomData *vdata = rna_mesh_vdata(ptr);
  rna_iterator_array_begin(iter,
                           (void *)vdata->layers,
                           sizeof(CustomDataLayer),
                           vdata->totlayer,
                           0,
                           rna_string_layer_check);
}
static void rna_Mesh_polygon_string_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  CustomData *pdata = rna_mesh_pdata(ptr);
  rna_iterator_array_begin(iter,
                           (void *)pdata->layers,
                           sizeof(CustomDataLayer),
                           pdata->totlayer,
                           0,
                           rna_string_layer_check);
}

static int rna_Mesh_vertex_string_layers_length(PointerRNA *ptr)
{
  return CustomData_number_of_layers(rna_mesh_vdata(ptr), CD_PROP_STRING);
}
static int rna_Mesh_polygon_string_layers_length(PointerRNA *ptr)
{
  return CustomData_number_of_layers(rna_mesh_pdata(ptr), CD_PROP_STRING);
}

/* Skin vertices */
DEFINE_CUSTOMDATA_LAYER_COLLECTION(skin_vertice, vdata, CD_MVERT_SKIN)

static char *rna_MeshSkinVertexLayer_path(PointerRNA *ptr)
{
  CustomDataLayer *cdl = ptr->data;
  char name_esc[sizeof(cdl->name) * 2];
  BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
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

/* Paint mask */
DEFINE_CUSTOMDATA_LAYER_COLLECTION(vertex_paint_mask, vdata, CD_PAINT_MASK)

static char *rna_MeshPaintMaskLayer_path(PointerRNA *ptr)
{
  CustomDataLayer *cdl = ptr->data;
  char name_esc[sizeof(cdl->name) * 2];
  BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
  return BLI_sprintfN("vertex_paint_masks[\"%s\"]", name_esc);
}

static char *rna_MeshPaintMask_path(PointerRNA *ptr)
{
  return rna_VertCustomData_data_path(ptr, "vertex_paint_masks", CD_PAINT_MASK);
}

static void rna_MeshPaintMaskLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  rna_iterator_array_begin(iter, layer->data, sizeof(MFloatProperty), me->totvert, 0, NULL);
}

static int rna_MeshPaintMaskLayer_data_length(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  return me->totvert;
}

/* End paint mask */

/* Face maps */

DEFINE_CUSTOMDATA_LAYER_COLLECTION(face_map, pdata, CD_FACEMAP)
DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(
    face_map, pdata, CD_FACEMAP, active, MeshFaceMapLayer)

static char *rna_MeshFaceMapLayer_path(PointerRNA *ptr)
{
  CustomDataLayer *cdl = ptr->data;
  char name_esc[sizeof(cdl->name) * 2];
  BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
  return BLI_sprintfN("face_maps[\"%s\"]", name_esc);
}

static void rna_MeshFaceMapLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  rna_iterator_array_begin(iter, layer->data, sizeof(int), me->totpoly, 0, NULL);
}

static int rna_MeshFaceMapLayer_data_length(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  return me->totpoly;
}

static PointerRNA rna_Mesh_face_map_new(struct Mesh *me, ReportList *reports, const char *name)
{
  if (BKE_mesh_ensure_facemap_customdata(me) == false) {
    BKE_report(reports, RPT_ERROR, "Currently only single face map layers are supported");
    return PointerRNA_NULL;
  }

  CustomData *pdata = rna_mesh_pdata_helper(me);

  int index = CustomData_get_layer_index(pdata, CD_FACEMAP);
  BLI_assert(index != -1);
  CustomDataLayer *cdl = &pdata->layers[index];
  rna_cd_layer_name_set(pdata, cdl, name);

  PointerRNA ptr;
  RNA_pointer_create(&me->id, &RNA_MeshFaceMapLayer, cdl, &ptr);
  return ptr;
}

static void rna_Mesh_face_map_remove(struct Mesh *me,
                                     ReportList *reports,
                                     struct CustomDataLayer *layer)
{
  /* just for sanity check */
  {
    CustomData *pdata = rna_mesh_pdata_helper(me);
    int index = CustomData_get_layer_index(pdata, CD_FACEMAP);
    if (index != -1) {
      CustomDataLayer *layer_test = &pdata->layers[index];
      if (layer != layer_test) {
        /* don't show name, its likely freed memory */
        BKE_report(reports, RPT_ERROR, "Face map not in mesh");
        return;
      }
    }
  }

  if (BKE_mesh_clear_facemap_customdata(me) == false) {
    BKE_report(reports, RPT_ERROR, "Error removing face map");
  }
}

/* End face maps */

/* poly.vertices - this is faked loop access for convenience */
static int rna_MeshPoly_vertices_get_length(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
  MPoly *mp = (MPoly *)ptr->data;
  /* note, raw access uses dummy item, this _could_ crash,
   * watch out for this, mface uses it but it cant work here. */
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
#  if 0
static void rna_MeshPoly_material_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  Mesh *me = rna_mesh(ptr);
  *min = 0;
  *max = max_ii(0, me->totcol - 1);
}
#  endif

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

static int rna_MeshLoopTriangle_index_get(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  MLoopTri *ltri = (MLoopTri *)ptr->data;
  return (int)(ltri - me->runtime.looptris.array);
}

static int rna_MeshLoopTriangle_material_index_get(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  MLoopTri *ltri = (MLoopTri *)ptr->data;
  return me->mpoly[ltri->poly].mat_nr;
}

static bool rna_MeshLoopTriangle_use_smooth_get(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  MLoopTri *ltri = (MLoopTri *)ptr->data;
  return me->mpoly[ltri->poly].flag & ME_SMOOTH;
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

  for (a = 0, dvert = me->dvert; a < me->totvert; a++, dvert++) {
    for (b = 0; b < dvert->totweight; b++) {
      if (dw == &dvert->dw[b]) {
        return BLI_sprintfN("vertices[%d].groups[%d]", a, b);
      }
    }
  }

  return NULL;
}

static char *rna_MeshPolygon_path(PointerRNA *ptr)
{
  return BLI_sprintfN("polygons[%d]", (int)((MPoly *)ptr->data - rna_mesh(ptr)->mpoly));
}

static char *rna_MeshLoopTriangle_path(PointerRNA *ptr)
{
  return BLI_sprintfN("loop_triangles[%d]",
                      (int)((MLoopTri *)ptr->data - rna_mesh(ptr)->runtime.looptris.array));
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

static char *rna_VertCustomData_data_path(PointerRNA *ptr, const char *collection, int type)
{
  CustomDataLayer *cdl;
  Mesh *me = rna_mesh(ptr);
  CustomData *vdata = rna_mesh_vdata(ptr);
  int a, b, totvert = (me->edit_mesh) ? 0 : me->totvert;

  for (cdl = vdata->layers, a = 0; a < vdata->totlayer; cdl++, a++) {
    if (cdl->type == type) {
      b = ((char *)ptr->data - ((char *)cdl->data)) / CustomData_sizeof(type);
      if (b >= 0 && b < totvert) {
        char name_esc[sizeof(cdl->name) * 2];
        BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
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
  int a, b, totpoly = (me->edit_mesh) ? 0 : me->totpoly;

  for (cdl = pdata->layers, a = 0; a < pdata->totlayer; cdl++, a++) {
    if (cdl->type == type) {
      b = ((char *)ptr->data - ((char *)cdl->data)) / CustomData_sizeof(type);
      if (b >= 0 && b < totpoly) {
        char name_esc[sizeof(cdl->name) * 2];
        BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
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
  int a, b, totloop = (me->edit_mesh) ? 0 : me->totloop;

  for (cdl = ldata->layers, a = 0; a < ldata->totlayer; cdl++, a++) {
    if (cdl->type == type) {
      b = ((char *)ptr->data - ((char *)cdl->data)) / CustomData_sizeof(type);
      if (b >= 0 && b < totloop) {
        char name_esc[sizeof(cdl->name) * 2];
        BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
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

static char *rna_MeshLoopColorLayer_path(PointerRNA *ptr)
{
  CustomDataLayer *cdl = ptr->data;
  char name_esc[sizeof(cdl->name) * 2];
  BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
  return BLI_sprintfN("vertex_colors[\"%s\"]", name_esc);
}

static char *rna_MeshColor_path(PointerRNA *ptr)
{
  return rna_LoopCustomData_data_path(ptr, "vertex_colors", CD_MLOOPCOL);
}

static char *rna_MeshVertColorLayer_path(PointerRNA *ptr)
{
  CustomDataLayer *cdl = ptr->data;
  char name_esc[sizeof(cdl->name) * 2];
  BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
  return BLI_sprintfN("sculpt_vertex_colors[\"%s\"]", name_esc);
}

static char *rna_MeshVertColor_path(PointerRNA *ptr)
{
  return rna_VertCustomData_data_path(ptr, "sculpt_vertex_colors", CD_PROP_COLOR);
}

/**** Float Property Layer API ****/
static char *rna_MeshVertexFloatPropertyLayer_path(PointerRNA *ptr)
{
  CustomDataLayer *cdl = ptr->data;
  char name_esc[sizeof(cdl->name) * 2];
  BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
  return BLI_sprintfN("vertex_float_layers[\"%s\"]", name_esc);
}
static char *rna_MeshPolygonFloatPropertyLayer_path(PointerRNA *ptr)
{
  CustomDataLayer *cdl = ptr->data;
  char name_esc[sizeof(cdl->name) * 2];
  BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
  return BLI_sprintfN("polygon_float_layers[\"%s\"]", name_esc);
}

static char *rna_MeshVertexFloatProperty_path(PointerRNA *ptr)
{
  return rna_VertCustomData_data_path(ptr, "vertex_layers_float", CD_PROP_FLOAT);
}
static char *rna_MeshPolygonFloatProperty_path(PointerRNA *ptr)
{
  return rna_PolyCustomData_data_path(ptr, "polygon_layers_float", CD_PROP_FLOAT);
}

static void rna_MeshVertexFloatPropertyLayer_data_begin(CollectionPropertyIterator *iter,
                                                        PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  rna_iterator_array_begin(iter, layer->data, sizeof(MFloatProperty), me->totvert, 0, NULL);
}
static void rna_MeshPolygonFloatPropertyLayer_data_begin(CollectionPropertyIterator *iter,
                                                         PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  rna_iterator_array_begin(iter, layer->data, sizeof(MFloatProperty), me->totpoly, 0, NULL);
}

static int rna_MeshVertexFloatPropertyLayer_data_length(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  return me->totvert;
}
static int rna_MeshPolygonFloatPropertyLayer_data_length(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  return me->totpoly;
}

/**** Int Property Layer API ****/
static char *rna_MeshVertexIntPropertyLayer_path(PointerRNA *ptr)
{
  CustomDataLayer *cdl = ptr->data;
  char name_esc[sizeof(cdl->name) * 2];
  BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
  return BLI_sprintfN("vertex_int_layers[\"%s\"]", name_esc);
}
static char *rna_MeshPolygonIntPropertyLayer_path(PointerRNA *ptr)
{
  CustomDataLayer *cdl = ptr->data;
  char name_esc[sizeof(cdl->name) * 2];
  BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
  return BLI_sprintfN("polygon_int_layers[\"%s\"]", name_esc);
}

static char *rna_MeshVertexIntProperty_path(PointerRNA *ptr)
{
  return rna_VertCustomData_data_path(ptr, "vertex_layers_int", CD_PROP_INT32);
}
static char *rna_MeshPolygonIntProperty_path(PointerRNA *ptr)
{
  return rna_PolyCustomData_data_path(ptr, "polygon_layers_int", CD_PROP_INT32);
}

static void rna_MeshVertexIntPropertyLayer_data_begin(CollectionPropertyIterator *iter,
                                                      PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  rna_iterator_array_begin(iter, layer->data, sizeof(MIntProperty), me->totvert, 0, NULL);
}
static void rna_MeshPolygonIntPropertyLayer_data_begin(CollectionPropertyIterator *iter,
                                                       PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  rna_iterator_array_begin(iter, layer->data, sizeof(MIntProperty), me->totpoly, 0, NULL);
}

static int rna_MeshVertexIntPropertyLayer_data_length(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  return me->totvert;
}
static int rna_MeshPolygonIntPropertyLayer_data_length(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  return me->totpoly;
}

/**** String Property Layer API ****/
static char *rna_MeshVertexStringPropertyLayer_path(PointerRNA *ptr)
{
  CustomDataLayer *cdl = ptr->data;
  char name_esc[sizeof(cdl->name) * 2];
  BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
  return BLI_sprintfN("vertex_string_layers[\"%s\"]", name_esc);
}
static char *rna_MeshPolygonStringPropertyLayer_path(PointerRNA *ptr)
{
  CustomDataLayer *cdl = ptr->data;
  char name_esc[sizeof(cdl->name) * 2];
  BLI_str_escape(name_esc, cdl->name, sizeof(name_esc));
  return BLI_sprintfN("polygon_string_layers[\"%s\"]", name_esc);
}

static char *rna_MeshVertexStringProperty_path(PointerRNA *ptr)
{
  return rna_VertCustomData_data_path(ptr, "vertex_layers_string", CD_PROP_STRING);
}
static char *rna_MeshPolygonStringProperty_path(PointerRNA *ptr)
{
  return rna_PolyCustomData_data_path(ptr, "polygon_layers_string", CD_PROP_STRING);
}

static void rna_MeshVertexStringPropertyLayer_data_begin(CollectionPropertyIterator *iter,
                                                         PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  rna_iterator_array_begin(iter, layer->data, sizeof(MStringProperty), me->totvert, 0, NULL);
}
static void rna_MeshPolygonStringPropertyLayer_data_begin(CollectionPropertyIterator *iter,
                                                          PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  rna_iterator_array_begin(iter, layer->data, sizeof(MStringProperty), me->totpoly, 0, NULL);
}

static int rna_MeshVertexStringPropertyLayer_data_length(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  return me->totvert;
}
static int rna_MeshPolygonStringPropertyLayer_data_length(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  return me->totpoly;
}

/* XXX, we don't have proper byte string support yet, so for now use the (bytes + 1)
 * bmesh API exposes correct python/byte-string access. */
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

static char *rna_MeshFaceMap_path(PointerRNA *ptr)
{
  return rna_PolyCustomData_data_path(ptr, "face_maps", CD_FACEMAP);
}

/***************************************/

static int rna_Mesh_tot_vert_get(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  return me->edit_mesh ? me->edit_mesh->bm->totvertsel : 0;
}
static int rna_Mesh_tot_edge_get(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  return me->edit_mesh ? me->edit_mesh->bm->totedgesel : 0;
}
static int rna_Mesh_tot_face_get(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  return me->edit_mesh ? me->edit_mesh->bm->totfacesel : 0;
}

static PointerRNA rna_Mesh_vertex_color_new(struct Mesh *me, const char *name, const bool do_init)
{
  PointerRNA ptr;
  CustomData *ldata;
  CustomDataLayer *cdl = NULL;
  int index = ED_mesh_color_add(me, name, false, do_init);

  if (index != -1) {
    ldata = rna_mesh_ldata_helper(me);
    cdl = &ldata->layers[CustomData_get_layer_index_n(ldata, CD_MLOOPCOL, index)];
  }

  RNA_pointer_create(&me->id, &RNA_MeshLoopColorLayer, cdl, &ptr);
  return ptr;
}

static void rna_Mesh_vertex_color_remove(struct Mesh *me,
                                         ReportList *reports,
                                         CustomDataLayer *layer)
{
  if (ED_mesh_color_remove_named(me, layer->name) == false) {
    BKE_reportf(reports, RPT_ERROR, "Vertex color '%s' not found", layer->name);
  }
}

static PointerRNA rna_Mesh_sculpt_vertex_color_new(struct Mesh *me,
                                                   const char *name,
                                                   const bool do_init)
{
  PointerRNA ptr;
  CustomData *vdata;
  CustomDataLayer *cdl = NULL;
  int index = ED_mesh_sculpt_color_add(me, name, false, do_init);

  if (index != -1) {
    vdata = rna_mesh_vdata_helper(me);
    cdl = &vdata->layers[CustomData_get_layer_index_n(vdata, CD_PROP_COLOR, index)];
  }

  RNA_pointer_create(&me->id, &RNA_MeshVertColorLayer, cdl, &ptr);
  return ptr;
}

static void rna_Mesh_sculpt_vertex_color_remove(struct Mesh *me,
                                                ReportList *reports,
                                                CustomDataLayer *layer)
{
  if (ED_mesh_sculpt_color_remove_named(me, layer->name) == false) {
    BKE_reportf(reports, RPT_ERROR, "Sculpt vertex color '%s' not found", layer->name);
  }
}

#  define DEFINE_CUSTOMDATA_PROPERTY_API( \
      elemname, datatype, cd_prop_type, cdata, countvar, layertype) \
    static PointerRNA rna_Mesh_##elemname##_##datatype##_property_new(struct Mesh *me, \
                                                                      const char *name) \
    { \
      PointerRNA ptr; \
      CustomDataLayer *cdl = NULL; \
      int index; \
\
      CustomData_add_layer_named(&me->cdata, cd_prop_type, CD_DEFAULT, NULL, me->countvar, name); \
      index = CustomData_get_named_layer_index(&me->cdata, cd_prop_type, name); \
\
      cdl = (index == -1) ? NULL : &(me->cdata.layers[index]); \
\
      RNA_pointer_create(&me->id, &RNA_##layertype, cdl, &ptr); \
      return ptr; \
    }

DEFINE_CUSTOMDATA_PROPERTY_API(
    vertex, float, CD_PROP_FLOAT, vdata, totvert, MeshVertexFloatPropertyLayer)
DEFINE_CUSTOMDATA_PROPERTY_API(
    vertex, int, CD_PROP_INT32, vdata, totvert, MeshVertexIntPropertyLayer)
DEFINE_CUSTOMDATA_PROPERTY_API(
    vertex, string, CD_PROP_STRING, vdata, totvert, MeshVertexStringPropertyLayer)
DEFINE_CUSTOMDATA_PROPERTY_API(
    polygon, float, CD_PROP_FLOAT, pdata, totpoly, MeshPolygonFloatPropertyLayer)
DEFINE_CUSTOMDATA_PROPERTY_API(
    polygon, int, CD_PROP_INT32, pdata, totpoly, MeshPolygonIntPropertyLayer)
DEFINE_CUSTOMDATA_PROPERTY_API(
    polygon, string, CD_PROP_STRING, pdata, totpoly, MeshPolygonStringPropertyLayer)
#  undef DEFINE_CUSTOMDATA_PROPERTY_API

static PointerRNA rna_Mesh_uv_layers_new(struct Mesh *me, const char *name, const bool do_init)
{
  PointerRNA ptr;
  CustomData *ldata;
  CustomDataLayer *cdl = NULL;
  int index = ED_mesh_uv_texture_add(me, name, false, do_init);

  if (index != -1) {
    ldata = rna_mesh_ldata_helper(me);
    cdl = &ldata->layers[CustomData_get_layer_index_n(ldata, CD_MLOOPUV, index)];
  }

  RNA_pointer_create(&me->id, &RNA_MeshUVLoopLayer, cdl, &ptr);
  return ptr;
}

static void rna_Mesh_uv_layers_remove(struct Mesh *me, ReportList *reports, CustomDataLayer *layer)
{
  if (ED_mesh_uv_texture_remove_named(me, layer->name) == false) {
    BKE_reportf(reports, RPT_ERROR, "Texture layer '%s' not found", layer->name);
  }
}

static bool rna_Mesh_is_editmode_get(PointerRNA *ptr)
{
  Mesh *me = rna_mesh(ptr);
  return (me->edit_mesh != NULL);
}

/* only to quiet warnings */
static void UNUSED_FUNCTION(rna_mesh_unused)(void)
{
  /* unused functions made by macros */
  (void)rna_Mesh_skin_vertice_index_range;
  (void)rna_Mesh_vertex_paint_mask_index_range;
  (void)rna_Mesh_uv_layer_render_get;
  (void)rna_Mesh_uv_layer_render_index_get;
  (void)rna_Mesh_uv_layer_render_index_set;
  (void)rna_Mesh_uv_layer_render_set;
  (void)rna_Mesh_face_map_index_range;
  (void)rna_Mesh_face_map_active_index_set;
  (void)rna_Mesh_face_map_active_index_get;
  (void)rna_Mesh_face_map_active_set;
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
  RNA_def_struct_ui_text(
      srna, "Vertex Group Element", "Weight value of a vertex in a vertex group");
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
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_edit_weight");
}

static void rna_def_mvert(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MeshVertex", NULL);
  RNA_def_struct_sdna(srna, "MVert");
  RNA_def_struct_ui_text(srna, "Mesh Vertex", "Vertex in a Mesh data-block");
  RNA_def_struct_path_func(srna, "rna_MeshVertex_path");
  RNA_def_struct_ui_icon(srna, ICON_VERTEXSEL);

  prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_ui_text(prop, "Location", "");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "normal", PROP_FLOAT, PROP_DIRECTION);
  /* RNA_def_property_float_sdna(prop, NULL, "no"); */
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_float_funcs(
      prop, "rna_MeshVertex_normal_get", "rna_MeshVertex_normal_set", NULL);
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
  RNA_def_property_float_funcs(
      prop, "rna_MeshVertex_bevel_weight_get", "rna_MeshVertex_bevel_weight_set", NULL);
  RNA_def_property_ui_text(
      prop, "Bevel Weight", "Weight used by the Bevel modifier 'Only Vertices' option");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "groups", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_MeshVertex_groups_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "VertexGroupElement");
  RNA_def_property_ui_text(
      prop, "Groups", "Weights for the vertex groups this vertex is member of");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_MeshVertex_index_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Index", "Index of this vertex");

  prop = RNA_def_property(srna, "undeformed_co", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop,
      "Undeformed Location",
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
  RNA_def_struct_ui_text(srna, "Mesh Edge", "Edge in a Mesh data-block");
  RNA_def_struct_path_func(srna, "rna_MeshEdge_path");
  RNA_def_struct_ui_icon(srna, ICON_EDGESEL);

  prop = RNA_def_property(srna, "vertices", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "v1");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Vertices", "Vertex indices");
  /* XXX allows creating invalid meshes */

  prop = RNA_def_property(srna, "crease", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_funcs(prop, "rna_MEdge_crease_get", "rna_MEdge_crease_set", NULL);
  RNA_def_property_ui_text(
      prop, "Crease", "Weight used by the Subdivision Surface modifier for creasing");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "bevel_weight", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_funcs(
      prop, "rna_MEdge_bevel_weight_get", "rna_MEdge_bevel_weight_set", NULL);
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
  RNA_def_property_ui_text(prop, "Sharp", "Sharp edge for the Edge Split modifier");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "is_loose", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_LOOSEEDGE);
  RNA_def_property_ui_text(prop, "Loose", "Loose edge");

  prop = RNA_def_property(srna, "use_freestyle_mark", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_MEdge_freestyle_edge_mark_get", "rna_MEdge_freestyle_edge_mark_set");
  RNA_def_property_ui_text(prop, "Freestyle Edge Mark", "Edge mark for Freestyle line rendering");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_MeshEdge_index_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Index", "Index of this edge");
}

static void rna_def_mlooptri(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  const int splitnor_dim[] = {3, 3};

  srna = RNA_def_struct(brna, "MeshLoopTriangle", NULL);
  RNA_def_struct_sdna(srna, "MLoopTri");
  RNA_def_struct_ui_text(srna, "Mesh Loop Triangle", "Tessellated triangle in a Mesh data-block");
  RNA_def_struct_path_func(srna, "rna_MeshLoopTriangle_path");
  RNA_def_struct_ui_icon(srna, ICON_FACESEL);

  prop = RNA_def_property(srna, "vertices", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_array(prop, 3);
  RNA_def_property_int_funcs(prop, "rna_MeshLoopTriangle_verts_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Vertices", "Indices of triangle vertices");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "loops", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "tri");
  RNA_def_property_ui_text(prop, "Loops", "Indices of mesh loops that make up the triangle");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "polygon_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "poly");
  RNA_def_property_ui_text(
      prop, "Polygon", "Index of mesh polygon that the triangle is a part of");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "normal", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_MeshLoopTriangle_normal_get", NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Triangle Normal", "Local space unit length normal vector for this triangle");

  prop = RNA_def_property(srna, "split_normals", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_multi_array(prop, 2, splitnor_dim);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_MeshLoopTriangle_split_normals_get", NULL, NULL);
  RNA_def_property_ui_text(
      prop,
      "Split Normals",
      "Local space unit length split normals vectors of the vertices of this triangle "
      "(must be computed beforehand using calc_normals_split or calc_tangents)");

  prop = RNA_def_property(srna, "area", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_MeshLoopTriangle_area_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Triangle Area", "Area of this triangle");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_MeshLoopTriangle_index_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Index", "Index of this loop triangle");

  prop = RNA_def_property(srna, "material_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_MeshLoopTriangle_material_index_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Material Index", "");

  prop = RNA_def_property(srna, "use_smooth", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_MeshLoopTriangle_use_smooth_get", NULL);
  RNA_def_property_ui_text(prop, "Smooth", "");
}

static void rna_def_mloop(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MeshLoop", NULL);
  RNA_def_struct_sdna(srna, "MLoop");
  RNA_def_struct_ui_text(srna, "Mesh Loop", "Loop in a Mesh data-block");
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
  RNA_def_property_float_funcs(prop, "rna_MeshLoop_normal_get", "rna_MeshLoop_normal_set", NULL);
  RNA_def_property_ui_text(
      prop,
      "Normal",
      "Local space unit length split normal vector of this vertex for this polygon "
      "(must be computed beforehand using calc_normals_split or calc_tangents)");

  prop = RNA_def_property(srna, "tangent", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_MeshLoop_tangent_get", NULL, NULL);
  RNA_def_property_ui_text(
      prop,
      "Tangent",
      "Local space unit length tangent vector of this vertex for this polygon "
      "(must be computed beforehand using calc_tangents)");

  prop = RNA_def_property(srna, "bitangent_sign", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_MeshLoop_bitangent_sign_get", NULL, NULL);
  RNA_def_property_ui_text(
      prop,
      "Bitangent Sign",
      "Sign of the bitangent vector of this vertex for this polygon (must be computed "
      "beforehand using calc_tangents, bitangent = bitangent_sign * cross(normal, tangent))");

  prop = RNA_def_property(srna, "bitangent", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_MeshLoop_bitangent_get", NULL, NULL);
  RNA_def_property_ui_text(
      prop,
      "Bitangent",
      "Bitangent vector of this vertex for this polygon (must be computed beforehand using "
      "calc_tangents, use it only if really needed, slower access than bitangent_sign)");
}

static void rna_def_mpolygon(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;

  srna = RNA_def_struct(brna, "MeshPolygon", NULL);
  RNA_def_struct_sdna(srna, "MPoly");
  RNA_def_struct_ui_text(srna, "Mesh Polygon", "Polygon in a Mesh data-block");
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
#  if 0
  RNA_def_property_int_funcs(prop, NULL, NULL, "rna_MeshPoly_material_index_range");
#  endif
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

  prop = RNA_def_property(srna, "use_freestyle_mark", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_MPoly_freestyle_face_mark_get", "rna_MPoly_freestyle_face_mark_set");
  RNA_def_property_ui_text(prop, "Freestyle Face Mark", "Face mark for Freestyle line rendering");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "normal", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_MeshPolygon_normal_get", NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Polygon Normal", "Local space unit length normal vector for this polygon");

  prop = RNA_def_property(srna, "center", PROP_FLOAT, PROP_XYZ);
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

  func = RNA_def_function(srna, "flip", "rna_MeshPolygon_flip");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Invert winding of this polygon (flip its normal)");
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
  RNA_def_property_collection_funcs(prop,
                                    "rna_MeshUVLoopLayer_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_MeshUVLoopLayer_data_length",
                                    NULL,
                                    NULL,
                                    NULL);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshLoopLayer_name_set");
  RNA_def_property_ui_text(prop, "Name", "Name of UV map");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_MeshUVLoopLayer_active_get", "rna_MeshUVLoopLayer_active_set");
  RNA_def_property_ui_text(prop, "Active", "Set the map as active for display and editing");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "active_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "active_rnd", 0);
  RNA_def_property_boolean_funcs(
      prop, "rna_MeshUVLoopLayer_active_render_get", "rna_MeshUVLoopLayer_active_render_set");
  RNA_def_property_ui_text(prop, "Active Render", "Set the map as active for rendering");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "active_clone", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "active_clone", 0);
  RNA_def_property_boolean_funcs(
      prop, "rna_MeshUVLoopLayer_clone_get", "rna_MeshUVLoopLayer_clone_set");
  RNA_def_property_ui_text(prop, "Active Clone", "Set the map as active for cloning");
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
}

static void rna_def_mloopcol(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MeshLoopColorLayer", NULL);
  RNA_def_struct_ui_text(
      srna, "Mesh Vertex Color Layer", "Layer of vertex colors in a Mesh data-block");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_path_func(srna, "rna_MeshLoopColorLayer_path");
  RNA_def_struct_ui_icon(srna, ICON_GROUP_VCOL);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshLoopLayer_name_set");
  RNA_def_property_ui_text(prop, "Name", "Name of Vertex color layer");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_MeshLoopColorLayer_active_get", "rna_MeshLoopColorLayer_active_set");
  RNA_def_property_ui_text(prop, "Active", "Sets the layer as active for display and editing");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "active_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "active_rnd", 0);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_MeshLoopColorLayer_active_render_get",
                                 "rna_MeshLoopColorLayer_active_render_set");
  RNA_def_property_ui_text(prop, "Active Render", "Sets the layer as active for rendering");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshLoopColor");
  RNA_def_property_ui_text(prop, "Data", "");
  RNA_def_property_collection_funcs(prop,
                                    "rna_MeshLoopColorLayer_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_MeshLoopColorLayer_data_length",
                                    NULL,
                                    NULL,
                                    NULL);

  srna = RNA_def_struct(brna, "MeshLoopColor", NULL);
  RNA_def_struct_sdna(srna, "MLoopCol");
  RNA_def_struct_ui_text(srna, "Mesh Vertex Color", "Vertex loop colors in a Mesh");
  RNA_def_struct_path_func(srna, "rna_MeshColor_path");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_funcs(
      prop, "rna_MeshLoopColor_color_get", "rna_MeshLoopColor_color_set", NULL);
  RNA_def_property_ui_text(prop, "Color", "");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
}

static void rna_def_MPropCol(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MeshVertColorLayer", NULL);
  RNA_def_struct_ui_text(srna,
                         "Mesh Sculpt Vertex Color Layer",
                         "Layer of sculpt vertex colors in a Mesh data-block");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_path_func(srna, "rna_MeshVertColorLayer_path");
  RNA_def_struct_ui_icon(srna, ICON_GROUP_VCOL);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshVertexLayer_name_set");
  RNA_def_property_ui_text(prop, "Name", "Name of Sculpt Vertex color layer");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_MeshVertColorLayer_active_get", "rna_MeshVertColorLayer_active_set");
  RNA_def_property_ui_text(
      prop, "Active", "Sets the sculpt vertex color layer as active for display and editing");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "active_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "active_rnd", 0);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_MeshVertColorLayer_active_render_get",
                                 "rna_MeshVertColorLayer_active_render_set");
  RNA_def_property_ui_text(
      prop, "Active Render", "Sets the sculpt vertex color layer as active for rendering");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshVertColor");
  RNA_def_property_ui_text(prop, "Data", "");
  RNA_def_property_collection_funcs(prop,
                                    "rna_MeshVertColorLayer_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_MeshVertColorLayer_data_length",
                                    NULL,
                                    NULL,
                                    NULL);

  srna = RNA_def_struct(brna, "MeshVertColor", NULL);
  RNA_def_struct_sdna(srna, "MPropCol");
  RNA_def_struct_ui_text(srna, "Mesh Sculpt Vertex Color", "Vertex colors in a Mesh");
  RNA_def_struct_path_func(srna, "rna_MeshVertColor_path");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Color", "");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
}
static void rna_def_mproperties(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* Float */
#  define MESH_FLOAT_PROPERTY_LAYER(elemname) \
    srna = RNA_def_struct(brna, "Mesh" elemname "FloatPropertyLayer", NULL); \
    RNA_def_struct_sdna(srna, "CustomDataLayer"); \
    RNA_def_struct_ui_text(srna, \
                           "Mesh " elemname " Float Property Layer", \
                           "User defined layer of floating-point number values"); \
    RNA_def_struct_path_func(srna, "rna_Mesh" elemname "FloatPropertyLayer_path"); \
\
    prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE); \
    RNA_def_struct_name_property(srna, prop); \
    RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshAnyLayer_name_set"); \
    RNA_def_property_ui_text(prop, "Name", ""); \
    RNA_def_property_update(prop, 0, "rna_Mesh_update_data"); \
\
    prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE); \
    RNA_def_property_struct_type(prop, "Mesh" elemname "FloatProperty"); \
    RNA_def_property_ui_text(prop, "Data", ""); \
    RNA_def_property_collection_funcs(prop, \
                                      "rna_Mesh" elemname "FloatPropertyLayer_data_begin", \
                                      "rna_iterator_array_next", \
                                      "rna_iterator_array_end", \
                                      "rna_iterator_array_get", \
                                      "rna_Mesh" elemname "FloatPropertyLayer_data_length", \
                                      NULL, \
                                      NULL, \
                                      NULL); \
\
    srna = RNA_def_struct(brna, "Mesh" elemname "FloatProperty", NULL); \
    RNA_def_struct_sdna(srna, "MFloatProperty"); \
    RNA_def_struct_ui_text( \
        srna, \
        "Mesh " elemname " Float Property", \
        "User defined floating-point number value in a float properties layer"); \
    RNA_def_struct_path_func(srna, "rna_Mesh" elemname "FloatProperty_path"); \
\
    prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_NONE); \
    RNA_def_property_float_sdna(prop, NULL, "f"); \
    RNA_def_property_ui_text(prop, "Value", ""); \
    RNA_def_property_update(prop, 0, "rna_Mesh_update_data"); \
    ((void)0)

  /* Int */
#  define MESH_INT_PROPERTY_LAYER(elemname) \
    srna = RNA_def_struct(brna, "Mesh" elemname "IntPropertyLayer", NULL); \
    RNA_def_struct_sdna(srna, "CustomDataLayer"); \
    RNA_def_struct_ui_text(srna, \
                           "Mesh " elemname " Int Property Layer", \
                           "User defined layer of integer number values"); \
    RNA_def_struct_path_func(srna, "rna_Mesh" elemname "IntPropertyLayer_path"); \
\
    prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE); \
    RNA_def_struct_name_property(srna, prop); \
    RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshAnyLayer_name_set"); \
    RNA_def_property_ui_text(prop, "Name", ""); \
    RNA_def_property_update(prop, 0, "rna_Mesh_update_data"); \
\
    prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE); \
    RNA_def_property_struct_type(prop, "Mesh" elemname "IntProperty"); \
    RNA_def_property_ui_text(prop, "Data", ""); \
    RNA_def_property_collection_funcs(prop, \
                                      "rna_Mesh" elemname "IntPropertyLayer_data_begin", \
                                      "rna_iterator_array_next", \
                                      "rna_iterator_array_end", \
                                      "rna_iterator_array_get", \
                                      "rna_Mesh" elemname "IntPropertyLayer_data_length", \
                                      NULL, \
                                      NULL, \
                                      NULL); \
\
    srna = RNA_def_struct(brna, "Mesh" elemname "IntProperty", NULL); \
    RNA_def_struct_sdna(srna, "MIntProperty"); \
    RNA_def_struct_ui_text(srna, \
                           "Mesh " elemname " Int Property", \
                           "User defined integer number value in an integer properties layer"); \
    RNA_def_struct_path_func(srna, "rna_Mesh" elemname "IntProperty_path"); \
\
    prop = RNA_def_property(srna, "value", PROP_INT, PROP_NONE); \
    RNA_def_property_int_sdna(prop, NULL, "i"); \
    RNA_def_property_ui_text(prop, "Value", ""); \
    RNA_def_property_update(prop, 0, "rna_Mesh_update_data"); \
    ((void)0)

  /* String */
#  define MESH_STRING_PROPERTY_LAYER(elemname) \
    srna = RNA_def_struct(brna, "Mesh" elemname "StringPropertyLayer", NULL); \
    RNA_def_struct_sdna(srna, "CustomDataLayer"); \
    RNA_def_struct_ui_text(srna, \
                           "Mesh " elemname " String Property Layer", \
                           "User defined layer of string text values"); \
    RNA_def_struct_path_func(srna, "rna_Mesh" elemname "StringPropertyLayer_path"); \
\
    prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE); \
    RNA_def_struct_name_property(srna, prop); \
    RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshAnyLayer_name_set"); \
    RNA_def_property_ui_text(prop, "Name", ""); \
    RNA_def_property_update(prop, 0, "rna_Mesh_update_data"); \
\
    prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE); \
    RNA_def_property_struct_type(prop, "Mesh" elemname "StringProperty"); \
    RNA_def_property_ui_text(prop, "Data", ""); \
    RNA_def_property_collection_funcs(prop, \
                                      "rna_Mesh" elemname "StringPropertyLayer_data_begin", \
                                      "rna_iterator_array_next", \
                                      "rna_iterator_array_end", \
                                      "rna_iterator_array_get", \
                                      "rna_Mesh" elemname "StringPropertyLayer_data_length", \
                                      NULL, \
                                      NULL, \
                                      NULL); \
\
    srna = RNA_def_struct(brna, "Mesh" elemname "StringProperty", NULL); \
    RNA_def_struct_sdna(srna, "MStringProperty"); \
    RNA_def_struct_ui_text(srna, \
                           "Mesh " elemname " String Property", \
                           "User defined string text value in a string properties layer"); \
    RNA_def_struct_path_func(srna, "rna_Mesh" elemname "StringProperty_path"); \
\
    /* low level mesh data access, treat as bytes */ \
    prop = RNA_def_property(srna, "value", PROP_STRING, PROP_BYTESTRING); \
    RNA_def_property_string_sdna(prop, NULL, "s"); \
    RNA_def_property_string_funcs(prop, \
                                  "rna_MeshStringProperty_s_get", \
                                  "rna_MeshStringProperty_s_length", \
                                  "rna_MeshStringProperty_s_set"); \
    RNA_def_property_ui_text(prop, "Value", ""); \
    RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  MESH_FLOAT_PROPERTY_LAYER("Vertex");
  MESH_FLOAT_PROPERTY_LAYER("Polygon");
  MESH_INT_PROPERTY_LAYER("Vertex");
  MESH_INT_PROPERTY_LAYER("Polygon");
  MESH_STRING_PROPERTY_LAYER("Vertex")
  MESH_STRING_PROPERTY_LAYER("Polygon")
#  undef MESH_PROPERTY_LAYER
}

void rna_def_texmat_common(StructRNA *srna, const char *texspace_editable)
{
  PropertyRNA *prop;

  /* texture space */
  prop = RNA_def_property(srna, "auto_texspace", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "texflag", ME_AUTOSPACE);
  RNA_def_property_ui_text(
      prop,
      "Auto Texture Space",
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

  /* materials */
  prop = RNA_def_property(srna, "materials", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "mat", "totcol");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_ui_text(prop, "Materials", "");
  RNA_def_property_srna(prop, "IDMaterials"); /* see rna_ID.c */
  RNA_def_property_collection_funcs(
      prop, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "rna_IDMaterials_assign_int");
}

/* scene.objects */
/* mesh.vertices */
static void rna_def_mesh_vertices(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  /*  PropertyRNA *prop; */

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "MeshVertices");
  srna = RNA_def_struct(brna, "MeshVertices", NULL);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "Mesh Vertices", "Collection of mesh vertices");

  func = RNA_def_function(srna, "add", "ED_mesh_verts_add");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(
      func, "count", 0, 0, INT_MAX, "Count", "Number of vertices to add", 0, INT_MAX);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
#  if 0 /* BMESH_TODO Remove until BMesh merge */
  func = RNA_def_function(srna, "remove", "ED_mesh_verts_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_int(func, "count", 0, 0, INT_MAX, "Count", "Number of vertices to remove", 0, INT_MAX);
#  endif
}

/* mesh.edges */
static void rna_def_mesh_edges(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  /*  PropertyRNA *prop; */

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "MeshEdges");
  srna = RNA_def_struct(brna, "MeshEdges", NULL);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "Mesh Edges", "Collection of mesh edges");

  func = RNA_def_function(srna, "add", "ED_mesh_edges_add");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(func, "count", 0, 0, INT_MAX, "Count", "Number of edges to add", 0, INT_MAX);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
#  if 0 /* BMESH_TODO Remove until BMesh merge */
  func = RNA_def_function(srna, "remove", "ED_mesh_edges_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_int(func, "count", 0, 0, INT_MAX, "Count", "Number of edges to remove", 0, INT_MAX);
#  endif
}

/* mesh.loop_triangles */
static void rna_def_mesh_looptris(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  RNA_def_property_srna(cprop, "MeshLoopTriangles");
  srna = RNA_def_struct(brna, "MeshLoopTriangles", NULL);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(
      srna, "Mesh Loop Triangles", "Tessellation of mesh polygons into triangles");
}

/* mesh.loops */
static void rna_def_mesh_loops(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  /*PropertyRNA *prop;*/

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "MeshLoops");
  srna = RNA_def_struct(brna, "MeshLoops", NULL);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "Mesh Loops", "Collection of mesh loops");

  func = RNA_def_function(srna, "add", "ED_mesh_loops_add");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(func, "count", 0, 0, INT_MAX, "Count", "Number of loops to add", 0, INT_MAX);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
}

/* mesh.polygons */
static void rna_def_mesh_polygons(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "MeshPolygons");
  srna = RNA_def_struct(brna, "MeshPolygons", NULL);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "Mesh Polygons", "Collection of mesh polygons");

  prop = RNA_def_property(srna, "active", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "act_face");
  RNA_def_property_ui_text(prop, "Active Polygon", "The active polygon for this mesh");

  func = RNA_def_function(srna, "add", "ED_mesh_polys_add");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(
      func, "count", 0, 0, INT_MAX, "Count", "Number of polygons to add", 0, INT_MAX);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
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
  RNA_def_boolean(func,
                  "do_init",
                  true,
                  "",
                  "Whether new layer's data should be initialized by copying current active one");
  parm = RNA_def_pointer(func, "layer", "MeshLoopColorLayer", "", "The newly created layer");
  RNA_def_parameter_flags(parm, 0, PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Mesh_vertex_color_remove");
  RNA_def_function_ui_description(func, "Remove a vertex color layer");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "layer", "MeshLoopColorLayer", "", "The layer to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshLoopColorLayer");
  RNA_def_property_pointer_funcs(
      prop, "rna_Mesh_vertex_color_active_get", "rna_Mesh_vertex_color_active_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "Active Vertex Color Layer", "Active vertex color layer");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_edit_active_color");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop,
                             "rna_Mesh_vertex_color_active_index_get",
                             "rna_Mesh_vertex_color_active_index_set",
                             "rna_Mesh_vertex_color_index_range");
  RNA_def_property_ui_text(prop, "Active Vertex Color Index", "Active vertex color index");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_edit_active_color");
}

static void rna_def_vert_colors(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "VertColors");
  srna = RNA_def_struct(brna, "VertColors", NULL);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "Vert Colors", "Collection of sculpt vertex colors");

  func = RNA_def_function(srna, "new", "rna_Mesh_sculpt_vertex_color_new");
  RNA_def_function_ui_description(func, "Add a sculpt vertex color layer to Mesh");
  RNA_def_string(func, "name", "Col", 0, "", "Sculpt Vertex color name");
  RNA_def_boolean(func,
                  "do_init",
                  true,
                  "",
                  "Whether new layer's data should be initialized by copying current active one");
  parm = RNA_def_pointer(func, "layer", "MeshVertColorLayer", "", "The newly created layer");
  RNA_def_parameter_flags(parm, 0, PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Mesh_sculpt_vertex_color_remove");
  RNA_def_function_ui_description(func, "Remove a vertex color layer");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "layer", "MeshVertColorLayer", "", "The layer to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshVertColorLayer");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Mesh_sculpt_vertex_color_active_get",
                                 "rna_Mesh_sculpt_vertex_color_active_set",
                                 NULL,
                                 NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(
      prop, "Active Sculpt Vertex Color Layer", "Active sculpt vertex color layer");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_edit_active_color");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop,
                             "rna_Mesh_sculpt_vertex_color_active_index_get",
                             "rna_Mesh_sculpt_vertex_color_active_index_set",
                             "rna_Mesh_sculpt_vertex_color_index_range");
  RNA_def_property_ui_text(
      prop, "Active Sculpt Vertex Color Index", "Active sculpt vertex color index");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data_edit_active_color");
}

static void rna_def_uv_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "UVLoopLayers");
  srna = RNA_def_struct(brna, "UVLoopLayers", NULL);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "UV Loop Layers", "Collection of uv loop layers");

  func = RNA_def_function(srna, "new", "rna_Mesh_uv_layers_new");
  RNA_def_function_ui_description(func, "Add a UV map layer to Mesh");
  RNA_def_string(func, "name", "UVMap", 0, "", "UV map name");
  RNA_def_boolean(func,
                  "do_init",
                  true,
                  "",
                  "Whether new layer's data should be initialized by copying current active one, "
                  "or if none is active, with a default UVmap");
  parm = RNA_def_pointer(func, "layer", "MeshUVLoopLayer", "", "The newly created layer");
  RNA_def_parameter_flags(parm, 0, PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Mesh_uv_layers_remove");
  RNA_def_function_ui_description(func, "Remove a vertex color layer");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "layer", "MeshUVLoopLayer", "", "The layer to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshUVLoopLayer");
  RNA_def_property_pointer_funcs(
      prop, "rna_Mesh_uv_layer_active_get", "rna_Mesh_uv_layer_active_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "Active UV Loop Layer", "Active UV loop layer");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop,
                             "rna_Mesh_uv_layer_active_index_get",
                             "rna_Mesh_uv_layer_active_index_set",
                             "rna_Mesh_uv_layer_index_range");
  RNA_def_property_ui_text(prop, "Active UV Loop Layer Index", "Active UV loop layer index");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
}

/* mesh float layers */
static void rna_def_vertex_float_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "VertexFloatProperties");
  srna = RNA_def_struct(brna, "VertexFloatProperties", NULL);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "Vertex Float Properties", "Collection of float properties");

  func = RNA_def_function(srna, "new", "rna_Mesh_vertex_float_property_new");
  RNA_def_function_ui_description(func, "Add a float property layer to Mesh");
  RNA_def_string(func, "name", "Float Prop", 0, "", "Float property name");
  parm = RNA_def_pointer(
      func, "layer", "MeshVertexFloatPropertyLayer", "", "The newly created layer");
  RNA_def_parameter_flags(parm, 0, PARM_RNAPTR);
  RNA_def_function_return(func, parm);
}

/* mesh int layers */
static void rna_def_vertex_int_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "VertexIntProperties");
  srna = RNA_def_struct(brna, "VertexIntProperties", NULL);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "Vertex Int Properties", "Collection of int properties");

  func = RNA_def_function(srna, "new", "rna_Mesh_vertex_int_property_new");
  RNA_def_function_ui_description(func, "Add a integer property layer to Mesh");
  RNA_def_string(func, "name", "Int Prop", 0, "", "Int property name");
  parm = RNA_def_pointer(
      func, "layer", "MeshVertexIntPropertyLayer", "", "The newly created layer");
  RNA_def_parameter_flags(parm, 0, PARM_RNAPTR);
  RNA_def_function_return(func, parm);
}

/* mesh string layers */
static void rna_def_vertex_string_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "VertexStringProperties");
  srna = RNA_def_struct(brna, "VertexStringProperties", NULL);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "Vertex String Properties", "Collection of string properties");

  func = RNA_def_function(srna, "new", "rna_Mesh_vertex_string_property_new");
  RNA_def_function_ui_description(func, "Add a string property layer to Mesh");
  RNA_def_string(func, "name", "String Prop", 0, "", "String property name");
  parm = RNA_def_pointer(
      func, "layer", "MeshVertexStringPropertyLayer", "", "The newly created layer");
  RNA_def_parameter_flags(parm, 0, PARM_RNAPTR);
  RNA_def_function_return(func, parm);
}

/* mesh float layers */
static void rna_def_polygon_float_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "PolygonFloatProperties");
  srna = RNA_def_struct(brna, "PolygonFloatProperties", NULL);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "Polygon Float Properties", "Collection of float properties");

  func = RNA_def_function(srna, "new", "rna_Mesh_polygon_float_property_new");
  RNA_def_function_ui_description(func, "Add a float property layer to Mesh");
  RNA_def_string(func, "name", "Float Prop", 0, "", "Float property name");
  parm = RNA_def_pointer(
      func, "layer", "MeshPolygonFloatPropertyLayer", "", "The newly created layer");
  RNA_def_parameter_flags(parm, 0, PARM_RNAPTR);
  RNA_def_function_return(func, parm);
}

/* mesh int layers */
static void rna_def_polygon_int_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "PolygonIntProperties");
  srna = RNA_def_struct(brna, "PolygonIntProperties", NULL);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "Polygon Int Properties", "Collection of int properties");

  func = RNA_def_function(srna, "new", "rna_Mesh_polygon_int_property_new");
  RNA_def_function_ui_description(func, "Add a integer property layer to Mesh");
  RNA_def_string(func, "name", "Int Prop", 0, "", "Int property name");
  parm = RNA_def_pointer(
      func, "layer", "MeshPolygonIntPropertyLayer", "", "The newly created layer");
  RNA_def_parameter_flags(parm, 0, PARM_RNAPTR);
  RNA_def_function_return(func, parm);
}

/* mesh string layers */
static void rna_def_polygon_string_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "PolygonStringProperties");
  srna = RNA_def_struct(brna, "PolygonStringProperties", NULL);
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "Polygon String Properties", "Collection of string properties");

  func = RNA_def_function(srna, "new", "rna_Mesh_polygon_string_property_new");
  RNA_def_function_ui_description(func, "Add a string property layer to Mesh");
  RNA_def_string(func, "name", "String Prop", 0, "", "String property name");
  parm = RNA_def_pointer(
      func, "layer", "MeshPolygonStringPropertyLayer", "", "The newly created layer");
  RNA_def_parameter_flags(parm, 0, PARM_RNAPTR);
  RNA_def_function_return(func, parm);
}

static void rna_def_skin_vertices(BlenderRNA *brna, PropertyRNA *UNUSED(cprop))
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MeshSkinVertexLayer", NULL);
  RNA_def_struct_ui_text(
      srna, "Mesh Skin Vertex Layer", "Per-vertex skin data for use with the Skin modifier");
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
  RNA_def_property_collection_funcs(prop,
                                    "rna_MeshSkinVertexLayer_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_MeshSkinVertexLayer_data_length",
                                    NULL,
                                    NULL,
                                    NULL);

  /* SkinVertex struct */
  srna = RNA_def_struct(brna, "MeshSkinVertex", NULL);
  RNA_def_struct_sdna(srna, "MVertSkin");
  RNA_def_struct_ui_text(
      srna, "Skin Vertex", "Per-vertex skin data for use with the Skin modifier");
  RNA_def_struct_path_func(srna, "rna_MeshSkinVertex_path");

  prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_range(prop, 0.001, 100.0, 1, 3);
  RNA_def_property_ui_text(prop, "Radius", "Radius of the skin");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  /* Flags */

  prop = RNA_def_property(srna, "use_root", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MVERT_SKIN_ROOT);
  RNA_def_property_ui_text(prop,
                           "Root",
                           "Vertex is a root for rotation calculations and armature generation, "
                           "setting this flag does not clear other roots in the same mesh island");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "use_loose", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", MVERT_SKIN_LOOSE);
  RNA_def_property_ui_text(
      prop, "Loose", "If vertex has multiple adjacent edges, it is hulled to them directly");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
}

static void rna_def_paint_mask(BlenderRNA *brna, PropertyRNA *UNUSED(cprop))
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MeshPaintMaskLayer", NULL);
  RNA_def_struct_ui_text(srna, "Mesh Paint Mask Layer", "Per-vertex paint mask data");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_path_func(srna, "rna_MeshPaintMaskLayer_path");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshPaintMaskProperty");
  RNA_def_property_ui_text(prop, "Data", "");

  RNA_def_property_collection_funcs(prop,
                                    "rna_MeshPaintMaskLayer_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_MeshPaintMaskLayer_data_length",
                                    NULL,
                                    NULL,
                                    NULL);

  srna = RNA_def_struct(brna, "MeshPaintMaskProperty", NULL);
  RNA_def_struct_sdna(srna, "MFloatProperty");
  RNA_def_struct_ui_text(srna, "Mesh Paint Mask Property", "Floating-point paint mask value");
  RNA_def_struct_path_func(srna, "rna_MeshPaintMask_path");

  prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "f");
  RNA_def_property_ui_text(prop, "Value", "");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
}

static void rna_def_face_map(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MeshFaceMapLayer", NULL);
  RNA_def_struct_ui_text(srna, "Mesh Face Map Layer", "Per-face map index");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_path_func(srna, "rna_MeshFaceMapLayer_path");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MeshPolyLayer_name_set");
  RNA_def_property_ui_text(prop, "Name", "Name of face map layer");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshFaceMap");
  RNA_def_property_ui_text(prop, "Data", "");
  RNA_def_property_collection_funcs(prop,
                                    "rna_MeshFaceMapLayer_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_MeshFaceMapLayer_data_length",
                                    NULL,
                                    NULL,
                                    NULL);

  /* FaceMap struct */
  srna = RNA_def_struct(brna, "MeshFaceMap", NULL);
  RNA_def_struct_sdna(srna, "MIntProperty");
  RNA_def_struct_ui_text(srna, "Int Property", "");
  RNA_def_struct_path_func(srna, "rna_MeshFaceMap_path");

  prop = RNA_def_property(srna, "value", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "i");
  RNA_def_property_ui_text(prop, "Value", "");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");
}

static void rna_def_face_maps(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "MeshFaceMapLayers");
  srna = RNA_def_struct(brna, "MeshFaceMapLayers", NULL);
  RNA_def_struct_ui_text(srna, "Mesh Face Map Layer", "Per-face map index");
  RNA_def_struct_sdna(srna, "Mesh");
  RNA_def_struct_ui_text(srna, "Mesh Face Maps", "Collection of mesh face maps");

  /* add this since we only ever have one layer anyway, don't bother with active_index */
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshFaceMapLayer");
  RNA_def_property_pointer_funcs(prop, "rna_Mesh_face_map_active_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Active Face Map Layer", "");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "new", "rna_Mesh_face_map_new");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Add a float property layer to Mesh");
  RNA_def_string(func, "name", "Face Map", 0, "", "Face map name");
  parm = RNA_def_pointer(func, "layer", "MeshFaceMapLayer", "", "The newly created layer");
  RNA_def_parameter_flags(parm, 0, PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Mesh_face_map_remove");
  RNA_def_function_ui_description(func, "Remove a face map layer");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "layer", "MeshFaceMapLayer", "", "The layer to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);
}

static void rna_def_mesh(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Mesh", "ID");
  RNA_def_struct_ui_text(srna, "Mesh", "Mesh data-block defining geometric surfaces");
  RNA_def_struct_ui_icon(srna, ICON_MESH_DATA);

  prop = RNA_def_property(srna, "vertices", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "mvert", "totvert");
  RNA_def_property_struct_type(prop, "MeshVertex");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Vertices", "Vertices of the mesh");
  rna_def_mesh_vertices(brna, prop);

  prop = RNA_def_property(srna, "edges", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "medge", "totedge");
  RNA_def_property_struct_type(prop, "MeshEdge");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Edges", "Edges of the mesh");
  rna_def_mesh_edges(brna, prop);

  prop = RNA_def_property(srna, "loops", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "mloop", "totloop");
  RNA_def_property_struct_type(prop, "MeshLoop");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Loops", "Loops of the mesh (polygon corners)");
  rna_def_mesh_loops(brna, prop);

  prop = RNA_def_property(srna, "polygons", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "mpoly", "totpoly");
  RNA_def_property_struct_type(prop, "MeshPolygon");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Polygons", "Polygons of the mesh");
  rna_def_mesh_polygons(brna, prop);

  prop = RNA_def_property(srna, "loop_triangles", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "runtime.looptris.array", "runtime.looptris.len");
  RNA_def_property_struct_type(prop, "MeshLoopTriangle");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Loop Triangles", "Tessellation of mesh polygons into triangles");
  rna_def_mesh_looptris(brna, prop);

  /* TODO, should this be allowed to be its self? */
  prop = RNA_def_property(srna, "texture_mesh", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "texcomesh");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop,
      "Texture Mesh",
      "Use another mesh for texture indices (vertex indices must be aligned)");

  /* UV loop layers */
  prop = RNA_def_property(srna, "uv_layers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "ldata.layers", "ldata.totlayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_uv_layers_begin",
                                    NULL,
                                    NULL,
                                    NULL,
                                    "rna_Mesh_uv_layers_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "MeshUVLoopLayer");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "UV Loop Layers", "All UV loop layers");
  rna_def_uv_layers(brna, prop);

  prop = RNA_def_property(srna, "uv_layer_clone", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshUVLoopLayer");
  RNA_def_property_pointer_funcs(
      prop, "rna_Mesh_uv_layer_clone_get", "rna_Mesh_uv_layer_clone_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Clone UV Loop Layer", "UV loop layer to be used as cloning source");

  prop = RNA_def_property(srna, "uv_layer_clone_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop,
                             "rna_Mesh_uv_layer_clone_index_get",
                             "rna_Mesh_uv_layer_clone_index_set",
                             "rna_Mesh_uv_layer_index_range");
  RNA_def_property_ui_text(prop, "Clone UV Loop Layer Index", "Clone UV loop layer index");

  prop = RNA_def_property(srna, "uv_layer_stencil", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "MeshUVLoopLayer");
  RNA_def_property_pointer_funcs(
      prop, "rna_Mesh_uv_layer_stencil_get", "rna_Mesh_uv_layer_stencil_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Mask UV Loop Layer", "UV loop layer to mask the painted area");

  prop = RNA_def_property(srna, "uv_layer_stencil_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop,
                             "rna_Mesh_uv_layer_stencil_index_get",
                             "rna_Mesh_uv_layer_stencil_index_set",
                             "rna_Mesh_uv_layer_index_range");
  RNA_def_property_ui_text(prop, "Mask UV Loop Layer Index", "Mask UV loop layer index");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  /* Vertex colors */

  prop = RNA_def_property(srna, "vertex_colors", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "ldata.layers", "ldata.totlayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_vertex_colors_begin",
                                    NULL,
                                    NULL,
                                    NULL,
                                    "rna_Mesh_vertex_colors_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "MeshLoopColorLayer");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Vertex Colors", "All vertex colors");
  rna_def_loop_colors(brna, prop);

  /* Sculpt Vertex colors */

  prop = RNA_def_property(srna, "sculpt_vertex_colors", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "vdata.layers", "vdata.totlayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_sculpt_vertex_colors_begin",
                                    NULL,
                                    NULL,
                                    NULL,
                                    "rna_Mesh_sculpt_vertex_colors_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "MeshVertColorLayer");
  RNA_def_property_ui_text(prop, "Sculpt Vertex Colors", "All vertex colors");
  rna_def_vert_colors(brna, prop);

  /* TODO, edge customdata layers (bmesh py api can access already) */
  prop = RNA_def_property(srna, "vertex_layers_float", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "vdata.layers", "vdata.totlayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_vertex_float_layers_begin",
                                    NULL,
                                    NULL,
                                    NULL,
                                    "rna_Mesh_vertex_float_layers_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "MeshVertexFloatPropertyLayer");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Float Property Layers", "");
  rna_def_vertex_float_layers(brna, prop);

  prop = RNA_def_property(srna, "vertex_layers_int", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "vdata.layers", "vdata.totlayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_vertex_int_layers_begin",
                                    NULL,
                                    NULL,
                                    NULL,
                                    "rna_Mesh_vertex_int_layers_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "MeshVertexIntPropertyLayer");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Int Property Layers", "");
  rna_def_vertex_int_layers(brna, prop);

  prop = RNA_def_property(srna, "vertex_layers_string", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "vdata.layers", "vdata.totlayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_vertex_string_layers_begin",
                                    NULL,
                                    NULL,
                                    NULL,
                                    "rna_Mesh_vertex_string_layers_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "MeshVertexStringPropertyLayer");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "String Property Layers", "");
  rna_def_vertex_string_layers(brna, prop);

  prop = RNA_def_property(srna, "polygon_layers_float", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "pdata.layers", "pdata.totlayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_polygon_float_layers_begin",
                                    NULL,
                                    NULL,
                                    NULL,
                                    "rna_Mesh_polygon_float_layers_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "MeshPolygonFloatPropertyLayer");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Float Property Layers", "");
  rna_def_polygon_float_layers(brna, prop);

  prop = RNA_def_property(srna, "polygon_layers_int", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "pdata.layers", "pdata.totlayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_polygon_int_layers_begin",
                                    NULL,
                                    NULL,
                                    NULL,
                                    "rna_Mesh_polygon_int_layers_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "MeshPolygonIntPropertyLayer");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Int Property Layers", "");
  rna_def_polygon_int_layers(brna, prop);

  prop = RNA_def_property(srna, "polygon_layers_string", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "pdata.layers", "pdata.totlayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_polygon_string_layers_begin",
                                    NULL,
                                    NULL,
                                    NULL,
                                    "rna_Mesh_polygon_string_layers_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "MeshPolygonStringPropertyLayer");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "String Property Layers", "");
  rna_def_polygon_string_layers(brna, prop);

  /* face-maps */
  prop = RNA_def_property(srna, "face_maps", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "pdata.layers", "pdata.totlayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_face_maps_begin",
                                    NULL,
                                    NULL,
                                    NULL,
                                    "rna_Mesh_face_maps_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "MeshFaceMapLayer");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Face Map", "");
  rna_def_face_maps(brna, prop);

  /* Skin vertices */
  prop = RNA_def_property(srna, "skin_vertices", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "vdata.layers", "vdata.totlayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_skin_vertices_begin",
                                    NULL,
                                    NULL,
                                    NULL,
                                    "rna_Mesh_skin_vertices_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "MeshSkinVertexLayer");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Skin Vertices", "All skin vertices");
  rna_def_skin_vertices(brna, prop);
  /* End skin vertices */

  /* Paint mask */
  prop = RNA_def_property(srna, "vertex_paint_masks", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "vdata.layers", "vdata.totlayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Mesh_vertex_paint_masks_begin",
                                    NULL,
                                    NULL,
                                    NULL,
                                    "rna_Mesh_vertex_paint_masks_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "MeshPaintMaskLayer");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Vertex Paint Mask", "Vertex paint mask");
  rna_def_paint_mask(brna, prop);
  /* End paint mask */

  /* Attributes */
  rna_def_attributes_common(srna);

  /* Remesh */
  prop = RNA_def_property(srna, "remesh_voxel_size", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "remesh_voxel_size");
  RNA_def_property_range(prop, 0.0001f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0001f, FLT_MAX, 0.01, 4);
  RNA_def_property_ui_text(prop,
                           "Voxel Size",
                           "Size of the voxel in object space used for volume evaluation. Lower "
                           "values preserve finer details");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

  prop = RNA_def_property(srna, "remesh_voxel_adaptivity", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "remesh_voxel_adaptivity");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01, 4);
  RNA_def_property_ui_text(
      prop,
      "Adaptivity",
      "Reduces the final face count by simplifying geometry where detail is not needed, "
      "generating triangles. A value greater than 0 disables Fix Poles");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

  prop = RNA_def_property(srna, "use_remesh_smooth_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_REMESH_SMOOTH_NORMALS);
  RNA_def_property_ui_text(prop, "Smooth Normals", "Smooth the normals of the remesher result");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

  prop = RNA_def_property(srna, "use_remesh_fix_poles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_REMESH_FIX_POLES);
  RNA_def_property_ui_text(prop, "Fix Poles", "Produces less poles and a better topology flow");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

  prop = RNA_def_property(srna, "use_remesh_preserve_volume", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_REMESH_REPROJECT_VOLUME);
  RNA_def_property_ui_text(
      prop,
      "Preserve Volume",
      "Projects the mesh to preserve the volume and details of the original mesh");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

  prop = RNA_def_property(srna, "use_remesh_preserve_paint_mask", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_REMESH_REPROJECT_PAINT_MASK);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_text(prop, "Preserve Paint Mask", "Keep the current mask on the new mesh");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

  prop = RNA_def_property(srna, "use_remesh_preserve_sculpt_face_sets", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_REMESH_REPROJECT_SCULPT_FACE_SETS);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_text(
      prop, "Preserve Face Sets", "Keep the current Face Sets on the new mesh");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

  prop = RNA_def_property(srna, "use_remesh_preserve_vertex_colors", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_REMESH_REPROJECT_VERTEX_COLORS);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_text(
      prop, "Preserve Vertex Colors", "Keep the current vertex colors on the new mesh");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

  prop = RNA_def_property(srna, "remesh_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "remesh_mode");
  RNA_def_property_enum_items(prop, rna_enum_mesh_remesh_mode_items);
  RNA_def_property_ui_text(prop, "Remesh Mode", "");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

  /* End remesh */

  /* Symmetry */
  prop = RNA_def_property(srna, "use_mirror_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "symmetry", ME_SYMMETRY_X);
  RNA_def_property_ui_text(prop, "X", "Enable symmetry in the X axis");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

  prop = RNA_def_property(srna, "use_mirror_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "symmetry", ME_SYMMETRY_Y);
  RNA_def_property_ui_text(prop, "Y", "Enable symmetry in the Y axis");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

  prop = RNA_def_property(srna, "use_mirror_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "symmetry", ME_SYMMETRY_Z);
  RNA_def_property_ui_text(prop, "Z", "Enable symmetry in the Z axis");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");

  prop = RNA_def_property(srna, "use_mirror_vertex_group_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "editflag", ME_EDIT_VERTEX_GROUPS_X_SYMMETRY);
  RNA_def_property_ui_text(
      prop, "Vertex Groups X Symmetry", "Mirror the left/right vertex groups when painting");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");
  /* End Symmetry */

  prop = RNA_def_property(srna, "use_auto_smooth", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_AUTOSMOOTH);
  RNA_def_property_ui_text(
      prop,
      "Auto Smooth",
      "Auto smooth (based on smooth/sharp faces/edges and angle between faces), "
      "or use custom split normals data if available");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  prop = RNA_def_property(srna, "auto_smooth_angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "smoothresh");
  RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
  RNA_def_property_ui_text(prop,
                           "Auto Smooth Angle",
                           "Maximum angle between face normals that will be considered as smooth "
                           "(unused if custom split normals data are available)");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

  RNA_define_verify_sdna(false);
  prop = RNA_def_property(srna, "has_custom_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "", 0);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Has Custom Normals", "True if there are custom split normals data in this mesh");
  RNA_def_property_boolean_funcs(prop, "rna_Mesh_has_custom_normals_get", NULL);
  RNA_define_verify_sdna(true);

  prop = RNA_def_property(srna, "texco_mesh", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "texcomesh");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop, "Texture Space Mesh", "Derive texture coordinates from another mesh");

  prop = RNA_def_property(srna, "shape_keys", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "key");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_clear_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_ui_text(prop, "Shape Keys", "");

  /* texture space */
  prop = RNA_def_property(srna, "use_auto_texspace", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "texflag", ME_AUTOSPACE);
  RNA_def_property_ui_text(
      prop,
      "Auto Texture Space",
      "Adjust active object's texture space automatically when transforming object");
  RNA_def_property_update(prop, 0, "rna_Mesh_update_data");

#  if 0
  prop = RNA_def_property(srna, "texspace_location", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Texture Space Location", "Texture space location");
  RNA_def_property_editable_func(prop, "rna_Mesh_texspace_editable");
  RNA_def_property_float_funcs(
      prop, "rna_Mesh_texspace_loc_get", "rna_Mesh_texspace_loc_set", NULL);
  RNA_def_property_update(prop, 0, "rna_Mesh_update_draw");
#  endif

  /* editflag */
  prop = RNA_def_property(srna, "use_mirror_topology", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "editflag", ME_EDIT_MIRROR_TOPO);
  RNA_def_property_ui_text(prop,
                           "Topology Mirror",
                           "Use topology based mirroring "
                           "(for when both sides of mesh have matching, unique topology)");

  prop = RNA_def_property(srna, "use_paint_mask", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "editflag", ME_EDIT_PAINT_FACE_SEL);
  RNA_def_property_ui_text(prop, "Paint Mask", "Face selection masking for painting");
  RNA_def_property_ui_icon(prop, ICON_FACESEL, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_Mesh_update_facemask");

  prop = RNA_def_property(srna, "use_paint_mask_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "editflag", ME_EDIT_PAINT_VERT_SEL);
  RNA_def_property_ui_text(prop, "Vertex Selection", "Vertex selection masking for painting");
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
  RNA_def_property_ui_text(prop, "Selected Vertex Total", "Selected vertex count in editmode");
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
  rna_def_mlooptri(brna);
  rna_def_mloop(brna);
  rna_def_mpolygon(brna);
  rna_def_mloopuv(brna);
  rna_def_mloopcol(brna);
  rna_def_MPropCol(brna);
  rna_def_mproperties(brna);
  rna_def_face_map(brna);
}

#endif
