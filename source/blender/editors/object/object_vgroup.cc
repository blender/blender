/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <algorithm>
#include <cmath>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_array.hh"
#include "BLI_bitmap.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"
#include "BLI_utildefines_stack.h"
#include "BLI_vector.hh"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_grease_pencil_vertex_groups.hh"
#include "BKE_lattice.hh"
#include "BKE_library.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_object_deform.h"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "BLT_translation.hh"

#include "DNA_armature_types.h"
#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_object_vgroup.hh"
#include "ED_screen.hh"

#include "UI_resources.hh"

#include "object_intern.hh"

namespace blender::ed::object {

static bool vertex_group_supported_poll_ex(bContext *C, const Object *ob);

/* -------------------------------------------------------------------- */
/** \name Local Utility Functions
 * \{ */

static bool object_array_for_wpaint_filter(const Object *ob, void *user_data)
{
  bContext *C = static_cast<bContext *>(user_data);
  if (vertex_group_supported_poll_ex(C, ob)) {
    return true;
  }
  return false;
}

static Vector<Object *> object_array_for_wpaint(bContext *C)
{
  return objects_in_mode_or_selected(C, object_array_for_wpaint_filter, C);
}

static bool vertex_group_use_vert_sel(Object *ob)
{
  if (ob->mode == OB_MODE_EDIT) {
    return true;
  }
  if ((ob->type == OB_MESH) &&
      ((Mesh *)ob->data)->editflag & (ME_EDIT_PAINT_VERT_SEL | ME_EDIT_PAINT_FACE_SEL))
  {
    return true;
  }
  return false;
}

static Lattice *vgroup_edit_lattice(Object *ob)
{
  Lattice *lt = static_cast<Lattice *>(ob->data);
  BLI_assert(ob->type == OB_LATTICE);
  return (lt->editlatt) ? lt->editlatt->latt : lt;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Utility Functions
 * \{ */

bool vgroup_sync_from_pose(Object *ob)
{
  Object *armobj = BKE_object_pose_armature_get(ob);
  if (armobj && (armobj->mode & OB_MODE_POSE)) {
    bArmature *arm = static_cast<bArmature *>(armobj->data);
    if (arm->act_bone) {
      int def_num = BKE_object_defgroup_name_index(ob, arm->act_bone->name);
      if (def_num != -1) {
        BKE_object_defgroup_active_index_set(ob, def_num + 1);
        return true;
      }
    }
  }
  return false;
}

void vgroup_data_clamp_range(ID *id, const int total)
{
  MDeformVert **dvert_arr;
  int dvert_tot;

  if (vgroup_parray_alloc(id, &dvert_arr, &dvert_tot, false)) {
    for (int i = 0; i < dvert_tot; i++) {
      MDeformVert *dv = dvert_arr[i];
      for (int j = 0; j < dv->totweight; j++) {
        if (dv->dw[j].def_nr >= total) {
          BKE_defvert_remove_group(dv, &dv->dw[j]);
          j--;
        }
      }
    }
  }
}

bool vgroup_parray_alloc(ID *id,
                         MDeformVert ***dvert_arr,
                         int *dvert_tot,
                         const bool use_vert_sel,
                         std::optional<int> current_frame)
{
  *dvert_tot = 0;
  *dvert_arr = nullptr;

  if (id) {
    switch (GS(id->name)) {
      case ID_ME: {
        Mesh *mesh = (Mesh *)id;

        if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
          BMesh *bm = em->bm;
          const int cd_dvert_offset = CustomData_get_offset(&bm->vdata, CD_MDEFORMVERT);
          BMIter iter;
          BMVert *eve;
          int i;

          if (cd_dvert_offset == -1) {
            return false;
          }

          i = em->bm->totvert;

          *dvert_arr = MEM_malloc_arrayN<MDeformVert *>(i, __func__);
          *dvert_tot = i;

          i = 0;
          if (use_vert_sel) {
            BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
              (*dvert_arr)[i] = BM_elem_flag_test(eve, BM_ELEM_SELECT) ?
                                    static_cast<MDeformVert *>(
                                        BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset)) :
                                    nullptr;
              i++;
            }
          }
          else {
            BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
              (*dvert_arr)[i] = static_cast<MDeformVert *>(
                  BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset));
              i++;
            }
          }

          return true;
        }
        if (!mesh->deform_verts().is_empty()) {
          MutableSpan<MDeformVert> dverts = mesh->deform_verts_for_write();

          *dvert_tot = mesh->verts_num;
          *dvert_arr = MEM_malloc_arrayN<MDeformVert *>(mesh->verts_num, __func__);

          if (use_vert_sel) {
            const bke::AttributeAccessor attributes = mesh->attributes();
            const VArray<bool> select_vert = *attributes.lookup_or_default<bool>(
                ".select_vert", bke::AttrDomain::Point, false);

            for (int i = 0; i < mesh->verts_num; i++) {
              (*dvert_arr)[i] = select_vert[i] ? &dverts[i] : nullptr;
            }
          }
          else {
            for (int i = 0; i < mesh->verts_num; i++) {
              (*dvert_arr)[i] = &dverts[i];
            }
          }

          return true;
        }
        return false;
      }
      case ID_LT: {
        Lattice *lt = (Lattice *)id;
        lt = (lt->editlatt) ? lt->editlatt->latt : lt;

        if (lt->dvert) {
          BPoint *def = lt->def;
          *dvert_tot = lt->pntsu * lt->pntsv * lt->pntsw;
          *dvert_arr = MEM_malloc_arrayN<MDeformVert *>((*dvert_tot), __func__);

          if (use_vert_sel) {
            for (int i = 0; i < *dvert_tot; i++) {
              (*dvert_arr)[i] = (def->f1 & SELECT) ? &lt->dvert[i] : nullptr;
            }
          }
          else {
            for (int i = 0; i < *dvert_tot; i++) {
              (*dvert_arr)[i] = lt->dvert + i;
            }
          }

          return true;
        }
        return false;
      }
      case ID_GP: {
        if (!current_frame) {
          return false;
        }
        GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(id);
        bke::greasepencil::Layer *layer = grease_pencil->get_active_layer();
        if (!layer) {
          return false;
        }
        bke::greasepencil::Drawing *drawing = grease_pencil->get_editable_drawing_at(
            *layer, *current_frame);
        if (!drawing) {
          return false;
        }
        bke::CurvesGeometry &curves = drawing->strokes_for_write();
        MutableSpan<MDeformVert> dverts = curves.deform_verts_for_write();

        if (!dverts.is_empty()) {
          const int points_num = curves.points_num();
          *dvert_tot = points_num;
          *dvert_arr = MEM_malloc_arrayN<MDeformVert *>(points_num, __func__);

          for (int i = 0; i < points_num; i++) {
            (*dvert_arr)[i] = &dverts[i];
          }

          return true;
        }
        return false;
      }

      default:
        break;
    }
  }

  return false;
}

void vgroup_parray_mirror_sync(Object *ob,
                               MDeformVert **dvert_array,
                               const int dvert_tot,
                               const bool *vgroup_validmap,
                               const int vgroup_tot)
{
  BMEditMesh *em = BKE_editmesh_from_object(ob);
  MDeformVert **dvert_array_all = nullptr;
  int dvert_tot_all;

  /* get an array of all verts, not only selected */
  if (vgroup_parray_alloc(static_cast<ID *>(ob->data), &dvert_array_all, &dvert_tot_all, false) ==
      false)
  {
    BLI_assert(0);
    return;
  }
  if (em) {
    BM_mesh_elem_table_ensure(em->bm, BM_VERT);
  }

  int flip_map_len;
  const int *flip_map = BKE_object_defgroup_flip_map(ob, true, &flip_map_len);

  for (int i_src = 0; i_src < dvert_tot; i_src++) {
    if (dvert_array[i_src] != nullptr) {
      /* its selected, check if its mirror exists */
      int i_dst = ED_mesh_mirror_get_vert(ob, i_src);
      if (i_dst != -1 && dvert_array_all[i_dst] != nullptr) {
        /* we found a match! */
        const MDeformVert *dv_src = dvert_array[i_src];
        MDeformVert *dv_dst = dvert_array_all[i_dst];

        BKE_defvert_mirror_subset(
            dv_dst, dv_src, vgroup_validmap, vgroup_tot, flip_map, flip_map_len);

        dvert_array[i_dst] = dvert_array_all[i_dst];
      }
    }
  }

  MEM_freeN(flip_map);
  MEM_freeN(dvert_array_all);
}

void vgroup_parray_mirror_assign(Object *ob, MDeformVert **dvert_array, const int dvert_tot)
{
  BMEditMesh *em = BKE_editmesh_from_object(ob);
  MDeformVert **dvert_array_all = nullptr;
  int dvert_tot_all;

  /* get an array of all verts, not only selected */
  if (vgroup_parray_alloc(static_cast<ID *>(ob->data), &dvert_array_all, &dvert_tot_all, false) ==
      false)
  {
    BLI_assert(0);
    return;
  }
  BLI_assert(dvert_tot == dvert_tot_all);
  if (em) {
    BM_mesh_elem_table_ensure(em->bm, BM_VERT);
  }

  for (int i = 0; i < dvert_tot; i++) {
    if (dvert_array[i] == nullptr) {
      /* its unselected, check if its mirror is */
      int i_sel = ED_mesh_mirror_get_vert(ob, i);
      if ((i_sel != -1) && (i_sel != i) && (dvert_array[i_sel])) {
        /* we found a match! */
        dvert_array[i] = dvert_array_all[i];
      }
    }
  }

  MEM_freeN(dvert_array_all);
}

void vgroup_parray_remove_zero(MDeformVert **dvert_array,
                               const int dvert_tot,
                               const bool *vgroup_validmap,
                               const int vgroup_tot,
                               const float epsilon,
                               const bool keep_single)
{
  MDeformVert *dv;

  for (int i = 0; i < dvert_tot; i++) {
    /* in case its not selected */
    if (!(dv = dvert_array[i])) {
      continue;
    }

    int j = dv->totweight;

    while (j--) {
      MDeformWeight *dw;

      if (keep_single && dv->totweight == 1) {
        break;
      }

      dw = dv->dw + j;
      if ((dw->def_nr < vgroup_tot) && vgroup_validmap[dw->def_nr]) {
        if (dw->weight <= epsilon) {
          BKE_defvert_remove_group(dv, dw);
        }
      }
    }
  }
}

bool vgroup_array_copy(Object *ob, Object *ob_from)
{
  MDeformVert **dvert_array_from = nullptr, **dvf;
  MDeformVert **dvert_array = nullptr, **dv;
  int dvert_tot_from;
  int dvert_tot;
  int i;
  ListBase *defbase_dst = BKE_object_defgroup_list_mutable(ob);
  const ListBase *defbase_src = BKE_object_defgroup_list(ob_from);

  int defbase_tot_from = BLI_listbase_count(defbase_src);
  int defbase_tot = BLI_listbase_count(defbase_dst);
  bool new_vgroup = false;

  BLI_assert(ob != ob_from);

  if (ob->data == ob_from->data) {
    return true;
  }

  /* In case we copy vgroup between two objects using same data,
   * we only have to care about object side of things. */
  if (ob->data != ob_from->data) {
    vgroup_parray_alloc(
        static_cast<ID *>(ob_from->data), &dvert_array_from, &dvert_tot_from, false);
    vgroup_parray_alloc(static_cast<ID *>(ob->data), &dvert_array, &dvert_tot, false);

    if ((dvert_array == nullptr) && (dvert_array_from != nullptr) &&
        BKE_object_defgroup_data_create(static_cast<ID *>(ob->data)))
    {
      vgroup_parray_alloc(static_cast<ID *>(ob->data), &dvert_array, &dvert_tot, false);
      new_vgroup = true;
    }

    if (dvert_tot == 0 || (dvert_tot != dvert_tot_from) || dvert_array_from == nullptr ||
        dvert_array == nullptr)
    {
      if (dvert_array) {
        MEM_freeN(dvert_array);
      }
      if (dvert_array_from) {
        MEM_freeN(dvert_array_from);
      }

      if (new_vgroup == true) {
        /* free the newly added vgroup since it wasn't compatible */
        BKE_object_defgroup_remove_all(ob);
      }

      /* if true: both are 0 and nothing needs changing, consider this a success */
      return (dvert_tot == dvert_tot_from);
    }
  }

  /* do the copy */
  BLI_freelistN(defbase_dst);
  BLI_duplicatelist(defbase_dst, defbase_src);
  BKE_object_defgroup_active_index_set(ob, BKE_object_defgroup_active_index_get(ob_from));

  if (defbase_tot_from < defbase_tot) {
    /* correct vgroup indices because the number of vgroups is being reduced. */
    Array<int> remap(defbase_tot + 1);
    for (i = 0; i <= defbase_tot_from; i++) {
      remap[i] = i;
    }
    for (; i <= defbase_tot; i++) {
      remap[i] = 0; /* can't use these, so disable */
    }

    BKE_object_defgroup_remap_update_users(ob, remap.data());
  }

  if (dvert_array_from != nullptr && dvert_array != nullptr) {
    dvf = dvert_array_from;
    dv = dvert_array;

    for (i = 0; i < dvert_tot; i++, dvf++, dv++) {
      MEM_SAFE_FREE((*dv)->dw);
      *(*dv) = *(*dvf);

      if ((*dv)->dw) {
        (*dv)->dw = static_cast<MDeformWeight *>(MEM_dupallocN((*dv)->dw));
      }
    }

    MEM_freeN(dvert_array);
    MEM_freeN(dvert_array_from);
  }

  return true;
}

void vgroup_parray_to_weight_array(const MDeformVert **dvert_array,
                                   const int dvert_tot,
                                   float *dvert_weights,
                                   const int def_nr)
{
  for (int i = 0; i < dvert_tot; i++) {
    const MDeformVert *dv = dvert_array[i];
    dvert_weights[i] = dv ? BKE_defvert_find_weight(dv, def_nr) : 0.0f;
  }
}

void vgroup_parray_from_weight_array(MDeformVert **dvert_array,
                                     const int dvert_tot,
                                     const float *dvert_weights,
                                     const int def_nr,
                                     const bool remove_zero)
{
  int i;

  for (i = 0; i < dvert_tot; i++) {
    MDeformVert *dv = dvert_array[i];
    if (dv) {
      if (dvert_weights[i] > 0.0f) {
        MDeformWeight *dw = BKE_defvert_ensure_index(dv, def_nr);
        BLI_assert(IN_RANGE_INCL(dvert_weights[i], 0.0f, 1.0f));
        dw->weight = dvert_weights[i];
      }
      else {
        MDeformWeight *dw = BKE_defvert_find_index(dv, def_nr);
        if (dw) {
          if (remove_zero) {
            BKE_defvert_remove_group(dv, dw);
          }
          else {
            dw->weight = 0.0f;
          }
        }
      }
    }
  }
}

/* TODO: cache flip data to speedup calls within a loop. */
static void mesh_defvert_mirror_update_internal(Object *ob,
                                                MDeformVert *dvert_dst,
                                                MDeformVert *dvert_src,
                                                const int def_nr)
{
  if (def_nr == -1) {
    /* All vgroups, add groups where needed. */
    int flip_map_len;
    int *flip_map = BKE_object_defgroup_flip_map_unlocked(ob, true, &flip_map_len);
    BKE_defvert_sync_mapped(dvert_dst, dvert_src, flip_map, flip_map_len, true);
    MEM_freeN(flip_map);
  }
  else {
    /* Single vgroup. */
    MDeformWeight *dw = BKE_defvert_ensure_index(dvert_dst,
                                                 BKE_object_defgroup_flip_index(ob, def_nr, true));
    if (dw) {
      dw->weight = BKE_defvert_find_weight(dvert_src, def_nr);
    }
  }
}

static void mesh_defvert_mirror_update_em(
    Object *ob, BMVert *eve, int def_nr, int vidx, const int cd_dvert_offset)
{
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  BMEditMesh *em = mesh->runtime->edit_mesh.get();
  BMVert *eve_mirr;
  bool use_topology = (mesh->editflag & ME_EDIT_MIRROR_TOPO) != 0;

  eve_mirr = editbmesh_get_x_mirror_vert(ob, em, eve, eve->co, vidx, use_topology);

  if (eve_mirr && eve_mirr != eve) {
    MDeformVert *dvert_src = static_cast<MDeformVert *>(
        BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset));
    MDeformVert *dvert_dst = static_cast<MDeformVert *>(
        BM_ELEM_CD_GET_VOID_P(eve_mirr, cd_dvert_offset));
    mesh_defvert_mirror_update_internal(ob, dvert_dst, dvert_src, def_nr);
  }
}

static void mesh_defvert_mirror_update_ob(Object *ob, int def_nr, int vidx)
{
  int vidx_mirr;
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  bool use_topology = (mesh->editflag & ME_EDIT_MIRROR_TOPO) != 0;

  if (vidx == -1) {
    return;
  }

  vidx_mirr = mesh_get_x_mirror_vert(ob, nullptr, vidx, use_topology);

  MutableSpan<MDeformVert> dverts = mesh->deform_verts_for_write();
  if ((vidx_mirr) >= 0 && (vidx_mirr != vidx)) {
    MDeformVert *dvert_src = &dverts[vidx];
    MDeformVert *dvert_dst = &dverts[vidx_mirr];
    mesh_defvert_mirror_update_internal(ob, dvert_dst, dvert_src, def_nr);
  }
}

void vgroup_vert_active_mirror(Object *ob, int def_nr)
{
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  BMEditMesh *em = mesh->runtime->edit_mesh.get();
  MDeformVert *dvert_act;

  if (mesh->symmetry & ME_SYMMETRY_X) {
    if (em) {
      BMVert *eve_act;
      dvert_act = ED_mesh_active_dvert_get_em(ob, &eve_act);
      if (dvert_act) {
        const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);
        mesh_defvert_mirror_update_em(ob, eve_act, def_nr, -1, cd_dvert_offset);
      }
    }
    else {
      int v_act;
      dvert_act = ED_mesh_active_dvert_get_ob(ob, &v_act);
      if (dvert_act) {
        mesh_defvert_mirror_update_ob(ob, def_nr, v_act);
      }
    }
  }
}

static void vgroup_remove_weight(Object *ob, const int def_nr)
{
  MDeformVert *dvert_act;
  MDeformWeight *dw;

  dvert_act = ED_mesh_active_dvert_get_only(ob);

  dw = BKE_defvert_find_index(dvert_act, def_nr);
  BKE_defvert_remove_group(dvert_act, dw);
}

static bool vgroup_normalize_active_vertex(Object *ob, eVGroupSelect subset_type)
{
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  BMEditMesh *em = mesh->runtime->edit_mesh.get();
  BMVert *eve_act;
  int v_act;
  MDeformVert *dvert_act;
  int subset_count, vgroup_tot;
  const bool *vgroup_validmap;

  if (em) {
    dvert_act = ED_mesh_active_dvert_get_em(ob, &eve_act);
  }
  else {
    dvert_act = ED_mesh_active_dvert_get_ob(ob, &v_act);
  }

  if (dvert_act == nullptr) {
    return false;
  }

  vgroup_validmap = BKE_object_defgroup_subset_from_select_type(
      ob, subset_type, &vgroup_tot, &subset_count);

  bool *lock_flags = BKE_object_defgroup_lock_flags_get(ob, vgroup_tot);

  if (lock_flags) {
    BKE_defvert_normalize_lock_map(
        *dvert_act, Span(vgroup_validmap, vgroup_tot), Span(lock_flags, vgroup_tot));
    MEM_freeN(lock_flags);
  }
  else {
    BKE_defvert_normalize_subset(*dvert_act, Span(vgroup_validmap, vgroup_tot));
  }

  MEM_freeN(vgroup_validmap);

  if (mesh->symmetry & ME_SYMMETRY_X) {
    if (em) {
      const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);
      mesh_defvert_mirror_update_em(ob, eve_act, -1, -1, cd_dvert_offset);
    }
    else {
      mesh_defvert_mirror_update_ob(ob, -1, v_act);
    }
  }

  return true;
}

static void vgroup_copy_active_to_sel(Object *ob, eVGroupSelect subset_type)
{
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  MDeformVert *dvert_act;
  int i, vgroup_tot, subset_count;
  const bool *vgroup_validmap = BKE_object_defgroup_subset_from_select_type(
      ob, subset_type, &vgroup_tot, &subset_count);

  if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
    BMIter iter;
    BMVert *eve, *eve_act;
    const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

    dvert_act = ED_mesh_active_dvert_get_em(ob, &eve_act);
    if (dvert_act) {
      BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
        if (BM_elem_flag_test(eve, BM_ELEM_SELECT) && eve != eve_act) {
          MDeformVert *dv = static_cast<MDeformVert *>(
              BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset));
          BKE_defvert_copy_subset(dv, dvert_act, vgroup_validmap, vgroup_tot);
          if (mesh->symmetry & ME_SYMMETRY_X) {
            mesh_defvert_mirror_update_em(ob, eve, -1, i, cd_dvert_offset);
          }
        }
      }
    }
  }
  else {
    const bke::AttributeAccessor attributes = mesh->attributes();
    const VArray<bool> select_vert = *attributes.lookup_or_default<bool>(
        ".select_vert", bke::AttrDomain::Point, false);

    int v_act;

    dvert_act = ED_mesh_active_dvert_get_ob(ob, &v_act);
    if (dvert_act) {
      MutableSpan<MDeformVert> dverts = mesh->deform_verts_for_write();
      for (i = 0; i < mesh->verts_num; i++) {
        if (select_vert[i] && &dverts[i] != dvert_act) {
          BKE_defvert_copy_subset(&dverts[i], dvert_act, vgroup_validmap, vgroup_tot);
          if (mesh->symmetry & ME_SYMMETRY_X) {
            mesh_defvert_mirror_update_ob(ob, -1, i);
          }
        }
      }
    }
  }

  MEM_freeN(vgroup_validmap);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shared Weight Transfer Operator Properties
 * \{ */

static const EnumPropertyItem WT_vertex_group_select_item[] = {
    {WT_VGROUP_ACTIVE, "ACTIVE", 0, "Active Group", "The active Vertex Group"},
    {WT_VGROUP_BONE_SELECT,
     "BONE_SELECT",
     0,
     "Selected Pose Bones",
     "All Vertex Groups assigned to Selection"},
    {WT_VGROUP_BONE_DEFORM,
     "BONE_DEFORM",
     0,
     "Deform Pose Bones",
     "All Vertex Groups assigned to Deform Bones"},
    {WT_VGROUP_ALL, "ALL", 0, "All Groups", "All Vertex Groups"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem *vgroup_selection_itemf_helper(const bContext *C,
                                                      PointerRNA * /*ptr*/,
                                                      PropertyRNA * /*prop*/,
                                                      bool *r_free,
                                                      const uint selection_mask)
{
  Object *ob;
  EnumPropertyItem *item = nullptr;
  int totitem = 0;

  if (C == nullptr) {
    /* needed for docs and i18n tools */
    return WT_vertex_group_select_item;
  }

  ob = context_object(C);
  if (selection_mask & (1 << WT_VGROUP_ACTIVE)) {
    RNA_enum_items_add_value(&item, &totitem, WT_vertex_group_select_item, WT_VGROUP_ACTIVE);
  }

  if (ob) {
    if (BKE_object_pose_armature_get(ob)) {
      if (selection_mask & (1 << WT_VGROUP_BONE_SELECT)) {
        RNA_enum_items_add_value(
            &item, &totitem, WT_vertex_group_select_item, WT_VGROUP_BONE_SELECT);
      }
    }

    if (BKE_modifiers_is_deformed_by_armature(ob)) {
      if (selection_mask & (1 << WT_VGROUP_BONE_DEFORM)) {
        RNA_enum_items_add_value(
            &item, &totitem, WT_vertex_group_select_item, WT_VGROUP_BONE_DEFORM);
      }
    }
  }

  if (selection_mask & (1 << WT_VGROUP_ALL)) {
    RNA_enum_items_add_value(&item, &totitem, WT_vertex_group_select_item, WT_VGROUP_ALL);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static const EnumPropertyItem *rna_vertex_group_with_single_itemf(bContext *C,
                                                                  PointerRNA *ptr,
                                                                  PropertyRNA *prop,
                                                                  bool *r_free)
{
  return vgroup_selection_itemf_helper(C, ptr, prop, r_free, WT_VGROUP_MASK_ALL);
}

static const EnumPropertyItem *rna_vertex_group_select_itemf(bContext *C,
                                                             PointerRNA *ptr,
                                                             PropertyRNA *prop,
                                                             bool *r_free)
{
  return vgroup_selection_itemf_helper(
      C, ptr, prop, r_free, WT_VGROUP_MASK_ALL & ~(1 << WT_VGROUP_ACTIVE));
}

static void vgroup_operator_subset_select_props(wmOperatorType *ot, bool use_active)
{
  PropertyRNA *prop;

  prop = RNA_def_enum(ot->srna,
                      "group_select_mode",
                      rna_enum_dummy_NULL_items,
                      use_active ? WT_VGROUP_ACTIVE : WT_VGROUP_ALL,
                      "Subset",
                      "Define which subset of groups shall be used");

  if (use_active) {
    RNA_def_enum_funcs(prop, rna_vertex_group_with_single_itemf);
  }
  else {
    RNA_def_enum_funcs(prop, rna_vertex_group_select_itemf);
  }
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name High Level Vertex Group Add/Remove
 *
 * Wrap lower level `BKE` functions.
 *
 * \note that operations on many vertices should use #vgroup_parray_alloc.
 * \{ */

/* for Mesh in Object mode */
/* allows editmode for Lattice */
static void vgroup_nr_vert_add(
    Object *ob, const int def_nr, const int vertnum, const float weight, const int assignmode)
{
  /* Add the vert to the deform group with the specified number. */
  MDeformVert *dvert = nullptr;
  int tot;

  /* Get the vert. */
  BKE_object_defgroup_array_get(static_cast<ID *>(ob->data), &dvert, &tot);

  if (dvert == nullptr) {
    return;
  }

  /* Check that vertnum is valid before trying to get the relevant dvert. */
  if ((vertnum < 0) || (vertnum >= tot)) {
    return;
  }

  MDeformVert *dv = &dvert[vertnum];
  MDeformWeight *dw;

  /* Lets first check to see if this vert is already in the weight group - if so lets update it. */
  dw = BKE_defvert_find_index(dv, def_nr);

  if (dw) {
    switch (assignmode) {
      case WEIGHT_REPLACE:
        dw->weight = weight;
        break;
      case WEIGHT_ADD:
        dw->weight += weight;
        dw->weight = std::min(dw->weight, 1.0f);
        break;
      case WEIGHT_SUBTRACT:
        dw->weight -= weight;
        /* If the weight is zero or less than remove the vert from the deform group. */
        if (dw->weight <= 0.0f) {
          BKE_defvert_remove_group(dv, dw);
        }
        break;
    }
  }
  else {
    /* If the vert wasn't in the deform group then we must take a different form of action. */

    switch (assignmode) {
      case WEIGHT_SUBTRACT:
        /* If we are subtracting then we don't need to do anything. */
        return;

      case WEIGHT_REPLACE:
      case WEIGHT_ADD:
        /* If we are doing an additive assignment, then we need to create the deform weight. */

        /* We checked if the vertex was added before so no need to test again, simply add. */
        BKE_defvert_add_index_notest(dv, def_nr, weight);
        break;
    }
  }
}

void vgroup_vert_add(Object *ob, bDeformGroup *dg, int vertnum, float weight, int assignmode)
{
  /* add the vert to the deform group with the
   * specified assign mode
   */
  const ListBase *defbase = BKE_object_defgroup_list(ob);
  const int def_nr = BLI_findindex(defbase, dg);

  MDeformVert *dv = nullptr;
  int tot;

  /* get the deform group number, exit if
   * it can't be found
   */
  if (def_nr != -1) {

    /* if there's no deform verts then create some,
     */
    if (BKE_object_defgroup_array_get(static_cast<ID *>(ob->data), &dv, &tot) && dv == nullptr) {
      BKE_object_defgroup_data_create(static_cast<ID *>(ob->data));
    }

    /* call another function to do the work
     */
    vgroup_nr_vert_add(ob, def_nr, vertnum, weight, assignmode);
  }
}

void vgroup_vert_remove(Object *ob, bDeformGroup *dg, int vertnum)
{
  /* This routine removes the vertex from the specified
   * deform group.
   */

  /* TODO(@ideasman42): This is slow in a loop, better pass def_nr directly,
   * but leave for later. */
  const ListBase *defbase = BKE_object_defgroup_list(ob);
  const int def_nr = BLI_findindex(defbase, dg);

  if (def_nr != -1) {
    MDeformVert *dvert = nullptr;
    int tot;

    /* get the deform vertices corresponding to the
     * vertnum
     */
    BKE_object_defgroup_array_get(static_cast<ID *>(ob->data), &dvert, &tot);

    if (dvert) {
      MDeformVert *dv = &dvert[vertnum];
      MDeformWeight *dw;

      dw = BKE_defvert_find_index(dv, def_nr);
      BKE_defvert_remove_group(dv, dw); /* dw can be nullptr */
    }
  }
}

static float get_vert_def_nr(Object *ob, const int def_nr, const int vertnum)
{
  const MDeformVert *dv = nullptr;

  /* get the deform vertices corresponding to the vertnum */
  if (ob->type == OB_MESH) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);

    if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
      const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);
      /* warning, this lookup is _not_ fast */

      if (cd_dvert_offset != -1 && vertnum < em->bm->totvert) {
        BMVert *eve;
        BM_mesh_elem_table_ensure(em->bm, BM_VERT);
        eve = BM_vert_at_index(em->bm, vertnum);
        dv = static_cast<const MDeformVert *>(BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset));
      }
      else {
        return 0.0f;
      }
    }
    else {
      const Span<MDeformVert> dverts = mesh->deform_verts();
      if (!dverts.is_empty()) {
        if (vertnum >= mesh->verts_num) {
          return 0.0f;
        }
        dv = &dverts[vertnum];
      }
    }
  }
  else if (ob->type == OB_LATTICE) {
    Lattice *lt = vgroup_edit_lattice(ob);

    if (lt->dvert) {
      if (vertnum >= lt->pntsu * lt->pntsv * lt->pntsw) {
        return 0.0f;
      }
      dv = &lt->dvert[vertnum];
    }
  }

  if (dv) {
    MDeformWeight *dw = BKE_defvert_find_index(dv, def_nr);
    if (dw) {
      return dw->weight;
    }
  }

  return -1;
}

float vgroup_vert_weight(Object *ob, bDeformGroup *dg, int vertnum)
{
  const ListBase *defbase = BKE_object_defgroup_list(ob);
  const int def_nr = BLI_findindex(defbase, dg);

  if (def_nr == -1) {
    return -1;
  }

  return get_vert_def_nr(ob, def_nr, vertnum);
}

void vgroup_select_by_name(Object *ob, const char *name)
{
  /* NOTE: `actdef == 0` signals on painting to create a new one,
   * if a bone in pose-mode is selected. */
  BKE_object_defgroup_active_index_set(ob, BKE_object_defgroup_name_index(ob, name) + 1);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Function Implementations
 * \{ */

static void vgroup_grease_pencil_select_verts(const Scene &scene,
                                              const ToolSettings &tool_settings,
                                              const bDeformGroup *def_group,
                                              const bool select,
                                              Object &object)
{
  using namespace bke;
  using namespace ed::greasepencil;
  const bke::AttrDomain selection_domain = ED_grease_pencil_edit_selection_domain_get(
      &tool_settings);
  GreasePencil *grease_pencil = static_cast<GreasePencil *>(object.data);

  Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, *grease_pencil);
  for (MutableDrawingInfo &info : drawings) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    ListBase &vertex_group_names = curves.vertex_group_names;

    const int def_nr = BKE_defgroup_name_index(&vertex_group_names, def_group->name);
    if (def_nr < 0) {
      /* No vertices assigned to the group in this drawing. */
      continue;
    }

    const Span<MDeformVert> dverts = curves.deform_verts();
    if (dverts.is_empty()) {
      continue;
    }

    GSpanAttributeWriter selection = ed::curves::ensure_selection_attribute(
        curves, selection_domain, bke::AttrType::Bool);
    switch (selection_domain) {
      case AttrDomain::Point:
        threading::parallel_for(curves.points_range(), 4096, [&](const IndexRange range) {
          for (const int point_i : range) {
            if (BKE_defvert_find_index(&dverts[point_i], def_nr)) {
              selection.span.typed<bool>()[point_i] = select;
            }
          }
        });
        break;
      case AttrDomain::Curve: {
        const OffsetIndices<int> points_by_curve = curves.points_by_curve();
        threading::parallel_for(curves.curves_range(), 1024, [&](const IndexRange range) {
          for (const int curve_i : range) {
            const IndexRange points = points_by_curve[curve_i];
            bool any_point_in_group = false;
            for (const int point_i : points) {
              if (BKE_defvert_find_index(&dverts[point_i], def_nr)) {
                any_point_in_group = true;
                break;
              }
            }
            if (any_point_in_group) {
              selection.span.typed<bool>()[curve_i] = select;
            }
          }
        });
        break;
      }
      default:
        BLI_assert_unreachable();
        break;
    }
    selection.finish();
  }

  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
}

/* only in editmode */
static void vgroup_select_verts(const ToolSettings &tool_settings,
                                Object *ob,
                                Scene &scene,
                                int select)
{
  const int def_nr = BKE_object_defgroup_active_index_get(ob) - 1;

  const ListBase *defbase = BKE_object_defgroup_list(ob);
  const bDeformGroup *def_group = static_cast<bDeformGroup *>(BLI_findlink(defbase, def_nr));
  if (!def_group) {
    return;
  }

  if (ob->type == OB_MESH) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);

    if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
      const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

      if (cd_dvert_offset != -1) {
        BMIter iter;
        BMVert *eve;

        BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
          if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
            MDeformVert *dv = static_cast<MDeformVert *>(
                BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset));
            if (BKE_defvert_find_index(dv, def_nr)) {
              BM_vert_select_set(em->bm, eve, select);
            }
          }
        }

        /* This has to be called, because this function operates on vertices only.
         * Vertices to edges/faces. */
        EDBM_select_flush_from_verts(em, select);

        EDBM_uvselect_clear(em);
      }
    }
    else {
      const Span<MDeformVert> dverts = mesh->deform_verts();
      if (!dverts.is_empty()) {
        bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
        const VArray<bool> hide_vert = *attributes.lookup_or_default<bool>(
            ".hide_vert", bke::AttrDomain::Point, false);
        bke::SpanAttributeWriter<bool> select_vert =
            attributes.lookup_or_add_for_write_only_span<bool>(".select_vert",
                                                               bke::AttrDomain::Point);

        for (const int i : select_vert.span.index_range()) {
          if (!hide_vert[i]) {
            if (BKE_defvert_find_index(&dverts[i], def_nr)) {
              select_vert.span[i] = select;
            }
          }
        }

        select_vert.finish();
        paintvert_flush_flags(ob);
      }
    }
  }
  else if (ob->type == OB_LATTICE) {
    Lattice *lt = vgroup_edit_lattice(ob);

    if (lt->dvert) {
      MDeformVert *dv;
      BPoint *bp, *actbp = BKE_lattice_active_point_get(lt);
      int a, tot;

      dv = lt->dvert;

      tot = lt->pntsu * lt->pntsv * lt->pntsw;
      for (a = 0, bp = lt->def; a < tot; a++, bp++, dv++) {
        if (BKE_defvert_find_index(dv, def_nr)) {
          if (select) {
            bp->f1 |= SELECT;
          }
          else {
            bp->f1 &= ~SELECT;
            if (actbp && bp == actbp) {
              lt->actbp = LT_ACTBP_NONE;
            }
          }
        }
      }
    }
  }
  else if (ob->type == OB_GREASE_PENCIL) {
    vgroup_grease_pencil_select_verts(scene, tool_settings, def_group, select, *ob);
  }
}

static void vgroup_duplicate(Object *ob)
{
  bDeformGroup *dg, *cdg;
  char name[sizeof(dg->name)];
  MDeformWeight *dw_org, *dw_cpy;
  MDeformVert **dvert_array = nullptr;
  int i, idg, icdg, dvert_tot = 0;

  ListBase *defbase = BKE_object_defgroup_list_mutable(ob);

  dg = static_cast<bDeformGroup *>(
      BLI_findlink(defbase, BKE_object_defgroup_active_index_get(ob) - 1));
  if (!dg) {
    return;
  }

  if (!strstr(dg->name, "_copy")) {
    SNPRINTF_UTF8(name, "%s_copy", dg->name);
  }
  else {
    STRNCPY_UTF8(name, dg->name);
  }

  cdg = BKE_defgroup_duplicate(dg);
  STRNCPY_UTF8(cdg->name, name);
  BKE_object_defgroup_unique_name(cdg, ob);

  BLI_addtail(defbase, cdg);

  idg = BKE_object_defgroup_active_index_get(ob) - 1;
  BKE_object_defgroup_active_index_set(ob, BLI_listbase_count(defbase));
  icdg = BKE_object_defgroup_active_index_get(ob) - 1;

  /* TODO(@ideasman42): we might want to allow only copy selected verts here? */
  vgroup_parray_alloc(static_cast<ID *>(ob->data), &dvert_array, &dvert_tot, false);

  if (dvert_array) {
    for (i = 0; i < dvert_tot; i++) {
      MDeformVert *dv = dvert_array[i];
      dw_org = BKE_defvert_find_index(dv, idg);
      if (dw_org) {
        /* #BKE_defvert_ensure_index re-allocates org so need to store the weight first. */
        const float weight = dw_org->weight;
        dw_cpy = BKE_defvert_ensure_index(dv, icdg);
        dw_cpy->weight = weight;
      }
    }

    MEM_freeN(dvert_array);
  }
}

static bool vgroup_normalize(Object *ob)
{
  MDeformWeight *dw;
  MDeformVert *dv, **dvert_array = nullptr;
  int dvert_tot = 0;
  const int def_nr = BKE_object_defgroup_active_index_get(ob) - 1;

  const bool use_vert_sel = vertex_group_use_vert_sel(ob);

  const ListBase *defbase = BKE_object_defgroup_list(ob);
  if (!BLI_findlink(defbase, def_nr)) {
    return false;
  }

  vgroup_parray_alloc(static_cast<ID *>(ob->data), &dvert_array, &dvert_tot, use_vert_sel);

  if (dvert_array) {
    float weight_max = 0.0f;

    for (int i = 0; i < dvert_tot; i++) {

      /* in case its not selected */
      if (!(dv = dvert_array[i])) {
        continue;
      }

      dw = BKE_defvert_find_index(dv, def_nr);
      if (dw) {
        weight_max = max_ff(dw->weight, weight_max);
      }
    }

    if (weight_max > 0.0f) {
      for (int i = 0; i < dvert_tot; i++) {

        /* in case its not selected */
        if (!(dv = dvert_array[i])) {
          continue;
        }

        dw = BKE_defvert_find_index(dv, def_nr);
        if (dw) {
          dw->weight /= weight_max;

          /* in case of division errors with very low weights */
          CLAMP(dw->weight, 0.0f, 1.0f);
        }
      }
    }

    MEM_freeN(dvert_array);

    return true;
  }

  return false;
}

static void vgroup_levels_subset(Object *ob,
                                 const bool *vgroup_validmap,
                                 const int vgroup_tot,
                                 const int /*subset_count*/,
                                 const float offset,
                                 const float gain)
{
  MDeformWeight *dw;
  MDeformVert *dv, **dvert_array = nullptr;
  int dvert_tot = 0;

  const bool use_vert_sel = vertex_group_use_vert_sel(ob);
  const bool use_mirror = (ob->type == OB_MESH) ?
                              (((Mesh *)ob->data)->symmetry & ME_SYMMETRY_X) != 0 :
                              false;

  vgroup_parray_alloc(static_cast<ID *>(ob->data), &dvert_array, &dvert_tot, use_vert_sel);

  if (dvert_array) {

    for (int i = 0; i < dvert_tot; i++) {
      /* in case its not selected */
      if (!(dv = dvert_array[i])) {
        continue;
      }

      int j = vgroup_tot;
      while (j--) {
        if (vgroup_validmap[j]) {
          dw = BKE_defvert_find_index(dv, j);
          if (dw) {
            dw->weight = gain * (dw->weight + offset);

            CLAMP(dw->weight, 0.0f, 1.0f);
          }
        }
      }
    }

    if (use_mirror && use_vert_sel) {
      vgroup_parray_mirror_sync(ob, dvert_array, dvert_tot, vgroup_validmap, vgroup_tot);
    }

    MEM_freeN(dvert_array);
  }
}

/**
 * Normalize vertex group weights.
 *
 * \param vgroup_validmap: An array of bools indicating which vertex groups
 * should be included (true) and excluded (false) from the normalization
 * process. Must have #vgroup_tot elements.
 *
 * \param lock_active: If true, the active vertex group is temporarily locked
 * during this normalization process.
 *
 * \param soft_lock_active: If true, the active vertex group is treated as "soft
 * locked". See `BKE_defvert_normalize_ex()`'s documentation for details of what
 * that means. Note: because locking is a stronger restriction, if `lock_active`
 * is true then this parameter has no effect.
 *
 * \return True if modification to weights might have happened, false if
 * modification was impossible (e.g. due to all groups being locked.)
 */
static bool vgroup_normalize_all(Object *ob,
                                 const bool *vgroup_validmap,
                                 const int vgroup_tot,
                                 const bool lock_active,
                                 const bool soft_lock_active,
                                 ReportList *reports,
                                 std::optional<int> current_frame = {})
{
  BLI_assert(vgroup_tot == BLI_listbase_count(BKE_object_defgroup_list(ob)));

  const int def_nr = BKE_object_defgroup_active_index_get(ob) - 1;
  const bool use_vert_sel = vertex_group_use_vert_sel(ob);
  BLI_assert(def_nr < vgroup_tot);

  MDeformVert **dvert_array = nullptr;
  int dvert_tot = 0;
  vgroup_parray_alloc(
      static_cast<ID *>(ob->data), &dvert_array, &dvert_tot, use_vert_sel, current_frame);

  if (!dvert_array) {
    return false;
  }

  Span<bool> subset_flags = Span(vgroup_validmap, vgroup_tot);

  MutableSpan<bool> lock_flags = {};
  bool *lock_flags_array = BKE_object_defgroup_lock_flags_get(ob, vgroup_tot);
  if (lock_flags_array) {
    lock_flags = MutableSpan(lock_flags_array, vgroup_tot);
  }
  else if (lock_active) {
    lock_flags_array = MEM_malloc_arrayN<bool>(vgroup_tot, "lock_flags_array");
    std::memset(lock_flags_array, 0, vgroup_tot); /* Clear to false. */
    lock_flags = MutableSpan(lock_flags_array, vgroup_tot);
  }
  if (lock_active) {
    lock_flags[def_nr] = true;
  }

  Vector<bool> soft_lock_flags;
  if (soft_lock_active && !lock_active) {
    soft_lock_flags = Vector(vgroup_tot, false);
    soft_lock_flags[def_nr] = true;
  }

  const bool all_locked = !lock_flags.is_empty() && !lock_flags.contains(false);
  if (all_locked) {
    BKE_report(reports, RPT_ERROR, "All groups are locked");
  }
  else {
    /* Normalize. */
    for (int i = 0; i < dvert_tot; i++) {
      MDeformVert *dv = dvert_array[i];
      /* in case its not selected */
      if (dv) {
        BKE_defvert_normalize_ex(*dv, subset_flags, lock_flags, soft_lock_flags);
      }
    }
  }

  MEM_freeN(dvert_array);
  MEM_SAFE_FREE(lock_flags_array);

  return !all_locked;
}

/**
 * If the currently active vertex group is for a deform bone, normalize all
 * vertex groups that are for deform bones.
 *
 * \param soft_lock_active: If true, the active vertex group is treated as "soft
 * locked". See `BKE_defvert_normalize_ex()`'s documentation for details of what
 * that means.
 */
static void vgroup_normalize_all_deform_if_active_is_deform(Object *ob,
                                                            const bool soft_lock_active,
                                                            ReportList *reports,
                                                            std::optional<int> current_frame = {})
{
  const int defgroup_tot = BKE_object_defgroup_count(ob);
  bool *defgroup_validmap = BKE_object_defgroup_validmap_get(ob, defgroup_tot);
  const int def_nr = BKE_object_defgroup_active_index_get(ob) - 1;

  /* Only auto-normalize if the active group is bone-deforming. */
  if (defgroup_validmap[def_nr] == true) {
    int subset_count, vgroup_tot;
    const bool *vgroup_validmap = BKE_object_defgroup_subset_from_select_type(
        ob, WT_VGROUP_BONE_DEFORM, &vgroup_tot, &subset_count);

    vgroup_normalize_all(
        ob, vgroup_validmap, vgroup_tot, false, soft_lock_active, reports, current_frame);
    MEM_SAFE_FREE(vgroup_validmap);
  }

  MEM_SAFE_FREE(defgroup_validmap);
}

enum {
  VGROUP_TOGGLE,
  VGROUP_LOCK,
  VGROUP_UNLOCK,
  VGROUP_INVERT,
};

static const EnumPropertyItem vgroup_lock_actions[] = {
    {VGROUP_TOGGLE,
     "TOGGLE",
     0,
     "Toggle",
     "Unlock all vertex groups if there is at least one locked group, lock all in other case"},
    {VGROUP_LOCK, "LOCK", 0, "Lock", "Lock all vertex groups"},
    {VGROUP_UNLOCK, "UNLOCK", 0, "Unlock", "Unlock all vertex groups"},
    {VGROUP_INVERT, "INVERT", 0, "Invert", "Invert the lock state of all vertex groups"},
    {0, nullptr, 0, nullptr, nullptr},
};

enum {
  VGROUP_MASK_ALL,
  VGROUP_MASK_SELECTED,
  VGROUP_MASK_UNSELECTED,
  VGROUP_MASK_INVERT_UNSELECTED,
};

static const EnumPropertyItem vgroup_lock_mask[] = {
    {VGROUP_MASK_ALL, "ALL", 0, "All", "Apply action to all vertex groups"},
    {VGROUP_MASK_SELECTED, "SELECTED", 0, "Selected", "Apply to selected vertex groups"},
    {VGROUP_MASK_UNSELECTED, "UNSELECTED", 0, "Unselected", "Apply to unselected vertex groups"},
    {VGROUP_MASK_INVERT_UNSELECTED,
     "INVERT_UNSELECTED",
     0,
     "Invert Unselected",
     "Apply the opposite of Lock/Unlock to unselected vertex groups"},
    {0, nullptr, 0, nullptr, nullptr},
};

static bool *vgroup_selected_get(Object *ob)
{
  int sel_count = 0, defbase_tot = BKE_object_defgroup_count(ob);
  bool *mask;

  if (ob->mode & OB_MODE_WEIGHT_PAINT) {
    mask = BKE_object_defgroup_selected_get(ob, defbase_tot, &sel_count);

    /* Mirror the selection if X Mirror is enabled. */
    Mesh *mesh = BKE_mesh_from_object(ob);

    if (mesh && ME_USING_MIRROR_X_VERTEX_GROUPS(mesh)) {
      BKE_object_defgroup_mirror_selection(ob, defbase_tot, mask, mask, &sel_count);
    }
  }
  else {
    mask = MEM_calloc_arrayN<bool>(defbase_tot, __func__);
  }

  const int actdef = BKE_object_defgroup_active_index_get(ob);
  if (sel_count == 0 && actdef >= 1 && actdef <= defbase_tot) {
    mask[actdef - 1] = true;
  }

  return mask;
}

static void vgroup_lock_all(Object *ob, int action, int mask)
{
  bDeformGroup *dg;
  bool *selected = nullptr;
  int i;

  if (mask != VGROUP_MASK_ALL) {
    selected = vgroup_selected_get(ob);
  }
  const ListBase *defbase = BKE_object_defgroup_list(ob);

  if (action == VGROUP_TOGGLE) {
    action = VGROUP_LOCK;

    for (dg = static_cast<bDeformGroup *>(defbase->first), i = 0; dg; dg = dg->next, i++) {
      switch (mask) {
        case VGROUP_MASK_INVERT_UNSELECTED:
        case VGROUP_MASK_SELECTED:
          if (!selected[i]) {
            continue;
          }
          break;
        case VGROUP_MASK_UNSELECTED:
          if (selected[i]) {
            continue;
          }
          break;
        default:
          break;
      }

      if (dg->flag & DG_LOCK_WEIGHT) {
        action = VGROUP_UNLOCK;
        break;
      }
    }
  }

  for (dg = static_cast<bDeformGroup *>(defbase->first), i = 0; dg; dg = dg->next, i++) {
    switch (mask) {
      case VGROUP_MASK_SELECTED:
        if (!selected[i]) {
          continue;
        }
        break;
      case VGROUP_MASK_UNSELECTED:
        if (selected[i]) {
          continue;
        }
        break;
      default:
        break;
    }

    switch (action) {
      case VGROUP_LOCK:
        dg->flag |= DG_LOCK_WEIGHT;
        break;
      case VGROUP_UNLOCK:
        dg->flag &= ~DG_LOCK_WEIGHT;
        break;
      case VGROUP_INVERT:
        dg->flag ^= DG_LOCK_WEIGHT;
        break;
    }

    if (mask == VGROUP_MASK_INVERT_UNSELECTED && !selected[i]) {
      dg->flag ^= DG_LOCK_WEIGHT;
    }
  }

  if (selected) {
    MEM_freeN(selected);
  }
}

static void vgroup_invert_subset(Object *ob,
                                 const bool *vgroup_validmap,
                                 const int vgroup_tot,
                                 const int /*subset_count*/,
                                 const bool auto_assign,
                                 const bool auto_remove)
{
  MDeformWeight *dw;
  MDeformVert *dv, **dvert_array = nullptr;
  int dvert_tot = 0;
  const bool use_vert_sel = vertex_group_use_vert_sel(ob);
  const bool use_mirror = (ob->type == OB_MESH) ?
                              (((Mesh *)ob->data)->symmetry & ME_SYMMETRY_X) != 0 :
                              false;

  vgroup_parray_alloc(static_cast<ID *>(ob->data), &dvert_array, &dvert_tot, use_vert_sel);

  if (dvert_array) {
    for (int i = 0; i < dvert_tot; i++) {
      /* in case its not selected */
      if (!(dv = dvert_array[i])) {
        continue;
      }

      int j = vgroup_tot;
      while (j--) {

        if (vgroup_validmap[j]) {
          if (auto_assign) {
            dw = BKE_defvert_ensure_index(dv, j);
          }
          else {
            dw = BKE_defvert_find_index(dv, j);
          }

          if (dw) {
            dw->weight = 1.0f - dw->weight;
            CLAMP(dw->weight, 0.0f, 1.0f);
          }
        }
      }
    }

    if (use_mirror && use_vert_sel) {
      vgroup_parray_mirror_sync(ob, dvert_array, dvert_tot, vgroup_validmap, vgroup_tot);
    }

    if (auto_remove) {
      vgroup_parray_remove_zero(dvert_array, dvert_tot, vgroup_validmap, vgroup_tot, 0.0f, false);
    }

    MEM_freeN(dvert_array);
  }
}

static void vgroup_smooth_subset(Object *ob,
                                 const bool *vgroup_validmap,
                                 const int vgroup_tot,
                                 const int subset_count,
                                 const float fac,
                                 const int repeat,
                                 const float fac_expand)
{
  /* Caller must check, while it's not an error it will do nothing. */
  BLI_assert(vgroup_tot > 0 && subset_count > 0);

  const float ifac = 1.0f - fac;
  MDeformVert **dvert_array = nullptr;
  int dvert_tot = 0;
  Array<int, 32> vgroup_subset_map(subset_count);
  Array<float, 32> vgroup_subset_weights(subset_count);
  const bool use_mirror = (ob->type == OB_MESH) ?
                              (((Mesh *)ob->data)->symmetry & ME_SYMMETRY_X) != 0 :
                              false;
  const bool use_select = vertex_group_use_vert_sel(ob);
  const bool use_hide = use_select;

  const int expand_sign = signum_i(fac_expand);
  const float expand = fabsf(fac_expand);
  const float iexpand = 1.0f - expand;

  BMEditMesh *em = BKE_editmesh_from_object(ob);
  BMesh *bm = em ? em->bm : nullptr;
  Mesh *mesh = em ? nullptr : static_cast<Mesh *>(ob->data);

  float *weight_accum_prev;
  float *weight_accum_curr;

  uint subset_index;

  /* vertex indices that will be smoothed, (only to avoid iterating over verts that do nothing) */
  uint *verts_used;
  STACK_DECLARE(verts_used);

  BKE_object_defgroup_subset_to_index_array(vgroup_validmap, vgroup_tot, vgroup_subset_map.data());
  vgroup_parray_alloc(static_cast<ID *>(ob->data), &dvert_array, &dvert_tot, false);
  vgroup_subset_weights.fill(0.0f);

  Array<int> vert_to_edge_offsets;
  Array<int> vert_to_edge_indices;
  GroupedSpan<int> emap;
  if (bm) {
    BM_mesh_elem_table_ensure(bm, BM_VERT);
    BM_mesh_elem_index_ensure(bm, BM_VERT);
  }
  else {
    emap = bke::mesh::build_vert_to_edge_map(
        mesh->edges(), mesh->verts_num, vert_to_edge_offsets, vert_to_edge_indices);
  }

  weight_accum_prev = MEM_malloc_arrayN<float>(dvert_tot, __func__);
  weight_accum_curr = MEM_malloc_arrayN<float>(dvert_tot, __func__);

  verts_used = MEM_malloc_arrayN<uint>(dvert_tot, __func__);
  STACK_INIT(verts_used, dvert_tot);

#define IS_BM_VERT_READ(v) (use_hide ? (BM_elem_flag_test(v, BM_ELEM_HIDDEN) == 0) : true)
#define IS_BM_VERT_WRITE(v) (use_select ? (BM_elem_flag_test(v, BM_ELEM_SELECT) != 0) : true)

  VArray<bool> hide_vert;
  if (mesh && use_hide) {
    hide_vert = *mesh->attributes().lookup_or_default<bool>(
        ".hide_vert", bke::AttrDomain::Point, false);
  }
  else {
    hide_vert = VArray<bool>::from_single(false, dvert_tot);
  }

  VArray<bool> select_vert;
  if (mesh && use_select) {
    select_vert = *mesh->attributes().lookup_or_default<bool>(
        ".select_vert", bke::AttrDomain::Point, false);
  }
  else {
    select_vert = VArray<bool>::from_single(true, dvert_tot);
  }

  /* initialize used verts */
  if (bm) {
    for (int i = 0; i < dvert_tot; i++) {
      BMVert *v = BM_vert_at_index(bm, i);
      if (IS_BM_VERT_WRITE(v)) {
        BMIter eiter;
        BMEdge *e;
        BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
          BMVert *v_other = BM_edge_other_vert(e, v);
          if (IS_BM_VERT_READ(v_other)) {
            STACK_PUSH(verts_used, i);
            break;
          }
        }
      }
    }
  }
  else {
    const Span<int2> edges = mesh->edges();
    for (int i = 0; i < dvert_tot; i++) {
      if (select_vert[i]) {
        for (int j = 0; j < emap[i].size(); j++) {
          const int2 &edge = edges[emap[i][j]];
          const int i_other = (edge[0] == i) ? edge[1] : edge[0];
          if (!hide_vert[i_other]) {
            STACK_PUSH(verts_used, i);
            break;
          }
        }
      }
    }
  }

  for (subset_index = 0; subset_index < subset_count; subset_index++) {
    const int def_nr = vgroup_subset_map[subset_index];
    int iter;

    vgroup_parray_to_weight_array(
        (const MDeformVert **)dvert_array, dvert_tot, weight_accum_prev, def_nr);
    memcpy(weight_accum_curr, weight_accum_prev, sizeof(*weight_accum_curr) * dvert_tot);

    for (iter = 0; iter < repeat; iter++) {
      uint *vi_step, *vi_end = verts_used + STACK_SIZE(verts_used);

      /* avoid looping over all verts */
      // for (i = 0; i < dvert_tot; i++)
      for (vi_step = verts_used; vi_step != vi_end; vi_step++) {
        const uint i = *vi_step;
        float weight_tot = 0.0f;
        float weight = 0.0f;

#define WEIGHT_ACCUMULATE \
  { \
    float weight_other = weight_accum_prev[i_other]; \
    float tot_factor = 1.0f; \
    if (expand_sign == 1) { /* expand */ \
      if (weight_other < weight_accum_prev[i]) { \
        weight_other = (weight_accum_prev[i] * expand) + (weight_other * iexpand); \
        tot_factor = iexpand; \
      } \
    } \
    else if (expand_sign == -1) { /* contract */ \
      if (weight_other > weight_accum_prev[i]) { \
        weight_other = (weight_accum_prev[i] * expand) + (weight_other * iexpand); \
        tot_factor = iexpand; \
      } \
    } \
    weight += tot_factor * weight_other; \
    weight_tot += tot_factor; \
  } \
  ((void)0)

        if (bm) {
          BMVert *v = BM_vert_at_index(bm, i);
          BMIter eiter;
          BMEdge *e;

          /* checked already */
          BLI_assert(IS_BM_VERT_WRITE(v));

          BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
            BMVert *v_other = BM_edge_other_vert(e, v);
            if (IS_BM_VERT_READ(v_other)) {
              const int i_other = BM_elem_index_get(v_other);

              WEIGHT_ACCUMULATE;
            }
          }
        }
        else {
          int j;
          const Span<int2> edges = mesh->edges();

          /* checked already */
          BLI_assert(select_vert[i]);

          for (j = 0; j < emap[i].size(); j++) {
            const int2 &edge = edges[emap[i][j]];
            const int i_other = (edge[0] == i ? edge[1] : edge[0]);
            if (!hide_vert[i_other]) {
              WEIGHT_ACCUMULATE;
            }
          }
        }

#undef WEIGHT_ACCUMULATE

        if (weight_tot != 0.0f) {
          weight /= weight_tot;
          weight = (weight_accum_prev[i] * ifac) + (weight * fac);

          /* should be within range, just clamp because of float precision */
          CLAMP(weight, 0.0f, 1.0f);
          weight_accum_curr[i] = weight;
        }
      }

      std::swap(weight_accum_curr, weight_accum_prev);
    }

    vgroup_parray_from_weight_array(dvert_array, dvert_tot, weight_accum_prev, def_nr, true);
  }

#undef IS_BM_VERT_READ
#undef IS_BM_VERT_WRITE

  MEM_freeN(weight_accum_curr);
  MEM_freeN(weight_accum_prev);
  MEM_freeN(verts_used);

  if (dvert_array) {
    MEM_freeN(dvert_array);
  }

  /* not so efficient to get 'dvert_array' again just so unselected verts are nullptr'd */
  if (use_mirror) {
    vgroup_parray_alloc(static_cast<ID *>(ob->data), &dvert_array, &dvert_tot, true);
    vgroup_parray_mirror_sync(ob, dvert_array, dvert_tot, vgroup_validmap, vgroup_tot);
    if (dvert_array) {
      MEM_freeN(dvert_array);
    }
  }
}

static int inv_cmp_mdef_vert_weights(const void *a1, const void *a2)
{
  /* #qsort sorts in ascending order. We want descending order to save a #memcpy
   * so this compare function is inverted from the standard greater than comparison #qsort needs.
   * A normal compare function is called with two pointer arguments and should return an integer
   * less than, equal to, or greater than zero corresponding to whether its first argument is
   * considered less than, equal to, or greater than its second argument.
   * This does the opposite. */
  const MDeformWeight *dw1 = static_cast<const MDeformWeight *>(a1);
  const MDeformWeight *dw2 = static_cast<const MDeformWeight *>(a2);

  if (dw1->weight < dw2->weight) {
    return 1;
  }
  if (dw1->weight > dw2->weight) {
    return -1;
  }

  /* Compare address for stable sort algorithm. */
  if (&dw1 < &dw2) {
    return 1;
  }
  if (&dw1 > &dw2) {
    return -1;
  }
  return 0;
}

/* Used for limiting the number of influencing bones per vertex when exporting
 * skinned meshes.  if all_deform_weights is True, limit all deform modifiers
 * to max_weights regardless of type, otherwise,
 * only limit the number of influencing bones per vertex. */
static int vgroup_limit_total_subset(Object *ob,
                                     const bool *vgroup_validmap,
                                     const int vgroup_tot,
                                     const int subset_count,
                                     const int max_weights)
{
  MDeformVert *dv, **dvert_array = nullptr;
  int i, dvert_tot = 0;
  const bool use_vert_sel = vertex_group_use_vert_sel(ob);
  int remove_tot = 0;

  vgroup_parray_alloc(static_cast<ID *>(ob->data), &dvert_array, &dvert_tot, use_vert_sel);

  if (dvert_array) {
    int num_to_drop = 0;

    for (i = 0; i < dvert_tot; i++) {

      MDeformWeight *dw_temp;
      int bone_count = 0, non_bone_count = 0;
      int j;

      /* in case its not selected */
      if (!(dv = dvert_array[i])) {
        continue;
      }

      num_to_drop = subset_count - max_weights;

      /* first check if we even need to test further */
      if (num_to_drop > 0) {
        /* re-pack dw array so that non-bone weights are first, bone-weighted verts at end
         * sort the tail, then copy only the truncated array back to dv->dw */
        dw_temp = MEM_malloc_arrayN<MDeformWeight>(dv->totweight, __func__);
        bone_count = 0;
        non_bone_count = 0;
        for (j = 0; j < dv->totweight; j++) {
          if (LIKELY(dv->dw[j].def_nr < vgroup_tot) && vgroup_validmap[dv->dw[j].def_nr]) {
            dw_temp[dv->totweight - 1 - bone_count] = dv->dw[j];
            bone_count += 1;
          }
          else {
            dw_temp[non_bone_count] = dv->dw[j];
            non_bone_count += 1;
          }
        }
        BLI_assert(bone_count + non_bone_count == dv->totweight);
        num_to_drop = bone_count - max_weights;
        if (num_to_drop > 0) {
          qsort(&dw_temp[non_bone_count],
                bone_count,
                sizeof(MDeformWeight),
                inv_cmp_mdef_vert_weights);
          dv->totweight -= num_to_drop;
          /* Do we want to clean/normalize here? */
          MEM_freeN(dv->dw);
          dv->dw = static_cast<MDeformWeight *>(
              MEM_reallocN(dw_temp, sizeof(MDeformWeight) * dv->totweight));
          remove_tot += num_to_drop;
        }
        else {
          MEM_freeN(dw_temp);
        }
      }
    }
    MEM_freeN(dvert_array);
  }

  return remove_tot;
}

static void vgroup_clean_subset(Object *ob,
                                const bool *vgroup_validmap,
                                const int vgroup_tot,
                                const int /*subset_count*/,
                                const float epsilon,
                                const bool keep_single)
{
  MDeformVert **dvert_array = nullptr;
  int dvert_tot = 0;
  const bool use_vert_sel = vertex_group_use_vert_sel(ob);
  const bool use_mirror = (ob->type == OB_MESH) ?
                              (((Mesh *)ob->data)->symmetry & ME_SYMMETRY_X) != 0 :
                              false;

  vgroup_parray_alloc(static_cast<ID *>(ob->data), &dvert_array, &dvert_tot, use_vert_sel);

  if (dvert_array) {
    if (use_mirror && use_vert_sel) {
      /* correct behavior in this case isn't well defined
       * for now assume both sides are mirrored correctly,
       * so cleaning one side also cleans the other */
      vgroup_parray_mirror_assign(ob, dvert_array, dvert_tot);
    }

    vgroup_parray_remove_zero(
        dvert_array, dvert_tot, vgroup_validmap, vgroup_tot, epsilon, keep_single);

    MEM_freeN(dvert_array);
  }
}

static void vgroup_quantize_subset(Object *ob,
                                   const bool *vgroup_validmap,
                                   const int vgroup_tot,
                                   const int /*subset_count*/,
                                   const int steps)
{
  MDeformVert **dvert_array = nullptr;
  int dvert_tot = 0;
  const bool use_vert_sel = vertex_group_use_vert_sel(ob);
  const bool use_mirror = (ob->type == OB_MESH) ?
                              (((Mesh *)ob->data)->symmetry & ME_SYMMETRY_X) != 0 :
                              false;
  vgroup_parray_alloc(static_cast<ID *>(ob->data), &dvert_array, &dvert_tot, use_vert_sel);

  if (dvert_array) {
    const float steps_fl = steps;
    MDeformVert *dv;

    if (use_mirror && use_vert_sel) {
      vgroup_parray_mirror_assign(ob, dvert_array, dvert_tot);
    }

    for (int i = 0; i < dvert_tot; i++) {
      MDeformWeight *dw;

      /* in case its not selected */
      if (!(dv = dvert_array[i])) {
        continue;
      }

      int j;
      for (j = 0, dw = dv->dw; j < dv->totweight; j++, dw++) {
        if ((dw->def_nr < vgroup_tot) && vgroup_validmap[dw->def_nr]) {
          dw->weight = floorf((dw->weight * steps_fl) + 0.5f) / steps_fl;
          CLAMP(dw->weight, 0.0f, 1.0f);
        }
      }
    }

    MEM_freeN(dvert_array);
  }
}

static void dvert_mirror_op(MDeformVert *dvert,
                            MDeformVert *dvert_mirr,
                            const char sel,
                            const char sel_mirr,
                            const int *flip_map,
                            const int flip_map_len,
                            const bool mirror_weights,
                            const bool flip_vgroups,
                            const bool all_vgroups,
                            const int act_vgroup)
{
  BLI_assert(sel || sel_mirr);

  if (sel_mirr && sel) {
    /* swap */
    if (mirror_weights) {
      if (all_vgroups) {
        std::swap(*dvert, *dvert_mirr);
      }
      else {
        MDeformWeight *dw = BKE_defvert_find_index(dvert, act_vgroup);
        MDeformWeight *dw_mirr = BKE_defvert_find_index(dvert_mirr, act_vgroup);

        if (dw && dw_mirr) {
          std::swap(dw->weight, dw_mirr->weight);
        }
        else if (dw) {
          dw_mirr = BKE_defvert_ensure_index(dvert_mirr, act_vgroup);
          dw_mirr->weight = dw->weight;
          BKE_defvert_remove_group(dvert, dw);
        }
        else if (dw_mirr) {
          dw = BKE_defvert_ensure_index(dvert, act_vgroup);
          dw->weight = dw_mirr->weight;
          BKE_defvert_remove_group(dvert_mirr, dw_mirr);
        }
      }
    }

    if (flip_vgroups) {
      BKE_defvert_flip(dvert, flip_map, flip_map_len);
      BKE_defvert_flip(dvert_mirr, flip_map, flip_map_len);
    }
  }
  else {
    /* dvert should always be the target, only swaps pointer */
    if (sel_mirr) {
      std::swap(dvert, dvert_mirr);
    }

    if (mirror_weights) {
      if (all_vgroups) {
        BKE_defvert_copy(dvert, dvert_mirr);
      }
      else {
        BKE_defvert_copy_index(dvert, act_vgroup, dvert_mirr, act_vgroup);
      }
    }

    /* flip map already modified for 'all_vgroups' */
    if (flip_vgroups) {
      BKE_defvert_flip(dvert, flip_map, flip_map_len);
    }
  }
}

void vgroup_mirror(Object *ob,
                   const bool mirror_weights,
                   const bool flip_vgroups,
                   const bool all_vgroups,
                   const bool use_topology,
                   int *r_totmirr,
                   int *r_totfail)
{
  /* TODO: vgroup locking. */

  const int def_nr = BKE_object_defgroup_active_index_get(ob) - 1;
  int totmirr = 0, totfail = 0;

  *r_totmirr = *r_totfail = 0;

  const ListBase *defbase = BKE_object_defgroup_list(ob);

  if ((mirror_weights == false && flip_vgroups == false) ||
      (BLI_findlink(defbase, def_nr) == nullptr))
  {
    return;
  }

  int *flip_map = nullptr;
  int flip_map_len;
  if (flip_vgroups) {
    flip_map = all_vgroups ? BKE_object_defgroup_flip_map(ob, false, &flip_map_len) :
                             BKE_object_defgroup_flip_map_single(ob, false, def_nr, &flip_map_len);

    BLI_assert(flip_map != nullptr);

    if (flip_map == nullptr) {
      /* something went wrong!, possibly no groups */
      return;
    }
  }
  else {
    flip_map = nullptr;
    flip_map_len = 0;
  }

  /* only the active group */
  if (ob->type == OB_MESH) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);

    if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
      const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);
      BMIter iter;

      if (cd_dvert_offset == -1) {
        goto cleanup;
      }

      EDBM_verts_mirror_cache_begin(em, 0, true, false, false, use_topology);

      BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT, BM_ELEM_TAG, false);

      /* Go through the list of edit-vertices and assign them. */
      BMVert *eve, *eve_mirr;
      BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
        if (!BM_elem_flag_test(eve, BM_ELEM_TAG)) {
          if ((eve_mirr = EDBM_verts_mirror_get(em, eve))) {
            if (eve_mirr != eve) {
              if (!BM_elem_flag_test(eve_mirr, BM_ELEM_TAG)) {
                const bool sel = BM_elem_flag_test(eve, BM_ELEM_SELECT);
                const bool sel_mirr = BM_elem_flag_test(eve_mirr, BM_ELEM_SELECT);

                if ((sel || sel_mirr) && (eve != eve_mirr)) {
                  dvert_mirror_op(
                      static_cast<MDeformVert *>(BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset)),
                      static_cast<MDeformVert *>(BM_ELEM_CD_GET_VOID_P(eve_mirr, cd_dvert_offset)),
                      sel,
                      sel_mirr,
                      flip_map,
                      flip_map_len,
                      mirror_weights,
                      flip_vgroups,
                      all_vgroups,
                      def_nr);
                  totmirr++;
                }

                /* don't use these again */
                BM_elem_flag_enable(eve, BM_ELEM_TAG);
                BM_elem_flag_enable(eve_mirr, BM_ELEM_TAG);
              }
            }
          }
          else {
            totfail++;
          }
        }
      }
      EDBM_verts_mirror_cache_end(em);
    }
    else {
      /* object mode / weight paint */
      const bool use_sel = (mesh->editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) !=
                           0;
      if (mesh->deform_verts().is_empty()) {
        goto cleanup;
      }

      BLI_bitmap *vert_tag = BLI_BITMAP_NEW(mesh->verts_num, __func__);
      MutableSpan<MDeformVert> dverts = mesh->deform_verts_for_write();
      const bke::AttributeAccessor attributes = mesh->attributes();
      const VArray<bool> select_vert = *attributes.lookup_or_default<bool>(
          ".select_vert", bke::AttrDomain::Point, false);

      for (int vidx = 0; vidx < mesh->verts_num; vidx++) {
        if (!BLI_BITMAP_TEST(vert_tag, vidx)) {
          int vidx_mirr;
          if ((vidx_mirr = mesh_get_x_mirror_vert(ob, nullptr, vidx, use_topology)) != -1) {
            if (vidx != vidx_mirr) {
              if (!BLI_BITMAP_TEST(vert_tag, vidx_mirr)) {
                const bool sel = use_sel ? select_vert[vidx] : true;
                const bool sel_mirr = use_sel ? select_vert[vidx_mirr] : true;

                if (sel || sel_mirr) {
                  dvert_mirror_op(&dverts[vidx],
                                  &dverts[vidx_mirr],
                                  sel,
                                  sel_mirr,
                                  flip_map,
                                  flip_map_len,
                                  mirror_weights,
                                  flip_vgroups,
                                  all_vgroups,
                                  def_nr);
                  totmirr++;
                }

                BLI_BITMAP_ENABLE(vert_tag, vidx);
                BLI_BITMAP_ENABLE(vert_tag, vidx_mirr);
              }
            }
          }
          else {
            totfail++;
          }
        }
      }

      MEM_freeN(vert_tag);
    }
  }
  else if (ob->type == OB_LATTICE) {
    Lattice *lt = vgroup_edit_lattice(ob);
    /* half but found up odd value */

    if (lt->pntsu == 1 || lt->dvert == nullptr) {
      goto cleanup;
    }

    /* unlike editmesh we know that by only looping over the first half of
     * the 'u' indices it will cover all points except the middle which is
     * ok in this case */
    int pntsu_half = lt->pntsu / 2;

    for (int w = 0; w < lt->pntsw; w++) {
      for (int v = 0; v < lt->pntsv; v++) {
        for (int u = 0; u < pntsu_half; u++) {
          int u_inv = (lt->pntsu - 1) - u;
          if (u != u_inv) {
            const int i1 = BKE_lattice_index_from_uvw(lt, u, v, w);
            const int i2 = BKE_lattice_index_from_uvw(lt, u_inv, v, w);

            const BPoint *bp = &lt->def[i1];
            const BPoint *bp_mirr = &lt->def[i2];

            const bool sel = bp->f1 & SELECT;
            const bool sel_mirr = bp_mirr->f1 & SELECT;

            if (sel || sel_mirr) {
              dvert_mirror_op(&lt->dvert[i1],
                              &lt->dvert[i2],
                              sel,
                              sel_mirr,
                              flip_map,
                              flip_map_len,
                              mirror_weights,
                              flip_vgroups,
                              all_vgroups,
                              def_nr);
              totmirr++;
            }
          }
        }
      }
    }
  }

  /* disabled, confusing when you have an active pose bone */
#if 0
  /* flip active group index */
  if (flip_vgroups && flip_map[def_nr] >= 0) {
    ob->actdef = flip_map[def_nr] + 1;
  }
#endif

cleanup:
  *r_totmirr = totmirr;
  *r_totfail = totfail;

  if (flip_map) {
    MEM_freeN(flip_map);
  }

#undef VGROUP_MIRR_OP
}

static void vgroup_delete_active(Object *ob)
{
  const ListBase *defbase = BKE_object_defgroup_list(ob);
  bDeformGroup *dg = static_cast<bDeformGroup *>(
      BLI_findlink(defbase, BKE_object_defgroup_active_index_get(ob) - 1));
  if (!dg) {
    return;
  }

  BKE_object_defgroup_remove(ob, dg);
}

/* only in editmode */
static void vgroup_assign_verts(Object *ob, Scene &scene, const float weight)
{
  const int def_nr = BKE_object_defgroup_active_index_get(ob) - 1;

  const ListBase *defbase = BKE_object_defgroup_list(ob);
  if (!BLI_findlink(defbase, def_nr)) {
    return;
  }

  if (ob->type == OB_MESH) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);

    if (mesh->runtime->edit_mesh) {
      BMEditMesh *em = mesh->runtime->edit_mesh.get();
      int cd_dvert_offset;

      BMIter iter;
      BMVert *eve;

      if (!CustomData_has_layer(&em->bm->vdata, CD_MDEFORMVERT)) {
        BM_data_layer_add(em->bm, &em->bm->vdata, CD_MDEFORMVERT);
      }

      cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

      /* Go through the list of edit-vertices and assign them. */
      BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
        if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
          MDeformVert *dv;
          MDeformWeight *dw;
          dv = static_cast<MDeformVert *>(
              BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset)); /* can be nullptr */
          dw = BKE_defvert_ensure_index(dv, def_nr);
          if (dw) {
            dw->weight = weight;
          }
        }
      }
    }
    else {
      const bke::AttributeAccessor attributes = mesh->attributes();
      const VArray<bool> select_vert = *attributes.lookup_or_default<bool>(
          ".select_vert", bke::AttrDomain::Point, false);

      MutableSpan<MDeformVert> dverts = mesh->deform_verts_for_write();

      for (int i = 0; i < mesh->verts_num; i++) {
        if (select_vert[i]) {
          MDeformWeight *dw;
          dw = BKE_defvert_ensure_index(&dverts[i], def_nr);
          if (dw) {
            dw->weight = weight;
          }
        }
      }
    }
  }
  else if (ob->type == OB_LATTICE) {
    Lattice *lt = vgroup_edit_lattice(ob);
    MDeformVert *dv;
    BPoint *bp;
    int a, tot;

    if (lt->dvert == nullptr) {
      BKE_object_defgroup_data_create(&lt->id);
    }

    dv = lt->dvert;

    tot = lt->pntsu * lt->pntsv * lt->pntsw;
    for (a = 0, bp = lt->def; a < tot; a++, bp++, dv++) {
      if (bp->f1 & SELECT) {
        MDeformWeight *dw;

        dw = BKE_defvert_ensure_index(dv, def_nr);
        if (dw) {
          dw->weight = weight;
        }
      }
    }
  }
  else if (ob->type == OB_GREASE_PENCIL) {
    GreasePencil *grease_pencil = static_cast<GreasePencil *>(ob->data);
    const bDeformGroup *defgroup = static_cast<const bDeformGroup *>(
        BLI_findlink(BKE_object_defgroup_list(ob), def_nr));

    {
      using namespace ed::greasepencil;
      Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, *grease_pencil);
      for (MutableDrawingInfo info : drawings) {
        bke::greasepencil::assign_to_vertex_group(info.drawing, defgroup->name, weight);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shared Operator Poll Functions
 * \{ */

static bool vertex_group_supported_poll_ex(bContext *C, const Object *ob)
{
  if (!ED_operator_object_active_local_editable_ex(C, ob)) {
    CTX_wm_operator_poll_msg_set(C, "No active editable object");
    return false;
  }

  if (!OB_TYPE_SUPPORT_VGROUP(ob->type)) {
    CTX_wm_operator_poll_msg_set(C, "Object type does not support vertex groups");
    return false;
  }

  /* Data checks. */
  const ID *data = static_cast<const ID *>(ob->data);
  if (data == nullptr || !ID_IS_EDITABLE(data) || ID_IS_OVERRIDE_LIBRARY(data)) {
    CTX_wm_operator_poll_msg_set(C, "Object type \"%s\" does not have editable data");
    return false;
  }

  return true;
}

static bool vertex_group_supported_poll(bContext *C)
{
  Object *ob = context_object(C);
  return vertex_group_supported_poll_ex(C, ob);
}

static bool vertex_group_poll_ex(bContext *C, Object *ob)
{
  if (!vertex_group_supported_poll_ex(C, ob)) {
    return false;
  }

  const ListBase *defbase = BKE_object_defgroup_list(ob);
  if (BLI_listbase_is_empty(defbase)) {
    CTX_wm_operator_poll_msg_set(C, "Object has no vertex groups");
    return false;
  }

  return true;
}

static bool vertex_group_poll(bContext *C)
{
  Object *ob = context_object(C);
  return vertex_group_poll_ex(C, ob);
}

static bool UNUSED_FUNCTION(vertex_group_poll_edit)(bContext *C)
{
  Object *ob = context_object(C);

  if (!vertex_group_supported_poll_ex(C, ob)) {
    return false;
  }

  return BKE_object_is_in_editmode_vgroup(ob);
}

/* editmode _or_ weight paint vertex sel */
static bool vertex_group_vert_poll_ex(bContext *C,
                                      const bool needs_select,
                                      const short ob_type_flag)
{
  Object *ob = context_object(C);

  if (!vertex_group_supported_poll_ex(C, ob)) {
    return false;
  }

  if (ob_type_flag && ((1 << ob->type) & ob_type_flag) == 0) {
    return false;
  }

  if (BKE_object_is_in_editmode_vgroup(ob)) {
    return true;
  }
  if (ob->mode & OB_MODE_WEIGHT_PAINT) {
    if (needs_select) {
      if (BKE_object_is_in_wpaint_select_vert(ob)) {
        return true;
      }
      CTX_wm_operator_poll_msg_set(C, "Vertex select needs to be enabled in weight paint mode");
      return false;
    }
    return true;
  }
  return false;
}

#if 0
static bool vertex_group_vert_poll(bContext *C)
{
  return vertex_group_vert_poll_ex(C, false, 0);
}
#endif

static bool vertex_group_mesh_vert_poll(bContext *C)
{
  return vertex_group_vert_poll_ex(C, false, (1 << OB_MESH));
}

static bool vertex_group_vert_select_poll(bContext *C)
{
  return vertex_group_vert_poll_ex(C, true, 0);
}

#if 0
static bool vertex_group_mesh_vert_select_poll(bContext *C)
{
  return vertex_group_vert_poll_ex(C, true, (1 << OB_MESH));
}
#endif

/* editmode _or_ weight paint vertex sel and active group unlocked */
static bool vertex_group_vert_select_unlocked_poll(bContext *C)
{
  Object *ob = context_object(C);

  if (!vertex_group_supported_poll_ex(C, ob)) {
    return false;
  }

  if (!(BKE_object_is_in_editmode_vgroup(ob) || BKE_object_is_in_wpaint_select_vert(ob))) {
    return false;
  }

  const int def_nr = BKE_object_defgroup_active_index_get(ob);
  if (def_nr != 0) {
    const ListBase *defbase = BKE_object_defgroup_list(ob);
    const bDeformGroup *dg = static_cast<const bDeformGroup *>(BLI_findlink(defbase, def_nr - 1));
    if (dg && (dg->flag & DG_LOCK_WEIGHT)) {
      CTX_wm_operator_poll_msg_set(C, "The active vertex group is locked");
      return false;
    }
  }
  return true;
}

static bool vertex_group_vert_select_mesh_poll(bContext *C)
{
  Object *ob = context_object(C);

  if (!vertex_group_supported_poll_ex(C, ob)) {
    return false;
  }

  /* only difference to #vertex_group_vert_select_poll */
  if (ob->type != OB_MESH) {
    return false;
  }

  return (BKE_object_is_in_editmode_vgroup(ob) || BKE_object_is_in_wpaint_select_vert(ob));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Add Operator
 * \{ */

static wmOperatorStatus vertex_group_add_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = context_object(C);

  BKE_object_defgroup_add(ob);
  DEG_relations_tag_update(CTX_data_main(C));
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_VERTEX_GROUP, ob->data);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Vertex Group";
  ot->idname = "OBJECT_OT_vertex_group_add";
  ot->description = "Add a new vertex group to the active object";

  /* API callbacks. */
  ot->poll = vertex_group_supported_poll;
  ot->exec = vertex_group_add_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Remove Operator
 * \{ */

static void grease_pencil_clear_from_vgroup(Scene &scene,
                                            Object &ob,
                                            bDeformGroup *dg,
                                            const bool use_selection,
                                            const bool all_drawings = false)
{
  using namespace ed::greasepencil;
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob.data);

  if (all_drawings) {
    /* When removing vgroup, iterate over all the drawing. */
    for (GreasePencilDrawingBase *base : grease_pencil.drawings()) {
      if (base->type != GP_DRAWING) {
        continue;
      }
      bke::greasepencil::Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
      bke::greasepencil::remove_from_vertex_group(drawing, dg->name, use_selection);
    }
    /* Remove vgroup from the list. */
    BKE_object_defgroup_remove(&ob, dg);
  }
  else {
    Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);
    for (const MutableDrawingInfo &info : drawings) {
      bke::greasepencil::remove_from_vertex_group(info.drawing, dg->name, use_selection);
    }
  }
}

static void grease_pencil_clear_from_all_vgroup(Scene &scene,
                                                Object &ob,
                                                const bool use_selection,
                                                const bool all_drawings = false,
                                                const bool only_unlocked = false)
{
  const ListBase *defbase = BKE_object_defgroup_list(&ob);

  bDeformGroup *dg = static_cast<bDeformGroup *>(defbase->first);
  while (dg) {
    bDeformGroup *next_group = dg->next;
    if (!only_unlocked || (dg->flag & DG_LOCK_WEIGHT) == 0) {
      grease_pencil_clear_from_vgroup(scene, ob, dg, use_selection, all_drawings);
    }
    dg = next_group;
  }
}

static wmOperatorStatus vertex_group_remove_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);
  Scene &scene = *CTX_data_scene(C);
  const bool all_vgroup = RNA_boolean_get(op->ptr, "all");
  const bool only_unlocked = RNA_boolean_get(op->ptr, "all_unlocked");

  if (ob->type == OB_GREASE_PENCIL) {
    if (all_vgroup || only_unlocked) {
      grease_pencil_clear_from_all_vgroup(scene, *ob, false, true, only_unlocked);
    }
    else {
      const ListBase *defbase = BKE_object_defgroup_list(ob);
      bDeformGroup *dg = static_cast<bDeformGroup *>(
          BLI_findlink(defbase, BKE_object_defgroup_active_index_get(ob) - 1));

      if (!dg) {
        return OPERATOR_CANCELLED;
      }
      grease_pencil_clear_from_vgroup(scene, *ob, dg, false, true);
    }
  }
  else {
    if (all_vgroup || only_unlocked) {
      BKE_object_defgroup_remove_all_ex(ob, only_unlocked);
    }
    else {
      vgroup_delete_active(ob);
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_GEOM | ND_VERTEX_GROUP, ob->data);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Vertex Group";
  ot->idname = "OBJECT_OT_vertex_group_remove";
  ot->description = "Delete the active or all vertex groups from the active object";

  /* API callbacks. */
  ot->poll = vertex_group_poll;
  ot->exec = vertex_group_remove_exec;

  /* flags */
  /* redo operator will fail in this case because vertex groups aren't stored
   * in local edit mode stack and toggling "all" property will lead to
   * all groups deleted without way to restore them (see #29527, sergey) */
  ot->flag = /*OPTYPE_REGISTER|*/ OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop = RNA_def_boolean(ot->srna, "all", false, "All", "Remove all vertex groups");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "all_unlocked", false, "All Unlocked", "Remove all unlocked vertex groups");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Assign Operator
 * \{ */

static wmOperatorStatus vertex_group_assign_exec(bContext *C, wmOperator *op)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = context_object(C);
  Scene &scene = *CTX_data_scene(C);

  vgroup_assign_verts(ob, scene, ts->vgroup_weight);

  if (ts->auto_normalize) {
    if (ob->type == OB_GREASE_PENCIL) {
      const int current_frame = scene.r.cfra;
      vgroup_normalize_all_deform_if_active_is_deform(ob, true, op->reports, current_frame);
    }
    else {
      vgroup_normalize_all_deform_if_active_is_deform(ob, true, op->reports);
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_assign(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Assign to Vertex Group";
  ot->idname = "OBJECT_OT_vertex_group_assign";
  ot->description = "Assign the selected vertices to the active vertex group";

  /* API callbacks. */
  ot->poll = vertex_group_vert_select_unlocked_poll;
  ot->exec = vertex_group_assign_exec;

  /* flags */
  /* redo operator will fail in this case because vertex group assignment
   * isn't stored in local edit mode stack and toggling "new" property will
   * lead to creating plenty of new vertex groups (see #29527, sergey) */
  ot->flag = /*OPTYPE_REGISTER|*/ OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Assign New Operator
 * \{ */

/* NOTE: just a wrapper around vertex_group_assign_exec(), except we add these to a new group */
static wmOperatorStatus vertex_group_assign_new_exec(bContext *C, wmOperator *op)
{
  /* create new group... */
  Object *ob = context_object(C);
  BKE_object_defgroup_add(ob);

  /* assign selection to new group */
  return vertex_group_assign_exec(C, op);
}

void OBJECT_OT_vertex_group_assign_new(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Assign to New Group";
  ot->idname = "OBJECT_OT_vertex_group_assign_new";
  ot->description = "Assign the selected vertices to a new vertex group";

  /* API callbacks. */
  ot->poll = vertex_group_vert_select_poll;
  ot->exec = vertex_group_assign_new_exec;

  /* flags */
  /* redo operator will fail in this case because vertex group assignment
   * isn't stored in local edit mode stack and toggling "new" property will
   * lead to creating plenty of new vertex groups (see #29527, sergey) */
  ot->flag = /*OPTYPE_REGISTER|*/ OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Remove From Operator
 * \{ */

static wmOperatorStatus vertex_group_remove_from_exec(bContext *C, wmOperator *op)
{
  const bool use_all_groups = RNA_boolean_get(op->ptr, "use_all_groups");
  const bool use_all_verts = RNA_boolean_get(op->ptr, "use_all_verts");
  Scene &scene = *CTX_data_scene(C);

  Object *ob = context_object(C);

  if (use_all_groups) {
    if (ob->type == OB_GREASE_PENCIL) {
      grease_pencil_clear_from_all_vgroup(scene, *ob, true);
    }
    else if (BKE_object_defgroup_clear_all(ob, true) == false) {
      return OPERATOR_CANCELLED;
    }
  }
  else {
    const ListBase *defbase = BKE_object_defgroup_list(ob);
    bDeformGroup *dg = static_cast<bDeformGroup *>(
        BLI_findlink(defbase, BKE_object_defgroup_active_index_get(ob) - 1));
    if (dg == nullptr) {
      return OPERATOR_CANCELLED;
    }

    if (ob->type == OB_GREASE_PENCIL) {
      grease_pencil_clear_from_vgroup(scene, *ob, dg, !use_all_verts);
    }
    else if (BKE_object_defgroup_clear(ob, dg, !use_all_verts) == false) {
      return OPERATOR_CANCELLED;
    }
  }

  ToolSettings *ts = CTX_data_tool_settings(C);
  if (ts->auto_normalize) {
    if (ob->type == OB_GREASE_PENCIL) {
      const int current_frame = scene.r.cfra;
      vgroup_normalize_all_deform_if_active_is_deform(ob, true, op->reports, current_frame);
    }
    else {
      vgroup_normalize_all_deform_if_active_is_deform(ob, true, op->reports);
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_remove_from(wmOperatorType *ot)
{
  PropertyRNA *prop;
  /* identifiers */
  ot->name = "Remove from Vertex Group";
  ot->idname = "OBJECT_OT_vertex_group_remove_from";
  ot->description = "Remove the selected vertices from active or all vertex group(s)";

  /* API callbacks. */
  ot->poll = vertex_group_vert_select_unlocked_poll;
  ot->exec = vertex_group_remove_from_exec;

  /* flags */
  /* redo operator will fail in this case because vertex groups assignment
   * isn't stored in local edit mode stack and toggling "all" property will lead to
   * removing vertices from all groups (see #29527, sergey) */
  ot->flag = /*OPTYPE_REGISTER|*/ OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_boolean(
      ot->srna, "use_all_groups", false, "All Groups", "Remove from all groups");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "use_all_verts", false, "All Vertices", "Clear the active group");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Select Operator
 * \{ */

static wmOperatorStatus vertex_group_select_exec(bContext *C, wmOperator * /*op*/)
{
  const ToolSettings &tool_settings = *CTX_data_scene(C)->toolsettings;
  Object *ob = context_object(C);
  Scene &scene = *CTX_data_scene(C);

  if (!ob || !ID_IS_EDITABLE(ob) || ID_IS_OVERRIDE_LIBRARY(ob)) {
    return OPERATOR_CANCELLED;
  }

  vgroup_select_verts(tool_settings, ob, scene, 1);
  DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_SYNC_TO_EVAL | ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Vertex Group";
  ot->idname = "OBJECT_OT_vertex_group_select";
  ot->description = "Select all the vertices assigned to the active vertex group";

  /* API callbacks. */
  ot->poll = vertex_group_vert_select_poll;
  ot->exec = vertex_group_select_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Deselect Operator
 * \{ */

static wmOperatorStatus vertex_group_deselect_exec(bContext *C, wmOperator * /*op*/)
{
  const ToolSettings &tool_settings = *CTX_data_scene(C)->toolsettings;
  Object *ob = context_object(C);
  Scene &scene = *CTX_data_scene(C);

  vgroup_select_verts(tool_settings, ob, scene, 0);
  DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_SYNC_TO_EVAL | ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_deselect(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Deselect Vertex Group";
  ot->idname = "OBJECT_OT_vertex_group_deselect";
  ot->description = "Deselect all selected vertices assigned to the active vertex group";

  /* API callbacks. */
  ot->poll = vertex_group_vert_select_poll;
  ot->exec = vertex_group_deselect_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Copy Operator
 * \{ */

static wmOperatorStatus vertex_group_copy_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = context_object(C);

  vgroup_duplicate(ob);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  WM_event_add_notifier(C, NC_GEOM | ND_VERTEX_GROUP, ob->data);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Duplicate Vertex Group";
  ot->idname = "OBJECT_OT_vertex_group_copy";
  ot->description = "Make a copy of the active vertex group";

  /* API callbacks. */
  ot->poll = vertex_group_poll;
  ot->exec = vertex_group_copy_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Levels Operator
 * \{ */

static wmOperatorStatus vertex_group_levels_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);

  float offset = RNA_float_get(op->ptr, "offset");
  float gain = RNA_float_get(op->ptr, "gain");
  eVGroupSelect subset_type = static_cast<eVGroupSelect>(
      RNA_enum_get(op->ptr, "group_select_mode"));

  int subset_count, vgroup_tot;

  const bool *vgroup_validmap = BKE_object_defgroup_subset_from_select_type(
      ob, subset_type, &vgroup_tot, &subset_count);
  vgroup_levels_subset(ob, vgroup_validmap, vgroup_tot, subset_count, offset, gain);
  MEM_freeN(vgroup_validmap);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_levels(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Group Levels";
  ot->idname = "OBJECT_OT_vertex_group_levels";
  ot->description =
      "Add some offset and multiply with some gain the weights of the active vertex group";

  /* API callbacks. */
  ot->poll = vertex_group_poll;
  ot->exec = vertex_group_levels_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  vgroup_operator_subset_select_props(ot, true);
  RNA_def_float(
      ot->srna, "offset", 0.0f, -1.0, 1.0, "Offset", "Value to add to weights", -1.0f, 1.0f);
  RNA_def_float(
      ot->srna, "gain", 1.0f, 0.0f, FLT_MAX, "Gain", "Value to multiply weights by", 0.0f, 10.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Normalize Operator
 * \{ */

static wmOperatorStatus vertex_group_normalize_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = context_object(C);
  bool changed;

  changed = vgroup_normalize(ob);

  if (changed) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_vertex_group_normalize(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Normalize Vertex Group";
  ot->idname = "OBJECT_OT_vertex_group_normalize";
  ot->description =
      "Normalize weights of the active vertex group, so that the highest ones are now 1.0";

  /* API callbacks. */
  ot->poll = vertex_group_poll;
  ot->exec = vertex_group_normalize_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Normalize All Operator
 * \{ */

/*
 * For a given object, determine which target vertex group to normalize.
 */
static eVGroupSelect normalize_vertex_group_target(Object *ob)
{
  /* Default to All Groups. */
  eVGroupSelect target_group = WT_VGROUP_ALL;

  /* If armature is present, and armature is actively deforming the object
   * (i.e armature modifier isn't disabled) use BONE DEFORM. */
  if (BKE_modifiers_is_deformed_by_armature(ob)) {

    int defgroup_tot = BKE_object_defgroup_count(ob);
    bool *defgroup_validmap = BKE_object_defgroup_validmap_get(ob, defgroup_tot);

    for (int i = 0; i < defgroup_tot; i++) {
      if (defgroup_validmap[i] == true) {
        target_group = WT_VGROUP_BONE_DEFORM;
        break;
      }
    }
    MEM_freeN(defgroup_validmap);
  }

  return target_group;
}

static wmOperatorStatus vertex_group_normalize_all_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);

  eVGroupSelect target_group = normalize_vertex_group_target(ob);

  RNA_enum_set(op->ptr, "group_select_mode", target_group);

  bool lock_active = RNA_boolean_get(op->ptr, "lock_active");
  eVGroupSelect subset_type = static_cast<eVGroupSelect>(
      RNA_enum_get(op->ptr, "group_select_mode"));
  bool changed;
  int subset_count, vgroup_tot;
  const bool *vgroup_validmap = BKE_object_defgroup_subset_from_select_type(
      ob, subset_type, &vgroup_tot, &subset_count);

  if (subset_count == 0) {
    BKE_report(op->reports, RPT_ERROR, "No vertex groups to operate on");
    changed = false;
  }
  else {
    if (ob->type == OB_GREASE_PENCIL) {
      int current_frame = CTX_data_scene(C)->r.cfra;
      changed = vgroup_normalize_all(
          ob, vgroup_validmap, vgroup_tot, lock_active, false, op->reports, current_frame);
    }
    else {
      changed = vgroup_normalize_all(
          ob, vgroup_validmap, vgroup_tot, lock_active, false, op->reports);
    }
  }

  MEM_freeN(vgroup_validmap);

  if (changed) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

    return OPERATOR_FINISHED;
  }

  /* allow to adjust settings */
  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_normalize_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Normalize All Vertex Groups";
  ot->idname = "OBJECT_OT_vertex_group_normalize_all";
  ot->description =
      "Normalize all weights of all vertex groups, "
      "so that for each vertex, the sum of all weights is 1.0";

  /* API callbacks. */
  ot->poll = vertex_group_poll;
  ot->exec = vertex_group_normalize_all_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  vgroup_operator_subset_select_props(ot, false);
  RNA_def_boolean(ot->srna,
                  "lock_active",
                  true,
                  "Lock Active",
                  "Keep the values of the active group while normalizing others");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Lock Operator
 * \{ */

static wmOperatorStatus vertex_group_lock_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);

  int action = RNA_enum_get(op->ptr, "action");
  int mask = RNA_enum_get(op->ptr, "mask");

  vgroup_lock_all(ob, action, mask);

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

static std::string vertex_group_lock_get_description(bContext * /*C*/,
                                                     wmOperatorType * /*ot*/,
                                                     PointerRNA *ptr)
{
  int action = RNA_enum_get(ptr, "action");
  int mask = RNA_enum_get(ptr, "mask");

  /* NOTE: constructing the following string literals can be done in a less verbose way,
   * however the resulting strings can't be usefully translated, (via `TIP_`). */
  switch (action) {
    case VGROUP_LOCK:
      switch (mask) {
        case VGROUP_MASK_ALL:
          return TIP_("Lock all vertex groups of the active object");
        case VGROUP_MASK_SELECTED:
          return TIP_("Lock selected vertex groups of the active object");
        case VGROUP_MASK_UNSELECTED:
          return TIP_("Lock unselected vertex groups of the active object");
        case VGROUP_MASK_INVERT_UNSELECTED:
          return TIP_("Lock selected and unlock unselected vertex groups of the active object");
      }
      break;
    case VGROUP_UNLOCK:
      switch (mask) {
        case VGROUP_MASK_ALL:
          return TIP_("Unlock all vertex groups of the active object");
        case VGROUP_MASK_SELECTED:
          return TIP_("Unlock selected vertex groups of the active object");
        case VGROUP_MASK_UNSELECTED:
          return TIP_("Unlock unselected vertex groups of the active object");
        case VGROUP_MASK_INVERT_UNSELECTED:
          return TIP_("Unlock selected and lock unselected vertex groups of the active object");
      }
      break;
    case VGROUP_TOGGLE:
      switch (mask) {
        case VGROUP_MASK_ALL:
          return TIP_("Toggle locks of all vertex groups of the active object");
        case VGROUP_MASK_SELECTED:
          return TIP_("Toggle locks of selected vertex groups of the active object");
        case VGROUP_MASK_UNSELECTED:
          return TIP_("Toggle locks of unselected vertex groups of the active object");
        case VGROUP_MASK_INVERT_UNSELECTED:
          return TIP_(
              "Toggle locks of all and invert unselected vertex groups of the active object");
      }
      break;
    case VGROUP_INVERT:
      switch (mask) {
        case VGROUP_MASK_ALL:
          return TIP_("Invert locks of all vertex groups of the active object");
        case VGROUP_MASK_SELECTED:
        case VGROUP_MASK_INVERT_UNSELECTED:
          return TIP_("Invert locks of selected vertex groups of the active object");
        case VGROUP_MASK_UNSELECTED:
          return TIP_("Invert locks of unselected vertex groups of the active object");
      }
      break;
    default:
      return {};
  }
  return {};
}

void OBJECT_OT_vertex_group_lock(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Change the Lock On Vertex Groups";
  ot->idname = "OBJECT_OT_vertex_group_lock";
  ot->description = "Change the lock state of all or some vertex groups of active object";

  /* API callbacks. */
  ot->poll = vertex_group_poll;
  ot->exec = vertex_group_lock_exec;
  ot->get_description = vertex_group_lock_get_description;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna,
               "action",
               vgroup_lock_actions,
               VGROUP_TOGGLE,
               "Action",
               "Lock action to execute on vertex groups");

  RNA_def_enum(ot->srna,
               "mask",
               vgroup_lock_mask,
               VGROUP_MASK_ALL,
               "Mask",
               "Apply the action based on vertex group selection");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Invert Operator
 * \{ */

static wmOperatorStatus vertex_group_invert_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);
  bool auto_assign = RNA_boolean_get(op->ptr, "auto_assign");
  bool auto_remove = RNA_boolean_get(op->ptr, "auto_remove");

  eVGroupSelect subset_type = static_cast<eVGroupSelect>(
      RNA_enum_get(op->ptr, "group_select_mode"));

  int subset_count, vgroup_tot;

  const bool *vgroup_validmap = BKE_object_defgroup_subset_from_select_type(
      ob, subset_type, &vgroup_tot, &subset_count);
  vgroup_invert_subset(ob, vgroup_validmap, vgroup_tot, subset_count, auto_assign, auto_remove);
  MEM_freeN(vgroup_validmap);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_invert(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Invert Vertex Group";
  ot->idname = "OBJECT_OT_vertex_group_invert";
  ot->description = "Invert active vertex group's weights";

  /* API callbacks. */
  ot->poll = vertex_group_poll;
  ot->exec = vertex_group_invert_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  vgroup_operator_subset_select_props(ot, true);
  RNA_def_boolean(ot->srna,
                  "auto_assign",
                  true,
                  "Add Weights",
                  "Add vertices from groups that have zero weight before inverting");
  RNA_def_boolean(ot->srna,
                  "auto_remove",
                  true,
                  "Remove Weights",
                  "Remove vertices from groups that have zero weight after inverting");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Smooth Operator
 * \{ */

static wmOperatorStatus vertex_group_smooth_exec(bContext *C, wmOperator *op)
{
  const float fac = RNA_float_get(op->ptr, "factor");
  const int repeat = RNA_int_get(op->ptr, "repeat");
  const eVGroupSelect subset_type = static_cast<eVGroupSelect>(
      RNA_enum_get(op->ptr, "group_select_mode"));
  const float fac_expand = RNA_float_get(op->ptr, "expand");

  bool has_vgroup_multi = false;
  const Vector<Object *> objects = object_array_for_wpaint(C);
  for (Object *ob : objects) {
    int subset_count, vgroup_tot;

    bool *vgroup_validmap = BKE_object_defgroup_subset_from_select_type(
        ob, subset_type, &vgroup_tot, &subset_count);

    if (vgroup_tot) {
      const bool *locked_vgroups = BKE_object_defgroup_lock_flags_get(ob, vgroup_tot);
      if (locked_vgroups) {
        /* Remove locked groups from the vgroup valid map. */
        for (int i = 0; i < vgroup_tot; i++) {
          if (vgroup_validmap[i] && locked_vgroups[i]) {
            vgroup_validmap[i] = false;
            subset_count--;
          }
        }
      }
      MEM_SAFE_FREE(locked_vgroups);

      has_vgroup_multi = true;

      if (subset_count) {
        vgroup_smooth_subset(
            ob, vgroup_validmap, vgroup_tot, subset_count, fac, repeat, fac_expand);

        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
        WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
        WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
      }
    }

    MEM_freeN(vgroup_validmap);
  }

  /* NOTE: typically we would return canceled if no changes were made (`changed_multi`).
   * In this case it's important only to do this if none of the objects *could* be changed.
   * TODO: skip meshes without any selected vertices.
   *
   * The reason this is a special case is returning canceled prevents the `group_select_mode`
   * from being changed, where this setting could have been the reason no change was possible. */
  if (!has_vgroup_multi) {
    BKE_reportf(op->reports, RPT_WARNING, "No meshes with vertex groups found");
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_smooth(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Smooth Vertex Weights";
  ot->idname = "OBJECT_OT_vertex_group_smooth";
  ot->description = "Smooth weights for selected vertices";

  /* API callbacks. */
  ot->poll = vertex_group_mesh_vert_poll;
  ot->exec = vertex_group_smooth_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  vgroup_operator_subset_select_props(ot, true);
  RNA_def_float(ot->srna, "factor", 0.5f, 0.0f, 1.0, "Factor", "", 0.0f, 1.0f);
  RNA_def_int(ot->srna, "repeat", 1, 1, 10000, "Iterations", "", 1, 200);

  RNA_def_float(ot->srna,
                "expand",
                0.0f,
                -1.0f,
                1.0,
                "Expand/Contract",
                "Expand/contract weights",
                -1.0f,
                1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Clean Operator
 * \{ */

static wmOperatorStatus vertex_group_clean_exec(bContext *C, wmOperator *op)
{
  const float limit = RNA_float_get(op->ptr, "limit");
  const bool keep_single = RNA_boolean_get(op->ptr, "keep_single");
  const eVGroupSelect subset_type = static_cast<eVGroupSelect>(
      RNA_enum_get(op->ptr, "group_select_mode"));

  const Vector<Object *> objects = object_array_for_wpaint(C);
  for (Object *ob : objects) {
    int subset_count, vgroup_tot;

    const bool *vgroup_validmap = BKE_object_defgroup_subset_from_select_type(
        ob, subset_type, &vgroup_tot, &subset_count);

    vgroup_clean_subset(ob, vgroup_validmap, vgroup_tot, subset_count, limit, keep_single);
    MEM_freeN(vgroup_validmap);

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_clean(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clean Vertex Group Weights";
  ot->idname = "OBJECT_OT_vertex_group_clean";
  ot->description = "Remove vertex group assignments which are not required";

  /* API callbacks. */
  ot->poll = vertex_group_poll;
  ot->exec = vertex_group_clean_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  vgroup_operator_subset_select_props(ot, true);
  RNA_def_float(ot->srna,
                "limit",
                0.0f,
                0.0f,
                1.0,
                "Limit",
                "Remove vertices which weight is below or equal to this limit",
                0.0f,
                0.99f);
  RNA_def_boolean(ot->srna,
                  "keep_single",
                  false,
                  "Keep Single",
                  "Keep verts assigned to at least one group when cleaning");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Quantize Operator
 * \{ */

static wmOperatorStatus vertex_group_quantize_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);

  const int steps = RNA_int_get(op->ptr, "steps");
  eVGroupSelect subset_type = static_cast<eVGroupSelect>(
      RNA_enum_get(op->ptr, "group_select_mode"));

  int subset_count, vgroup_tot;

  const bool *vgroup_validmap = BKE_object_defgroup_subset_from_select_type(
      ob, subset_type, &vgroup_tot, &subset_count);
  vgroup_quantize_subset(ob, vgroup_validmap, vgroup_tot, subset_count, steps);
  MEM_freeN(vgroup_validmap);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_quantize(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Quantize Vertex Weights";
  ot->idname = "OBJECT_OT_vertex_group_quantize";
  ot->description = "Set weights to a fixed number of steps";

  /* API callbacks. */
  ot->poll = vertex_group_poll;
  ot->exec = vertex_group_quantize_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  vgroup_operator_subset_select_props(ot, true);
  RNA_def_int(ot->srna, "steps", 4, 1, 1000, "Steps", "Number of steps between 0 and 1", 1, 100);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Limit Total Operator
 * \{ */

static wmOperatorStatus vertex_group_limit_total_exec(bContext *C, wmOperator *op)
{
  const int limit = RNA_int_get(op->ptr, "limit");
  const eVGroupSelect subset_type = static_cast<eVGroupSelect>(
      RNA_enum_get(op->ptr, "group_select_mode"));
  int remove_multi_count = 0;

  const Vector<Object *> objects = object_array_for_wpaint(C);
  for (Object *ob : objects) {

    int subset_count, vgroup_tot;
    const bool *vgroup_validmap = BKE_object_defgroup_subset_from_select_type(
        ob, subset_type, &vgroup_tot, &subset_count);
    const int remove_count = vgroup_limit_total_subset(
        ob, vgroup_validmap, vgroup_tot, subset_count, limit);
    MEM_freeN(vgroup_validmap);

    if (remove_count != 0) {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
    }
    remove_multi_count += remove_count;
  }

  if (remove_multi_count) {
    BKE_reportf(op->reports,
                remove_multi_count ? RPT_INFO : RPT_WARNING,
                "%d vertex weights limited",
                remove_multi_count);

    return OPERATOR_FINISHED;
  }

  /* NOTE: would normally return canceled, except we want the redo
   * UI to show up for users to change */
  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_limit_total(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Limit Number of Weights per Vertex";
  ot->idname = "OBJECT_OT_vertex_group_limit_total";
  ot->description =
      "Limit deform weights associated with a vertex to a specified number by removing lowest "
      "weights";

  /* API callbacks. */
  ot->poll = vertex_group_poll;
  ot->exec = vertex_group_limit_total_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  vgroup_operator_subset_select_props(ot, false);
  RNA_def_int(ot->srna, "limit", 4, 1, 32, "Limit", "Maximum number of deform weights", 1, 32);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Mirror Operator
 * \{ */

static wmOperatorStatus vertex_group_mirror_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);
  int totmirr = 0, totfail = 0;

  vgroup_mirror(ob,
                RNA_boolean_get(op->ptr, "mirror_weights"),
                RNA_boolean_get(op->ptr, "flip_group_names"),
                RNA_boolean_get(op->ptr, "all_groups"),
                RNA_boolean_get(op->ptr, "use_topology"),
                &totmirr,
                &totfail);

  ED_mesh_report_mirror(*op->reports, totmirr, totfail);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_mirror(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mirror Vertex Group";
  ot->idname = "OBJECT_OT_vertex_group_mirror";
  ot->description =
      "Mirror vertex group, flip weights and/or names, editing only selected vertices, "
      "flipping when both sides are selected otherwise copy from unselected";

  /* API callbacks. */
  ot->poll = vertex_group_poll;
  ot->exec = vertex_group_mirror_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "mirror_weights", true, "Mirror Weights", "Mirror weights");
  RNA_def_boolean(
      ot->srna, "flip_group_names", true, "Flip Group Names", "Flip vertex group names");
  RNA_def_boolean(ot->srna, "all_groups", false, "All Groups", "Mirror all vertex groups weights");
  RNA_def_boolean(
      ot->srna,
      "use_topology",
      false,
      "Topology Mirror",
      "Use topology based mirroring (for when both sides of mesh have matching, unique topology)");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Copy to Selected Operator
 * \{ */

static wmOperatorStatus vertex_group_copy_to_selected_exec(bContext *C, wmOperator *op)
{
  Object *obact = context_object(C);
  int changed_tot = 0;
  int fail = 0;

  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    if (obact != ob && BKE_object_supports_vertex_groups(ob)) {
      if (vgroup_array_copy(ob, obact)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
        DEG_relations_tag_update(CTX_data_main(C));
        WM_event_add_notifier(C, NC_GEOM | ND_VERTEX_GROUP, ob);
        changed_tot++;
      }
      else {
        fail++;
      }
    }
  }
  CTX_DATA_END;

  if ((changed_tot == 0 && fail == 0) || fail) {
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Copy vertex groups to selected: %d done, %d failed (object data must support "
                "vertex groups and have matching indices)",
                changed_tot,
                fail);
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_copy_to_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Vertex Group to Selected";
  ot->idname = "OBJECT_OT_vertex_group_copy_to_selected";
  ot->description = "Replace vertex groups of selected objects by vertex groups of active object";

  /* API callbacks. */
  ot->poll = vertex_group_poll;
  ot->exec = vertex_group_copy_to_selected_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Set Active Operator
 * \{ */

static wmOperatorStatus set_active_group_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);
  int nr = RNA_enum_get(op->ptr, "group");

  BLI_assert(nr + 1 >= 0);
  BKE_object_defgroup_active_index_set(ob, nr + 1);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_VERTEX_GROUP, ob);

  return OPERATOR_FINISHED;
}

static const EnumPropertyItem *vgroup_itemf(bContext *C,
                                            PointerRNA * /*ptr*/,
                                            PropertyRNA * /*prop*/,
                                            bool *r_free)
{
  if (C == nullptr) {
    return rna_enum_dummy_NULL_items;
  }

  Object *ob = context_object(C);
  EnumPropertyItem tmp = {0, "", 0, "", ""};
  EnumPropertyItem *item = nullptr;
  bDeformGroup *def;
  int a, totitem = 0;

  if (!ob) {
    return rna_enum_dummy_NULL_items;
  }

  const ListBase *defbase = BKE_object_defgroup_list(ob);
  for (a = 0, def = static_cast<bDeformGroup *>(defbase->first); def; def = def->next, a++) {
    tmp.value = a;
    tmp.icon = ICON_GROUP_VERTEX;
    tmp.identifier = def->name;
    tmp.name = def->name;
    RNA_enum_item_add(&item, &totitem, &tmp);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

void OBJECT_OT_vertex_group_set_active(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Set Active Vertex Group";
  ot->idname = "OBJECT_OT_vertex_group_set_active";
  ot->description = "Set the active vertex group";

  /* API callbacks. */
  ot->poll = vertex_group_poll;
  ot->exec = set_active_group_exec;
  ot->invoke = WM_menu_invoke;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(
      ot->srna, "group", rna_enum_dummy_NULL_items, 0, "Group", "Vertex group to set as active");
  RNA_def_enum_funcs(prop, vgroup_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Sort Operator
 * \{ */

/* creates the name_array parameter for vgroup_do_remap, call this before fiddling
 * with the order of vgroups then call vgroup_do_remap after */
static char *vgroup_init_remap(Object *ob)
{
  const ListBase *defbase = BKE_object_defgroup_list(ob);
  int defbase_tot = BLI_listbase_count(defbase);
  char *name_array = static_cast<char *>(
      MEM_mallocN(MAX_VGROUP_NAME * sizeof(char) * defbase_tot, "sort vgroups"));
  char *name;

  name = name_array;
  LISTBASE_FOREACH (const bDeformGroup *, def, defbase) {
    BLI_strncpy_utf8(name, def->name, MAX_VGROUP_NAME);
    name += MAX_VGROUP_NAME;
  }

  return name_array;
}

static wmOperatorStatus vgroup_do_remap(Object *ob, const char *name_array, wmOperator *op)
{
  MDeformVert *dvert = nullptr;
  const bDeformGroup *def;
  const ListBase *defbase = BKE_object_defgroup_list(ob);
  int defbase_tot = BLI_listbase_count(defbase);

  /* Needs a dummy index at the start. */
  int *sort_map_update = MEM_malloc_arrayN<int>((defbase_tot + 1), __func__);
  int *sort_map = sort_map_update + 1;

  const char *name;
  int i;

  name = name_array;
  for (def = static_cast<const bDeformGroup *>(defbase->first), i = 0; def; def = def->next, i++) {
    sort_map[i] = BKE_defgroup_name_index(defbase, name);
    name += MAX_VGROUP_NAME;

    BLI_assert(sort_map[i] != -1);
  }

  if (ob->type == OB_GREASE_PENCIL) {
    /* For Grease Pencil objects we don't have to do anything, because all drawings in the object
     * store their own set of #vertex_group_names. So changing the vertex group order on object
     * level is just a UI matter, no remapping in drawings is needed. */
  }
  else if (ob->mode == OB_MODE_EDIT) {
    if (ob->type == OB_MESH) {
      BMEditMesh *em = BKE_editmesh_from_object(ob);
      const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

      if (cd_dvert_offset != -1) {
        BMIter iter;
        BMVert *eve;

        BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
          dvert = static_cast<MDeformVert *>(BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset));
          if (dvert->totweight) {
            BKE_defvert_remap(dvert, sort_map, defbase_tot);
          }
        }
      }
    }
    else {
      BKE_report(op->reports, RPT_ERROR, "Editmode lattice is not supported yet");
      MEM_freeN(sort_map_update);
      return OPERATOR_CANCELLED;
    }
  }
  else {
    int dvert_tot = 0;
    BKE_object_defgroup_array_get(static_cast<ID *>(ob->data), &dvert, &dvert_tot);

    /* Create as necessary. */
    if (dvert) {
      while (dvert_tot--) {
        if (dvert->totweight) {
          BKE_defvert_remap(dvert, sort_map, defbase_tot);
        }
        dvert++;
      }
    }
  }

  /* update users */
  for (i = 0; i < defbase_tot; i++) {
    sort_map[i]++;
  }

  sort_map_update[0] = 0;
  BKE_object_defgroup_remap_update_users(ob, sort_map_update);

  BLI_assert(sort_map_update[BKE_object_defgroup_active_index_get(ob)] >= 0);
  BKE_object_defgroup_active_index_set(ob,
                                       sort_map_update[BKE_object_defgroup_active_index_get(ob)]);

  MEM_freeN(sort_map_update);

  return OPERATOR_FINISHED;
}

static int vgroup_sort_name(const void *def_a_ptr, const void *def_b_ptr)
{
  const bDeformGroup *def_a = static_cast<const bDeformGroup *>(def_a_ptr);
  const bDeformGroup *def_b = static_cast<const bDeformGroup *>(def_b_ptr);

  return BLI_strcasecmp_natural(def_a->name, def_b->name);
}

/**
 * Sorts the weight groups according to the bone hierarchy of the
 * associated armature (similar to how bones are ordered in the Outliner)
 */
static void vgroup_sort_bone_hierarchy(Object *ob, ListBase *bonebase)
{
  if (bonebase == nullptr) {
    Object *armobj = BKE_modifiers_is_deformed_by_armature(ob);
    if (armobj != nullptr) {
      bArmature *armature = static_cast<bArmature *>(armobj->data);
      bonebase = &armature->bonebase;
    }
  }
  ListBase *defbase = BKE_object_defgroup_list_mutable(ob);

  if (bonebase != nullptr) {
    LISTBASE_FOREACH_BACKWARD (Bone *, bone, bonebase) {
      bDeformGroup *dg = BKE_object_defgroup_find_name(ob, bone->name);
      vgroup_sort_bone_hierarchy(ob, &bone->childbase);

      if (dg != nullptr) {
        BLI_remlink(defbase, dg);
        BLI_addhead(defbase, dg);
      }
    }
  }
}

enum {
  SORT_TYPE_NAME = 0,
  SORT_TYPE_BONEHIERARCHY = 1,
};

static wmOperatorStatus vertex_group_sort_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);
  char *name_array;
  wmOperatorStatus ret;
  int sort_type = RNA_enum_get(op->ptr, "sort_type");

  /* Init remapping. */
  name_array = vgroup_init_remap(ob);

  ListBase *defbase = BKE_object_defgroup_list_mutable(ob);

  /* Sort vgroup names. */
  switch (sort_type) {
    case SORT_TYPE_NAME:
      BLI_listbase_sort(defbase, vgroup_sort_name);
      break;
    case SORT_TYPE_BONEHIERARCHY:
      vgroup_sort_bone_hierarchy(ob, nullptr);
      break;
  }

  /* Remap vgroup data to map to correct names. */
  ret = vgroup_do_remap(ob, name_array, op);

  if (ret != OPERATOR_CANCELLED) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_VERTEX_GROUP, ob);
  }

  if (name_array) {
    MEM_freeN(name_array);
  }

  return ret;
}

void OBJECT_OT_vertex_group_sort(wmOperatorType *ot)
{
  static const EnumPropertyItem vgroup_sort_type[] = {
      {SORT_TYPE_NAME, "NAME", 0, "Name", ""},
      {SORT_TYPE_BONEHIERARCHY, "BONE_HIERARCHY", 0, "Bone Hierarchy", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  ot->name = "Sort Vertex Groups";
  ot->idname = "OBJECT_OT_vertex_group_sort";
  ot->description = "Sort vertex groups";

  /* API callbacks. */
  ot->poll = vertex_group_poll;
  ot->exec = vertex_group_sort_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna, "sort_type", vgroup_sort_type, SORT_TYPE_NAME, "Sort Type", "Sort type");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Move Operator
 * \{ */

static wmOperatorStatus vgroup_move_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);
  bDeformGroup *def;
  char *name_array;
  int dir = RNA_enum_get(op->ptr, "direction");
  wmOperatorStatus ret = OPERATOR_FINISHED;

  ListBase *defbase = BKE_object_defgroup_list_mutable(ob);

  def = static_cast<bDeformGroup *>(
      BLI_findlink(defbase, BKE_object_defgroup_active_index_get(ob) - 1));
  if (!def) {
    return OPERATOR_CANCELLED;
  }

  name_array = vgroup_init_remap(ob);

  if (BLI_listbase_link_move(defbase, def, dir)) {
    ret = vgroup_do_remap(ob, name_array, op);

    if (ret != OPERATOR_CANCELLED) {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_GEOM | ND_VERTEX_GROUP, ob);
    }
  }

  if (name_array) {
    MEM_freeN(name_array);
  }

  return ret;
}

void OBJECT_OT_vertex_group_move(wmOperatorType *ot)
{
  static const EnumPropertyItem vgroup_slot_move[] = {
      {-1, "UP", 0, "Up", ""},
      {1, "DOWN", 0, "Down", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Move Vertex Group";
  ot->idname = "OBJECT_OT_vertex_group_move";
  ot->description = "Move the active vertex group up/down in the list";

  /* API callbacks. */
  ot->poll = vertex_group_poll;
  ot->exec = vgroup_move_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna,
               "direction",
               vgroup_slot_move,
               0,
               "Direction",
               "Direction to move the active vertex group towards");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Weight Paste Operator
 * \{ */

static void vgroup_copy_active_to_sel_single(Object *ob, const int def_nr)
{
  MDeformVert *dvert_act;

  Mesh *mesh = static_cast<Mesh *>(ob->data);
  int i;

  if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
    const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);
    BMIter iter;
    BMVert *eve, *eve_act;

    dvert_act = ED_mesh_active_dvert_get_em(ob, &eve_act);
    if (dvert_act == nullptr) {
      return;
    }

    BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
      if (BM_elem_flag_test(eve, BM_ELEM_SELECT) && (eve != eve_act)) {
        MDeformVert *dvert_dst = static_cast<MDeformVert *>(
            BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset));

        BKE_defvert_copy_index(dvert_dst, def_nr, dvert_act, def_nr);

        if (mesh->symmetry & ME_SYMMETRY_X) {
          mesh_defvert_mirror_update_em(ob, eve, -1, i, cd_dvert_offset);
        }
      }
    }

    if (mesh->symmetry & ME_SYMMETRY_X) {
      mesh_defvert_mirror_update_em(ob, eve_act, -1, -1, cd_dvert_offset);
    }
  }
  else {
    int v_act;

    dvert_act = ED_mesh_active_dvert_get_ob(ob, &v_act);
    if (dvert_act == nullptr) {
      return;
    }

    MutableSpan<MDeformVert> dverts = mesh->deform_verts_for_write();
    const bke::AttributeAccessor attributes = mesh->attributes();
    const VArray<bool> select_vert = *attributes.lookup_or_default<bool>(
        ".select_vert", bke::AttrDomain::Point, false);

    for (i = 0; i < mesh->verts_num; i++) {
      if (select_vert[i] && (&dverts[i] != dvert_act)) {
        BKE_defvert_copy_index(&dverts[i], def_nr, dvert_act, def_nr);

        if (mesh->symmetry & ME_SYMMETRY_X) {
          mesh_defvert_mirror_update_ob(ob, -1, i);
        }
      }
    }

    if (mesh->symmetry & ME_SYMMETRY_X) {
      mesh_defvert_mirror_update_ob(ob, -1, v_act);
    }
  }
}

static bool check_vertex_group_accessible(wmOperator *op, Object *ob, int def_nr)
{
  const ListBase *defbase = BKE_object_defgroup_list(ob);
  bDeformGroup *dg = static_cast<bDeformGroup *>(BLI_findlink(defbase, def_nr));

  if (!dg) {
    BKE_report(op->reports, RPT_ERROR, "Invalid vertex group index");
    return false;
  }

  if (dg->flag & DG_LOCK_WEIGHT) {
    BKE_report(op->reports, RPT_ERROR, "Vertex group is locked");
    return false;
  }

  return true;
}

static wmOperatorStatus vertex_weight_paste_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);
  const int def_nr = RNA_int_get(op->ptr, "weight_group");

  if (!check_vertex_group_accessible(op, ob, def_nr)) {
    return OPERATOR_CANCELLED;
  }

  vgroup_copy_active_to_sel_single(ob, def_nr);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_weight_paste(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Paste Weight to Selected";
  ot->idname = "OBJECT_OT_vertex_weight_paste";
  ot->description =
      "Copy this group's weight to other selected vertices (disabled if vertex group is locked)";

  /* API callbacks. */
  ot->poll = vertex_group_vert_select_mesh_poll;
  ot->exec = vertex_weight_paste_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_int(ot->srna,
                     "weight_group",
                     -1,
                     -1,
                     INT_MAX,
                     "Weight Index",
                     "Index of source weight in active vertex group",
                     -1,
                     INT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Weight Delete Operator
 * \{ */

static wmOperatorStatus vertex_weight_delete_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);
  const int def_nr = RNA_int_get(op->ptr, "weight_group");

  if (!check_vertex_group_accessible(op, ob, def_nr)) {
    return OPERATOR_CANCELLED;
  }

  vgroup_remove_weight(ob, def_nr);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_weight_delete(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Delete Weight";
  ot->idname = "OBJECT_OT_vertex_weight_delete";
  ot->description = "Delete this weight from the vertex (disabled if vertex group is locked)";

  /* API callbacks. */
  ot->poll = vertex_group_vert_select_mesh_poll;
  ot->exec = vertex_weight_delete_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_int(ot->srna,
                     "weight_group",
                     -1,
                     -1,
                     INT_MAX,
                     "Weight Index",
                     "Index of source weight in active vertex group",
                     -1,
                     INT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Set Active by Weight Operator
 * \{ */

static wmOperatorStatus vertex_weight_set_active_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_object(C);
  const int wg_index = RNA_int_get(op->ptr, "weight_group");

  if (wg_index != -1) {
    BKE_object_defgroup_active_index_set(ob, wg_index + 1);
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_weight_set_active(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Set Active Group";
  ot->idname = "OBJECT_OT_vertex_weight_set_active";
  ot->description = "Set as active vertex group";

  /* API callbacks. */
  ot->poll = vertex_group_vert_select_mesh_poll;
  ot->exec = vertex_weight_set_active_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_int(ot->srna,
                     "weight_group",
                     -1,
                     -1,
                     INT_MAX,
                     "Weight Index",
                     "Index of source weight in active vertex group",
                     -1,
                     INT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Normalize Active Vertex Operator
 * \{ */

static wmOperatorStatus vertex_weight_normalize_active_vertex_exec(bContext *C,
                                                                   wmOperator * /*op*/)
{
  Object *ob = context_object(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  eVGroupSelect subset_type = static_cast<eVGroupSelect>(ts->vgroupsubset);
  bool changed;

  changed = vgroup_normalize_active_vertex(ob, subset_type);

  if (changed) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_vertex_weight_normalize_active_vertex(wmOperatorType *ot)
{

  ot->name = "Normalize Active";
  ot->idname = "OBJECT_OT_vertex_weight_normalize_active_vertex";
  ot->description = "Normalize active vertex's weights";

  /* API callbacks. */
  ot->poll = vertex_group_vert_select_mesh_poll;
  ot->exec = vertex_weight_normalize_active_vertex_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Group Copy Weights from Active Operator
 * \{ */

static wmOperatorStatus vertex_weight_copy_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = context_object(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  eVGroupSelect subset_type = static_cast<eVGroupSelect>(ts->vgroupsubset);

  vgroup_copy_active_to_sel(ob, subset_type);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_weight_copy(wmOperatorType *ot)
{

  ot->name = "Copy Active";
  ot->idname = "OBJECT_OT_vertex_weight_copy";
  ot->description = "Copy weights from active to selected";

  /* API callbacks. */
  ot->poll = vertex_group_vert_select_mesh_poll;
  ot->exec = vertex_weight_copy_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

}  // namespace blender::ed::object
