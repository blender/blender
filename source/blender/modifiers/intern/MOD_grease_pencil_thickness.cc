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

#include "BKE_colortools.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_modifier.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "MOD_grease_pencil_util.hh"
#include "MOD_ui_common.hh"

#include "RNA_prototypes.h"

namespace blender {

static void init_data(ModifierData *md)
{
  GreasePencilThickModifierData *gpmd = reinterpret_cast<GreasePencilThickModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(GreasePencilThickModifierData), modifier);
  modifier::greasepencil::init_influence_data(&gpmd->influence, true);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const GreasePencilThickModifierData *gmd =
      reinterpret_cast<const GreasePencilThickModifierData *>(md);
  GreasePencilThickModifierData *tgmd = reinterpret_cast<GreasePencilThickModifierData *>(target);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&gmd->influence, &tgmd->influence, flag);
}

static void free_data(ModifierData *md)
{
  GreasePencilThickModifierData *mmd = reinterpret_cast<GreasePencilThickModifierData *>(md);

  modifier::greasepencil::free_influence_data(&mmd->influence);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  GreasePencilThickModifierData *mmd = reinterpret_cast<GreasePencilThickModifierData *>(md);

  modifier::greasepencil::foreach_influence_ID_link(&mmd->influence, ob, walk, user_data);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const GreasePencilThickModifierData *mmd =
      reinterpret_cast<const GreasePencilThickModifierData *>(md);

  BLO_write_struct(writer, GreasePencilThickModifierData, mmd);
  modifier::greasepencil::write_influence_data(writer, &mmd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  GreasePencilThickModifierData *mmd = reinterpret_cast<GreasePencilThickModifierData *>(md);
  modifier::greasepencil::read_influence_data(reader, &mmd->influence);
}

static void deform_drawing(const ModifierData &md,
                           const Object &ob,
                           bke::greasepencil::Drawing &drawing)
{
  const auto &mmd = reinterpret_cast<const GreasePencilThickModifierData &>(md);

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

  MutableSpan<float> radii = drawing.radii_for_write();
  const OffsetIndices points_by_curve = curves.points_by_curve();
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  const VArray<float> vgroup_weights = *attributes.lookup_or_default<float>(
      mmd.influence.vertex_group_name, bke::AttrDomain::Point, 1.0f);
  const bool is_normalized = (mmd.flag & MOD_GREASE_PENCIL_THICK_NORMALIZE) != 0;
  const bool is_inverted = ((mmd.flag & MOD_GREASE_PENCIL_THICK_WEIGHT_FACTOR) == 0) &&
                           ((mmd.influence.flag & GREASE_PENCIL_INFLUENCE_INVERT_VERTEX_GROUP) !=
                            0);

  strokes.foreach_index(GrainSize(512), [&](const int curve) {
    const IndexRange points = points_by_curve[curve];
    for (const int i : points.index_range()) {
      const int point = points[i];
      const float weight = vgroup_weights[point];
      if (weight <= 0.0f) {
        continue;
      }

      if ((!is_normalized) && (mmd.flag & MOD_GREASE_PENCIL_THICK_WEIGHT_FACTOR)) {
        radii[point] *= (is_inverted ? 1.0f - weight : weight);
        radii[point] = math::max(radii[point], 0.0f);
        continue;
      }

      const float influence = [&]() {
        if (mmd.influence.flag & GREASE_PENCIL_INFLUENCE_USE_CUSTOM_CURVE &&
            (mmd.influence.custom_curve))
        {
          /* Normalize value to evaluate curve. */
          const float value = math::safe_divide(float(i), float(points.size() - 1));
          return BKE_curvemapping_evaluateF(mmd.influence.custom_curve, 0, value);
        }
        return 1.0f;
      }();

      const float target = [&]() {
        if (is_normalized) {
          return mmd.thickness * influence;
        }
        return radii[point] * math::interpolate(1.0f, mmd.thickness_fac, influence);
      }();

      const float radius = math::interpolate(radii[point], target, weight);
      radii[point] = math::max(radius, 0.0f);
    }
  });
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  GreasePencilThickModifierData *mmd = reinterpret_cast<GreasePencilThickModifierData *>(md);

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

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "use_uniform_thickness", UI_ITEM_NONE, nullptr, ICON_NONE);
  if (RNA_boolean_get(ptr, "use_uniform_thickness")) {
    uiItemR(layout, ptr, "thickness", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else {
    const bool is_weighted = !RNA_boolean_get(ptr, "use_weight_factor");
    uiLayout *row = uiLayoutRow(layout, true);
    uiLayoutSetActive(row, is_weighted);
    uiItemR(row, ptr, "thickness_factor", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiLayout *sub = uiLayoutRow(row, true);
    uiLayoutSetActive(sub, true);
    uiItemR(row, ptr, "use_weight_factor", UI_ITEM_NONE, "", ICON_MOD_VERTEX_WEIGHT);
  }

  if (uiLayout *influence_panel = uiLayoutPanelProp(
          C, layout, ptr, "open_influence_panel", "Influence"))
  {
    modifier::greasepencil::draw_layer_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_material_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_vertex_group_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_custom_curve_settings(C, influence_panel, ptr);
  }

  modifier_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_GreasePencilThickness, panel_draw);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilThickness = {
    /*idname*/ "GreasePencilThicknessModifier",
    /*name*/ N_("Thickness"),
    /*struct_name*/ "GreasePencilThickModifierData",
    /*struct_size*/ sizeof(GreasePencilThickModifierData),
    /*srna*/ &RNA_GreasePencilThickModifierData,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/
    eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping,
    /*icon*/ ICON_MOD_THICKNESS,

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
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ blender::foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ blender::panel_register,
    /*blend_write*/ blender::blend_write,
    /*blend_read*/ blender::blend_read,
};
