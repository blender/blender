/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/shader.h"
#include "scene/background.h"
#include "scene/integrator.h"
#include "scene/light.h"
#include "scene/osl.h"
#include "scene/scene.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"

#include "blender/image.h"
#include "blender/sync.h"
#include "blender/util.h"

#include "util/set.h"
#include "util/string.h"
#include "util/task.h"

#include "BKE_duplilist.hh"
#include "BKE_node.hh"

#include "NOD_shader_nodes_inline.hh"

CCL_NAMESPACE_BEGIN

using PtrInputMap = unordered_multimap<void *, ShaderInput *>;
using PtrOutputMap = map<void *, ShaderOutput *>;
using ProxyMap = map<string, ConvertNode *>;

/* Find */

void BlenderSync::find_shader(const BL::ID &id,
                              array<Node *> &used_shaders,
                              Shader *default_shader)
{
  Shader *synced_shader = (id) ? shader_map.find(id) : nullptr;
  Shader *shader = (synced_shader) ? synced_shader : default_shader;

  used_shaders.push_back_slow(shader);
  shader->tag_used(scene);
}

/* RNA translation utilities */

static VolumeSampling get_volume_sampling(PointerRNA &ptr)
{
  return (VolumeSampling)get_enum(
      ptr, "volume_sampling", VOLUME_NUM_SAMPLING, VOLUME_SAMPLING_DISTANCE);
}

static VolumeInterpolation get_volume_interpolation(PointerRNA &ptr)
{
  return (VolumeInterpolation)get_enum(
      ptr, "volume_interpolation", VOLUME_NUM_INTERPOLATION, VOLUME_INTERPOLATION_LINEAR);
}

static EmissionSampling get_emission_sampling(PointerRNA &ptr)
{
  return (EmissionSampling)get_enum(
      ptr, "emission_sampling", EMISSION_SAMPLING_NUM, EMISSION_SAMPLING_AUTO);
}

static int validate_enum_value(const int value, const int num_values, const int default_value)
{
  if (value >= num_values) {
    return default_value;
  }
  return value;
}

static DisplacementMethod get_displacement_method(BL::Material &b_mat)
{
  const int value = b_mat.displacement_method();
  return (DisplacementMethod)validate_enum_value(value, DISPLACE_NUM_METHODS, DISPLACE_BUMP);
}

template<typename NodeType> static InterpolationType get_image_interpolation(NodeType &b_node)
{
  const int value = b_node.interpolation();
  return (InterpolationType)validate_enum_value(
      value, INTERPOLATION_NUM_TYPES, INTERPOLATION_LINEAR);
}

template<typename NodeType> static ExtensionType get_image_extension(NodeType &b_node)
{
  const int value = b_node.extension();
  return (ExtensionType)validate_enum_value(value, EXTENSION_NUM_TYPES, EXTENSION_REPEAT);
}

static ImageAlphaType get_image_alpha_type(BL::Image &b_image)
{
  const int value = b_image.alpha_mode();
  return (ImageAlphaType)validate_enum_value(value, IMAGE_ALPHA_NUM_TYPES, IMAGE_ALPHA_AUTO);
}

/* Attribute name translation utilities */

/* Since Eevee needs to know whether the attribute is uniform or varying
 * at the time it compiles the shader for the material, Blender had to
 * introduce different namespaces (types) in its attribute node. However,
 * Cycles already has object attributes that form a uniform namespace with
 * the more common varying attributes. Without completely reworking the
 * attribute handling in Cycles to introduce separate namespaces (this could
 * be especially hard for OSL which directly uses the name string), the
 * space identifier has to be added to the attribute name as a prefix.
 *
 * The prefixes include a control character to ensure the user specified
 * name can't accidentally include a special prefix.
 */

static const string_view object_attr_prefix("\x01object:");
static const string_view instancer_attr_prefix("\x01instancer:");
static const string_view view_layer_attr_prefix("\x01layer:");

static ustring blender_attribute_name_add_type(const string &name, BlenderAttributeType type)
{
  switch (type) {
    case BL::ShaderNodeAttribute::attribute_type_OBJECT:
      return ustring::concat(object_attr_prefix, name);
    case BL::ShaderNodeAttribute::attribute_type_INSTANCER:
      return ustring::concat(instancer_attr_prefix, name);
    case BL::ShaderNodeAttribute::attribute_type_VIEW_LAYER:
      return ustring::concat(view_layer_attr_prefix, name);
    default:
      return ustring(name);
  }
}

BlenderAttributeType blender_attribute_name_split_type(ustring name, string *r_real_name)
{
  const string_view sname(name);

  if (sname.substr(0, object_attr_prefix.size()) == object_attr_prefix) {
    *r_real_name = sname.substr(object_attr_prefix.size());
    return BL::ShaderNodeAttribute::attribute_type_OBJECT;
  }

  if (sname.substr(0, instancer_attr_prefix.size()) == instancer_attr_prefix) {
    *r_real_name = sname.substr(instancer_attr_prefix.size());
    return BL::ShaderNodeAttribute::attribute_type_INSTANCER;
  }

  if (sname.substr(0, view_layer_attr_prefix.size()) == view_layer_attr_prefix) {
    *r_real_name = sname.substr(view_layer_attr_prefix.size());
    return BL::ShaderNodeAttribute::attribute_type_VIEW_LAYER;
  }

  return BL::ShaderNodeAttribute::attribute_type_GEOMETRY;
}

/* Graph */

static BL::NodeSocket get_node_output(BL::Node &b_node, const string &name)
{
  for (BL::NodeSocket &b_out : b_node.outputs) {
    if (b_out.identifier() == name) {
      return b_out;
    }
  }
  assert(0);
  return *b_node.outputs.begin();
}

static float3 get_node_output_rgba(BL::Node &b_node, const string &name)
{
  BL::NodeSocket b_sock = get_node_output(b_node, name);
  float value[4];
  RNA_float_get_array(&b_sock.ptr, "default_value", value);
  return make_float3(value[0], value[1], value[2]);
}

static float get_node_output_value(BL::Node &b_node, const string &name)
{
  BL::NodeSocket b_sock = get_node_output(b_node, name);
  return RNA_float_get(&b_sock.ptr, "default_value");
}

static float3 get_node_output_vector(BL::Node &b_node, const string &name)
{
  BL::NodeSocket b_sock = get_node_output(b_node, name);
  float value[3];
  RNA_float_get_array(&b_sock.ptr, "default_value", value);
  return make_float3(value[0], value[1], value[2]);
}

static SocketType::Type convert_socket_type(BL::NodeSocket &b_socket)
{
  switch (b_socket.type()) {
    case BL::NodeSocket::type_VALUE:
      return SocketType::FLOAT;
    case BL::NodeSocket::type_BOOLEAN:
    case BL::NodeSocket::type_INT:
      return SocketType::INT;
    case BL::NodeSocket::type_VECTOR:
      return SocketType::VECTOR;
    case BL::NodeSocket::type_RGBA:
      return SocketType::COLOR;
    case BL::NodeSocket::type_STRING:
      return SocketType::STRING;
    case BL::NodeSocket::type_SHADER:
      return SocketType::CLOSURE;

    default:
      return SocketType::UNDEFINED;
  }
}

static void set_default_value(ShaderInput *input,
                              BL::NodeSocket &b_sock,
                              BL::BlendData &b_data,
                              BL::ID &b_id)
{
  Node *node = input->parent;
  const SocketType &socket = input->socket_type;

  /* copy values for non linked inputs */
  switch (input->type()) {
    case SocketType::FLOAT: {
      node->set(socket, get_float(b_sock.ptr, "default_value"));
      break;
    }
    case SocketType::INT: {
      if (b_sock.type() == BL::NodeSocket::type_BOOLEAN) {
        /* Make sure to call the int overload of set() since this is an integer socket as far as
         * Cycles is concerned. */
        node->set(socket, get_boolean(b_sock.ptr, "default_value") ? 1 : 0);
      }
      else {
        node->set(socket, get_int(b_sock.ptr, "default_value"));
      }
      break;
    }
    case SocketType::COLOR: {
      node->set(socket, make_float3(get_float4(b_sock.ptr, "default_value")));
      break;
    }
    case SocketType::NORMAL:
    case SocketType::POINT:
    case SocketType::VECTOR: {
      node->set(socket, get_float3(b_sock.ptr, "default_value"));
      break;
    }
    case SocketType::STRING: {
      node->set(
          socket,
          (ustring)blender_absolute_path(b_data, b_id, get_string(b_sock.ptr, "default_value")));
      break;
    }
    default:
      break;
  }
}

static void get_tex_mapping(TextureNode *mapping, BL::TexMapping &b_mapping)
{
  if (!b_mapping) {
    return;
  }

  mapping->set_tex_mapping_translation(get_float3(b_mapping.translation()));
  mapping->set_tex_mapping_rotation(get_float3(b_mapping.rotation()));
  mapping->set_tex_mapping_scale(get_float3(b_mapping.scale()));
  mapping->set_tex_mapping_type((TextureMapping::Type)b_mapping.vector_type());

  mapping->set_tex_mapping_x_mapping((TextureMapping::Mapping)b_mapping.mapping_x());
  mapping->set_tex_mapping_y_mapping((TextureMapping::Mapping)b_mapping.mapping_y());
  mapping->set_tex_mapping_z_mapping((TextureMapping::Mapping)b_mapping.mapping_z());
}

static bool is_image_animated(BL::Image::source_enum b_image_source, BL::ImageUser &b_image_user)
{
  return (b_image_source == BL::Image::source_MOVIE ||
          b_image_source == BL::Image::source_SEQUENCE) &&
         b_image_user.use_auto_refresh();
}

static ShaderNode *add_node(Scene *scene,
                            BL::RenderEngine &b_engine,
                            BL::BlendData &b_data,
                            BL::Scene &b_scene,
                            ShaderGraph *graph,
                            BL::ShaderNodeTree &b_ntree,
                            BL::ShaderNode &b_node)
{
  ShaderNode *node = nullptr;

  /* existing blender nodes */
  if (b_node.is_a(&RNA_ShaderNodeRGBCurve)) {
    BL::ShaderNodeRGBCurve b_curve_node(b_node);
    BL::CurveMapping mapping(b_curve_node.mapping());
    RGBCurvesNode *curves = graph->create_node<RGBCurvesNode>();
    array<float3> curve_mapping_curves;
    float min_x;
    float max_x;
    curvemapping_color_to_array(mapping, curve_mapping_curves, RAMP_TABLE_SIZE, true);
    curvemapping_minmax(mapping, 4, &min_x, &max_x);
    curves->set_min_x(min_x);
    curves->set_max_x(max_x);
    curves->set_curves(curve_mapping_curves);
    curves->set_extrapolate(mapping.extend() == BL::CurveMapping::extend_EXTRAPOLATED);
    node = curves;
  }
  if (b_node.is_a(&RNA_ShaderNodeVectorCurve)) {
    BL::ShaderNodeVectorCurve b_curve_node(b_node);
    BL::CurveMapping mapping(b_curve_node.mapping());
    VectorCurvesNode *curves = graph->create_node<VectorCurvesNode>();
    array<float3> curve_mapping_curves;
    float min_x;
    float max_x;
    curvemapping_color_to_array(mapping, curve_mapping_curves, RAMP_TABLE_SIZE, false);
    curvemapping_minmax(mapping, 3, &min_x, &max_x);
    curves->set_min_x(min_x);
    curves->set_max_x(max_x);
    curves->set_curves(curve_mapping_curves);
    curves->set_extrapolate(mapping.extend() == BL::CurveMapping::extend_EXTRAPOLATED);
    node = curves;
  }
  else if (b_node.is_a(&RNA_ShaderNodeFloatCurve)) {
    BL::ShaderNodeFloatCurve b_curve_node(b_node);
    BL::CurveMapping mapping(b_curve_node.mapping());
    FloatCurveNode *curve = graph->create_node<FloatCurveNode>();
    array<float> curve_mapping_curve;
    float min_x;
    float max_x;
    curvemapping_float_to_array(mapping, curve_mapping_curve, RAMP_TABLE_SIZE);
    curvemapping_minmax(mapping, 1, &min_x, &max_x);
    curve->set_min_x(min_x);
    curve->set_max_x(max_x);
    curve->set_curve(curve_mapping_curve);
    curve->set_extrapolate(mapping.extend() == BL::CurveMapping::extend_EXTRAPOLATED);
    node = curve;
  }
  else if (b_node.is_a(&RNA_ShaderNodeValToRGB)) {
    RGBRampNode *ramp = graph->create_node<RGBRampNode>();
    BL::ShaderNodeValToRGB b_ramp_node(b_node);
    BL::ColorRamp b_color_ramp(b_ramp_node.color_ramp());
    array<float3> ramp_values;
    array<float> ramp_alpha;
    colorramp_to_array(b_color_ramp, ramp_values, ramp_alpha, RAMP_TABLE_SIZE);
    ramp->set_ramp(ramp_values);
    ramp->set_ramp_alpha(ramp_alpha);
    ramp->set_interpolate(b_color_ramp.interpolation() != BL::ColorRamp::interpolation_CONSTANT);
    node = ramp;
  }
  else if (b_node.is_a(&RNA_ShaderNodeRGB)) {
    ColorNode *color = graph->create_node<ColorNode>();
    color->set_value(get_node_output_rgba(b_node, "Color"));
    node = color;
  }
  else if (b_node.is_a(&RNA_ShaderNodeValue)) {
    ValueNode *value = graph->create_node<ValueNode>();
    value->set_value(get_node_output_value(b_node, "Value"));
    node = value;
  }
  else if (b_node.is_a(&RNA_ShaderNodeCameraData)) {
    node = graph->create_node<CameraNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeInvert)) {
    node = graph->create_node<InvertNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeGamma)) {
    node = graph->create_node<GammaNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeBrightContrast)) {
    node = graph->create_node<BrightContrastNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeMixRGB)) {
    BL::ShaderNodeMixRGB b_mix_node(b_node);
    MixNode *mix = graph->create_node<MixNode>();
    mix->set_mix_type((NodeMix)b_mix_node.blend_type());
    mix->set_use_clamp(b_mix_node.use_clamp());
    node = mix;
  }
  else if (b_node.is_a(&RNA_ShaderNodeMix)) {
    BL::ShaderNodeMix b_mix_node(b_node);
    if (b_mix_node.data_type() == BL::ShaderNodeMix::data_type_VECTOR) {
      if (b_mix_node.factor_mode() == BL::ShaderNodeMix::factor_mode_UNIFORM) {
        MixVectorNode *mix_node = graph->create_node<MixVectorNode>();
        mix_node->set_use_clamp(b_mix_node.clamp_factor());
        node = mix_node;
      }
      else {
        MixVectorNonUniformNode *mix_node = graph->create_node<MixVectorNonUniformNode>();
        mix_node->set_use_clamp(b_mix_node.clamp_factor());
        node = mix_node;
      }
    }
    else if (b_mix_node.data_type() == BL::ShaderNodeMix::data_type_RGBA) {
      MixColorNode *mix_node = graph->create_node<MixColorNode>();
      mix_node->set_blend_type((NodeMix)b_mix_node.blend_type());
      mix_node->set_use_clamp(b_mix_node.clamp_factor());
      mix_node->set_use_clamp_result(b_mix_node.clamp_result());
      node = mix_node;
    }
    else {
      MixFloatNode *mix_node = graph->create_node<MixFloatNode>();
      mix_node->set_use_clamp(b_mix_node.clamp_factor());
      node = mix_node;
    }
  }
  else if (b_node.is_a(&RNA_ShaderNodeSeparateColor)) {
    BL::ShaderNodeSeparateColor b_separate_node(b_node);
    SeparateColorNode *separate_node = graph->create_node<SeparateColorNode>();
    separate_node->set_color_type((NodeCombSepColorType)b_separate_node.mode());
    node = separate_node;
  }
  else if (b_node.is_a(&RNA_ShaderNodeCombineColor)) {
    BL::ShaderNodeCombineColor b_combine_node(b_node);
    CombineColorNode *combine_node = graph->create_node<CombineColorNode>();
    combine_node->set_color_type((NodeCombSepColorType)b_combine_node.mode());
    node = combine_node;
  }
  else if (b_node.is_a(&RNA_ShaderNodeSeparateXYZ)) {
    node = graph->create_node<SeparateXYZNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeCombineXYZ)) {
    node = graph->create_node<CombineXYZNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeHueSaturation)) {
    node = graph->create_node<HSVNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeRGBToBW)) {
    node = graph->create_node<RGBToBWNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeMapRange)) {
    BL::ShaderNodeMapRange b_map_range_node(b_node);
    if (b_map_range_node.data_type() == BL::ShaderNodeMapRange::data_type_FLOAT_VECTOR) {
      VectorMapRangeNode *vector_map_range_node = graph->create_node<VectorMapRangeNode>();
      vector_map_range_node->set_use_clamp(b_map_range_node.clamp());
      vector_map_range_node->set_range_type(
          (NodeMapRangeType)b_map_range_node.interpolation_type());
      node = vector_map_range_node;
    }
    else {
      MapRangeNode *map_range_node = graph->create_node<MapRangeNode>();
      map_range_node->set_clamp(b_map_range_node.clamp());
      map_range_node->set_range_type((NodeMapRangeType)b_map_range_node.interpolation_type());
      node = map_range_node;
    }
  }
  else if (b_node.is_a(&RNA_ShaderNodeClamp)) {
    BL::ShaderNodeClamp b_clamp_node(b_node);
    ClampNode *clamp_node = graph->create_node<ClampNode>();
    clamp_node->set_clamp_type((NodeClampType)b_clamp_node.clamp_type());
    node = clamp_node;
  }
  else if (b_node.is_a(&RNA_ShaderNodeMath)) {
    BL::ShaderNodeMath b_math_node(b_node);
    MathNode *math_node = graph->create_node<MathNode>();
    math_node->set_math_type((NodeMathType)b_math_node.operation());
    math_node->set_use_clamp(b_math_node.use_clamp());
    node = math_node;
  }
  else if (b_node.is_a(&RNA_ShaderNodeVectorMath)) {
    BL::ShaderNodeVectorMath b_vector_math_node(b_node);
    VectorMathNode *vector_math_node = graph->create_node<VectorMathNode>();
    vector_math_node->set_math_type((NodeVectorMathType)b_vector_math_node.operation());
    node = vector_math_node;
  }
  else if (b_node.is_a(&RNA_ShaderNodeVectorRotate)) {
    BL::ShaderNodeVectorRotate b_vector_rotate_node(b_node);
    VectorRotateNode *vector_rotate_node = graph->create_node<VectorRotateNode>();
    vector_rotate_node->set_rotate_type(
        (NodeVectorRotateType)b_vector_rotate_node.rotation_type());
    vector_rotate_node->set_invert(b_vector_rotate_node.invert());
    node = vector_rotate_node;
  }
  else if (b_node.is_a(&RNA_ShaderNodeVectorTransform)) {
    BL::ShaderNodeVectorTransform b_vector_transform_node(b_node);
    VectorTransformNode *vtransform = graph->create_node<VectorTransformNode>();
    vtransform->set_transform_type((NodeVectorTransformType)b_vector_transform_node.vector_type());
    vtransform->set_convert_from(
        (NodeVectorTransformConvertSpace)b_vector_transform_node.convert_from());
    vtransform->set_convert_to(
        (NodeVectorTransformConvertSpace)b_vector_transform_node.convert_to());
    node = vtransform;
  }
  else if (b_node.is_a(&RNA_ShaderNodeNormal)) {
    BL::Node::outputs_iterator out_it;
    b_node.outputs.begin(out_it);

    NormalNode *norm = graph->create_node<NormalNode>();
    norm->set_direction(get_node_output_vector(b_node, "Normal"));
    node = norm;
  }
  else if (b_node.is_a(&RNA_ShaderNodeMapping)) {
    BL::ShaderNodeMapping b_mapping_node(b_node);
    MappingNode *mapping = graph->create_node<MappingNode>();
    mapping->set_mapping_type((NodeMappingType)b_mapping_node.vector_type());
    node = mapping;
  }
  else if (b_node.is_a(&RNA_ShaderNodeFresnel)) {
    node = graph->create_node<FresnelNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeLayerWeight)) {
    node = graph->create_node<LayerWeightNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeAddShader)) {
    node = graph->create_node<AddClosureNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeMixShader)) {
    node = graph->create_node<MixClosureNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeAttribute)) {
    BL::ShaderNodeAttribute b_attr_node(b_node);
    AttributeNode *attr = graph->create_node<AttributeNode>();
    attr->set_attribute(blender_attribute_name_add_type(b_attr_node.attribute_name(),
                                                        b_attr_node.attribute_type()));
    node = attr;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBackground)) {
    node = graph->create_node<BackgroundNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeHoldout)) {
    node = graph->create_node<HoldoutNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfDiffuse)) {
    node = graph->create_node<DiffuseBsdfNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeSubsurfaceScattering)) {
    BL::ShaderNodeSubsurfaceScattering b_subsurface_node(b_node);

    SubsurfaceScatteringNode *subsurface = graph->create_node<SubsurfaceScatteringNode>();

    switch (b_subsurface_node.falloff()) {
      case BL::ShaderNodeSubsurfaceScattering::falloff_BURLEY:
        subsurface->set_method(CLOSURE_BSSRDF_BURLEY_ID);
        break;
      case BL::ShaderNodeSubsurfaceScattering::falloff_RANDOM_WALK:
        subsurface->set_method(CLOSURE_BSSRDF_RANDOM_WALK_ID);
        break;
      case BL::ShaderNodeSubsurfaceScattering::falloff_RANDOM_WALK_SKIN:
        subsurface->set_method(CLOSURE_BSSRDF_RANDOM_WALK_SKIN_ID);
        break;
    }

    node = subsurface;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfMetallic)) {
    BL::ShaderNodeBsdfMetallic b_metallic_node(b_node);
    MetallicBsdfNode *metal = graph->create_node<MetallicBsdfNode>();

    switch (b_metallic_node.distribution()) {
      case BL::ShaderNodeBsdfMetallic::distribution_BECKMANN:
        metal->set_distribution(CLOSURE_BSDF_MICROFACET_BECKMANN_ID);
        break;
      case BL::ShaderNodeBsdfMetallic::distribution_GGX:
        metal->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_ID);
        break;
      case BL::ShaderNodeBsdfMetallic::distribution_MULTI_GGX:
        metal->set_distribution(CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID);
        break;
    }

    switch (b_metallic_node.fresnel_type()) {
      case BL::ShaderNodeBsdfMetallic::fresnel_type_PHYSICAL_CONDUCTOR:
        metal->set_fresnel_type(CLOSURE_BSDF_PHYSICAL_CONDUCTOR);
        break;
      case BL::ShaderNodeBsdfMetallic::fresnel_type_F82:
        metal->set_fresnel_type(CLOSURE_BSDF_F82_CONDUCTOR);
        break;
    }
    node = metal;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfAnisotropic)) {
    BL::ShaderNodeBsdfAnisotropic b_glossy_node(b_node);
    GlossyBsdfNode *glossy = graph->create_node<GlossyBsdfNode>();

    switch (b_glossy_node.distribution()) {
      case BL::ShaderNodeBsdfAnisotropic::distribution_BECKMANN:
        glossy->set_distribution(CLOSURE_BSDF_MICROFACET_BECKMANN_ID);
        break;
      case BL::ShaderNodeBsdfAnisotropic::distribution_GGX:
        glossy->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_ID);
        break;
      case BL::ShaderNodeBsdfAnisotropic::distribution_ASHIKHMIN_SHIRLEY:
        glossy->set_distribution(CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID);
        break;
      case BL::ShaderNodeBsdfAnisotropic::distribution_MULTI_GGX:
        glossy->set_distribution(CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID);
        break;
    }
    node = glossy;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfGlass)) {
    BL::ShaderNodeBsdfGlass b_glass_node(b_node);
    GlassBsdfNode *glass = graph->create_node<GlassBsdfNode>();
    switch (b_glass_node.distribution()) {
      case BL::ShaderNodeBsdfGlass::distribution_BECKMANN:
        glass->set_distribution(CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID);
        break;
      case BL::ShaderNodeBsdfGlass::distribution_GGX:
        glass->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID);
        break;
      case BL::ShaderNodeBsdfGlass::distribution_MULTI_GGX:
        glass->set_distribution(CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID);
        break;
    }
    node = glass;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfRefraction)) {
    BL::ShaderNodeBsdfRefraction b_refraction_node(b_node);
    RefractionBsdfNode *refraction = graph->create_node<RefractionBsdfNode>();
    switch (b_refraction_node.distribution()) {
      case BL::ShaderNodeBsdfRefraction::distribution_BECKMANN:
        refraction->set_distribution(CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID);
        break;
      case BL::ShaderNodeBsdfRefraction::distribution_GGX:
        refraction->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
        break;
    }
    node = refraction;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfToon)) {
    BL::ShaderNodeBsdfToon b_toon_node(b_node);
    ToonBsdfNode *toon = graph->create_node<ToonBsdfNode>();
    switch (b_toon_node.component()) {
      case BL::ShaderNodeBsdfToon::component_DIFFUSE:
        toon->set_component(CLOSURE_BSDF_DIFFUSE_TOON_ID);
        break;
      case BL::ShaderNodeBsdfToon::component_GLOSSY:
        toon->set_component(CLOSURE_BSDF_GLOSSY_TOON_ID);
        break;
    }
    node = toon;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfHair)) {
    BL::ShaderNodeBsdfHair b_hair_node(b_node);
    HairBsdfNode *hair = graph->create_node<HairBsdfNode>();
    switch (b_hair_node.component()) {
      case BL::ShaderNodeBsdfHair::component_Reflection:
        hair->set_component(CLOSURE_BSDF_HAIR_REFLECTION_ID);
        break;
      case BL::ShaderNodeBsdfHair::component_Transmission:
        hair->set_component(CLOSURE_BSDF_HAIR_TRANSMISSION_ID);
        break;
    }
    node = hair;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfHairPrincipled)) {
    BL::ShaderNodeBsdfHairPrincipled b_principled_hair_node(b_node);
    PrincipledHairBsdfNode *principled_hair = graph->create_node<PrincipledHairBsdfNode>();
    principled_hair->set_model((NodePrincipledHairModel)get_enum(b_principled_hair_node.ptr,
                                                                 "model",
                                                                 NODE_PRINCIPLED_HAIR_MODEL_NUM,
                                                                 NODE_PRINCIPLED_HAIR_HUANG));
    principled_hair->set_parametrization(
        (NodePrincipledHairParametrization)get_enum(b_principled_hair_node.ptr,
                                                    "parametrization",
                                                    NODE_PRINCIPLED_HAIR_PARAMETRIZATION_NUM,
                                                    NODE_PRINCIPLED_HAIR_REFLECTANCE));
    node = principled_hair;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfPrincipled)) {
    BL::ShaderNodeBsdfPrincipled b_principled_node(b_node);
    PrincipledBsdfNode *principled = graph->create_node<PrincipledBsdfNode>();
    switch (b_principled_node.distribution()) {
      case BL::ShaderNodeBsdfPrincipled::distribution_GGX:
        principled->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID);
        break;
      case BL::ShaderNodeBsdfPrincipled::distribution_MULTI_GGX:
        principled->set_distribution(CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID);
        break;
    }
    switch (b_principled_node.subsurface_method()) {
      case BL::ShaderNodeBsdfPrincipled::subsurface_method_BURLEY:
        principled->set_subsurface_method(CLOSURE_BSSRDF_BURLEY_ID);
        break;
      case BL::ShaderNodeBsdfPrincipled::subsurface_method_RANDOM_WALK:
        principled->set_subsurface_method(CLOSURE_BSSRDF_RANDOM_WALK_ID);
        break;
      case BL::ShaderNodeBsdfPrincipled::subsurface_method_RANDOM_WALK_SKIN:
        principled->set_subsurface_method(CLOSURE_BSSRDF_RANDOM_WALK_SKIN_ID);
        break;
    }
    node = principled;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfTranslucent)) {
    node = graph->create_node<TranslucentBsdfNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfTransparent)) {
    node = graph->create_node<TransparentBsdfNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfRayPortal)) {
    node = graph->create_node<RayPortalBsdfNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfSheen)) {
    BL::ShaderNodeBsdfSheen b_sheen_node(b_node);
    SheenBsdfNode *sheen = graph->create_node<SheenBsdfNode>();
    switch (b_sheen_node.distribution()) {
      case BL::ShaderNodeBsdfSheen::distribution_ASHIKHMIN:
        sheen->set_distribution(CLOSURE_BSDF_ASHIKHMIN_VELVET_ID);
        break;
      case BL::ShaderNodeBsdfSheen::distribution_MICROFIBER:
        sheen->set_distribution(CLOSURE_BSDF_SHEEN_ID);
        break;
    }
    node = sheen;
  }
  else if (b_node.is_a(&RNA_ShaderNodeEmission)) {
    node = graph->create_node<EmissionNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeAmbientOcclusion)) {
    BL::ShaderNodeAmbientOcclusion b_ao_node(b_node);
    AmbientOcclusionNode *ao = graph->create_node<AmbientOcclusionNode>();
    ao->set_samples(b_ao_node.samples());
    ao->set_inside(b_ao_node.inside());
    ao->set_only_local(b_ao_node.only_local());
    node = ao;
  }
  else if (b_node.is_a(&RNA_ShaderNodeVolumeScatter)) {
    BL::ShaderNodeVolumeScatter b_scatter_node(b_node);
    ScatterVolumeNode *scatter = graph->create_node<ScatterVolumeNode>();
    switch (b_scatter_node.phase()) {
      case BL::ShaderNodeVolumeScatter::phase_HENYEY_GREENSTEIN:
        scatter->set_phase(CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID);
        break;
      case BL::ShaderNodeVolumeScatter::phase_FOURNIER_FORAND:
        scatter->set_phase(CLOSURE_VOLUME_FOURNIER_FORAND_ID);
        break;
      case BL::ShaderNodeVolumeScatter::phase_DRAINE:
        scatter->set_phase(CLOSURE_VOLUME_DRAINE_ID);
        break;
      case BL::ShaderNodeVolumeScatter::phase_RAYLEIGH:
        scatter->set_phase(CLOSURE_VOLUME_RAYLEIGH_ID);
        break;
      case BL::ShaderNodeVolumeScatter::phase_MIE:
        scatter->set_phase(CLOSURE_VOLUME_MIE_ID);
        break;
    }
    node = scatter;
  }
  else if (b_node.is_a(&RNA_ShaderNodeVolumeAbsorption)) {
    node = graph->create_node<AbsorptionVolumeNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeVolumeCoefficients)) {
    BL::ShaderNodeVolumeCoefficients b_coeffs_node(b_node);
    VolumeCoefficientsNode *coeffs = graph->create_node<VolumeCoefficientsNode>();
    switch (b_coeffs_node.phase()) {
      case BL::ShaderNodeVolumeCoefficients::phase_HENYEY_GREENSTEIN:
        coeffs->set_phase(CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID);
        break;
      case BL::ShaderNodeVolumeCoefficients::phase_FOURNIER_FORAND:
        coeffs->set_phase(CLOSURE_VOLUME_FOURNIER_FORAND_ID);
        break;
      case BL::ShaderNodeVolumeCoefficients::phase_DRAINE:
        coeffs->set_phase(CLOSURE_VOLUME_DRAINE_ID);
        break;
      case BL::ShaderNodeVolumeCoefficients::phase_RAYLEIGH:
        coeffs->set_phase(CLOSURE_VOLUME_RAYLEIGH_ID);
        break;
      case BL::ShaderNodeVolumeCoefficients::phase_MIE:
        coeffs->set_phase(CLOSURE_VOLUME_MIE_ID);
        break;
    }
    node = coeffs;
  }
  else if (b_node.is_a(&RNA_ShaderNodeVolumePrincipled)) {
    PrincipledVolumeNode *principled = graph->create_node<PrincipledVolumeNode>();
    node = principled;
  }
  else if (b_node.is_a(&RNA_ShaderNodeNewGeometry)) {
    node = graph->create_node<GeometryNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeWireframe)) {
    BL::ShaderNodeWireframe b_wireframe_node(b_node);
    WireframeNode *wire = graph->create_node<WireframeNode>();
    wire->set_use_pixel_size(b_wireframe_node.use_pixel_size());
    node = wire;
  }
  else if (b_node.is_a(&RNA_ShaderNodeWavelength)) {
    node = graph->create_node<WavelengthNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeBlackbody)) {
    node = graph->create_node<BlackbodyNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeLightPath)) {
    node = graph->create_node<LightPathNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeLightFalloff)) {
    node = graph->create_node<LightFalloffNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeObjectInfo)) {
    node = graph->create_node<ObjectInfoNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeParticleInfo)) {
    node = graph->create_node<ParticleInfoNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeHairInfo)) {
    node = graph->create_node<HairInfoNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodePointInfo)) {
    node = graph->create_node<PointInfoNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeVolumeInfo)) {
    node = graph->create_node<VolumeInfoNode>();
  }
  else if (b_node.is_a(&RNA_ShaderNodeVertexColor)) {
    BL::ShaderNodeVertexColor b_vertex_color_node(b_node);
    VertexColorNode *vertex_color_node = graph->create_node<VertexColorNode>();
    vertex_color_node->set_layer_name(ustring(b_vertex_color_node.layer_name()));
    node = vertex_color_node;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBump)) {
    BL::ShaderNodeBump b_bump_node(b_node);
    BumpNode *bump = graph->create_node<BumpNode>();
    bump->set_invert(b_bump_node.invert());
    node = bump;
  }
  else if (b_node.is_a(&RNA_ShaderNodeScript)) {
#ifdef WITH_OSL
    if (scene->shader_manager->use_osl()) {
      /* create script node */
      BL::ShaderNodeScript b_script_node(b_node);

      const string bytecode_hash = b_script_node.bytecode_hash();
      if (!bytecode_hash.empty()) {
        node = OSLShaderManager::osl_node(
            graph, scene, "", bytecode_hash, b_script_node.bytecode());
      }
      else {
        const string absolute_filepath = blender_absolute_path(
            b_data, b_ntree, b_script_node.filepath());
        node = OSLShaderManager::osl_node(graph, scene, absolute_filepath, "");
      }
    }
#else
    (void)b_data;
    (void)b_ntree;
#endif
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexImage)) {
    BL::ShaderNodeTexImage b_image_node(b_node);
    BL::Image b_image(b_image_node.image());
    BL::ImageUser b_image_user(b_image_node.image_user());
    ImageTextureNode *image = graph->create_node<ImageTextureNode>();

    image->set_interpolation(get_image_interpolation(b_image_node));
    image->set_extension(get_image_extension(b_image_node));
    image->set_projection((NodeImageProjection)b_image_node.projection());
    image->set_projection_blend(b_image_node.projection_blend());
    BL::TexMapping b_texture_mapping(b_image_node.texture_mapping());
    get_tex_mapping(image, b_texture_mapping);

    if (b_image) {
      const BL::Image::source_enum b_image_source = b_image.source();
      PointerRNA colorspace_ptr = b_image.colorspace_settings().ptr;
      image->set_colorspace(ustring(get_enum_identifier(colorspace_ptr, "name")));

      image->set_animated(is_image_animated(b_image_source, b_image_user));
      image->set_alpha_type(get_image_alpha_type(b_image));

      if (b_image_source == BL::Image::source_TILED) {
        array<int> tiles;
        for (BL::UDIMTile &b_tile : b_image.tiles) {
          tiles.push_back_slow(b_tile.number());
        }
        image->set_tiles(tiles);
      }

      /* builtin images will use callback-based reading because
       * they could only be loaded correct from blender side
       */
      const bool is_builtin = image_is_builtin(b_image, b_engine);

      if (is_builtin) {
        /* for builtin images we're using image datablock name to find an image to
         * read pixels from later
         *
         * also store frame number as well, so there's no differences in handling
         * builtin names for packed images and movies
         */
        const int scene_frame = b_scene.frame_current();
        const int image_frame = image_user_frame_number(b_image_user, b_image, scene_frame);
        if (b_image_source != BL::Image::source_TILED) {
          image->handle = scene->image_manager->add_image(
              make_unique<BlenderImageLoader>(static_cast<::Image *>(b_image.ptr.data),
                                              static_cast<::ImageUser *>(b_image_user.ptr.data),
                                              image_frame,
                                              0,
                                              b_engine.is_preview()),
              image->image_params());
        }
        else {
          vector<unique_ptr<ImageLoader>> loaders;
          loaders.reserve(image->get_tiles().size());
          for (const int tile_number : image->get_tiles()) {
            loaders.push_back(
                make_unique<BlenderImageLoader>(static_cast<::Image *>(b_image.ptr.data),
                                                static_cast<::ImageUser *>(b_image_user.ptr.data),
                                                image_frame,
                                                tile_number,
                                                b_engine.is_preview()));
          }

          image->handle = scene->image_manager->add_image(std::move(loaders),
                                                          image->image_params());
        }
      }
      else {
        const ustring filename = ustring(
            image_user_file_path(b_data, b_image_user, b_image, b_scene.frame_current()));
        image->set_filename(filename);
      }
    }
    node = image;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexEnvironment)) {
    BL::ShaderNodeTexEnvironment b_env_node(b_node);
    BL::Image b_image(b_env_node.image());
    BL::ImageUser b_image_user(b_env_node.image_user());
    EnvironmentTextureNode *env = graph->create_node<EnvironmentTextureNode>();

    env->set_interpolation(get_image_interpolation(b_env_node));
    env->set_projection((NodeEnvironmentProjection)b_env_node.projection());
    BL::TexMapping b_texture_mapping(b_env_node.texture_mapping());
    get_tex_mapping(env, b_texture_mapping);

    if (b_image) {
      const BL::Image::source_enum b_image_source = b_image.source();
      PointerRNA colorspace_ptr = b_image.colorspace_settings().ptr;
      env->set_colorspace(ustring(get_enum_identifier(colorspace_ptr, "name")));
      env->set_animated(is_image_animated(b_image_source, b_image_user));
      env->set_alpha_type(get_image_alpha_type(b_image));

      const bool is_builtin = image_is_builtin(b_image, b_engine);

      if (is_builtin) {
        const int scene_frame = b_scene.frame_current();
        const int image_frame = image_user_frame_number(b_image_user, b_image, scene_frame);
        env->handle = scene->image_manager->add_image(
            make_unique<BlenderImageLoader>(static_cast<::Image *>(b_image.ptr.data),
                                            static_cast<::ImageUser *>(b_image_user.ptr.data),
                                            image_frame,
                                            0,
                                            b_engine.is_preview()),
            env->image_params());
      }
      else {
        env->set_filename(
            ustring(image_user_file_path(b_data, b_image_user, b_image, b_scene.frame_current())));
      }
    }
    node = env;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexGradient)) {
    BL::ShaderNodeTexGradient b_gradient_node(b_node);
    GradientTextureNode *gradient = graph->create_node<GradientTextureNode>();
    gradient->set_gradient_type((NodeGradientType)b_gradient_node.gradient_type());
    BL::TexMapping b_texture_mapping(b_gradient_node.texture_mapping());
    get_tex_mapping(gradient, b_texture_mapping);
    node = gradient;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexVoronoi)) {
    BL::ShaderNodeTexVoronoi b_voronoi_node(b_node);
    VoronoiTextureNode *voronoi = graph->create_node<VoronoiTextureNode>();
    voronoi->set_dimensions(b_voronoi_node.voronoi_dimensions());
    voronoi->set_feature((NodeVoronoiFeature)b_voronoi_node.feature());
    voronoi->set_metric((NodeVoronoiDistanceMetric)b_voronoi_node.distance());
    voronoi->set_use_normalize(b_voronoi_node.normalize());
    BL::TexMapping b_texture_mapping(b_voronoi_node.texture_mapping());
    get_tex_mapping(voronoi, b_texture_mapping);
    node = voronoi;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexMagic)) {
    BL::ShaderNodeTexMagic b_magic_node(b_node);
    MagicTextureNode *magic = graph->create_node<MagicTextureNode>();
    magic->set_depth(b_magic_node.turbulence_depth());
    BL::TexMapping b_texture_mapping(b_magic_node.texture_mapping());
    get_tex_mapping(magic, b_texture_mapping);
    node = magic;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexWave)) {
    BL::ShaderNodeTexWave b_wave_node(b_node);
    WaveTextureNode *wave = graph->create_node<WaveTextureNode>();
    wave->set_wave_type((NodeWaveType)b_wave_node.wave_type());
    wave->set_bands_direction((NodeWaveBandsDirection)b_wave_node.bands_direction());
    wave->set_rings_direction((NodeWaveRingsDirection)b_wave_node.rings_direction());
    wave->set_profile((NodeWaveProfile)b_wave_node.wave_profile());
    BL::TexMapping b_texture_mapping(b_wave_node.texture_mapping());
    get_tex_mapping(wave, b_texture_mapping);
    node = wave;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexChecker)) {
    BL::ShaderNodeTexChecker b_checker_node(b_node);
    CheckerTextureNode *checker = graph->create_node<CheckerTextureNode>();
    BL::TexMapping b_texture_mapping(b_checker_node.texture_mapping());
    get_tex_mapping(checker, b_texture_mapping);
    node = checker;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexBrick)) {
    BL::ShaderNodeTexBrick b_brick_node(b_node);
    BrickTextureNode *brick = graph->create_node<BrickTextureNode>();
    brick->set_offset(b_brick_node.offset());
    brick->set_offset_frequency(b_brick_node.offset_frequency());
    brick->set_squash(b_brick_node.squash());
    brick->set_squash_frequency(b_brick_node.squash_frequency());
    BL::TexMapping b_texture_mapping(b_brick_node.texture_mapping());
    get_tex_mapping(brick, b_texture_mapping);
    node = brick;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexNoise)) {
    BL::ShaderNodeTexNoise b_noise_node(b_node);
    NoiseTextureNode *noise = graph->create_node<NoiseTextureNode>();
    noise->set_dimensions(b_noise_node.noise_dimensions());
    noise->set_type((NodeNoiseType)b_noise_node.noise_type());
    noise->set_use_normalize(b_noise_node.normalize());
    BL::TexMapping b_texture_mapping(b_noise_node.texture_mapping());
    get_tex_mapping(noise, b_texture_mapping);
    node = noise;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexGabor)) {
    BL::ShaderNodeTexGabor b_gabor_node(b_node);
    GaborTextureNode *gabor = graph->create_node<GaborTextureNode>();
    gabor->set_type((NodeGaborType)b_gabor_node.gabor_type());
    BL::TexMapping b_texture_mapping(b_gabor_node.texture_mapping());
    get_tex_mapping(gabor, b_texture_mapping);
    node = gabor;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexCoord)) {
    BL::ShaderNodeTexCoord b_tex_coord_node(b_node);
    TextureCoordinateNode *tex_coord = graph->create_node<TextureCoordinateNode>();
    tex_coord->set_from_dupli(b_tex_coord_node.from_instancer());
    if (b_tex_coord_node.object()) {
      tex_coord->set_use_transform(true);
      tex_coord->set_ob_tfm(get_transform(b_tex_coord_node.object().matrix_world()));
    }
    node = tex_coord;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexSky)) {
    BL::ShaderNodeTexSky b_sky_node(b_node);
    SkyTextureNode *sky = graph->create_node<SkyTextureNode>();
    sky->set_sky_type((NodeSkyType)b_sky_node.sky_type());
    sky->set_sun_direction(normalize(get_float3(b_sky_node.sun_direction())));
    sky->set_turbidity(b_sky_node.turbidity());
    sky->set_ground_albedo(b_sky_node.ground_albedo());
    sky->set_sun_disc(b_sky_node.sun_disc());
    sky->set_sun_size(b_sky_node.sun_size());
    sky->set_sun_intensity(b_sky_node.sun_intensity());
    sky->set_sun_elevation(b_sky_node.sun_elevation());
    sky->set_sun_rotation(b_sky_node.sun_rotation());
    sky->set_altitude(b_sky_node.altitude());
    sky->set_air_density(b_sky_node.air_density());
    sky->set_aerosol_density(b_sky_node.aerosol_density());
    sky->set_ozone_density(b_sky_node.ozone_density());
    BL::TexMapping b_texture_mapping(b_sky_node.texture_mapping());
    get_tex_mapping(sky, b_texture_mapping);
    node = sky;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexIES)) {
    BL::ShaderNodeTexIES b_ies_node(b_node);
    IESLightNode *ies = graph->create_node<IESLightNode>();
    switch (b_ies_node.mode()) {
      case BL::ShaderNodeTexIES::mode_EXTERNAL:
        ies->set_filename(ustring(blender_absolute_path(b_data, b_ntree, b_ies_node.filepath())));
        break;
      case BL::ShaderNodeTexIES::mode_INTERNAL:
        ustring ies_content = ustring(get_text_datablock_content(b_ies_node.ies().ptr));
        if (ies_content.empty()) {
          ies_content = "\n";
        }
        ies->set_ies(ies_content);
        break;
    }
    node = ies;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexWhiteNoise)) {
    BL::ShaderNodeTexWhiteNoise b_tex_white_noise_node(b_node);
    WhiteNoiseTextureNode *white_noise_node = graph->create_node<WhiteNoiseTextureNode>();
    white_noise_node->set_dimensions(b_tex_white_noise_node.noise_dimensions());
    node = white_noise_node;
  }
  else if (b_node.is_a(&RNA_ShaderNodeNormalMap)) {
    BL::ShaderNodeNormalMap b_normal_map_node(b_node);
    NormalMapNode *nmap = graph->create_node<NormalMapNode>();
    nmap->set_space((NodeNormalMapSpace)b_normal_map_node.space());
    nmap->set_attribute(ustring(b_normal_map_node.uv_map()));
    node = nmap;
  }
  else if (b_node.is_a(&RNA_ShaderNodeRadialTiling)) {
    BL::ShaderNodeRadialTiling b_radial_tiling_node(b_node);
    RadialTilingNode *radial_tiling = graph->create_node<RadialTilingNode>();
    radial_tiling->set_use_normalize(b_radial_tiling_node.normalize());
    node = radial_tiling;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTangent)) {
    BL::ShaderNodeTangent b_tangent_node(b_node);
    TangentNode *tangent = graph->create_node<TangentNode>();
    tangent->set_direction_type((NodeTangentDirectionType)b_tangent_node.direction_type());
    tangent->set_axis((NodeTangentAxis)b_tangent_node.axis());
    tangent->set_attribute(ustring(b_tangent_node.uv_map()));
    node = tangent;
  }
  else if (b_node.is_a(&RNA_ShaderNodeUVMap)) {
    BL::ShaderNodeUVMap b_uvmap_node(b_node);
    UVMapNode *uvm = graph->create_node<UVMapNode>();
    uvm->set_attribute(ustring(b_uvmap_node.uv_map()));
    uvm->set_from_dupli(b_uvmap_node.from_instancer());
    node = uvm;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBevel)) {
    BL::ShaderNodeBevel b_bevel_node(b_node);
    BevelNode *bevel = graph->create_node<BevelNode>();
    bevel->set_samples(b_bevel_node.samples());
    node = bevel;
  }
  else if (b_node.is_a(&RNA_ShaderNodeDisplacement)) {
    BL::ShaderNodeDisplacement b_disp_node(b_node);
    DisplacementNode *disp = graph->create_node<DisplacementNode>();
    disp->set_space((NodeNormalMapSpace)b_disp_node.space());
    node = disp;
  }
  else if (b_node.is_a(&RNA_ShaderNodeVectorDisplacement)) {
    BL::ShaderNodeVectorDisplacement b_disp_node(b_node);
    VectorDisplacementNode *disp = graph->create_node<VectorDisplacementNode>();
    disp->set_space((NodeNormalMapSpace)b_disp_node.space());
    disp->set_attribute(ustring(""));
    node = disp;
  }
  else if (b_node.is_a(&RNA_ShaderNodeOutputAOV)) {
    BL::ShaderNodeOutputAOV b_aov_node(b_node);
    OutputAOVNode *aov = graph->create_node<OutputAOVNode>();
    aov->set_name(ustring(b_aov_node.aov_name()));
    node = aov;
  }

  if (node) {
    node->name = b_node.name();
  }

  return node;
}

static bool node_use_modified_socket_name(ShaderNode *node)
{
  if (node->special_type == SHADER_SPECIAL_TYPE_OSL) {
    return false;
  }

  return true;
}

static ShaderInput *node_find_input_by_name(BL::Node b_node,
                                            ShaderNode *node,
                                            BL::NodeSocket &b_socket)
{
  string name = b_socket.identifier();
  ShaderInput *input = node->input(name.c_str());

  if (!input && node_use_modified_socket_name(node)) {
    /* Different internal name for shader. */
    if (string_startswith(name, "Shader")) {
      string_replace(name, "Shader", "Closure");
    }

    /* Map mix node internal name for shader. */
    if (b_node.is_a(&RNA_ShaderNodeMix)) {
      if (string_endswith(name, "Factor_Float")) {
        string_replace(name, "Factor_Float", "Factor");
      }
      else if (string_endswith(name, "Factor_Vector")) {
        string_replace(name, "Factor_Vector", "Factor");
      }
      else if (string_endswith(name, "A_Float")) {
        string_replace(name, "A_Float", "A");
      }
      else if (string_endswith(name, "B_Float")) {
        string_replace(name, "B_Float", "B");
      }
      else if (string_endswith(name, "A_Color")) {
        string_replace(name, "A_Color", "A");
      }
      else if (string_endswith(name, "B_Color")) {
        string_replace(name, "B_Color", "B");
      }
      else if (string_endswith(name, "A_Vector")) {
        string_replace(name, "A_Vector", "A");
      }
      else if (string_endswith(name, "B_Vector")) {
        string_replace(name, "B_Vector", "B");
      }
    }

    input = node->input(name.c_str());

    if (!input) {
      /* Different internal numbering of two sockets with same name.
       * Note that the Blender convention for unique socket names changed
       * from . to _ at some point, so we check both to handle old files. */
      if (string_endswith(name, "_001")) {
        string_replace(name, "_001", "2");
      }
      else if (string_endswith(name, ".001")) {
        string_replace(name, ".001", "2");
      }
      else if (string_endswith(name, "_002")) {
        string_replace(name, "_002", "3");
      }
      else if (string_endswith(name, ".002")) {
        string_replace(name, ".002", "3");
      }
      else {
        name += "1";
      }

      input = node->input(name.c_str());
    }
  }

  return input;
}

static ShaderOutput *node_find_output_by_name(BL::Node b_node,
                                              ShaderNode *node,
                                              BL::NodeSocket &b_socket)
{
  string name = b_socket.identifier();
  ShaderOutput *output = node->output(name.c_str());

  if (!output && node_use_modified_socket_name(node)) {
    /* Different internal name for shader. */
    if (name == "Shader") {
      name = "Closure";
      output = node->output(name.c_str());
    }
    /* Map internal name for shader. */
    if (b_node.is_a(&RNA_ShaderNodeMix)) {
      if (string_endswith(name, "Result_Float")) {
        string_replace(name, "Result_Float", "Result");
        output = node->output(name.c_str());
      }
      else if (string_endswith(name, "Result_Color")) {
        string_replace(name, "Result_Color", "Result");
        output = node->output(name.c_str());
      }
      else if (string_endswith(name, "Result_Vector")) {
        string_replace(name, "Result_Vector", "Result");
        output = node->output(name.c_str());
      }
    }
  }

  return output;
}

static void add_nodes(Scene *scene,
                      BL::RenderEngine &b_engine,
                      BL::BlendData &b_data,
                      BL::Scene &b_scene,
                      ShaderGraph *graph,
                      BL::ShaderNodeTree &b_ntree,
                      const ProxyMap &proxy_input_map,
                      const ProxyMap &proxy_output_map);

static void add_nodes_inlined(Scene *scene,
                              BL::RenderEngine &b_engine,
                              BL::BlendData &b_data,
                              BL::Scene &b_scene,
                              ShaderGraph *graph,
                              BL::ShaderNodeTree &b_ntree,
                              const ProxyMap &proxy_input_map,
                              const ProxyMap &proxy_output_map)
{
  /* add nodes */
  PtrInputMap input_map;
  PtrOutputMap output_map;

  /* find the node to use for output if there are multiple */
  const BL::ShaderNode output_node = b_ntree.get_output_node(
      BL::ShaderNodeOutputMaterial::target_CYCLES);

  /* add nodes */
  for (BL::Node &b_node : b_ntree.nodes) {
    if (b_node.mute() || b_node.is_a(&RNA_NodeReroute)) {
      /* replace muted node with internal links */
      for (BL::NodeLink &b_link : b_node.internal_links) {
        BL::NodeSocket to_socket(b_link.to_socket());
        const SocketType::Type to_socket_type = convert_socket_type(to_socket);
        if (to_socket_type == SocketType::UNDEFINED) {
          continue;
        }

        ConvertNode *proxy = graph->create_node<ConvertNode>(to_socket_type, to_socket_type, true);

        /* Muted nodes can result in multiple Cycles input sockets mapping to the same Blender
         * input socket, so this needs to be a multimap. */
        input_map.emplace(b_link.from_socket().ptr.data, proxy->inputs[0]);
        output_map[b_link.to_socket().ptr.data] = proxy->outputs[0];
      }
    }
    else if (b_node.is_a(&RNA_ShaderNodeGroup) || b_node.is_a(&RNA_NodeCustomGroup) ||
             b_node.is_a(&RNA_ShaderNodeCustomGroup))
    {

      BL::ShaderNodeTree b_group_ntree(PointerRNA_NULL);
      if (b_node.is_a(&RNA_ShaderNodeGroup)) {
        b_group_ntree = BL::ShaderNodeTree(((BL::NodeGroup)(b_node)).node_tree());
      }
      else if (b_node.is_a(&RNA_NodeCustomGroup)) {
        b_group_ntree = BL::ShaderNodeTree(((BL::NodeCustomGroup)(b_node)).node_tree());
      }
      else {
        b_group_ntree = BL::ShaderNodeTree(((BL::ShaderNodeCustomGroup)(b_node)).node_tree());
      }

      ProxyMap group_proxy_input_map;
      ProxyMap group_proxy_output_map;

      /* Add a proxy node for each socket
       * Do this even if the node group has no internal tree,
       * so that links have something to connect to and assert won't fail.
       */
      for (BL::NodeSocket &b_input : b_node.inputs) {
        const SocketType::Type input_type = convert_socket_type(b_input);
        if (input_type == SocketType::UNDEFINED) {
          continue;
        }

        ConvertNode *proxy = graph->create_node<ConvertNode>(input_type, input_type, true);

        /* register the proxy node for internal binding */
        group_proxy_input_map[b_input.identifier()] = proxy;

        input_map.emplace(b_input.ptr.data, proxy->inputs[0]);

        set_default_value(proxy->inputs[0], b_input, b_data, b_ntree);
      }
      for (BL::NodeSocket &b_output : b_node.outputs) {
        const SocketType::Type output_type = convert_socket_type(b_output);
        if (output_type == SocketType::UNDEFINED) {
          continue;
        }

        ConvertNode *proxy = graph->create_node<ConvertNode>(output_type, output_type, true);

        /* register the proxy node for internal binding */
        group_proxy_output_map[b_output.identifier()] = proxy;

        output_map[b_output.ptr.data] = proxy->outputs[0];
      }

      if (b_group_ntree) {
        add_nodes(scene,
                  b_engine,
                  b_data,
                  b_scene,
                  graph,
                  b_group_ntree,
                  group_proxy_input_map,
                  group_proxy_output_map);
      }
    }
    else if (b_node.is_a(&RNA_NodeGroupInput)) {
      /* map each socket to a proxy node */
      for (BL::NodeSocket &b_output : b_node.outputs) {
        const ProxyMap::const_iterator proxy_it = proxy_input_map.find(b_output.identifier());
        if (proxy_it != proxy_input_map.end()) {
          ConvertNode *proxy = proxy_it->second;

          output_map[b_output.ptr.data] = proxy->outputs[0];
        }
      }
    }
    else if (b_node.is_a(&RNA_NodeGroupOutput)) {
      BL::NodeGroupOutput b_output_node(b_node);
      /* only the active group output is used */
      if (b_output_node.is_active_output()) {
        /* map each socket to a proxy node */
        for (BL::NodeSocket &b_input : b_node.inputs) {
          const ProxyMap::const_iterator proxy_it = proxy_output_map.find(b_input.identifier());
          if (proxy_it != proxy_output_map.end()) {
            ConvertNode *proxy = proxy_it->second;

            input_map.emplace(b_input.ptr.data, proxy->inputs[0]);

            set_default_value(proxy->inputs[0], b_input, b_data, b_ntree);
          }
        }
      }
    }
    /* TODO: All the previous cases can be removed? */
    else {
      ShaderNode *node = nullptr;

      if (b_node.ptr.data == output_node.ptr.data) {
        node = graph->output();
      }
      else {
        BL::ShaderNode b_shader_node(b_node);
        node = add_node(scene, b_engine, b_data, b_scene, graph, b_ntree, b_shader_node);
      }

      if (node) {
        /* map node sockets for linking */
        for (BL::NodeSocket &b_input : b_node.inputs) {
          if (b_input.is_unavailable()) {
            /* Skip unavailable sockets. */
            continue;
          }
          ShaderInput *input = node_find_input_by_name(b_node, node, b_input);
          if (!input) {
            /* XXX should not happen, report error? */
            continue;
          }
          input_map.emplace(b_input.ptr.data, input);

          set_default_value(input, b_input, b_data, b_ntree);
        }
        for (BL::NodeSocket &b_output : b_node.outputs) {
          if (b_output.is_unavailable()) {
            /* Skip unavailable sockets. */
            continue;
          }
          ShaderOutput *output = node_find_output_by_name(b_node, node, b_output);
          if (!output) {
            /* XXX should not happen, report error? */
            continue;
          }
          output_map[b_output.ptr.data] = output;
        }
      }
    }
  }

  /* connect nodes */
  for (BL::NodeLink &b_link : b_ntree.links) {
    /* Ignore invalid links to avoid unwanted cycles created in graph.
     * Also ignore links with unavailable sockets. */
    if (!(b_link.is_valid() && b_link.from_socket().enabled() && b_link.to_socket().enabled()) ||
        b_link.is_muted())
    {
      continue;
    }
    /* get blender link data */
    const BL::NodeSocket b_from_sock = b_link.from_socket();
    const BL::NodeSocket b_to_sock = b_link.to_socket();

    ShaderOutput *output = nullptr;
    const PtrOutputMap::iterator output_it = output_map.find(b_from_sock.ptr.data);
    if (output_it != output_map.end()) {
      output = output_it->second;
    }

    /* either socket may be nullptr when the node was not exported, typically
     * because the node type is not supported */
    if (output != nullptr) {
      ShaderOutput *output = output_it->second;
      auto inputs = input_map.equal_range(b_to_sock.ptr.data);
      for (PtrInputMap::iterator input_it = inputs.first; input_it != inputs.second; ++input_it) {
        ShaderInput *input = input_it->second;
        if (input != nullptr) {
          graph->connect(output, input);
        }
      }
    }
  }
}

static void add_nodes(Scene *scene,
                      BL::RenderEngine &b_engine,
                      BL::BlendData &b_data,
                      BL::Scene &b_scene,
                      ShaderGraph *graph,
                      BL::ShaderNodeTree &b_ntree,
                      const ProxyMap &proxy_input_map,
                      const ProxyMap &proxy_output_map)
{
  bNodeTree *ntree = b_ntree.ptr.data_as<bNodeTree>();
  bNodeTree *localtree = blender::bke::node_tree_add_tree(
      nullptr, (blender::StringRef(ntree->id.name) + " Inlined").c_str(), ntree->idname);
  blender::nodes::InlineShaderNodeTreeParams inline_params;
  inline_params.allow_preserving_repeat_zones = false;
  blender::nodes::inline_shader_node_tree(*ntree, *localtree, inline_params);

  BL::ShaderNodeTree b_localtree(RNA_id_pointer_create(&localtree->id));
  add_nodes_inlined(
      scene, b_engine, b_data, b_scene, graph, b_localtree, proxy_input_map, proxy_output_map);

  BKE_id_free(nullptr, &localtree->id);
}

static void add_nodes(Scene *scene,
                      BL::RenderEngine &b_engine,
                      BL::BlendData &b_data,
                      BL::Scene &b_scene,
                      ShaderGraph *graph,
                      BL::ShaderNodeTree &b_ntree)
{
  static const ProxyMap empty_proxy_map;
  add_nodes(scene, b_engine, b_data, b_scene, graph, b_ntree, empty_proxy_map, empty_proxy_map);
}

/* Look up and constant fold all references to View Layer attributes. */
void BlenderSync::resolve_view_layer_attributes(Shader *shader,
                                                ShaderGraph *graph,
                                                BL::Depsgraph &b_depsgraph)
{
  bool updated = false;

  for (ShaderNode *node : graph->nodes) {
    if (node->is_a(AttributeNode::get_node_type())) {
      AttributeNode *attr_node = static_cast<AttributeNode *>(node);

      std::string real_name;
      const BlenderAttributeType type = blender_attribute_name_split_type(
          attr_node->get_attribute(), &real_name);

      if (type == BL::ShaderNodeAttribute::attribute_type_VIEW_LAYER) {
        /* Look up the value. */
        const BL::ViewLayer b_layer = b_depsgraph.view_layer_eval();
        const BL::Scene b_scene = b_depsgraph.scene_eval();
        float4 value;

        BKE_view_layer_find_rgba_attribute((::Scene *)b_scene.ptr.data,
                                           (::ViewLayer *)b_layer.ptr.data,
                                           real_name.c_str(),
                                           &value.x);

        /* Replace all outgoing links, using appropriate output types. */
        const float val_avg = (value.x + value.y + value.z) / 3.0f;

        for (ShaderOutput *output : node->outputs) {
          float val_float;
          float3 val_float3;

          if (output->type() == SocketType::FLOAT) {
            val_float = (output->name() == "Alpha") ? value.w : val_avg;
            val_float3 = make_float3(val_float);
          }
          else {
            val_float = val_avg;
            val_float3 = make_float3(value);
          }

          for (ShaderInput *sock : output->links) {
            if (sock->type() == SocketType::FLOAT) {
              sock->set(val_float);
            }
            else if (SocketType::is_float3(sock->type())) {
              sock->set(val_float3);
            }

            sock->constant_folded_in = true;
          }

          graph->disconnect(output);
        }

        /* Clear the attribute name to avoid further attempts to look up. */
        attr_node->set_attribute(ustring());
        updated = true;
      }
    }
  }

  if (updated) {
    shader_map.set_flag(shader, SHADER_WITH_LAYER_ATTRS);
  }
  else {
    shader_map.clear_flag(shader, SHADER_WITH_LAYER_ATTRS);
  }
}

bool BlenderSync::scene_attr_needs_recalc(Shader *shader, BL::Depsgraph &b_depsgraph)
{
  if (shader && shader_map.test_flag(shader, SHADER_WITH_LAYER_ATTRS)) {
    BL::Scene scene = b_depsgraph.scene_eval();

    return shader_map.check_recalc(scene) || shader_map.check_recalc(scene.world()) ||
           shader_map.check_recalc(scene.camera());
  }

  return false;
}

/* Sync Materials */

void BlenderSync::sync_materials(BL::Depsgraph &b_depsgraph, bool update_all)
{
  shader_map.set_default(scene->default_surface);

  TaskPool pool;
  set<Shader *> updated_shaders;

  for (BL::ID &b_id : b_depsgraph.ids) {
    if (!b_id.is_a(&RNA_Material)) {
      continue;
    }

    BL::Material b_mat(b_id);
    Shader *shader;

    /* test if we need to sync */
    if (shader_map.add_or_update(&shader, b_mat) || update_all ||
        scene_attr_needs_recalc(shader, b_depsgraph))
    {
      unique_ptr<ShaderGraph> graph = make_unique<ShaderGraph>();

      shader->name = b_mat.name().c_str();
      shader->set_pass_id(b_mat.pass_index());

      /* create nodes */
      if (b_mat.node_tree()) {
        BL::ShaderNodeTree b_ntree(b_mat.node_tree());

        add_nodes(scene, b_engine, b_data, b_scene, graph.get(), b_ntree);
      }
      else {
        DiffuseBsdfNode *diffuse = graph->create_node<DiffuseBsdfNode>();
        diffuse->set_color(get_float3(b_mat.diffuse_color()));

        ShaderNode *out = graph->output();
        graph->connect(diffuse->output("BSDF"), out->input("Surface"));
      }

      resolve_view_layer_attributes(shader, graph.get(), b_depsgraph);

      /* settings */
      PointerRNA cmat = RNA_pointer_get(&b_mat.ptr, "cycles");
      shader->set_emission_sampling_method(get_emission_sampling(cmat));
      shader->set_use_transparent_shadow(b_mat.use_transparent_shadow());
      shader->set_use_bump_map_correction(get_boolean(cmat, "use_bump_map_correction"));
      shader->set_volume_sampling_method(get_volume_sampling(cmat));
      shader->set_volume_interpolation_method(get_volume_interpolation(cmat));
      shader->set_volume_step_rate(get_float(cmat, "volume_step_rate"));
      shader->set_displacement_method(get_displacement_method(b_mat));

      shader->set_graph(std::move(graph));

      /* By simplifying the shader graph as soon as possible, some
       * redundant shader nodes might be removed which prevents loading
       * unnecessary attributes later.
       *
       * However, since graph simplification also accounts for mix
       * weight, this would cause frequent expensive resyncs in interactive
       * sessions, so for those sessions optimization is only performed
       * right before compiling.
       */
      if (!preview) {
        pool.push([graph = shader->graph.get(), scene = scene] { graph->simplify(scene); });
        /* NOTE: Update shaders out of the threads since those routines
         * are accessing and writing to a global context.
         */
        updated_shaders.insert(shader);
      }
      else {
        /* NOTE: Update tagging can access links which are being
         * optimized out.
         */
        shader->tag_update(scene);
      }
    }
  }

  pool.wait_work();

  for (Shader *shader : updated_shaders) {
    shader->tag_update(scene);
  }
}

/* Sync World */

void BlenderSync::sync_world(BL::Depsgraph &b_depsgraph, BL::SpaceView3D &b_v3d, bool update_all)
{
  Background *background = scene->background;
  Integrator *integrator = scene->integrator;
  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

  BL::World b_world = view_layer.world_override ? view_layer.world_override : b_scene.world();

  const BlenderViewportParameters new_viewport_parameters(b_v3d, use_developer_ui);

  Shader *shader = scene->default_background;

  if (world_recalc || update_all || b_world.ptr.data != world_map ||
      viewport_parameters.shader_modified(new_viewport_parameters) ||
      scene_attr_needs_recalc(shader, b_depsgraph))
  {
    unique_ptr<ShaderGraph> graph = make_unique<ShaderGraph>();

    /* create nodes */
    if (new_viewport_parameters.use_scene_world && b_world && b_world.node_tree()) {
      BL::ShaderNodeTree b_ntree(b_world.node_tree());

      add_nodes(scene, b_engine, b_data, b_scene, graph.get(), b_ntree);

      /* volume */
      PointerRNA cworld = RNA_pointer_get(&b_world.ptr, "cycles");
      shader->set_volume_sampling_method(get_volume_sampling(cworld));
      shader->set_volume_interpolation_method(get_volume_interpolation(cworld));
      shader->set_volume_step_rate(get_float(cworld, "volume_step_size"));
    }
    else if (new_viewport_parameters.use_scene_world && b_world) {
      BackgroundNode *background = graph->create_node<BackgroundNode>();
      background->set_color(get_float3(b_world.color()));

      ShaderNode *out = graph->output();
      graph->connect(background->output("Background"), out->input("Surface"));
    }
    else if (!new_viewport_parameters.use_scene_world) {
      float3 world_color;
      if (b_world) {
        world_color = get_float3(b_world.color());
      }
      else {
        world_color = zero_float3();
      }

      BackgroundNode *background = graph->create_node<BackgroundNode>();
      LightPathNode *light_path = graph->create_node<LightPathNode>();

      MixNode *mix_scene_with_background = graph->create_node<MixNode>();
      mix_scene_with_background->set_color2(world_color);

      EnvironmentTextureNode *texture_environment = graph->create_node<EnvironmentTextureNode>();
      texture_environment->set_tex_mapping_type(TextureMapping::VECTOR);
      float3 rotation_z = texture_environment->get_tex_mapping_rotation();
      rotation_z[2] = new_viewport_parameters.studiolight_rotate_z;
      texture_environment->set_tex_mapping_rotation(rotation_z);
      texture_environment->set_filename(new_viewport_parameters.studiolight_path);

      MixNode *mix_intensity = graph->create_node<MixNode>();
      mix_intensity->set_mix_type(NODE_MIX_MUL);
      mix_intensity->set_fac(1.0f);
      mix_intensity->set_color2(make_float3(new_viewport_parameters.studiolight_intensity,
                                            new_viewport_parameters.studiolight_intensity,
                                            new_viewport_parameters.studiolight_intensity));

      TextureCoordinateNode *texture_coordinate = graph->create_node<TextureCoordinateNode>();

      MixNode *mix_background_with_environment = graph->create_node<MixNode>();
      mix_background_with_environment->set_fac(
          new_viewport_parameters.studiolight_background_alpha);
      mix_background_with_environment->set_color1(world_color);

      ShaderNode *out = graph->output();

      graph->connect(texture_coordinate->output("Generated"),
                     texture_environment->input("Vector"));
      graph->connect(texture_environment->output("Color"), mix_intensity->input("Color1"));
      graph->connect(light_path->output("Is Camera Ray"), mix_scene_with_background->input("Fac"));
      graph->connect(mix_intensity->output("Color"), mix_scene_with_background->input("Color1"));
      graph->connect(mix_intensity->output("Color"),
                     mix_background_with_environment->input("Color2"));
      graph->connect(mix_background_with_environment->output("Color"),
                     mix_scene_with_background->input("Color2"));
      graph->connect(mix_scene_with_background->output("Color"), background->input("Color"));
      graph->connect(background->output("Background"), out->input("Surface"));
    }

    /* Visibility */
    if (b_world) {
      PointerRNA cvisibility = RNA_pointer_get(&b_world.ptr, "cycles_visibility");
      uint visibility = 0;

      visibility |= get_boolean(cvisibility, "camera") ? PATH_RAY_CAMERA : PathRayFlag(0);
      visibility |= get_boolean(cvisibility, "diffuse") ? PATH_RAY_DIFFUSE : PathRayFlag(0);
      visibility |= get_boolean(cvisibility, "glossy") ? PATH_RAY_GLOSSY : PathRayFlag(0);
      visibility |= get_boolean(cvisibility, "transmission") ? PATH_RAY_TRANSMIT : PathRayFlag(0);
      visibility |= get_boolean(cvisibility, "scatter") ? PATH_RAY_VOLUME_SCATTER : PathRayFlag(0);

      background->set_visibility(visibility);
    }

    resolve_view_layer_attributes(shader, graph.get(), b_depsgraph);

    shader->set_graph(std::move(graph));
    shader->tag_update(scene);
  }

  /* Fast GI */
  if (b_world) {
    BL::WorldLighting b_light = b_world.light_settings();
    enum { FAST_GI_METHOD_REPLACE = 0, FAST_GI_METHOD_ADD = 1, FAST_GI_METHOD_NUM };

    const bool use_fast_gi = get_boolean(cscene, "use_fast_gi");
    if (use_fast_gi) {
      const int fast_gi_method = get_enum(
          cscene, "fast_gi_method", FAST_GI_METHOD_NUM, FAST_GI_METHOD_REPLACE);
      integrator->set_ao_factor((fast_gi_method == FAST_GI_METHOD_REPLACE) ? b_light.ao_factor() :
                                                                             0.0f);
      integrator->set_ao_additive_factor(
          (fast_gi_method == FAST_GI_METHOD_ADD) ? b_light.ao_factor() : 0.0f);
    }
    else {
      integrator->set_ao_factor(0.0f);
      integrator->set_ao_additive_factor(0.0f);
    }

    integrator->set_ao_distance(b_light.distance());
  }
  else {
    integrator->set_ao_factor(0.0f);
    integrator->set_ao_additive_factor(0.0f);
    integrator->set_ao_distance(10.0f);
  }

  background->set_transparent(b_scene.render().film_transparent());

  if (background->get_transparent()) {
    background->set_transparent_glass(get_boolean(cscene, "film_transparent_glass"));
    background->set_transparent_roughness_threshold(
        get_float(cscene, "film_transparent_roughness"));
  }
  else {
    background->set_transparent_glass(false);
    background->set_transparent_roughness_threshold(0.0f);
  }

  background->set_use_shader(view_layer.use_background_shader ||
                             viewport_parameters.use_custom_shader());

  background->set_lightgroup(ustring(b_world ? b_world.lightgroup() : ""));

  background->tag_update(scene);
}

/* Sync Lights */

void BlenderSync::sync_lights(BL::Depsgraph &b_depsgraph, bool update_all)
{
  shader_map.set_default(scene->default_light);

  for (BL::ID &b_id : b_depsgraph.ids) {
    if (!b_id.is_a(&RNA_Light)) {
      continue;
    }

    BL::Light b_light(b_id);
    Shader *shader;

    /* test if we need to sync */
    if (shader_map.add_or_update(&shader, b_light) || update_all ||
        scene_attr_needs_recalc(shader, b_depsgraph))
    {
      unique_ptr<ShaderGraph> graph = make_unique<ShaderGraph>();

      /* create nodes */
      if (b_light.use_nodes() && b_light.node_tree()) {
        shader->name = b_light.name().c_str();

        BL::ShaderNodeTree b_ntree(b_light.node_tree());

        add_nodes(scene, b_engine, b_data, b_scene, graph.get(), b_ntree);
      }
      else {
        EmissionNode *emission = graph->create_node<EmissionNode>();
        emission->set_color(one_float3());
        emission->set_strength(1.0f);

        ShaderNode *out = graph->output();
        graph->connect(emission->output("Emission"), out->input("Surface"));
      }

      resolve_view_layer_attributes(shader, graph.get(), b_depsgraph);

      shader->set_graph(std::move(graph));
      shader->tag_update(scene);
    }
  }
}

void BlenderSync::sync_shaders(BL::Depsgraph &b_depsgraph, BL::SpaceView3D &b_v3d, bool update_all)
{
  shader_map.pre_sync();

  sync_world(b_depsgraph, b_v3d, update_all);
  sync_lights(b_depsgraph, update_all);
  sync_materials(b_depsgraph, update_all);
}

CCL_NAMESPACE_END
