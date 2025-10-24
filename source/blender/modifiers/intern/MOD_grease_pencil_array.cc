/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "DNA_defaults.h"
#include "DNA_modifier_types.h"

#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_lib_query.hh"
#include "BKE_material.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"

#include "BLO_read_write.hh"

#include "GEO_realize_instances.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLT_translation.hh"

#include "BLI_bounds_types.hh"
#include "BLI_hash.h"
#include "BLI_math_matrix.hh"
#include "BLI_rand.h"

#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MOD_grease_pencil_util.hh"
#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

namespace blender {

static void init_data(ModifierData *md)
{
  auto *mmd = reinterpret_cast<GreasePencilArrayModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(mmd, modifier));

  MEMCPY_STRUCT_AFTER(mmd, DNA_struct_default_get(GreasePencilArrayModifierData), modifier);
  modifier::greasepencil::init_influence_data(&mmd->influence, false);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const auto *mmd = reinterpret_cast<const GreasePencilArrayModifierData *>(md);
  auto *tmmd = reinterpret_cast<GreasePencilArrayModifierData *>(target);

  modifier::greasepencil::free_influence_data(&tmmd->influence);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&mmd->influence, &tmmd->influence, flag);
}

static void free_data(ModifierData *md)
{
  auto *mmd = reinterpret_cast<GreasePencilArrayModifierData *>(md);
  modifier::greasepencil::free_influence_data(&mmd->influence);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *mmd = reinterpret_cast<GreasePencilArrayModifierData *>(md);
  walk(user_data, ob, (ID **)&mmd->object, IDWALK_CB_NOP);
  modifier::greasepencil::foreach_influence_ID_link(&mmd->influence, ob, walk, user_data);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  auto *mmd = reinterpret_cast<GreasePencilArrayModifierData *>(md);
  if (mmd->object != nullptr) {
    DEG_add_object_relation(
        ctx->node, mmd->object, DEG_OB_COMP_TRANSFORM, "Grease Pencil Array Modifier");
    DEG_add_depends_on_transform_relation(ctx->node, "Grease Pencil Array Modifier");
  }
}

static float4x4 get_array_matrix(const Object &ob,
                                 const GreasePencilArrayModifierData &mmd,
                                 const int elem_idx,
                                 const bool use_object_offset)
{

  if (use_object_offset) {
    float4x4 mat_offset = float4x4::identity();

    if (mmd.flag & MOD_GREASE_PENCIL_ARRAY_USE_OFFSET) {
      mat_offset[3] += mmd.offset;
    }
    const float4x4 &obinv = ob.world_to_object();

    return mat_offset * obinv * mmd.object->object_to_world();
  }

  const float3 offset = [&]() {
    if (mmd.flag & MOD_GREASE_PENCIL_ARRAY_USE_OFFSET) {
      return float3(mmd.offset) * elem_idx;
    }
    return float3(0.0f);
  }();

  return math::from_location<float4x4>(offset);
}

static float4x4 get_rand_matrix(const GreasePencilArrayModifierData &mmd,
                                const Object &ob,
                                const int elem_id)
{
  int seed = mmd.seed;
  seed += BLI_hash_string(ob.id.name + 2);
  seed += BLI_hash_string(mmd.modifier.name);
  const float rand_offset = BLI_hash_int_01(seed);
  float3x3 rand;
  for (int j = 0; j < 3; j++) {
    const uint3 primes(2, 3, 7);
    double3 offset(0.0);
    double3 r;
    /* To ensure a nice distribution, we use halton sequence and offset using the seed. */
    BLI_halton_3d(primes, offset, elem_id, r);

    if ((mmd.flag & MOD_GREASE_PENCIL_ARRAY_UNIFORM_RANDOM_SCALE) && j == 2) {
      float rand_value;
      rand_value = math::mod(r[0] * 2.0 - 1.0 + rand_offset, 1.0);
      rand_value = math::mod(math::sin(rand_value * 12.9898 + j * 78.233) * 43758.5453, 1.0);
      rand[j] = float3(rand_value);
    }
    else {
      for (int i = 0; i < 3; i++) {
        rand[j][i] = math::mod(r[i] * 2.0 - 1.0 + rand_offset, 1.0);
        rand[j][i] = math::mod(math::sin(rand[j][i] * 12.9898 + j * 78.233) * 43758.5453, 1.0);
      }
    }
  }
  /* Calculate Random matrix. */
  return math::from_loc_rot_scale<float4x4>(
      mmd.rnd_offset * rand[0], mmd.rnd_rot * rand[1], float3(1.0f) + mmd.rnd_scale * rand[2]);
};

static bke::CurvesGeometry create_array_copies(const Object &ob,
                                               const GreasePencilArrayModifierData &mmd,
                                               const bke::CurvesGeometry &base_curves,
                                               bke::CurvesGeometry filtered_curves)
{
  /* Assign replacement material on filtered curves so all copies can have this material when later
   * when they get instanced. */
  if (mmd.mat_rpl > 0) {
    bke::MutableAttributeAccessor attributes = filtered_curves.attributes_for_write();
    bke::SpanAttributeWriter<int> stroke_materials = attributes.lookup_or_add_for_write_span<int>(
        "material_index", bke::AttrDomain::Curve);
    stroke_materials.span.fill(mmd.mat_rpl - 1);
    stroke_materials.finish();
  }

  Curves *base_curves_id = bke::curves_new_nomain(base_curves);
  Curves *filtered_curves_id = bke::curves_new_nomain(filtered_curves);
  bke::GeometrySet base_geo = bke::GeometrySet::from_curves(base_curves_id);
  bke::GeometrySet filtered_geo = bke::GeometrySet::from_curves(filtered_curves_id);

  std::unique_ptr<bke::Instances> instances = std::make_unique<bke::Instances>();
  const int base_handle = instances->add_reference(bke::InstanceReference{base_geo});
  const int filtered_handle = instances->add_reference(bke::InstanceReference{filtered_geo});

  /* Always add untouched original curves. */
  instances->add_instance(base_handle, float4x4::identity());

  float3 size(0.0f);
  if (mmd.flag & MOD_GREASE_PENCIL_ARRAY_USE_RELATIVE) {
    std::optional<blender::Bounds<float3>> bounds = filtered_curves.bounds_min_max();
    if (bounds.has_value()) {
      size = bounds.value().max - bounds.value().min;
      /* Need a minimum size (for flat drawings). */
      size = math::max(size, float3(0.01f));
    }
  }

  float4x4 current_offset = float4x4::identity();
  for (const int elem_id : IndexRange(1, mmd.count - 1)) {
    const bool use_object_offset = (mmd.flag & MOD_GREASE_PENCIL_ARRAY_USE_OB_OFFSET) &&
                                   (mmd.object);
    const float4x4 mat = get_array_matrix(ob, mmd, elem_id, use_object_offset);

    if (use_object_offset) {
      current_offset = current_offset * mat;
    }
    else {
      current_offset = mat;
    }

    /* Apply relative offset. */
    if (mmd.flag & MOD_GREASE_PENCIL_ARRAY_USE_RELATIVE) {
      float3 relative = size * float3(mmd.shift);
      float3 translate = relative * float3(float(elem_id));
      current_offset.w += float4(translate, 1.0f);
    }

    current_offset *= get_rand_matrix(mmd, ob, elem_id);

    instances->add_instance(filtered_handle, current_offset);
  }

  geometry::RealizeInstancesOptions options;
  options.keep_original_ids = true;
  options.realize_instance_attributes = false; /* Should this be true? */
  bke::GeometrySet result_geo = geometry::realize_instances(
                                    bke::GeometrySet::from_instances(instances.release()), options)
                                    .geometry;
  return std::move(result_geo.get_curves_for_write()->geometry.wrap());
}

static void modify_drawing(const GreasePencilArrayModifierData &mmd,
                           const ModifierEvalContext &ctx,
                           bke::greasepencil::Drawing &drawing)
{
  const bke::CurvesGeometry &src_curves = drawing.strokes();
  if (src_curves.curve_num == 0) {
    return;
  }

  IndexMaskMemory curve_mask_memory;
  const IndexMask curves_mask = modifier::greasepencil::get_filtered_stroke_mask(
      ctx.object, src_curves, mmd.influence, curve_mask_memory);

  if (curves_mask.size() == src_curves.curve_num) {
    /* Make a full copy so we can modify materials inside #create_array_copies before instancing.
     */
    bke::CurvesGeometry copy = bke::CurvesGeometry(src_curves);

    drawing.strokes_for_write() = create_array_copies(
        *ctx.object, mmd, src_curves, std::move(copy));
  }
  else {
    bke::CurvesGeometry masked_curves = bke::curves_copy_curve_selection(
        src_curves, curves_mask, {});

    drawing.strokes_for_write() = create_array_copies(
        *ctx.object, mmd, src_curves, std::move(masked_curves));
  }

  drawing.tag_topology_changed();
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  using bke::greasepencil::Drawing;

  auto *mmd = reinterpret_cast<GreasePencilArrayModifierData *>(md);

  if (!geometry_set->has_grease_pencil()) {
    return;
  }
  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();
  const int frame = grease_pencil.runtime->eval_frame;

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, mmd->influence, mask_memory);

  const Vector<Drawing *> drawings = modifier::greasepencil::get_drawings_for_write(
      grease_pencil, layer_mask, frame);
  threading::parallel_for_each(drawings,
                               [&](Drawing *drawing) { modify_drawing(*mmd, *ctx, *drawing); });
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  layout->prop(ptr, "count", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "replace_material", UI_ITEM_NONE, IFACE_("Material Override"), ICON_NONE);
  PanelLayout relative_offset_layout = layout->panel_prop_with_bool_header(
      C, ptr, "open_relative_offset_panel", ptr, "use_relative_offset", IFACE_("Relative Offset"));
  if (uiLayout *sub = relative_offset_layout.body) {
    uiLayout *col = &sub->column(false);
    col->active_set(RNA_boolean_get(ptr, "use_relative_offset"));
    col->prop(ptr, "relative_offset", UI_ITEM_NONE, IFACE_("Factor"), ICON_NONE);
  }
  PanelLayout constant_offset_layout = layout->panel_prop_with_bool_header(
      C, ptr, "open_constant_offset_panel", ptr, "use_constant_offset", IFACE_("Constant Offset"));
  if (uiLayout *sub = constant_offset_layout.body) {
    uiLayout *col = &sub->column(false);
    col->active_set(RNA_boolean_get(ptr, "use_constant_offset"));
    col->prop(ptr, "constant_offset", UI_ITEM_NONE, IFACE_("Distance"), ICON_NONE);
  }
  PanelLayout object_offset_layout = layout->panel_prop_with_bool_header(
      C, ptr, "open_object_offset_panel", ptr, "use_object_offset", IFACE_("Object Offset"));
  if (uiLayout *sub = object_offset_layout.body) {
    uiLayout *col = &sub->column(false);
    col->active_set(RNA_boolean_get(ptr, "use_object_offset"));
    col->prop(ptr, "offset_object", UI_ITEM_NONE, IFACE_("Object"), ICON_NONE);
  }

  if (uiLayout *sub = layout->panel_prop(C, ptr, "open_randomize_panel", IFACE_("Randomize"))) {
    sub->use_property_split_set(true);
    sub->prop(ptr, "random_offset", UI_ITEM_NONE, IFACE_("Offset"), ICON_NONE);
    sub->prop(ptr, "random_rotation", UI_ITEM_NONE, IFACE_("Rotation"), ICON_NONE);
    sub->prop(ptr, "random_scale", UI_ITEM_NONE, IFACE_("Scale"), ICON_NONE);
    sub->prop(ptr, "use_uniform_random_scale", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    sub->prop(ptr, "seed", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (uiLayout *influence_panel = layout->panel_prop(
          C, ptr, "open_influence_panel", IFACE_("Influence")))
  {
    modifier::greasepencil::draw_layer_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_material_filter_settings(C, influence_panel, ptr);
  }

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_GreasePencilArray, panel_draw);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *mmd = reinterpret_cast<const GreasePencilArrayModifierData *>(md);

  BLO_write_struct(writer, GreasePencilArrayModifierData, mmd);
  modifier::greasepencil::write_influence_data(writer, &mmd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *mmd = reinterpret_cast<GreasePencilArrayModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &mmd->influence);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilArray = {
    /*idname*/ "GreasePencilArrayModifier",
    /*name*/ N_("Array"),
    /*struct_name*/ "GreasePencilArrayModifierData",
    /*struct_size*/ sizeof(GreasePencilArrayModifierData),
    /*srna*/ &RNA_GreasePencilArrayModifier,
    /*type*/ ModifierTypeType::Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping,
    /*icon*/ ICON_MOD_ARRAY,

    /*copy_data*/ blender::copy_data,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ blender::modify_geometry_set,

    /*init_data*/ blender::init_data,
    /*required_data_mask*/ nullptr,
    /*free_data*/ blender::free_data,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ blender::update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ blender::foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ blender::panel_register,
    /*blend_write*/ blender::blend_write,
    /*blend_read*/ blender::blend_read,
};
