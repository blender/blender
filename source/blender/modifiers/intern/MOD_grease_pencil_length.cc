/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_hash.h"
#include "BLI_rand.h"
#include "BLI_task.h"

#include "BLT_translation.hh"

#include "BLO_read_write.hh"

#include "DNA_defaults.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_node_types.h" /* For `GeometryNodeCurveSampleMode` */
#include "DNA_object_types.h"

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

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "GEO_extend_curves.hh"
#include "GEO_trim_curves.hh"

namespace blender {

static void init_data(ModifierData *md)
{
  GreasePencilLengthModifierData *gpmd = reinterpret_cast<GreasePencilLengthModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(GreasePencilLengthModifierData), modifier);
  modifier::greasepencil::init_influence_data(&gpmd->influence, false);
}

static void copy_data(const ModifierData *md, ModifierData *target, int flags)
{
  const auto *omd = reinterpret_cast<const GreasePencilLengthModifierData *>(md);
  auto *tomd = reinterpret_cast<GreasePencilLengthModifierData *>(target);

  modifier::greasepencil::free_influence_data(&tomd->influence);

  BKE_modifier_copydata_generic(md, target, flags);
  modifier::greasepencil::copy_influence_data(&omd->influence, &tomd->influence, flags);
}

static void free_data(ModifierData *md)
{
  auto *omd = reinterpret_cast<GreasePencilLengthModifierData *>(md);
  modifier::greasepencil::free_influence_data(&omd->influence);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *omd = reinterpret_cast<GreasePencilLengthModifierData *>(md);
  modifier::greasepencil::foreach_influence_ID_link(&omd->influence, ob, walk, user_data);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const GreasePencilLengthModifierData *mmd =
      reinterpret_cast<const GreasePencilLengthModifierData *>(md);

  BLO_write_struct(writer, GreasePencilLengthModifierData, mmd);
  modifier::greasepencil::write_influence_data(writer, &mmd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  GreasePencilLengthModifierData *mmd = reinterpret_cast<GreasePencilLengthModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &mmd->influence);
}

static Array<float> noise_table(int len, int offset, int seed)
{
  Array<float> table(len);
  for (const int i : table.index_range()) {
    table[i] = BLI_hash_int_01(BLI_hash_int_2d(seed, i + offset + 1));
  }
  return table;
}

static float table_sample(MutableSpan<float> table, float x)
{
  return math::interpolate(table[int(math::ceil(x))], table[int(math::floor(x))], math::fract(x));
}

static void deform_drawing(const ModifierData &md,
                           const Object &ob,
                           bke::greasepencil::Drawing &drawing,
                           const int current_time)
{
  const GreasePencilLengthModifierData &mmd =
      reinterpret_cast<const GreasePencilLengthModifierData &>(md);
  bke::CurvesGeometry &curves = drawing.strokes_for_write();

  if (curves.points_num() == 0) {
    return;
  }

  IndexMaskMemory memory;
  const IndexMask selection = modifier::greasepencil::get_filtered_stroke_mask(
      &ob, curves, mmd.influence, memory);

  const int curves_num = curves.curves_num();

  /* Variable for tagging shrinking when values are adjusted after random. */
  std::atomic<bool> needs_additional_shrinking = false;

  VArray<float> use_starts = VArray<float>::ForSingle(mmd.start_fac, curves_num);
  VArray<float> use_ends = VArray<float>::ForSingle(mmd.end_fac, curves_num);

  if (mmd.rand_start_fac != 0.0 || mmd.rand_end_fac != 0.0) {
    /* Use random to modify start/end factors. Put the modified values outside the
     * branch so it could be accessed in later stretching/shrinking stages. */
    Array<float> modified_starts(curves.curves_num(), mmd.start_fac);
    Array<float> modified_ends(curves.curves_num(), mmd.end_fac);
    use_starts = VArray<float>::ForSpan(modified_starts.as_mutable_span());
    use_ends = VArray<float>::ForSpan(modified_ends.as_mutable_span());

    int seed = mmd.seed;

    /* Make sure different modifiers get different seeds. */
    seed += BLI_hash_string(ob.id.name + 2);
    seed += BLI_hash_string(md.name);

    if (mmd.flag & GP_LENGTH_USE_RANDOM) {
      seed += current_time / mmd.step;
    }

    float rand_offset = BLI_hash_int_01(seed);

    Array<float> noise_table_length = noise_table(
        4 + curves_num, int(math::floor(mmd.rand_offset)), seed + 2);

    threading::parallel_for(IndexRange(curves_num), 512, [&](const IndexRange parallel_range) {
      for (const int i : parallel_range) {
        /* To ensure a nice distribution, we use halton sequence and offset using the seed. */
        double r[2];
        const uint primes[2] = {2, 3};
        double offset[2] = {0.0f, 0.0f};
        BLI_halton_2d(primes, offset, i, r);

        float rand[2] = {0.0f, 0.0f};
        for (int j = 0; j < 2; j++) {
          float noise = table_sample(noise_table_length, i + j * 2 + math::fract(mmd.rand_offset));

          rand[j] = math::mod(float(r[j] + rand_offset), 1.0f);
          rand[j] = math::abs(
              math::mod(sin(rand[j] * 12.9898f + j * 78.233f) * 43758.5453f, 1.0f) + noise);
        }

        modified_starts[i] = modified_starts[i] + rand[0] * mmd.rand_start_fac;
        modified_ends[i] = modified_ends[i] + rand[1] * mmd.rand_end_fac;

        if (modified_starts[i] <= 0.0f || modified_ends[i] <= 0.0f) {
          needs_additional_shrinking.store(true, std::memory_order_relaxed);
        }
      }
    });
  }

  curves = geometry::extend_curves(curves,
                                   selection,
                                   use_starts,
                                   use_ends,
                                   mmd.overshoot_fac,
                                   (mmd.flag & GP_LENGTH_USE_CURVATURE) != 0,
                                   mmd.point_density,
                                   mmd.segment_influence,
                                   mmd.max_angle,
                                   (mmd.flag & GP_LENGTH_INVERT_CURVATURE) != 0,
                                   ((mmd.mode & GP_LENGTH_ABSOLUTE) != 0) ?
                                       GEO_NODE_CURVE_SAMPLE_LENGTH :
                                       GEO_NODE_CURVE_SAMPLE_FACTOR,
                                   {});

  /* Always do the stretching first since it might depend on points which could be deleted by the
   * shrink. */
  if (mmd.start_fac < 0.0f || mmd.end_fac < 0.0f || needs_additional_shrinking) {
    /* #trim_curves() accepts the `end` values if it's sampling from the beginning of the
     * curve, so we need to get the lengths of the curves and subtract it from the back when the
     * modifier is in Absolute mode. For convenience, we always call #trim_curves() in LENGTH
     * mode since the function itself will need length to be sampled anyway. */
    Array<float> starts(curves.curves_num());
    Array<float> ends(curves.curves_num());
    Array<bool> needs_removal(curves.curves_num());
    needs_removal.fill(false);

    curves.ensure_evaluated_lengths();

    threading::parallel_for(curves.curves_range(), 512, [&](const IndexRange parallel_range) {
      for (const int curve : parallel_range) {
        float length = curves.evaluated_length_total_for_curve(curve, false);
        if (mmd.mode & GP_LENGTH_ABSOLUTE) {
          starts[curve] = -math::min(use_starts[curve], 0.0f);
          ends[curve] = length - (-math::min(use_ends[curve], 0.0f));
        }
        else {
          starts[curve] = -math::min(use_starts[curve], 0.0f) * length;
          ends[curve] = (1 + math::min(use_ends[curve], 0.0f)) * length;
        }
        if (starts[curve] > ends[curve]) {
          needs_removal[curve] = true;
        }
      }
    });
    curves = geometry::trim_curves(curves,
                                   selection,
                                   VArray<float>::ForSpan(starts.as_span()),
                                   VArray<float>::ForSpan(ends.as_span()),
                                   GEO_NODE_CURVE_SAMPLE_LENGTH,
                                   {});

    /* #trim_curves() will leave the last segment there when trimmed length is greater than
     * curve original length, thus we need to remove those curves afterwards. */
    IndexMaskMemory memory_remove;
    const IndexMask to_remove = IndexMask::from_bools(needs_removal.as_span(), memory_remove);
    if (!to_remove.is_empty()) {
      curves.remove_curves(to_remove, {});
    }
  }

  drawing.tag_topology_changed();
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                blender::bke::GeometrySet *geometry_set)
{
  GreasePencilLengthModifierData *mmd = reinterpret_cast<GreasePencilLengthModifierData *>(md);

  if (!geometry_set->has_grease_pencil()) {
    return;
  }

  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, mmd->influence, mask_memory);
  const Vector<bke::greasepencil::Drawing *> drawings =
      modifier::greasepencil::get_drawings_for_write(
          grease_pencil, layer_mask, grease_pencil.runtime->eval_frame);

  threading::parallel_for_each(drawings, [&](bke::greasepencil::Drawing *drawing) {
    deform_drawing(*md, *ctx->object, *drawing, grease_pencil.runtime->eval_frame);
  });
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);
  uiItemR(layout, ptr, "mode", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiLayout *col = uiLayoutColumn(layout, true);

  if (RNA_enum_get(ptr, "mode") == GP_LENGTH_RELATIVE) {
    uiItemR(col, ptr, "start_factor", UI_ITEM_NONE, IFACE_("Start"), ICON_NONE);
    uiItemR(col, ptr, "end_factor", UI_ITEM_NONE, IFACE_("End"), ICON_NONE);
  }
  else {
    uiItemR(col, ptr, "start_length", UI_ITEM_NONE, IFACE_("Start"), ICON_NONE);
    uiItemR(col, ptr, "end_length", UI_ITEM_NONE, IFACE_("End"), ICON_NONE);
  }

  uiItemR(layout, ptr, "overshoot_factor", UI_ITEM_R_SLIDER, IFACE_("Used Length"), ICON_NONE);

  if (uiLayout *random_layout = uiLayoutPanelProp(
          C, layout, ptr, "open_random_panel", "Randomize"))
  {
    uiItemR(random_layout, ptr, "use_random", UI_ITEM_NONE, IFACE_("Randomize"), ICON_NONE);

    uiLayout *subcol = uiLayoutColumn(random_layout, false);
    uiLayoutSetPropSep(subcol, true);
    uiLayoutSetActive(subcol, RNA_boolean_get(ptr, "use_random"));

    uiItemR(subcol, ptr, "step", UI_ITEM_NONE, nullptr, ICON_NONE);

    uiItemR(subcol, ptr, "random_start_factor", UI_ITEM_NONE, IFACE_("Offset Start"), ICON_NONE);
    uiItemR(subcol, ptr, "random_end_factor", UI_ITEM_NONE, IFACE_("End"), ICON_NONE);
    uiItemR(subcol, ptr, "random_offset", UI_ITEM_NONE, IFACE_("Noise Offset"), ICON_NONE);
    uiItemR(subcol, ptr, "seed", UI_ITEM_NONE, nullptr, ICON_NONE);
  }

  if (uiLayout *curvature_layout = uiLayoutPanelProp(
          C, layout, ptr, "open_curvature_panel", "Curvature"))
  {
    uiItemR(curvature_layout, ptr, "use_curvature", UI_ITEM_NONE, IFACE_("Curvature"), ICON_NONE);

    uiLayout *subcol = uiLayoutColumn(curvature_layout, false);
    uiLayoutSetPropSep(subcol, true);
    uiLayoutSetActive(subcol, RNA_boolean_get(ptr, "use_curvature"));

    uiItemR(subcol, ptr, "point_density", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(subcol, ptr, "segment_influence", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(subcol, ptr, "max_angle", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(subcol, ptr, "invert_curvature", UI_ITEM_NONE, IFACE_("Invert"), ICON_NONE);
  }

  if (uiLayout *influence_panel = uiLayoutPanelProp(
          C, layout, ptr, "open_influence_panel", "Influence"))
  {
    modifier::greasepencil::draw_layer_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_material_filter_settings(C, influence_panel, ptr);
  }

  modifier_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_GreasePencilLength, panel_draw);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilLength = {
    /*idname*/ "GreasePencilLengthModifier",
    /*name*/ N_("Length"),
    /*struct_name*/ "GreasePencilLengthModifierData",
    /*struct_size*/ sizeof(GreasePencilLengthModifierData),
    /*srna*/ &RNA_GreasePencilLengthModifier,
    /*type*/ ModifierTypeType::Nonconstructive,
    /*flags*/
    eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_EnableInEditmode |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_LENGTH,

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
