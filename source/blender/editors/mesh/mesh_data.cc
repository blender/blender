/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2009 Blender Foundation */

/** \file
 * \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BLI_array.hh"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_prototypes.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BLT_translation.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_paint.h"
#include "ED_screen.h"
#include "ED_uvedit.h"
#include "ED_view3d.h"

#include "GEO_mesh_split_edges.hh"

#include "mesh_intern.h" /* own include */

using blender::Array;
using blender::float2;
using blender::float3;
using blender::MutableSpan;
using blender::Span;

static CustomData *mesh_customdata_get_type(Mesh *me, const char htype, int *r_tot)
{
  CustomData *data;
  BMesh *bm = (me->edit_mesh) ? me->edit_mesh->bm : nullptr;
  int tot;

  switch (htype) {
    case BM_VERT:
      if (bm) {
        data = &bm->vdata;
        tot = bm->totvert;
      }
      else {
        data = &me->vdata;
        tot = me->totvert;
      }
      break;
    case BM_EDGE:
      if (bm) {
        data = &bm->edata;
        tot = bm->totedge;
      }
      else {
        data = &me->edata;
        tot = me->totedge;
      }
      break;
    case BM_LOOP:
      if (bm) {
        data = &bm->ldata;
        tot = bm->totloop;
      }
      else {
        data = &me->ldata;
        tot = me->totloop;
      }
      break;
    case BM_FACE:
      if (bm) {
        data = &bm->pdata;
        tot = bm->totface;
      }
      else {
        data = &me->pdata;
        tot = me->totpoly;
      }
      break;
    default:
      BLI_assert(0);
      tot = 0;
      data = nullptr;
      break;
  }

  *r_tot = tot;
  return data;
}

#define GET_CD_DATA(me, data) ((me)->edit_mesh ? &(me)->edit_mesh->bm->data : &(me)->data)

static void mesh_uv_reset_array(float **fuv, const int len)
{
  if (len == 3) {
    fuv[0][0] = 0.0;
    fuv[0][1] = 0.0;

    fuv[1][0] = 1.0;
    fuv[1][1] = 0.0;

    fuv[2][0] = 1.0;
    fuv[2][1] = 1.0;
  }
  else if (len == 4) {
    fuv[0][0] = 0.0;
    fuv[0][1] = 0.0;

    fuv[1][0] = 1.0;
    fuv[1][1] = 0.0;

    fuv[2][0] = 1.0;
    fuv[2][1] = 1.0;

    fuv[3][0] = 0.0;
    fuv[3][1] = 1.0;
    /* Make sure we ignore 2-sided faces. */
  }
  else if (len > 2) {
    float fac = 0.0f, dfac = 1.0f / float(len);

    dfac *= float(M_PI) * 2.0f;

    for (int i = 0; i < len; i++) {
      fuv[i][0] = 0.5f * sinf(fac) + 0.5f;
      fuv[i][1] = 0.5f * cosf(fac) + 0.5f;

      fac += dfac;
    }
  }
}

static void mesh_uv_reset_bmface(BMFace *f, const int cd_loop_uv_offset)
{
  Array<float *, BM_DEFAULT_NGON_STACK_SIZE> fuv(f->len);
  BMIter liter;
  BMLoop *l;
  int i;

  BM_ITER_ELEM_INDEX (l, &liter, f, BM_LOOPS_OF_FACE, i) {
    fuv[i] = ((float *)BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset));
  }

  mesh_uv_reset_array(fuv.data(), f->len);
}

static void mesh_uv_reset_mface(const blender::IndexRange poly, float2 *mloopuv)
{
  Array<float *, BM_DEFAULT_NGON_STACK_SIZE> fuv(poly.size());

  for (int i = 0; i < poly.size(); i++) {
    fuv[i] = mloopuv[poly[i]];
  }

  mesh_uv_reset_array(fuv.data(), poly.size());
}

void ED_mesh_uv_loop_reset_ex(Mesh *me, const int layernum)
{
  BMEditMesh *em = me->edit_mesh;

  if (em) {
    /* Collect BMesh UVs */
    const int cd_loop_uv_offset = CustomData_get_n_offset(
        &em->bm->ldata, CD_PROP_FLOAT2, layernum);

    BMFace *efa;
    BMIter iter;

    BLI_assert(cd_loop_uv_offset >= 0);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
        continue;
      }

      mesh_uv_reset_bmface(efa, cd_loop_uv_offset);
    }
  }
  else {
    /* Collect Mesh UVs */
    BLI_assert(CustomData_has_layer(&me->ldata, CD_PROP_FLOAT2));
    float2 *mloopuv = static_cast<float2 *>(
        CustomData_get_layer_n_for_write(&me->ldata, CD_PROP_FLOAT2, layernum, me->totloop));

    const blender::OffsetIndices polys = me->polys();
    for (const int i : polys.index_range()) {
      mesh_uv_reset_mface(polys[i], mloopuv);
    }
  }

  DEG_id_tag_update(&me->id, 0);
}

void ED_mesh_uv_loop_reset(bContext *C, Mesh *me)
{
  /* could be ldata or pdata */
  CustomData *ldata = GET_CD_DATA(me, ldata);
  const int layernum = CustomData_get_active_layer(ldata, CD_PROP_FLOAT2);
  ED_mesh_uv_loop_reset_ex(me, layernum);

  WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);
}

int ED_mesh_uv_add(
    Mesh *me, const char *name, const bool active_set, const bool do_init, ReportList *reports)
{
  /* NOTE: keep in sync with #ED_mesh_color_add. */

  BMEditMesh *em;
  int layernum_dst;

  if (!name) {
    name = DATA_("UVMap");
  }

  char unique_name[MAX_CUSTOMDATA_LAYER_NAME];
  BKE_id_attribute_calc_unique_name(&me->id, name, unique_name);
  bool is_init = false;

  if (me->edit_mesh) {
    em = me->edit_mesh;

    layernum_dst = CustomData_number_of_layers(&em->bm->ldata, CD_PROP_FLOAT2);
    if (layernum_dst >= MAX_MTFACE) {
      BKE_reportf(reports, RPT_WARNING, "Cannot add more than %i UV maps", MAX_MTFACE);
      return -1;
    }

    BM_data_layer_add_named(em->bm, &em->bm->ldata, CD_PROP_FLOAT2, unique_name);
    BM_uv_map_ensure_select_and_pin_attrs(em->bm);
    /* copy data from active UV */
    if (layernum_dst && do_init) {
      const int layernum_src = CustomData_get_active_layer(&em->bm->ldata, CD_PROP_FLOAT2);
      BM_data_layer_copy(em->bm, &em->bm->ldata, CD_PROP_FLOAT2, layernum_src, layernum_dst);

      is_init = true;
    }
    if (active_set || layernum_dst == 0) {
      CustomData_set_layer_active(&em->bm->ldata, CD_PROP_FLOAT2, layernum_dst);
    }
  }
  else {
    layernum_dst = CustomData_number_of_layers(&me->ldata, CD_PROP_FLOAT2);
    if (layernum_dst >= MAX_MTFACE) {
      BKE_reportf(reports, RPT_WARNING, "Cannot add more than %i UV maps", MAX_MTFACE);
      return -1;
    }

    if (CustomData_has_layer(&me->ldata, CD_PROP_FLOAT2) && do_init) {
      CustomData_add_layer_named_with_data(
          &me->ldata,
          CD_PROP_FLOAT2,
          MEM_dupallocN(CustomData_get_layer(&me->ldata, CD_PROP_FLOAT2)),
          me->totloop,
          unique_name,
          nullptr);

      is_init = true;
    }
    else {
      CustomData_add_layer_named(
          &me->ldata, CD_PROP_FLOAT2, CD_SET_DEFAULT, me->totloop, unique_name);
    }

    if (active_set || layernum_dst == 0) {
      CustomData_set_layer_active(&me->ldata, CD_PROP_FLOAT2, layernum_dst);
    }
  }

  /* don't overwrite our copied coords */
  if (!is_init && do_init) {
    ED_mesh_uv_loop_reset_ex(me, layernum_dst);
  }

  DEG_id_tag_update(&me->id, 0);
  WM_main_add_notifier(NC_GEOM | ND_DATA, me);

  return layernum_dst;
}

static const bool *mesh_loop_boolean_custom_data_get_by_name(const Mesh &mesh, const char *name)
{
  return static_cast<const bool *>(CustomData_get_layer_named(&mesh.ldata, CD_PROP_BOOL, name));
}

const bool *ED_mesh_uv_map_vert_select_layer_get(const Mesh *mesh, const int uv_index)
{
  using namespace blender::bke;
  char buffer[MAX_CUSTOMDATA_LAYER_NAME];
  const char *uv_name = CustomData_get_layer_name(&mesh->ldata, CD_PROP_FLOAT2, uv_index);
  return mesh_loop_boolean_custom_data_get_by_name(
      *mesh, BKE_uv_map_vert_select_name_get(uv_name, buffer));
}
const bool *ED_mesh_uv_map_edge_select_layer_get(const Mesh *mesh, const int uv_index)
{
  /* UV map edge selections are stored on face corners (loops) and not on edges
   * because we need selections per face edge, even when the edge is split in UV space. */

  using namespace blender::bke;
  char buffer[MAX_CUSTOMDATA_LAYER_NAME];
  const char *uv_name = CustomData_get_layer_name(&mesh->ldata, CD_PROP_FLOAT2, uv_index);
  return mesh_loop_boolean_custom_data_get_by_name(
      *mesh, BKE_uv_map_edge_select_name_get(uv_name, buffer));
}

const bool *ED_mesh_uv_map_pin_layer_get(const Mesh *mesh, const int uv_index)
{
  using namespace blender::bke;
  char buffer[MAX_CUSTOMDATA_LAYER_NAME];
  const char *uv_name = CustomData_get_layer_name(&mesh->ldata, CD_PROP_FLOAT2, uv_index);
  return mesh_loop_boolean_custom_data_get_by_name(*mesh,
                                                   BKE_uv_map_pin_name_get(uv_name, buffer));
}

static bool *ensure_corner_boolean_attribute(Mesh &mesh, const blender::StringRefNull name)
{
  bool *data = static_cast<bool *>(
      CustomData_get_layer_named_for_write(&mesh.ldata, CD_PROP_BOOL, name.c_str(), mesh.totloop));
  if (!data) {
    data = static_cast<bool *>(CustomData_add_layer_named(
        &mesh.ldata, CD_PROP_BOOL, CD_SET_DEFAULT, mesh.totpoly, name.c_str()));
  }
  return data;
}

bool *ED_mesh_uv_map_vert_select_layer_ensure(Mesh *mesh, const int uv_index)
{
  using namespace blender::bke;
  char buffer[MAX_CUSTOMDATA_LAYER_NAME];
  const char *uv_name = CustomData_get_layer_name(&mesh->ldata, CD_PROP_FLOAT2, uv_index);
  return ensure_corner_boolean_attribute(*mesh, BKE_uv_map_vert_select_name_get(uv_name, buffer));
}
bool *ED_mesh_uv_map_edge_select_layer_ensure(Mesh *mesh, const int uv_index)
{
  using namespace blender::bke;
  char buffer[MAX_CUSTOMDATA_LAYER_NAME];
  const char *uv_name = CustomData_get_layer_name(&mesh->ldata, CD_PROP_FLOAT2, uv_index);
  return ensure_corner_boolean_attribute(*mesh, BKE_uv_map_edge_select_name_get(uv_name, buffer));
}
bool *ED_mesh_uv_map_pin_layer_ensure(Mesh *mesh, const int uv_index)
{
  using namespace blender::bke;
  char buffer[MAX_CUSTOMDATA_LAYER_NAME];
  const char *uv_name = CustomData_get_layer_name(&mesh->ldata, CD_PROP_FLOAT2, uv_index);
  return ensure_corner_boolean_attribute(*mesh, BKE_uv_map_pin_name_get(uv_name, buffer));
}

void ED_mesh_uv_ensure(Mesh *me, const char *name)
{
  BMEditMesh *em;
  int layernum_dst;

  if (me->edit_mesh) {
    em = me->edit_mesh;

    layernum_dst = CustomData_number_of_layers(&em->bm->ldata, CD_PROP_FLOAT2);
    if (layernum_dst == 0) {
      ED_mesh_uv_add(me, name, true, true, nullptr);
    }
  }
  else {
    layernum_dst = CustomData_number_of_layers(&me->ldata, CD_PROP_FLOAT2);
    if (layernum_dst == 0) {
      ED_mesh_uv_add(me, name, true, true, nullptr);
    }
  }
}

int ED_mesh_color_add(
    Mesh *me, const char *name, const bool active_set, const bool do_init, ReportList *reports)
{
  /* If no name is supplied, provide a backwards compatible default. */
  if (!name) {
    name = "Col";
  }

  CustomDataLayer *layer = BKE_id_attribute_new(
      &me->id, name, CD_PROP_BYTE_COLOR, ATTR_DOMAIN_CORNER, reports);

  if (do_init) {
    const char *active_name = me->active_color_attribute;
    if (const CustomDataLayer *active_layer = BKE_id_attributes_color_find(&me->id, active_name)) {
      if (const BMEditMesh *em = me->edit_mesh) {
        BMesh &bm = *em->bm;
        const int src_i = CustomData_get_named_layer(&bm.ldata, CD_PROP_BYTE_COLOR, active_name);
        const int dst_i = CustomData_get_named_layer(&bm.ldata, CD_PROP_BYTE_COLOR, layer->name);
        BM_data_layer_copy(&bm, &bm.ldata, CD_PROP_BYTE_COLOR, src_i, dst_i);
      }
      else {
        memcpy(layer->data, active_layer->data, CustomData_get_elem_size(layer) * me->totloop);
      }
    }
  }

  if (active_set) {
    BKE_id_attributes_active_color_set(&me->id, layer->name);
  }

  DEG_id_tag_update(&me->id, 0);
  WM_main_add_notifier(NC_GEOM | ND_DATA, me);

  int dummy;
  const CustomData *data = mesh_customdata_get_type(me, BM_LOOP, &dummy);
  return CustomData_get_named_layer(data, CD_PROP_BYTE_COLOR, layer->name);
}

bool ED_mesh_color_ensure(Mesh *me, const char *name)
{
  using namespace blender;
  BLI_assert(me->edit_mesh == nullptr);
  if (me->attributes().contains(me->active_color_attribute)) {
    return true;
  }

  char unique_name[MAX_CUSTOMDATA_LAYER_NAME];
  BKE_id_attribute_calc_unique_name(&me->id, name, unique_name);
  if (!me->attributes_for_write().add(
          unique_name, ATTR_DOMAIN_CORNER, CD_PROP_BYTE_COLOR, bke::AttributeInitDefaultValue()))
  {
    return false;
  }

  BKE_id_attributes_active_color_set(&me->id, unique_name);
  BKE_id_attributes_default_color_set(&me->id, unique_name);
  BKE_mesh_tessface_clear(me);
  DEG_id_tag_update(&me->id, 0);

  return true;
}

/*********************** General poll ************************/

static bool layers_poll(bContext *C)
{
  Object *ob = ED_object_context(C);
  ID *data = (ob) ? static_cast<ID *>(ob->data) : nullptr;
  return (ob && !ID_IS_LINKED(ob) && !ID_IS_OVERRIDE_LIBRARY(ob) && ob->type == OB_MESH && data &&
          !ID_IS_LINKED(data) && !ID_IS_OVERRIDE_LIBRARY(data));
}

/*********************** UV texture operators ************************/

static bool uv_texture_remove_poll(bContext *C)
{
  if (!layers_poll(C)) {
    return false;
  }

  Object *ob = ED_object_context(C);
  Mesh *me = static_cast<Mesh *>(ob->data);
  CustomData *ldata = GET_CD_DATA(me, ldata);
  const int active = CustomData_get_active_layer(ldata, CD_PROP_FLOAT2);
  if (active != -1) {
    return true;
  }

  return false;
}

static int mesh_uv_texture_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  Mesh *me = static_cast<Mesh *>(ob->data);

  if (ED_mesh_uv_add(me, nullptr, true, true, op->reports) == -1) {
    return OPERATOR_CANCELLED;
  }

  if (ob->mode & OB_MODE_TEXTURE_PAINT) {
    Scene *scene = CTX_data_scene(C);
    ED_paint_proj_mesh_data_check(scene, ob, nullptr, nullptr, nullptr, nullptr);
    WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, nullptr);
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_uv_texture_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add UV Map";
  ot->description = "Add UV map";
  ot->idname = "MESH_OT_uv_texture_add";

  /* api callbacks */
  ot->poll = layers_poll;
  ot->exec = mesh_uv_texture_add_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int mesh_uv_texture_remove_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  Mesh *me = static_cast<Mesh *>(ob->data);

  CustomData *ldata = GET_CD_DATA(me, ldata);
  const char *name = CustomData_get_active_layer_name(ldata, CD_PROP_FLOAT2);
  if (!BKE_id_attribute_remove(&me->id, name, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  if (ob->mode & OB_MODE_TEXTURE_PAINT) {
    Scene *scene = CTX_data_scene(C);
    ED_paint_proj_mesh_data_check(scene, ob, nullptr, nullptr, nullptr, nullptr);
    WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, nullptr);
  }

  DEG_id_tag_update(&me->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, me);

  return OPERATOR_FINISHED;
}

void MESH_OT_uv_texture_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove UV Map";
  ot->description = "Remove UV map";
  ot->idname = "MESH_OT_uv_texture_remove";

  /* api callbacks */
  ot->poll = uv_texture_remove_poll;
  ot->exec = mesh_uv_texture_remove_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* *** CustomData clear functions, we need an operator for each *** */

static int mesh_customdata_clear_exec__internal(bContext *C,
                                                char htype,
                                                const eCustomDataType type)
{
  Mesh *me = ED_mesh_context(C);

  int tot;
  CustomData *data = mesh_customdata_get_type(me, htype, &tot);

  BLI_assert(CustomData_layertype_is_singleton(type) == true);

  if (CustomData_has_layer(data, type)) {
    if (me->edit_mesh) {
      BM_data_layer_free(me->edit_mesh->bm, data, type);
    }
    else {
      CustomData_free_layers(data, type, tot);
    }

    DEG_id_tag_update(&me->id, 0);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static int mesh_customdata_add_exec__internal(bContext *C, char htype, const eCustomDataType type)
{
  Mesh *mesh = ED_mesh_context(C);

  int tot;
  CustomData *data = mesh_customdata_get_type(mesh, htype, &tot);

  BLI_assert(CustomData_layertype_is_singleton(type) == true);

  if (mesh->edit_mesh) {
    BM_data_layer_add(mesh->edit_mesh->bm, data, type);
  }
  else {
    CustomData_add_layer(data, eCustomDataType(type), CD_SET_DEFAULT, tot);
  }

  DEG_id_tag_update(&mesh->id, 0);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, mesh);

  return CustomData_has_layer(data, type) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

/* Clear Mask */
static bool mesh_customdata_mask_clear_poll(bContext *C)
{
  Object *ob = ED_object_context(C);
  if (ob && ob->type == OB_MESH) {
    Mesh *me = static_cast<Mesh *>(ob->data);

    /* special case - can't run this if we're in sculpt mode */
    if (ob->mode & OB_MODE_SCULPT) {
      return false;
    }

    if (!ID_IS_LINKED(me) && !ID_IS_OVERRIDE_LIBRARY(me)) {
      CustomData *data = GET_CD_DATA(me, vdata);
      if (CustomData_has_layer(data, CD_PAINT_MASK)) {
        return true;
      }
      data = GET_CD_DATA(me, ldata);
      if (CustomData_has_layer(data, CD_GRID_PAINT_MASK)) {
        return true;
      }
    }
  }
  return false;
}
static int mesh_customdata_mask_clear_exec(bContext *C, wmOperator * /*op*/)
{
  int ret_a = mesh_customdata_clear_exec__internal(C, BM_VERT, CD_PAINT_MASK);
  int ret_b = mesh_customdata_clear_exec__internal(C, BM_LOOP, CD_GRID_PAINT_MASK);

  if (ret_a == OPERATOR_FINISHED || ret_b == OPERATOR_FINISHED) {
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void MESH_OT_customdata_mask_clear(wmOperatorType *ot)
{
  /* NOTE: no create_mask yet */

  /* identifiers */
  ot->name = "Clear Sculpt Mask Data";
  ot->idname = "MESH_OT_customdata_mask_clear";
  ot->description = "Clear vertex sculpt masking data from the mesh";

  /* api callbacks */
  ot->exec = mesh_customdata_mask_clear_exec;
  ot->poll = mesh_customdata_mask_clear_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/**
 * Clear Skin
 * \return -1 invalid state, 0 no skin, 1 has skin.
 */
static int mesh_customdata_skin_state(bContext *C)
{
  Object *ob = ED_object_context(C);

  if (ob && ob->type == OB_MESH) {
    Mesh *me = static_cast<Mesh *>(ob->data);
    if (!ID_IS_LINKED(me) && !ID_IS_OVERRIDE_LIBRARY(me)) {
      CustomData *data = GET_CD_DATA(me, vdata);
      return CustomData_has_layer(data, CD_MVERT_SKIN);
    }
  }
  return -1;
}

static bool mesh_customdata_skin_add_poll(bContext *C)
{
  return (mesh_customdata_skin_state(C) == 0);
}

static int mesh_customdata_skin_add_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = ED_object_context(C);
  Mesh *me = static_cast<Mesh *>(ob->data);

  BKE_mesh_ensure_skin_customdata(me);

  DEG_id_tag_update(&me->id, 0);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);

  return OPERATOR_FINISHED;
}

void MESH_OT_customdata_skin_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Skin Data";
  ot->idname = "MESH_OT_customdata_skin_add";
  ot->description = "Add a vertex skin layer";

  /* api callbacks */
  ot->exec = mesh_customdata_skin_add_exec;
  ot->poll = mesh_customdata_skin_add_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static bool mesh_customdata_skin_clear_poll(bContext *C)
{
  return (mesh_customdata_skin_state(C) == 1);
}

static int mesh_customdata_skin_clear_exec(bContext *C, wmOperator * /*op*/)
{
  return mesh_customdata_clear_exec__internal(C, BM_VERT, CD_MVERT_SKIN);
}

void MESH_OT_customdata_skin_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Skin Data";
  ot->idname = "MESH_OT_customdata_skin_clear";
  ot->description = "Clear vertex skin layer";

  /* api callbacks */
  ot->exec = mesh_customdata_skin_clear_exec;
  ot->poll = mesh_customdata_skin_clear_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* Clear custom loop normals */
static int mesh_customdata_custom_splitnormals_add_exec(bContext *C, wmOperator * /*op*/)
{
  using namespace blender;
  Mesh *me = ED_mesh_context(C);
  if (BKE_mesh_has_custom_loop_normals(me)) {
    return OPERATOR_CANCELLED;
  }

  if (me->edit_mesh) {
    BMesh &bm = *me->edit_mesh->bm;
    /* Tag edges as sharp according to smooth threshold if needed,
     * to preserve auto-smooth shading. */
    if (me->flag & ME_AUTOSMOOTH) {
      BM_edges_sharp_from_angle_set(&bm, me->smoothresh);
    }

    BM_data_layer_add(&bm, &bm.ldata, CD_CUSTOMLOOPNORMAL);
  }
  else {
    /* Tag edges as sharp according to smooth threshold if needed,
     * to preserve auto-smooth shading. */
    if (me->flag & ME_AUTOSMOOTH) {
      bke::MutableAttributeAccessor attributes = me->attributes_for_write();
      bke::SpanAttributeWriter<bool> sharp_edges = attributes.lookup_or_add_for_write_span<bool>(
          "sharp_edge", ATTR_DOMAIN_EDGE);
      const bool *sharp_faces = static_cast<const bool *>(
          CustomData_get_layer_named(&me->pdata, CD_PROP_BOOL, "sharp_face"));
      bke::mesh::edges_sharp_from_angle_set(me->polys(),
                                            me->corner_verts(),
                                            me->corner_edges(),
                                            me->poly_normals(),
                                            sharp_faces,
                                            me->smoothresh,
                                            sharp_edges.span);
      sharp_edges.finish();
    }

    CustomData_add_layer(&me->ldata, CD_CUSTOMLOOPNORMAL, CD_SET_DEFAULT, me->totloop);
  }

  DEG_id_tag_update(&me->id, 0);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);

  return OPERATOR_FINISHED;
}

void MESH_OT_customdata_custom_splitnormals_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Custom Split Normals Data";
  ot->idname = "MESH_OT_customdata_custom_splitnormals_add";
  ot->description = "Add a custom split normals layer, if none exists yet";

  /* api callbacks */
  ot->exec = mesh_customdata_custom_splitnormals_add_exec;
  ot->poll = ED_operator_editable_mesh;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int mesh_customdata_custom_splitnormals_clear_exec(bContext *C, wmOperator * /*op*/)
{
  Mesh *me = ED_mesh_context(C);

  if (BKE_mesh_has_custom_loop_normals(me)) {
    BMEditMesh *em = me->edit_mesh;
    if (em != nullptr && em->bm->lnor_spacearr != nullptr) {
      BKE_lnor_spacearr_clear(em->bm->lnor_spacearr);
    }
    return mesh_customdata_clear_exec__internal(C, BM_LOOP, CD_CUSTOMLOOPNORMAL);
  }
  return OPERATOR_CANCELLED;
}

void MESH_OT_customdata_custom_splitnormals_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Custom Split Normals Data";
  ot->idname = "MESH_OT_customdata_custom_splitnormals_clear";
  ot->description = "Remove the custom split normals layer, if it exists";

  /* api callbacks */
  ot->exec = mesh_customdata_custom_splitnormals_clear_exec;
  ot->poll = ED_operator_editable_mesh;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* Edge crease. */

static int mesh_customdata_crease_edge_state(bContext *C)
{
  const Object *ob = ED_object_context(C);

  if (ob && ob->type == OB_MESH) {
    const Mesh *mesh = static_cast<Mesh *>(ob->data);
    if (!ID_IS_LINKED(mesh)) {
      const CustomData *data = GET_CD_DATA(mesh, edata);
      return CustomData_has_layer(data, CD_CREASE);
    }
  }
  return -1;
}

static bool mesh_customdata_crease_edge_add_poll(bContext *C)
{
  return mesh_customdata_crease_edge_state(C) == 0;
}

static int mesh_customdata_crease_edge_add_exec(bContext *C, wmOperator * /*op*/)
{
  return mesh_customdata_add_exec__internal(C, BM_EDGE, CD_CREASE);
}

void MESH_OT_customdata_crease_edge_add(wmOperatorType *ot)
{
  ot->name = "Add Edge Crease";
  ot->idname = "MESH_OT_customdata_crease_edge_add";
  ot->description = "Add an edge crease layer";

  ot->exec = mesh_customdata_crease_edge_add_exec;
  ot->poll = mesh_customdata_crease_edge_add_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static bool mesh_customdata_crease_edge_clear_poll(bContext *C)
{
  return mesh_customdata_crease_edge_state(C) == 1;
}

static int mesh_customdata_crease_edge_clear_exec(bContext *C, wmOperator * /*op*/)
{
  return mesh_customdata_clear_exec__internal(C, BM_EDGE, CD_CREASE);
}

void MESH_OT_customdata_crease_edge_clear(wmOperatorType *ot)
{
  ot->name = "Clear Edge Crease";
  ot->idname = "MESH_OT_customdata_crease_edge_clear";
  ot->description = "Clear the edge crease layer";

  ot->exec = mesh_customdata_crease_edge_clear_exec;
  ot->poll = mesh_customdata_crease_edge_clear_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* Vertex crease. */

static int mesh_customdata_crease_vertex_state(bContext *C)
{
  const Object *object = ED_object_context(C);

  if (object && object->type == OB_MESH) {
    const Mesh *mesh = static_cast<Mesh *>(object->data);
    if (!ID_IS_LINKED(mesh)) {
      const CustomData *data = GET_CD_DATA(mesh, vdata);
      return CustomData_has_layer(data, CD_CREASE);
    }
  }
  return -1;
}

static bool mesh_customdata_crease_vertex_add_poll(bContext *C)
{
  return mesh_customdata_crease_vertex_state(C) == 0;
}

static int mesh_customdata_crease_vertex_add_exec(bContext *C, wmOperator * /*op*/)
{
  return mesh_customdata_add_exec__internal(C, BM_VERT, CD_CREASE);
}

void MESH_OT_customdata_crease_vertex_add(wmOperatorType *ot)
{
  ot->name = "Add Vertex Crease";
  ot->idname = "MESH_OT_customdata_crease_vertex_add";
  ot->description = "Add a vertex crease layer";

  ot->exec = mesh_customdata_crease_vertex_add_exec;
  ot->poll = mesh_customdata_crease_vertex_add_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static bool mesh_customdata_crease_vertex_clear_poll(bContext *C)
{
  return (mesh_customdata_crease_vertex_state(C) == 1);
}

static int mesh_customdata_crease_vertex_clear_exec(bContext *C, wmOperator * /*op*/)
{
  return mesh_customdata_clear_exec__internal(C, BM_VERT, CD_CREASE);
}

void MESH_OT_customdata_crease_vertex_clear(wmOperatorType *ot)
{
  ot->name = "Clear Vertex Crease";
  ot->idname = "MESH_OT_customdata_crease_vertex_clear";
  ot->description = "Clear the vertex crease layer";

  ot->exec = mesh_customdata_crease_vertex_clear_exec;
  ot->poll = mesh_customdata_crease_vertex_clear_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************** Add Geometry Layers *************************/

void ED_mesh_update(Mesh *mesh, bContext *C, bool calc_edges, bool calc_edges_loose)
{
  if (calc_edges || ((mesh->totpoly || mesh->totface) && mesh->totedge == 0)) {
    BKE_mesh_calc_edges(mesh, calc_edges, true);
  }

  if (calc_edges_loose) {
    mesh->runtime->loose_edges_cache.tag_dirty();
  }

  /* Default state is not to have tessface's so make sure this is the case. */
  BKE_mesh_tessface_clear(mesh);

  mesh->runtime->vert_normals_dirty = true;
  mesh->runtime->poly_normals_dirty = true;

  DEG_id_tag_update(&mesh->id, 0);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, mesh);
}

bool ED_mesh_edge_is_loose(const Mesh *mesh, const int index)
{
  using namespace blender;
  const bke::LooseEdgeCache &loose_edges = mesh->loose_edges();
  return loose_edges.count > 0 && loose_edges.is_loose_bits[index];
}

static void mesh_add_verts(Mesh *mesh, int len)
{
  using namespace blender;
  if (len == 0) {
    return;
  }

  int totvert = mesh->totvert + len;
  CustomData vdata;
  CustomData_copy_layout(&mesh->vdata, &vdata, CD_MASK_MESH.vmask, CD_SET_DEFAULT, totvert);
  CustomData_copy_data(&mesh->vdata, &vdata, 0, 0, mesh->totvert);

  if (!CustomData_has_layer_named(&vdata, CD_PROP_FLOAT3, "position")) {
    CustomData_add_layer_named(&vdata, CD_PROP_FLOAT3, CD_SET_DEFAULT, totvert, "position");
  }

  CustomData_free(&mesh->vdata, mesh->totvert);
  mesh->vdata = vdata;

  BKE_mesh_runtime_clear_cache(mesh);

  mesh->totvert = totvert;

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".select_vert", ATTR_DOMAIN_POINT);
  select_vert.span.take_back(len).fill(true);
  select_vert.finish();
}

static void mesh_add_edges(Mesh *mesh, int len)
{
  using namespace blender;
  CustomData edata;
  int totedge;

  if (len == 0) {
    return;
  }

  totedge = mesh->totedge + len;

  /* Update custom-data. */
  CustomData_copy_layout(&mesh->edata, &edata, CD_MASK_MESH.emask, CD_SET_DEFAULT, totedge);
  CustomData_copy_data(&mesh->edata, &edata, 0, 0, mesh->totedge);

  if (!CustomData_has_layer_named(&edata, CD_PROP_INT32_2D, ".edge_verts")) {
    CustomData_add_layer_named(&edata, CD_PROP_INT32_2D, CD_SET_DEFAULT, totedge, ".edge_verts");
  }

  CustomData_free(&mesh->edata, mesh->totedge);
  mesh->edata = edata;

  BKE_mesh_runtime_clear_cache(mesh);

  mesh->totedge = totedge;

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> select_edge = attributes.lookup_or_add_for_write_span<bool>(
      ".select_edge", ATTR_DOMAIN_EDGE);
  select_edge.span.take_back(len).fill(true);
  select_edge.finish();
}

static void mesh_add_loops(Mesh *mesh, int len)
{
  CustomData ldata;
  int totloop;

  if (len == 0) {
    return;
  }

  totloop = mesh->totloop + len; /* new face count */

  /* update customdata */
  CustomData_copy_layout(&mesh->ldata, &ldata, CD_MASK_MESH.lmask, CD_SET_DEFAULT, totloop);
  CustomData_copy_data(&mesh->ldata, &ldata, 0, 0, mesh->totloop);

  if (!CustomData_has_layer_named(&ldata, CD_PROP_INT32, ".corner_vert")) {
    CustomData_add_layer_named(&ldata, CD_PROP_INT32, CD_SET_DEFAULT, totloop, ".corner_vert");
  }
  if (!CustomData_has_layer_named(&ldata, CD_PROP_INT32, ".corner_edge")) {
    CustomData_add_layer_named(&ldata, CD_PROP_INT32, CD_SET_DEFAULT, totloop, ".corner_edge");
  }

  BKE_mesh_runtime_clear_cache(mesh);

  CustomData_free(&mesh->ldata, mesh->totloop);
  mesh->ldata = ldata;

  mesh->totloop = totloop;

  /* Keep the last poly offset up to date with the corner total (they must be the same). We have
   * to be careful here though, since the mesh may not be in a valid state at this point. */
  if (mesh->poly_offset_indices) {
    mesh->poly_offsets_for_write().last() = mesh->totloop;
  }
}

static void mesh_add_polys(Mesh *mesh, int len)
{
  using namespace blender;
  CustomData pdata;
  int totpoly;

  if (len == 0) {
    return;
  }

  totpoly = mesh->totpoly + len; /* new face count */

  /* update customdata */
  CustomData_copy_layout(&mesh->pdata, &pdata, CD_MASK_MESH.pmask, CD_SET_DEFAULT, totpoly);
  CustomData_copy_data(&mesh->pdata, &pdata, 0, 0, mesh->totpoly);

  implicit_sharing::resize_trivial_array(&mesh->poly_offset_indices,
                                         &mesh->runtime->poly_offsets_sharing_info,
                                         mesh->totpoly == 0 ? 0 : (mesh->totpoly + 1),
                                         totpoly + 1);
  /* Set common values for convenience. */
  mesh->poly_offset_indices[0] = 0;
  mesh->poly_offset_indices[totpoly] = mesh->totloop;

  CustomData_free(&mesh->pdata, mesh->totpoly);
  mesh->pdata = pdata;

  BKE_mesh_runtime_clear_cache(mesh);

  mesh->totpoly = totpoly;

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".select_poly", ATTR_DOMAIN_FACE);
  select_poly.span.take_back(len).fill(true);
  select_poly.finish();
}

/* -------------------------------------------------------------------- */
/** \name Add Geometry
 * \{ */

void ED_mesh_verts_add(Mesh *mesh, ReportList *reports, int count)
{
  if (mesh->edit_mesh) {
    BKE_report(reports, RPT_ERROR, "Cannot add vertices in edit mode");
    return;
  }
  mesh_add_verts(mesh, count);
}

void ED_mesh_edges_add(Mesh *mesh, ReportList *reports, int count)
{
  if (mesh->edit_mesh) {
    BKE_report(reports, RPT_ERROR, "Cannot add edges in edit mode");
    return;
  }
  mesh_add_edges(mesh, count);
}

void ED_mesh_loops_add(Mesh *mesh, ReportList *reports, int count)
{
  if (mesh->edit_mesh) {
    BKE_report(reports, RPT_ERROR, "Cannot add loops in edit mode");
    return;
  }
  mesh_add_loops(mesh, count);
}

void ED_mesh_polys_add(Mesh *mesh, ReportList *reports, int count)
{
  if (mesh->edit_mesh) {
    BKE_report(reports, RPT_ERROR, "Cannot add polygons in edit mode");
    return;
  }
  mesh_add_polys(mesh, count);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Geometry
 * \{ */

static void mesh_remove_verts(Mesh *mesh, int len)
{
  if (len == 0) {
    return;
  }
  const int totvert = mesh->totvert - len;
  CustomData_free_elem(&mesh->vdata, totvert, len);
  mesh->totvert = totvert;
}

static void mesh_remove_edges(Mesh *mesh, int len)
{
  if (len == 0) {
    return;
  }
  const int totedge = mesh->totedge - len;
  CustomData_free_elem(&mesh->edata, totedge, len);
  mesh->totedge = totedge;
}

static void mesh_remove_loops(Mesh *mesh, int len)
{
  if (len == 0) {
    return;
  }
  const int totloop = mesh->totloop - len;
  CustomData_free_elem(&mesh->ldata, totloop, len);
  mesh->totloop = totloop;
}

static void mesh_remove_polys(Mesh *mesh, int len)
{
  if (len == 0) {
    return;
  }
  const int totpoly = mesh->totpoly - len;
  CustomData_free_elem(&mesh->pdata, totpoly, len);
  mesh->totpoly = totpoly;
}

void ED_mesh_verts_remove(Mesh *mesh, ReportList *reports, int count)
{
  if (mesh->edit_mesh) {
    BKE_report(reports, RPT_ERROR, "Cannot remove vertices in edit mode");
    return;
  }
  if (count > mesh->totvert) {
    BKE_report(reports, RPT_ERROR, "Cannot remove more vertices than the mesh contains");
    return;
  }

  mesh_remove_verts(mesh, count);
}

void ED_mesh_edges_remove(Mesh *mesh, ReportList *reports, int count)
{
  if (mesh->edit_mesh) {
    BKE_report(reports, RPT_ERROR, "Cannot remove edges in edit mode");
    return;
  }
  if (count > mesh->totedge) {
    BKE_report(reports, RPT_ERROR, "Cannot remove more edges than the mesh contains");
    return;
  }

  mesh_remove_edges(mesh, count);
}

void ED_mesh_loops_remove(Mesh *mesh, ReportList *reports, int count)
{
  if (mesh->edit_mesh) {
    BKE_report(reports, RPT_ERROR, "Cannot remove loops in edit mode");
    return;
  }
  if (count > mesh->totloop) {
    BKE_report(reports, RPT_ERROR, "Cannot remove more loops than the mesh contains");
    return;
  }

  mesh_remove_loops(mesh, count);
}

void ED_mesh_polys_remove(Mesh *mesh, ReportList *reports, int count)
{
  if (mesh->edit_mesh) {
    BKE_report(reports, RPT_ERROR, "Cannot remove polys in edit mode");
    return;
  }
  if (count > mesh->totpoly) {
    BKE_report(reports, RPT_ERROR, "Cannot remove more polys than the mesh contains");
    return;
  }

  mesh_remove_polys(mesh, count);
}

void ED_mesh_geometry_clear(Mesh *mesh)
{
  mesh_remove_verts(mesh, mesh->totvert);
  mesh_remove_edges(mesh, mesh->totedge);
  mesh_remove_loops(mesh, mesh->totloop);
  mesh_remove_polys(mesh, mesh->totpoly);
}

/** \} */

void ED_mesh_report_mirror_ex(wmOperator *op, int totmirr, int totfail, char selectmode)
{
  const char *elem_type;

  if (selectmode & SCE_SELECT_VERTEX) {
    elem_type = "vertices";
  }
  else if (selectmode & SCE_SELECT_EDGE) {
    elem_type = "edges";
  }
  else {
    elem_type = "faces";
  }

  if (totfail) {
    BKE_reportf(
        op->reports, RPT_WARNING, "%d %s mirrored, %d failed", totmirr, elem_type, totfail);
  }
  else {
    BKE_reportf(op->reports, RPT_INFO, "%d %s mirrored", totmirr, elem_type);
  }
}

void ED_mesh_report_mirror(wmOperator *op, int totmirr, int totfail)
{
  ED_mesh_report_mirror_ex(op, totmirr, totfail, SCE_SELECT_VERTEX);
}

Mesh *ED_mesh_context(bContext *C)
{
  Mesh *mesh = static_cast<Mesh *>(CTX_data_pointer_get_type(C, "mesh", &RNA_Mesh).data);
  if (mesh != nullptr) {
    return mesh;
  }

  Object *ob = ED_object_active_context(C);
  if (ob == nullptr) {
    return nullptr;
  }

  ID *data = (ID *)ob->data;
  if (data == nullptr || GS(data->name) != ID_ME) {
    return nullptr;
  }

  return (Mesh *)data;
}

void ED_mesh_split_faces(Mesh *mesh)
{
  using namespace blender;
  const OffsetIndices polys = mesh->polys();
  const Span<int> corner_verts = mesh->corner_verts();
  const Span<int> corner_edges = mesh->corner_edges();
  const float split_angle = (mesh->flag & ME_AUTOSMOOTH) != 0 ? mesh->smoothresh : float(M_PI);
  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArray<bool> mesh_sharp_edges = *attributes.lookup_or_default<bool>(
      "sharp_edge", ATTR_DOMAIN_EDGE, false);
  const bool *sharp_faces = static_cast<const bool *>(
      CustomData_get_layer_named(&mesh->pdata, CD_PROP_BOOL, "sharp_face"));

  Array<bool> sharp_edges(mesh->totedge);
  mesh_sharp_edges.materialize(sharp_edges);

  bke::mesh::edges_sharp_from_angle_set(polys,
                                        corner_verts,
                                        corner_edges,
                                        mesh->poly_normals(),
                                        sharp_faces,
                                        split_angle,
                                        sharp_edges);

  threading::parallel_for(polys.index_range(), 1024, [&](const IndexRange range) {
    for (const int poly_i : range) {
      if (sharp_faces && sharp_faces[poly_i]) {
        for (const int edge : corner_edges.slice(polys[poly_i])) {
          sharp_edges[edge] = true;
        }
      }
    }
  });

  IndexMaskMemory memory;
  const IndexMask split_mask = IndexMask::from_bools(sharp_edges, memory);
  if (split_mask.is_empty()) {
    return;
  }

  geometry::split_edges(*mesh, split_mask, {});
}
