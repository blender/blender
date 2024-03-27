/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <optional>

#include "MEM_guardedalloc.h"

#include "DNA_defaults.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_bounds.hh"
#include "BLI_index_range.hh"
#include "BLI_math_vector.hh"
#include "BLI_rand.h"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_anim_data.hh"
#include "BKE_bake_data_block_id.hh"
#include "BKE_customdata.hh"
#include "BKE_geometry_set.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_pointcloud.hh"

#include "BLT_translation.hh"

#include "DEG_depsgraph_query.hh"

#include "BLO_read_write.hh"

using blender::float3;
using blender::IndexRange;
using blender::MutableSpan;
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

  pointcloud->runtime = new blender::bke::PointCloudRuntime();

  CustomData_reset(&pointcloud->pdata);
  pointcloud->attributes_for_write().add<float3>(
      "position", blender::bke::AttrDomain::Point, blender::bke::AttributeInitConstruct());
}

static void pointcloud_copy_data(Main * /*bmain*/,
                                 std::optional<Library *> /*owner_library*/,
                                 ID *id_dst,
                                 const ID *id_src,
                                 const int /*flag*/)
{
  PointCloud *pointcloud_dst = (PointCloud *)id_dst;
  const PointCloud *pointcloud_src = (const PointCloud *)id_src;
  pointcloud_dst->mat = static_cast<Material **>(MEM_dupallocN(pointcloud_src->mat));

  CustomData_copy(
      &pointcloud_src->pdata, &pointcloud_dst->pdata, CD_MASK_ALL, pointcloud_dst->totpoint);

  pointcloud_dst->runtime = new blender::bke::PointCloudRuntime();
  pointcloud_dst->runtime->bounds_cache = pointcloud_src->runtime->bounds_cache;
  if (pointcloud_src->runtime->bake_materials) {
    pointcloud_dst->runtime->bake_materials =
        std::make_unique<blender::bke::bake::BakeMaterialsList>(
            *pointcloud_src->runtime->bake_materials);
  }

  pointcloud_dst->batch_cache = nullptr;
}

static void pointcloud_free_data(ID *id)
{
  PointCloud *pointcloud = (PointCloud *)id;
  BKE_animdata_free(&pointcloud->id, false);
  BKE_pointcloud_batch_cache_free(pointcloud);
  CustomData_free(&pointcloud->pdata, pointcloud->totpoint);
  MEM_SAFE_FREE(pointcloud->mat);
  delete pointcloud->runtime;
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
}

static void pointcloud_blend_read_data(BlendDataReader *reader, ID *id)
{
  PointCloud *pointcloud = (PointCloud *)id;

  /* Geometry */
  CustomData_blend_read(reader, &pointcloud->pdata, pointcloud->totpoint);

  /* Materials */
  BLO_read_pointer_array(reader, (void **)&pointcloud->mat);

  pointcloud->runtime = new blender::bke::PointCloudRuntime();
}

IDTypeInfo IDType_ID_PT = {
    /*id_code*/ ID_PT,
    /*id_filter*/ FILTER_ID_PT,
    /*dependencies_id_types*/ FILTER_ID_MA,
    /*main_listbase_index*/ INDEX_ID_PT,
    /*struct_size*/ sizeof(PointCloud),
    /*name*/ "PointCloud",
    /*name_plural*/ N_("pointclouds"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_POINTCLOUD,
    /*flags*/ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /*init_data*/ pointcloud_init_data,
    /*copy_data*/ pointcloud_copy_data,
    /*free_data*/ pointcloud_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ pointcloud_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ pointcloud_blend_write,
    /*blend_read_data*/ pointcloud_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

static void pointcloud_random(PointCloud *pointcloud)
{
  BLI_assert(pointcloud->totpoint == 0);
  pointcloud->totpoint = 400;
  CustomData_realloc(&pointcloud->pdata, 0, pointcloud->totpoint);

  RNG *rng = BLI_rng_new(0);

  blender::bke::MutableAttributeAccessor attributes = pointcloud->attributes_for_write();
  blender::MutableSpan<float3> positions = pointcloud->positions_for_write();
  blender::bke::SpanAttributeWriter<float> radii =
      attributes.lookup_or_add_for_write_only_span<float>(POINTCLOUD_ATTR_RADIUS,
                                                          blender::bke::AttrDomain::Point);

  for (const int i : positions.index_range()) {
    positions[i] = float3(BLI_rng_get_float(rng), BLI_rng_get_float(rng), BLI_rng_get_float(rng)) *
                       2.0f -
                   1.0f;
    radii.span[i] = 0.05f * BLI_rng_get_float(rng);
  }

  radii.finish();

  BLI_rng_free(rng);
}

Span<float3> PointCloud::positions() const
{
  return {static_cast<const float3 *>(
              CustomData_get_layer_named(&this->pdata, CD_PROP_FLOAT3, "position")),
          this->totpoint};
}

MutableSpan<float3> PointCloud::positions_for_write()
{
  return {static_cast<float3 *>(CustomData_get_layer_named_for_write(
              &this->pdata, CD_PROP_FLOAT3, "position", this->totpoint)),
          this->totpoint};
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
  pointcloud_random(pointcloud);

  return pointcloud;
}

PointCloud *BKE_pointcloud_new_nomain(const int totpoint)
{
  PointCloud *pointcloud = static_cast<PointCloud *>(BKE_libblock_alloc(
      nullptr, ID_PT, BKE_idtype_idcode_to_name(ID_PT), LIB_ID_CREATE_LOCALIZE));

  pointcloud_init_data(&pointcloud->id);

  CustomData_realloc(&pointcloud->pdata, 0, totpoint);
  pointcloud->totpoint = totpoint;

  return pointcloud;
}

void BKE_pointcloud_nomain_to_pointcloud(PointCloud *pointcloud_src, PointCloud *pointcloud_dst)
{
  BLI_assert(pointcloud_src->id.tag & LIB_TAG_NO_MAIN);

  CustomData_free(&pointcloud_dst->pdata, pointcloud_dst->totpoint);

  const int totpoint = pointcloud_dst->totpoint = pointcloud_src->totpoint;
  CustomData_copy(&pointcloud_src->pdata, &pointcloud_dst->pdata, CD_MASK_ALL, totpoint);

  BKE_id_free(nullptr, pointcloud_src);
}

std::optional<blender::Bounds<blender::float3>> PointCloud::bounds_min_max() const
{
  using namespace blender;
  using namespace blender::bke;
  if (this->totpoint == 0) {
    return std::nullopt;
  }
  this->runtime->bounds_cache.ensure([&](Bounds<float3> &r_bounds) {
    const AttributeAccessor attributes = this->attributes();
    const Span<float3> positions = this->positions();
    if (attributes.contains(POINTCLOUD_ATTR_RADIUS)) {
      const VArraySpan radii = *attributes.lookup<float>(POINTCLOUD_ATTR_RADIUS);
      r_bounds = *bounds::min_max_with_radii(positions, radii);
    }
    else {
      r_bounds = *bounds::min_max(positions);
    }
  });
  return this->runtime->bounds_cache.data();
}

bool BKE_pointcloud_attribute_required(const PointCloud * /*pointcloud*/, const char *name)
{
  return STREQ(name, POINTCLOUD_ATTR_POSITION);
}

/* Dependency Graph */

PointCloud *BKE_pointcloud_copy_for_eval(const PointCloud *pointcloud_src)
{
  return reinterpret_cast<PointCloud *>(
      BKE_id_copy_ex(nullptr, &pointcloud_src->id, nullptr, LIB_ID_COPY_LOCALIZE));
}

static void pointcloud_evaluate_modifiers(Depsgraph *depsgraph,
                                          Scene *scene,
                                          Object *object,
                                          blender::bke::GeometrySet &geometry_set)
{
  /* Modifier evaluation modes. */
  const bool use_render = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  const int required_mode = use_render ? eModifierMode_Render : eModifierMode_Realtime;
  ModifierApplyFlag apply_flag = use_render ? MOD_APPLY_RENDER : MOD_APPLY_USECACHE;
  const ModifierEvalContext mectx = {depsgraph, object, apply_flag};

  BKE_modifiers_clear_errors(object);

  /* Get effective list of modifiers to execute. Some effects like shape keys
   * are added as virtual modifiers before the user created modifiers. */
  VirtualModifierData virtual_modifier_data;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(object, &virtual_modifier_data);

  /* Evaluate modifiers. */
  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md->type);

    if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
      continue;
    }

    blender::bke::ScopedModifierTimer modifier_timer{*md};

    if (mti->modify_geometry_set) {
      mti->modify_geometry_set(md, &mectx, &geometry_set);
    }
  }
}

static PointCloud *take_pointcloud_ownership_from_geometry_set(
    blender::bke::GeometrySet &geometry_set)
{
  if (!geometry_set.has<blender::bke::PointCloudComponent>()) {
    return nullptr;
  }
  blender::bke::PointCloudComponent &pointcloud_component =
      geometry_set.get_component_for_write<blender::bke::PointCloudComponent>();
  PointCloud *pointcloud = pointcloud_component.release();
  if (pointcloud != nullptr) {
    /* Add back, but as read-only non-owning component. */
    pointcloud_component.replace(pointcloud, blender::bke::GeometryOwnershipType::ReadOnly);
  }
  else {
    /* The component was empty, we can also remove it. */
    geometry_set.remove<blender::bke::PointCloudComponent>();
  }
  return pointcloud;
}

void BKE_pointcloud_data_update(Depsgraph *depsgraph, Scene *scene, Object *object)
{
  /* Free any evaluated data and restore original data. */
  BKE_object_free_derived_caches(object);

  /* Evaluate modifiers. */
  PointCloud *pointcloud = static_cast<PointCloud *>(object->data);
  blender::bke::GeometrySet geometry_set = blender::bke::GeometrySet::from_pointcloud(
      pointcloud, blender::bke::GeometryOwnershipType::ReadOnly);
  pointcloud_evaluate_modifiers(depsgraph, scene, object, geometry_set);

  PointCloud *pointcloud_eval = take_pointcloud_ownership_from_geometry_set(geometry_set);

  /* If the geometry set did not contain a point cloud, we still create an empty one. */
  if (pointcloud_eval == nullptr) {
    pointcloud_eval = BKE_pointcloud_new_nomain(0);
  }

  /* Assign evaluated object. */
  const bool eval_is_owned = pointcloud_eval != pointcloud;
  BKE_object_eval_assign_data(object, &pointcloud_eval->id, eval_is_owned);
  object->runtime->geometry_set_eval = new blender::bke::GeometrySet(std::move(geometry_set));
}

void PointCloud::tag_positions_changed()
{
  this->runtime->bounds_cache.tag_dirty();
}

void PointCloud::tag_radii_changed()
{
  this->runtime->bounds_cache.tag_dirty();
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
