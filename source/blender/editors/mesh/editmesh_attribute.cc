/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "BLI_generic_pointer.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"
#include "BKE_mesh.hh"
#include "BKE_type_conversions.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "ED_geometry.hh"
#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_transform.hh"
#include "ED_view3d.hh"

#include "DNA_object_types.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "mesh_intern.hh"

using blender::Vector;

/* -------------------------------------------------------------------- */
/** \name Delete Operator
 * \{ */

namespace blender::ed::mesh {

static char domain_to_htype(const bke::AttrDomain domain)
{
  switch (domain) {
    case bke::AttrDomain::Point:
      return BM_VERT;
    case bke::AttrDomain::Edge:
      return BM_EDGE;
    case bke::AttrDomain::Face:
      return BM_FACE;
    case bke::AttrDomain::Corner:
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
  if (!geometry::attribute_set_poll(*C, mesh->id)) {
    return false;
  }
  return true;
}

namespace set_attribute {

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

static wmOperatorStatus mesh_set_attribute_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  Mesh *active_mesh = ED_mesh_context(C);
  AttributeOwner active_owner = AttributeOwner::from_id(&active_mesh->id);
  const StringRef name = *BKE_attributes_active_name_get(active_owner);
  const BMDataLayerLookup active_attr = BM_data_layer_lookup(*active_mesh->runtime->edit_mesh->bm,
                                                             name);
  const bke::AttrType active_type = active_attr.type;
  const CPPType &type = bke::attribute_type_to_cpp_type(active_type);

  BUFFER_FOR_CPP_TYPE_VALUE(type, buffer);
  BLI_SCOPED_DEFER([&]() { type.destruct(buffer); });
  const GPointer value = geometry::rna_property_for_attribute_type_retrieve_value(
      *op->ptr, active_type, buffer);

  const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();

  bool changed = false;
  for (Object *object : objects) {
    Mesh *mesh = static_cast<Mesh *>(object->data);
    BMEditMesh *em = BKE_editmesh_from_object(object);
    BMesh *bm = em->bm;
    BMDataLayerLookup attr = BM_data_layer_lookup(*bm, name);
    if (!attr) {
      continue;
    }
    /* Use implicit conversions to try to handle the case where the active attribute has a
     * different type on multiple objects. */
    const CPPType &dst_type = bke::attribute_type_to_cpp_type(attr.type);
    if (&type != &dst_type && !conversions.is_convertible(type, dst_type)) {
      continue;
    }
    BUFFER_FOR_CPP_TYPE_VALUE(dst_type, dst_buffer);
    BLI_SCOPED_DEFER([&]() { dst_type.destruct(dst_buffer); });
    conversions.convert_to_uninitialized(type, dst_type, value.get(), dst_buffer);
    const GPointer dst_value(dst_type, dst_buffer);
    switch (attr.domain) {
      case bke::AttrDomain::Point:
        bmesh_vert_edge_face_layer_selected_values_set(
            *bm, BM_VERTS_OF_MESH, dst_value, attr.offset);
        break;
      case bke::AttrDomain::Edge:
        bmesh_vert_edge_face_layer_selected_values_set(
            *bm, BM_EDGES_OF_MESH, dst_value, attr.offset);
        break;
      case bke::AttrDomain::Face:
        bmesh_vert_edge_face_layer_selected_values_set(
            *bm, BM_FACES_OF_MESH, dst_value, attr.offset);
        break;
      case bke::AttrDomain::Corner:
        bmesh_loop_layer_selected_values_set(*em, dst_value, attr.offset);
        break;
      default:
        BLI_assert_unreachable();
        break;
    }

    changed = true;
    EDBMUpdate_Params update{};
    update.calc_looptris = false;
    update.calc_normals = false;
    update.is_destructive = false;
    EDBM_update(mesh, &update);
  }

  return changed ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static wmOperatorStatus mesh_set_attribute_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event)
{
  Mesh *mesh = ED_mesh_context(C);
  BMesh *bm = mesh->runtime->edit_mesh->bm;
  AttributeOwner owner = AttributeOwner::from_id(&mesh->id);

  const StringRef name = *BKE_attributes_active_name_get(owner);
  const BMDataLayerLookup attr = BM_data_layer_lookup(*mesh->runtime->edit_mesh->bm, name);
  const bke::AttrType data_type = attr.type;
  const bke::AttrDomain domain = attr.domain;
  const BMElem *active_elem = BM_mesh_active_elem_get(bm);
  if (!active_elem) {
    return WM_operator_props_popup(C, op, event);
  }

  /* Only support filling the active data when the active selection mode matches the active
   * attribute domain. NOTE: This doesn't work well for corner domain attributes. */
  if (active_elem->head.htype != domain_to_htype(domain)) {
    return WM_operator_props_popup(C, op, event);
  }

  const CPPType &type = bke::attribute_type_to_cpp_type(data_type);
  const GPointer active_value(type, POINTER_OFFSET(active_elem->head.data, attr.offset));

  PropertyRNA *prop = geometry::rna_property_for_type(*op->ptr, data_type);
  if (!RNA_property_is_set(op->ptr, prop)) {
    geometry::rna_property_for_attribute_type_set_value(*op->ptr, *prop, active_value);
  }

  return WM_operator_props_popup(C, op, event);
}

static void mesh_set_attribute_ui(bContext *C, wmOperator *op)
{
  uiLayout *layout = &op->layout->column(true);
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  Mesh *mesh = ED_mesh_context(C);
  AttributeOwner owner = AttributeOwner::from_id(&mesh->id);
  const StringRef name = *BKE_attributes_active_name_get(owner);
  const BMDataLayerLookup attr = BM_data_layer_lookup(*mesh->runtime->edit_mesh->bm, name);
  const StringRefNull prop_name = geometry::rna_property_name_for_type(attr.type);
  layout->prop(op->ptr, prop_name, UI_ITEM_NONE, name, ICON_NONE);
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

  blender::ed::geometry::register_rna_properties_for_attribute_types(*ot->srna);
}

/** \} */
