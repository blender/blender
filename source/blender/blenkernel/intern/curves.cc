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

/** \file
 * \ingroup bke
 */

#include <cmath>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_curves_types.h"
#include "DNA_defaults.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_vec_types.hh"
#include "BLI_rand.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_anim_data.h"
#include "BKE_curves.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "BLT_translation.h"

#include "DEG_depsgraph_query.h"

#include "BLO_read_write.h"

using blender::float3;
using blender::IndexRange;
using blender::MutableSpan;
using blender::RandomNumberGenerator;

static const char *ATTR_POSITION = "position";
static const char *ATTR_RADIUS = "radius";

static void curves_random(Curves *curves);

static void curves_init_data(ID *id)
{
  Curves *curves = (Curves *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(curves, id));

  MEMCPY_STRUCT_AFTER(curves, DNA_struct_default_get(Curves), id);

  CustomData_reset(&curves->geometry.point_data);
  CustomData_reset(&curves->geometry.curve_data);

  CustomData_add_layer_named(&curves->geometry.point_data,
                             CD_PROP_FLOAT3,
                             CD_CALLOC,
                             nullptr,
                             curves->geometry.point_size,
                             ATTR_POSITION);
  CustomData_add_layer_named(&curves->geometry.point_data,
                             CD_PROP_FLOAT,
                             CD_CALLOC,
                             nullptr,
                             curves->geometry.point_size,
                             ATTR_RADIUS);

  BKE_curves_update_customdata_pointers(curves);

  curves_random(curves);
}

static void curves_copy_data(Main *UNUSED(bmain), ID *id_dst, const ID *id_src, const int flag)
{
  Curves *curves_dst = (Curves *)id_dst;
  const Curves *curves_src = (const Curves *)id_src;
  curves_dst->mat = static_cast<Material **>(MEM_dupallocN(curves_src->mat));

  curves_dst->geometry.point_size = curves_src->geometry.point_size;
  curves_dst->geometry.curve_size = curves_src->geometry.curve_size;

  const eCDAllocType alloc_type = (flag & LIB_ID_COPY_CD_REFERENCE) ? CD_REFERENCE : CD_DUPLICATE;
  CustomData_copy(&curves_src->geometry.point_data,
                  &curves_dst->geometry.point_data,
                  CD_MASK_ALL,
                  alloc_type,
                  curves_dst->geometry.point_size);
  CustomData_copy(&curves_src->geometry.curve_data,
                  &curves_dst->geometry.curve_data,
                  CD_MASK_ALL,
                  alloc_type,
                  curves_dst->geometry.curve_size);
  BKE_curves_update_customdata_pointers(curves_dst);

  curves_dst->geometry.offsets = static_cast<int *>(MEM_dupallocN(curves_src->geometry.offsets));

  curves_dst->batch_cache = nullptr;
}

static void curves_free_data(ID *id)
{
  Curves *curves = (Curves *)id;
  BKE_animdata_free(&curves->id, false);

  BKE_curves_batch_cache_free(curves);

  CustomData_free(&curves->geometry.point_data, curves->geometry.point_size);
  CustomData_free(&curves->geometry.curve_data, curves->geometry.curve_size);

  MEM_SAFE_FREE(curves->geometry.offsets);

  MEM_SAFE_FREE(curves->mat);
}

static void curves_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Curves *curves = (Curves *)id;
  for (int i = 0; i < curves->totcol; i++) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, curves->mat[i], IDWALK_CB_USER);
  }
}

static void curves_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Curves *curves = (Curves *)id;

  CustomDataLayer *players = nullptr, players_buff[CD_TEMP_CHUNK_SIZE];
  CustomDataLayer *clayers = nullptr, clayers_buff[CD_TEMP_CHUNK_SIZE];
  CustomData_blend_write_prepare(
      &curves->geometry.point_data, &players, players_buff, ARRAY_SIZE(players_buff));
  CustomData_blend_write_prepare(
      &curves->geometry.curve_data, &clayers, clayers_buff, ARRAY_SIZE(clayers_buff));

  /* Write LibData */
  BLO_write_id_struct(writer, Curves, id_address, &curves->id);
  BKE_id_blend_write(writer, &curves->id);

  /* Direct data */
  CustomData_blend_write(writer,
                         &curves->geometry.point_data,
                         players,
                         curves->geometry.point_size,
                         CD_MASK_ALL,
                         &curves->id);
  CustomData_blend_write(writer,
                         &curves->geometry.curve_data,
                         clayers,
                         curves->geometry.curve_size,
                         CD_MASK_ALL,
                         &curves->id);

  BLO_write_int32_array(writer, curves->geometry.curve_size + 1, curves->geometry.offsets);

  BLO_write_pointer_array(writer, curves->totcol, curves->mat);
  if (curves->adt) {
    BKE_animdata_blend_write(writer, curves->adt);
  }

  /* Remove temporary data. */
  if (players && players != players_buff) {
    MEM_freeN(players);
  }
  if (clayers && clayers != clayers_buff) {
    MEM_freeN(clayers);
  }
}

static void curves_blend_read_data(BlendDataReader *reader, ID *id)
{
  Curves *curves = (Curves *)id;
  BLO_read_data_address(reader, &curves->adt);
  BKE_animdata_blend_read_data(reader, curves->adt);

  /* Geometry */
  CustomData_blend_read(reader, &curves->geometry.point_data, curves->geometry.point_size);
  CustomData_blend_read(reader, &curves->geometry.curve_data, curves->geometry.point_size);
  BKE_curves_update_customdata_pointers(curves);

  BLO_read_int32_array(reader, curves->geometry.curve_size + 1, &curves->geometry.offsets);

  /* Materials */
  BLO_read_pointer_array(reader, (void **)&curves->mat);
}

static void curves_blend_read_lib(BlendLibReader *reader, ID *id)
{
  Curves *curves = (Curves *)id;
  for (int a = 0; a < curves->totcol; a++) {
    BLO_read_id_address(reader, curves->id.lib, &curves->mat[a]);
  }
}

static void curves_blend_read_expand(BlendExpander *expander, ID *id)
{
  Curves *curves = (Curves *)id;
  for (int a = 0; a < curves->totcol; a++) {
    BLO_expand(expander, curves->mat[a]);
  }
}

IDTypeInfo IDType_ID_CV = {
    /*id_code */ ID_CV,
    /*id_filter */ FILTER_ID_CV,
    /*main_listbase_index */ INDEX_ID_CV,
    /*struct_size */ sizeof(Curves),
    /*name */ "Hair Curves",
    /*name_plural */ "Hair Curves",
    /*translation_context */ BLT_I18NCONTEXT_ID_CURVES,
    /*flags */ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info */ nullptr,

    /*init_data */ curves_init_data,
    /*copy_data */ curves_copy_data,
    /*free_data */ curves_free_data,
    /*make_local */ nullptr,
    /*foreach_id */ curves_foreach_id,
    /*foreach_cache */ nullptr,
    /*foreach_path */ nullptr,
    /*owner_get */ nullptr,

    /*blend_write */ curves_blend_write,
    /*blend_read_data */ curves_blend_read_data,
    /*blend_read_lib */ curves_blend_read_lib,
    /*blend_read_expand */ curves_blend_read_expand,

    /*blend_read_undo_preserve */ nullptr,

    /*lib_override_apply_post */ nullptr,
};

static void curves_random(Curves *curves)
{
  CurvesGeometry &geometry = curves->geometry;
  const int numpoints = 8;

  geometry.curve_size = 500;

  geometry.curve_size = 500;
  geometry.point_size = geometry.curve_size * numpoints;

  curves->geometry.offsets = (int *)MEM_calloc_arrayN(
      curves->geometry.curve_size + 1, sizeof(int), __func__);
  CustomData_realloc(&geometry.point_data, geometry.point_size);
  CustomData_realloc(&geometry.curve_data, geometry.curve_size);
  BKE_curves_update_customdata_pointers(curves);

  MutableSpan<int> offsets{geometry.offsets, geometry.curve_size + 1};
  MutableSpan<float3> positions{(float3 *)geometry.position, geometry.point_size};
  MutableSpan<float> radii{geometry.radius, geometry.point_size};

  for (const int i : offsets.index_range()) {
    geometry.offsets[i] = numpoints * i;
  }

  RandomNumberGenerator rng;

  for (int i = 0; i < geometry.curve_size; i++) {
    const IndexRange curve_range(offsets[i], offsets[i + 1] - offsets[i]);
    MutableSpan<float3> curve_positions = positions.slice(curve_range);
    MutableSpan<float> curve_radii = radii.slice(curve_range);

    const float theta = 2.0f * M_PI * rng.get_float();
    const float phi = saacosf(2.0f * rng.get_float() - 1.0f);

    float3 no = {std::sin(theta) * std::sin(phi), std::cos(theta) * std::sin(phi), std::cos(phi)};
    no = blender::math::normalize(no);

    float3 co = no;
    for (int key = 0; key < numpoints; key++) {
      float t = key / (float)(numpoints - 1);
      curve_positions[key] = co;
      curve_radii[key] = 0.02f * (1.0f - t);

      float3 offset = float3(rng.get_float(), rng.get_float(), rng.get_float()) * 2.0f - 1.0f;
      co += (offset + no) / numpoints;
    }
  }
}

void *BKE_curves_add(Main *bmain, const char *name)
{
  Curves *curves = static_cast<Curves *>(BKE_id_new(bmain, ID_CV, name));

  return curves;
}

BoundBox *BKE_curves_boundbox_get(Object *ob)
{
  BLI_assert(ob->type == OB_CURVES);
  Curves *curves = static_cast<Curves *>(ob->data);

  if (ob->runtime.bb != nullptr && (ob->runtime.bb->flag & BOUNDBOX_DIRTY) == 0) {
    return ob->runtime.bb;
  }

  if (ob->runtime.bb == nullptr) {
    ob->runtime.bb = MEM_cnew<BoundBox>(__func__);

    float min[3], max[3];
    INIT_MINMAX(min, max);

    float(*curves_co)[3] = curves->geometry.position;
    float *curves_radius = curves->geometry.radius;
    for (int a = 0; a < curves->geometry.point_size; a++) {
      float *co = curves_co[a];
      float radius = (curves_radius) ? curves_radius[a] : 0.0f;
      const float co_min[3] = {co[0] - radius, co[1] - radius, co[2] - radius};
      const float co_max[3] = {co[0] + radius, co[1] + radius, co[2] + radius};
      DO_MIN(co_min, min);
      DO_MAX(co_max, max);
    }

    BKE_boundbox_init_from_minmax(ob->runtime.bb, min, max);
  }

  return ob->runtime.bb;
}

void BKE_curves_update_customdata_pointers(Curves *curves)
{
  curves->geometry.position = (float(*)[3])CustomData_get_layer_named(
      &curves->geometry.point_data, CD_PROP_FLOAT3, ATTR_POSITION);
  curves->geometry.radius = (float *)CustomData_get_layer_named(
      &curves->geometry.point_data, CD_PROP_FLOAT, ATTR_RADIUS);
}

bool BKE_curves_customdata_required(Curves *UNUSED(curves), CustomDataLayer *layer)
{
  return layer->type == CD_PROP_FLOAT3 && STREQ(layer->name, ATTR_POSITION);
}

/* Dependency Graph */

Curves *BKE_curves_new_for_eval(const Curves *curves_src, int totpoint, int totcurve)
{
  Curves *curves_dst = static_cast<Curves *>(BKE_id_new_nomain(ID_CV, nullptr));

  STRNCPY(curves_dst->id.name, curves_src->id.name);
  curves_dst->mat = static_cast<Material **>(MEM_dupallocN(curves_src->mat));
  curves_dst->totcol = curves_src->totcol;

  curves_dst->geometry.point_size = totpoint;
  curves_dst->geometry.curve_size = totcurve;
  CustomData_copy(&curves_src->geometry.point_data,
                  &curves_dst->geometry.point_data,
                  CD_MASK_ALL,
                  CD_CALLOC,
                  totpoint);
  CustomData_copy(&curves_src->geometry.curve_data,
                  &curves_dst->geometry.curve_data,
                  CD_MASK_ALL,
                  CD_CALLOC,
                  totcurve);
  BKE_curves_update_customdata_pointers(curves_dst);

  return curves_dst;
}

Curves *BKE_curves_copy_for_eval(Curves *curves_src, bool reference)
{
  int flags = LIB_ID_COPY_LOCALIZE;

  if (reference) {
    flags |= LIB_ID_COPY_CD_REFERENCE;
  }

  Curves *result = (Curves *)BKE_id_copy_ex(nullptr, &curves_src->id, nullptr, flags);
  return result;
}

static Curves *curves_evaluate_modifiers(struct Depsgraph *depsgraph,
                                         struct Scene *scene,
                                         Object *object,
                                         Curves *curves_input)
{
  Curves *curves = curves_input;

  /* Modifier evaluation modes. */
  const bool use_render = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  const int required_mode = use_render ? eModifierMode_Render : eModifierMode_Realtime;
  ModifierApplyFlag apply_flag = use_render ? MOD_APPLY_RENDER : MOD_APPLY_USECACHE;
  const ModifierEvalContext mectx = {depsgraph, object, apply_flag};

  /* Get effective list of modifiers to execute. Some effects like shape keys
   * are added as virtual modifiers before the user created modifiers. */
  VirtualModifierData virtualModifierData;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(object, &virtualModifierData);

  /* Evaluate modifiers. */
  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(static_cast<ModifierType>(md->type));

    if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
      continue;
    }

    if ((mti->type == eModifierTypeType_OnlyDeform) &&
        (mti->flags & eModifierTypeFlag_AcceptsVertexCosOnly)) {
      /* Ensure we are not modifying the input. */
      if (curves == curves_input) {
        curves = BKE_curves_copy_for_eval(curves, true);
      }

      /* Ensure we are not overwriting referenced data. */
      CustomData_duplicate_referenced_layer_named(&curves->geometry.point_data,
                                                  CD_PROP_FLOAT3,
                                                  ATTR_POSITION,
                                                  curves->geometry.point_size);
      BKE_curves_update_customdata_pointers(curves);

      /* Created deformed coordinates array on demand. */
      mti->deformVerts(
          md, &mectx, nullptr, curves->geometry.position, curves->geometry.point_size);
    }
  }

  return curves;
}

void BKE_curves_data_update(struct Depsgraph *depsgraph, struct Scene *scene, Object *object)
{
  /* Free any evaluated data and restore original data. */
  BKE_object_free_derived_caches(object);

  /* Evaluate modifiers. */
  Curves *curves = static_cast<Curves *>(object->data);
  Curves *curves_eval = curves_evaluate_modifiers(depsgraph, scene, object, curves);

  /* Assign evaluated object. */
  const bool is_owned = (curves != curves_eval);
  BKE_object_eval_assign_data(object, &curves_eval->id, is_owned);
}

/* Draw Cache */

void (*BKE_curves_batch_cache_dirty_tag_cb)(Curves *curves, int mode) = nullptr;
void (*BKE_curves_batch_cache_free_cb)(Curves *curves) = nullptr;

void BKE_curves_batch_cache_dirty_tag(Curves *curves, int mode)
{
  if (curves->batch_cache) {
    BKE_curves_batch_cache_dirty_tag_cb(curves, mode);
  }
}

void BKE_curves_batch_cache_free(Curves *curves)
{
  if (curves->batch_cache) {
    BKE_curves_batch_cache_free_cb(curves);
  }
}
