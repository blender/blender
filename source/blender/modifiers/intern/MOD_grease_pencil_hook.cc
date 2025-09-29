/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_index_mask.hh"

#include "BLT_translation.hh"

#include "BLO_read_write.hh"

#include "DNA_defaults.h"
#include "DNA_modifier_types.h"
#include "DNA_screen_types.h"

#include "RNA_access.hh"

#include "BKE_action.hh"
#include "BKE_colortools.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_object_types.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "MOD_grease_pencil_util.hh"
#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#include "RNA_prototypes.hh"

namespace blender {

static void init_data(ModifierData *md)
{
  auto *gpmd = reinterpret_cast<GreasePencilHookModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(GreasePencilHookModifierData), modifier);
  modifier::greasepencil::init_influence_data(&gpmd->influence, true);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const auto *gmd = reinterpret_cast<const GreasePencilHookModifierData *>(md);
  auto *tgmd = reinterpret_cast<GreasePencilHookModifierData *>(target);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&gmd->influence, &tgmd->influence, flag);
}

static void free_data(ModifierData *md)
{
  auto *mmd = reinterpret_cast<GreasePencilHookModifierData *>(md);

  modifier::greasepencil::free_influence_data(&mmd->influence);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  auto *mmd = reinterpret_cast<GreasePencilHookModifierData *>(md);

  return (mmd->object == nullptr);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  auto *mmd = reinterpret_cast<GreasePencilHookModifierData *>(md);
  if (mmd->object != nullptr) {
    DEG_add_object_relation(ctx->node, mmd->object, DEG_OB_COMP_TRANSFORM, "Hook Modifier");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Hook Modifier");
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *mmd = reinterpret_cast<GreasePencilHookModifierData *>(md);

  modifier::greasepencil::foreach_influence_ID_link(&mmd->influence, ob, walk, user_data);

  walk(user_data, ob, (ID **)&mmd->object, IDWALK_CB_NOP);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *mmd = reinterpret_cast<const GreasePencilHookModifierData *>(md);

  BLO_write_struct(writer, GreasePencilHookModifierData, mmd);
  modifier::greasepencil::write_influence_data(writer, &mmd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *mmd = reinterpret_cast<GreasePencilHookModifierData *>(md);
  modifier::greasepencil::read_influence_data(reader, &mmd->influence);
}

/* Calculate the factor of falloff. */
static float hook_falloff(const float falloff,
                          const int falloff_type,
                          const float falloff_sq,
                          const float fac_orig,
                          const CurveMapping *curfalloff,
                          const float len_sq)
{
  BLI_assert(falloff_sq);
  if (len_sq > falloff_sq) {
    return 0.0f;
  }
  if (len_sq <= 0.0f) {
    return fac_orig;
  }
  if (falloff_type == MOD_GREASE_PENCIL_HOOK_Falloff_Const) {
    return fac_orig;
  }
  if (falloff_type == MOD_GREASE_PENCIL_HOOK_Falloff_InvSquare) {
    /* Avoid sqrt below. */
    return (1.0f - (len_sq / falloff_sq)) * fac_orig;
  }

  float fac = 1.0f - (math::sqrt(len_sq) / falloff);

  switch (falloff_type) {
    case MOD_GREASE_PENCIL_HOOK_Falloff_Curve:
      return BKE_curvemapping_evaluateF(curfalloff, 0, fac) * fac_orig;
    case MOD_GREASE_PENCIL_HOOK_Falloff_Sharp:
      return fac * fac * fac_orig;
    case MOD_GREASE_PENCIL_HOOK_Falloff_Smooth:
      return (3.0f * fac * fac - 2.0f * fac * fac * fac) * fac_orig;
    case MOD_GREASE_PENCIL_HOOK_Falloff_Root:
      return math::sqrt(fac) * fac_orig;
      break;
    case MOD_GREASE_PENCIL_HOOK_Falloff_Sphere:
      return math::sqrt(2 * fac - fac * fac) * fac_orig;
    case MOD_GREASE_PENCIL_HOOK_Falloff_Linear:
      ATTR_FALLTHROUGH; /* Pass. */
    default:
      return fac * fac_orig;
  }
}

static void deform_drawing(const ModifierData &md,
                           const Object &ob,
                           bke::greasepencil::Drawing &drawing)
{
  const auto &mmd = reinterpret_cast<const GreasePencilHookModifierData &>(md);
  modifier::greasepencil::ensure_no_bezier_curves(drawing);
  bke::CurvesGeometry &curves = drawing.strokes_for_write();

  if (curves.is_empty()) {
    return;
  }
  IndexMaskMemory memory;
  const IndexMask strokes = modifier::greasepencil::get_filtered_stroke_mask(
      &ob, curves, mmd.influence, memory);
  if (strokes.is_empty()) {
    return;
  }

  const VArray<float> input_weights = modifier::greasepencil::get_influence_vertex_weights(
      curves, mmd.influence);

  const int falloff_type = mmd.falloff_type;
  const float falloff = (mmd.falloff_type == eHook_Falloff_None) ? 0.0f : mmd.falloff;
  const float falloff_sq = square_f(falloff);
  const float fac_orig = mmd.force;
  const bool use_falloff = falloff_sq != 0.0f;
  const bool use_uniform = (mmd.flag & MOD_GREASE_PENCIL_HOOK_UNIFORM_SPACE) != 0;

  const float3x3 mat_uniform = use_uniform ? float3x3(float4x4(mmd.parentinv)) :
                                             float3x3::identity();
  const float3 cent = use_uniform ? math::transform_point(mat_uniform, float3(mmd.cent)) :
                                    float3(mmd.cent);

  float4x4 dmat;
  /* Get world-space matrix of target, corrected for the space the verts are in. */
  if (mmd.subtarget[0]) {
    bPoseChannel *pchan = BKE_pose_channel_find_name(mmd.object->pose, mmd.subtarget);
    if (pchan) {
      /* Bone target if there's a matching pose-channel. */
      dmat = mmd.object->object_to_world() * float4x4(pchan->pose_mat);
    }
  }
  else {
    /* Just object target. */
    dmat = mmd.object->object_to_world();
  }
  float4x4 use_mat = ob.world_to_object() * dmat * float4x4(mmd.parentinv);

  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  MutableSpan<float3> positions = curves.positions_for_write();

  strokes.foreach_index(blender::GrainSize(128), [&](const int stroke) {
    const IndexRange points_range = points_by_curve[stroke].index_range();
    for (const int point_i : points_range) {
      const int point = point_i + points_by_curve[stroke].first();
      const float weight = input_weights[point];
      if (weight < 0.0f) {
        continue;
      }

      float fac;
      if (use_falloff) {
        float len_sq;
        if (use_uniform) {
          const float3 co_uniform = math::transform_point(mat_uniform, positions[point]);
          len_sq = math::distance(cent, co_uniform);
        }
        else {
          len_sq = math::distance(cent, positions[point]);
        }
        fac = hook_falloff(
            falloff, falloff_type, falloff_sq, fac_orig, mmd.influence.custom_curve, len_sq);
      }
      else {
        fac = fac_orig;
      }

      if (fac != 0.0f) {
        const float3 co_tmp = math::transform_point(use_mat, positions[point]);
        positions[point] = math::interpolate(positions[point], co_tmp, fac * weight);
      }
    }
  });

  drawing.tag_positions_changed();
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  const GreasePencilHookModifierData *mmd = reinterpret_cast<GreasePencilHookModifierData *>(md);

  if (!geometry_set->has_grease_pencil()) {
    return;
  }

  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();

  const int current_frame = grease_pencil.runtime->eval_frame;

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, mmd->influence, mask_memory);
  const Vector<bke::greasepencil::Drawing *> drawings =
      modifier::greasepencil::get_drawings_for_write(grease_pencil, layer_mask, current_frame);

  threading::parallel_for_each(drawings, [&](bke::greasepencil::Drawing *drawing) {
    deform_drawing(*md, *ctx->object, *drawing);
  });
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA hook_object_ptr = RNA_pointer_get(ptr, "object");

  layout->use_property_split_set(true);

  uiLayout *col = &layout->column(false);
  col->prop(ptr, "object", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (!RNA_pointer_is_null(&hook_object_ptr) &&
      RNA_enum_get(&hook_object_ptr, "type") == OB_ARMATURE)
  {
    PointerRNA hook_object_data_ptr = RNA_pointer_get(&hook_object_ptr, "data");
    col->prop_search(ptr, "subtarget", &hook_object_data_ptr, "bones", IFACE_("Bone"), ICON_NONE);
  }

  layout->prop(ptr, "strength", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);

  if (uiLayout *sub = layout->panel_prop(C, ptr, "open_falloff_panel", IFACE_("Falloff"))) {
    sub->use_property_split_set(true);

    sub->prop(ptr, "falloff_type", UI_ITEM_NONE, IFACE_("Type"), ICON_NONE);

    bool use_falloff = RNA_enum_get(ptr, "falloff_type") != eWarp_Falloff_None;

    uiLayout *row = &sub->row(false);
    row->active_set(use_falloff);
    row->prop(ptr, "falloff_radius", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    sub->prop(ptr, "use_falloff_uniform", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    if (RNA_enum_get(ptr, "falloff_type") == eWarp_Falloff_Curve) {
      uiTemplateCurveMapping(sub, ptr, "custom_curve", 0, false, false, false, false, false);
    }
  }

  if (uiLayout *influence_panel = layout->panel_prop(
          C, ptr, "open_influence_panel", IFACE_("Influence")))
  {
    modifier::greasepencil::draw_layer_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_material_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_vertex_group_settings(C, influence_panel, ptr);
  }

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_GreasePencilHook, panel_draw);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilHook = {
    /*idname*/ "GreasePencilHookModifier",
    /*name*/ N_("Hook"),
    /*struct_name*/ "GreasePencilHookModifierData",
    /*struct_size*/ sizeof(GreasePencilHookModifierData),
    /*srna*/ &RNA_GreasePencilHookModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/
    eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping,
    /*icon*/ ICON_HOOK,

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
