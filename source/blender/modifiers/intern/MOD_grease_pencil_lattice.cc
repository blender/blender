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
#include "BKE_lattice.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"

#include "BLO_read_write.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLT_translation.hh"

#include "WM_types.hh"

#include "RNA_prototypes.hh"

#include "MOD_grease_pencil_util.hh"
#include "MOD_ui_common.hh"

namespace blender {

using bke::greasepencil::Drawing;
using bke::greasepencil::FramesMapKeyT;
using bke::greasepencil::Layer;

static void init_data(ModifierData *md)
{
  auto *lmd = reinterpret_cast<GreasePencilLatticeModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(lmd, modifier));

  MEMCPY_STRUCT_AFTER(lmd, DNA_struct_default_get(GreasePencilLatticeModifierData), modifier);
  modifier::greasepencil::init_influence_data(&lmd->influence, false);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const auto *lmd = reinterpret_cast<const GreasePencilLatticeModifierData *>(md);
  auto *tlmd = reinterpret_cast<GreasePencilLatticeModifierData *>(target);

  modifier::greasepencil::free_influence_data(&tlmd->influence);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&lmd->influence, &tlmd->influence, flag);
}

static void free_data(ModifierData *md)
{
  auto *lmd = reinterpret_cast<GreasePencilLatticeModifierData *>(md);
  modifier::greasepencil::free_influence_data(&lmd->influence);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *lmd = reinterpret_cast<GreasePencilLatticeModifierData *>(md);
  modifier::greasepencil::foreach_influence_ID_link(&lmd->influence, ob, walk, user_data);

  walk(user_data, ob, (ID **)&lmd->object, IDWALK_CB_NOP);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  auto *lmd = reinterpret_cast<GreasePencilLatticeModifierData *>(md);
  if (lmd->object != nullptr) {
    DEG_add_object_relation(
        ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Grease Pencil Lattice Modifier");
    DEG_add_object_relation(
        ctx->node, lmd->object, DEG_OB_COMP_GEOMETRY, "Grease Pencil Lattice Modifier");
  }
  DEG_add_depends_on_transform_relation(ctx->node, "Grease Pencil Lattice Modifier");
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  auto *lmd = reinterpret_cast<GreasePencilLatticeModifierData *>(md);

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the lattice is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  return lmd->object == nullptr || lmd->object->type != OB_LATTICE;
}

static void modify_curves(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          const LatticeDeformData &cache_data,
                          Drawing &drawing)
{
  const auto *lmd = reinterpret_cast<GreasePencilLatticeModifierData *>(md);
  modifier::greasepencil::ensure_no_bezier_curves(drawing);
  bke::CurvesGeometry &curves = drawing.strokes_for_write();

  IndexMaskMemory mask_memory;
  const IndexMask curves_mask = modifier::greasepencil::get_filtered_stroke_mask(
      ctx->object, curves, lmd->influence, mask_memory);

  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  MutableSpan<float3> positions = curves.positions_for_write();
  const VArray<float> vgroup_weights = modifier::greasepencil::get_influence_vertex_weights(
      curves, lmd->influence);

  curves_mask.foreach_index(GrainSize(512), [&](const int64_t curve_i) {
    const IndexRange points = points_by_curve[curve_i];
    for (const int64_t point_i : points) {
      const float weight = vgroup_weights[point_i];
      BKE_lattice_deform_data_eval_co(const_cast<LatticeDeformData *>(&cache_data),
                                      positions[point_i],
                                      lmd->strength * weight);
    }
  });

  drawing.tag_positions_changed();
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  const auto *lmd = reinterpret_cast<GreasePencilLatticeModifierData *>(md);
  BLI_assert(lmd->object != nullptr && lmd->object->type == OB_LATTICE);
  LatticeDeformData *cache_data = BKE_lattice_deform_data_create(lmd->object, ctx->object);

  if (!geometry_set->has_grease_pencil()) {
    return;
  }
  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, lmd->influence, mask_memory);
  const int frame = grease_pencil.runtime->eval_frame;
  const Vector<Drawing *> drawings = modifier::greasepencil::get_drawings_for_write(
      grease_pencil, layer_mask, frame);
  threading::parallel_for_each(
      drawings, [&](Drawing *drawing) { modify_curves(md, ctx, *cache_data, *drawing); });

  BKE_lattice_deform_data_destroy(cache_data);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  layout->prop(ptr, "object", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "strength", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);

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
  modifier_panel_register(region_type, eModifierType_GreasePencilLattice, panel_draw);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *lmd = reinterpret_cast<const GreasePencilLatticeModifierData *>(md);

  BLO_write_struct(writer, GreasePencilLatticeModifierData, lmd);
  modifier::greasepencil::write_influence_data(writer, &lmd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *lmd = reinterpret_cast<GreasePencilLatticeModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &lmd->influence);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilLattice = {
    /*idname*/ "GreasePencilLattice",
    /*name*/ N_("Lattice"),
    /*struct_name*/ "GreasePencilLatticeModifierData",
    /*struct_size*/ sizeof(GreasePencilLatticeModifierData),
    /*srna*/ &RNA_GreasePencilLatticeModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping,
    /*icon*/ ICON_MOD_LATTICE,

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
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
