/* SPDX-FileCopyrightText: 2024 Blender Authors
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

#include "BKE_geometry_fields.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_modifier.hh"

#include "ED_grease_pencil.hh"

#include "GEO_resample_curves.hh"
#include "GEO_simplify_curves.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "MOD_grease_pencil_util.hh"
#include "MOD_ui_common.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

namespace blender {

static void init_data(ModifierData *md)
{
  auto *gpmd = reinterpret_cast<GreasePencilSimplifyModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(GreasePencilSimplifyModifierData), modifier);
  modifier::greasepencil::init_influence_data(&gpmd->influence, true);
}

static void free_data(ModifierData *md)
{
  auto *mmd = reinterpret_cast<GreasePencilSimplifyModifierData *>(md);

  modifier::greasepencil::free_influence_data(&mmd->influence);
}

static void copy_data(const ModifierData *md, ModifierData *target, int flag)
{
  const auto *gmd = reinterpret_cast<const GreasePencilSimplifyModifierData *>(md);
  auto *tgmd = reinterpret_cast<GreasePencilSimplifyModifierData *>(target);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&gmd->influence, &tgmd->influence, flag);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *mmd = reinterpret_cast<const GreasePencilSimplifyModifierData *>(md);

  BLO_write_struct(writer, GreasePencilSimplifyModifierData, mmd);
  modifier::greasepencil::write_influence_data(writer, &mmd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *mmd = reinterpret_cast<GreasePencilSimplifyModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &mmd->influence);
}

static IndexMask simplify_fixed(const bke::CurvesGeometry &curves,
                                const int step,
                                IndexMaskMemory &memory)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const Array<int> point_to_curve_map = curves.point_to_curve_map();
  return IndexMask::from_predicate(
      curves.points_range(), GrainSize(2048), memory, [&](const int64_t i) {
        const int curve_i = point_to_curve_map[i];
        const IndexRange points = points_by_curve[curve_i];
        if (points.size() <= 2) {
          return true;
        }
        const int local_i = i - points.start();
        return (local_i % int(math::pow(2.0f, float(step))) == 0) || points.last() == i;
      });
}

static void simplify_drawing(const GreasePencilSimplifyModifierData &mmd,
                             const Object &ob,
                             bke::greasepencil::Drawing &drawing)
{
  modifier::greasepencil::ensure_no_bezier_curves(drawing);
  const bke::CurvesGeometry &curves = drawing.strokes();

  IndexMaskMemory memory;
  const IndexMask strokes = modifier::greasepencil::get_filtered_stroke_mask(
      &ob, curves, mmd.influence, memory);
  if (strokes.is_empty()) {
    return;
  }

  switch (mmd.mode) {
    case MOD_GREASE_PENCIL_SIMPLIFY_FIXED: {
      const IndexMask points_to_keep = simplify_fixed(curves, mmd.step, memory);
      if (points_to_keep.is_empty()) {
        drawing.strokes_for_write() = {};
        break;
      }
      if (points_to_keep.size() == curves.points_num()) {
        break;
      }
      drawing.strokes_for_write() = bke::curves_copy_point_selection(curves, points_to_keep, {});
      break;
    }
    case MOD_GREASE_PENCIL_SIMPLIFY_ADAPTIVE: {
      const IndexMask points_to_delete = geometry::simplify_curve_attribute(
          curves.positions(),
          strokes,
          curves.points_by_curve(),
          curves.cyclic(),
          mmd.factor,
          curves.positions(),
          memory);
      drawing.strokes_for_write().remove_points(points_to_delete, {});
      break;
    }
    case MOD_GREASE_PENCIL_SIMPLIFY_SAMPLE: {
      drawing.strokes_for_write() = geometry::resample_to_length(
          curves, strokes, VArray<float>::from_single(mmd.length, curves.curves_num()), {});
      break;
    }
    case MOD_GREASE_PENCIL_SIMPLIFY_MERGE: {
      const OffsetIndices points_by_curve = curves.points_by_curve();
      const Array<int> point_to_curve_map = curves.point_to_curve_map();
      const IndexMask points = IndexMask::from_predicate(
          curves.points_range(), GrainSize(2048), memory, [&](const int64_t i) {
            const int curve_i = point_to_curve_map[i];
            const IndexRange points = points_by_curve[curve_i];
            if (points.drop_front(1).drop_back(1).contains(i)) {
              return true;
            }
            return false;
          });
      drawing.strokes_for_write() = ed::greasepencil::curves_merge_by_distance(
          curves, mmd.distance, points, {});
      break;
    }
    default:
      break;
  }

  drawing.tag_topology_changed();
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  const auto *mmd = reinterpret_cast<GreasePencilSimplifyModifierData *>(md);

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
    simplify_drawing(*mmd, *ctx->object, *drawing);
  });
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *mmd = reinterpret_cast<GreasePencilSimplifyModifierData *>(md);

  modifier::greasepencil::foreach_influence_ID_link(&mmd->influence, ob, walk, user_data);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  int mode = RNA_enum_get(ptr, "mode");

  layout->use_property_split_set(true);

  layout->prop(ptr, "mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (mode == MOD_GREASE_PENCIL_SIMPLIFY_FIXED) {
    layout->prop(ptr, "step", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else if (mode == MOD_GREASE_PENCIL_SIMPLIFY_ADAPTIVE) {
    layout->prop(ptr, "factor", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else if (mode == MOD_GREASE_PENCIL_SIMPLIFY_SAMPLE) {
    layout->prop(ptr, "length", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(ptr, "sharp_threshold", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else if (mode == MOD_GREASE_PENCIL_SIMPLIFY_MERGE) {
    layout->prop(ptr, "distance", UI_ITEM_NONE, std::nullopt, ICON_NONE);
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
  modifier_panel_register(region_type, eModifierType_GreasePencilSimplify, panel_draw);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilSimplify = {
    /*idname*/ "GreasePencilSimplifyModifier",
    /*name*/ N_("Simplify"),
    /*struct_name*/ "GreasePencilSimplifyModifierData",
    /*struct_size*/ sizeof(GreasePencilSimplifyModifierData),
    /*srna*/ &RNA_GreasePencilSimplifyModifier,
    /*type*/ ModifierTypeType::Nonconstructive,
    /*flags*/
    eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode,
    /*icon*/ ICON_MOD_SIMPLIFY,

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
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
