/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "BLI_color.hh"
#include "BLI_generic_pointer.hh"
#include "BLI_math_quaternion.hh"

#include "BKE_attribute.h"
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_mesh.hh"
#include "BKE_report.h"
#include "BKE_type_conversions.hh"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "BLT_translation.h"

#include "DNA_object_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "bmesh_tools.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "mesh_intern.h"

/* -------------------------------------------------------------------- */
/** \name Delete Operator
 * \{ */

namespace blender::ed::mesh {

static char domain_to_htype(const eAttrDomain domain)
{
  switch (domain) {
    case ATTR_DOMAIN_POINT:
      return BM_VERT;
    case ATTR_DOMAIN_EDGE:
      return BM_EDGE;
    case ATTR_DOMAIN_FACE:
      return BM_FACE;
    case ATTR_DOMAIN_CORNER:
      return BM_LOOP;
    default:
      BLI_assert_unreachable();
      return BM_VERT;
  }
}

static bool mesh_active_attribute_poll(bContext *C)
{
  if (!ED_operator_editmesh(C)) {
    return false;
  }
  const Mesh *mesh = ED_mesh_context(C);
  const CustomDataLayer *layer = BKE_id_attributes_active_get(&const_cast<ID &>(mesh->id));
  if (!layer) {
    CTX_wm_operator_poll_msg_set(C, "No active attribute");
    return false;
  }
  if (layer->type == CD_PROP_STRING) {
    CTX_wm_operator_poll_msg_set(C, "Active string attribute not supported");
    return false;
  }
  return true;
}

namespace set_attribute {

static StringRefNull rna_property_name_for_type(const eCustomDataType type)
{
  switch (type) {
    case CD_PROP_FLOAT:
      return "value_float";
    case CD_PROP_FLOAT2:
      return "value_float_vector_2d";
    case CD_PROP_FLOAT3:
      return "value_float_vector_3d";
    case CD_PROP_COLOR:
    case CD_PROP_BYTE_COLOR:
      return "value_color";
    case CD_PROP_BOOL:
      return "value_bool";
    case CD_PROP_INT8:
    case CD_PROP_INT32:
      return "value_int";
    case CD_PROP_INT32_2D:
      return "value_int_vector_2d";
    case CD_PROP_QUATERNION:
      return "value_quat";
    default:
      BLI_assert_unreachable();
      return "";
  }
}

static void bmesh_vert_edge_face_layer_selected_values_set(BMesh &bm,
                                                           const BMIterType iter_type,
                                                           const GPointer value,
                                                           const int offset)
{
  const CPPType &type = *value.type();
  BMIter iter;
  BMElem *elem;
  BM_ITER_MESH (elem, &iter, &bm, iter_type) {
    if (BM_elem_flag_test(elem, BM_ELEM_SELECT)) {
      type.copy_assign(value.get(), POINTER_OFFSET(elem->head.data, offset));
    }
  }
}

/**
 * For face select mode, set face corner values of any selected face. For edge and vertex
 * select mode, set face corner values of loops connected to selected vertices.
 */
static void bmesh_loop_layer_selected_values_set(BMEditMesh &em,
                                                 const GPointer value,
                                                 const int offset)
{
  /* In the separate select modes we may set the same loop values more than once.
   * This is okay because we're always setting the same value. */
  BMesh &bm = *em.bm;
  const CPPType &type = *value.type();
  if (em.selectmode & SCE_SELECT_FACE) {
    BMIter face_iter;
    BMFace *face;
    BM_ITER_MESH (face, &face_iter, &bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(face, BM_ELEM_SELECT)) {
        BMIter loop_iter;
        BMLoop *loop;
        BM_ITER_ELEM (loop, &loop_iter, face, BM_LOOPS_OF_FACE) {
          type.copy_assign(value.get(), POINTER_OFFSET(loop->head.data, offset));
        }
      }
    }
  }
  if (em.selectmode & (SCE_SELECT_VERTEX | SCE_SELECT_EDGE)) {
    BMIter vert_iter;
    BMVert *vert;
    BM_ITER_MESH (vert, &vert_iter, &bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(vert, BM_ELEM_SELECT)) {
        BMIter loop_iter;
        BMLoop *loop;
        BM_ITER_ELEM (loop, &loop_iter, vert, BM_LOOPS_OF_VERT) {
          type.copy_assign(value.get(), POINTER_OFFSET(loop->head.data, offset));
        }
      }
    }
  }
}

static int mesh_set_attribute_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C), &objects_len);

  Mesh *mesh = ED_mesh_context(C);
  CustomDataLayer *active_attribute = BKE_id_attributes_active_get(&mesh->id);
  const eCustomDataType active_type = eCustomDataType(active_attribute->type);
  const CPPType &type = *bke::custom_data_type_to_cpp_type(active_type);

  BUFFER_FOR_CPP_TYPE_VALUE(type, buffer);
  BLI_SCOPED_DEFER([&]() { type.destruct(buffer); });

  const StringRefNull prop_name = rna_property_name_for_type(active_type);
  switch (active_type) {
    case CD_PROP_FLOAT:
      *static_cast<float *>(buffer) = RNA_float_get(op->ptr, prop_name.c_str());
      break;
    case CD_PROP_FLOAT2:
      RNA_float_get_array(op->ptr, prop_name.c_str(), static_cast<float *>(buffer));
      break;
    case CD_PROP_FLOAT3:
      RNA_float_get_array(op->ptr, prop_name.c_str(), static_cast<float *>(buffer));
      break;
    case CD_PROP_COLOR:
      RNA_float_get_array(op->ptr, prop_name.c_str(), static_cast<float *>(buffer));
      break;
    case CD_PROP_QUATERNION: {
      float4 value;
      RNA_float_get_array(op->ptr, prop_name.c_str(), value);
      *static_cast<math::Quaternion *>(buffer) = math::normalize(math::Quaternion(value));
      break;
    }
    case CD_PROP_BYTE_COLOR:
      ColorGeometry4f value;
      RNA_float_get_array(op->ptr, prop_name.c_str(), value);
      *static_cast<ColorGeometry4b *>(buffer) = value.encode();
      break;
    case CD_PROP_BOOL:
      *static_cast<bool *>(buffer) = RNA_boolean_get(op->ptr, prop_name.c_str());
      break;
    case CD_PROP_INT8:
      *static_cast<int8_t *>(buffer) = RNA_int_get(op->ptr, prop_name.c_str());
      break;
    case CD_PROP_INT32:
      *static_cast<int32_t *>(buffer) = RNA_int_get(op->ptr, prop_name.c_str());
      break;
    case CD_PROP_INT32_2D:
      RNA_int_get_array(op->ptr, prop_name.c_str(), static_cast<int *>(buffer));
      break;
    default:
      BLI_assert_unreachable();
  }
  const GPointer value(type, buffer);
  const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();

  bool changed = false;
  for (const int i : IndexRange(objects_len)) {
    Object *object = objects[i];
    Mesh *mesh = static_cast<Mesh *>(object->data);
    BMEditMesh *em = BKE_editmesh_from_object(object);
    BMesh *bm = em->bm;

    CustomDataLayer *layer = BKE_id_attributes_active_get(&mesh->id);
    if (!layer) {
      continue;
    }
    /* Use implicit conversions to try to handle the case where the active attribute has a
     * different type on multiple objects. */
    const eCustomDataType dst_data_type = eCustomDataType(active_attribute->type);
    const CPPType &dst_type = *bke::custom_data_type_to_cpp_type(dst_data_type);
    if (&type != &dst_type && !conversions.is_convertible(type, dst_type)) {
      continue;
    }
    BUFFER_FOR_CPP_TYPE_VALUE(dst_type, dst_buffer);
    BLI_SCOPED_DEFER([&]() { dst_type.destruct(dst_buffer); });
    conversions.convert_to_uninitialized(type, dst_type, value.get(), dst_buffer);
    const GPointer dst_value(dst_type, dst_buffer);
    switch (BKE_id_attribute_domain(&mesh->id, layer)) {
      case ATTR_DOMAIN_POINT:
        bmesh_vert_edge_face_layer_selected_values_set(
            *bm, BM_VERTS_OF_MESH, dst_value, layer->offset);
        break;
      case ATTR_DOMAIN_EDGE:
        bmesh_vert_edge_face_layer_selected_values_set(
            *bm, BM_EDGES_OF_MESH, dst_value, layer->offset);
        break;
      case ATTR_DOMAIN_FACE:
        bmesh_vert_edge_face_layer_selected_values_set(
            *bm, BM_FACES_OF_MESH, dst_value, layer->offset);
        break;
      case ATTR_DOMAIN_CORNER:
        bmesh_loop_layer_selected_values_set(*em, dst_value, layer->offset);
        break;
      default:
        BLI_assert_unreachable();
        break;
    }

    changed = true;
    EDBMUpdate_Params update{};
    update.calc_looptri = false;
    update.calc_normals = false;
    update.is_destructive = false;
    EDBM_update(mesh, &update);
  }

  MEM_freeN(objects);

  return changed ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static int mesh_set_attribute_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Mesh *mesh = ED_mesh_context(C);
  BMesh *bm = mesh->edit_mesh->bm;

  const CustomDataLayer *layer = BKE_id_attributes_active_get(&mesh->id);
  const eCustomDataType data_type = eCustomDataType(layer->type);
  const eAttrDomain domain = BKE_id_attribute_domain(&mesh->id, layer);
  const BMElem *active_elem = BM_mesh_active_elem_get(bm);
  if (!active_elem) {
    return WM_operator_props_popup(C, op, event);
  }

  /* Only support filling the active data when the active selection mode matches the active
   * attribute domain. NOTE: This doesn't work well for corner domain attributes. */
  if (active_elem->head.htype != domain_to_htype(domain)) {
    return WM_operator_props_popup(C, op, event);
  }

  const StringRefNull prop_name = rna_property_name_for_type(data_type);
  const CPPType &type = *bke::custom_data_type_to_cpp_type(data_type);
  const GPointer active_value(type, POINTER_OFFSET(active_elem->head.data, layer->offset));

  PropertyRNA *prop = RNA_struct_find_property(op->ptr, prop_name.c_str());
  if (!RNA_property_is_set(op->ptr, prop)) {
    switch (data_type) {
      case CD_PROP_FLOAT:
        RNA_property_float_set(op->ptr, prop, *active_value.get<float>());
        break;
      case CD_PROP_FLOAT2:
        RNA_property_float_set_array(op->ptr, prop, *active_value.get<float2>());
        break;
      case CD_PROP_FLOAT3:
        RNA_property_float_set_array(op->ptr, prop, *active_value.get<float3>());
        break;
      case CD_PROP_BYTE_COLOR:
        RNA_property_float_set_array(op->ptr, prop, active_value.get<ColorGeometry4b>()->decode());
        break;
      case CD_PROP_COLOR:
        RNA_property_float_set_array(op->ptr, prop, *active_value.get<ColorGeometry4f>());
        break;
      case CD_PROP_BOOL:
        RNA_property_boolean_set(op->ptr, prop, *active_value.get<bool>());
        break;
      case CD_PROP_INT8:
        RNA_property_int_set(op->ptr, prop, *active_value.get<int8_t>());
        break;
      case CD_PROP_INT32:
        RNA_property_int_set(op->ptr, prop, *active_value.get<int32_t>());
        break;
      case CD_PROP_INT32_2D:
        RNA_property_int_set_array(op->ptr, prop, *active_value.get<int2>());
        break;
      case CD_PROP_QUATERNION: {
        const math::Quaternion value = math::normalize(*active_value.get<math::Quaternion>());
        RNA_property_float_set_array(op->ptr, prop, float4(value));
        break;
      }
      default:
        BLI_assert_unreachable();
    }
  }

  return WM_operator_props_popup(C, op, event);
}

static void mesh_set_attribute_ui(bContext *C, wmOperator *op)
{
  uiLayout *layout = uiLayoutColumn(op->layout, true);
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  Mesh *mesh = ED_mesh_context(C);
  CustomDataLayer *active_attribute = BKE_id_attributes_active_get(&mesh->id);
  const eCustomDataType active_type = eCustomDataType(active_attribute->type);
  const StringRefNull prop_name = rna_property_name_for_type(active_type);
  const char *name = active_attribute->name;
  uiItemR(layout, op->ptr, prop_name.c_str(), UI_ITEM_NONE, name, ICON_NONE);
}

}  // namespace set_attribute

}  // namespace blender::ed::mesh

void MESH_OT_attribute_set(wmOperatorType *ot)
{
  using namespace blender::ed::mesh;
  using namespace blender::ed::mesh::set_attribute;
  ot->name = "Set Attribute";
  ot->description = "Set values of the active attribute for selected elements";
  ot->idname = "MESH_OT_attribute_set";

  ot->exec = mesh_set_attribute_exec;
  ot->invoke = mesh_set_attribute_invoke;
  ot->poll = mesh_active_attribute_poll;
  ot->ui = mesh_set_attribute_ui;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  static blender::float4 color_default(1);

  RNA_def_float(ot->srna, "value_float", 0.0f, -FLT_MAX, FLT_MAX, "Value", "", -FLT_MAX, FLT_MAX);
  RNA_def_float_array(ot->srna,
                      "value_float_vector_2d",
                      2,
                      nullptr,
                      -FLT_MAX,
                      FLT_MAX,
                      "Value",
                      "",
                      -FLT_MAX,
                      FLT_MAX);
  RNA_def_float_array(ot->srna,
                      "value_float_vector_3d",
                      3,
                      nullptr,
                      -FLT_MAX,
                      FLT_MAX,
                      "Value",
                      "",
                      -FLT_MAX,
                      FLT_MAX);
  RNA_def_int(ot->srna, "value_int", 0, INT_MIN, INT_MAX, "Value", "", INT_MIN, INT_MAX);
  RNA_def_int_array(ot->srna,
                    "value_int_vector_2d",
                    2,
                    nullptr,
                    INT_MIN,
                    INT_MAX,
                    "Value",
                    "",
                    INT_MIN,
                    INT_MAX);
  RNA_def_float_color(
      ot->srna, "value_color", 4, color_default, -FLT_MAX, FLT_MAX, "Value", "", 0.0f, 1.0f);
  RNA_def_boolean(ot->srna, "value_bool", false, "Value", "");
  RNA_def_float_array(ot->srna,
                      "value_quat",
                      4,
                      rna_default_quaternion,
                      -FLT_MAX,
                      FLT_MAX,
                      "Value",
                      "",
                      FLT_MAX,
                      FLT_MAX);
}

/** \} */
