/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#pragma once

#include "BLI_index_mask.hh"
#include "BLI_vector.hh"

#include "BKE_modifier.hh"

struct ARegionType;
struct bContext;
struct GreasePencil;
struct GreasePencilModifierInfluenceData;
struct GreasePencilModifierLayerFilter;
struct GreasePencilModifierMaterialFilter;
struct PanelType;
struct PointerRNA;
struct uiLayout;
namespace blender::bke {
class CurvesGeometry;
namespace greasepencil {
class Drawing;
}
}  // namespace blender::bke

namespace blender::modifier::greasepencil {

void init_influence_data(GreasePencilModifierInfluenceData *influence_data, bool has_custom_curve);
void copy_influence_data(const GreasePencilModifierInfluenceData *influence_data_src,
                         GreasePencilModifierInfluenceData *influence_data_dst,
                         int flag);
void free_influence_data(GreasePencilModifierInfluenceData *influence_data);
void foreach_influence_ID_link(GreasePencilModifierInfluenceData *influence_data,
                               Object *ob,
                               IDWalkFunc walk,
                               void *user_data);
void write_influence_data(BlendWriter *writer,
                          const GreasePencilModifierInfluenceData *influence_data);
void read_influence_data(BlendDataReader *reader,
                         GreasePencilModifierInfluenceData *influence_data);

void draw_layer_filter_settings(const bContext *C, uiLayout *layout, PointerRNA *ptr);
void draw_material_filter_settings(const bContext *C, uiLayout *layout, PointerRNA *ptr);
void draw_vertex_group_settings(const bContext *C, uiLayout *layout, PointerRNA *ptr);
void draw_custom_curve_settings(const bContext *C, uiLayout *layout, PointerRNA *ptr);

IndexMask get_filtered_layer_mask(const GreasePencil &grease_pencil,
                                  const GreasePencilModifierInfluenceData &influence_data,
                                  IndexMaskMemory &memory);

IndexMask get_filtered_stroke_mask(const Object *ob,
                                   const bke::CurvesGeometry &curves,
                                   const GreasePencilModifierInfluenceData &influence_data,
                                   IndexMaskMemory &memory);

VArray<float> get_influence_vertex_weights(
    const bke::CurvesGeometry &curves, const GreasePencilModifierInfluenceData &influence_data);

Vector<bke::greasepencil::Drawing *> get_drawings_for_write(GreasePencil &grease_pencil,
                                                            const IndexMask &layer_mask,
                                                            int frame);

struct LayerDrawingInfo {
  bke::greasepencil::Drawing *drawing;
  /* Layer containing the drawing. */
  int layer_index;
};

Vector<LayerDrawingInfo> get_drawing_infos_by_layer(GreasePencil &grease_pencil,
                                                    const IndexMask &layer_mask,
                                                    int frame);

struct FrameDrawingInfo {
  bke::greasepencil::Drawing *drawing;
  /* Frame on which this drawing starts. */
  int start_frame_number;
};

Vector<FrameDrawingInfo> get_drawing_infos_by_frame(GreasePencil &grease_pencil,
                                                    const IndexMask &layer_mask,
                                                    int frame);

}  // namespace blender::modifier::greasepencil
