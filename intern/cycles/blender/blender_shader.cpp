/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "render/background.h"
#include "render/colorspace.h"
#include "render/graph.h"
#include "render/light.h"
#include "render/nodes.h"
#include "render/osl.h"
#include "render/scene.h"
#include "render/shader.h"

#include "blender/blender_texture.h"
#include "blender/blender_sync.h"
#include "blender/blender_util.h"

#include "util/util_debug.h"
#include "util/util_foreach.h"
#include "util/util_string.h"
#include "util/util_set.h"
#include "util/util_task.h"

CCL_NAMESPACE_BEGIN

typedef map<void *, ShaderInput *> PtrInputMap;
typedef map<void *, ShaderOutput *> PtrOutputMap;
typedef map<string, ConvertNode *> ProxyMap;

/* Find */

void BlenderSync::find_shader(BL::ID &id, vector<Shader *> &used_shaders, Shader *default_shader)
{
  Shader *shader = (id) ? shader_map.find(id) : default_shader;

  used_shaders.push_back(shader);
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

static DisplacementMethod get_displacement_method(PointerRNA &ptr)
{
  return (DisplacementMethod)get_enum(
      ptr, "displacement_method", DISPLACE_NUM_METHODS, DISPLACE_BUMP);
}

static int validate_enum_value(int value, int num_values, int default_value)
{
  if (value >= num_values) {
    return default_value;
  }
  return value;
}

template<typename NodeType> static InterpolationType get_image_interpolation(NodeType &b_node)
{
  int value = b_node.interpolation();
  return (InterpolationType)validate_enum_value(
      value, INTERPOLATION_NUM_TYPES, INTERPOLATION_LINEAR);
}

template<typename NodeType> static ExtensionType get_image_extension(NodeType &b_node)
{
  int value = b_node.extension();
  return (ExtensionType)validate_enum_value(value, EXTENSION_NUM_TYPES, EXTENSION_REPEAT);
}

static ImageAlphaType get_image_alpha_type(BL::Image &b_image)
{
  int value = b_image.alpha_mode();
  return (ImageAlphaType)validate_enum_value(value, IMAGE_ALPHA_NUM_TYPES, IMAGE_ALPHA_AUTO);
}

/* Graph */

static BL::NodeSocket get_node_output(BL::Node &b_node, const string &name)
{
  BL::Node::outputs_iterator b_out;

  for (b_node.outputs.begin(b_out); b_out != b_node.outputs.end(); ++b_out)
    if (b_out->name() == name)
      return *b_out;

  assert(0);

  return *b_out;
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
      node->set(socket, get_int(b_sock.ptr, "default_value"));
      break;
    }
    case SocketType::COLOR: {
      node->set(socket, float4_to_float3(get_float4(b_sock.ptr, "default_value")));
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

static void get_tex_mapping(TextureMapping *mapping, BL::TexMapping &b_mapping)
{
  if (!b_mapping)
    return;

  mapping->translation = get_float3(b_mapping.translation());
  mapping->rotation = get_float3(b_mapping.rotation());
  mapping->scale = get_float3(b_mapping.scale());
  mapping->type = (TextureMapping::Type)b_mapping.vector_type();

  mapping->x_mapping = (TextureMapping::Mapping)b_mapping.mapping_x();
  mapping->y_mapping = (TextureMapping::Mapping)b_mapping.mapping_y();
  mapping->z_mapping = (TextureMapping::Mapping)b_mapping.mapping_z();
}

static void get_tex_mapping(TextureMapping *mapping, BL::ShaderNodeMapping &b_mapping)
{
  if (!b_mapping)
    return;

  mapping->translation = get_float3(b_mapping.translation());
  mapping->rotation = get_float3(b_mapping.rotation());
  mapping->scale = get_float3(b_mapping.scale());
  mapping->type = (TextureMapping::Type)b_mapping.vector_type();

  mapping->use_minmax = b_mapping.use_min() || b_mapping.use_max();

  if (b_mapping.use_min())
    mapping->min = get_float3(b_mapping.min());
  if (b_mapping.use_max())
    mapping->max = get_float3(b_mapping.max());
}

static ShaderNode *add_node(Scene *scene,
                            BL::RenderEngine &b_engine,
                            BL::BlendData &b_data,
                            BL::Depsgraph &b_depsgraph,
                            BL::Scene &b_scene,
                            ShaderGraph *graph,
                            BL::ShaderNodeTree &b_ntree,
                            BL::ShaderNode &b_node)
{
  ShaderNode *node = NULL;

  /* existing blender nodes */
  if (b_node.is_a(&RNA_ShaderNodeRGBCurve)) {
    BL::ShaderNodeRGBCurve b_curve_node(b_node);
    BL::CurveMapping mapping(b_curve_node.mapping());
    RGBCurvesNode *curves = new RGBCurvesNode();
    curvemapping_color_to_array(mapping, curves->curves, RAMP_TABLE_SIZE, true);
    curvemapping_minmax(mapping, true, &curves->min_x, &curves->max_x);
    node = curves;
  }
  if (b_node.is_a(&RNA_ShaderNodeVectorCurve)) {
    BL::ShaderNodeVectorCurve b_curve_node(b_node);
    BL::CurveMapping mapping(b_curve_node.mapping());
    VectorCurvesNode *curves = new VectorCurvesNode();
    curvemapping_color_to_array(mapping, curves->curves, RAMP_TABLE_SIZE, false);
    curvemapping_minmax(mapping, false, &curves->min_x, &curves->max_x);
    node = curves;
  }
  else if (b_node.is_a(&RNA_ShaderNodeValToRGB)) {
    RGBRampNode *ramp = new RGBRampNode();
    BL::ShaderNodeValToRGB b_ramp_node(b_node);
    BL::ColorRamp b_color_ramp(b_ramp_node.color_ramp());
    colorramp_to_array(b_color_ramp, ramp->ramp, ramp->ramp_alpha, RAMP_TABLE_SIZE);
    ramp->interpolate = b_color_ramp.interpolation() != BL::ColorRamp::interpolation_CONSTANT;
    node = ramp;
  }
  else if (b_node.is_a(&RNA_ShaderNodeRGB)) {
    ColorNode *color = new ColorNode();
    color->value = get_node_output_rgba(b_node, "Color");
    node = color;
  }
  else if (b_node.is_a(&RNA_ShaderNodeValue)) {
    ValueNode *value = new ValueNode();
    value->value = get_node_output_value(b_node, "Value");
    node = value;
  }
  else if (b_node.is_a(&RNA_ShaderNodeCameraData)) {
    node = new CameraNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeInvert)) {
    node = new InvertNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeGamma)) {
    node = new GammaNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeBrightContrast)) {
    node = new BrightContrastNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeMixRGB)) {
    BL::ShaderNodeMixRGB b_mix_node(b_node);
    MixNode *mix = new MixNode();
    mix->type = (NodeMix)b_mix_node.blend_type();
    mix->use_clamp = b_mix_node.use_clamp();
    node = mix;
  }
  else if (b_node.is_a(&RNA_ShaderNodeSeparateRGB)) {
    node = new SeparateRGBNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeCombineRGB)) {
    node = new CombineRGBNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeSeparateHSV)) {
    node = new SeparateHSVNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeCombineHSV)) {
    node = new CombineHSVNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeSeparateXYZ)) {
    node = new SeparateXYZNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeCombineXYZ)) {
    node = new CombineXYZNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeHueSaturation)) {
    node = new HSVNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeRGBToBW)) {
    node = new RGBToBWNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeMath)) {
    BL::ShaderNodeMath b_math_node(b_node);
    MathNode *math = new MathNode();
    math->type = (NodeMath)b_math_node.operation();
    math->use_clamp = b_math_node.use_clamp();
    node = math;
  }
  else if (b_node.is_a(&RNA_ShaderNodeVectorMath)) {
    BL::ShaderNodeVectorMath b_vector_math_node(b_node);
    VectorMathNode *vmath = new VectorMathNode();
    vmath->type = (NodeVectorMath)b_vector_math_node.operation();
    node = vmath;
  }
  else if (b_node.is_a(&RNA_ShaderNodeVectorTransform)) {
    BL::ShaderNodeVectorTransform b_vector_transform_node(b_node);
    VectorTransformNode *vtransform = new VectorTransformNode();
    vtransform->type = (NodeVectorTransformType)b_vector_transform_node.vector_type();
    vtransform->convert_from = (NodeVectorTransformConvertSpace)
                                   b_vector_transform_node.convert_from();
    vtransform->convert_to = (NodeVectorTransformConvertSpace)b_vector_transform_node.convert_to();
    node = vtransform;
  }
  else if (b_node.is_a(&RNA_ShaderNodeNormal)) {
    BL::Node::outputs_iterator out_it;
    b_node.outputs.begin(out_it);

    NormalNode *norm = new NormalNode();
    norm->direction = get_node_output_vector(b_node, "Normal");
    node = norm;
  }
  else if (b_node.is_a(&RNA_ShaderNodeMapping)) {
    BL::ShaderNodeMapping b_mapping_node(b_node);
    MappingNode *mapping = new MappingNode();

    get_tex_mapping(&mapping->tex_mapping, b_mapping_node);

    node = mapping;
  }
  else if (b_node.is_a(&RNA_ShaderNodeFresnel)) {
    node = new FresnelNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeLayerWeight)) {
    node = new LayerWeightNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeAddShader)) {
    node = new AddClosureNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeMixShader)) {
    node = new MixClosureNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeAttribute)) {
    BL::ShaderNodeAttribute b_attr_node(b_node);
    AttributeNode *attr = new AttributeNode();
    attr->attribute = b_attr_node.attribute_name();
    node = attr;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBackground)) {
    node = new BackgroundNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeHoldout)) {
    node = new HoldoutNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfAnisotropic)) {
    BL::ShaderNodeBsdfAnisotropic b_aniso_node(b_node);
    AnisotropicBsdfNode *aniso = new AnisotropicBsdfNode();

    switch (b_aniso_node.distribution()) {
      case BL::ShaderNodeBsdfAnisotropic::distribution_BECKMANN:
        aniso->distribution = CLOSURE_BSDF_MICROFACET_BECKMANN_ANISO_ID;
        break;
      case BL::ShaderNodeBsdfAnisotropic::distribution_GGX:
        aniso->distribution = CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID;
        break;
      case BL::ShaderNodeBsdfAnisotropic::distribution_MULTI_GGX:
        aniso->distribution = CLOSURE_BSDF_MICROFACET_MULTI_GGX_ANISO_ID;
        break;
      case BL::ShaderNodeBsdfAnisotropic::distribution_ASHIKHMIN_SHIRLEY:
        aniso->distribution = CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ANISO_ID;
        break;
    }

    node = aniso;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfDiffuse)) {
    node = new DiffuseBsdfNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeSubsurfaceScattering)) {
    BL::ShaderNodeSubsurfaceScattering b_subsurface_node(b_node);

    SubsurfaceScatteringNode *subsurface = new SubsurfaceScatteringNode();

    switch (b_subsurface_node.falloff()) {
      case BL::ShaderNodeSubsurfaceScattering::falloff_CUBIC:
        subsurface->falloff = CLOSURE_BSSRDF_CUBIC_ID;
        break;
      case BL::ShaderNodeSubsurfaceScattering::falloff_GAUSSIAN:
        subsurface->falloff = CLOSURE_BSSRDF_GAUSSIAN_ID;
        break;
      case BL::ShaderNodeSubsurfaceScattering::falloff_BURLEY:
        subsurface->falloff = CLOSURE_BSSRDF_BURLEY_ID;
        break;
      case BL::ShaderNodeSubsurfaceScattering::falloff_RANDOM_WALK:
        subsurface->falloff = CLOSURE_BSSRDF_RANDOM_WALK_ID;
        break;
    }

    node = subsurface;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfGlossy)) {
    BL::ShaderNodeBsdfGlossy b_glossy_node(b_node);
    GlossyBsdfNode *glossy = new GlossyBsdfNode();

    switch (b_glossy_node.distribution()) {
      case BL::ShaderNodeBsdfGlossy::distribution_SHARP:
        glossy->distribution = CLOSURE_BSDF_REFLECTION_ID;
        break;
      case BL::ShaderNodeBsdfGlossy::distribution_BECKMANN:
        glossy->distribution = CLOSURE_BSDF_MICROFACET_BECKMANN_ID;
        break;
      case BL::ShaderNodeBsdfGlossy::distribution_GGX:
        glossy->distribution = CLOSURE_BSDF_MICROFACET_GGX_ID;
        break;
      case BL::ShaderNodeBsdfGlossy::distribution_ASHIKHMIN_SHIRLEY:
        glossy->distribution = CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID;
        break;
      case BL::ShaderNodeBsdfGlossy::distribution_MULTI_GGX:
        glossy->distribution = CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID;
        break;
    }
    node = glossy;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfGlass)) {
    BL::ShaderNodeBsdfGlass b_glass_node(b_node);
    GlassBsdfNode *glass = new GlassBsdfNode();
    switch (b_glass_node.distribution()) {
      case BL::ShaderNodeBsdfGlass::distribution_SHARP:
        glass->distribution = CLOSURE_BSDF_SHARP_GLASS_ID;
        break;
      case BL::ShaderNodeBsdfGlass::distribution_BECKMANN:
        glass->distribution = CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID;
        break;
      case BL::ShaderNodeBsdfGlass::distribution_GGX:
        glass->distribution = CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID;
        break;
      case BL::ShaderNodeBsdfGlass::distribution_MULTI_GGX:
        glass->distribution = CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID;
        break;
    }
    node = glass;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfRefraction)) {
    BL::ShaderNodeBsdfRefraction b_refraction_node(b_node);
    RefractionBsdfNode *refraction = new RefractionBsdfNode();
    switch (b_refraction_node.distribution()) {
      case BL::ShaderNodeBsdfRefraction::distribution_SHARP:
        refraction->distribution = CLOSURE_BSDF_REFRACTION_ID;
        break;
      case BL::ShaderNodeBsdfRefraction::distribution_BECKMANN:
        refraction->distribution = CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID;
        break;
      case BL::ShaderNodeBsdfRefraction::distribution_GGX:
        refraction->distribution = CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
        break;
    }
    node = refraction;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfToon)) {
    BL::ShaderNodeBsdfToon b_toon_node(b_node);
    ToonBsdfNode *toon = new ToonBsdfNode();
    switch (b_toon_node.component()) {
      case BL::ShaderNodeBsdfToon::component_DIFFUSE:
        toon->component = CLOSURE_BSDF_DIFFUSE_TOON_ID;
        break;
      case BL::ShaderNodeBsdfToon::component_GLOSSY:
        toon->component = CLOSURE_BSDF_GLOSSY_TOON_ID;
        break;
    }
    node = toon;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfHair)) {
    BL::ShaderNodeBsdfHair b_hair_node(b_node);
    HairBsdfNode *hair = new HairBsdfNode();
    switch (b_hair_node.component()) {
      case BL::ShaderNodeBsdfHair::component_Reflection:
        hair->component = CLOSURE_BSDF_HAIR_REFLECTION_ID;
        break;
      case BL::ShaderNodeBsdfHair::component_Transmission:
        hair->component = CLOSURE_BSDF_HAIR_TRANSMISSION_ID;
        break;
    }
    node = hair;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfHairPrincipled)) {
    BL::ShaderNodeBsdfHairPrincipled b_principled_hair_node(b_node);
    PrincipledHairBsdfNode *principled_hair = new PrincipledHairBsdfNode();
    principled_hair->parametrization = (NodePrincipledHairParametrization)get_enum(
        b_principled_hair_node.ptr,
        "parametrization",
        NODE_PRINCIPLED_HAIR_NUM,
        NODE_PRINCIPLED_HAIR_REFLECTANCE);
    node = principled_hair;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfPrincipled)) {
    BL::ShaderNodeBsdfPrincipled b_principled_node(b_node);
    PrincipledBsdfNode *principled = new PrincipledBsdfNode();
    switch (b_principled_node.distribution()) {
      case BL::ShaderNodeBsdfPrincipled::distribution_GGX:
        principled->distribution = CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID;
        break;
      case BL::ShaderNodeBsdfPrincipled::distribution_MULTI_GGX:
        principled->distribution = CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID;
        break;
    }
    switch (b_principled_node.subsurface_method()) {
      case BL::ShaderNodeBsdfPrincipled::subsurface_method_BURLEY:
        principled->subsurface_method = CLOSURE_BSSRDF_PRINCIPLED_ID;
        break;
      case BL::ShaderNodeBsdfPrincipled::subsurface_method_RANDOM_WALK:
        principled->subsurface_method = CLOSURE_BSSRDF_PRINCIPLED_RANDOM_WALK_ID;
        break;
    }
    node = principled;
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfTranslucent)) {
    node = new TranslucentBsdfNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfTransparent)) {
    node = new TransparentBsdfNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeBsdfVelvet)) {
    node = new VelvetBsdfNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeEmission)) {
    node = new EmissionNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeAmbientOcclusion)) {
    BL::ShaderNodeAmbientOcclusion b_ao_node(b_node);
    AmbientOcclusionNode *ao = new AmbientOcclusionNode();
    ao->samples = b_ao_node.samples();
    ao->inside = b_ao_node.inside();
    ao->only_local = b_ao_node.only_local();
    node = ao;
  }
  else if (b_node.is_a(&RNA_ShaderNodeVolumeScatter)) {
    node = new ScatterVolumeNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeVolumeAbsorption)) {
    node = new AbsorptionVolumeNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeVolumePrincipled)) {
    PrincipledVolumeNode *principled = new PrincipledVolumeNode();
    node = principled;
  }
  else if (b_node.is_a(&RNA_ShaderNodeNewGeometry)) {
    node = new GeometryNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeWireframe)) {
    BL::ShaderNodeWireframe b_wireframe_node(b_node);
    WireframeNode *wire = new WireframeNode();
    wire->use_pixel_size = b_wireframe_node.use_pixel_size();
    node = wire;
  }
  else if (b_node.is_a(&RNA_ShaderNodeWavelength)) {
    node = new WavelengthNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeBlackbody)) {
    node = new BlackbodyNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeLightPath)) {
    node = new LightPathNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeLightFalloff)) {
    node = new LightFalloffNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeObjectInfo)) {
    node = new ObjectInfoNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeParticleInfo)) {
    node = new ParticleInfoNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeHairInfo)) {
    node = new HairInfoNode();
  }
  else if (b_node.is_a(&RNA_ShaderNodeBump)) {
    BL::ShaderNodeBump b_bump_node(b_node);
    BumpNode *bump = new BumpNode();
    bump->invert = b_bump_node.invert();
    node = bump;
  }
  else if (b_node.is_a(&RNA_ShaderNodeScript)) {
#ifdef WITH_OSL
    if (scene->shader_manager->use_osl()) {
      /* create script node */
      BL::ShaderNodeScript b_script_node(b_node);

      OSLShaderManager *manager = (OSLShaderManager *)scene->shader_manager;
      string bytecode_hash = b_script_node.bytecode_hash();

      if (!bytecode_hash.empty()) {
        node = manager->osl_node("", bytecode_hash, b_script_node.bytecode());
      }
      else {
        string absolute_filepath = blender_absolute_path(
            b_data, b_ntree, b_script_node.filepath());
        node = manager->osl_node(absolute_filepath, "");
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
    ImageTextureNode *image = new ImageTextureNode();
    if (b_image) {
      /* builtin images will use callback-based reading because
       * they could only be loaded correct from blender side
       */
      bool is_builtin = b_image.packed_file() || b_image.source() == BL::Image::source_GENERATED ||
                        b_image.source() == BL::Image::source_MOVIE ||
                        (b_engine.is_preview() && b_image.source() != BL::Image::source_SEQUENCE);

      if (is_builtin) {
        /* for builtin images we're using image datablock name to find an image to
         * read pixels from later
         *
         * also store frame number as well, so there's no differences in handling
         * builtin names for packed images and movies
         */
        int scene_frame = b_scene.frame_current();
        int image_frame = image_user_frame_number(b_image_user, scene_frame);
        image->filename = b_image.name() + "@" + string_printf("%d", image_frame);
        image->builtin_data = b_image.ptr.data;
      }
      else {
        image->filename = image_user_file_path(b_image_user, b_image, b_scene.frame_current());
        image->builtin_data = NULL;
      }

      PointerRNA colorspace_ptr = b_image.colorspace_settings().ptr;
      image->colorspace = get_enum_identifier(colorspace_ptr, "name");

      image->animated = b_image_node.image_user().use_auto_refresh();
      image->alpha_type = get_image_alpha_type(b_image);

      /* TODO: restore */
      /* TODO(sergey): Does not work properly when we change builtin type. */
#if 0
      if (b_image.is_updated()) {
        scene->image_manager->tag_reload_image(image->filename.string(),
                                               image->builtin_data,
                                               get_image_interpolation(b_image_node),
                                               get_image_extension(b_image_node),
                                               image->use_alpha,
                                               image->colorspace);
      }
#endif
    }
    image->projection = (NodeImageProjection)b_image_node.projection();
    image->interpolation = get_image_interpolation(b_image_node);
    image->extension = get_image_extension(b_image_node);
    image->projection_blend = b_image_node.projection_blend();
    BL::TexMapping b_texture_mapping(b_image_node.texture_mapping());
    get_tex_mapping(&image->tex_mapping, b_texture_mapping);
    node = image;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexEnvironment)) {
    BL::ShaderNodeTexEnvironment b_env_node(b_node);
    BL::Image b_image(b_env_node.image());
    BL::ImageUser b_image_user(b_env_node.image_user());
    EnvironmentTextureNode *env = new EnvironmentTextureNode();
    if (b_image) {
      bool is_builtin = b_image.packed_file() || b_image.source() == BL::Image::source_GENERATED ||
                        b_image.source() == BL::Image::source_MOVIE ||
                        (b_engine.is_preview() && b_image.source() != BL::Image::source_SEQUENCE);

      if (is_builtin) {
        int scene_frame = b_scene.frame_current();
        int image_frame = image_user_frame_number(b_image_user, scene_frame);
        env->filename = b_image.name() + "@" + string_printf("%d", image_frame);
        env->builtin_data = b_image.ptr.data;
      }
      else {
        env->filename = image_user_file_path(b_image_user, b_image, b_scene.frame_current());
        env->builtin_data = NULL;
      }

      PointerRNA colorspace_ptr = b_image.colorspace_settings().ptr;
      env->colorspace = get_enum_identifier(colorspace_ptr, "name");

      env->animated = b_env_node.image_user().use_auto_refresh();
      env->alpha_type = get_image_alpha_type(b_image);

      /* TODO: restore */
      /* TODO(sergey): Does not work properly when we change builtin type. */
#if 0
      if (b_image.is_updated()) {
        scene->image_manager->tag_reload_image(env->filename.string(),
                                               env->builtin_data,
                                               get_image_interpolation(b_env_node),
                                               EXTENSION_REPEAT,
                                               env->use_alpha,
                                               env->colorspace);
      }
#endif
    }
    env->interpolation = get_image_interpolation(b_env_node);
    env->projection = (NodeEnvironmentProjection)b_env_node.projection();
    BL::TexMapping b_texture_mapping(b_env_node.texture_mapping());
    get_tex_mapping(&env->tex_mapping, b_texture_mapping);
    node = env;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexGradient)) {
    BL::ShaderNodeTexGradient b_gradient_node(b_node);
    GradientTextureNode *gradient = new GradientTextureNode();
    gradient->type = (NodeGradientType)b_gradient_node.gradient_type();
    BL::TexMapping b_texture_mapping(b_gradient_node.texture_mapping());
    get_tex_mapping(&gradient->tex_mapping, b_texture_mapping);
    node = gradient;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexVoronoi)) {
    BL::ShaderNodeTexVoronoi b_voronoi_node(b_node);
    VoronoiTextureNode *voronoi = new VoronoiTextureNode();
    voronoi->coloring = (NodeVoronoiColoring)b_voronoi_node.coloring();
    voronoi->metric = (NodeVoronoiDistanceMetric)b_voronoi_node.distance();
    voronoi->feature = (NodeVoronoiFeature)b_voronoi_node.feature();
    BL::TexMapping b_texture_mapping(b_voronoi_node.texture_mapping());
    get_tex_mapping(&voronoi->tex_mapping, b_texture_mapping);
    node = voronoi;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexMagic)) {
    BL::ShaderNodeTexMagic b_magic_node(b_node);
    MagicTextureNode *magic = new MagicTextureNode();
    magic->depth = b_magic_node.turbulence_depth();
    BL::TexMapping b_texture_mapping(b_magic_node.texture_mapping());
    get_tex_mapping(&magic->tex_mapping, b_texture_mapping);
    node = magic;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexWave)) {
    BL::ShaderNodeTexWave b_wave_node(b_node);
    WaveTextureNode *wave = new WaveTextureNode();
    wave->type = (NodeWaveType)b_wave_node.wave_type();
    wave->profile = (NodeWaveProfile)b_wave_node.wave_profile();
    BL::TexMapping b_texture_mapping(b_wave_node.texture_mapping());
    get_tex_mapping(&wave->tex_mapping, b_texture_mapping);
    node = wave;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexChecker)) {
    BL::ShaderNodeTexChecker b_checker_node(b_node);
    CheckerTextureNode *checker = new CheckerTextureNode();
    BL::TexMapping b_texture_mapping(b_checker_node.texture_mapping());
    get_tex_mapping(&checker->tex_mapping, b_texture_mapping);
    node = checker;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexBrick)) {
    BL::ShaderNodeTexBrick b_brick_node(b_node);
    BrickTextureNode *brick = new BrickTextureNode();
    brick->offset = b_brick_node.offset();
    brick->offset_frequency = b_brick_node.offset_frequency();
    brick->squash = b_brick_node.squash();
    brick->squash_frequency = b_brick_node.squash_frequency();
    BL::TexMapping b_texture_mapping(b_brick_node.texture_mapping());
    get_tex_mapping(&brick->tex_mapping, b_texture_mapping);
    node = brick;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexNoise)) {
    BL::ShaderNodeTexNoise b_noise_node(b_node);
    NoiseTextureNode *noise = new NoiseTextureNode();
    BL::TexMapping b_texture_mapping(b_noise_node.texture_mapping());
    get_tex_mapping(&noise->tex_mapping, b_texture_mapping);
    node = noise;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexMusgrave)) {
    BL::ShaderNodeTexMusgrave b_musgrave_node(b_node);
    MusgraveTextureNode *musgrave = new MusgraveTextureNode();
    musgrave->type = (NodeMusgraveType)b_musgrave_node.musgrave_type();
    BL::TexMapping b_texture_mapping(b_musgrave_node.texture_mapping());
    get_tex_mapping(&musgrave->tex_mapping, b_texture_mapping);
    node = musgrave;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexCoord)) {
    BL::ShaderNodeTexCoord b_tex_coord_node(b_node);
    TextureCoordinateNode *tex_coord = new TextureCoordinateNode();
    tex_coord->from_dupli = b_tex_coord_node.from_instancer();
    if (b_tex_coord_node.object()) {
      tex_coord->use_transform = true;
      tex_coord->ob_tfm = get_transform(b_tex_coord_node.object().matrix_world());
    }
    node = tex_coord;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexSky)) {
    BL::ShaderNodeTexSky b_sky_node(b_node);
    SkyTextureNode *sky = new SkyTextureNode();
    sky->type = (NodeSkyType)b_sky_node.sky_type();
    sky->sun_direction = normalize(get_float3(b_sky_node.sun_direction()));
    sky->turbidity = b_sky_node.turbidity();
    sky->ground_albedo = b_sky_node.ground_albedo();
    BL::TexMapping b_texture_mapping(b_sky_node.texture_mapping());
    get_tex_mapping(&sky->tex_mapping, b_texture_mapping);
    node = sky;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexIES)) {
    BL::ShaderNodeTexIES b_ies_node(b_node);
    IESLightNode *ies = new IESLightNode();
    switch (b_ies_node.mode()) {
      case BL::ShaderNodeTexIES::mode_EXTERNAL:
        ies->filename = blender_absolute_path(b_data, b_ntree, b_ies_node.filepath());
        break;
      case BL::ShaderNodeTexIES::mode_INTERNAL:
        ies->ies = get_text_datablock_content(b_ies_node.ies().ptr);
        if (ies->ies.empty()) {
          ies->ies = "\n";
        }
        break;
    }
    node = ies;
  }
  else if (b_node.is_a(&RNA_ShaderNodeNormalMap)) {
    BL::ShaderNodeNormalMap b_normal_map_node(b_node);
    NormalMapNode *nmap = new NormalMapNode();
    nmap->space = (NodeNormalMapSpace)b_normal_map_node.space();
    nmap->attribute = b_normal_map_node.uv_map();
    node = nmap;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTangent)) {
    BL::ShaderNodeTangent b_tangent_node(b_node);
    TangentNode *tangent = new TangentNode();
    tangent->direction_type = (NodeTangentDirectionType)b_tangent_node.direction_type();
    tangent->axis = (NodeTangentAxis)b_tangent_node.axis();
    tangent->attribute = b_tangent_node.uv_map();
    node = tangent;
  }
  else if (b_node.is_a(&RNA_ShaderNodeUVMap)) {
    BL::ShaderNodeUVMap b_uvmap_node(b_node);
    UVMapNode *uvm = new UVMapNode();
    uvm->attribute = b_uvmap_node.uv_map();
    uvm->from_dupli = b_uvmap_node.from_instancer();
    node = uvm;
  }
  else if (b_node.is_a(&RNA_ShaderNodeTexPointDensity)) {
    BL::ShaderNodeTexPointDensity b_point_density_node(b_node);
    PointDensityTextureNode *point_density = new PointDensityTextureNode();
    point_density->filename = b_point_density_node.name();
    point_density->space = (NodeTexVoxelSpace)b_point_density_node.space();
    point_density->interpolation = get_image_interpolation(b_point_density_node);
    point_density->builtin_data = b_point_density_node.ptr.data;
    point_density->image_manager = scene->image_manager;

    /* TODO(sergey): Use more proper update flag. */
    if (true) {
      point_density->add_image();
      b_point_density_node.cache_point_density(b_depsgraph);
      scene->image_manager->tag_reload_image(point_density->filename.string(),
                                             point_density->builtin_data,
                                             point_density->interpolation,
                                             EXTENSION_CLIP,
                                             IMAGE_ALPHA_AUTO,
                                             u_colorspace_raw);
    }
    node = point_density;

    /* Transformation form world space to texture space.
     *
     * NOTE: Do this after the texture is cached, this is because getting
     * min/max will need to access this cache.
     */
    BL::Object b_ob(b_point_density_node.object());
    if (b_ob) {
      float3 loc, size;
      point_density_texture_space(b_depsgraph, b_point_density_node, loc, size);
      point_density->tfm = transform_translate(-loc) * transform_scale(size) *
                           transform_inverse(get_transform(b_ob.matrix_world()));
    }
  }
  else if (b_node.is_a(&RNA_ShaderNodeBevel)) {
    BL::ShaderNodeBevel b_bevel_node(b_node);
    BevelNode *bevel = new BevelNode();
    bevel->samples = b_bevel_node.samples();
    node = bevel;
  }
  else if (b_node.is_a(&RNA_ShaderNodeDisplacement)) {
    BL::ShaderNodeDisplacement b_disp_node(b_node);
    DisplacementNode *disp = new DisplacementNode();
    disp->space = (NodeNormalMapSpace)b_disp_node.space();
    node = disp;
  }
  else if (b_node.is_a(&RNA_ShaderNodeVectorDisplacement)) {
    BL::ShaderNodeVectorDisplacement b_disp_node(b_node);
    VectorDisplacementNode *disp = new VectorDisplacementNode();
    disp->space = (NodeNormalMapSpace)b_disp_node.space();
    disp->attribute = "";
    node = disp;
  }

  if (node) {
    node->name = b_node.name();
    graph->add(node);
  }

  return node;
}

static bool node_use_modified_socket_name(ShaderNode *node)
{
  if (node->special_type == SHADER_SPECIAL_TYPE_OSL)
    return false;

  return true;
}

static ShaderInput *node_find_input_by_name(ShaderNode *node,
                                            BL::Node &b_node,
                                            BL::NodeSocket &b_socket)
{
  string name = b_socket.name();

  if (node_use_modified_socket_name(node)) {
    BL::Node::inputs_iterator b_input;
    bool found = false;
    int counter = 0, total = 0;

    for (b_node.inputs.begin(b_input); b_input != b_node.inputs.end(); ++b_input) {
      if (b_input->name() == name) {
        if (!found)
          counter++;
        total++;
      }

      if (b_input->ptr.data == b_socket.ptr.data)
        found = true;
    }

    /* rename if needed */
    if (name == "Shader")
      name = "Closure";

    if (total > 1)
      name = string_printf("%s%d", name.c_str(), counter);
  }

  return node->input(name.c_str());
}

static ShaderOutput *node_find_output_by_name(ShaderNode *node,
                                              BL::Node &b_node,
                                              BL::NodeSocket &b_socket)
{
  string name = b_socket.name();

  if (node_use_modified_socket_name(node)) {
    BL::Node::outputs_iterator b_output;
    bool found = false;
    int counter = 0, total = 0;

    for (b_node.outputs.begin(b_output); b_output != b_node.outputs.end(); ++b_output) {
      if (b_output->name() == name) {
        if (!found)
          counter++;
        total++;
      }

      if (b_output->ptr.data == b_socket.ptr.data)
        found = true;
    }

    /* rename if needed */
    if (name == "Shader")
      name = "Closure";

    if (total > 1)
      name = string_printf("%s%d", name.c_str(), counter);
  }

  return node->output(name.c_str());
}

static void add_nodes(Scene *scene,
                      BL::RenderEngine &b_engine,
                      BL::BlendData &b_data,
                      BL::Depsgraph &b_depsgraph,
                      BL::Scene &b_scene,
                      ShaderGraph *graph,
                      BL::ShaderNodeTree &b_ntree,
                      const ProxyMap &proxy_input_map,
                      const ProxyMap &proxy_output_map)
{
  /* add nodes */
  BL::ShaderNodeTree::nodes_iterator b_node;
  PtrInputMap input_map;
  PtrOutputMap output_map;

  BL::Node::inputs_iterator b_input;
  BL::Node::outputs_iterator b_output;

  /* find the node to use for output if there are multiple */
  BL::ShaderNode output_node = b_ntree.get_output_node(
      BL::ShaderNodeOutputMaterial::target_CYCLES);

  /* add nodes */
  for (b_ntree.nodes.begin(b_node); b_node != b_ntree.nodes.end(); ++b_node) {
    if (b_node->mute() || b_node->is_a(&RNA_NodeReroute)) {
      /* replace muted node with internal links */
      BL::Node::internal_links_iterator b_link;
      for (b_node->internal_links.begin(b_link); b_link != b_node->internal_links.end();
           ++b_link) {
        BL::NodeSocket to_socket(b_link->to_socket());
        SocketType::Type to_socket_type = convert_socket_type(to_socket);
        if (to_socket_type == SocketType::UNDEFINED) {
          continue;
        }

        ConvertNode *proxy = new ConvertNode(to_socket_type, to_socket_type, true);

        input_map[b_link->from_socket().ptr.data] = proxy->inputs[0];
        output_map[b_link->to_socket().ptr.data] = proxy->outputs[0];

        graph->add(proxy);
      }
    }
    else if (b_node->is_a(&RNA_ShaderNodeGroup) || b_node->is_a(&RNA_NodeCustomGroup) ||
             b_node->is_a(&RNA_ShaderNodeCustomGroup)) {

      BL::ShaderNodeTree b_group_ntree(PointerRNA_NULL);
      if (b_node->is_a(&RNA_ShaderNodeGroup))
        b_group_ntree = BL::ShaderNodeTree(((BL::NodeGroup)(*b_node)).node_tree());
      else if (b_node->is_a(&RNA_NodeCustomGroup))
        b_group_ntree = BL::ShaderNodeTree(((BL::NodeCustomGroup)(*b_node)).node_tree());
      else
        b_group_ntree = BL::ShaderNodeTree(((BL::ShaderNodeCustomGroup)(*b_node)).node_tree());

      ProxyMap group_proxy_input_map, group_proxy_output_map;

      /* Add a proxy node for each socket
       * Do this even if the node group has no internal tree,
       * so that links have something to connect to and assert won't fail.
       */
      for (b_node->inputs.begin(b_input); b_input != b_node->inputs.end(); ++b_input) {
        SocketType::Type input_type = convert_socket_type(*b_input);
        if (input_type == SocketType::UNDEFINED) {
          continue;
        }

        ConvertNode *proxy = new ConvertNode(input_type, input_type, true);
        graph->add(proxy);

        /* register the proxy node for internal binding */
        group_proxy_input_map[b_input->identifier()] = proxy;

        input_map[b_input->ptr.data] = proxy->inputs[0];

        set_default_value(proxy->inputs[0], *b_input, b_data, b_ntree);
      }
      for (b_node->outputs.begin(b_output); b_output != b_node->outputs.end(); ++b_output) {
        SocketType::Type output_type = convert_socket_type(*b_output);
        if (output_type == SocketType::UNDEFINED) {
          continue;
        }

        ConvertNode *proxy = new ConvertNode(output_type, output_type, true);
        graph->add(proxy);

        /* register the proxy node for internal binding */
        group_proxy_output_map[b_output->identifier()] = proxy;

        output_map[b_output->ptr.data] = proxy->outputs[0];
      }

      if (b_group_ntree) {
        add_nodes(scene,
                  b_engine,
                  b_data,
                  b_depsgraph,
                  b_scene,
                  graph,
                  b_group_ntree,
                  group_proxy_input_map,
                  group_proxy_output_map);
      }
    }
    else if (b_node->is_a(&RNA_NodeGroupInput)) {
      /* map each socket to a proxy node */
      for (b_node->outputs.begin(b_output); b_output != b_node->outputs.end(); ++b_output) {
        ProxyMap::const_iterator proxy_it = proxy_input_map.find(b_output->identifier());
        if (proxy_it != proxy_input_map.end()) {
          ConvertNode *proxy = proxy_it->second;

          output_map[b_output->ptr.data] = proxy->outputs[0];
        }
      }
    }
    else if (b_node->is_a(&RNA_NodeGroupOutput)) {
      BL::NodeGroupOutput b_output_node(*b_node);
      /* only the active group output is used */
      if (b_output_node.is_active_output()) {
        /* map each socket to a proxy node */
        for (b_node->inputs.begin(b_input); b_input != b_node->inputs.end(); ++b_input) {
          ProxyMap::const_iterator proxy_it = proxy_output_map.find(b_input->identifier());
          if (proxy_it != proxy_output_map.end()) {
            ConvertNode *proxy = proxy_it->second;

            input_map[b_input->ptr.data] = proxy->inputs[0];

            set_default_value(proxy->inputs[0], *b_input, b_data, b_ntree);
          }
        }
      }
    }
    else {
      ShaderNode *node = NULL;

      if (b_node->ptr.data == output_node.ptr.data) {
        node = graph->output();
      }
      else {
        BL::ShaderNode b_shader_node(*b_node);
        node = add_node(
            scene, b_engine, b_data, b_depsgraph, b_scene, graph, b_ntree, b_shader_node);
      }

      if (node) {
        /* map node sockets for linking */
        for (b_node->inputs.begin(b_input); b_input != b_node->inputs.end(); ++b_input) {
          ShaderInput *input = node_find_input_by_name(node, *b_node, *b_input);
          if (!input) {
            /* XXX should not happen, report error? */
            continue;
          }
          input_map[b_input->ptr.data] = input;

          set_default_value(input, *b_input, b_data, b_ntree);
        }
        for (b_node->outputs.begin(b_output); b_output != b_node->outputs.end(); ++b_output) {
          ShaderOutput *output = node_find_output_by_name(node, *b_node, *b_output);
          if (!output) {
            /* XXX should not happen, report error? */
            continue;
          }
          output_map[b_output->ptr.data] = output;
        }
      }
    }
  }

  /* connect nodes */
  BL::NodeTree::links_iterator b_link;

  for (b_ntree.links.begin(b_link); b_link != b_ntree.links.end(); ++b_link) {
    /* Ignore invalid links to avoid unwanted cycles created in graph. */
    if (!b_link->is_valid()) {
      continue;
    }
    /* get blender link data */
    BL::NodeSocket b_from_sock = b_link->from_socket();
    BL::NodeSocket b_to_sock = b_link->to_socket();

    ShaderOutput *output = 0;
    ShaderInput *input = 0;

    PtrOutputMap::iterator output_it = output_map.find(b_from_sock.ptr.data);
    if (output_it != output_map.end())
      output = output_it->second;
    PtrInputMap::iterator input_it = input_map.find(b_to_sock.ptr.data);
    if (input_it != input_map.end())
      input = input_it->second;

    /* either node may be NULL when the node was not exported, typically
     * because the node type is not supported */
    if (output && input)
      graph->connect(output, input);
  }
}

static void add_nodes(Scene *scene,
                      BL::RenderEngine &b_engine,
                      BL::BlendData &b_data,
                      BL::Depsgraph &b_depsgraph,
                      BL::Scene &b_scene,
                      ShaderGraph *graph,
                      BL::ShaderNodeTree &b_ntree)
{
  static const ProxyMap empty_proxy_map;
  add_nodes(scene,
            b_engine,
            b_data,
            b_depsgraph,
            b_scene,
            graph,
            b_ntree,
            empty_proxy_map,
            empty_proxy_map);
}

/* Sync Materials */

void BlenderSync::sync_materials(BL::Depsgraph &b_depsgraph, bool update_all)
{
  shader_map.set_default(scene->default_surface);

  TaskPool pool;
  set<Shader *> updated_shaders;

  BL::Depsgraph::ids_iterator b_id;
  for (b_depsgraph.ids.begin(b_id); b_id != b_depsgraph.ids.end(); ++b_id) {
    if (!b_id->is_a(&RNA_Material)) {
      continue;
    }

    BL::Material b_mat(*b_id);
    Shader *shader;

    /* test if we need to sync */
    if (shader_map.sync(&shader, b_mat) || shader->need_sync_object || update_all) {
      ShaderGraph *graph = new ShaderGraph();

      shader->name = b_mat.name().c_str();
      shader->pass_id = b_mat.pass_index();
      shader->need_sync_object = false;

      /* create nodes */
      if (b_mat.use_nodes() && b_mat.node_tree()) {
        BL::ShaderNodeTree b_ntree(b_mat.node_tree());

        add_nodes(scene, b_engine, b_data, b_depsgraph, b_scene, graph, b_ntree);
      }
      else {
        DiffuseBsdfNode *diffuse = new DiffuseBsdfNode();
        diffuse->color = get_float3(b_mat.diffuse_color());
        graph->add(diffuse);

        ShaderNode *out = graph->output();
        graph->connect(diffuse->output("BSDF"), out->input("Surface"));
      }

      /* settings */
      PointerRNA cmat = RNA_pointer_get(&b_mat.ptr, "cycles");
      shader->use_mis = get_boolean(cmat, "sample_as_light");
      shader->use_transparent_shadow = get_boolean(cmat, "use_transparent_shadow");
      shader->heterogeneous_volume = !get_boolean(cmat, "homogeneous_volume");
      shader->volume_sampling_method = get_volume_sampling(cmat);
      shader->volume_interpolation_method = get_volume_interpolation(cmat);
      shader->displacement_method = get_displacement_method(cmat);

      shader->set_graph(graph);

      /* By simplifying the shader graph as soon as possible, some
       * redundant shader nodes might be removed which prevents loading
       * unnecessary attributes later.
       *
       * However, since graph simplification also accounts for e.g. mix
       * weight, this would cause frequent expensive resyncs in interactive
       * sessions, so for those sessions optimization is only performed
       * right before compiling.
       */
      if (!preview) {
        pool.push(function_bind(&ShaderGraph::simplify, graph, scene));
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

  foreach (Shader *shader, updated_shaders) {
    shader->tag_update(scene);
  }
}

/* Sync World */

void BlenderSync::sync_world(BL::Depsgraph &b_depsgraph, bool update_all)
{
  Background *background = scene->background;
  Background prevbackground = *background;

  BL::World b_world = b_scene.world();

  if (world_recalc || update_all || b_world.ptr.data != world_map) {
    Shader *shader = scene->default_background;
    ShaderGraph *graph = new ShaderGraph();

    /* create nodes */
    if (b_world && b_world.use_nodes() && b_world.node_tree()) {
      BL::ShaderNodeTree b_ntree(b_world.node_tree());

      add_nodes(scene, b_engine, b_data, b_depsgraph, b_scene, graph, b_ntree);

      /* volume */
      PointerRNA cworld = RNA_pointer_get(&b_world.ptr, "cycles");
      shader->heterogeneous_volume = !get_boolean(cworld, "homogeneous_volume");
      shader->volume_sampling_method = get_volume_sampling(cworld);
      shader->volume_interpolation_method = get_volume_interpolation(cworld);
    }
    else if (b_world) {
      BackgroundNode *background = new BackgroundNode();
      background->color = get_float3(b_world.color());
      graph->add(background);

      ShaderNode *out = graph->output();
      graph->connect(background->output("Background"), out->input("Surface"));
    }

    if (b_world) {
      /* AO */
      BL::WorldLighting b_light = b_world.light_settings();

      background->use_ao = b_light.use_ambient_occlusion();
      background->ao_factor = b_light.ao_factor();
      background->ao_distance = b_light.distance();

      /* visibility */
      PointerRNA cvisibility = RNA_pointer_get(&b_world.ptr, "cycles_visibility");
      uint visibility = 0;

      visibility |= get_boolean(cvisibility, "camera") ? PATH_RAY_CAMERA : 0;
      visibility |= get_boolean(cvisibility, "diffuse") ? PATH_RAY_DIFFUSE : 0;
      visibility |= get_boolean(cvisibility, "glossy") ? PATH_RAY_GLOSSY : 0;
      visibility |= get_boolean(cvisibility, "transmission") ? PATH_RAY_TRANSMIT : 0;
      visibility |= get_boolean(cvisibility, "scatter") ? PATH_RAY_VOLUME_SCATTER : 0;

      background->visibility = visibility;
    }
    else {
      background->use_ao = false;
      background->ao_factor = 0.0f;
      background->ao_distance = FLT_MAX;
    }

    shader->set_graph(graph);
    shader->tag_update(scene);
    background->tag_update(scene);
  }

  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
  background->transparent = b_scene.render().film_transparent();

  if (background->transparent) {
    background->transparent_glass = get_boolean(cscene, "film_transparent_glass");
    background->transparent_roughness_threshold = get_float(cscene, "film_transparent_roughness");
  }
  else {
    background->transparent_glass = false;
    background->transparent_roughness_threshold = 0.0f;
  }

  background->use_shader = view_layer.use_background_shader;
  background->use_ao = background->use_ao && view_layer.use_background_ao;

  if (background->modified(prevbackground))
    background->tag_update(scene);
}

/* Sync Lights */

void BlenderSync::sync_lights(BL::Depsgraph &b_depsgraph, bool update_all)
{
  shader_map.set_default(scene->default_light);

  BL::Depsgraph::ids_iterator b_id;
  for (b_depsgraph.ids.begin(b_id); b_id != b_depsgraph.ids.end(); ++b_id) {
    if (!b_id->is_a(&RNA_Light)) {
      continue;
    }

    BL::Light b_light(*b_id);
    Shader *shader;

    /* test if we need to sync */
    if (shader_map.sync(&shader, b_light) || update_all) {
      ShaderGraph *graph = new ShaderGraph();

      /* create nodes */
      if (b_light.use_nodes() && b_light.node_tree()) {
        shader->name = b_light.name().c_str();

        BL::ShaderNodeTree b_ntree(b_light.node_tree());

        add_nodes(scene, b_engine, b_data, b_depsgraph, b_scene, graph, b_ntree);
      }
      else {
        EmissionNode *emission = new EmissionNode();
        emission->color = make_float3(1.0f, 1.0f, 1.0f);
        emission->strength = 1.0f;
        graph->add(emission);

        ShaderNode *out = graph->output();
        graph->connect(emission->output("Emission"), out->input("Surface"));
      }

      shader->set_graph(graph);
      shader->tag_update(scene);
    }
  }
}

void BlenderSync::sync_shaders(BL::Depsgraph &b_depsgraph)
{
  /* for auto refresh images */
  bool auto_refresh_update = false;

  if (preview) {
    ImageManager *image_manager = scene->image_manager;
    int frame = b_scene.frame_current();
    auto_refresh_update = image_manager->set_animation_frame_update(frame);
  }

  shader_map.pre_sync();

  sync_world(b_depsgraph, auto_refresh_update);
  sync_lights(b_depsgraph, auto_refresh_update);
  sync_materials(b_depsgraph, auto_refresh_update);

  /* false = don't delete unused shaders, not supported */
  shader_map.post_sync(false);
}

CCL_NAMESPACE_END
