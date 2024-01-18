/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BLO_read_write.hh"

#include "DNA_defaults.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"

#include "GEO_subdivide_curves.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "ED_grease_pencil.hh"

#include "MOD_grease_pencil_util.hh"
#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#include "RNA_prototypes.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

namespace blender {

static void init_data(ModifierData *md)
{
  GreasePencilSubdivModifierData *gpmd = reinterpret_cast<GreasePencilSubdivModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(GreasePencilSubdivModifierData), modifier);
  modifier::greasepencil::init_influence_data(&gpmd->influence, true);
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

static void deform_drawing(ModifierData &md,
                           Depsgraph * /*depsgraph*/,
                           Object &ob,
                           bke::greasepencil::Drawing &drawing)
{
  GreasePencilSubdivModifierData &mmd = reinterpret_cast<GreasePencilSubdivModifierData &>(md);

  IndexMaskMemory memory;
  const IndexMask strokes = modifier::greasepencil::get_filtered_stroke_mask(
      &ob, drawing.strokes_for_write(), mmd.influence, memory);

  VArray<int> cuts = VArray<int>::ForSingle(mmd.level, drawing.strokes().points_num());

  drawing.strokes_for_write() = geometry::subdivide_curves(
      drawing.strokes(), strokes, std::move(cuts), {});
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
  const int current_frame = DEG_get_evaluated_scene(ctx->depsgraph)->r.cfra;

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, mmd->influence, mask_memory);
  const Vector<bke::greasepencil::Drawing *> drawings =
      modifier::greasepencil::get_drawings_for_write(grease_pencil, layer_mask, current_frame);

  threading::parallel_for_each(drawings, [&](bke::greasepencil::Drawing *drawing) {
    deform_drawing(*md, ctx->depsgraph, *ctx->object, *drawing);
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

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "level", UI_ITEM_NONE, IFACE_("Subdivisions"), ICON_NONE);

  if (uiLayout *influence_panel = uiLayoutPanel(
          C, layout, "Influence", ptr, "open_influence_panel"))
  {
    modifier::greasepencil::draw_layer_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_material_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_vertex_group_settings(C, influence_panel, ptr);
  }

  modifier_panel_end(layout, ptr);
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
    (eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
     eModifierTypeFlag_EnableInEditmode),
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
};
