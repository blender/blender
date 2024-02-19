/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_index_mask.hh"
#include "BLI_math_rotation.hh"
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
  GreasePencilWeightAngleModifierData *gpmd =
      reinterpret_cast<GreasePencilWeightAngleModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(GreasePencilWeightAngleModifierData), modifier);
  modifier::greasepencil::init_influence_data(&gpmd->influence, false);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const GreasePencilWeightAngleModifierData *gmd =
      reinterpret_cast<const GreasePencilWeightAngleModifierData *>(md);
  GreasePencilWeightAngleModifierData *tgmd =
      reinterpret_cast<GreasePencilWeightAngleModifierData *>(target);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&gmd->influence, &tgmd->influence, flag);
}

static void free_data(ModifierData *md)
{
  GreasePencilWeightAngleModifierData *mmd =
      reinterpret_cast<GreasePencilWeightAngleModifierData *>(md);

  modifier::greasepencil::free_influence_data(&mmd->influence);
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  GreasePencilWeightAngleModifierData *mmd = (GreasePencilWeightAngleModifierData *)md;

  return (mmd->target_vgname[0] == '\0');
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  GreasePencilWeightAngleModifierData *mmd =
      reinterpret_cast<GreasePencilWeightAngleModifierData *>(md);

  modifier::greasepencil::foreach_influence_ID_link(&mmd->influence, ob, walk, user_data);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const GreasePencilWeightAngleModifierData *mmd =
      reinterpret_cast<const GreasePencilWeightAngleModifierData *>(md);

  BLO_write_struct(writer, GreasePencilWeightAngleModifierData, mmd);
  modifier::greasepencil::write_influence_data(writer, &mmd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  GreasePencilWeightAngleModifierData *mmd =
      reinterpret_cast<GreasePencilWeightAngleModifierData *>(md);
  modifier::greasepencil::read_influence_data(reader, &mmd->influence);
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
  const auto &mmd = reinterpret_cast<const GreasePencilWeightAngleModifierData &>(md);
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

  const VArray<float> input_weights = modifier::greasepencil::get_influence_vertex_weights(
      curves, mmd.influence);

  /* Use default Z up. */
  const float3 z_up(0.0f, 0.0f, 1.0f);
  float3 axis(0.0f);
  axis[mmd.axis] = 1.0f;
  float3 vec_ref;
  /* Apply modifier rotation (sub 90 degrees for Y axis due Z-Up vector). */
  const float rot_angle = mmd.angle - ((mmd.axis == 1) ? M_PI_2 : 0.0f);
  rotate_normalized_v3_v3v3fl(vec_ref, z_up, axis, rot_angle);

  const float3x3 obmat3x3(ob.object_to_world());

  /* Apply the rotation of the object. */
  if (mmd.space == MOD_GREASE_PENCIL_WEIGHT_ANGLE_SPACE_LOCAL) {
    vec_ref = math::transform_point(obmat3x3, vec_ref);
  }

  const OffsetIndices points_by_curve = curves.points_by_curve();
  const Span<float3> positions = curves.positions();

  strokes.foreach_index(GrainSize(512), [&](const int stroke) {
    const IndexRange points = points_by_curve[stroke];
    if (points.size() == 1) {
      dst_weights.span[points.start()] = 1.0f;
      return;
    }
    for (const int point : points.drop_front(1)) {
      const float3 p1 = math::transform_point(obmat3x3, positions[point]);
      const float3 p2 = math::transform_point(obmat3x3, positions[point - 1]);
      const float3 vec = p2 - p1;
      const float angle = angle_on_axis_v3v3_v3(vec_ref, vec, axis);
      float weight = 1.0f - math::sin(angle);

      if (mmd.flag & MOD_GREASE_PENCIL_WEIGHT_ANGLE_INVERT_OUTPUT) {
        weight = 1.0f - weight;
      }

      dst_weights.span[point] = (mmd.flag & MOD_GREASE_PENCIL_WEIGHT_ANGLE_MULTIPLY_DATA) ?
                                    dst_weights.span[point] * weight :
                                    weight;
      dst_weights.span[point] = math::clamp(dst_weights.span[point], mmd.min_weight, 1.0f);
    }
    /* First point has the same weight as the second one. */
    dst_weights.span[points[0]] = dst_weights.span[points[1]];
  });

  dst_weights.finish();
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  const GreasePencilWeightAngleModifierData *mmd =
      reinterpret_cast<GreasePencilWeightAngleModifierData *>(md);

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

  uiItemR(layout, ptr, "angle", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "axis", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "space", UI_ITEM_NONE, nullptr, ICON_NONE);

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
  modifier_panel_register(region_type, eModifierType_GreasePencilWeightAngle, panel_draw);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilWeightAngle = {
    /*idname*/ "GreasePencilWeightAngleModifier",
    /*name*/ N_("Weight Angle"),
    /*struct_name*/ "GreasePencilWeightAngleModifierData",
    /*struct_size*/ sizeof(GreasePencilWeightAngleModifierData),
    /*srna*/ &RNA_GreasePencilWeightAngleModifier,
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
