/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_hash.h"
#include "BLI_math_vector.hh"
#include "BLI_rand.hh"
#include "BLI_task.hh"

#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_paint.hh"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "grease_pencil_intern.hh"
#include "paint_intern.hh"

namespace blender::ed::sculpt_paint::greasepencil {

/* Use a hash to generate random numbers. */
static float hash_rng(uint32_t seed1, uint32_t seed2, int index)
{
  return BLI_hash_int_01(BLI_hash_int_3d(seed1, seed2, uint32_t(index)));
}

class RandomizeOperation : public GreasePencilStrokeOperationCommon {
 public:
  using GreasePencilStrokeOperationCommon::GreasePencilStrokeOperationCommon;

  /* Get a different seed value for each stroke. */
  uint32_t unique_seed() const;

  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext & /*C*/) override {}
};

uint32_t RandomizeOperation::unique_seed() const
{
  return RandomNumberGenerator::from_random_seed().get_uint32();
}

void RandomizeOperation::on_stroke_begin(const bContext &C, const InputSample &start_sample)
{
  this->init_stroke(C, start_sample);
}

void RandomizeOperation::on_stroke_extended(const bContext &C, const InputSample &extension_sample)
{
  const Scene &scene = *CTX_data_scene(&C);
  Paint &paint = *BKE_paint_get_active_from_context(&C);
  const Brush &brush = *BKE_paint_brush(&paint);
  const int sculpt_mode_flag = brush.gpencil_settings->sculpt_mode_flag;

  this->foreach_editable_drawing(C, [&](const GreasePencilStrokeParams &params) {
    const uint32_t seed = this->unique_seed();

    IndexMaskMemory selection_memory;
    const IndexMask selection = point_selection_mask(params, selection_memory);
    if (selection.is_empty()) {
      return false;
    }

    Array<float2> view_positions = calculate_view_positions(params, selection);
    bke::CurvesGeometry &curves = params.drawing.strokes_for_write();
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

    bool changed = false;
    if (sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_POSITION) {
      MutableSpan<float3> positions = curves.positions_for_write();

      /* Jitter is applied perpendicular to the mouse movement vector. */
      const float2 forward = math::normalize(this->mouse_delta(extension_sample));
      const float2 sideways = float2(-forward.y, forward.x);

      selection.foreach_index(GrainSize(4096), [&](const int64_t point_i) {
        const float2 &co = view_positions[point_i];
        const float influence = brush_influence(
            scene, brush, co, extension_sample, params.multi_frame_falloff);
        if (influence <= 0.0f) {
          return;
        }
        const float noise = 2.0f * hash_rng(seed, 5678, point_i) - 1.0f;
        positions[point_i] = params.placement.project(co + sideways * influence * noise);
      });

      params.drawing.tag_positions_changed();
      changed = true;
    }
    if (sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_STRENGTH) {
      MutableSpan<float> opacities = params.drawing.opacities_for_write();
      selection.foreach_index(GrainSize(4096), [&](const int64_t point_i) {
        const float2 &co = view_positions[point_i];
        const float influence = brush_influence(
            scene, brush, co, extension_sample, params.multi_frame_falloff);
        if (influence <= 0.0f) {
          return;
        }
        const float noise = 2.0f * hash_rng(seed, 1212, point_i) - 1.0f;
        opacities[point_i] = math::clamp(opacities[point_i] + influence * noise, 0.0f, 1.0f);
      });
      changed = true;
    }
    if (sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_THICKNESS) {
      const MutableSpan<float> radii = params.drawing.radii_for_write();
      selection.foreach_index(GrainSize(4096), [&](const int64_t point_i) {
        const float2 &co = view_positions[point_i];
        const float influence = brush_influence(
            scene, brush, co, extension_sample, params.multi_frame_falloff);
        if (influence <= 0.0f) {
          return;
        }
        const float noise = 2.0f * hash_rng(seed, 1212, point_i) - 1.0f;
        radii[point_i] = math::max(radii[point_i] + influence * noise * 0.001f, 0.0f);
      });
      curves.tag_radii_changed();
      changed = true;
    }
    if (sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_UV) {
      bke::SpanAttributeWriter<float> rotations = attributes.lookup_or_add_for_write_span<float>(
          "rotation", bke::AttrDomain::Point);
      selection.foreach_index(GrainSize(4096), [&](const int64_t point_i) {
        const float2 &co = view_positions[point_i];
        const float influence = brush_influence(
            scene, brush, co, extension_sample, params.multi_frame_falloff);
        if (influence <= 0.0f) {
          return;
        }
        const float noise = 2.0f * hash_rng(seed, 1212, point_i) - 1.0f;
        rotations.span[point_i] = math::clamp(
            rotations.span[point_i] + influence * noise, -float(M_PI_2), float(M_PI_2));
      });
      rotations.finish();
      changed = true;
    }
    return changed;
  });
  this->stroke_extended(extension_sample);
}

std::unique_ptr<GreasePencilStrokeOperation> new_randomize_operation(
    const BrushStrokeMode stroke_mode)
{
  return std::make_unique<RandomizeOperation>(stroke_mode);
}

}  // namespace blender::ed::sculpt_paint::greasepencil
