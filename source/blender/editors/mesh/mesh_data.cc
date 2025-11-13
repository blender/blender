/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_array.hh"
#include "BLI_math_constants.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_key.hh"
#include "BKE_library.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"

#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "BLT_translation.hh"

#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_paint.hh"
#include "ED_screen.hh"

#include "GEO_mesh_split_edges.hh"

#include "mesh_intern.hh" /* own include */

using blender::Array;
using blender::float2;
using blender::float3;
using blender::MutableSpan;
using blender::Span;
using blender::StringRef;

static CustomData *mesh_customdata_get_type(Mesh *mesh, const char htype, int *r_tot)
{
  CustomData *data;
  BMesh *bm = (mesh->runtime->edit_mesh) ? mesh->runtime->edit_mesh->bm : nullptr;
  int tot;

  switch (htype) {
    case BM_VERT:
      if (bm) {
        data = &bm->vdata;
        tot = bm->totvert;
      }
      else {
        data = &mesh->vert_data;
        tot = mesh->verts_num;
      }
      break;
    case BM_EDGE:
      if (bm) {
        data = &bm->edata;
        tot = bm->totedge;
      }
      else {
        data = &mesh->edge_data;
        tot = mesh->edges_num;
      }
      break;
    case BM_LOOP:
      if (bm) {
        data = &bm->ldata;
        tot = bm->totloop;
      }
      else {
        data = &mesh->corner_data;
        tot = mesh->corners_num;
      }
      break;
    case BM_FACE:
      if (bm) {
        data = &bm->pdata;
        tot = bm->totface;
      }
      else {
        data = &mesh->face_data;
        tot = mesh->faces_num;
      }
      break;
    default:
      BLI_assert(0);
      tot = 0;
      data = nullptr;
      break;
  }

  if (r_tot) {
    *r_tot = tot;
  }
  return data;
}

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

static void reset_uvs_bmesh(BMFace *f, const int cd_loop_uv_offset)
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

static void reset_uvs_mesh(const blender::IndexRange face, MutableSpan<float2> uv_map)
{
  Array<float *, BM_DEFAULT_NGON_STACK_SIZE> fuv(face.size());

  for (int i = 0; i < face.size(); i++) {
    fuv[i] = &uv_map[face[i]].x;
  }

  mesh_uv_reset_array(fuv.data(), face.size());
}

static void reset_uv_map(Mesh *mesh, const StringRef name)
{
  using namespace blender;
  if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
    const int cd_loop_uv_offset = CustomData_get_offset_named(
        &em->bm->ldata, CD_PROP_FLOAT2, name);
    BLI_assert(cd_loop_uv_offset >= 0);

    BMFace *efa;
    BMIter iter;
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
        continue;
      }
      reset_uvs_bmesh(efa, cd_loop_uv_offset);
    }
  }
  else {
    const OffsetIndices faces = mesh->faces();
    bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
    bke::SpanAttributeWriter uv_map = attributes.lookup_for_write_span<float2>(name);
    BLI_assert(uv_map.domain == bke::AttrDomain::Corner);
    for (const int i : faces.index_range()) {
      reset_uvs_mesh(faces[i], uv_map.span);
    }
    uv_map.finish();
  }

  DEG_id_tag_update(&mesh->id, 0);
}

void ED_mesh_uv_loop_reset(bContext *C, Mesh *mesh)
{
  reset_uv_map(mesh, mesh->active_uv_map_name());

  WM_event_add_notifier(C, NC_GEOM | ND_DATA, mesh);
}

int ED_mesh_uv_add(
    Mesh *mesh, const char *name, const bool active_set, const bool do_init, ReportList *reports)
{
  using namespace blender;
  /* NOTE: keep in sync with #ED_mesh_color_add. */

  int layernum_dst;

  if (!name) {
    name = DATA_("UVMap");
  }

  AttributeOwner owner = AttributeOwner::from_id(&mesh->id);
  const std::string unique_name = BKE_attribute_calc_unique_name(owner, name);
  bool is_init = false;

  if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
    layernum_dst = CustomData_number_of_layers(&em->bm->ldata, CD_PROP_FLOAT2);
    if (layernum_dst >= MAX_MTFACE) {
      BKE_reportf(reports, RPT_WARNING, "Cannot add more than %i UV maps", MAX_MTFACE);
      return -1;
    }

    BM_data_layer_add_named(em->bm, &em->bm->ldata, CD_PROP_FLOAT2, unique_name.c_str());
    BM_uv_map_attr_pin_ensure_for_all_layers(em->bm);
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
    bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
    layernum_dst = mesh->uv_map_names().size();
    if (layernum_dst >= MAX_MTFACE) {
      BKE_reportf(reports, RPT_WARNING, "Cannot add more than %i UV maps", MAX_MTFACE);
      return -1;
    }

    const StringRef active_name = mesh->active_uv_map_name();
    if (!active_name.is_empty() && do_init) {
      const VArray<float2> active_uv_map = *attributes.lookup_or_default<float2>(
          active_name, bke::AttrDomain::Corner, float2(0));
      attributes.add<float2>(
          unique_name, bke::AttrDomain::Corner, bke::AttributeInitVArray(active_uv_map));

      is_init = true;
    }
    else {
      attributes.add<float2>(
          unique_name, bke::AttrDomain::Corner, bke::AttributeInitDefaultValue());
    }

    if (active_set || layernum_dst == 0) {
      CustomData_set_layer_active(&mesh->corner_data, CD_PROP_FLOAT2, layernum_dst);
    }
  }

  /* don't overwrite our copied coords */
  if (!is_init && do_init) {
    reset_uv_map(mesh, unique_name);
  }

  DEG_id_tag_update(&mesh->id, 0);
  WM_main_add_notifier(NC_GEOM | ND_DATA, mesh);

  return layernum_dst;
}

static blender::VArray<bool> get_corner_boolean_attribute(const Mesh &mesh, const StringRef name)
{
  const blender::bke::AttributeAccessor attributes = mesh.attributes();
  return *attributes.lookup_or_default<bool>(name, blender::bke::AttrDomain::Corner, false);
}

blender::VArray<bool> ED_mesh_uv_map_pin_layer_get(const Mesh *mesh, const int uv_index)
{
  using namespace blender::bke;
  char buffer[MAX_CUSTOMDATA_LAYER_NAME];
  const char *uv_name = mesh->uv_map_names()[uv_index].c_str();
  return get_corner_boolean_attribute(*mesh, BKE_uv_map_pin_name_get(uv_name, buffer));
}

static blender::bke::AttributeWriter<bool> ensure_corner_boolean_attribute(Mesh &mesh,
                                                                           const StringRef name)
{
  blender::bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  return attributes.lookup_or_add_for_write<bool>(
      name, blender::bke::AttrDomain::Corner, blender::bke::AttributeInitDefaultValue());
}

blender::bke::AttributeWriter<bool> ED_mesh_uv_map_pin_layer_ensure(Mesh *mesh, const int uv_index)
{
  using namespace blender::bke;
  char buffer[MAX_CUSTOMDATA_LAYER_NAME];
  const char *uv_name = mesh->uv_map_names()[uv_index].c_str();
  return ensure_corner_boolean_attribute(*mesh, BKE_uv_map_pin_name_get(uv_name, buffer));
}

void ED_mesh_uv_ensure(Mesh *mesh, const char *name)
{
  if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
    int layernum_dst = CustomData_number_of_layers(&em->bm->ldata, CD_PROP_FLOAT2);
    if (layernum_dst == 0) {
      ED_mesh_uv_add(mesh, name, true, true, nullptr);
    }
  }
  else {
    if (mesh->uv_map_names().is_empty()) {
      ED_mesh_uv_add(mesh, name, true, true, nullptr);
    }
  }
}

std::string ED_mesh_color_add(Mesh *mesh,
                              const char *name,
                              const bool active_set,
                              const bool do_init,
                              ReportList * /*reports*/)
{
  using namespace blender;
  /* If no name is supplied, provide a backwards compatible default. */
  if (!name) {
    name = "Col";
  }

  AttributeOwner owner = AttributeOwner::from_id(&mesh->id);
  std::string new_name = BKE_attribute_calc_unique_name(owner, name);

  const StringRef active_name = mesh->active_color_attribute;
  if (const BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
    BM_data_layer_add_named(em->bm, &em->bm->ldata, CD_PROP_BYTE_COLOR, new_name);
    if (do_init) {
      const BMDataLayerLookup active_attr = BM_data_layer_lookup(*em->bm, name);
      if (active_attr.type == bke::AttrType::ColorByte &&
          active_attr.domain == bke::AttrDomain::Corner)
      {
        BMesh &bm = *em->bm;
        const int src_i = CustomData_get_named_layer(&bm.ldata, CD_PROP_BYTE_COLOR, active_name);
        const int dst_i = CustomData_get_named_layer(&bm.ldata, CD_PROP_BYTE_COLOR, new_name);
        BM_data_layer_copy(&bm, &bm.ldata, CD_PROP_BYTE_COLOR, src_i, dst_i);
      }
    }
  }
  else {
    bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
    attributes.add<ColorGeometry4b>(
        new_name, bke::AttrDomain::Corner, bke::AttributeInitDefaultValue());
    if (do_init) {
      if (const VArray active_attr = *attributes.lookup<ColorGeometry4b>(active_name,
                                                                         bke::AttrDomain::Corner))
      {
        bke::SpanAttributeWriter new_attr = attributes.lookup_for_write_span<ColorGeometry4b>(
            new_name);
        active_attr.materialize(new_attr.span);
        new_attr.finish();
      }
    }
  }

  if (active_set) {
    BKE_id_attributes_active_color_set(&mesh->id, new_name);
  }

  DEG_id_tag_update(&mesh->id, 0);
  WM_main_add_notifier(NC_GEOM | ND_DATA, mesh);

  return new_name;
}

bool ED_mesh_color_ensure(Mesh *mesh, const char *name)
{
  using namespace blender;
  BLI_assert(mesh->runtime->edit_mesh == nullptr);
  if (BKE_color_attribute_supported(*mesh, mesh->active_color_attribute)) {
    return true;
  }

  AttributeOwner owner = AttributeOwner::from_id(&mesh->id);
  const std::string unique_name = BKE_attribute_calc_unique_name(owner, name);
  if (!mesh->attributes_for_write().add(unique_name,
                                        bke::AttrDomain::Corner,
                                        bke::AttrType::ColorByte,
                                        bke::AttributeInitDefaultValue()))
  {
    return false;
  }

  BKE_id_attributes_active_color_set(&mesh->id, unique_name);
  BKE_id_attributes_default_color_set(&mesh->id, unique_name);
  BKE_mesh_tessface_clear(mesh);
  DEG_id_tag_update(&mesh->id, 0);

  return true;
}

/*********************** General poll ************************/

static bool layers_poll(bContext *C)
{
  Object *ob = blender::ed::object::context_object(C);
  ID *data = (ob) ? static_cast<ID *>(ob->data) : nullptr;
  return (ob && ID_IS_EDITABLE(ob) && !ID_IS_OVERRIDE_LIBRARY(ob) && ob->type == OB_MESH && data &&
          ID_IS_EDITABLE(data) && !ID_IS_OVERRIDE_LIBRARY(data));
}

/*********************** UV texture operators ************************/

static bool uv_texture_remove_poll(bContext *C)
{
  if (!layers_poll(C)) {
    return false;
  }

  Object *ob = blender::ed::object::context_object(C);
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  const StringRef active_name = mesh->active_uv_map_name();
  if (mesh->runtime->edit_mesh) {
    const BMesh &bm = *mesh->runtime->edit_mesh->bm;
    if (!CustomData_has_layer_named(&bm.ldata, CD_PROP_FLOAT2, active_name)) {
      return false;
    }
  }
  else {
    if (!mesh->uv_map_names().contains_as(active_name)) {
      return false;
    }
  }

  return true;
}

static wmOperatorStatus mesh_uv_texture_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = blender::ed::object::context_object(C);
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  if (ED_mesh_uv_add(mesh, nullptr, true, true, op->reports) == -1) {
    return OPERATOR_CANCELLED;
  }

  if (ob->mode & OB_MODE_TEXTURE_PAINT) {
    Scene *scene = CTX_data_scene(C);
    ED_paint_proj_mesh_data_check(*scene, *ob, nullptr, nullptr, nullptr, nullptr);
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

  /* API callbacks. */
  ot->poll = layers_poll;
  ot->exec = mesh_uv_texture_add_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus mesh_uv_texture_remove_exec(bContext *C, wmOperator *op)
{
  Object *ob = blender::ed::object::context_object(C);
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  AttributeOwner owner = AttributeOwner::from_id(&mesh->id);
  const StringRef name = mesh->active_uv_map_name();
  if (!BKE_attribute_remove(owner, name, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  if (ob->mode & OB_MODE_TEXTURE_PAINT) {
    Scene *scene = CTX_data_scene(C);
    ED_paint_proj_mesh_data_check(*scene, *ob, nullptr, nullptr, nullptr, nullptr);
    WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, nullptr);
  }

  DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, mesh);

  return OPERATOR_FINISHED;
}

void MESH_OT_uv_texture_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove UV Map";
  ot->description = "Remove UV map";
  ot->idname = "MESH_OT_uv_texture_remove";

  /* API callbacks. */
  ot->poll = uv_texture_remove_poll;
  ot->exec = mesh_uv_texture_remove_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* *** CustomData clear functions, we need an operator for each *** */

static wmOperatorStatus mesh_customdata_clear_exec__internal(bContext *C,
                                                             char htype,
                                                             const eCustomDataType type)
{
  Mesh *mesh = ED_mesh_context(C);

  CustomData *data = mesh_customdata_get_type(mesh, htype, nullptr);

  BLI_assert(CustomData_layertype_is_singleton(type) == true);

  if (CustomData_has_layer(data, type)) {
    if (mesh->runtime->edit_mesh) {
      BM_data_layer_free(mesh->runtime->edit_mesh->bm, data, type);
    }
    else {
      CustomData_free_layers(data, type);
    }

    DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, mesh);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

/* Clear Mask */
static bool mesh_customdata_mask_clear_poll(bContext *C)
{
  Object *ob = blender::ed::object::context_object(C);
  if (ob && ob->type == OB_MESH) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);

    /* special case - can't run this if we're in sculpt mode */
    if (ob->mode & OB_MODE_SCULPT) {
      return false;
    }

    if (ID_IS_EDITABLE(mesh) && !ID_IS_OVERRIDE_LIBRARY(mesh)) {
      CustomData *data = mesh_customdata_get_type(mesh, BM_VERT, nullptr);
      if (CustomData_has_layer_named(data, CD_PROP_FLOAT, ".sculpt_mask")) {
        return true;
      }
      data = mesh_customdata_get_type(mesh, BM_LOOP, nullptr);
      if (CustomData_has_layer(data, CD_GRID_PAINT_MASK)) {
        return true;
      }
    }
  }
  return false;
}
static wmOperatorStatus mesh_customdata_mask_clear_exec(bContext *C, wmOperator *op)
{
  Object *object = blender::ed::object::context_object(C);
  Mesh *mesh = static_cast<Mesh *>(object->data);
  AttributeOwner owner = AttributeOwner::from_id(&mesh->id);
  const bool ret_a = BKE_attribute_remove(owner, ".sculpt_mask", op->reports);
  int ret_b = mesh_customdata_clear_exec__internal(C, BM_LOOP, CD_GRID_PAINT_MASK);

  if (ret_a || ret_b == OPERATOR_FINISHED) {
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

  /* API callbacks. */
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
  Object *ob = blender::ed::object::context_object(C);

  if (ob && ob->type == OB_MESH) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);
    if (ID_IS_EDITABLE(mesh) && !ID_IS_OVERRIDE_LIBRARY(mesh)) {
      CustomData *data = mesh_customdata_get_type(mesh, BM_VERT, nullptr);
      return CustomData_has_layer(data, CD_MVERT_SKIN);
    }
  }
  return -1;
}

static bool mesh_customdata_skin_add_poll(bContext *C)
{
  return (mesh_customdata_skin_state(C) == 0);
}

static wmOperatorStatus mesh_customdata_skin_add_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = blender::ed::object::context_object(C);
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  BKE_mesh_ensure_skin_customdata(mesh);

  DEG_id_tag_update(&mesh->id, 0);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, mesh);

  return OPERATOR_FINISHED;
}

void MESH_OT_customdata_skin_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Skin Data";
  ot->idname = "MESH_OT_customdata_skin_add";
  ot->description = "Add a vertex skin layer";

  /* API callbacks. */
  ot->exec = mesh_customdata_skin_add_exec;
  ot->poll = mesh_customdata_skin_add_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static bool mesh_customdata_skin_clear_poll(bContext *C)
{
  return (mesh_customdata_skin_state(C) == 1);
}

static wmOperatorStatus mesh_customdata_skin_clear_exec(bContext *C, wmOperator * /*op*/)
{
  return mesh_customdata_clear_exec__internal(C, BM_VERT, CD_MVERT_SKIN);
}

void MESH_OT_customdata_skin_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Skin Data";
  ot->idname = "MESH_OT_customdata_skin_clear";
  ot->description = "Clear vertex skin layer";

  /* API callbacks. */
  ot->exec = mesh_customdata_skin_clear_exec;
  ot->poll = mesh_customdata_skin_clear_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* Clear custom loop normals */
static wmOperatorStatus mesh_customdata_custom_splitnormals_add_exec(bContext *C,
                                                                     wmOperator * /*op*/)
{
  using namespace blender;
  Mesh *mesh = ED_mesh_context(C);
  if (BKE_mesh_has_custom_loop_normals(mesh)) {
    return OPERATOR_CANCELLED;
  }

  if (mesh->runtime->edit_mesh) {
    BMesh &bm = *mesh->runtime->edit_mesh->bm;
    BM_data_layer_ensure_named(&bm, &bm.ldata, CD_PROP_INT16_2D, "custom_normal");
  }
  else {
    if (!mesh->attributes_for_write().add<short2>(
            "custom_normal", bke::AttrDomain::Corner, bke::AttributeInitDefaultValue()))
    {
      return OPERATOR_CANCELLED;
    }
  }

  DEG_id_tag_update(&mesh->id, 0);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, mesh);

  return OPERATOR_FINISHED;
}

void MESH_OT_customdata_custom_splitnormals_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Custom Normals Data";
  ot->idname = "MESH_OT_customdata_custom_splitnormals_add";
  ot->description = "Add a custom normals layer, if none exists yet";

  /* API callbacks. */
  ot->exec = mesh_customdata_custom_splitnormals_add_exec;
  ot->poll = ED_operator_editable_mesh;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus mesh_customdata_custom_splitnormals_clear_exec(bContext *C,
                                                                       wmOperator * /*op*/)
{
  Mesh *mesh = ED_mesh_context(C);

  if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
    BMesh &bm = *em->bm;
    if (!CustomData_has_layer_named(&bm.ldata, CD_PROP_INT16_2D, "custom_normal")) {
      return OPERATOR_CANCELLED;
    }
    BM_data_layer_free_named(&bm, &bm.ldata, "custom_normal");
    if (bm.lnor_spacearr) {
      BKE_lnor_spacearr_clear(bm.lnor_spacearr);
    }
  }
  else {
    if (!mesh->attributes_for_write().remove("custom_normal")) {
      return OPERATOR_CANCELLED;
    }
  }

  mesh->tag_custom_normals_changed();
  DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, mesh);

  return OPERATOR_FINISHED;
}

void MESH_OT_customdata_custom_splitnormals_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Custom Normals Data";
  ot->idname = "MESH_OT_customdata_custom_splitnormals_clear";
  ot->description = "Remove the custom normals layer, if it exists";

  /* API callbacks. */
  ot->exec = mesh_customdata_custom_splitnormals_clear_exec;
  ot->poll = ED_operator_editable_mesh;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static void mesh_add_verts(Mesh *mesh, int len)
{
  using namespace blender;
  if (len == 0) {
    return;
  }

  int totvert = mesh->verts_num + len;
  CustomData vert_data;
  CustomData_init_layout_from(
      &mesh->vert_data, &vert_data, CD_MASK_MESH.vmask, CD_SET_DEFAULT, totvert);
  CustomData_copy_data(&mesh->vert_data, &vert_data, 0, 0, mesh->verts_num);

  if (!CustomData_has_layer_named(&vert_data, CD_PROP_FLOAT3, "position")) {
    CustomData_add_layer_named(&vert_data, CD_PROP_FLOAT3, CD_SET_DEFAULT, totvert, "position");
  }

  CustomData_free(&mesh->vert_data);
  mesh->vert_data = vert_data;

  BKE_mesh_runtime_clear_cache(mesh);

  mesh->verts_num = totvert;

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".select_vert", bke::AttrDomain::Point);
  select_vert.span.take_back(len).fill(true);
  select_vert.finish();
}

static void mesh_add_edges(Mesh *mesh, int len)
{
  using namespace blender;
  CustomData edge_data;
  int totedge;

  if (len == 0) {
    return;
  }

  totedge = mesh->edges_num + len;

  /* Update custom-data. */
  CustomData_init_layout_from(
      &mesh->edge_data, &edge_data, CD_MASK_MESH.emask, CD_SET_DEFAULT, totedge);
  CustomData_copy_data(&mesh->edge_data, &edge_data, 0, 0, mesh->edges_num);

  if (!CustomData_has_layer_named(&edge_data, CD_PROP_INT32_2D, ".edge_verts")) {
    CustomData_add_layer_named(
        &edge_data, CD_PROP_INT32_2D, CD_SET_DEFAULT, totedge, ".edge_verts");
  }

  CustomData_free(&mesh->edge_data);
  mesh->edge_data = edge_data;

  BKE_mesh_runtime_clear_cache(mesh);

  mesh->edges_num = totedge;

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> select_edge = attributes.lookup_or_add_for_write_span<bool>(
      ".select_edge", bke::AttrDomain::Edge);
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

  totloop = mesh->corners_num + len; /* new face count */

  /* update customdata */
  CustomData_init_layout_from(
      &mesh->corner_data, &ldata, CD_MASK_MESH.lmask, CD_SET_DEFAULT, totloop);
  CustomData_copy_data(&mesh->corner_data, &ldata, 0, 0, mesh->corners_num);

  if (!CustomData_has_layer_named(&ldata, CD_PROP_INT32, ".corner_vert")) {
    CustomData_add_layer_named(&ldata, CD_PROP_INT32, CD_SET_DEFAULT, totloop, ".corner_vert");
  }
  if (!CustomData_has_layer_named(&ldata, CD_PROP_INT32, ".corner_edge")) {
    CustomData_add_layer_named(&ldata, CD_PROP_INT32, CD_SET_DEFAULT, totloop, ".corner_edge");
  }

  BKE_mesh_runtime_clear_cache(mesh);

  CustomData_free(&mesh->corner_data);
  mesh->corner_data = ldata;

  mesh->corners_num = totloop;

  /* Keep the last face offset up to date with the corner total (they must be the same). We have
   * to be careful here though, since the mesh may not be in a valid state at this point. */
  if (mesh->face_offset_indices) {
    mesh->face_offsets_for_write().last() = mesh->corners_num;
  }
}

static void mesh_add_faces(Mesh *mesh, int len)
{
  using namespace blender;
  CustomData face_data;
  int faces_num;

  if (len == 0) {
    return;
  }

  faces_num = mesh->faces_num + len; /* new face count */

  /* update customdata */
  CustomData_init_layout_from(
      &mesh->face_data, &face_data, CD_MASK_MESH.pmask, CD_SET_DEFAULT, faces_num);
  CustomData_copy_data(&mesh->face_data, &face_data, 0, 0, mesh->faces_num);

  implicit_sharing::resize_trivial_array(&mesh->face_offset_indices,
                                         &mesh->runtime->face_offsets_sharing_info,
                                         mesh->faces_num == 0 ? 0 : (mesh->faces_num + 1),
                                         faces_num + 1);
  /* Set common values for convenience. */
  mesh->face_offset_indices[0] = 0;
  mesh->face_offset_indices[faces_num] = mesh->corners_num;

  CustomData_free(&mesh->face_data);
  mesh->face_data = face_data;

  BKE_mesh_runtime_clear_cache(mesh);

  mesh->faces_num = faces_num;

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".select_poly", bke::AttrDomain::Face);
  select_poly.span.take_back(len).fill(true);
  select_poly.finish();
}

/* -------------------------------------------------------------------- */
/** \name Add Geometry
 * \{ */

void ED_mesh_verts_add(Mesh *mesh, ReportList *reports, int count)
{
  if (mesh->runtime->edit_mesh) {
    BKE_report(reports, RPT_ERROR, "Cannot add vertices in edit mode");
    return;
  }
  mesh_add_verts(mesh, count);
}

void ED_mesh_edges_add(Mesh *mesh, ReportList *reports, int count)
{
  if (mesh->runtime->edit_mesh) {
    BKE_report(reports, RPT_ERROR, "Cannot add edges in edit mode");
    return;
  }
  mesh_add_edges(mesh, count);
}

void ED_mesh_loops_add(Mesh *mesh, ReportList *reports, int count)
{
  if (mesh->runtime->edit_mesh) {
    BKE_report(reports, RPT_ERROR, "Cannot add loops in edit mode");
    return;
  }
  mesh_add_loops(mesh, count);
}

void ED_mesh_faces_add(Mesh *mesh, ReportList *reports, int count)
{
  if (mesh->runtime->edit_mesh) {
    BKE_report(reports, RPT_ERROR, "Cannot add faces in edit mode");
    return;
  }
  mesh_add_faces(mesh, count);
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
  CustomData_ensure_layers_are_mutable(&mesh->vert_data, mesh->verts_num);
  const int totvert = mesh->verts_num - len;
  CustomData_free_elem(&mesh->vert_data, totvert, len);
  mesh->verts_num = totvert;
}

static void mesh_remove_edges(Mesh *mesh, int len)
{
  if (len == 0) {
    return;
  }
  CustomData_ensure_layers_are_mutable(&mesh->edge_data, mesh->edges_num);
  const int totedge = mesh->edges_num - len;
  CustomData_free_elem(&mesh->edge_data, totedge, len);
  mesh->edges_num = totedge;
}

static void mesh_remove_loops(Mesh *mesh, int len)
{
  if (len == 0) {
    return;
  }
  CustomData_ensure_layers_are_mutable(&mesh->corner_data, mesh->corners_num);
  const int totloop = mesh->corners_num - len;
  CustomData_free_elem(&mesh->corner_data, totloop, len);
  mesh->corners_num = totloop;
}

static void mesh_remove_faces(Mesh *mesh, int len)
{
  if (len == 0) {
    return;
  }
  CustomData_ensure_layers_are_mutable(&mesh->face_data, mesh->faces_num);
  const int faces_num = mesh->faces_num - len;
  CustomData_free_elem(&mesh->face_data, faces_num, len);
  mesh->faces_num = faces_num;
}

void ED_mesh_verts_remove(Mesh *mesh, ReportList *reports, int count)
{
  if (mesh->runtime->edit_mesh) {
    BKE_report(reports, RPT_ERROR, "Cannot remove vertices in edit mode");
    return;
  }
  if (count > mesh->verts_num) {
    BKE_report(reports, RPT_ERROR, "Cannot remove more vertices than the mesh contains");
    return;
  }

  mesh_remove_verts(mesh, count);
}

void ED_mesh_edges_remove(Mesh *mesh, ReportList *reports, int count)
{
  if (mesh->runtime->edit_mesh) {
    BKE_report(reports, RPT_ERROR, "Cannot remove edges in edit mode");
    return;
  }
  if (count > mesh->edges_num) {
    BKE_report(reports, RPT_ERROR, "Cannot remove more edges than the mesh contains");
    return;
  }

  mesh_remove_edges(mesh, count);
}

void ED_mesh_loops_remove(Mesh *mesh, ReportList *reports, int count)
{
  if (mesh->runtime->edit_mesh) {
    BKE_report(reports, RPT_ERROR, "Cannot remove loops in edit mode");
    return;
  }
  if (count > mesh->corners_num) {
    BKE_report(reports, RPT_ERROR, "Cannot remove more loops than the mesh contains");
    return;
  }

  mesh_remove_loops(mesh, count);
}

void ED_mesh_faces_remove(Mesh *mesh, ReportList *reports, int count)
{
  if (mesh->runtime->edit_mesh) {
    BKE_report(reports, RPT_ERROR, "Cannot remove polys in edit mode");
    return;
  }
  if (count > mesh->faces_num) {
    BKE_report(reports, RPT_ERROR, "Cannot remove more polys than the mesh contains");
    return;
  }

  mesh_remove_faces(mesh, count);
}

void ED_mesh_geometry_clear(Mesh *mesh)
{
  mesh_remove_verts(mesh, mesh->verts_num);
  mesh_remove_edges(mesh, mesh->edges_num);
  mesh_remove_loops(mesh, mesh->corners_num);
  mesh_remove_faces(mesh, mesh->faces_num);
}

/** \} */

void ED_mesh_report_mirror_ex(ReportList &reports, int totmirr, int totfail, char selectmode)
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
    BKE_reportf(&reports, RPT_WARNING, "%d %s mirrored, %d failed", totmirr, elem_type, totfail);
  }
  else {
    BKE_reportf(&reports, RPT_INFO, "%d %s mirrored", totmirr, elem_type);
  }
}

void ED_mesh_report_mirror(ReportList &reports, int totmirr, int totfail)
{
  ED_mesh_report_mirror_ex(reports, totmirr, totfail, SCE_SELECT_VERTEX);
}

KeyBlock *ED_mesh_get_edit_shape_key(const Mesh *me)
{
  BLI_assert(me->runtime->edit_mesh && me->runtime->edit_mesh->bm);

  return BKE_keyblock_find_by_index(me->key, me->runtime->edit_mesh->bm->shapenr - 1);
}

Mesh *ED_mesh_context(bContext *C)
{
  Mesh *mesh = static_cast<Mesh *>(CTX_data_pointer_get_type(C, "mesh", &RNA_Mesh).data);
  if (mesh != nullptr) {
    return mesh;
  }

  Object *ob = blender::ed::object::context_active_object(C);
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
  const OffsetIndices polys = mesh->faces();
  const Span<int> corner_edges = mesh->corner_edges();
  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArray<bool> mesh_sharp_edges = *attributes.lookup_or_default<bool>(
      "sharp_edge", bke::AttrDomain::Edge, false);
  const VArraySpan<bool> sharp_faces = *attributes.lookup<bool>("sharp_face",
                                                                bke::AttrDomain::Face);

  Array<bool> sharp_edges(mesh->edges_num);
  mesh_sharp_edges.materialize(sharp_edges);

  threading::parallel_for(polys.index_range(), 1024, [&](const IndexRange range) {
    for (const int face_i : range) {
      if (!sharp_faces.is_empty() && sharp_faces[face_i]) {
        for (const int edge : corner_edges.slice(polys[face_i])) {
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
