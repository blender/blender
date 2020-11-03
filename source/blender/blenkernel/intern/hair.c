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

#include "MEM_guardedalloc.h"

#include "DNA_defaults.h"
#include "DNA_hair_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_anim_data.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_hair.h"
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

static const char *HAIR_ATTR_POSITION = "Position";
static const char *HAIR_ATTR_RADIUS = "Radius";

/* Hair datablock */

static void hair_random(Hair *hair);

static void hair_init_data(ID *id)
{
  Hair *hair = (Hair *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(hair, id));

  MEMCPY_STRUCT_AFTER(hair, DNA_struct_default_get(Hair), id);

  CustomData_reset(&hair->pdata);
  CustomData_reset(&hair->cdata);

  CustomData_add_layer_named(
      &hair->pdata, CD_PROP_FLOAT3, CD_CALLOC, NULL, hair->totpoint, HAIR_ATTR_POSITION);
  CustomData_add_layer_named(
      &hair->pdata, CD_PROP_FLOAT, CD_CALLOC, NULL, hair->totpoint, HAIR_ATTR_RADIUS);
  CustomData_add_layer(&hair->cdata, CD_HAIRCURVE, CD_CALLOC, NULL, hair->totcurve);
  BKE_hair_update_customdata_pointers(hair);

  hair_random(hair);
}

static void hair_copy_data(Main *UNUSED(bmain), ID *id_dst, const ID *id_src, const int flag)
{
  Hair *hair_dst = (Hair *)id_dst;
  const Hair *hair_src = (const Hair *)id_src;
  hair_dst->mat = MEM_dupallocN(hair_dst->mat);

  const eCDAllocType alloc_type = (flag & LIB_ID_COPY_CD_REFERENCE) ? CD_REFERENCE : CD_DUPLICATE;
  CustomData_copy(&hair_src->pdata, &hair_dst->pdata, CD_MASK_ALL, alloc_type, hair_dst->totpoint);
  CustomData_copy(&hair_src->cdata, &hair_dst->cdata, CD_MASK_ALL, alloc_type, hair_dst->totcurve);
  BKE_hair_update_customdata_pointers(hair_dst);

  hair_dst->batch_cache = NULL;
}

static void hair_free_data(ID *id)
{
  Hair *hair = (Hair *)id;
  BKE_animdata_free(&hair->id, false);

  BKE_hair_batch_cache_free(hair);

  CustomData_free(&hair->pdata, hair->totpoint);
  CustomData_free(&hair->cdata, hair->totcurve);

  MEM_SAFE_FREE(hair->mat);
}

static void hair_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Hair *hair = (Hair *)id;
  for (int i = 0; i < hair->totcol; i++) {
    BKE_LIB_FOREACHID_PROCESS(data, hair->mat[i], IDWALK_CB_USER);
  }
}

static void hair_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Hair *hair = (Hair *)id;
  if (hair->id.us > 0 || BLO_write_is_undo(writer)) {
    CustomDataLayer *players = NULL, players_buff[CD_TEMP_CHUNK_SIZE];
    CustomDataLayer *clayers = NULL, clayers_buff[CD_TEMP_CHUNK_SIZE];
    CustomData_blend_write_prepare(&hair->pdata, &players, players_buff, ARRAY_SIZE(players_buff));
    CustomData_blend_write_prepare(&hair->cdata, &clayers, clayers_buff, ARRAY_SIZE(clayers_buff));

    /* Write LibData */
    BLO_write_id_struct(writer, Hair, id_address, &hair->id);
    BKE_id_blend_write(writer, &hair->id);

    /* Direct data */
    CustomData_blend_write(writer, &hair->pdata, players, hair->totpoint, CD_MASK_ALL, &hair->id);
    CustomData_blend_write(writer, &hair->cdata, clayers, hair->totcurve, CD_MASK_ALL, &hair->id);

    BLO_write_pointer_array(writer, hair->totcol, hair->mat);
    if (hair->adt) {
      BKE_animdata_blend_write(writer, hair->adt);
    }

    /* Remove temporary data. */
    if (players && players != players_buff) {
      MEM_freeN(players);
    }
    if (clayers && clayers != clayers_buff) {
      MEM_freeN(clayers);
    }
  }
}

static void hair_blend_read_data(BlendDataReader *reader, ID *id)
{
  Hair *hair = (Hair *)id;
  BLO_read_data_address(reader, &hair->adt);
  BKE_animdata_blend_read_data(reader, hair->adt);

  /* Geometry */
  CustomData_blend_read(reader, &hair->pdata, hair->totpoint);
  CustomData_blend_read(reader, &hair->cdata, hair->totcurve);
  BKE_hair_update_customdata_pointers(hair);

  /* Materials */
  BLO_read_pointer_array(reader, (void **)&hair->mat);
}

static void hair_blend_read_lib(BlendLibReader *reader, ID *id)
{
  Hair *hair = (Hair *)id;
  for (int a = 0; a < hair->totcol; a++) {
    BLO_read_id_address(reader, hair->id.lib, &hair->mat[a]);
  }
}

static void hair_blend_read_expand(BlendExpander *expander, ID *id)
{
  Hair *hair = (Hair *)id;
  for (int a = 0; a < hair->totcol; a++) {
    BLO_expand(expander, hair->mat[a]);
  }
}

IDTypeInfo IDType_ID_HA = {
    .id_code = ID_HA,
    .id_filter = FILTER_ID_HA,
    .main_listbase_index = INDEX_ID_HA,
    .struct_size = sizeof(Hair),
    .name = "Hair",
    .name_plural = "hairs",
    .translation_context = BLT_I18NCONTEXT_ID_HAIR,
    .flags = 0,

    .init_data = hair_init_data,
    .copy_data = hair_copy_data,
    .free_data = hair_free_data,
    .make_local = NULL,
    .foreach_id = hair_foreach_id,
    .foreach_cache = NULL,

    .blend_write = hair_blend_write,
    .blend_read_data = hair_blend_read_data,
    .blend_read_lib = hair_blend_read_lib,
    .blend_read_expand = hair_blend_read_expand,

    .blend_read_undo_preserve = NULL,
};

static void hair_random(Hair *hair)
{
  const int numpoints = 8;

  hair->totcurve = 500;
  hair->totpoint = hair->totcurve * numpoints;

  CustomData_realloc(&hair->pdata, hair->totpoint);
  CustomData_realloc(&hair->cdata, hair->totcurve);
  BKE_hair_update_customdata_pointers(hair);

  RNG *rng = BLI_rng_new(0);

  for (int i = 0; i < hair->totcurve; i++) {
    HairCurve *curve = &hair->curves[i];
    curve->firstpoint = i * numpoints;
    curve->numpoints = numpoints;

    float theta = 2.0f * M_PI * BLI_rng_get_float(rng);
    float phi = saacosf(2.0f * BLI_rng_get_float(rng) - 1.0f);

    float no[3] = {sinf(theta) * sinf(phi), cosf(theta) * sinf(phi), cosf(phi)};
    normalize_v3(no);

    float co[3];
    copy_v3_v3(co, no);

    float(*curve_co)[3] = hair->co + curve->firstpoint;
    float *curve_radius = hair->radius + curve->firstpoint;
    for (int key = 0; key < numpoints; key++) {
      float t = key / (float)(numpoints - 1);
      copy_v3_v3(curve_co[key], co);
      curve_radius[key] = 0.02f * (1.0f - t);

      float offset[3] = {2.0f * BLI_rng_get_float(rng) - 1.0f,
                         2.0f * BLI_rng_get_float(rng) - 1.0f,
                         2.0f * BLI_rng_get_float(rng) - 1.0f};
      add_v3_v3(offset, no);
      madd_v3_v3fl(co, offset, 1.0f / numpoints);
    }
  }

  BLI_rng_free(rng);
}

void *BKE_hair_add(Main *bmain, const char *name)
{
  Hair *hair = BKE_id_new(bmain, ID_HA, name);

  return hair;
}

BoundBox *BKE_hair_boundbox_get(Object *ob)
{
  BLI_assert(ob->type == OB_HAIR);
  Hair *hair = ob->data;

  if (ob->runtime.bb != NULL && (ob->runtime.bb->flag & BOUNDBOX_DIRTY) == 0) {
    return ob->runtime.bb;
  }

  if (ob->runtime.bb == NULL) {
    ob->runtime.bb = MEM_callocN(sizeof(BoundBox), "hair boundbox");

    float min[3], max[3];
    INIT_MINMAX(min, max);

    float(*hair_co)[3] = hair->co;
    float *hair_radius = hair->radius;
    for (int a = 0; a < hair->totpoint; a++) {
      float *co = hair_co[a];
      float radius = (hair_radius) ? hair_radius[a] : 0.0f;
      const float co_min[3] = {co[0] - radius, co[1] - radius, co[2] - radius};
      const float co_max[3] = {co[0] + radius, co[1] + radius, co[2] + radius};
      DO_MIN(co_min, min);
      DO_MAX(co_max, max);
    }

    BKE_boundbox_init_from_minmax(ob->runtime.bb, min, max);
  }

  return ob->runtime.bb;
}

void BKE_hair_update_customdata_pointers(Hair *hair)
{
  hair->co = CustomData_get_layer_named(&hair->pdata, CD_PROP_FLOAT3, HAIR_ATTR_POSITION);
  hair->radius = CustomData_get_layer_named(&hair->pdata, CD_PROP_FLOAT, HAIR_ATTR_RADIUS);
  hair->curves = CustomData_get_layer(&hair->cdata, CD_HAIRCURVE);
  hair->mapping = CustomData_get_layer(&hair->cdata, CD_HAIRMAPPING);
}

bool BKE_hair_customdata_required(Hair *UNUSED(hair), CustomDataLayer *layer)
{
  return layer->type == CD_PROP_FLOAT3 && STREQ(layer->name, HAIR_ATTR_POSITION);
}

/* Dependency Graph */

Hair *BKE_hair_new_for_eval(const Hair *hair_src, int totpoint, int totcurve)
{
  Hair *hair_dst = BKE_id_new_nomain(ID_HA, NULL);

  STRNCPY(hair_dst->id.name, hair_src->id.name);
  hair_dst->mat = MEM_dupallocN(hair_src->mat);
  hair_dst->totcol = hair_src->totcol;

  hair_dst->totpoint = totpoint;
  hair_dst->totcurve = totcurve;
  CustomData_copy(&hair_src->pdata, &hair_dst->pdata, CD_MASK_ALL, CD_CALLOC, totpoint);
  CustomData_copy(&hair_src->cdata, &hair_dst->cdata, CD_MASK_ALL, CD_CALLOC, totcurve);
  BKE_hair_update_customdata_pointers(hair_dst);

  return hair_dst;
}

Hair *BKE_hair_copy_for_eval(Hair *hair_src, bool reference)
{
  int flags = LIB_ID_COPY_LOCALIZE;

  if (reference) {
    flags |= LIB_ID_COPY_CD_REFERENCE;
  }

  Hair *result = (Hair *)BKE_id_copy_ex(NULL, &hair_src->id, NULL, flags);
  return result;
}

static Hair *hair_evaluate_modifiers(struct Depsgraph *depsgraph,
                                     struct Scene *scene,
                                     Object *object,
                                     Hair *hair_input)
{
  Hair *hair = hair_input;

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
    const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);

    if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
      continue;
    }

    if ((mti->type == eModifierTypeType_OnlyDeform) &&
        (mti->flags & eModifierTypeFlag_AcceptsVertexCosOnly)) {
      /* Ensure we are not modifying the input. */
      if (hair == hair_input) {
        hair = BKE_hair_copy_for_eval(hair, true);
      }

      /* Ensure we are not overwriting referenced data. */
      CustomData_duplicate_referenced_layer_named(
          &hair->pdata, CD_PROP_FLOAT3, HAIR_ATTR_POSITION, hair->totpoint);
      BKE_hair_update_customdata_pointers(hair);

      /* Created deformed coordinates array on demand. */
      mti->deformVerts(md, &mectx, NULL, hair->co, hair->totpoint);
    }
    else if (mti->modifyHair) {
      /* Ensure we are not modifying the input. */
      if (hair == hair_input) {
        hair = BKE_hair_copy_for_eval(hair, true);
      }

      Hair *hair_next = mti->modifyHair(md, &mectx, hair);

      if (hair_next && hair_next != hair) {
        /* If the modifier returned a new hair, release the old one. */
        if (hair != hair_input) {
          BKE_id_free(NULL, hair);
        }
        hair = hair_next;
      }
    }
  }

  return hair;
}

void BKE_hair_data_update(struct Depsgraph *depsgraph, struct Scene *scene, Object *object)
{
  /* Free any evaluated data and restore original data. */
  BKE_object_free_derived_caches(object);

  /* Evaluate modifiers. */
  Hair *hair = object->data;
  Hair *hair_eval = hair_evaluate_modifiers(depsgraph, scene, object, hair);

  /* Assign evaluated object. */
  const bool is_owned = (hair != hair_eval);
  BKE_object_eval_assign_data(object, &hair_eval->id, is_owned);
}

/* Draw Cache */
void (*BKE_hair_batch_cache_dirty_tag_cb)(Hair *hair, int mode) = NULL;
void (*BKE_hair_batch_cache_free_cb)(Hair *hair) = NULL;

void BKE_hair_batch_cache_dirty_tag(Hair *hair, int mode)
{
  if (hair->batch_cache) {
    BKE_hair_batch_cache_dirty_tag_cb(hair, mode);
  }
}

void BKE_hair_batch_cache_free(Hair *hair)
{
  if (hair->batch_cache) {
    BKE_hair_batch_cache_free_cb(hair);
  }
}
