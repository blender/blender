/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 *
 * Use to transform object origins only.
 *
 * This is a small API to store & apply transformations to object data,
 * where a transformation matrix can be continually applied on top of the original values
 * so we don't lose precision over time.
 */

#include <cstdlib>
#include <cstring>

#include "DNA_armature_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_task.hh"

#include "BKE_armature.hh"
#include "BKE_curve.hh"
#include "BKE_curves_utils.hh"
#include "BKE_editmesh.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_key.hh"
#include "BKE_lattice.hh"
#include "BKE_mball.hh"
#include "BKE_mesh_types.hh"

#include "bmesh.hh"

#include "DEG_depsgraph.hh"

#include "ED_armature.hh"
#include "ED_curves.hh"
#include "ED_mesh.hh"
#include "ED_object.hh"

namespace blender::ed::object {

/* -------------------------------------------------------------------- */
/** \name Internal Transform Get/Apply
 *
 * Some object data types don't have utility functions to access their transformation data.
 * Define these locally.
 *
 * \{ */

/* Armature */

struct ElemData_Armature {
  float tail[3];
  float head[3];
  float roll;
  float arm_tail[3];
  float arm_head[3];
  float arm_roll;
  float rad_tail;
  float rad_head;
  float dist;
  float xwidth;
  float zwidth;
};

static ElemData_Armature *armature_coords_and_quats_get_recurse(const ListBase *bone_base,
                                                                ElemData_Armature *elem_array)
{
  ElemData_Armature *elem = elem_array;
  LISTBASE_FOREACH (const Bone *, bone, bone_base) {

#define COPY_PTR(member) memcpy(elem->member, bone->member, sizeof(bone->member))
#define COPY_VAL(member) memcpy(&elem->member, &bone->member, sizeof(bone->member))
    COPY_PTR(head);
    COPY_PTR(tail);
    COPY_VAL(roll);
    COPY_PTR(arm_head);
    COPY_PTR(arm_tail);
    COPY_VAL(arm_roll);
    COPY_VAL(rad_tail);
    COPY_VAL(rad_head);
    COPY_VAL(dist);
    COPY_VAL(xwidth);
    COPY_VAL(zwidth);
#undef COPY_PTR
#undef COPY_VAL

    elem = armature_coords_and_quats_get_recurse(&bone->childbase, elem + 1);
  }
  return elem;
}

static void armature_coords_and_quats_get(const bArmature *arm,
                                          MutableSpan<ElemData_Armature> elem_array)
{
  armature_coords_and_quats_get_recurse(&arm->bonebase, elem_array.data());
}

static const ElemData_Armature *armature_coords_and_quats_apply_with_mat4_recurse(
    ListBase *bone_base, const ElemData_Armature *elem_array, const float4x4 &transform)
{
  const ElemData_Armature *elem = elem_array;
  LISTBASE_FOREACH (Bone *, bone, bone_base) {

#define COPY_PTR(member) memcpy(bone->member, elem->member, sizeof(bone->member))
#define COPY_VAL(member) memcpy(&bone->member, &elem->member, sizeof(bone->member))
    COPY_PTR(head);
    COPY_PTR(tail);
    COPY_VAL(roll);
    COPY_PTR(arm_head);
    COPY_PTR(arm_tail);
    COPY_VAL(arm_roll);
    COPY_VAL(rad_tail);
    COPY_VAL(rad_head);
    COPY_VAL(dist);
    COPY_VAL(xwidth);
    COPY_VAL(zwidth);
#undef COPY_PTR
#undef COPY_VAL

    elem = armature_coords_and_quats_apply_with_mat4_recurse(
        &bone->childbase, elem + 1, transform);
  }
  return elem;
}

static void armature_coords_and_quats_apply_with_mat4(bArmature *arm,
                                                      const Span<ElemData_Armature> elem_array,
                                                      const float4x4 &transform)
{
  armature_coords_and_quats_apply_with_mat4_recurse(&arm->bonebase, elem_array.data(), transform);
  BKE_armature_transform(arm, transform.ptr(), true);
}

static void armature_coords_and_quats_apply(bArmature *arm,
                                            const Span<ElemData_Armature> elem_array)
{
  /* Avoid code duplication by using a unit matrix. */
  armature_coords_and_quats_apply_with_mat4(arm, elem_array, float4x4::identity());
}

/* Edit Armature */
static void edit_armature_coords_and_quats_get(const bArmature *arm,
                                               MutableSpan<ElemData_Armature> elem_array)
{
  ElemData_Armature *elem = elem_array.data();
  for (EditBone *ebone = static_cast<EditBone *>(arm->edbo->first); ebone;
       ebone = ebone->next, elem++)
  {

#define COPY_PTR(member) memcpy(elem->member, ebone->member, sizeof(ebone->member))
#define COPY_VAL(member) memcpy(&elem->member, &ebone->member, sizeof(ebone->member))
    /* Unused for edit bones: arm_head, arm_tail, arm_roll */
    COPY_PTR(head);
    COPY_PTR(tail);
    COPY_VAL(roll);
    COPY_VAL(rad_tail);
    COPY_VAL(rad_head);
    COPY_VAL(dist);
    COPY_VAL(xwidth);
    COPY_VAL(zwidth);
#undef COPY_PTR
#undef COPY_VAL
  }
}

static void edit_armature_coords_and_quats_apply_with_mat4(
    bArmature *arm, const Span<ElemData_Armature> elem_array, const float4x4 &transform)
{
  const ElemData_Armature *elem = elem_array.data();
  for (EditBone *ebone = static_cast<EditBone *>(arm->edbo->first); ebone;
       ebone = ebone->next, elem++)
  {

#define COPY_PTR(member) memcpy(ebone->member, elem->member, sizeof(ebone->member))
#define COPY_VAL(member) memcpy(&ebone->member, &elem->member, sizeof(ebone->member))
    /* Unused for edit bones: arm_head, arm_tail, arm_roll */
    COPY_PTR(head);
    COPY_PTR(tail);
    COPY_VAL(roll);
    COPY_VAL(rad_tail);
    COPY_VAL(rad_head);
    COPY_VAL(dist);
    COPY_VAL(xwidth);
    COPY_VAL(zwidth);
#undef COPY_PTR
#undef COPY_VAL
  }
  ED_armature_edit_transform(arm, transform.ptr(), true);
}

static void edit_armature_coords_and_quats_apply(bArmature *arm,
                                                 const Span<ElemData_Armature> elem_array)
{
  /* Avoid code duplication by using a unit matrix. */
  edit_armature_coords_and_quats_apply_with_mat4(arm, elem_array, float4x4::identity());
}

/* MetaBall */

struct ElemData_MetaBall {
  float co[3];
  float quat[4];
  float exp[3];
  float rad;
};

static void metaball_coords_and_quats_get(const MetaBall *mb,
                                          MutableSpan<ElemData_MetaBall> elem_array)
{
  ElemData_MetaBall *elem = elem_array.data();
  for (const MetaElem *ml = static_cast<const MetaElem *>(mb->elems.first); ml;
       ml = ml->next, elem++)
  {
    copy_v3_v3(elem->co, &ml->x);
    copy_qt_qt(elem->quat, ml->quat);
    copy_v3_v3(elem->exp, &ml->expx);
    elem->rad = ml->rad;
  }
}

static void metaball_coords_and_quats_apply_with_mat4(MetaBall *mb,
                                                      const Span<ElemData_MetaBall> elem_array,
                                                      const float4x4 &transform)
{
  const ElemData_MetaBall *elem = elem_array.data();
  for (MetaElem *ml = static_cast<MetaElem *>(mb->elems.first); ml; ml = ml->next, elem++) {
    copy_v3_v3(&ml->x, elem->co);
    copy_qt_qt(ml->quat, elem->quat);
    copy_v3_v3(&ml->expx, elem->exp);
    ml->rad = elem->rad;
  }
  BKE_mball_transform(mb, transform.ptr(), true);
}

static void metaball_coords_and_quats_apply(MetaBall *mb, const Span<ElemData_MetaBall> elem_array)
{
  /* Avoid code duplication by using a unit matrix. */
  metaball_coords_and_quats_apply_with_mat4(mb, elem_array, float4x4::identity());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Object Data Storage API
 *
 * Used for interactively transforming object data.
 *
 * Store object data transformation in an opaque struct.
 * \{ */

struct XFormObjectData_Mesh : public XFormObjectData {
  /* Optional data for shape keys. */
  Array<float3> key_data;
  Array<float3> positions;
  bool is_edit_mode = false;
  virtual ~XFormObjectData_Mesh() = default;
};

struct XFormObjectData_Lattice : public XFormObjectData {
  /* Optional data for shape keys. */
  Array<float3> key_data;
  Array<float3> positions;
  bool is_edit_mode = false;
  virtual ~XFormObjectData_Lattice() = default;
};

struct XFormObjectData_Curve : public XFormObjectData {
  /* Optional data for shape keys. */
  Array<float3> key_data;
  Array<float3> positions;
  bool is_edit_mode = false;
  virtual ~XFormObjectData_Curve() = default;
};

struct XFormObjectData_Armature : public XFormObjectData {
  Array<ElemData_Armature> elems;
  bool is_edit_mode = false;
  virtual ~XFormObjectData_Armature() = default;
};

struct XFormObjectData_MetaBall : public XFormObjectData {
  Array<ElemData_MetaBall> elems;
  bool is_edit_mode = false;
  virtual ~XFormObjectData_MetaBall() = default;
};

struct XFormObjectData_GreasePencil : public XFormObjectData {
  Array<float3> positions;
  Array<float> radii;
  virtual ~XFormObjectData_GreasePencil() = default;
};

struct XFormObjectData_Curves : public XFormObjectData {
  Array<float3> positions;
  Array<float> radii;
  virtual ~XFormObjectData_Curves() = default;
};

struct XFormObjectData_PointCloud : public XFormObjectData {
  Array<float3> positions;
  Array<float> radii;
  virtual ~XFormObjectData_PointCloud() = default;
};

static std::unique_ptr<XFormObjectData> data_xform_create_ex(ID *id, bool is_edit_mode)
{
  if (id == nullptr) {
    return {};
  }

  switch (GS(id->name)) {
    case ID_ME: {
      Mesh *mesh = (Mesh *)id;
      Key *key = mesh->key;
      const int key_index = -1;

      if (is_edit_mode) {
        BMesh *bm = mesh->runtime->edit_mesh->bm;
        /* Always operate on all keys for the moment. */
        // key_index = bm->shapenr - 1;
        auto xod = std::make_unique<XFormObjectData_Mesh>();
        xod->id = id;
        xod->is_edit_mode = is_edit_mode;
        xod->positions.reinitialize(bm->totvert);

        BM_mesh_vert_coords_get(bm, xod->positions.as_mutable_span());

        if (key != nullptr) {
          const size_t key_size = BKE_keyblock_element_calc_size_from_shape(key, key_index);
          if (key_size) {
            xod->key_data.reinitialize(key_size);
            BKE_keyblock_data_get_from_shape(key, xod->key_data, key_index);
          }
        }
        return xod;
      }

      auto xod = std::make_unique<XFormObjectData_Mesh>();
      xod->id = id;
      xod->is_edit_mode = is_edit_mode;
      xod->positions = mesh->vert_positions();

      if (key != nullptr) {
        const size_t key_size = BKE_keyblock_element_calc_size_from_shape(key, key_index);
        if (key_size) {
          xod->key_data.reinitialize(key_size);
          BKE_keyblock_data_get_from_shape(key, xod->key_data, key_index);
        }
      }
      return xod;
    }
    case ID_LT: {
      Lattice *lt_orig = (Lattice *)id;
      Lattice *lt = is_edit_mode ? lt_orig->editlatt->latt : lt_orig;
      Key *key = lt->key;
      const int key_index = -1;

      if (is_edit_mode) {
        /* Always operate on all keys for the moment. */
        // key_index = lt_orig->editlatt->shapenr - 1;
      }

      auto xod = std::make_unique<XFormObjectData_Lattice>();
      xod->id = id;
      xod->is_edit_mode = is_edit_mode;
      xod->positions = BKE_lattice_vert_coords_alloc(lt);

      if (key != nullptr) {
        const size_t key_size = BKE_keyblock_element_calc_size_from_shape(key, key_index);
        if (key_size) {
          xod->key_data.reinitialize(key_size);
          BKE_keyblock_data_get_from_shape(key, xod->key_data, key_index);
        }
      }

      return xod;
    }
    case ID_CU_LEGACY: {
      Curve *cu = (Curve *)id;
      Key *key = cu->key;

      if (cu->ob_type == OB_FONT) {
        /* We could support translation. */
        break;
      }

      const int key_index = -1;
      ListBase *nurbs;
      if (is_edit_mode) {
        EditNurb *editnurb = cu->editnurb;
        nurbs = &editnurb->nurbs;
        /* Always operate on all keys for the moment. */
        // key_index = editnurb->shapenr - 1;
      }
      else {
        nurbs = &cu->nurb;
      }

      auto xod = std::make_unique<XFormObjectData_Curve>();
      xod->id = id;
      xod->is_edit_mode = is_edit_mode;
      xod->positions = BKE_curve_nurbs_vert_coords_alloc(nurbs);

      if (key != nullptr) {
        const size_t key_size = BKE_keyblock_element_calc_size_from_shape(key, key_index);
        if (key_size) {
          xod->key_data.reinitialize(key_size);
          BKE_keyblock_data_get_from_shape(key, xod->key_data, key_index);
        }
      }

      return xod;
    }
    case ID_AR: {
      bArmature *arm = (bArmature *)id;
      if (is_edit_mode) {
        auto xod = std::make_unique<XFormObjectData_Armature>();
        xod->id = id;
        xod->is_edit_mode = is_edit_mode;
        xod->elems.reinitialize(BLI_listbase_count(arm->edbo));
        edit_armature_coords_and_quats_get(arm, xod->elems);
        return xod;
      }
      auto xod = std::make_unique<XFormObjectData_Armature>();
      xod->id = id;
      xod->is_edit_mode = is_edit_mode;
      xod->elems.reinitialize(BKE_armature_bonelist_count(&arm->bonebase));
      armature_coords_and_quats_get(arm, xod->elems);
      return xod;
    }
    case ID_MB: {
      /* Edit mode and object mode are shared. */
      MetaBall *mb = (MetaBall *)id;
      auto xod = std::make_unique<XFormObjectData_MetaBall>();
      xod->id = id;
      xod->is_edit_mode = is_edit_mode;
      xod->elems.reinitialize(BLI_listbase_count(&mb->elems));
      metaball_coords_and_quats_get(mb, xod->elems);
      return xod;
    }
    case ID_GP: {
      GreasePencil *grease_pencil = (GreasePencil *)id;
      const int elem_array_len = BKE_grease_pencil_stroke_point_count(*grease_pencil);
      auto xod = std::make_unique<XFormObjectData_GreasePencil>();
      xod->id = id;
      if (!BKE_grease_pencil_has_curve_with_type(*grease_pencil, CURVE_TYPE_BEZIER)) {
        xod->positions.reinitialize(elem_array_len);
      }
      else {
        xod->positions.reinitialize(elem_array_len * 3);
      }
      xod->radii.reinitialize(elem_array_len);
      BKE_grease_pencil_point_coords_get(*grease_pencil, xod->positions, xod->radii);
      return xod;
    }
    case ID_CV: {
      Curves *curves_id = reinterpret_cast<Curves *>(id);
      const bke::CurvesGeometry &curves = curves_id->geometry.wrap();
      auto xod = std::make_unique<XFormObjectData_Curves>();
      xod->id = id;

      if (!curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
        xod->positions = curves.positions();
      }
      else {
        xod->positions = bke::curves::bezier::retrieve_all_positions(curves,
                                                                     curves.curves_range());
      }

      xod->radii.reinitialize(curves.points_num());
      curves.radius().materialize(xod->radii);
      return xod;
    }
    case ID_PT: {
      PointCloud *pointcloud = reinterpret_cast<PointCloud *>(id);
      auto xod = std::make_unique<XFormObjectData_PointCloud>();
      xod->id = id;
      xod->positions = pointcloud->positions();
      xod->radii.reinitialize(pointcloud->totpoint);
      pointcloud->radius().materialize(xod->radii);
      return xod;
    }
    default: {
      return {};
    }
  }
  return {};
}

std::unique_ptr<XFormObjectData> data_xform_create(ID *id)
{
  return data_xform_create_ex(id, false);
}

std::unique_ptr<XFormObjectData> data_xform_create_from_edit_mode(ID *id)
{
  return data_xform_create_ex(id, true);
}

static void copy_transformed_positions(const Span<float3> src,
                                       const float4x4 &transform,
                                       MutableSpan<float3> dst)
{
  threading::parallel_for(src.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      dst[i] = math::transform_point(transform, src[i]);
    }
  });
}

static void copy_transformed_radii(const Span<float> src,
                                   const float4x4 &transform,
                                   MutableSpan<float> dst)
{
  const float scale = mat4_to_scale(transform.ptr());
  threading::parallel_for(src.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      dst[i] = src[i] * scale;
    }
  });
}

void data_xform_by_mat4(XFormObjectData &xod_base, const float4x4 &transform)
{
  switch (GS(xod_base.id->name)) {
    case ID_ME: {
      Mesh *mesh = (Mesh *)xod_base.id;

      Key *key = mesh->key;
      const int key_index = -1;

      const auto &xod = reinterpret_cast<XFormObjectData_Mesh &>(xod_base);
      if (xod.is_edit_mode) {
        BMesh *bm = mesh->runtime->edit_mesh->bm;
        BM_mesh_vert_coords_apply_with_mat4(bm, xod.positions, transform);
        /* Always operate on all keys for the moment. */
        // key_index = bm->shapenr - 1;
      }
      else {
        copy_transformed_positions(xod.positions, transform, mesh->vert_positions_for_write());
        mesh->tag_positions_changed();
      }

      if (key != nullptr) {
        BKE_keyblock_data_set_with_mat4(key, key_index, xod.key_data, transform);
      }

      break;
    }
    case ID_LT: {
      const auto &xod = reinterpret_cast<XFormObjectData_Lattice &>(xod_base);
      Lattice *lt_orig = (Lattice *)xod_base.id;
      Lattice *lt = xod.is_edit_mode ? lt_orig->editlatt->latt : lt_orig;

      Key *key = lt->key;
      const int key_index = -1;

      BKE_lattice_vert_coords_apply_with_mat4(lt, xod.positions, transform);
      if (xod.is_edit_mode) {
        /* Always operate on all keys for the moment. */
        // key_index = lt_orig->editlatt->shapenr - 1;
      }

      if ((key != nullptr) && !xod.key_data.is_empty()) {
        BKE_keyblock_data_set_with_mat4(key, key_index, xod.key_data, transform);
      }

      break;
    }
    case ID_CU_LEGACY: {
      const auto &xod = reinterpret_cast<XFormObjectData_Curve &>(xod_base);
      BLI_assert(xod.is_edit_mode == false); /* Not used currently. */
      Curve *cu = (Curve *)xod_base.id;

      Key *key = cu->key;
      const int key_index = -1;
      ListBase *nurb = nullptr;

      if (xod.is_edit_mode) {
        EditNurb *editnurb = cu->editnurb;
        nurb = &editnurb->nurbs;
        BKE_curve_nurbs_vert_coords_apply_with_mat4(
            &editnurb->nurbs, xod.positions, transform, CU_IS_2D(cu));
        /* Always operate on all keys for the moment. */
        // key_index = editnurb->shapenr - 1;
      }
      else {
        nurb = &cu->nurb;
        BKE_curve_nurbs_vert_coords_apply_with_mat4(
            &cu->nurb, xod.positions, transform, CU_IS_2D(cu));
      }

      if ((key != nullptr) && !xod.key_data.is_empty()) {
        BKE_keyblock_curve_data_set_with_mat4(
            key, nurb, key_index, xod.key_data.data(), transform);
      }

      break;
    }
    case ID_AR: {
      const auto &xod = reinterpret_cast<XFormObjectData_Armature &>(xod_base);
      BLI_assert(xod.is_edit_mode == false); /* Not used currently. */
      bArmature *arm = (bArmature *)xod_base.id;
      if (xod.is_edit_mode) {
        edit_armature_coords_and_quats_apply_with_mat4(arm, xod.elems, transform);
      }
      else {
        armature_coords_and_quats_apply_with_mat4(arm, xod.elems, transform);
      }
      break;
    }
    case ID_MB: {
      /* Meta-balls are a special case, edit-mode and object mode data is shared. */
      MetaBall *mb = (MetaBall *)xod_base.id;
      const auto &xod = reinterpret_cast<XFormObjectData_MetaBall &>(xod_base);
      metaball_coords_and_quats_apply_with_mat4(mb, xod.elems, transform);
      break;
    }
    case ID_GP: {
      GreasePencil *grease_pencil = (GreasePencil *)xod_base.id;
      const auto &xod = reinterpret_cast<XFormObjectData_GreasePencil &>(xod_base);
      BKE_grease_pencil_point_coords_apply_with_mat4(
          *grease_pencil, xod.positions, xod.radii, transform);
      break;
    }
    case ID_CV: {
      Curves *curves_id = reinterpret_cast<Curves *>(xod_base.id);
      bke::CurvesGeometry &curves = curves_id->geometry.wrap();
      const auto &xod = reinterpret_cast<const XFormObjectData_Curves &>(xod_base);
      if (!curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
        copy_transformed_positions(xod.positions, transform, curves.positions_for_write());
      }
      else {
        Array<float3> transformed_positions(xod.positions.size());
        copy_transformed_positions(xod.positions, transform, transformed_positions);
        bke::curves::bezier::write_all_positions(
            curves, curves.curves_range(), transformed_positions);
      }
      copy_transformed_radii(xod.radii, transform, curves.radius_for_write());
      break;
    }
    case ID_PT: {
      PointCloud *pointcloud = reinterpret_cast<PointCloud *>(xod_base.id);
      const auto &xod = reinterpret_cast<const XFormObjectData_PointCloud &>(xod_base);
      copy_transformed_positions(xod.positions, transform, pointcloud->positions_for_write());
      copy_transformed_radii(xod.radii, transform, pointcloud->radius_for_write());
      break;
    }
    default: {
      break;
    }
  }
}

void data_xform_restore(XFormObjectData &xod_base)
{
  switch (GS(xod_base.id->name)) {
    case ID_ME: {
      Mesh *mesh = (Mesh *)xod_base.id;

      Key *key = mesh->key;
      const int key_index = -1;

      const auto &xod = reinterpret_cast<XFormObjectData_Mesh &>(xod_base);
      if (xod.is_edit_mode) {
        BMesh *bm = mesh->runtime->edit_mesh->bm;
        BM_mesh_vert_coords_apply(bm, xod.positions);
        /* Always operate on all keys for the moment. */
        // key_index = bm->shapenr - 1;
      }
      else {
        mesh->vert_positions_for_write().copy_from(xod.positions);
        mesh->tag_positions_changed();
      }

      if ((key != nullptr) && !xod.key_data.is_empty()) {
        BKE_keyblock_data_set(key, key_index, xod.key_data.data());
      }

      break;
    }
    case ID_LT: {
      const auto &xod = reinterpret_cast<XFormObjectData_Lattice &>(xod_base);
      Lattice *lt_orig = (Lattice *)xod_base.id;
      Lattice *lt = xod.is_edit_mode ? lt_orig->editlatt->latt : lt_orig;

      Key *key = lt->key;
      const int key_index = -1;

      BKE_lattice_vert_coords_apply(lt, xod.positions);
      if (xod.is_edit_mode) {
        /* Always operate on all keys for the moment. */
        // key_index = lt_orig->editlatt->shapenr - 1;
      }

      if ((key != nullptr) && !xod.key_data.is_empty()) {
        BKE_keyblock_data_set(key, key_index, xod.key_data.data());
      }

      break;
    }
    case ID_CU_LEGACY: {
      Curve *cu = (Curve *)xod_base.id;

      Key *key = cu->key;
      const int key_index = -1;

      const auto &xod = reinterpret_cast<XFormObjectData_Curve &>(xod_base);
      if (xod.is_edit_mode) {
        EditNurb *editnurb = cu->editnurb;
        BKE_curve_nurbs_vert_coords_apply(&editnurb->nurbs, xod.positions, CU_IS_2D(cu));
        /* Always operate on all keys for the moment. */
        // key_index = editnurb->shapenr - 1;
      }
      else {
        BKE_curve_nurbs_vert_coords_apply(&cu->nurb, xod.positions, CU_IS_2D(cu));
      }

      if ((key != nullptr) && !xod.key_data.is_empty()) {
        BKE_keyblock_data_set(key, key_index, xod.key_data.data());
      }

      break;
    }
    case ID_AR: {
      bArmature *arm = (bArmature *)xod_base.id;
      const auto &xod = reinterpret_cast<XFormObjectData_Armature &>(xod_base);
      if (xod.is_edit_mode) {
        edit_armature_coords_and_quats_apply(arm, xod.elems);
      }
      else {
        armature_coords_and_quats_apply(arm, xod.elems);
      }
      break;
    }
    case ID_MB: {
      /* Meta-balls are a special case, edit-mode and object mode data is shared. */
      MetaBall *mb = (MetaBall *)xod_base.id;
      const auto &xod = reinterpret_cast<XFormObjectData_MetaBall &>(xod_base);
      metaball_coords_and_quats_apply(mb, xod.elems);
      break;
    }
    case ID_GP: {
      GreasePencil *grease_pencil = (GreasePencil *)xod_base.id;
      const auto &xod = reinterpret_cast<XFormObjectData_GreasePencil &>(xod_base);
      BKE_grease_pencil_point_coords_apply(*grease_pencil, xod.positions, xod.radii);
      break;
    }
    case ID_CV: {
      Curves *curves_id = reinterpret_cast<Curves *>(xod_base.id);
      bke::CurvesGeometry &curves = curves_id->geometry.wrap();
      const auto &xod = reinterpret_cast<const XFormObjectData_Curves &>(xod_base);
      if (!curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
        curves.positions_for_write().copy_from(xod.positions);
      }
      else {
        bke::curves::bezier::write_all_positions(curves, curves.curves_range(), xod.positions);
      }
      curves.radius_for_write().copy_from(xod.radii);
      break;
    }
    case ID_PT: {
      PointCloud *pointcloud = reinterpret_cast<PointCloud *>(xod_base.id);
      const auto &xod = reinterpret_cast<const XFormObjectData_PointCloud &>(xod_base);
      pointcloud->positions_for_write().copy_from(xod.positions);
      pointcloud->radius_for_write().copy_from(xod.radii);
      break;
    }
    default: {
      break;
    }
  }
}

void data_xform_tag_update(XFormObjectData &xod_base)
{
  switch (GS(xod_base.id->name)) {
    case ID_ME: {
      Mesh *mesh = (Mesh *)xod_base.id;
      const auto &xod = reinterpret_cast<XFormObjectData_Mesh &>(xod_base);
      if (xod.is_edit_mode) {
        EDBMUpdate_Params params{};
        params.calc_looptris = true;
        params.calc_normals = true;
        params.is_destructive = false;
        EDBM_update(mesh, &params);
      }
      DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY);
      break;
    }
    case ID_LT: {
      /* Generic update. */
      Lattice *lt = (Lattice *)xod_base.id;
      DEG_id_tag_update(&lt->id, ID_RECALC_GEOMETRY);
      break;
    }
    case ID_CU_LEGACY: {
      /* Generic update. */
      Curve *cu = (Curve *)xod_base.id;
      DEG_id_tag_update(&cu->id, ID_RECALC_GEOMETRY);
      break;
    }
    case ID_AR: {
      /* Generic update. */
      bArmature *arm = (bArmature *)xod_base.id;
      /* XXX, zero is needed, no other flags properly update this. */
      DEG_id_tag_update(&arm->id, 0);
      break;
    }
    case ID_MB: {
      /* Generic update. */
      MetaBall *mb = (MetaBall *)xod_base.id;
      DEG_id_tag_update(&mb->id, ID_RECALC_GEOMETRY | ID_RECALC_SYNC_TO_EVAL);
      break;
    }
    case ID_GD_LEGACY: {
      /* Generic update. */
      bGPdata *gpd = (bGPdata *)xod_base.id;
      DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY | ID_RECALC_SYNC_TO_EVAL);
      break;
    }
    case ID_GP: {
      /* Generic update. */
      GreasePencil *grease_pencil = (GreasePencil *)xod_base.id;
      DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY | ID_RECALC_SYNC_TO_EVAL);
      break;
    }
    case ID_CV: {
      Curves *curves_id = reinterpret_cast<Curves *>(xod_base.id);
      bke::CurvesGeometry &curves = curves_id->geometry.wrap();
      curves.tag_positions_changed();
      curves.tag_radii_changed();
      DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY | ID_RECALC_SYNC_TO_EVAL);
      break;
    }
    case ID_PT: {
      PointCloud *pointcloud = reinterpret_cast<PointCloud *>(xod_base.id);
      pointcloud->tag_positions_changed();
      pointcloud->tag_radii_changed();
      DEG_id_tag_update(&pointcloud->id, ID_RECALC_GEOMETRY | ID_RECALC_SYNC_TO_EVAL);
      break;
    }
    default: {
      break;
    }
  }
}

/** \} */

}  // namespace blender::ed::object
