/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_curve_types.h"
#include "DNA_defaults.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_curve.hh"
#include "BKE_deform.hh"
#include "BKE_displist.h"
#include "BKE_idtype.hh"
#include "BKE_lattice.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"

#include "BLO_read_write.hh"

using blender::Array;
using blender::float3;
using blender::float4x4;
using blender::MutableSpan;
using blender::Span;

static void lattice_init_data(ID *id)
{
  Lattice *lattice = (Lattice *)id;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(lattice, id));

  MEMCPY_STRUCT_AFTER(lattice, DNA_struct_default_get(Lattice), id);

  lattice->def = MEM_callocN<BPoint>("lattvert"); /* temporary */
  BKE_lattice_resize(lattice, 2, 2, 2, nullptr);  /* creates a uniform lattice */
}

static void lattice_copy_data(Main *bmain,
                              std::optional<Library *> owner_library,
                              ID *id_dst,
                              const ID *id_src,
                              const int flag)
{
  Lattice *lattice_dst = (Lattice *)id_dst;
  const Lattice *lattice_src = (const Lattice *)id_src;

  lattice_dst->def = static_cast<BPoint *>(MEM_dupallocN(lattice_src->def));

  if (lattice_src->key && (flag & LIB_ID_COPY_SHAPEKEY)) {
    BKE_id_copy_in_lib(bmain,
                       owner_library,
                       &lattice_src->key->id,
                       &lattice_dst->id,
                       reinterpret_cast<ID **>(&lattice_dst->key),
                       flag);
  }

  BKE_defgroup_copy_list(&lattice_dst->vertex_group_names, &lattice_src->vertex_group_names);

  if (lattice_src->dvert) {
    int tot = lattice_src->pntsu * lattice_src->pntsv * lattice_src->pntsw;
    lattice_dst->dvert = MEM_malloc_arrayN<MDeformVert>(size_t(tot), "Lattice MDeformVert");
    BKE_defvert_array_copy(lattice_dst->dvert, lattice_src->dvert, tot);
  }

  lattice_dst->editlatt = nullptr;
  lattice_dst->batch_cache = nullptr;
}

static void lattice_free_data(ID *id)
{
  Lattice *lattice = (Lattice *)id;

  BKE_lattice_batch_cache_free(lattice);

  BLI_freelistN(&lattice->vertex_group_names);

  MEM_SAFE_FREE(lattice->def);
  if (lattice->dvert) {
    BKE_defvert_array_free(lattice->dvert, lattice->pntsu * lattice->pntsv * lattice->pntsw);
    lattice->dvert = nullptr;
  }
  if (lattice->editlatt) {
    Lattice *editlt = lattice->editlatt->latt;

    if (editlt->def) {
      MEM_freeN(editlt->def);
    }
    if (editlt->dvert) {
      BKE_defvert_array_free(editlt->dvert, lattice->pntsu * lattice->pntsv * lattice->pntsw);
    }

    MEM_freeN(editlt);
    MEM_freeN(lattice->editlatt);
    lattice->editlatt = nullptr;
  }
}

static void lattice_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Lattice *lattice = reinterpret_cast<Lattice *>(id);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, lattice->key, IDWALK_CB_USER);
}

static void lattice_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Lattice *lt = (Lattice *)id;

  /* Clean up, important in undo case to reduce false detection of changed datablocks. */
  lt->editlatt = nullptr;
  lt->batch_cache = nullptr;

  /* write LibData */
  BLO_write_id_struct(writer, Lattice, id_address, &lt->id);
  BKE_id_blend_write(writer, &lt->id);

  /* direct data */
  BLO_write_struct_array(writer, BPoint, lt->pntsu * lt->pntsv * lt->pntsw, lt->def);

  BKE_defbase_blend_write(writer, &lt->vertex_group_names);
  BKE_defvert_blend_write(writer, lt->pntsu * lt->pntsv * lt->pntsw, lt->dvert);
}

static void lattice_blend_read_data(BlendDataReader *reader, ID *id)
{
  Lattice *lt = (Lattice *)id;
  BLO_read_struct_array(reader, BPoint, lt->pntsu * lt->pntsv * lt->pntsw, &lt->def);

  BLO_read_struct_array(reader, MDeformVert, lt->pntsu * lt->pntsv * lt->pntsw, &lt->dvert);
  BKE_defvert_blend_read(reader, lt->pntsu * lt->pntsv * lt->pntsw, lt->dvert);
  BLO_read_struct_list(reader, bDeformGroup, &lt->vertex_group_names);

  lt->editlatt = nullptr;
  lt->batch_cache = nullptr;
}

IDTypeInfo IDType_ID_LT = {
    /*id_code*/ Lattice::id_type,
    /*id_filter*/ FILTER_ID_LT,
    /*dependencies_id_types*/ FILTER_ID_KE,
    /*main_listbase_index*/ INDEX_ID_LT,
    /*struct_size*/ sizeof(Lattice),
    /*name*/ "Lattice",
    /*name_plural*/ N_("lattices"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_LATTICE,
    /*flags*/ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /*init_data*/ lattice_init_data,
    /*copy_data*/ lattice_copy_data,
    /*free_data*/ lattice_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ lattice_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ lattice_blend_write,
    /*blend_read_data*/ lattice_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

int BKE_lattice_index_from_uvw(const Lattice *lt, const int u, const int v, const int w)
{
  const int totu = lt->pntsu;
  const int totv = lt->pntsv;

  return (w * (totu * totv) + (v * totu) + u);
}

void BKE_lattice_index_to_uvw(const Lattice *lt, const int index, int *r_u, int *r_v, int *r_w)
{
  const int totu = lt->pntsu;
  const int totv = lt->pntsv;

  *r_u = (index % totu);
  *r_v = (index / totu) % totv;
  *r_w = (index / (totu * totv));
}

int BKE_lattice_index_flip(
    const Lattice *lt, const int index, const bool flip_u, const bool flip_v, const bool flip_w)
{
  int u, v, w;

  BKE_lattice_index_to_uvw(lt, index, &u, &v, &w);

  if (flip_u) {
    u = (lt->pntsu - 1) - u;
  }

  if (flip_v) {
    v = (lt->pntsv - 1) - v;
  }

  if (flip_w) {
    w = (lt->pntsw - 1) - w;
  }

  return BKE_lattice_index_from_uvw(lt, u, v, w);
}

void BKE_lattice_bitmap_from_flag(const Lattice *lt,
                                  BLI_bitmap *bitmap,
                                  const uint8_t flag,
                                  const bool clear,
                                  const bool respecthide)
{
  const uint tot = lt->pntsu * lt->pntsv * lt->pntsw;
  BPoint *bp;

  bp = lt->def;
  for (int i = 0; i < tot; i++, bp++) {
    if ((bp->f1 & flag) && (!respecthide || !bp->hide)) {
      BLI_BITMAP_ENABLE(bitmap, i);
    }
    else {
      if (clear) {
        BLI_BITMAP_DISABLE(bitmap, i);
      }
    }
  }
}

void calc_lat_fudu(int flag, int res, float *r_fu, float *r_du)
{
  if (res == 1) {
    *r_fu = 0.0;
    *r_du = 0.0;
  }
  else if (flag & LT_GRID) {
    *r_fu = -0.5f * (res - 1);
    *r_du = 1.0f;
  }
  else {
    *r_fu = -1.0f;
    *r_du = 2.0f / (res - 1);
  }
}

void BKE_lattice_resize(Lattice *lt, int u_new, int v_new, int w_new, Object *lt_ob)
{
  BPoint *bp;
  int i, u, v, w;
  float fu, fv, fw, uc, vc, wc, du = 0.0, dv = 0.0, dw = 0.0;
  float *co, (*vert_coords)[3] = nullptr;

  /* vertex weight groups are just freed all for now */
  if (lt->dvert) {
    BKE_defvert_array_free(lt->dvert, lt->pntsu * lt->pntsv * lt->pntsw);
    lt->dvert = nullptr;
  }

  while (u_new * v_new * w_new > 32000) {
    if (u_new >= v_new && u_new >= w_new) {
      u_new--;
    }
    else if (v_new >= u_new && v_new >= w_new) {
      v_new--;
    }
    else {
      w_new--;
    }
  }

  vert_coords = MEM_malloc_arrayN<float[3]>(size_t(u_new) * size_t(v_new) * size_t(w_new),
                                            "tmp_vcos");

  calc_lat_fudu(lt->flag, u_new, &fu, &du);
  calc_lat_fudu(lt->flag, v_new, &fv, &dv);
  calc_lat_fudu(lt->flag, w_new, &fw, &dw);

  /* If old size is different than resolution changed in interface,
   * try to do clever reinitialize of points. Pretty simply idea, we just
   * deform new verts by old lattice, but scaling them to match old
   * size first.
   */
  if (lt_ob) {
    const float default_size = 1.0;

    if (u_new != 1) {
      fu = -default_size / 2.0;
      du = default_size / (u_new - 1);
    }

    if (v_new != 1) {
      fv = -default_size / 2.0;
      dv = default_size / (v_new - 1);
    }

    if (w_new != 1) {
      fw = -default_size / 2.0;
      dw = default_size / (w_new - 1);
    }
  }

  co = vert_coords[0];
  for (w = 0, wc = fw; w < w_new; w++, wc += dw) {
    for (v = 0, vc = fv; v < v_new; v++, vc += dv) {
      for (u = 0, uc = fu; u < u_new; u++, co += 3, uc += du) {
        co[0] = uc;
        co[1] = vc;
        co[2] = wc;
      }
    }
  }

  if (lt_ob) {
    float mat[4][4];
    int typeu = lt->typeu, typev = lt->typev, typew = lt->typew;

    /* works best if we force to linear type (endpoints match) */
    lt->typeu = lt->typev = lt->typew = KEY_LINEAR;

    if (lt_ob->runtime->curve_cache) {
      /* prevent using deformed locations */
      BKE_displist_free(&lt_ob->runtime->curve_cache->disp);
    }

    copy_m4_m4(mat, lt_ob->object_to_world().ptr());
    unit_m4(lt_ob->runtime->object_to_world.ptr());
    BKE_lattice_deform_coords(
        lt_ob, nullptr, vert_coords, u_new * v_new * w_new, 0, nullptr, 1.0f);
    copy_m4_m4(lt_ob->runtime->object_to_world.ptr(), mat);

    lt->typeu = typeu;
    lt->typev = typev;
    lt->typew = typew;
  }

  lt->fu = fu;
  lt->fv = fv;
  lt->fw = fw;
  lt->du = du;
  lt->dv = dv;
  lt->dw = dw;

  lt->pntsu = u_new;
  lt->pntsv = v_new;
  lt->pntsw = w_new;

  lt->actbp = LT_ACTBP_NONE;
  MEM_freeN(lt->def);
  lt->def = MEM_calloc_arrayN<BPoint>(size_t(lt->pntsu) * size_t(lt->pntsv) * size_t(lt->pntsw),
                                      "lattice bp");

  bp = lt->def;

  for (i = 0; i < lt->pntsu * lt->pntsv * lt->pntsw; i++, bp++) {
    copy_v3_v3(bp->vec, vert_coords[i]);
  }

  MEM_freeN(vert_coords);
}

Lattice *BKE_lattice_add(Main *bmain, const char *name)
{
  Lattice *lt;

  lt = BKE_id_new<Lattice>(bmain, name);

  return lt;
}

static BPoint *latt_bp(Lattice *lt, int u, int v, int w)
{
  return &lt->def[BKE_lattice_index_from_uvw(lt, u, v, w)];
}

void outside_lattice(Lattice *lt)
{
  BPoint *bp, *bp1, *bp2;
  int u, v, w;
  float fac1, du = 0.0, dv = 0.0, dw = 0.0;

  if (lt->flag & LT_OUTSIDE) {
    bp = lt->def;

    if (lt->pntsu > 1) {
      du = 1.0f / float(lt->pntsu - 1);
    }
    if (lt->pntsv > 1) {
      dv = 1.0f / float(lt->pntsv - 1);
    }
    if (lt->pntsw > 1) {
      dw = 1.0f / float(lt->pntsw - 1);
    }

    for (w = 0; w < lt->pntsw; w++) {

      for (v = 0; v < lt->pntsv; v++) {

        for (u = 0; u < lt->pntsu; u++, bp++) {
          if (u == 0 || v == 0 || w == 0 || u == lt->pntsu - 1 || v == lt->pntsv - 1 ||
              w == lt->pntsw - 1)
          {
            /* pass */
          }
          else {
            bp->hide = 1;
            bp->f1 &= ~SELECT;

            /* U extrema. */
            bp1 = latt_bp(lt, 0, v, w);
            bp2 = latt_bp(lt, lt->pntsu - 1, v, w);

            fac1 = du * u;
            bp->vec[0] = (1.0f - fac1) * bp1->vec[0] + fac1 * bp2->vec[0];
            bp->vec[1] = (1.0f - fac1) * bp1->vec[1] + fac1 * bp2->vec[1];
            bp->vec[2] = (1.0f - fac1) * bp1->vec[2] + fac1 * bp2->vec[2];

            /* V extrema. */
            bp1 = latt_bp(lt, u, 0, w);
            bp2 = latt_bp(lt, u, lt->pntsv - 1, w);

            fac1 = dv * v;
            bp->vec[0] += (1.0f - fac1) * bp1->vec[0] + fac1 * bp2->vec[0];
            bp->vec[1] += (1.0f - fac1) * bp1->vec[1] + fac1 * bp2->vec[1];
            bp->vec[2] += (1.0f - fac1) * bp1->vec[2] + fac1 * bp2->vec[2];

            /* W extrema. */
            bp1 = latt_bp(lt, u, v, 0);
            bp2 = latt_bp(lt, u, v, lt->pntsw - 1);

            fac1 = dw * w;
            bp->vec[0] += (1.0f - fac1) * bp1->vec[0] + fac1 * bp2->vec[0];
            bp->vec[1] += (1.0f - fac1) * bp1->vec[1] + fac1 * bp2->vec[1];
            bp->vec[2] += (1.0f - fac1) * bp1->vec[2] + fac1 * bp2->vec[2];

            mul_v3_fl(bp->vec, 1.0f / 3.0f);
          }
        }
      }
    }
  }
  else {
    bp = lt->def;

    for (w = 0; w < lt->pntsw; w++) {
      for (v = 0; v < lt->pntsv; v++) {
        for (u = 0; u < lt->pntsu; u++, bp++) {
          bp->hide = 0;
        }
      }
    }
  }
}

void BKE_lattice_vert_coords_get(const Lattice *lt, MutableSpan<float3> vert_coords)
{
  const int vert_len = lt->pntsu * lt->pntsv * lt->pntsw;
  for (int i = 0; i < vert_len; i++) {
    vert_coords[i] = lt->def[i].vec;
  }
}

Array<float3> BKE_lattice_vert_coords_alloc(const Lattice *lt)
{
  const int vert_len = lt->pntsu * lt->pntsv * lt->pntsw;
  Array<float3> vert_coords(vert_len);
  BKE_lattice_vert_coords_get(lt, vert_coords);
  return vert_coords;
}

void BKE_lattice_vert_coords_apply_with_mat4(Lattice *lt,
                                             const Span<float3> vert_coords,
                                             const float4x4 &transform)
{
  int i, numVerts = lt->pntsu * lt->pntsv * lt->pntsw;
  for (i = 0; i < numVerts; i++) {
    mul_v3_m4v3(lt->def[i].vec, transform.ptr(), vert_coords[i]);
  }
}

void BKE_lattice_vert_coords_apply(Lattice *lt, const Span<float3> vert_coords)
{
  const int vert_len = lt->pntsu * lt->pntsv * lt->pntsw;
  for (int i = 0; i < vert_len; i++) {
    copy_v3_v3(lt->def[i].vec, vert_coords[i]);
  }
}

void BKE_lattice_modifiers_calc(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  BKE_object_free_derived_caches(ob);
  if (ob->runtime->curve_cache == nullptr) {
    ob->runtime->curve_cache = MEM_callocN<CurveCache>("CurveCache for lattice");
  }

  Lattice *lt = static_cast<Lattice *>(ob->data);
  VirtualModifierData virtual_modifier_data;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtual_modifier_data);
  Array<float3> vert_coords;
  const bool is_editmode = (lt->editlatt != nullptr);
  const ModifierEvalContext mectx = {depsgraph, ob, ModifierApplyFlag(0)};

  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));

    if (!(mti->flags & eModifierTypeFlag_AcceptsVertexCosOnly)) {
      continue;
    }
    if (!(md->mode & eModifierMode_Realtime)) {
      continue;
    }
    if (is_editmode && !(md->mode & eModifierMode_Editmode)) {
      continue;
    }
    if (mti->is_disabled && mti->is_disabled(scene, md, false)) {
      continue;
    }
    if (mti->type != ModifierTypeType::OnlyDeform) {
      continue;
    }

    if (vert_coords.is_empty()) {
      /* Get either the edit-mode or regular lattice, whichever is in use now. */
      const Lattice *effective_lattice = BKE_object_get_lattice(ob);
      vert_coords = BKE_lattice_vert_coords_alloc(effective_lattice);
    }

    mti->deform_verts(md, &mectx, nullptr, vert_coords);
  }

  if (vert_coords.is_empty()) {
    return;
  }

  Lattice *lt_eval = BKE_object_get_evaluated_lattice(ob);
  if (lt_eval == nullptr) {
    BKE_id_copy_ex(nullptr, &lt->id, (ID **)&lt_eval, LIB_ID_COPY_LOCALIZE);
    BKE_object_eval_assign_data(ob, &lt_eval->id, true);
  }

  BKE_lattice_vert_coords_apply(lt_eval, vert_coords);
}

MDeformVert *BKE_lattice_deform_verts_get(const Object *oblatt)
{
  BLI_assert(oblatt->type == OB_LATTICE);
  Lattice *lt = BKE_object_get_lattice(oblatt);
  return lt->dvert;
}

BPoint *BKE_lattice_active_point_get(Lattice *lt)
{
  BLI_assert(GS(lt->id.name) == ID_LT);

  if (lt->editlatt) {
    lt = lt->editlatt->latt;
  }

  BLI_assert(lt->actbp < lt->pntsu * lt->pntsv * lt->pntsw);

  if ((lt->actbp != LT_ACTBP_NONE) && (lt->actbp < lt->pntsu * lt->pntsv * lt->pntsw)) {
    return &lt->def[lt->actbp];
  }

  return nullptr;
}

void BKE_lattice_center_median(Lattice *lt, float cent[3])
{
  int i, numVerts;

  if (lt->editlatt) {
    lt = lt->editlatt->latt;
  }
  numVerts = lt->pntsu * lt->pntsv * lt->pntsw;

  zero_v3(cent);

  for (i = 0; i < numVerts; i++) {
    add_v3_v3(cent, lt->def[i].vec);
  }

  mul_v3_fl(cent, 1.0f / float(numVerts));
}

std::optional<blender::Bounds<blender::float3>> BKE_lattice_minmax(const Lattice *lt)
{
  int i, numVerts;

  if (lt->editlatt) {
    lt = lt->editlatt->latt;
  }
  numVerts = lt->pntsu * lt->pntsv * lt->pntsw;
  if (numVerts == 0) {
    return std::nullopt;
  }

  blender::float3 min = lt->def[0].vec;
  blender::float3 max = lt->def[0].vec;
  for (i = 0; i < numVerts; i++) {
    minmax_v3v3_v3(min, max, lt->def[i].vec);
  }
  return blender::Bounds<blender::float3>{min, max};
}

void BKE_lattice_transform(Lattice *lt, const float mat[4][4], bool do_keys)
{
  BPoint *bp = lt->def;
  int i = lt->pntsu * lt->pntsv * lt->pntsw;

  while (i--) {
    mul_m4_v3(mat, bp->vec);
    bp++;
  }

  if (do_keys && lt->key) {
    LISTBASE_FOREACH (KeyBlock *, kb, &lt->key->block) {
      float *fp = static_cast<float *>(kb->data);
      for (i = kb->totelem; i--; fp += 3) {
        mul_m4_v3(mat, fp);
      }
    }
  }
}

void BKE_lattice_translate(Lattice *lt, const float offset[3], bool do_keys)
{
  int i, numVerts;

  numVerts = lt->pntsu * lt->pntsv * lt->pntsw;

  if (lt->def) {
    for (i = 0; i < numVerts; i++) {
      add_v3_v3(lt->def[i].vec, offset);
    }
  }

  if (lt->editlatt) {
    for (i = 0; i < numVerts; i++) {
      add_v3_v3(lt->editlatt->latt->def[i].vec, offset);
    }
  }

  if (do_keys && lt->key) {
    LISTBASE_FOREACH (KeyBlock *, kb, &lt->key->block) {
      float *fp = static_cast<float *>(kb->data);
      for (i = kb->totelem; i--; fp += 3) {
        add_v3_v3(fp, offset);
      }
    }
  }
}

bool BKE_lattice_is_any_selected(const Lattice *lt)
{
  /* Intentionally don't handle 'lt->editlatt' (caller must do this). */
  const BPoint *bp = lt->def;
  int a = lt->pntsu * lt->pntsv * lt->pntsw;
  while (a--) {
    if (bp->hide == 0) {
      if (bp->f1 & SELECT) {
        return true;
      }
    }
    bp++;
  }
  return false;
}

/* **** Depsgraph evaluation **** */

void BKE_lattice_eval_geometry(Depsgraph * /*depsgraph*/, Lattice * /*latt*/) {}

/* Draw Engine */

void (*BKE_lattice_batch_cache_dirty_tag_cb)(Lattice *lt, int mode) = nullptr;
void (*BKE_lattice_batch_cache_free_cb)(Lattice *lt) = nullptr;

void BKE_lattice_batch_cache_dirty_tag(Lattice *lt, int mode)
{
  if (lt->batch_cache) {
    BKE_lattice_batch_cache_dirty_tag_cb(lt, mode);
  }
}
void BKE_lattice_batch_cache_free(Lattice *lt)
{
  if (lt->batch_cache) {
    BKE_lattice_batch_cache_free_cb(lt);
  }
}
