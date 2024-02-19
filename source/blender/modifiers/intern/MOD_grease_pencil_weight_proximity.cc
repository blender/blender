/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_index_mask.hh"
#include "BLI_math_matrix.hh"
#include "BLI_string.h" /* For #STRNCPY. */

#include "BLT_translation.hh"

#include "BLO_read_write.hh"

#include "DNA_defaults.h"
#include "DNA_modifier_types.h"
#include "DNA_screen_types.h"

#include "RNA_access.hh"

#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "MOD_grease_pencil_util.hh"
#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#include "RNA_prototypes.h"

namespace blender {

static void init_data(ModifierData *md)
{
  auto *gpmd = reinterpret_cast<GreasePencilWeightProximityModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(
      gpmd, DNA_struct_default_get(GreasePencilWeightProximityModifierData), modifier);
  modifier::greasepencil::init_influence_data(&gpmd->influence, false);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const auto *gmd = reinterpret_cast<const GreasePencilWeightProximityModifierData *>(md);
  auto *tgmd = reinterpret_cast<GreasePencilWeightProximityModifierData *>(target);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&gmd->influence, &tgmd->influence, flag);
}

static void free_data(ModifierData *md)
{
  auto *mmd = reinterpret_cast<GreasePencilWeightProximityModifierData *>(md);

  modifier::greasepencil::free_influence_data(&mmd->influence);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  auto *mmd = reinterpret_cast<GreasePencilWeightProximityModifierData *>(md);

  return (mmd->target_vgname[0] == '\0' || mmd->object == nullptr);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  GreasePencilWeightProximityModifierData *mmd =
      reinterpret_cast<GreasePencilWeightProximityModifierData *>(md);

  modifier::greasepencil::foreach_influence_ID_link(&mmd->influence, ob, walk, user_data);

  walk(user_data, ob, (ID **)&mmd->object, IDWALK_CB_NOP);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  auto *mmd = reinterpret_cast<GreasePencilWeightProximityModifierData *>(md);
  if (mmd->object != nullptr) {
    DEG_add_object_relation(
        ctx->node, mmd->object, DEG_OB_COMP_TRANSFORM, "Grease Pencil Proximity Modifier");
    DEG_add_depends_on_transform_relation(ctx->node, "Grease Pencil Proximity Modifier");
  }
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const GreasePencilWeightProximityModifierData *mmd =
      reinterpret_cast<const GreasePencilWeightProximityModifierData *>(md);

  BLO_write_struct(writer, GreasePencilWeightProximityModifierData, mmd);
  modifier::greasepencil::write_influence_data(writer, &mmd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  GreasePencilWeightProximityModifierData *mmd =
      reinterpret_cast<GreasePencilWeightProximityModifierData *>(md);
  modifier::greasepencil::read_influence_data(reader, &mmd->influence);
}

static float get_distance_factor(
    float3 target_pos, float4x4 obmat, float3 pos, const float dist_min, const float dist_max)
{
  const float3 gvert = math::transform_point(obmat, pos);
  const float dist = math::distance(target_pos, gvert);

  if (dist > dist_max) {
    return 1.0f;
  }
  if (dist <= dist_max && dist > dist_min) {
    return 1.0f - ((dist_max - dist) / math::max((dist_max - dist_min), 0.0001f));
  }
  return 0.0f;
}

static int ensure_vertex_group(const StringRefNull name, ListBase &vertex_group_names)
{
  int def_nr = BLI_findstringindex(
      &vertex_group_names, name.c_str(), offsetof(bDeformGroup, name));
  if (def_nr < 0) {
    bDeformGroup *defgroup = MEM_cnew<bDeformGroup>(__func__);
    STRNCPY(defgroup->name, name.c_str());
    BLI_addtail(&vertex_group_names, defgroup);
    def_nr = BLI_listbase_count(&vertex_group_names) - 1;
    BLI_assert(def_nr >= 0);
  }
  return def_nr;
}

static bool target_vertex_group_available(const StringRefNull name,
                                          const ListBase &vertex_group_names)
{
  const int def_nr = BLI_findstringindex(
      &vertex_group_names, name.c_str(), offsetof(bDeformGroup, name));
  if (def_nr < 0) {
    return false;
  }
  return true;
}

static void write_weights_for_drawing(const ModifierData &md,
                                      const Object &ob,
                                      bke::greasepencil::Drawing &drawing)
{
  const auto &mmd = reinterpret_cast<const GreasePencilWeightProximityModifierData &>(md);
  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  if (curves.points_num() == 0) {
    return;
  }
  IndexMaskMemory memory;
  const IndexMask strokes = modifier::greasepencil::get_filtered_stroke_mask(
      &ob, curves, mmd.influence, memory);
  if (strokes.is_empty()) {
    return;
  }

  /* Make sure that the target vertex group is added to this drawing so we can write to it. */
  ensure_vertex_group(mmd.target_vgname, curves.vertex_group_names);

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::SpanAttributeWriter<float> dst_weights = attributes.lookup_for_write_span<float>(
      mmd.target_vgname);

  BLI_assert(!dst_weights.span.is_empty());

  const VArray<float> vgroup_weights = modifier::greasepencil::get_influence_vertex_weights(
      curves, mmd.influence);

  const Span<float3> positions = curves.positions();
  const float4x4 obmat = ob.object_to_world();
  const float3 target_pos = mmd.object->object_to_world().location();
  const bool invert = (mmd.flag & MOD_GREASE_PENCIL_WEIGHT_PROXIMITY_INVERT_OUTPUT) != 0;
  const bool do_multiply = (mmd.flag & MOD_GREASE_PENCIL_WEIGHT_PROXIMITY_MULTIPLY_DATA) != 0;

  threading::parallel_for(positions.index_range(), 1024, [&](const IndexRange range) {
    for (const int point_i : range) {
      const float weight = vgroup_weights[point_i];
      if (weight < 0.0f) {
        continue;
      }

      float dist_fac = get_distance_factor(
          target_pos, obmat, positions[point_i], mmd.dist_start, mmd.dist_end);

      if (invert) {
        dist_fac = 1.0f - dist_fac;
      }

      dst_weights.span[point_i] = do_multiply ? dst_weights.span[point_i] * dist_fac : dist_fac;

      dst_weights.span[point_i] = math::clamp(
          dst_weights.span[point_i],
          /** Weight==0 will remove the point from the group, assign a sufficiently small value
           * there to prevent the visual disconnect, and keep the behavior same as the old
           * modifier. */
          math::max(mmd.min_weight, 1e-5f),
          1.0f);
    }
  });

  dst_weights.finish();
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  const GreasePencilWeightProximityModifierData *mmd =
      reinterpret_cast<GreasePencilWeightProximityModifierData *>(md);

  if (!geometry_set->has_grease_pencil()) {
    return;
  }

  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();

  if (!target_vertex_group_available(mmd->target_vgname, grease_pencil.vertex_group_names)) {
    return;
  }

  const int current_frame = grease_pencil.runtime->eval_frame;

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, mmd->influence, mask_memory);
  const Vector<bke::greasepencil::Drawing *> drawings =
      modifier::greasepencil::get_drawings_for_write(grease_pencil, layer_mask, current_frame);

  threading::parallel_for_each(drawings, [&](bke::greasepencil::Drawing *drawing) {
    write_weights_for_drawing(*md, *ctx->object, *drawing);
  });
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *row, *sub;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);
  row = uiLayoutRow(layout, true);
  uiItemPointerR(row, ptr, "target_vertex_group", &ob_ptr, "vertex_groups", nullptr, ICON_NONE);
  sub = uiLayoutRow(row, true);
  bool has_output = RNA_string_length(ptr, "target_vertex_group") != 0;
  uiLayoutSetPropDecorate(sub, false);
  uiLayoutSetActive(sub, has_output);
  uiItemR(sub, ptr, "use_invert_output", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);

  uiItemR(layout, ptr, "object", UI_ITEM_NONE, nullptr, ICON_NONE);

  sub = uiLayoutColumn(layout, true);
  uiItemR(sub, ptr, "distance_start", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(sub, ptr, "distance_end", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "minimum_weight", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_multiply", UI_ITEM_NONE, nullptr, ICON_NONE);

  if (uiLayout *influence_panel = uiLayoutPanelProp(
          C, layout, ptr, "open_influence_panel", "Influence"))
  {
    modifier::greasepencil::draw_layer_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_material_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_vertex_group_settings(C, influence_panel, ptr);
  }

  modifier_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_GreasePencilWeightProximity, panel_draw);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilWeightProximity = {
    /*idname*/ "GreasePencilWeightProximityModifier",
    /*name*/ N_("Weight Proximity"),
    /*struct_name*/ "GreasePencilWeightProximityModifierData",
    /*struct_size*/ sizeof(GreasePencilWeightProximityModifierData),
    /*srna*/ &RNA_GreasePencilWeightProximityModifier,
    /*type*/ ModifierTypeType::NonGeometrical,
    /*flags*/
    eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping,
    /*icon*/ ICON_MOD_VERTEX_WEIGHT,

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
    /*is_disabled*/ blender::is_disabled,
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
