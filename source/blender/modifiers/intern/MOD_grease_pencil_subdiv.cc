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

#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_modifier.hh"

#include "GEO_subdivide_curves.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "MOD_grease_pencil_util.hh"
#include "MOD_ui_common.hh"

#include "RNA_prototypes.hh"

namespace blender {

static void init_data(ModifierData *md)
{
  GreasePencilSubdivModifierData *gpmd = reinterpret_cast<GreasePencilSubdivModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(GreasePencilSubdivModifierData), modifier);
  modifier::greasepencil::init_influence_data(&gpmd->influence, false);
}

static void free_data(ModifierData *md)
{
  GreasePencilSubdivModifierData *mmd = reinterpret_cast<GreasePencilSubdivModifierData *>(md);

  modifier::greasepencil::free_influence_data(&mmd->influence);
}

static void copy_data(const ModifierData *md, ModifierData *target, int flag)
{
  const GreasePencilSubdivModifierData *gmd =
      reinterpret_cast<const GreasePencilSubdivModifierData *>(md);
  GreasePencilSubdivModifierData *tgmd = reinterpret_cast<GreasePencilSubdivModifierData *>(
      target);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&gmd->influence, &tgmd->influence, flag);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const GreasePencilSubdivModifierData *mmd =
      reinterpret_cast<const GreasePencilSubdivModifierData *>(md);

  BLO_write_struct(writer, GreasePencilSubdivModifierData, mmd);
  modifier::greasepencil::write_influence_data(writer, &mmd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  GreasePencilSubdivModifierData *mmd = reinterpret_cast<GreasePencilSubdivModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &mmd->influence);
}

static void subdivide_drawing(ModifierData &md, Object &ob, bke::greasepencil::Drawing &drawing)
{
  GreasePencilSubdivModifierData &mmd = reinterpret_cast<GreasePencilSubdivModifierData &>(md);
  const bool use_catmull_clark = mmd.type == MOD_GREASE_PENCIL_SUBDIV_CATMULL;

  IndexMaskMemory memory;
  const IndexMask strokes = modifier::greasepencil::get_filtered_stroke_mask(
      &ob, drawing.strokes_for_write(), mmd.influence, memory);

  if (use_catmull_clark) {
    modifier::greasepencil::ensure_no_bezier_curves(drawing);
    bke::CurvesGeometry subdivided_curves = drawing.strokes();
    const VArray<bool> cyclic = subdivided_curves.cyclic();
    for ([[maybe_unused]] const int level_i : IndexRange(mmd.level)) {
      VArray<int> one_cut = VArray<int>::from_single(1, subdivided_curves.points_num());
      subdivided_curves = geometry::subdivide_curves(
          subdivided_curves, strokes, std::move(one_cut), {});

      offset_indices::OffsetIndices<int> points_by_curve = subdivided_curves.points_by_curve();
      const Array<float3> src_positions = subdivided_curves.positions();
      MutableSpan<float3> dst_positions = subdivided_curves.positions_for_write();
      threading::parallel_for(subdivided_curves.curves_range(), 1024, [&](const IndexRange range) {
        for (const int curve_i : range) {
          const IndexRange points = points_by_curve[curve_i];
          for (const int point_i : points.drop_front(1).drop_back(1)) {
            dst_positions[point_i] = math::interpolate(
                src_positions[point_i],
                math::interpolate(src_positions[point_i - 1], src_positions[point_i + 1], 0.5f),
                0.5f);
          }

          if (cyclic[curve_i] && points.size() > 1) {
            const float3 &first_pos = src_positions[points.first()];
            const float3 &last_pos = src_positions[points.last()];
            const float3 &after_first_pos = src_positions[points.first() + 1];
            const float3 &before_last_pos = src_positions[points.last() - 1];
            dst_positions[points.first()] = math::interpolate(
                first_pos, math::interpolate(last_pos, after_first_pos, 0.5f), 0.5f);
            dst_positions[points.last()] = math::interpolate(
                last_pos, math::interpolate(before_last_pos, first_pos, 0.5f), 0.5f);
          }
        }
      });
    }
    drawing.strokes_for_write() = subdivided_curves;
  }
  else {
    VArray<int> cuts = VArray<int>::from_single(math::pow(mmd.level, 2),
                                                drawing.strokes().points_num());
    drawing.strokes_for_write() = geometry::subdivide_curves(drawing.strokes(), strokes, cuts, {});
  }

  drawing.tag_topology_changed();
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  GreasePencilSubdivModifierData *mmd = reinterpret_cast<GreasePencilSubdivModifierData *>(md);

  if (mmd->level < 1 || !geometry_set->has_grease_pencil()) {
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
    subdivide_drawing(*md, *ctx->object, *drawing);
  });
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  GreasePencilSubdivModifierData *mmd = reinterpret_cast<GreasePencilSubdivModifierData *>(md);

  modifier::greasepencil::foreach_influence_ID_link(&mmd->influence, ob, walk, user_data);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  layout->prop(ptr, "subdivision_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "level", UI_ITEM_NONE, IFACE_("Subdivisions"), ICON_NONE);

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
  modifier_panel_register(region_type, eModifierType_GreasePencilSubdiv, panel_draw);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilSubdiv = {
    /*idname*/ "GreasePencilSubdivModifier",
    /*name*/ N_("Subdivide"),
    /*struct_name*/ "GreasePencilSubdivModifierData",
    /*struct_size*/ sizeof(GreasePencilSubdivModifierData),
    /*srna*/ &RNA_GreasePencilSubdivModifier,
    /*type*/ ModifierTypeType::Constructive,
    /*flags*/
    eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode,
    /*icon*/ ICON_MOD_SUBSURF,

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
