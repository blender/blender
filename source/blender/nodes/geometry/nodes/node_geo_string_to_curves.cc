/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "DNA_curve_types.h"
#include "DNA_vfont_types.h"

#include "BKE_curve.h"
#include "BKE_spline.hh"
#include "BKE_vfont.h"

#include "BLI_hash.h"
#include "BLI_string_utf8.h"
#include "BLI_task.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_string_to_curves_cc {

NODE_STORAGE_FUNCS(NodeGeometryStringToCurves)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>(N_("String"));
  b.add_input<decl::Float>(N_("Size")).default_value(1.0f).min(0.0f).subtype(PROP_DISTANCE);
  b.add_input<decl::Float>(N_("Character Spacing"))
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE);
  b.add_input<decl::Float>(N_("Word Spacing"))
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE);
  b.add_input<decl::Float>(N_("Line Spacing"))
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE);
  b.add_input<decl::Float>(N_("Text Box Width"))
      .default_value(0.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE);
  b.add_input<decl::Float>(N_("Text Box Height"))
      .default_value(0.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .make_available([](bNode &node) {
        node_storage(node).overflow = GEO_NODE_STRING_TO_CURVES_MODE_SCALE_TO_FIT;
      });
  b.add_output<decl::Geometry>(N_("Curve Instances"));
  b.add_output<decl::String>(N_("Remainder")).make_available([](bNode &node) {
    node_storage(node).overflow = GEO_NODE_STRING_TO_CURVES_MODE_TRUNCATE;
  });
  b.add_output<decl::Int>(N_("Line")).field_source();
  b.add_output<decl::Vector>(N_("Pivot Point")).field_source();
}

static void node_layout(uiLayout *layout, struct bContext *C, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiTemplateID(layout,
               C,
               ptr,
               "font",
               nullptr,
               "FONT_OT_open",
               "FONT_OT_unlink",
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);
  uiItemR(layout, ptr, "overflow", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "align_x", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "align_y", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "pivot_mode", 0, IFACE_("Pivot Point"), ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryStringToCurves *data = MEM_cnew<NodeGeometryStringToCurves>(__func__);

  data->overflow = GEO_NODE_STRING_TO_CURVES_MODE_OVERFLOW;
  data->align_x = GEO_NODE_STRING_TO_CURVES_ALIGN_X_LEFT;
  data->align_y = GEO_NODE_STRING_TO_CURVES_ALIGN_Y_TOP_BASELINE;
  data->pivot_mode = GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_BOTTOM_LEFT;
  node->storage = data;
  node->id = (ID *)BKE_vfont_builtin_get();
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryStringToCurves &storage = node_storage(*node);
  const GeometryNodeStringToCurvesOverflowMode overflow = (GeometryNodeStringToCurvesOverflowMode)
                                                              storage.overflow;
  bNodeSocket *socket_remainder = ((bNodeSocket *)node->outputs.first)->next;
  nodeSetSocketAvailability(
      ntree, socket_remainder, overflow == GEO_NODE_STRING_TO_CURVES_MODE_TRUNCATE);

  bNodeSocket *height_socket = (bNodeSocket *)node->inputs.last;
  bNodeSocket *width_socket = height_socket->prev;
  nodeSetSocketAvailability(
      ntree, height_socket, overflow != GEO_NODE_STRING_TO_CURVES_MODE_OVERFLOW);
  node_sock_label(width_socket,
                  overflow == GEO_NODE_STRING_TO_CURVES_MODE_OVERFLOW ? N_("Max Width") :
                                                                        N_("Text Box Width"));
}

static float3 get_pivot_point(GeoNodeExecParams &params, CurveEval &curve)
{
  const NodeGeometryStringToCurves &storage = node_storage(params.node());
  const GeometryNodeStringToCurvesPivotMode pivot_mode = (GeometryNodeStringToCurvesPivotMode)
                                                             storage.pivot_mode;

  float3 min(FLT_MAX), max(FLT_MIN);

  /* Check if curve is empty. */
  if (!curve.bounds_min_max(min, max, false)) {
    return {0.0f, 0.0f, 0.0f};
  }

  switch (pivot_mode) {
    case GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_MIDPOINT:
      return (min + max) / 2;
    case GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_BOTTOM_LEFT:
      return float3(min.x, min.y, 0.0f);
    case GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_BOTTOM_CENTER:
      return float3((min.x + max.x) / 2, min.y, 0.0f);
    case GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_BOTTOM_RIGHT:
      return float3(max.x, min.y, 0.0f);
    case GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_TOP_LEFT:
      return float3(min.x, max.y, 0.0f);
    case GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_TOP_CENTER:
      return float3((min.x + max.x) / 2, max.y, 0.0f);
    case GEO_NODE_STRING_TO_CURVES_PIVOT_MODE_TOP_RIGHT:
      return float3(max.x, max.y, 0.0f);
  }
  return {0.0f, 0.0f, 0.0f};
}

struct TextLayout {
  /* Position of each character. */
  Vector<float2> positions;

  /* Line number of each character. */
  Array<int> line_numbers;

  /* Map of Pivot point for each character code. */
  Map<int, float3> pivot_points;

  /* UTF-32 Character codes. */
  Vector<char32_t> char_codes;

  /* The text that fit into the text box, with newline character sequences replaced. */
  std::string text;

  /* The text that didn't fit into the text box in 'Truncate' mode. May be empty. */
  std::string truncated_text;

  /* Font size could be modified if in 'Scale to fit'-mode. */
  float final_font_size;
};

static TextLayout get_text_layout(GeoNodeExecParams &params)
{
  TextLayout layout;
  layout.text = params.extract_input<std::string>("String");
  if (layout.text.empty()) {
    return {};
  }

  const NodeGeometryStringToCurves &storage = node_storage(params.node());
  const GeometryNodeStringToCurvesOverflowMode overflow = (GeometryNodeStringToCurvesOverflowMode)
                                                              storage.overflow;
  const GeometryNodeStringToCurvesAlignXMode align_x = (GeometryNodeStringToCurvesAlignXMode)
                                                           storage.align_x;
  const GeometryNodeStringToCurvesAlignYMode align_y = (GeometryNodeStringToCurvesAlignYMode)
                                                           storage.align_y;

  const float font_size = std::max(params.extract_input<float>("Size"), 0.0f);
  const float char_spacing = params.extract_input<float>("Character Spacing");
  const float word_spacing = params.extract_input<float>("Word Spacing");
  const float line_spacing = params.extract_input<float>("Line Spacing");
  const float textbox_w = params.extract_input<float>("Text Box Width");
  const float textbox_h = overflow == GEO_NODE_STRING_TO_CURVES_MODE_OVERFLOW ?
                              0.0f :
                              params.extract_input<float>("Text Box Height");
  VFont *vfont = (VFont *)params.node().id;

  Curve cu = {{nullptr}};
  cu.type = OB_FONT;
  /* Set defaults */
  cu.resolu = 12;
  cu.smallcaps_scale = 0.75f;
  cu.wordspace = 1.0f;
  /* Set values from inputs */
  cu.spacemode = align_x;
  cu.align_y = align_y;
  cu.fsize = font_size;
  cu.spacing = char_spacing;
  cu.wordspace = word_spacing;
  cu.linedist = line_spacing;
  cu.vfont = vfont;
  cu.overflow = overflow;
  cu.tb = (TextBox *)MEM_calloc_arrayN(MAXTEXTBOX, sizeof(TextBox), __func__);
  cu.tb->w = textbox_w;
  cu.tb->h = textbox_h;
  cu.totbox = 1;
  size_t len_bytes;
  size_t len_chars = BLI_strlen_utf8_ex(layout.text.c_str(), &len_bytes);
  cu.len_char32 = len_chars;
  cu.len = len_bytes;
  cu.pos = len_chars;
  /* The reason for the additional character here is unknown, but reflects other code elsewhere. */
  cu.str = (char *)MEM_mallocN(len_bytes + sizeof(char32_t), __func__);
  cu.strinfo = (CharInfo *)MEM_callocN((len_chars + 1) * sizeof(CharInfo), __func__);
  BLI_strncpy(cu.str, layout.text.c_str(), len_bytes + 1);

  struct CharTrans *chartransdata = nullptr;
  int text_len;
  bool text_free;
  const char32_t *r_text = nullptr;
  /* Mode FO_DUPLI used because it doesn't create curve splines. */
  BKE_vfont_to_curve_ex(
      nullptr, &cu, FO_DUPLI, nullptr, &r_text, &text_len, &text_free, &chartransdata);

  if (text_free) {
    MEM_freeN((void *)r_text);
  }

  Span<CharInfo> info{cu.strinfo, text_len};
  layout.final_font_size = cu.fsize_realtime;
  layout.positions.reserve(text_len);

  for (const int i : IndexRange(text_len)) {
    CharTrans &ct = chartransdata[i];
    layout.positions.append(float2(ct.xof, ct.yof) * layout.final_font_size);

    if ((info[i].flag & CU_CHINFO_OVERFLOW) && (cu.overflow == CU_OVERFLOW_TRUNCATE)) {
      const int offset = BLI_str_utf8_offset_from_index(layout.text.c_str(), i + 1);
      layout.truncated_text = layout.text.substr(offset);
      layout.text = layout.text.substr(0, offset);
      break;
    }
  }

  if (params.output_is_required("Line")) {
    layout.line_numbers.reinitialize(layout.positions.size());
    for (const int i : layout.positions.index_range()) {
      CharTrans &ct = chartransdata[i];
      layout.line_numbers[i] = ct.linenr;
    }
  }

  /* Convert UTF-8 encoded string to UTF-32. */
  len_chars = BLI_strlen_utf8_ex(layout.text.c_str(), &len_bytes);
  layout.char_codes.resize(len_chars + 1);
  BLI_str_utf8_as_utf32(layout.char_codes.data(), layout.text.c_str(), layout.char_codes.size());
  layout.char_codes.remove_last();

  MEM_SAFE_FREE(chartransdata);
  MEM_SAFE_FREE(cu.str);
  MEM_SAFE_FREE(cu.strinfo);
  MEM_SAFE_FREE(cu.tb);

  return layout;
}

/* Returns a mapping of UTF-32 character code to instance handle. */
static Map<int, int> create_curve_instances(GeoNodeExecParams &params,
                                            TextLayout &layout,
                                            InstancesComponent &instance_component)
{
  VFont *vfont = (VFont *)params.node().id;
  Map<int, int> handles;
  bool pivot_required = params.output_is_required("Pivot Point");

  for (int i : layout.char_codes.index_range()) {
    if (handles.contains(layout.char_codes[i])) {
      continue;
    }
    Curve cu = {{nullptr}};
    cu.type = OB_FONT;
    cu.resolu = 12;
    cu.vfont = vfont;
    CharInfo charinfo = {0};
    charinfo.mat_nr = 1;

    BKE_vfont_build_char(&cu, &cu.nurb, layout.char_codes[i], &charinfo, 0, 0, 0, i, 1);
    std::unique_ptr<CurveEval> curve_eval = curve_eval_from_dna_curve(cu);
    BKE_nurbList_free(&cu.nurb);

    float4x4 size_matrix = float4x4::identity();
    size_matrix.apply_scale(layout.final_font_size);
    curve_eval->transform(size_matrix);

    if (pivot_required) {
      float3 pivot_point = get_pivot_point(params, *curve_eval);
      layout.pivot_points.add_new(layout.char_codes[i], pivot_point);
    }

    GeometrySet geometry_set_curve = GeometrySet::create_with_curve(curve_eval.release());
    handles.add_new(layout.char_codes[i],
                    instance_component.add_reference(std::move(geometry_set_curve)));
  }
  return handles;
}

static void add_instances_from_handles(InstancesComponent &instances,
                                       const Map<int, int> &char_handles,
                                       const TextLayout &layout)
{
  instances.resize(layout.positions.size());
  MutableSpan<int> handles = instances.instance_reference_handles();
  MutableSpan<float4x4> transforms = instances.instance_transforms();

  threading::parallel_for(IndexRange(layout.positions.size()), 256, [&](IndexRange range) {
    for (const int i : range) {
      handles[i] = char_handles.lookup(layout.char_codes[i]);
      transforms[i] = float4x4::from_location({layout.positions[i].x, layout.positions[i].y, 0});
    }
  });
}

static void create_attributes(GeoNodeExecParams &params,
                              const TextLayout &layout,
                              InstancesComponent &instances)
{
  if (params.output_is_required("Line")) {
    StrongAnonymousAttributeID line_id = StrongAnonymousAttributeID("Line");
    OutputAttribute_Typed<int> line_attribute = instances.attribute_try_get_for_output_only<int>(
        line_id.get(), ATTR_DOMAIN_INSTANCE);
    MutableSpan<int> lines = line_attribute.as_span();
    lines.copy_from(layout.line_numbers);
    line_attribute.save();
    params.set_output("Line",
                      AnonymousAttributeFieldInput::Create<int>(std::move(line_id),
                                                                params.attribute_producer_name()));
  }

  if (params.output_is_required("Pivot Point")) {
    StrongAnonymousAttributeID pivot_id = StrongAnonymousAttributeID("Pivot");
    OutputAttribute_Typed<float3> pivot_attribute =
        instances.attribute_try_get_for_output_only<float3>(pivot_id.get(), ATTR_DOMAIN_INSTANCE);
    MutableSpan<float3> pivots = pivot_attribute.as_span();

    for (const int i : layout.char_codes.index_range()) {
      pivots[i] = layout.pivot_points.lookup(layout.char_codes[i]);
    }

    pivot_attribute.save();
    params.set_output("Pivot Point",
                      AnonymousAttributeFieldInput::Create<float3>(
                          std::move(pivot_id), params.attribute_producer_name()));
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  TextLayout layout = get_text_layout(params);

  const NodeGeometryStringToCurves &storage =
      *(const NodeGeometryStringToCurves *)params.node().storage;
  if (storage.overflow == GEO_NODE_STRING_TO_CURVES_MODE_TRUNCATE) {
    params.set_output("Remainder", std::move(layout.truncated_text));
  }

  if (layout.positions.size() == 0) {
    params.set_output("Curve Instances", GeometrySet());
    params.set_default_remaining_outputs();
    return;
  }

  /* Create and add instances. */
  GeometrySet geometry_set_out;
  InstancesComponent &instances = geometry_set_out.get_component_for_write<InstancesComponent>();
  Map<int, int> char_handles = create_curve_instances(params, layout, instances);
  add_instances_from_handles(instances, char_handles, layout);
  create_attributes(params, layout, instances);

  params.set_output("Curve Instances", std::move(geometry_set_out));
}

}  // namespace blender::nodes::node_geo_string_to_curves_cc

void register_node_type_geo_string_to_curves()
{
  namespace file_ns = blender::nodes::node_geo_string_to_curves_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_STRING_TO_CURVES, "String to Curves", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);
  node_type_size(&ntype, 190, 120, 700);
  node_type_storage(&ntype,
                    "NodeGeometryStringToCurves",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
