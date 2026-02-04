/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/shader.h"
#include "kernel/svm/types.h"
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

#include "BLI_listbase.h"

#include "BKE_duplilist.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "NOD_shader.h"
#include "NOD_shader_nodes_inline.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_world_types.h"

CCL_NAMESPACE_BEGIN

using PtrInputMap = unordered_multimap<void *, ShaderInput *>;
using PtrOutputMap = map<void *, ShaderOutput *>;
using ProxyMap = map<string, ConvertNode *>;

/* Find */

void BlenderSync::find_shader(const blender::ID *id,
                              array<Node *> &used_shaders,
                              Shader *default_shader)
{
  Shader *synced_shader = (id) ? shader_map.find(id) : nullptr;
  Shader *shader = (synced_shader) ? synced_shader : default_shader;

  used_shaders.push_back_slow(shader);
  shader->tag_used(scene);
}

/* RNA translation utilities */

static VolumeSampling get_volume_sampling(blender::PointerRNA &ptr)
{
  return (VolumeSampling)get_enum(
      ptr, "volume_sampling", VOLUME_NUM_SAMPLING, VOLUME_SAMPLING_DISTANCE);
}

static VolumeInterpolation get_volume_interpolation(blender::PointerRNA &ptr)
{
  return (VolumeInterpolation)get_enum(
      ptr, "volume_interpolation", VOLUME_NUM_INTERPOLATION, VOLUME_INTERPOLATION_LINEAR);
}

static EmissionSampling get_emission_sampling(blender::PointerRNA &ptr)
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

static DisplacementMethod get_displacement_method(blender::Material &b_mat)
{
  const int value = b_mat.displacement_method;
  return (DisplacementMethod)validate_enum_value(value, DISPLACE_NUM_METHODS, DISPLACE_BUMP);
}

template<typename NodeType> static InterpolationType get_image_interpolation(NodeType &b_node)
{
  const int value = b_node.interpolation;
  return (InterpolationType)validate_enum_value(
      value, INTERPOLATION_NUM_TYPES, INTERPOLATION_LINEAR);
}

template<typename NodeType> static ExtensionType get_image_extension(NodeType &b_node)
{
  const int value = b_node.extension;
  return (ExtensionType)validate_enum_value(value, EXTENSION_NUM_TYPES, EXTENSION_REPEAT);
}

static ImageAlphaType get_image_alpha_type(blender::Image &b_image)
{
  const int value = b_image.alpha_mode;
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

static ustring blender_attribute_name_add_type(const string &name, int type)
{
  switch (type) {
    case blender::SHD_ATTRIBUTE_OBJECT:
      return ustring::concat(object_attr_prefix, name);
    case blender::SHD_ATTRIBUTE_INSTANCER:
      return ustring::concat(instancer_attr_prefix, name);
    case blender::SHD_ATTRIBUTE_VIEW_LAYER:
      return ustring::concat(view_layer_attr_prefix, name);
    default:
      return ustring(name);
  }
}

int blender_attribute_name_split_type(ustring name, string *r_real_name)
{
  const string_view sname(name);

  if (sname.substr(0, object_attr_prefix.size()) == object_attr_prefix) {
    *r_real_name = sname.substr(object_attr_prefix.size());
    return blender::SHD_ATTRIBUTE_OBJECT;
  }

  if (sname.substr(0, instancer_attr_prefix.size()) == instancer_attr_prefix) {
    *r_real_name = sname.substr(instancer_attr_prefix.size());
    return blender::SHD_ATTRIBUTE_INSTANCER;
  }

  if (sname.substr(0, view_layer_attr_prefix.size()) == view_layer_attr_prefix) {
    *r_real_name = sname.substr(view_layer_attr_prefix.size());
    return blender::SHD_ATTRIBUTE_VIEW_LAYER;
  }

  return blender::SHD_ATTRIBUTE_GEOMETRY;
}

/* Graph */

static float3 get_node_output_rgba(blender::bNode &b_node, const string &name)
{
  blender::bNodeSocket *b_sock = b_node.output_by_identifier(name);
  BLI_assert(b_sock->type == blender::SOCK_RGBA);
  const auto &default_value = *b_sock->default_value_typed<blender::bNodeSocketValueRGBA>();
  return make_float3(default_value.value[0], default_value.value[1], default_value.value[2]);
}

static float get_node_output_value(blender::bNode &b_node, const string &name)
{
  blender::bNodeSocket *b_sock = b_node.output_by_identifier(name);
  BLI_assert(b_sock->type == blender::SOCK_FLOAT);
  const auto &default_value = *b_sock->default_value_typed<blender::bNodeSocketValueFloat>();
  return default_value.value;
}

static float3 get_node_output_vector(blender::bNode &b_node, const string &name)
{
  blender::bNodeSocket *b_sock = b_node.output_by_identifier(name);
  BLI_assert(b_sock->type == blender::SOCK_VECTOR);
  const auto &default_value = *b_sock->default_value_typed<blender::bNodeSocketValueVector>();
  return make_float3(default_value.value[0], default_value.value[1], default_value.value[2]);
}

static SocketType::Type convert_socket_type(blender::bNodeSocket &b_socket)
{
  switch (blender::eNodeSocketDatatype(b_socket.type)) {
    case blender::SOCK_FLOAT:
      return SocketType::FLOAT;
    case blender::SOCK_BOOLEAN:
    case blender::SOCK_INT:
      return SocketType::INT;
    case blender::SOCK_VECTOR:
      return SocketType::VECTOR;
    case blender::SOCK_RGBA:
      return SocketType::COLOR;
    case blender::SOCK_STRING:
      return SocketType::STRING;
    case blender::SOCK_SHADER:
      return SocketType::CLOSURE;

    default:
      return SocketType::UNDEFINED;
  }
}

static void set_default_value(ShaderInput *input,
                              blender::bNodeSocket &b_sock,
                              blender::Main &b_data,
                              blender::ID &b_id)
{
  Node *node = input->parent;
  const SocketType &socket = input->socket_type;

  /* copy values for non linked inputs */
  switch (input->type()) {
    case SocketType::FLOAT: {
      const auto &default_value = *b_sock.default_value_typed<blender::bNodeSocketValueFloat>();
      node->set(socket, default_value.value);
      break;
    }
    case SocketType::INT: {
      if (b_sock.type == blender::SOCK_BOOLEAN) {
        const auto &default_value =
            *b_sock.default_value_typed<blender::bNodeSocketValueBoolean>();
        /* Make sure to call the int overload of set() since this is an integer socket as far as
         * Cycles is concerned. */
        node->set(socket, default_value.value ? 1 : 0);
      }
      else {
        const auto &default_value = *b_sock.default_value_typed<blender::bNodeSocketValueInt>();
        node->set(socket, default_value.value);
      }
      break;
    }
    case SocketType::COLOR: {
      const auto &default_value = *b_sock.default_value_typed<blender::bNodeSocketValueRGBA>();
      const float *value = default_value.value;
      node->set(socket, make_float3(value[0], value[1], value[2]));
      break;
    }
    case SocketType::NORMAL:
    case SocketType::POINT:
    case SocketType::VECTOR: {
      const auto &default_value = *b_sock.default_value_typed<blender::bNodeSocketValueVector>();
      const float *value = default_value.value;
      node->set(socket, make_float3(value[0], value[1], value[2]));
      break;
    }
    case SocketType::STRING: {
      const auto &default_value = *b_sock.default_value_typed<blender::bNodeSocketValueString>();
      node->set(socket, (ustring)blender_absolute_path(b_data, b_id, default_value.value).c_str());
      break;
    }
    default:
      break;
  }
}

static void get_tex_mapping(TextureNode *mapping, const blender::TexMapping *b_mapping)
{
  if (!b_mapping) {
    return;
  }

  mapping->set_tex_mapping_translation(
      make_float3(b_mapping->loc[0], b_mapping->loc[1], b_mapping->loc[2]));
  mapping->set_tex_mapping_rotation(
      make_float3(b_mapping->rot[0], b_mapping->rot[1], b_mapping->rot[2]));
  mapping->set_tex_mapping_scale(
      make_float3(b_mapping->size[0], b_mapping->size[1], b_mapping->size[2]));
  mapping->set_tex_mapping_type((TextureMapping::Type)b_mapping->type);

  mapping->set_tex_mapping_x_mapping((TextureMapping::Mapping)b_mapping->projx);
  mapping->set_tex_mapping_y_mapping((TextureMapping::Mapping)b_mapping->projy);
  mapping->set_tex_mapping_z_mapping((TextureMapping::Mapping)b_mapping->projz);
}

static bool is_image_animated(blender::eImageSource b_image_source,
                              blender::ImageUser &b_image_user)
{
  return (b_image_source == blender::IMA_SRC_MOVIE ||
          b_image_source == blender::IMA_SRC_SEQUENCE) &&
         (b_image_user.flag & blender::IMA_ANIM_ALWAYS) != 0;
}

static ShaderNode *add_node(Scene *scene,
                            blender::RenderEngine &b_engine,
                            blender::Main &b_data,
                            blender::Scene &b_scene,
                            ShaderGraph *graph,
                            blender::bNodeTree &b_ntree,
                            blender::bNode &b_node)
{
  ShaderNode *node = nullptr;

  /* existing blender nodes */
  if (b_node.is_type("ShaderNodeRGBCurve")) {
    const auto &mapping = *static_cast<blender::CurveMapping *>(b_node.storage);
    RGBCurvesNode *curves = graph->create_node<RGBCurvesNode>();
    array<float3> curve_mapping_curves;
    float min_x;
    float max_x;
    curvemapping_color_to_array(mapping, curve_mapping_curves, RAMP_TABLE_SIZE, true);
    curvemapping_minmax(mapping, 4, &min_x, &max_x);
    curves->set_min_x(min_x);
    curves->set_max_x(max_x);
    curves->set_curves(curve_mapping_curves);
    curves->set_extrapolate((mapping.flag & blender::CUMA_EXTEND_EXTRAPOLATE) != 0);
    node = curves;
  }
  if (b_node.is_type("ShaderNodeVectorCurve")) {
    const auto &mapping = *static_cast<blender::CurveMapping *>(b_node.storage);
    VectorCurvesNode *curves = graph->create_node<VectorCurvesNode>();
    array<float3> curve_mapping_curves;
    float min_x;
    float max_x;
    curvemapping_color_to_array(mapping, curve_mapping_curves, RAMP_TABLE_SIZE, false);
    curvemapping_minmax(mapping, 3, &min_x, &max_x);
    curves->set_min_x(min_x);
    curves->set_max_x(max_x);
    curves->set_curves(curve_mapping_curves);
    curves->set_extrapolate((mapping.flag & blender::CUMA_EXTEND_EXTRAPOLATE) != 0);
    node = curves;
  }
  else if (b_node.is_type("ShaderNodeFloatCurve")) {
    const auto &mapping = *static_cast<blender::CurveMapping *>(b_node.storage);
    FloatCurveNode *curve = graph->create_node<FloatCurveNode>();
    array<float> curve_mapping_curve;
    float min_x;
    float max_x;
    curvemapping_float_to_array(mapping, curve_mapping_curve, RAMP_TABLE_SIZE);
    curvemapping_minmax(mapping, 1, &min_x, &max_x);
    curve->set_min_x(min_x);
    curve->set_max_x(max_x);
    curve->set_curve(curve_mapping_curve);
    curve->set_extrapolate((mapping.flag & blender::CUMA_EXTEND_EXTRAPOLATE) != 0);
    node = curve;
  }
  else if (b_node.is_type("ShaderNodeValToRGB")) {
    RGBRampNode *ramp = graph->create_node<RGBRampNode>();
    const auto &b_color_ramp = *static_cast<blender::ColorBand *>(b_node.storage);
    array<float3> ramp_values;
    array<float> ramp_alpha;
    colorramp_to_array(b_color_ramp, ramp_values, ramp_alpha, RAMP_TABLE_SIZE);
    ramp->set_ramp(ramp_values);
    ramp->set_ramp_alpha(ramp_alpha);
    ramp->set_interpolate(b_color_ramp.ipotype != blender::COLBAND_INTERP_CONSTANT);
    node = ramp;
  }
  else if (b_node.is_type("ShaderNodeRGB")) {
    ColorNode *color = graph->create_node<ColorNode>();
    color->set_value(get_node_output_rgba(b_node, "Color"));
    node = color;
  }
  else if (b_node.is_type("ShaderNodeValue")) {
    ValueNode *value = graph->create_node<ValueNode>();
    value->set_value(get_node_output_value(b_node, "Value"));
    node = value;
  }
  else if (b_node.is_type("ShaderNodeCameraData")) {
    node = graph->create_node<CameraNode>();
  }
  else if (b_node.is_type("ShaderNodeInvert")) {
    node = graph->create_node<InvertNode>();
  }
  else if (b_node.is_type("ShaderNodeGamma")) {
    node = graph->create_node<GammaNode>();
  }
  else if (b_node.is_type("ShaderNodeBrightContrast")) {
    node = graph->create_node<BrightContrastNode>();
  }
  else if (b_node.is_type("ShaderNodeMixRGB")) {
    MixNode *mix = graph->create_node<MixNode>();
    mix->set_mix_type((NodeMix)b_node.custom1);
    mix->set_use_clamp(b_node.custom2 & blender::SHD_MIXRGB_CLAMP);
    node = mix;
  }
  else if (b_node.is_type("ShaderNodeMix")) {
    const auto &storage = *static_cast<blender::NodeShaderMix *>(b_node.storage);
    if (storage.data_type == blender::SOCK_VECTOR) {
      if (storage.factor_mode == blender::NODE_MIX_MODE_UNIFORM) {
        MixVectorNode *mix_node = graph->create_node<MixVectorNode>();
        mix_node->set_use_clamp(storage.clamp_factor);
        node = mix_node;
      }
      else {
        MixVectorNonUniformNode *mix_node = graph->create_node<MixVectorNonUniformNode>();
        mix_node->set_use_clamp(storage.clamp_factor);
        node = mix_node;
      }
    }
    else if (storage.data_type == blender::SOCK_RGBA) {
      MixColorNode *mix_node = graph->create_node<MixColorNode>();
      mix_node->set_blend_type((NodeMix)storage.blend_type);
      mix_node->set_use_clamp(storage.clamp_factor);
      mix_node->set_use_clamp_result(storage.clamp_result);
      node = mix_node;
    }
    else {
      MixFloatNode *mix_node = graph->create_node<MixFloatNode>();
      mix_node->set_use_clamp(storage.clamp_factor);
      node = mix_node;
    }
  }
  else if (b_node.is_type("ShaderNodeSeparateColor")) {
    const auto &storage = *static_cast<blender::NodeCombSepColor *>(b_node.storage);
    SeparateColorNode *separate_node = graph->create_node<SeparateColorNode>();
    separate_node->set_color_type((NodeCombSepColorType)storage.mode);
    node = separate_node;
  }
  else if (b_node.is_type("ShaderNodeCombineColor")) {
    const auto &storage = *static_cast<blender::NodeCombSepColor *>(b_node.storage);
    CombineColorNode *combine_node = graph->create_node<CombineColorNode>();
    combine_node->set_color_type((NodeCombSepColorType)storage.mode);
    node = combine_node;
  }
  else if (b_node.is_type("ShaderNodeSeparateXYZ")) {
    node = graph->create_node<SeparateXYZNode>();
  }
  else if (b_node.is_type("ShaderNodeCombineXYZ")) {
    node = graph->create_node<CombineXYZNode>();
  }
  else if (b_node.is_type("ShaderNodeHueSaturation")) {
    node = graph->create_node<HSVNode>();
  }
  else if (b_node.is_type("ShaderNodeRGBToBW")) {
    node = graph->create_node<RGBToBWNode>();
  }
  else if (b_node.is_type("ShaderNodeMapRange")) {
    const auto &storage = *static_cast<blender::NodeMapRange *>(b_node.storage);
    if (storage.data_type == blender::CD_PROP_FLOAT3) {
      VectorMapRangeNode *vector_map_range_node = graph->create_node<VectorMapRangeNode>();
      vector_map_range_node->set_use_clamp(storage.clamp);
      vector_map_range_node->set_range_type((NodeMapRangeType)storage.interpolation_type);
      node = vector_map_range_node;
    }
    else {
      MapRangeNode *map_range_node = graph->create_node<MapRangeNode>();
      map_range_node->set_clamp(storage.clamp);
      map_range_node->set_range_type((NodeMapRangeType)storage.interpolation_type);
      node = map_range_node;
    }
  }
  else if (b_node.is_type("ShaderNodeClamp")) {
    ClampNode *clamp_node = graph->create_node<ClampNode>();
    clamp_node->set_clamp_type((NodeClampType)b_node.custom1);
    node = clamp_node;
  }
  else if (b_node.is_type("ShaderNodeMath")) {
    MathNode *math_node = graph->create_node<MathNode>();
    math_node->set_math_type((NodeMathType)b_node.custom1);
    math_node->set_use_clamp(b_node.custom2);
    node = math_node;
  }
  else if (b_node.is_type("ShaderNodeVectorMath")) {
    VectorMathNode *vector_math_node = graph->create_node<VectorMathNode>();
    vector_math_node->set_math_type((NodeVectorMathType)b_node.custom1);
    node = vector_math_node;
  }
  else if (b_node.is_type("ShaderNodeVectorRotate")) {
    VectorRotateNode *vector_rotate_node = graph->create_node<VectorRotateNode>();
    vector_rotate_node->set_rotate_type((NodeVectorRotateType)b_node.custom1);
    vector_rotate_node->set_invert(b_node.custom2);
    node = vector_rotate_node;
  }
  else if (b_node.is_type("ShaderNodeVectorTransform")) {
    const auto &storage = *static_cast<blender::NodeShaderVectTransform *>(b_node.storage);
    VectorTransformNode *vtransform = graph->create_node<VectorTransformNode>();
    vtransform->set_transform_type((NodeVectorTransformType)storage.type);
    vtransform->set_convert_from((NodeVectorTransformConvertSpace)storage.convert_from);
    vtransform->set_convert_to((NodeVectorTransformConvertSpace)storage.convert_to);
    node = vtransform;
  }
  else if (b_node.is_type("ShaderNodeNormal")) {
    NormalNode *norm = graph->create_node<NormalNode>();
    norm->set_direction(get_node_output_vector(b_node, "Normal"));
    node = norm;
  }
  else if (b_node.is_type("ShaderNodeMapping")) {
    MappingNode *mapping = graph->create_node<MappingNode>();
    mapping->set_mapping_type((NodeMappingType)b_node.custom1);
    node = mapping;
  }
  else if (b_node.is_type("ShaderNodeFresnel")) {
    node = graph->create_node<FresnelNode>();
  }
  else if (b_node.is_type("ShaderNodeLayerWeight")) {
    node = graph->create_node<LayerWeightNode>();
  }
  else if (b_node.is_type("ShaderNodeAddShader")) {
    node = graph->create_node<AddClosureNode>();
  }
  else if (b_node.is_type("ShaderNodeMixShader")) {
    node = graph->create_node<MixClosureNode>();
  }
  else if (b_node.is_type("ShaderNodeAttribute")) {
    const auto &storage = *static_cast<blender::NodeShaderAttribute *>(b_node.storage);
    AttributeNode *attr = graph->create_node<AttributeNode>();
    attr->set_attribute(blender_attribute_name_add_type(storage.name, storage.type));
    node = attr;
  }
  else if (b_node.is_type("ShaderNodeBackground")) {
    node = graph->create_node<BackgroundNode>();
  }
  else if (b_node.is_type("ShaderNodeHoldout")) {
    node = graph->create_node<HoldoutNode>();
  }
  else if (b_node.is_type("ShaderNodeBsdfDiffuse")) {
    node = graph->create_node<DiffuseBsdfNode>();
  }
  else if (b_node.is_type("ShaderNodeSubsurfaceScattering")) {
    SubsurfaceScatteringNode *subsurface = graph->create_node<SubsurfaceScatteringNode>();

    switch (b_node.custom1) {
      case blender::SHD_SUBSURFACE_BURLEY:
        subsurface->set_method(CLOSURE_BSSRDF_BURLEY_ID);
        break;
      case blender::SHD_SUBSURFACE_RANDOM_WALK:
        subsurface->set_method(CLOSURE_BSSRDF_RANDOM_WALK_ID);
        break;
      case blender::SHD_SUBSURFACE_RANDOM_WALK_SKIN:
        subsurface->set_method(CLOSURE_BSSRDF_RANDOM_WALK_SKIN_ID);
        break;
    }

    node = subsurface;
  }
  else if (b_node.is_type("ShaderNodeBsdfMetallic")) {
    MetallicBsdfNode *metal = graph->create_node<MetallicBsdfNode>();

    switch (b_node.custom1) {
      case blender::SHD_GLOSSY_BECKMANN:
        metal->set_distribution(CLOSURE_BSDF_MICROFACET_BECKMANN_ID);
        break;
      case blender::SHD_GLOSSY_GGX:
        metal->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_ID);
        break;
      case blender::SHD_GLOSSY_MULTI_GGX:
        metal->set_distribution(CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID);
        break;
    }

    switch (b_node.custom2) {
      case blender::SHD_PHYSICAL_CONDUCTOR:
        metal->set_fresnel_type(CLOSURE_BSDF_PHYSICAL_CONDUCTOR);
        break;
      case blender::SHD_CONDUCTOR_F82:
        metal->set_fresnel_type(CLOSURE_BSDF_F82_CONDUCTOR);
        break;
    }
    node = metal;
  }
  else if (b_node.is_type("ShaderNodeBsdfAnisotropic")) {
    GlossyBsdfNode *glossy = graph->create_node<GlossyBsdfNode>();

    switch (b_node.custom1) {
      case blender::SHD_GLOSSY_BECKMANN:
        glossy->set_distribution(CLOSURE_BSDF_MICROFACET_BECKMANN_ID);
        break;
      case blender::SHD_GLOSSY_GGX:
        glossy->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_ID);
        break;
      case blender::SHD_GLOSSY_ASHIKHMIN_SHIRLEY:
        glossy->set_distribution(CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID);
        break;
      case blender::SHD_GLOSSY_MULTI_GGX:
        glossy->set_distribution(CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID);
        break;
    }
    node = glossy;
  }
  else if (b_node.is_type("ShaderNodeBsdfGlass")) {
    GlassBsdfNode *glass = graph->create_node<GlassBsdfNode>();
    switch (b_node.custom1) {
      case blender::SHD_GLOSSY_BECKMANN:
        glass->set_distribution(CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID);
        break;
      case blender::SHD_GLOSSY_GGX:
        glass->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID);
        break;
      case blender::SHD_GLOSSY_MULTI_GGX:
        glass->set_distribution(CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID);
        break;
    }
    node = glass;
  }
  else if (b_node.is_type("ShaderNodeBsdfRefraction")) {
    RefractionBsdfNode *refraction = graph->create_node<RefractionBsdfNode>();
    switch (b_node.custom1) {
      case blender::SHD_GLOSSY_BECKMANN:
        refraction->set_distribution(CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID);
        break;
      case blender::SHD_GLOSSY_GGX:
        refraction->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
        break;
    }
    node = refraction;
  }
  else if (b_node.is_type("ShaderNodeBsdfToon")) {
    ToonBsdfNode *toon = graph->create_node<ToonBsdfNode>();
    switch (b_node.custom1) {
      case blender::SHD_TOON_DIFFUSE:
        toon->set_component(CLOSURE_BSDF_DIFFUSE_TOON_ID);
        break;
      case blender::SHD_TOON_GLOSSY:
        toon->set_component(CLOSURE_BSDF_GLOSSY_TOON_ID);
        break;
    }
    node = toon;
  }
  else if (b_node.is_type("ShaderNodeBsdfHair")) {
    HairBsdfNode *hair = graph->create_node<HairBsdfNode>();
    switch (b_node.custom1) {
      case blender::SHD_HAIR_REFLECTION:
        hair->set_component(CLOSURE_BSDF_HAIR_REFLECTION_ID);
        break;
      case blender::SHD_HAIR_TRANSMISSION:
        hair->set_component(CLOSURE_BSDF_HAIR_TRANSMISSION_ID);
        break;
    }
    node = hair;
  }
  else if (b_node.is_type("ShaderNodeBsdfHairPrincipled")) {
    const auto &storage = *static_cast<blender::NodeShaderHairPrincipled *>(b_node.storage);
    PrincipledHairBsdfNode *principled_hair = graph->create_node<PrincipledHairBsdfNode>();
    principled_hair->set_model((NodePrincipledHairModel)validate_enum_value(
        storage.model, NODE_PRINCIPLED_HAIR_MODEL_NUM, NODE_PRINCIPLED_HAIR_HUANG));
    principled_hair->set_parametrization((NodePrincipledHairParametrization)validate_enum_value(
        storage.parametrization,
        NODE_PRINCIPLED_HAIR_PARAMETRIZATION_NUM,
        NODE_PRINCIPLED_HAIR_REFLECTANCE));
    node = principled_hair;
  }
  else if (b_node.is_type("ShaderNodeBsdfPrincipled")) {
    PrincipledBsdfNode *principled = graph->create_node<PrincipledBsdfNode>();
    switch (b_node.custom1) {
      case blender::SHD_GLOSSY_GGX:
        principled->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID);
        break;
      case blender::SHD_GLOSSY_MULTI_GGX:
        principled->set_distribution(CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID);
        break;
    }
    switch (b_node.custom2) {
      case blender::SHD_SUBSURFACE_BURLEY:
        principled->set_subsurface_method(CLOSURE_BSSRDF_BURLEY_ID);
        break;
      case blender::SHD_SUBSURFACE_RANDOM_WALK:
        principled->set_subsurface_method(CLOSURE_BSSRDF_RANDOM_WALK_ID);
        break;
      case blender::SHD_SUBSURFACE_RANDOM_WALK_SKIN:
        principled->set_subsurface_method(CLOSURE_BSSRDF_RANDOM_WALK_SKIN_ID);
        break;
    }
    node = principled;
  }
  else if (b_node.is_type("ShaderNodeBsdfTranslucent")) {
    node = graph->create_node<TranslucentBsdfNode>();
  }
  else if (b_node.is_type("ShaderNodeBsdfTransparent")) {
    node = graph->create_node<TransparentBsdfNode>();
  }
  else if (b_node.is_type("ShaderNodeBsdfRayPortal")) {
    node = graph->create_node<RayPortalBsdfNode>();
  }
  else if (b_node.is_type("ShaderNodeBsdfSheen")) {
    SheenBsdfNode *sheen = graph->create_node<SheenBsdfNode>();
    switch (b_node.custom1) {
      case SHD_SHEEN_ASHIKHMIN:
        sheen->set_distribution(CLOSURE_BSDF_ASHIKHMIN_VELVET_ID);
        break;
      case SHD_SHEEN_MICROFIBER:
        sheen->set_distribution(CLOSURE_BSDF_SHEEN_ID);
        break;
    }
    node = sheen;
  }
  else if (b_node.is_type("ShaderNodeEmission")) {
    node = graph->create_node<EmissionNode>();
  }
  else if (b_node.is_type("ShaderNodeAmbientOcclusion")) {
    AmbientOcclusionNode *ao = graph->create_node<AmbientOcclusionNode>();
    ao->set_samples(b_node.custom1);
    ao->set_inside(b_node.custom2 & blender::SHD_AO_INSIDE);
    ao->set_only_local(b_node.custom2 & blender::SHD_AO_LOCAL);
    node = ao;
  }
  else if (b_node.is_type("ShaderNodeVolumeScatter")) {
    ScatterVolumeNode *scatter = graph->create_node<ScatterVolumeNode>();
    switch (b_node.custom1) {
      case blender::SHD_PHASE_HENYEY_GREENSTEIN:
        scatter->set_phase(CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID);
        break;
      case blender::SHD_PHASE_FOURNIER_FORAND:
        scatter->set_phase(CLOSURE_VOLUME_FOURNIER_FORAND_ID);
        break;
      case blender::SHD_PHASE_DRAINE:
        scatter->set_phase(CLOSURE_VOLUME_DRAINE_ID);
        break;
      case blender::SHD_PHASE_RAYLEIGH:
        scatter->set_phase(CLOSURE_VOLUME_RAYLEIGH_ID);
        break;
      case blender::SHD_PHASE_MIE:
        scatter->set_phase(CLOSURE_VOLUME_MIE_ID);
        break;
    }
    node = scatter;
  }
  else if (b_node.is_type("ShaderNodeVolumeAbsorption")) {
    node = graph->create_node<AbsorptionVolumeNode>();
  }
  else if (b_node.is_type("ShaderNodeVolumeCoefficients")) {
    VolumeCoefficientsNode *coeffs = graph->create_node<VolumeCoefficientsNode>();
    switch (b_node.custom1) {
      case blender::SHD_PHASE_HENYEY_GREENSTEIN:
        coeffs->set_phase(CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID);
        break;
      case blender::SHD_PHASE_FOURNIER_FORAND:
        coeffs->set_phase(CLOSURE_VOLUME_FOURNIER_FORAND_ID);
        break;
      case blender::SHD_PHASE_DRAINE:
        coeffs->set_phase(CLOSURE_VOLUME_DRAINE_ID);
        break;
      case blender::SHD_PHASE_RAYLEIGH:
        coeffs->set_phase(CLOSURE_VOLUME_RAYLEIGH_ID);
        break;
      case blender::SHD_PHASE_MIE:
        coeffs->set_phase(CLOSURE_VOLUME_MIE_ID);
        break;
    }
    node = coeffs;
  }
  else if (b_node.is_type("ShaderNodeVolumePrincipled")) {
    PrincipledVolumeNode *principled = graph->create_node<PrincipledVolumeNode>();
    node = principled;
  }
  else if (b_node.is_type("ShaderNodeNewGeometry")) {
    node = graph->create_node<GeometryNode>();
  }
  else if (b_node.is_type("ShaderNodeWireframe")) {
    WireframeNode *wire = graph->create_node<WireframeNode>();
    wire->set_use_pixel_size(b_node.custom1);
    node = wire;
  }
  else if (b_node.is_type("ShaderNodeWavelength")) {
    node = graph->create_node<WavelengthNode>();
  }
  else if (b_node.is_type("ShaderNodeBlackbody")) {
    node = graph->create_node<BlackbodyNode>();
  }
  else if (b_node.is_type("ShaderNodeLightPath")) {
    node = graph->create_node<LightPathNode>();
  }
  else if (b_node.is_type("ShaderNodeLightFalloff")) {
    node = graph->create_node<LightFalloffNode>();
  }
  else if (b_node.is_type("ShaderNodeObjectInfo")) {
    node = graph->create_node<ObjectInfoNode>();
  }
  else if (b_node.is_type("ShaderNodeParticleInfo")) {
    node = graph->create_node<ParticleInfoNode>();
  }
  else if (b_node.is_type("ShaderNodeHairInfo")) {
    node = graph->create_node<HairInfoNode>();
  }
  else if (b_node.is_type("ShaderNodePointInfo")) {
    node = graph->create_node<PointInfoNode>();
  }
  else if (b_node.is_type("ShaderNodeVolumeInfo")) {
    node = graph->create_node<VolumeInfoNode>();
  }
  else if (b_node.is_type("ShaderNodeVertexColor")) {
    const auto &storage = *static_cast<blender::NodeShaderVertexColor *>(b_node.storage);
    VertexColorNode *vertex_color_node = graph->create_node<VertexColorNode>();
    vertex_color_node->set_layer_name(ustring(storage.layer_name));
    node = vertex_color_node;
  }
  else if (b_node.is_type("ShaderNodeBump")) {
    BumpNode *bump = graph->create_node<BumpNode>();
    bump->set_invert(b_node.custom1);
    node = bump;
  }
  else if (b_node.is_type("ShaderNodeScript")) {
#ifdef WITH_OSL
    const auto &storage = *static_cast<blender::NodeShaderScript *>(b_node.storage);
    if (scene->shader_manager->use_osl()) {
      const string bytecode_hash = storage.bytecode_hash;
      if (!bytecode_hash.empty()) {
        node = OSLShaderManager::osl_node(graph, scene, "", bytecode_hash, storage.bytecode);
      }
      else {
        const string absolute_filepath = blender_absolute_path(
            b_data, b_ntree.id, storage.filepath);
        node = OSLShaderManager::osl_node(graph, scene, absolute_filepath, "");
      }
    }
#else
    (void)b_data;
    (void)b_ntree;
#endif
  }
  else if (b_node.is_type("ShaderNodeTexImage")) {
    const auto &storage = *static_cast<blender::NodeTexImage *>(b_node.storage);
    blender::Image *b_image = blender::id_cast<blender::Image *>(b_node.id);
    blender::ImageUser &b_image_user = const_cast<blender::ImageUser &>(storage.iuser);
    ImageTextureNode *image = graph->create_node<ImageTextureNode>();

    image->set_interpolation(get_image_interpolation(storage));
    image->set_extension(get_image_extension(storage));
    image->set_projection((NodeImageProjection)storage.projection);
    image->set_projection_blend(storage.projection_blend);
    const blender::TexMapping &b_texture_mapping = storage.base.tex_mapping;
    get_tex_mapping(image, &b_texture_mapping);

    if (b_image) {
      const blender::eImageSource b_image_source = blender::eImageSource(b_image->source);
      blender::PointerRNA image_rna_ptr = RNA_id_pointer_create(&b_image->id);
      blender::PointerRNA colorspace_ptr = RNA_pointer_get(&image_rna_ptr, "colorspace_settings");
      image->set_colorspace(ustring(get_enum_identifier(colorspace_ptr, "name")));

      image->set_animated(is_image_animated(b_image_source, b_image_user));
      image->set_alpha_type(get_image_alpha_type(*b_image));

      if (b_image_source == blender::IMA_SRC_TILED) {
        array<int> tiles;
        for (blender::ImageTile &b_tile : b_image->tiles) {
          tiles.push_back_slow(b_tile.tile_number);
        }
        image->set_tiles(tiles);
      }

      /* builtin images will use callback-based reading because
       * they could only be loaded correct from blender side
       */
      const bool is_builtin = image_is_builtin(*b_image, b_engine);

      if (is_builtin) {
        /* for builtin images we're using image datablock name to find an image to
         * read pixels from later
         *
         * also store frame number as well, so there's no differences in handling
         * builtin names for packed images and movies
         */
        const int scene_frame = b_scene.r.cfra;
        const int image_frame = image_user_frame_number(b_image_user, *b_image, scene_frame);
        if (b_image_source != blender::IMA_SRC_TILED) {
          image->handle = scene->image_manager->add_image(
              make_unique<BlenderImageLoader>(b_image,
                                              &b_image_user,
                                              image_frame,
                                              0,
                                              (b_engine.flag & blender::RE_ENGINE_PREVIEW) != 0),
              image->image_params());
        }
        else {
          vector<unique_ptr<ImageLoader>> loaders;
          loaders.reserve(image->get_tiles().size());
          for (const int tile_number : image->get_tiles()) {
            loaders.push_back(make_unique<BlenderImageLoader>(
                b_image,
                &b_image_user,
                image_frame,
                tile_number,
                (b_engine.flag & blender::RE_ENGINE_PREVIEW) != 0));
          }

          image->handle = scene->image_manager->add_image(std::move(loaders),
                                                          image->image_params());
        }
      }
      else {
        const ustring filename = ustring(
            image_user_file_path(b_data, b_image_user, *b_image, b_scene.r.cfra));
        image->set_filename(filename);
      }
    }
    node = image;
  }
  else if (b_node.is_type("ShaderNodeTexEnvironment")) {
    const auto &storage = *static_cast<blender::NodeTexEnvironment *>(b_node.storage);
    blender::Image *b_image = blender::id_cast<blender::Image *>(b_node.id);
    blender::ImageUser &b_image_user = const_cast<blender::ImageUser &>(storage.iuser);
    EnvironmentTextureNode *env = graph->create_node<EnvironmentTextureNode>();

    env->set_interpolation(get_image_interpolation(storage));
    env->set_projection((NodeEnvironmentProjection)storage.projection);
    const blender::TexMapping &b_texture_mapping = storage.base.tex_mapping;
    get_tex_mapping(env, &b_texture_mapping);

    if (b_image) {
      const blender::eImageSource b_image_source = blender::eImageSource(b_image->source);
      blender::PointerRNA image_rna_ptr = RNA_id_pointer_create(&b_image->id);
      blender::PointerRNA colorspace_ptr = RNA_pointer_get(&image_rna_ptr, "colorspace_settings");
      env->set_colorspace(ustring(get_enum_identifier(colorspace_ptr, "name")));
      env->set_animated(is_image_animated(b_image_source, b_image_user));
      env->set_alpha_type(get_image_alpha_type(*b_image));

      const bool is_builtin = image_is_builtin(*b_image, b_engine);

      if (is_builtin) {
        const int scene_frame = b_scene.r.cfra;
        const int image_frame = image_user_frame_number(b_image_user, *b_image, scene_frame);
        env->handle = scene->image_manager->add_image(
            make_unique<BlenderImageLoader>(b_image,
                                            &b_image_user,
                                            image_frame,
                                            0,
                                            (b_engine.flag & blender::RE_ENGINE_PREVIEW) != 0),
            env->image_params());
      }
      else {
        env->set_filename(
            ustring(image_user_file_path(b_data, b_image_user, *b_image, b_scene.r.cfra)));
      }
    }
    node = env;
  }
  else if (b_node.is_type("ShaderNodeTexGradient")) {
    const auto &storage = *static_cast<blender::NodeTexGradient *>(b_node.storage);
    GradientTextureNode *gradient = graph->create_node<GradientTextureNode>();
    gradient->set_gradient_type((NodeGradientType)storage.gradient_type);
    const blender::TexMapping &b_texture_mapping = storage.base.tex_mapping;
    get_tex_mapping(gradient, &b_texture_mapping);
    node = gradient;
  }
  else if (b_node.is_type("ShaderNodeTexVoronoi")) {
    const auto &storage = *static_cast<blender::NodeTexVoronoi *>(b_node.storage);
    VoronoiTextureNode *voronoi = graph->create_node<VoronoiTextureNode>();
    voronoi->set_dimensions(storage.dimensions);
    voronoi->set_feature((NodeVoronoiFeature)storage.feature);
    voronoi->set_metric((NodeVoronoiDistanceMetric)storage.distance);
    voronoi->set_use_normalize(storage.normalize);
    const blender::TexMapping &b_texture_mapping = storage.base.tex_mapping;
    get_tex_mapping(voronoi, &b_texture_mapping);
    node = voronoi;
  }
  else if (b_node.is_type("ShaderNodeTexMagic")) {
    const auto &storage = *static_cast<blender::NodeTexMagic *>(b_node.storage);
    MagicTextureNode *magic = graph->create_node<MagicTextureNode>();
    magic->set_depth(storage.depth);
    const blender::TexMapping &b_texture_mapping = storage.base.tex_mapping;
    get_tex_mapping(magic, &b_texture_mapping);
    node = magic;
  }
  else if (b_node.is_type("ShaderNodeTexWave")) {
    const auto &storage = *static_cast<blender::NodeTexWave *>(b_node.storage);
    WaveTextureNode *wave = graph->create_node<WaveTextureNode>();
    wave->set_wave_type((NodeWaveType)storage.wave_type);
    wave->set_bands_direction((NodeWaveBandsDirection)storage.bands_direction);
    wave->set_rings_direction((NodeWaveRingsDirection)storage.rings_direction);
    wave->set_profile((NodeWaveProfile)storage.wave_profile);
    const blender::TexMapping &b_texture_mapping = storage.base.tex_mapping;
    get_tex_mapping(wave, &b_texture_mapping);
    node = wave;
  }
  else if (b_node.is_type("ShaderNodeTexChecker")) {
    const auto &storage = *static_cast<blender::NodeTexChecker *>(b_node.storage);
    CheckerTextureNode *checker = graph->create_node<CheckerTextureNode>();
    const blender::TexMapping &b_texture_mapping = storage.base.tex_mapping;
    get_tex_mapping(checker, &b_texture_mapping);
    node = checker;
  }
  else if (b_node.is_type("ShaderNodeTexBrick")) {
    const auto &storage = *static_cast<blender::NodeTexBrick *>(b_node.storage);
    BrickTextureNode *brick = graph->create_node<BrickTextureNode>();
    brick->set_offset(storage.offset);
    brick->set_offset_frequency(storage.offset_freq);
    brick->set_squash(storage.squash);
    brick->set_squash_frequency(storage.squash_freq);
    const blender::TexMapping &b_texture_mapping = storage.base.tex_mapping;
    get_tex_mapping(brick, &b_texture_mapping);
    node = brick;
  }
  else if (b_node.is_type("ShaderNodeTexNoise")) {
    const auto &storage = *static_cast<blender::NodeTexNoise *>(b_node.storage);
    NoiseTextureNode *noise = graph->create_node<NoiseTextureNode>();
    noise->set_dimensions(storage.dimensions);
    noise->set_type((NodeNoiseType)storage.type);
    noise->set_use_normalize(storage.normalize);
    const blender::TexMapping &b_texture_mapping = storage.base.tex_mapping;
    get_tex_mapping(noise, &b_texture_mapping);
    node = noise;
  }
  else if (b_node.is_type("ShaderNodeTexGabor")) {
    const auto &storage = *static_cast<blender::NodeTexGabor *>(b_node.storage);
    GaborTextureNode *gabor = graph->create_node<GaborTextureNode>();
    gabor->set_type((NodeGaborType)storage.type);
    const blender::TexMapping &b_texture_mapping = storage.base.tex_mapping;
    get_tex_mapping(gabor, &b_texture_mapping);
    node = gabor;
  }
  else if (b_node.is_type("ShaderNodeTexCoord")) {
    TextureCoordinateNode *tex_coord = graph->create_node<TextureCoordinateNode>();
    tex_coord->set_from_dupli(b_node.custom1);
    if (const blender::ID *b_object = b_node.id) {
      tex_coord->set_use_transform(true);
      tex_coord->set_ob_tfm(
          get_transform(blender::id_cast<const blender::Object *>(b_object)->object_to_world()));
    }
    node = tex_coord;
  }
  else if (b_node.is_type("ShaderNodeTexSky")) {
    const auto &storage = *static_cast<blender::NodeTexSky *>(b_node.storage);
    SkyTextureNode *sky = graph->create_node<SkyTextureNode>();
    sky->set_sky_type((NodeSkyType)storage.sky_model);
    sky->set_sun_direction(normalize(make_float3(
        storage.sun_direction[0], storage.sun_direction[1], storage.sun_direction[2])));
    sky->set_turbidity(storage.turbidity);
    sky->set_ground_albedo(storage.ground_albedo);
    sky->set_sun_disc(storage.sun_disc);
    sky->set_sun_size(storage.sun_size);
    sky->set_sun_intensity(storage.sun_intensity);
    sky->set_sun_elevation(storage.sun_elevation);
    sky->set_sun_rotation(storage.sun_rotation);
    sky->set_altitude(storage.altitude);
    sky->set_air_density(storage.air_density);
    sky->set_aerosol_density(storage.aerosol_density);
    sky->set_ozone_density(storage.ozone_density);
    const blender::TexMapping &b_texture_mapping = storage.base.tex_mapping;
    get_tex_mapping(sky, &b_texture_mapping);
    node = sky;
  }
  else if (b_node.is_type("ShaderNodeTexIES")) {
    const auto &storage = *static_cast<blender::NodeShaderTexIES *>(b_node.storage);
    IESLightNode *ies = graph->create_node<IESLightNode>();
    switch (storage.mode) {
      case blender::NODE_IES_EXTERNAL:
        ies->set_filename(ustring(blender_absolute_path(b_data, b_ntree.id, storage.filepath)));
        break;
      case blender::NODE_IES_INTERNAL:
        ustring ies_content = ustring(get_text_datablock_content(b_node.id));
        if (ies_content.empty()) {
          ies_content = "\n";
        }
        ies->set_ies(ies_content);
        break;
    }
    node = ies;
  }
  else if (b_node.is_type("ShaderNodeTexWhiteNoise")) {
    WhiteNoiseTextureNode *white_noise_node = graph->create_node<WhiteNoiseTextureNode>();
    white_noise_node->set_dimensions(b_node.custom1);
    node = white_noise_node;
  }
  else if (b_node.is_type("ShaderNodeNormalMap")) {
    const auto &storage = *static_cast<blender::NodeShaderNormalMap *>(b_node.storage);
    NormalMapNode *nmap = graph->create_node<NormalMapNode>();
    nmap->set_space((NodeNormalMapSpace)storage.space);
    nmap->set_attribute(ustring(storage.uv_map));
    nmap->set_convention((NodeNormalMapConvention)storage.convention);
    node = nmap;
  }
  else if (b_node.is_type("ShaderNodeRadialTiling")) {
    const auto &storage = *static_cast<blender::NodeRadialTiling *>(b_node.storage);
    RadialTilingNode *radial_tiling = graph->create_node<RadialTilingNode>();
    radial_tiling->set_use_normalize(storage.normalize);
    node = radial_tiling;
  }
  else if (b_node.is_type("ShaderNodeTangent")) {
    const auto &storage = *static_cast<blender::NodeShaderTangent *>(b_node.storage);
    TangentNode *tangent = graph->create_node<TangentNode>();
    tangent->set_direction_type((NodeTangentDirectionType)storage.direction_type);
    tangent->set_axis((NodeTangentAxis)storage.axis);
    tangent->set_attribute(ustring(storage.uv_map));
    node = tangent;
  }
  else if (b_node.is_type("ShaderNodeUVMap")) {
    const auto &storage = *static_cast<blender::NodeShaderUVMap *>(b_node.storage);
    UVMapNode *uvm = graph->create_node<UVMapNode>();
    uvm->set_attribute(ustring(storage.uv_map));
    uvm->set_from_dupli(b_node.custom1);
    node = uvm;
  }
  else if (b_node.is_type("ShaderNodeBevel")) {
    BevelNode *bevel = graph->create_node<BevelNode>();
    bevel->set_samples(b_node.custom1);
    node = bevel;
  }
  else if (b_node.is_type("ShaderNodeDisplacement")) {
    DisplacementNode *disp = graph->create_node<DisplacementNode>();
    disp->set_space((NodeNormalMapSpace)b_node.custom1);
    node = disp;
  }
  else if (b_node.is_type("ShaderNodeVectorDisplacement")) {
    VectorDisplacementNode *disp = graph->create_node<VectorDisplacementNode>();
    disp->set_space((NodeNormalMapSpace)b_node.custom1);
    disp->set_attribute(ustring(""));
    node = disp;
  }
  else if (b_node.is_type("ShaderNodeOutputAOV")) {
    const auto &storage = *static_cast<blender::NodeShaderOutputAOV *>(b_node.storage);
    OutputAOVNode *aov = graph->create_node<OutputAOVNode>();
    aov->set_name(ustring(storage.name));
    node = aov;
  }
  else if (b_node.is_type("ShaderNodeRaycast")) {
    RaycastNode *raycast = graph->create_node<RaycastNode>();
    raycast->set_only_local(b_node.custom1);
    node = raycast;
  }

  if (node) {
    node->name = b_node.name;
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

static ShaderInput *node_find_input_by_name(const blender::bNode &b_node,
                                            ShaderNode *node,
                                            blender::bNodeSocket &b_socket)
{
  string name = b_socket.identifier;
  ShaderInput *input = node->input(name.c_str());

  if (!input && node_use_modified_socket_name(node)) {
    /* Different internal name for shader. */
    if (string_startswith(name, "Shader")) {
      string_replace(name, "Shader", "Closure");
    }

    /* Map mix node internal name for shader. */
    if (b_node.is_type("ShaderNodeMix")) {
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

static ShaderOutput *node_find_output_by_name(blender::bNode &b_node,
                                              ShaderNode *node,
                                              blender::bNodeSocket &b_socket)
{
  string name = b_socket.identifier;
  ShaderOutput *output = node->output(name.c_str());

  if (!output && node_use_modified_socket_name(node)) {
    /* Different internal name for shader. */
    if (name == "Shader") {
      name = "Closure";
      output = node->output(name.c_str());
    }
    /* Map internal name for shader. */
    if (b_node.is_type("ShaderNodeMix")) {
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
                      blender::RenderEngine &b_engine,
                      blender::Main &b_data,
                      blender::Scene &b_scene,
                      ShaderGraph *graph,
                      blender::bNodeTree &b_ntree,
                      const ProxyMap &proxy_input_map,
                      const ProxyMap &proxy_output_map);

static void add_nodes_inlined(Scene *scene,
                              blender::RenderEngine &b_engine,
                              blender::Main &b_data,
                              blender::Scene &b_scene,
                              ShaderGraph *graph,
                              blender::bNodeTree &b_ntree,
                              const ProxyMap &proxy_input_map,
                              const ProxyMap &proxy_output_map)
{
  /* add nodes */
  PtrInputMap input_map;
  PtrOutputMap output_map;

  /* find the node to use for output if there are multiple */
  const blender::bNode *output_node = ntreeShaderOutputNode(&b_ntree, blender::SHD_OUTPUT_CYCLES);

  /* add nodes */
  for (blender::bNode *b_node : b_ntree.all_nodes()) {
    if (b_node->is_muted() || b_node->is_reroute()) {
      /* replace muted node with internal links */
      for (blender::bNodeLink &b_link : b_node->runtime->internal_links) {
        blender::bNodeSocket *to_socket = b_link.tosock;
        const SocketType::Type to_socket_type = convert_socket_type(*to_socket);
        if (to_socket_type == SocketType::UNDEFINED) {
          continue;
        }

        ConvertNode *proxy = graph->create_node<ConvertNode>(to_socket_type, to_socket_type, true);

        /* Muted nodes can result in multiple Cycles input sockets mapping to the same Blender
         * input socket, so this needs to be a multimap. */
        input_map.emplace(b_link.fromsock, proxy->inputs[0]);
        output_map[b_link.tosock] = proxy->outputs[0];
      }
    }
    else if (b_node->is_group()) {
      blender::bNodeTree *b_group_ntree = blender::id_cast<blender::bNodeTree *>(b_node->id);

      ProxyMap group_proxy_input_map;
      ProxyMap group_proxy_output_map;

      /* Add a proxy node for each socket
       * Do this even if the node group has no internal tree,
       * so that links have something to connect to and assert won't fail.
       */
      for (blender::bNodeSocket *b_input : b_node->input_sockets()) {
        const SocketType::Type input_type = convert_socket_type(*b_input);
        if (input_type == SocketType::UNDEFINED) {
          continue;
        }

        ConvertNode *proxy = graph->create_node<ConvertNode>(input_type, input_type, true);

        /* register the proxy node for internal binding */
        group_proxy_input_map[b_input->identifier] = proxy;

        input_map.emplace(b_input, proxy->inputs[0]);

        set_default_value(proxy->inputs[0], *b_input, b_data, b_ntree.id);
      }
      for (blender::bNodeSocket *b_output : b_node->output_sockets()) {
        const SocketType::Type output_type = convert_socket_type(*b_output);
        if (output_type == SocketType::UNDEFINED) {
          continue;
        }

        ConvertNode *proxy = graph->create_node<ConvertNode>(output_type, output_type, true);

        /* register the proxy node for internal binding */
        group_proxy_output_map[b_output->identifier] = proxy;

        output_map[b_output] = proxy->outputs[0];
      }

      if (b_group_ntree) {
        add_nodes(scene,
                  b_engine,
                  b_data,
                  b_scene,
                  graph,
                  *b_group_ntree,
                  group_proxy_input_map,
                  group_proxy_output_map);
      }
    }
    else if (b_node->is_type("NodeGroupInput")) {
      /* map each socket to a proxy node */
      for (blender::bNodeSocket *b_output : b_node->output_sockets()) {
        const ProxyMap::const_iterator proxy_it = proxy_input_map.find(b_output->identifier);
        if (proxy_it != proxy_input_map.end()) {
          ConvertNode *proxy = proxy_it->second;

          output_map[b_output] = proxy->outputs[0];
        }
      }
    }
    else if (b_node->is_type("NodeGroupOutput")) {
      /* only the active group output is used */
      if (b_node->flag & blender::NODE_DO_OUTPUT) {
        /* map each socket to a proxy node */
        for (blender::bNodeSocket *b_input : b_node->input_sockets()) {
          const ProxyMap::const_iterator proxy_it = proxy_output_map.find(b_input->identifier);
          if (proxy_it != proxy_output_map.end()) {
            ConvertNode *proxy = proxy_it->second;

            input_map.emplace(b_input, proxy->inputs[0]);

            set_default_value(proxy->inputs[0], *b_input, b_data, b_ntree.id);
          }
        }
      }
    }
    /* TODO: All the previous cases can be removed? */
    else {
      ShaderNode *node = nullptr;

      if (b_node == output_node) {
        node = graph->output();
      }
      else {
        node = add_node(scene, b_engine, b_data, b_scene, graph, b_ntree, *b_node);
      }

      if (node) {
        /* map node sockets for linking */
        for (blender::bNodeSocket *b_input : b_node->input_sockets()) {
          if (!b_input->is_available()) {
            /* Skip unavailable sockets. */
            continue;
          }
          ShaderInput *input = node_find_input_by_name(*b_node, node, *b_input);
          if (!input) {
            /* XXX should not happen, report error? */
            continue;
          }
          input_map.emplace(b_input, input);

          set_default_value(input, *b_input, b_data, b_ntree.id);
        }
        for (blender::bNodeSocket *b_output : b_node->output_sockets()) {
          if (!b_output->is_available()) {
            /* Skip unavailable sockets. */
            continue;
          }
          ShaderOutput *output = node_find_output_by_name(*b_node, node, *b_output);
          if (!output) {
            /* XXX should not happen, report error? */
            continue;
          }
          output_map[b_output] = output;
        }
      }
    }
  }

  /* connect nodes */
  for (blender::bNodeLink *b_link : b_ntree.all_links()) {
    /* Ignore invalid links to avoid unwanted cycles created in graph.
     * Also ignore links with unavailable sockets. */
    if (!((b_link->flag & blender::NODE_LINK_VALID) != 0 && b_link->fromsock->is_available() &&
          b_link->tosock->is_available()) ||
        b_link->is_muted())
    {
      continue;
    }
    /* get blender link data */
    blender::bNodeSocket *b_from_sock = b_link->fromsock;
    blender::bNodeSocket *b_to_sock = b_link->tosock;

    ShaderOutput *output = nullptr;
    const PtrOutputMap::iterator output_it = output_map.find(b_from_sock);
    if (output_it != output_map.end()) {
      output = output_it->second;
    }

    /* either socket may be nullptr when the node was not exported, typically
     * because the node type is not supported */
    if (output != nullptr) {
      ShaderOutput *output = output_it->second;
      auto inputs = input_map.equal_range(b_to_sock);
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
                      blender::RenderEngine &b_engine,
                      blender::Main &b_data,
                      blender::Scene &b_scene,
                      ShaderGraph *graph,
                      blender::bNodeTree &b_ntree,
                      const ProxyMap &proxy_input_map,
                      const ProxyMap &proxy_output_map)
{
  blender::bNodeTree *localtree = blender::bke::node_tree_add_tree(
      nullptr, (blender::StringRef(b_ntree.id.name) + " Inlined").c_str(), b_ntree.idname);
  blender::nodes::InlineShaderNodeTreeParams inline_params;
  inline_params.allow_preserving_repeat_zones = false;
  inline_params.target_engine_ = blender::SHD_OUTPUT_CYCLES;
  blender::nodes::inline_shader_node_tree(b_ntree, *localtree, inline_params);

  add_nodes_inlined(
      scene, b_engine, b_data, b_scene, graph, *localtree, proxy_input_map, proxy_output_map);

  BKE_id_free(nullptr, &localtree->id);
}

static void add_nodes(Scene *scene,
                      blender::RenderEngine &b_engine,
                      blender::Main &b_data,
                      blender::Scene &b_scene,
                      ShaderGraph *graph,
                      blender::bNodeTree &b_ntree)
{
  static const ProxyMap empty_proxy_map;
  add_nodes(scene, b_engine, b_data, b_scene, graph, b_ntree, empty_proxy_map, empty_proxy_map);
}

/* Look up and constant fold all references to View Layer attributes. */
void BlenderSync::resolve_view_layer_attributes(Shader *shader,
                                                ShaderGraph *graph,
                                                blender::Depsgraph &b_depsgraph)
{
  bool updated = false;

  for (ShaderNode *node : graph->nodes) {
    if (node->is_a(AttributeNode::get_node_type())) {
      AttributeNode *attr_node = static_cast<AttributeNode *>(node);

      std::string real_name;
      const int type = blender_attribute_name_split_type(attr_node->get_attribute(), &real_name);

      if (type == blender::SHD_ATTRIBUTE_VIEW_LAYER) {
        /* Look up the value. */
        const blender::ViewLayer *b_layer = DEG_get_evaluated_view_layer(&b_depsgraph);
        const blender::Scene *b_scene = DEG_get_evaluated_scene(&b_depsgraph);
        float4 value;

        BKE_view_layer_find_rgba_attribute(b_scene, b_layer, real_name.c_str(), &value.x);

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

bool BlenderSync::scene_attr_needs_recalc(Shader *shader, blender::Depsgraph &b_depsgraph)
{
  if (shader && shader_map.test_flag(shader, SHADER_WITH_LAYER_ATTRS)) {
    blender::Scene *scene = DEG_get_evaluated_scene(&b_depsgraph);

    return shader_map.check_recalc(&scene->id) ||
           shader_map.check_recalc(reinterpret_cast<blender::ID *>(scene->world)) ||
           shader_map.check_recalc(reinterpret_cast<blender::ID *>(scene->camera));
  }

  return false;
}

/* Sync Materials */

void BlenderSync::sync_materials(blender::Depsgraph &b_depsgraph, bool update_all)
{
  shader_map.set_default(scene->default_surface);

  TaskPool pool;
  set<Shader *> updated_shaders;

  blender::DEGIDIterData data{};
  data.graph = &b_depsgraph;
  ITER_BEGIN (blender::DEG_iterator_ids_begin,
              blender::DEG_iterator_ids_next,
              blender::DEG_iterator_ids_end,
              &data,
              blender::ID *,
              b_id)
  {
    if (GS(b_id->name) != blender::ID_MA) {
      continue;
    }

    blender::Material &b_mat = blender::id_cast<blender::Material &>(*b_id);
    Shader *shader;

    /* test if we need to sync */
    if (shader_map.add_or_update(&shader, &b_mat.id) || update_all ||
        scene_attr_needs_recalc(shader, b_depsgraph))
    {
      unique_ptr<ShaderGraph> graph = make_unique<ShaderGraph>();

      shader->name = BKE_id_name(b_mat.id);
      shader->set_pass_id(b_mat.index);

      /* create nodes */
      if (b_mat.nodetree) {
        add_nodes(scene, *b_engine, *b_data, *b_scene, graph.get(), *b_mat.nodetree);
      }
      else {
        DiffuseBsdfNode *diffuse = graph->create_node<DiffuseBsdfNode>();
        diffuse->set_color(make_float3(b_mat.r, b_mat.g, b_mat.b));

        ShaderNode *out = graph->output();
        graph->connect(diffuse->output("BSDF"), out->input("Surface"));
      }

      resolve_view_layer_attributes(shader, graph.get(), b_depsgraph);

      /* settings */
      blender::PointerRNA mat_rna_ptr = RNA_id_pointer_create(&b_mat.id);
      blender::PointerRNA cmat = RNA_pointer_get(&mat_rna_ptr, "cycles");
      shader->set_emission_sampling_method(get_emission_sampling(cmat));
      shader->set_use_transparent_shadow(b_mat.blend_flag & blender::MA_BL_TRANSPARENT_SHADOW);
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
  ITER_END;

  pool.wait_work();

  for (Shader *shader : updated_shaders) {
    shader->tag_update(scene);
  }
}

/* Sync World */

void BlenderSync::sync_world(blender::Depsgraph &b_depsgraph,
                             blender::bScreen *b_screen,
                             blender::View3D *b_v3d,
                             bool update_all)
{
  Background *background = scene->background;
  Integrator *integrator = scene->integrator;
  blender::PointerRNA scene_rna_ptr = RNA_id_pointer_create(&b_scene->id);
  blender::PointerRNA cscene = RNA_pointer_get(&scene_rna_ptr, "cycles");

  blender::World *b_world = view_layer.world_override ? view_layer.world_override : b_scene->world;

  const BlenderViewportParameters new_viewport_parameters(b_screen, b_v3d, use_developer_ui);

  Shader *shader = scene->default_background;

  if (world_recalc || update_all || b_world != world_map ||
      viewport_parameters.shader_modified(new_viewport_parameters) ||
      scene_attr_needs_recalc(shader, b_depsgraph))
  {
    unique_ptr<ShaderGraph> graph = make_unique<ShaderGraph>();

    /* create nodes */
    if (new_viewport_parameters.use_scene_world && b_world && b_world->nodetree) {
      add_nodes(scene, *b_engine, *b_data, *b_scene, graph.get(), *b_world->nodetree);

      /* volume */
      blender::PointerRNA world_rna_ptr = RNA_id_pointer_create(&b_world->id);
      blender::PointerRNA cworld = RNA_pointer_get(&world_rna_ptr, "cycles");
      shader->set_volume_sampling_method(get_volume_sampling(cworld));
      shader->set_volume_interpolation_method(get_volume_interpolation(cworld));
      shader->set_volume_step_rate(get_float(cworld, "volume_step_size"));
    }
    else if (new_viewport_parameters.use_scene_world && b_world) {
      BackgroundNode *background = graph->create_node<BackgroundNode>();
      background->set_color(make_float3(b_world->horr, b_world->horg, b_world->horb));

      ShaderNode *out = graph->output();
      graph->connect(background->output("Background"), out->input("Surface"));
    }
    else if (!new_viewport_parameters.use_scene_world) {
      float3 world_color;
      if (b_world) {
        world_color = make_float3(b_world->horr, b_world->horg, b_world->horb);
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
      blender::PointerRNA world_rna_ptr = RNA_id_pointer_create(&b_world->id);
      blender::PointerRNA cvisibility = RNA_pointer_get(&world_rna_ptr, "cycles_visibility");
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
    enum { FAST_GI_METHOD_REPLACE = 0, FAST_GI_METHOD_ADD = 1, FAST_GI_METHOD_NUM };

    const bool use_fast_gi = get_boolean(cscene, "use_fast_gi");
    if (use_fast_gi) {
      const int fast_gi_method = get_enum(
          cscene, "fast_gi_method", FAST_GI_METHOD_NUM, FAST_GI_METHOD_REPLACE);
      integrator->set_ao_factor((fast_gi_method == FAST_GI_METHOD_REPLACE) ? b_world->aoenergy :
                                                                             0.0f);
      integrator->set_ao_additive_factor(
          (fast_gi_method == FAST_GI_METHOD_ADD) ? b_world->aoenergy : 0.0f);
    }
    else {
      integrator->set_ao_factor(0.0f);
      integrator->set_ao_additive_factor(0.0f);
    }

    integrator->set_ao_distance(b_world->aodist);
  }
  else {
    integrator->set_ao_factor(0.0f);
    integrator->set_ao_additive_factor(0.0f);
    integrator->set_ao_distance(10.0f);
  }

  background->set_transparent((b_scene->r.alphamode & blender::R_ALPHAPREMUL) != 0);

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

  background->set_lightgroup(
      ustring((b_world && b_world->lightgroup) ? b_world->lightgroup->name : ""));

  background->tag_update(scene);
}

/* Sync Lights */

void BlenderSync::sync_lights(blender::Depsgraph &b_depsgraph, bool update_all)
{
  shader_map.set_default(scene->default_light);

  blender::DEGIDIterData data{};
  data.graph = &b_depsgraph;
  ITER_BEGIN (blender::DEG_iterator_ids_begin,
              blender::DEG_iterator_ids_next,
              blender::DEG_iterator_ids_end,
              &data,
              blender::ID *,
              b_id)
  {
    if (GS(b_id->name) != blender::ID_LA) {
      continue;
    }

    blender::Light &b_light = blender::id_cast<blender::Light &>(*b_id);
    Shader *shader;

    /* test if we need to sync */
    if (shader_map.add_or_update(&shader, &b_light.id) || update_all ||
        scene_attr_needs_recalc(shader, b_depsgraph))
    {
      unique_ptr<ShaderGraph> graph = make_unique<ShaderGraph>();

      /* create nodes */
      if (b_light.nodetree) {
        shader->name = BKE_id_name(b_light.id);

        add_nodes(scene, *b_engine, *b_data, *b_scene, graph.get(), *b_light.nodetree);
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
  ITER_END;
}

void BlenderSync::sync_shaders(blender::Depsgraph &b_depsgraph,
                               blender::bScreen *b_screen,
                               blender::View3D *b_v3d,
                               bool update_all)
{
  shader_map.pre_sync();

  sync_world(b_depsgraph, b_screen, b_v3d, update_all);
  sync_lights(b_depsgraph, update_all);
  sync_materials(b_depsgraph, update_all);
}

CCL_NAMESPACE_END
