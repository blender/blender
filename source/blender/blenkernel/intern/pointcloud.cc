/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_defaults.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_bounds.hh"
#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_math_vec_types.hh"
#include "BLI_rand.h"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_anim_data.h"
#include "BKE_customdata.h"
#include "BKE_geometry_set.hh"
#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_pointcloud.h"

#include "BLT_translation.h"

#include "DEG_depsgraph_query.h"

#include "BLO_read_write.h"

using blender::float3;
using blender::IndexRange;
using blender::Span;
using blender::Vector;

/* PointCloud datablock */

static void pointcloud_random(PointCloud *pointcloud);

const char *POINTCLOUD_ATTR_POSITION = "position";
const char *POINTCLOUD_ATTR_RADIUS = "radius";

static void pointcloud_init_data(ID *id)
{
  PointCloud *pointcloud = (PointCloud *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(pointcloud, id));

  MEMCPY_STRUCT_AFTER(pointcloud, DNA_struct_default_get(PointCloud), id);

  CustomData_reset(&pointcloud->pdata);
  CustomData_add_layer_named(&pointcloud->pdata,
                             CD_PROP_FLOAT3,
                             CD_CALLOC,
                             nullptr,
                             pointcloud->totpoint,
                             POINTCLOUD_ATTR_POSITION);
  BKE_pointcloud_update_customdata_pointers(pointcloud);
}

static void pointcloud_copy_data(Main *UNUSED(bmain), ID *id_dst, const ID *id_src, const int flag)
{
  PointCloud *pointcloud_dst = (PointCloud *)id_dst;
  const PointCloud *pointcloud_src = (const PointCloud *)id_src;
  pointcloud_dst->mat = static_cast<Material **>(MEM_dupallocN(pointcloud_src->mat));

  const eCDAllocType alloc_type = (flag & LIB_ID_COPY_CD_REFERENCE) ? CD_REFERENCE : CD_DUPLICATE;
  CustomData_copy(&pointcloud_src->pdata,
                  &pointcloud_dst->pdata,
                  CD_MASK_ALL,
                  alloc_type,
                  pointcloud_dst->totpoint);
  BKE_pointcloud_update_customdata_pointers(pointcloud_dst);

  pointcloud_dst->batch_cache = nullptr;
}

static void pointcloud_free_data(ID *id)
{
  PointCloud *pointcloud = (PointCloud *)id;
  BKE_animdata_free(&pointcloud->id, false);
  BKE_pointcloud_batch_cache_free(pointcloud);
  CustomData_free(&pointcloud->pdata, pointcloud->totpoint);
  MEM_SAFE_FREE(pointcloud->mat);
}

static void pointcloud_foreach_id(ID *id, LibraryForeachIDData *data)
{
  PointCloud *pointcloud = (PointCloud *)id;
  for (int i = 0; i < pointcloud->totcol; i++) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, pointcloud->mat[i], IDWALK_CB_USER);
  }
}

static void pointcloud_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  PointCloud *pointcloud = (PointCloud *)id;

  Vector<CustomDataLayer, 16> point_layers;
  CustomData_blend_write_prepare(pointcloud->pdata, point_layers);

  /* Write LibData */
  BLO_write_id_struct(writer, PointCloud, id_address, &pointcloud->id);
  BKE_id_blend_write(writer, &pointcloud->id);

  /* Direct data */
  CustomData_blend_write(writer,
                         &pointcloud->pdata,
                         point_layers,
                         pointcloud->totpoint,
                         CD_MASK_ALL,
                         &pointcloud->id);

  BLO_write_pointer_array(writer, pointcloud->totcol, pointcloud->mat);
  if (pointcloud->adt) {
    BKE_animdata_blend_write(writer, pointcloud->adt);
  }
}

static void pointcloud_blend_read_data(BlendDataReader *reader, ID *id)
{
  PointCloud *pointcloud = (PointCloud *)id;
  BLO_read_data_address(reader, &pointcloud->adt);
  BKE_animdata_blend_read_data(reader, pointcloud->adt);

  /* Geometry */
  CustomData_blend_read(reader, &pointcloud->pdata, pointcloud->totpoint);
  BKE_pointcloud_update_customdata_pointers(pointcloud);

  /* Materials */
  BLO_read_pointer_array(reader, (void **)&pointcloud->mat);
}

static void pointcloud_blend_read_lib(BlendLibReader *reader, ID *id)
{
  PointCloud *pointcloud = (PointCloud *)id;
  for (int a = 0; a < pointcloud->totcol; a++) {
    BLO_read_id_address(reader, pointcloud->id.lib, &pointcloud->mat[a]);
  }
}

static void pointcloud_blend_read_expand(BlendExpander *expander, ID *id)
{
  PointCloud *pointcloud = (PointCloud *)id;
  for (int a = 0; a < pointcloud->totcol; a++) {
    BLO_expand(expander, pointcloud->mat[a]);
  }
}

IDTypeInfo IDType_ID_PT = {
    /* id_code */ ID_PT,
    /* id_filter */ FILTER_ID_PT,
    /* main_listbase_index */ INDEX_ID_PT,
    /* struct_size */ sizeof(PointCloud),
    /* name */ "PointCloud",
    /* name_plural */ "pointclouds",
    /* translation_context */ BLT_I18NCONTEXT_ID_POINTCLOUD,
    /* flags */ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /* asset_type_info */ nullptr,

    /* init_data */ pointcloud_init_data,
    /* copy_data */ pointcloud_copy_data,
    /* free_data */ pointcloud_free_data,
    /* make_local */ nullptr,
    /* foreach_id */ pointcloud_foreach_id,
    /* foreach_cache */ nullptr,
    /* foreach_path */ nullptr,
    /* owner_get */ nullptr,

    /* blend_write */ pointcloud_blend_write,
    /* blend_read_data */ pointcloud_blend_read_data,
    /* blend_read_lib */ pointcloud_blend_read_lib,
    /* blend_read_expand */ pointcloud_blend_read_expand,

    /* blend_read_undo_preserve */ nullptr,

    /* lib_override_apply_post */ nullptr,
};

static void pointcloud_random(PointCloud *pointcloud)
{
  pointcloud->totpoint = 400;
  CustomData_realloc(&pointcloud->pdata, pointcloud->totpoint);
  BKE_pointcloud_update_customdata_pointers(pointcloud);

  RNG *rng = BLI_rng_new(0);

  for (int i = 0; i < pointcloud->totpoint; i++) {
    pointcloud->co[i][0] = 2.0f * BLI_rng_get_float(rng) - 1.0f;
    pointcloud->co[i][1] = 2.0f * BLI_rng_get_float(rng) - 1.0f;
    pointcloud->co[i][2] = 2.0f * BLI_rng_get_float(rng) - 1.0f;
    pointcloud->radius[i] = 0.05f * BLI_rng_get_float(rng);
  }

  BLI_rng_free(rng);
}

void *BKE_pointcloud_add(Main *bmain, const char *name)
{
  PointCloud *pointcloud = static_cast<PointCloud *>(BKE_id_new(bmain, ID_PT, name));

  return pointcloud;
}

void *BKE_pointcloud_add_default(Main *bmain, const char *name)
{
  PointCloud *pointcloud = static_cast<PointCloud *>(BKE_libblock_alloc(bmain, ID_PT, name, 0));

  pointcloud_init_data(&pointcloud->id);

  CustomData_add_layer_named(&pointcloud->pdata,
                             CD_PROP_FLOAT,
                             CD_CALLOC,
                             nullptr,
                             pointcloud->totpoint,
                             POINTCLOUD_ATTR_RADIUS);
  pointcloud_random(pointcloud);

  return pointcloud;
}

PointCloud *BKE_pointcloud_new_nomain(const int totpoint)
{
  PointCloud *pointcloud = static_cast<PointCloud *>(BKE_libblock_alloc(
      nullptr, ID_PT, BKE_idtype_idcode_to_name(ID_PT), LIB_ID_CREATE_LOCALIZE));

  pointcloud_init_data(&pointcloud->id);

  pointcloud->totpoint = totpoint;

  CustomData_add_layer_named(&pointcloud->pdata,
                             CD_PROP_FLOAT,
                             CD_CALLOC,
                             nullptr,
                             pointcloud->totpoint,
                             POINTCLOUD_ATTR_RADIUS);

  pointcloud->totpoint = totpoint;
  CustomData_realloc(&pointcloud->pdata, pointcloud->totpoint);
  BKE_pointcloud_update_customdata_pointers(pointcloud);

  return pointcloud;
}

static std::optional<blender::bounds::MinMaxResult<float3>> point_cloud_bounds(
    const PointCloud &pointcloud)
{
  Span<float3> positions{reinterpret_cast<float3 *>(pointcloud.co), pointcloud.totpoint};
  if (pointcloud.radius) {
    Span<float> radii{pointcloud.radius, pointcloud.totpoint};
    return blender::bounds::min_max_with_radii(positions, radii);
  }
  return blender::bounds::min_max(positions);
}

bool BKE_pointcloud_minmax(const PointCloud *pointcloud, float r_min[3], float r_max[3])
{
  using namespace blender;

  const std::optional<bounds::MinMaxResult<float3>> min_max = point_cloud_bounds(*pointcloud);
  if (!min_max) {
    return false;
  }

  copy_v3_v3(r_min, math::min(min_max->min, float3(r_min)));
  copy_v3_v3(r_max, math::max(min_max->max, float3(r_max)));

  return true;
}

BoundBox *BKE_pointcloud_boundbox_get(Object *ob)
{
  BLI_assert(ob->type == OB_POINTCLOUD);

  if (ob->runtime.bb != nullptr && (ob->runtime.bb->flag & BOUNDBOX_DIRTY) == 0) {
    return ob->runtime.bb;
  }

  if (ob->runtime.bb == nullptr) {
    ob->runtime.bb = static_cast<BoundBox *>(MEM_callocN(sizeof(BoundBox), "pointcloud boundbox"));
  }

  float3 min, max;
  INIT_MINMAX(min, max);
  if (ob->runtime.geometry_set_eval != nullptr) {
    ob->runtime.geometry_set_eval->compute_boundbox_without_instances(&min, &max);
  }
  else {
    const PointCloud *pointcloud = static_cast<PointCloud *>(ob->data);
    BKE_pointcloud_minmax(pointcloud, min, max);
  }
  BKE_boundbox_init_from_minmax(ob->runtime.bb, min, max);

  return ob->runtime.bb;
}

void BKE_pointcloud_update_customdata_pointers(PointCloud *pointcloud)
{
  pointcloud->co = static_cast<float(*)[3]>(
      CustomData_get_layer_named(&pointcloud->pdata, CD_PROP_FLOAT3, POINTCLOUD_ATTR_POSITION));
  pointcloud->radius = static_cast<float *>(
      CustomData_get_layer_named(&pointcloud->pdata, CD_PROP_FLOAT, POINTCLOUD_ATTR_RADIUS));
}

bool BKE_pointcloud_customdata_required(const PointCloud *UNUSED(pointcloud), const char *name)
{
  return STREQ(name, POINTCLOUD_ATTR_POSITION);
}

/* Dependency Graph */

PointCloud *BKE_pointcloud_new_for_eval(const PointCloud *pointcloud_src, int totpoint)
{
  PointCloud *pointcloud_dst = static_cast<PointCloud *>(BKE_id_new_nomain(ID_PT, nullptr));
  CustomData_free(&pointcloud_dst->pdata, pointcloud_dst->totpoint);

  STRNCPY(pointcloud_dst->id.name, pointcloud_src->id.name);
  pointcloud_dst->mat = static_cast<Material **>(MEM_dupallocN(pointcloud_src->mat));
  pointcloud_dst->totcol = pointcloud_src->totcol;

  pointcloud_dst->totpoint = totpoint;
  CustomData_copy(
      &pointcloud_src->pdata, &pointcloud_dst->pdata, CD_MASK_ALL, CD_CALLOC, totpoint);
  BKE_pointcloud_update_customdata_pointers(pointcloud_dst);

  return pointcloud_dst;
}

PointCloud *BKE_pointcloud_copy_for_eval(struct PointCloud *pointcloud_src, bool reference)
{
  int flags = LIB_ID_COPY_LOCALIZE;

  if (reference) {
    flags |= LIB_ID_COPY_CD_REFERENCE;
  }

  PointCloud *result = (PointCloud *)BKE_id_copy_ex(nullptr, &pointcloud_src->id, nullptr, flags);
  return result;
}

static void pointcloud_evaluate_modifiers(struct Depsgraph *depsgraph,
                                          struct Scene *scene,
                                          Object *object,
                                          GeometrySet &geometry_set,
                                          bool *r_need_caching)
{
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
    const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md->type);

    if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
      continue;
    }

    if (r_need_caching && (mti->flags & eModifierTypeFlag_NeedCaching)) {
      *r_need_caching = true;
    }

    if (mti->modifyGeometrySet) {
      mti->modifyGeometrySet(md, &mectx, &geometry_set);
    }
  }
}

static PointCloud *take_pointcloud_ownership_from_geometry_set(GeometrySet &geometry_set)
{
  if (!geometry_set.has<PointCloudComponent>()) {
    return nullptr;
  }
  PointCloudComponent &pointcloud_component =
      geometry_set.get_component_for_write<PointCloudComponent>();
  PointCloud *pointcloud = pointcloud_component.release();
  if (pointcloud != nullptr) {
    /* Add back, but as read-only non-owning component. */
    pointcloud_component.replace(pointcloud, GeometryOwnershipType::ReadOnly);
  }
  else {
    /* The component was empty, we can also remove it. */
    geometry_set.remove<PointCloudComponent>();
  }
  return pointcloud;
}

void BKE_pointcloud_data_update(struct Depsgraph *depsgraph, struct Scene *scene, Object *object)
{
  /* Free any evaluated data and restore original data. */
  BKE_object_free_derived_caches(object);

  /* Evaluate modifiers. */
  PointCloud *pointcloud = static_cast<PointCloud *>(object->data);
  GeometrySet geometry_set = GeometrySet::create_with_pointcloud(pointcloud,
                                                                 GeometryOwnershipType::ReadOnly);
  bool need_caching = false;
  pointcloud_evaluate_modifiers(depsgraph, scene, object, geometry_set, &need_caching);

  PointCloud *pointcloud_eval = take_pointcloud_ownership_from_geometry_set(geometry_set);

  /* If the geometry set did not contain a point cloud, we still create an empty one. */
  if (pointcloud_eval == nullptr) {
    pointcloud_eval = BKE_pointcloud_new_nomain(0);
  }

  /* Assign evaluated object. */
  const bool eval_is_owned = pointcloud_eval != pointcloud;
  BKE_object_eval_assign_data(object, &pointcloud_eval->id, eval_is_owned);
  object->runtime.geometry_set_eval = new GeometrySet(std::move(geometry_set));
  BKE_object_runtime_ensure_geometry_cache(object, need_caching);
}

/* Draw Cache */

void (*BKE_pointcloud_batch_cache_dirty_tag_cb)(PointCloud *pointcloud, int mode) = nullptr;
void (*BKE_pointcloud_batch_cache_free_cb)(PointCloud *pointcloud) = nullptr;

void BKE_pointcloud_batch_cache_dirty_tag(PointCloud *pointcloud, int mode)
{
  if (pointcloud->batch_cache) {
    BKE_pointcloud_batch_cache_dirty_tag_cb(pointcloud, mode);
  }
}

void BKE_pointcloud_batch_cache_free(PointCloud *pointcloud)
{
  if (pointcloud->batch_cache) {
    BKE_pointcloud_batch_cache_free_cb(pointcloud);
  }
}
