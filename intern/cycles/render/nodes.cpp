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

#include "render/colorspace.h"
#include "render/film.h"
#include "render/image.h"
#include "render/integrator.h"
#include "render/light.h"
#include "render/nodes.h"
#include "render/scene.h"
#include "render/svm.h"
#include "kernel/svm/svm_color_util.h"
#include "kernel/svm/svm_ramp_util.h"
#include "kernel/svm/svm_math_util.h"
#include "render/osl.h"
#include "render/constant_fold.h"

#include "util/util_sky_model.h"
#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_transform.h"

CCL_NAMESPACE_BEGIN

/* Texture Mapping */

#define TEXTURE_MAPPING_DEFINE(TextureNode) \
  SOCKET_POINT(tex_mapping.translation, "Translation", make_float3(0.0f, 0.0f, 0.0f)); \
  SOCKET_VECTOR(tex_mapping.rotation, "Rotation", make_float3(0.0f, 0.0f, 0.0f)); \
  SOCKET_VECTOR(tex_mapping.scale, "Scale", make_float3(1.0f, 1.0f, 1.0f)); \
\
  SOCKET_VECTOR(tex_mapping.min, "Min", make_float3(-FLT_MAX, -FLT_MAX, -FLT_MAX)); \
  SOCKET_VECTOR(tex_mapping.max, "Max", make_float3(FLT_MAX, FLT_MAX, FLT_MAX)); \
  SOCKET_BOOLEAN(tex_mapping.use_minmax, "Use Min Max", false); \
\
  static NodeEnum mapping_axis_enum; \
  mapping_axis_enum.insert("none", TextureMapping::NONE); \
  mapping_axis_enum.insert("x", TextureMapping::X); \
  mapping_axis_enum.insert("y", TextureMapping::Y); \
  mapping_axis_enum.insert("z", TextureMapping::Z); \
  SOCKET_ENUM(tex_mapping.x_mapping, "x_mapping", mapping_axis_enum, TextureMapping::X); \
  SOCKET_ENUM(tex_mapping.y_mapping, "y_mapping", mapping_axis_enum, TextureMapping::Y); \
  SOCKET_ENUM(tex_mapping.z_mapping, "z_mapping", mapping_axis_enum, TextureMapping::Z); \
\
  static NodeEnum mapping_type_enum; \
  mapping_type_enum.insert("point", TextureMapping::POINT); \
  mapping_type_enum.insert("texture", TextureMapping::TEXTURE); \
  mapping_type_enum.insert("vector", TextureMapping::VECTOR); \
  mapping_type_enum.insert("normal", TextureMapping::NORMAL); \
  SOCKET_ENUM(tex_mapping.type, "Type", mapping_type_enum, TextureMapping::TEXTURE); \
\
  static NodeEnum mapping_projection_enum; \
  mapping_projection_enum.insert("flat", TextureMapping::FLAT); \
  mapping_projection_enum.insert("cube", TextureMapping::CUBE); \
  mapping_projection_enum.insert("tube", TextureMapping::TUBE); \
  mapping_projection_enum.insert("sphere", TextureMapping::SPHERE); \
  SOCKET_ENUM(tex_mapping.projection, "Projection", mapping_projection_enum, TextureMapping::FLAT);

TextureMapping::TextureMapping()
{
}

Transform TextureMapping::compute_transform()
{
  Transform mmat = transform_scale(make_float3(0.0f, 0.0f, 0.0f));

  if (x_mapping != NONE)
    mmat[0][x_mapping - 1] = 1.0f;
  if (y_mapping != NONE)
    mmat[1][y_mapping - 1] = 1.0f;
  if (z_mapping != NONE)
    mmat[2][z_mapping - 1] = 1.0f;

  float3 scale_clamped = scale;

  if (type == TEXTURE || type == NORMAL) {
    /* keep matrix invertible */
    if (fabsf(scale.x) < 1e-5f)
      scale_clamped.x = signf(scale.x) * 1e-5f;
    if (fabsf(scale.y) < 1e-5f)
      scale_clamped.y = signf(scale.y) * 1e-5f;
    if (fabsf(scale.z) < 1e-5f)
      scale_clamped.z = signf(scale.z) * 1e-5f;
  }

  Transform smat = transform_scale(scale_clamped);
  Transform rmat = transform_euler(rotation);
  Transform tmat = transform_translate(translation);

  Transform mat;

  switch (type) {
    case TEXTURE:
      /* inverse transform on texture coordinate gives
       * forward transform on texture */
      mat = tmat * rmat * smat;
      mat = transform_inverse(mat);
      break;
    case POINT:
      /* full transform */
      mat = tmat * rmat * smat;
      break;
    case VECTOR:
      /* no translation for vectors */
      mat = rmat * smat;
      break;
    case NORMAL:
      /* no translation for normals, and inverse transpose */
      mat = rmat * smat;
      mat = transform_transposed_inverse(mat);
      break;
  }

  /* projection last */
  mat = mat * mmat;

  return mat;
}

bool TextureMapping::skip()
{
  if (translation != make_float3(0.0f, 0.0f, 0.0f))
    return false;
  if (rotation != make_float3(0.0f, 0.0f, 0.0f))
    return false;
  if (scale != make_float3(1.0f, 1.0f, 1.0f))
    return false;

  if (x_mapping != X || y_mapping != Y || z_mapping != Z)
    return false;
  if (use_minmax)
    return false;

  return true;
}

void TextureMapping::compile(SVMCompiler &compiler, int offset_in, int offset_out)
{
  compiler.add_node(NODE_MAPPING, offset_in, offset_out);

  Transform tfm = compute_transform();
  compiler.add_node(tfm.x);
  compiler.add_node(tfm.y);
  compiler.add_node(tfm.z);

  if (use_minmax) {
    compiler.add_node(NODE_MIN_MAX, offset_out, offset_out);
    compiler.add_node(float3_to_float4(min));
    compiler.add_node(float3_to_float4(max));
  }

  if (type == NORMAL) {
    compiler.add_node(NODE_VECTOR_MATH, NODE_VECTOR_MATH_NORMALIZE, offset_out, offset_out);
    compiler.add_node(NODE_VECTOR_MATH, SVM_STACK_INVALID, offset_out);
  }
}

/* Convenience function for texture nodes, allocating stack space to output
 * a modified vector and returning its offset */
int TextureMapping::compile_begin(SVMCompiler &compiler, ShaderInput *vector_in)
{
  if (!skip()) {
    int offset_in = compiler.stack_assign(vector_in);
    int offset_out = compiler.stack_find_offset(SocketType::VECTOR);

    compile(compiler, offset_in, offset_out);

    return offset_out;
  }

  return compiler.stack_assign(vector_in);
}

void TextureMapping::compile_end(SVMCompiler &compiler, ShaderInput *vector_in, int vector_offset)
{
  if (!skip()) {
    compiler.stack_clear_offset(vector_in->type(), vector_offset);
  }
}

void TextureMapping::compile(OSLCompiler &compiler)
{
  if (!skip()) {
    compiler.parameter("mapping", compute_transform());
    compiler.parameter("use_mapping", 1);
  }
}

/* Image Texture */

NODE_DEFINE(ImageTextureNode)
{
  NodeType *type = NodeType::add("image_texture", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(ImageTextureNode);

  SOCKET_STRING(filename, "Filename", ustring());
  SOCKET_STRING(colorspace, "Colorspace", u_colorspace_auto);

  SOCKET_BOOLEAN(use_alpha, "Use Alpha", true);

  static NodeEnum interpolation_enum;
  interpolation_enum.insert("closest", INTERPOLATION_CLOSEST);
  interpolation_enum.insert("linear", INTERPOLATION_LINEAR);
  interpolation_enum.insert("cubic", INTERPOLATION_CUBIC);
  interpolation_enum.insert("smart", INTERPOLATION_SMART);
  SOCKET_ENUM(interpolation, "Interpolation", interpolation_enum, INTERPOLATION_LINEAR);

  static NodeEnum extension_enum;
  extension_enum.insert("periodic", EXTENSION_REPEAT);
  extension_enum.insert("clamp", EXTENSION_EXTEND);
  extension_enum.insert("black", EXTENSION_CLIP);
  SOCKET_ENUM(extension, "Extension", extension_enum, EXTENSION_REPEAT);

  static NodeEnum projection_enum;
  projection_enum.insert("flat", NODE_IMAGE_PROJ_FLAT);
  projection_enum.insert("box", NODE_IMAGE_PROJ_BOX);
  projection_enum.insert("sphere", NODE_IMAGE_PROJ_SPHERE);
  projection_enum.insert("tube", NODE_IMAGE_PROJ_TUBE);
  SOCKET_ENUM(projection, "Projection", projection_enum, NODE_IMAGE_PROJ_FLAT);

  SOCKET_FLOAT(projection_blend, "Projection Blend", 0.0f);

  SOCKET_IN_POINT(vector, "Vector", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_TEXTURE_UV);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(alpha, "Alpha");

  return type;
}

ImageTextureNode::ImageTextureNode() : ImageSlotTextureNode(node_type)
{
  image_manager = NULL;
  slot = -1;
  is_float = -1;
  compress_as_srgb = false;
  colorspace = u_colorspace_raw;
  builtin_data = NULL;
  animated = false;
}

ImageTextureNode::~ImageTextureNode()
{
  if (image_manager) {
    image_manager->remove_image(
        filename.string(), builtin_data, interpolation, extension, use_alpha, colorspace);
  }
}

ShaderNode *ImageTextureNode::clone() const
{
  /* Increase image user count for new node. */
  if (slot != -1) {
    image_manager->add_image_user(slot);
  }
  return new ImageTextureNode(*this);
}

void ImageTextureNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
#ifdef WITH_PTEX
  /* todo: avoid loading other texture coordinates when using ptex,
   * and hide texture coordinate socket in the UI */
  if (shader->has_surface && string_endswith(filename, ".ptx")) {
    /* ptex */
    attributes->add(ATTR_STD_PTEX_FACE_ID);
    attributes->add(ATTR_STD_PTEX_UV);
  }
#endif

  ShaderNode::attributes(shader, attributes);
}

void ImageTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");
  ShaderOutput *color_out = output("Color");
  ShaderOutput *alpha_out = output("Alpha");

  image_manager = compiler.image_manager;
  if (is_float == -1) {
    ImageMetaData metadata;
    slot = image_manager->add_image(filename.string(),
                                    builtin_data,
                                    animated,
                                    0,
                                    interpolation,
                                    extension,
                                    use_alpha,
                                    colorspace,
                                    metadata);
    is_float = metadata.is_float;
    compress_as_srgb = metadata.compress_as_srgb;
    colorspace = metadata.colorspace;
  }

  if (slot != -1) {
    int vector_offset = tex_mapping.compile_begin(compiler, vector_in);
    uint flags = 0;

    if (compress_as_srgb) {
      flags |= NODE_IMAGE_COMPRESS_AS_SRGB;
    }
    if (!alpha_out->links.empty()) {
      flags |= NODE_IMAGE_ALPHA_UNASSOCIATE;
    }

    if (projection != NODE_IMAGE_PROJ_BOX) {
      compiler.add_node(NODE_TEX_IMAGE,
                        slot,
                        compiler.encode_uchar4(vector_offset,
                                               compiler.stack_assign_if_linked(color_out),
                                               compiler.stack_assign_if_linked(alpha_out),
                                               flags),
                        projection);
    }
    else {
      compiler.add_node(NODE_TEX_IMAGE_BOX,
                        slot,
                        compiler.encode_uchar4(vector_offset,
                                               compiler.stack_assign_if_linked(color_out),
                                               compiler.stack_assign_if_linked(alpha_out),
                                               flags),
                        __float_as_int(projection_blend));
    }

    tex_mapping.compile_end(compiler, vector_in, vector_offset);
  }
  else {
    /* image not found */
    if (!color_out->links.empty()) {
      compiler.add_node(NODE_VALUE_V, compiler.stack_assign(color_out));
      compiler.add_node(
          NODE_VALUE_V,
          make_float3(TEX_IMAGE_MISSING_R, TEX_IMAGE_MISSING_G, TEX_IMAGE_MISSING_B));
    }
    if (!alpha_out->links.empty())
      compiler.add_node(
          NODE_VALUE_F, __float_as_int(TEX_IMAGE_MISSING_A), compiler.stack_assign(alpha_out));
  }
}

void ImageTextureNode::compile(OSLCompiler &compiler)
{
  ShaderOutput *alpha_out = output("Alpha");

  tex_mapping.compile(compiler);

  image_manager = compiler.image_manager;
  if (is_float == -1) {
    ImageMetaData metadata;
    if (builtin_data == NULL) {
      image_manager->get_image_metadata(filename.string(), NULL, colorspace, metadata);
    }
    else {
      slot = image_manager->add_image(filename.string(),
                                      builtin_data,
                                      animated,
                                      0,
                                      interpolation,
                                      extension,
                                      use_alpha,
                                      colorspace,
                                      metadata);
    }
    is_float = metadata.is_float;
    compress_as_srgb = metadata.compress_as_srgb;
    colorspace = metadata.colorspace;
  }

  if (slot == -1) {
    compiler.parameter_texture("filename", filename, colorspace);
  }
  else {
    compiler.parameter_texture("filename", slot);
  }

  compiler.parameter(this, "projection");
  compiler.parameter(this, "projection_blend");
  compiler.parameter("convert_from_srgb", compress_as_srgb);
  compiler.parameter("ignore_alpha", !use_alpha);
  compiler.parameter("unassociate_alpha", !alpha_out->links.empty());
  compiler.parameter("is_float", is_float);
  compiler.parameter(this, "interpolation");
  compiler.parameter(this, "extension");

  compiler.add(this, "node_image_texture");
}

/* Environment Texture */

NODE_DEFINE(EnvironmentTextureNode)
{
  NodeType *type = NodeType::add("environment_texture", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(EnvironmentTextureNode);

  SOCKET_STRING(filename, "Filename", ustring());
  SOCKET_STRING(colorspace, "Colorspace", u_colorspace_auto);

  SOCKET_BOOLEAN(use_alpha, "Use Alpha", true);

  static NodeEnum interpolation_enum;
  interpolation_enum.insert("closest", INTERPOLATION_CLOSEST);
  interpolation_enum.insert("linear", INTERPOLATION_LINEAR);
  interpolation_enum.insert("cubic", INTERPOLATION_CUBIC);
  interpolation_enum.insert("smart", INTERPOLATION_SMART);
  SOCKET_ENUM(interpolation, "Interpolation", interpolation_enum, INTERPOLATION_LINEAR);

  static NodeEnum projection_enum;
  projection_enum.insert("equirectangular", NODE_ENVIRONMENT_EQUIRECTANGULAR);
  projection_enum.insert("mirror_ball", NODE_ENVIRONMENT_MIRROR_BALL);
  SOCKET_ENUM(projection, "Projection", projection_enum, NODE_ENVIRONMENT_EQUIRECTANGULAR);

  SOCKET_IN_POINT(vector, "Vector", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_POSITION);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(alpha, "Alpha");

  return type;
}

EnvironmentTextureNode::EnvironmentTextureNode() : ImageSlotTextureNode(node_type)
{
  image_manager = NULL;
  slot = -1;
  is_float = -1;
  compress_as_srgb = false;
  colorspace = u_colorspace_raw;
  builtin_data = NULL;
  animated = false;
}

EnvironmentTextureNode::~EnvironmentTextureNode()
{
  if (image_manager) {
    image_manager->remove_image(
        filename.string(), builtin_data, interpolation, EXTENSION_REPEAT, use_alpha, colorspace);
  }
}

ShaderNode *EnvironmentTextureNode::clone() const
{
  /* Increase image user count for new node. */
  if (slot != -1) {
    image_manager->add_image_user(slot);
  }
  return new EnvironmentTextureNode(*this);
}

void EnvironmentTextureNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
#ifdef WITH_PTEX
  if (shader->has_surface && string_endswith(filename, ".ptx")) {
    /* ptex */
    attributes->add(ATTR_STD_PTEX_FACE_ID);
    attributes->add(ATTR_STD_PTEX_UV);
  }
#endif

  ShaderNode::attributes(shader, attributes);
}

void EnvironmentTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");
  ShaderOutput *color_out = output("Color");
  ShaderOutput *alpha_out = output("Alpha");

  image_manager = compiler.image_manager;
  if (slot == -1) {
    ImageMetaData metadata;
    slot = image_manager->add_image(filename.string(),
                                    builtin_data,
                                    animated,
                                    0,
                                    interpolation,
                                    EXTENSION_REPEAT,
                                    use_alpha,
                                    colorspace,
                                    metadata);
    is_float = metadata.is_float;
    compress_as_srgb = metadata.compress_as_srgb;
    colorspace = metadata.colorspace;
  }

  if (slot != -1) {
    int vector_offset = tex_mapping.compile_begin(compiler, vector_in);
    uint flags = 0;

    if (compress_as_srgb) {
      flags |= NODE_IMAGE_COMPRESS_AS_SRGB;
    }

    compiler.add_node(NODE_TEX_ENVIRONMENT,
                      slot,
                      compiler.encode_uchar4(vector_offset,
                                             compiler.stack_assign_if_linked(color_out),
                                             compiler.stack_assign_if_linked(alpha_out),
                                             flags),
                      projection);

    tex_mapping.compile_end(compiler, vector_in, vector_offset);
  }
  else {
    /* image not found */
    if (!color_out->links.empty()) {
      compiler.add_node(NODE_VALUE_V, compiler.stack_assign(color_out));
      compiler.add_node(
          NODE_VALUE_V,
          make_float3(TEX_IMAGE_MISSING_R, TEX_IMAGE_MISSING_G, TEX_IMAGE_MISSING_B));
    }
    if (!alpha_out->links.empty())
      compiler.add_node(
          NODE_VALUE_F, __float_as_int(TEX_IMAGE_MISSING_A), compiler.stack_assign(alpha_out));
  }
}

void EnvironmentTextureNode::compile(OSLCompiler &compiler)
{
  tex_mapping.compile(compiler);

  /* See comments in ImageTextureNode::compile about support
   * of builtin images.
   */
  image_manager = compiler.image_manager;
  if (is_float == -1) {
    ImageMetaData metadata;
    if (builtin_data == NULL) {
      image_manager->get_image_metadata(filename.string(), NULL, colorspace, metadata);
    }
    else {
      slot = image_manager->add_image(filename.string(),
                                      builtin_data,
                                      animated,
                                      0,
                                      interpolation,
                                      EXTENSION_REPEAT,
                                      use_alpha,
                                      colorspace,
                                      metadata);
    }
    is_float = metadata.is_float;
    compress_as_srgb = metadata.compress_as_srgb;
    colorspace = metadata.colorspace;
  }

  if (slot == -1) {
    compiler.parameter_texture("filename", filename, colorspace);
  }
  else {
    compiler.parameter_texture("filename", slot);
  }

  compiler.parameter(this, "projection");
  compiler.parameter(this, "interpolation");
  compiler.parameter("convert_from_srgb", compress_as_srgb);
  compiler.parameter("ignore_alpha", !use_alpha);
  compiler.parameter("is_float", is_float);
  compiler.add(this, "node_environment_texture");
}

/* Sky Texture */

static float2 sky_spherical_coordinates(float3 dir)
{
  return make_float2(acosf(dir.z), atan2f(dir.x, dir.y));
}

typedef struct SunSky {
  /* sun direction in spherical and cartesian */
  float theta, phi;

  /* Parameter */
  float radiance_x, radiance_y, radiance_z;
  float config_x[9], config_y[9], config_z[9];
} SunSky;

/* Preetham model */
static float sky_perez_function(float lam[6], float theta, float gamma)
{
  return (1.0f + lam[0] * expf(lam[1] / cosf(theta))) *
         (1.0f + lam[2] * expf(lam[3] * gamma) + lam[4] * cosf(gamma) * cosf(gamma));
}

static void sky_texture_precompute_old(SunSky *sunsky, float3 dir, float turbidity)
{
  /*
   * We re-use the SunSky struct of the new model, to avoid extra variables
   * zenith_Y/x/y is now radiance_x/y/z
   * perez_Y/x/y is now config_x/y/z
   */

  float2 spherical = sky_spherical_coordinates(dir);
  float theta = spherical.x;
  float phi = spherical.y;

  sunsky->theta = theta;
  sunsky->phi = phi;

  float theta2 = theta * theta;
  float theta3 = theta2 * theta;
  float T = turbidity;
  float T2 = T * T;

  float chi = (4.0f / 9.0f - T / 120.0f) * (M_PI_F - 2.0f * theta);
  sunsky->radiance_x = (4.0453f * T - 4.9710f) * tanf(chi) - 0.2155f * T + 2.4192f;
  sunsky->radiance_x *= 0.06f;

  sunsky->radiance_y = (0.00166f * theta3 - 0.00375f * theta2 + 0.00209f * theta) * T2 +
                       (-0.02903f * theta3 + 0.06377f * theta2 - 0.03202f * theta + 0.00394f) * T +
                       (0.11693f * theta3 - 0.21196f * theta2 + 0.06052f * theta + 0.25886f);

  sunsky->radiance_z = (0.00275f * theta3 - 0.00610f * theta2 + 0.00317f * theta) * T2 +
                       (-0.04214f * theta3 + 0.08970f * theta2 - 0.04153f * theta + 0.00516f) * T +
                       (0.15346f * theta3 - 0.26756f * theta2 + 0.06670f * theta + 0.26688f);

  sunsky->config_x[0] = (0.1787f * T - 1.4630f);
  sunsky->config_x[1] = (-0.3554f * T + 0.4275f);
  sunsky->config_x[2] = (-0.0227f * T + 5.3251f);
  sunsky->config_x[3] = (0.1206f * T - 2.5771f);
  sunsky->config_x[4] = (-0.0670f * T + 0.3703f);

  sunsky->config_y[0] = (-0.0193f * T - 0.2592f);
  sunsky->config_y[1] = (-0.0665f * T + 0.0008f);
  sunsky->config_y[2] = (-0.0004f * T + 0.2125f);
  sunsky->config_y[3] = (-0.0641f * T - 0.8989f);
  sunsky->config_y[4] = (-0.0033f * T + 0.0452f);

  sunsky->config_z[0] = (-0.0167f * T - 0.2608f);
  sunsky->config_z[1] = (-0.0950f * T + 0.0092f);
  sunsky->config_z[2] = (-0.0079f * T + 0.2102f);
  sunsky->config_z[3] = (-0.0441f * T - 1.6537f);
  sunsky->config_z[4] = (-0.0109f * T + 0.0529f);

  /* unused for old sky model */
  for (int i = 5; i < 9; i++) {
    sunsky->config_x[i] = 0.0f;
    sunsky->config_y[i] = 0.0f;
    sunsky->config_z[i] = 0.0f;
  }

  sunsky->radiance_x /= sky_perez_function(sunsky->config_x, 0, theta);
  sunsky->radiance_y /= sky_perez_function(sunsky->config_y, 0, theta);
  sunsky->radiance_z /= sky_perez_function(sunsky->config_z, 0, theta);
}

/* Hosek / Wilkie */
static void sky_texture_precompute_new(SunSky *sunsky,
                                       float3 dir,
                                       float turbidity,
                                       float ground_albedo)
{
  /* Calculate Sun Direction and save coordinates */
  float2 spherical = sky_spherical_coordinates(dir);
  float theta = spherical.x;
  float phi = spherical.y;

  /* Clamp Turbidity */
  turbidity = clamp(turbidity, 0.0f, 10.0f);

  /* Clamp to Horizon */
  theta = clamp(theta, 0.0f, M_PI_2_F);

  sunsky->theta = theta;
  sunsky->phi = phi;

  float solarElevation = M_PI_2_F - theta;

  /* Initialize Sky Model */
  ArHosekSkyModelState *sky_state;
  sky_state = arhosek_xyz_skymodelstate_alloc_init(
      (double)turbidity, (double)ground_albedo, (double)solarElevation);

  /* Copy values from sky_state to SunSky */
  for (int i = 0; i < 9; ++i) {
    sunsky->config_x[i] = (float)sky_state->configs[0][i];
    sunsky->config_y[i] = (float)sky_state->configs[1][i];
    sunsky->config_z[i] = (float)sky_state->configs[2][i];
  }
  sunsky->radiance_x = (float)sky_state->radiances[0];
  sunsky->radiance_y = (float)sky_state->radiances[1];
  sunsky->radiance_z = (float)sky_state->radiances[2];

  /* Free sky_state */
  arhosekskymodelstate_free(sky_state);
}

NODE_DEFINE(SkyTextureNode)
{
  NodeType *type = NodeType::add("sky_texture", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(SkyTextureNode);

  static NodeEnum type_enum;
  type_enum.insert("preetham", NODE_SKY_OLD);
  type_enum.insert("hosek_wilkie", NODE_SKY_NEW);
  SOCKET_ENUM(type, "Type", type_enum, NODE_SKY_NEW);

  SOCKET_VECTOR(sun_direction, "Sun Direction", make_float3(0.0f, 0.0f, 1.0f));
  SOCKET_FLOAT(turbidity, "Turbidity", 2.2f);
  SOCKET_FLOAT(ground_albedo, "Ground Albedo", 0.3f);

  SOCKET_IN_POINT(
      vector, "Vector", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_TEXTURE_GENERATED);

  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

SkyTextureNode::SkyTextureNode() : TextureNode(node_type)
{
}

void SkyTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");
  ShaderOutput *color_out = output("Color");

  SunSky sunsky;
  if (type == NODE_SKY_OLD)
    sky_texture_precompute_old(&sunsky, sun_direction, turbidity);
  else if (type == NODE_SKY_NEW)
    sky_texture_precompute_new(&sunsky, sun_direction, turbidity, ground_albedo);
  else
    assert(false);

  int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

  compiler.stack_assign(color_out);
  compiler.add_node(NODE_TEX_SKY, vector_offset, compiler.stack_assign(color_out), type);
  compiler.add_node(__float_as_uint(sunsky.phi),
                    __float_as_uint(sunsky.theta),
                    __float_as_uint(sunsky.radiance_x),
                    __float_as_uint(sunsky.radiance_y));
  compiler.add_node(__float_as_uint(sunsky.radiance_z),
                    __float_as_uint(sunsky.config_x[0]),
                    __float_as_uint(sunsky.config_x[1]),
                    __float_as_uint(sunsky.config_x[2]));
  compiler.add_node(__float_as_uint(sunsky.config_x[3]),
                    __float_as_uint(sunsky.config_x[4]),
                    __float_as_uint(sunsky.config_x[5]),
                    __float_as_uint(sunsky.config_x[6]));
  compiler.add_node(__float_as_uint(sunsky.config_x[7]),
                    __float_as_uint(sunsky.config_x[8]),
                    __float_as_uint(sunsky.config_y[0]),
                    __float_as_uint(sunsky.config_y[1]));
  compiler.add_node(__float_as_uint(sunsky.config_y[2]),
                    __float_as_uint(sunsky.config_y[3]),
                    __float_as_uint(sunsky.config_y[4]),
                    __float_as_uint(sunsky.config_y[5]));
  compiler.add_node(__float_as_uint(sunsky.config_y[6]),
                    __float_as_uint(sunsky.config_y[7]),
                    __float_as_uint(sunsky.config_y[8]),
                    __float_as_uint(sunsky.config_z[0]));
  compiler.add_node(__float_as_uint(sunsky.config_z[1]),
                    __float_as_uint(sunsky.config_z[2]),
                    __float_as_uint(sunsky.config_z[3]),
                    __float_as_uint(sunsky.config_z[4]));
  compiler.add_node(__float_as_uint(sunsky.config_z[5]),
                    __float_as_uint(sunsky.config_z[6]),
                    __float_as_uint(sunsky.config_z[7]),
                    __float_as_uint(sunsky.config_z[8]));

  tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void SkyTextureNode::compile(OSLCompiler &compiler)
{
  tex_mapping.compile(compiler);

  SunSky sunsky;
  if (type == NODE_SKY_OLD)
    sky_texture_precompute_old(&sunsky, sun_direction, turbidity);
  else if (type == NODE_SKY_NEW)
    sky_texture_precompute_new(&sunsky, sun_direction, turbidity, ground_albedo);
  else
    assert(false);

  compiler.parameter(this, "type");
  compiler.parameter("theta", sunsky.theta);
  compiler.parameter("phi", sunsky.phi);
  compiler.parameter_color("radiance",
                           make_float3(sunsky.radiance_x, sunsky.radiance_y, sunsky.radiance_z));
  compiler.parameter_array("config_x", sunsky.config_x, 9);
  compiler.parameter_array("config_y", sunsky.config_y, 9);
  compiler.parameter_array("config_z", sunsky.config_z, 9);
  compiler.add(this, "node_sky_texture");
}

/* Gradient Texture */

NODE_DEFINE(GradientTextureNode)
{
  NodeType *type = NodeType::add("gradient_texture", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(GradientTextureNode);

  static NodeEnum type_enum;
  type_enum.insert("linear", NODE_BLEND_LINEAR);
  type_enum.insert("quadratic", NODE_BLEND_QUADRATIC);
  type_enum.insert("easing", NODE_BLEND_EASING);
  type_enum.insert("diagonal", NODE_BLEND_DIAGONAL);
  type_enum.insert("radial", NODE_BLEND_RADIAL);
  type_enum.insert("quadratic_sphere", NODE_BLEND_QUADRATIC_SPHERE);
  type_enum.insert("spherical", NODE_BLEND_SPHERICAL);
  SOCKET_ENUM(type, "Type", type_enum, NODE_BLEND_LINEAR);

  SOCKET_IN_POINT(
      vector, "Vector", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_TEXTURE_GENERATED);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

GradientTextureNode::GradientTextureNode() : TextureNode(node_type)
{
}

void GradientTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");
  ShaderOutput *color_out = output("Color");
  ShaderOutput *fac_out = output("Fac");

  int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

  compiler.add_node(NODE_TEX_GRADIENT,
                    compiler.encode_uchar4(type,
                                           vector_offset,
                                           compiler.stack_assign_if_linked(fac_out),
                                           compiler.stack_assign_if_linked(color_out)));

  tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void GradientTextureNode::compile(OSLCompiler &compiler)
{
  tex_mapping.compile(compiler);

  compiler.parameter(this, "type");
  compiler.add(this, "node_gradient_texture");
}

/* Noise Texture */

NODE_DEFINE(NoiseTextureNode)
{
  NodeType *type = NodeType::add("noise_texture", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(NoiseTextureNode);

  SOCKET_IN_FLOAT(scale, "Scale", 1.0f);
  SOCKET_IN_FLOAT(detail, "Detail", 2.0f);
  SOCKET_IN_FLOAT(distortion, "Distortion", 0.0f);
  SOCKET_IN_POINT(
      vector, "Vector", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_TEXTURE_GENERATED);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

NoiseTextureNode::NoiseTextureNode() : TextureNode(node_type)
{
}

void NoiseTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *distortion_in = input("Distortion");
  ShaderInput *detail_in = input("Detail");
  ShaderInput *scale_in = input("Scale");
  ShaderInput *vector_in = input("Vector");
  ShaderOutput *color_out = output("Color");
  ShaderOutput *fac_out = output("Fac");

  int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

  compiler.add_node(NODE_TEX_NOISE,
                    compiler.encode_uchar4(vector_offset,
                                           compiler.stack_assign_if_linked(scale_in),
                                           compiler.stack_assign_if_linked(detail_in),
                                           compiler.stack_assign_if_linked(distortion_in)),
                    compiler.encode_uchar4(compiler.stack_assign_if_linked(color_out),
                                           compiler.stack_assign_if_linked(fac_out)));
  compiler.add_node(__float_as_int(scale), __float_as_int(detail), __float_as_int(distortion));

  tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void NoiseTextureNode::compile(OSLCompiler &compiler)
{
  tex_mapping.compile(compiler);

  compiler.add(this, "node_noise_texture");
}

/* Voronoi Texture */

NODE_DEFINE(VoronoiTextureNode)
{
  NodeType *type = NodeType::add("voronoi_texture", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(VoronoiTextureNode);

  static NodeEnum coloring_enum;
  coloring_enum.insert("intensity", NODE_VORONOI_INTENSITY);
  coloring_enum.insert("cells", NODE_VORONOI_CELLS);
  SOCKET_ENUM(coloring, "Coloring", coloring_enum, NODE_VORONOI_INTENSITY);

  static NodeEnum metric;
  metric.insert("distance", NODE_VORONOI_DISTANCE);
  metric.insert("manhattan", NODE_VORONOI_MANHATTAN);
  metric.insert("chebychev", NODE_VORONOI_CHEBYCHEV);
  metric.insert("minkowski", NODE_VORONOI_MINKOWSKI);
  SOCKET_ENUM(metric, "Distance Metric", metric, NODE_VORONOI_INTENSITY);

  static NodeEnum feature_enum;
  feature_enum.insert("F1", NODE_VORONOI_F1);
  feature_enum.insert("F2", NODE_VORONOI_F2);
  feature_enum.insert("F3", NODE_VORONOI_F3);
  feature_enum.insert("F4", NODE_VORONOI_F4);
  feature_enum.insert("F2F1", NODE_VORONOI_F2F1);
  SOCKET_ENUM(feature, "Feature", feature_enum, NODE_VORONOI_INTENSITY);

  SOCKET_IN_FLOAT(scale, "Scale", 1.0f);
  SOCKET_IN_FLOAT(exponent, "Exponent", 0.5f);
  SOCKET_IN_POINT(
      vector, "Vector", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_TEXTURE_GENERATED);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

VoronoiTextureNode::VoronoiTextureNode() : TextureNode(node_type)
{
}

void VoronoiTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *scale_in = input("Scale");
  ShaderInput *vector_in = input("Vector");
  ShaderInput *exponent_in = input("Exponent");
  ShaderOutput *color_out = output("Color");
  ShaderOutput *fac_out = output("Fac");

  if (vector_in->link)
    compiler.stack_assign(vector_in);
  if (scale_in->link)
    compiler.stack_assign(scale_in);
  if (exponent_in->link)
    compiler.stack_assign(exponent_in);

  int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

  compiler.add_node(NODE_TEX_VORONOI,
                    compiler.encode_uchar4(vector_offset, coloring, metric, feature),
                    compiler.encode_uchar4(compiler.stack_assign_if_linked(scale_in),
                                           compiler.stack_assign_if_linked(exponent_in),
                                           compiler.stack_assign(fac_out),
                                           compiler.stack_assign(color_out)));
  compiler.add_node(__float_as_int(scale), __float_as_int(exponent));

  tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void VoronoiTextureNode::compile(OSLCompiler &compiler)
{
  tex_mapping.compile(compiler);

  compiler.parameter(this, "coloring");
  compiler.parameter(this, "metric");
  compiler.parameter(this, "feature");
  compiler.add(this, "node_voronoi_texture");
}

/* IES Light */

NODE_DEFINE(IESLightNode)
{
  NodeType *type = NodeType::add("ies_light", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(IESLightNode);

  SOCKET_STRING(ies, "IES", ustring());
  SOCKET_STRING(filename, "File Name", ustring());

  SOCKET_IN_FLOAT(strength, "Strength", 1.0f);
  SOCKET_IN_POINT(
      vector, "Vector", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_TEXTURE_NORMAL);

  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

IESLightNode::IESLightNode() : TextureNode(node_type)
{
  light_manager = NULL;
  slot = -1;
}

ShaderNode *IESLightNode::clone() const
{
  IESLightNode *node = new IESLightNode(*this);

  node->light_manager = NULL;
  node->slot = -1;

  return node;
}

IESLightNode::~IESLightNode()
{
  if (light_manager) {
    light_manager->remove_ies(slot);
  }
}

void IESLightNode::get_slot()
{
  assert(light_manager);

  if (slot == -1) {
    if (ies.empty()) {
      slot = light_manager->add_ies_from_file(filename);
    }
    else {
      slot = light_manager->add_ies(ies);
    }
  }
}

void IESLightNode::compile(SVMCompiler &compiler)
{
  light_manager = compiler.light_manager;
  get_slot();

  ShaderInput *strength_in = input("Strength");
  ShaderInput *vector_in = input("Vector");
  ShaderOutput *fac_out = output("Fac");

  int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

  compiler.add_node(NODE_IES,
                    compiler.encode_uchar4(compiler.stack_assign_if_linked(strength_in),
                                           vector_offset,
                                           compiler.stack_assign(fac_out),
                                           0),
                    slot,
                    __float_as_int(strength));

  tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void IESLightNode::compile(OSLCompiler &compiler)
{
  light_manager = compiler.light_manager;
  get_slot();

  tex_mapping.compile(compiler);

  compiler.parameter_texture_ies("filename", slot);
  compiler.add(this, "node_ies_light");
}

/* Musgrave Texture */

NODE_DEFINE(MusgraveTextureNode)
{
  NodeType *type = NodeType::add("musgrave_texture", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(MusgraveTextureNode);

  static NodeEnum type_enum;
  type_enum.insert("multifractal", NODE_MUSGRAVE_MULTIFRACTAL);
  type_enum.insert("fBM", NODE_MUSGRAVE_FBM);
  type_enum.insert("hybrid_multifractal", NODE_MUSGRAVE_HYBRID_MULTIFRACTAL);
  type_enum.insert("ridged_multifractal", NODE_MUSGRAVE_RIDGED_MULTIFRACTAL);
  type_enum.insert("hetero_terrain", NODE_MUSGRAVE_HETERO_TERRAIN);
  SOCKET_ENUM(type, "Type", type_enum, NODE_MUSGRAVE_FBM);

  SOCKET_IN_FLOAT(scale, "Scale", 1.0f);
  SOCKET_IN_FLOAT(detail, "Detail", 2.0f);
  SOCKET_IN_FLOAT(dimension, "Dimension", 2.0f);
  SOCKET_IN_FLOAT(lacunarity, "Lacunarity", 1.0f);
  SOCKET_IN_FLOAT(offset, "Offset", 0.0f);
  SOCKET_IN_FLOAT(gain, "Gain", 1.0f);
  SOCKET_IN_POINT(
      vector, "Vector", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_TEXTURE_GENERATED);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

MusgraveTextureNode::MusgraveTextureNode() : TextureNode(node_type)
{
}

void MusgraveTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");
  ShaderInput *scale_in = input("Scale");
  ShaderInput *dimension_in = input("Dimension");
  ShaderInput *lacunarity_in = input("Lacunarity");
  ShaderInput *detail_in = input("Detail");
  ShaderInput *offset_in = input("Offset");
  ShaderInput *gain_in = input("Gain");
  ShaderOutput *fac_out = output("Fac");
  ShaderOutput *color_out = output("Color");

  int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

  compiler.add_node(NODE_TEX_MUSGRAVE,
                    compiler.encode_uchar4(type,
                                           vector_offset,
                                           compiler.stack_assign_if_linked(color_out),
                                           compiler.stack_assign_if_linked(fac_out)),
                    compiler.encode_uchar4(compiler.stack_assign_if_linked(dimension_in),
                                           compiler.stack_assign_if_linked(lacunarity_in),
                                           compiler.stack_assign_if_linked(detail_in),
                                           compiler.stack_assign_if_linked(offset_in)),
                    compiler.encode_uchar4(compiler.stack_assign_if_linked(gain_in),
                                           compiler.stack_assign_if_linked(scale_in)));
  compiler.add_node(__float_as_int(dimension),
                    __float_as_int(lacunarity),
                    __float_as_int(detail),
                    __float_as_int(offset));
  compiler.add_node(__float_as_int(gain), __float_as_int(scale));

  tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void MusgraveTextureNode::compile(OSLCompiler &compiler)
{
  tex_mapping.compile(compiler);

  compiler.parameter(this, "type");
  compiler.add(this, "node_musgrave_texture");
}

/* Wave Texture */

NODE_DEFINE(WaveTextureNode)
{
  NodeType *type = NodeType::add("wave_texture", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(WaveTextureNode);

  static NodeEnum type_enum;
  type_enum.insert("bands", NODE_WAVE_BANDS);
  type_enum.insert("rings", NODE_WAVE_RINGS);
  SOCKET_ENUM(type, "Type", type_enum, NODE_WAVE_BANDS);

  static NodeEnum profile_enum;
  profile_enum.insert("sine", NODE_WAVE_PROFILE_SIN);
  profile_enum.insert("saw", NODE_WAVE_PROFILE_SAW);
  SOCKET_ENUM(profile, "Profile", profile_enum, NODE_WAVE_PROFILE_SIN);

  SOCKET_IN_FLOAT(scale, "Scale", 1.0f);
  SOCKET_IN_FLOAT(distortion, "Distortion", 0.0f);
  SOCKET_IN_FLOAT(detail, "Detail", 2.0f);
  SOCKET_IN_FLOAT(detail_scale, "Detail Scale", 0.0f);
  SOCKET_IN_POINT(
      vector, "Vector", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_TEXTURE_GENERATED);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

WaveTextureNode::WaveTextureNode() : TextureNode(node_type)
{
}

void WaveTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *scale_in = input("Scale");
  ShaderInput *distortion_in = input("Distortion");
  ShaderInput *dscale_in = input("Detail Scale");
  ShaderInput *detail_in = input("Detail");
  ShaderInput *vector_in = input("Vector");
  ShaderOutput *fac_out = output("Fac");
  ShaderOutput *color_out = output("Color");

  int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

  compiler.add_node(NODE_TEX_WAVE,
                    compiler.encode_uchar4(type,
                                           compiler.stack_assign_if_linked(color_out),
                                           compiler.stack_assign_if_linked(fac_out),
                                           compiler.stack_assign_if_linked(dscale_in)),
                    compiler.encode_uchar4(vector_offset,
                                           compiler.stack_assign_if_linked(scale_in),
                                           compiler.stack_assign_if_linked(detail_in),
                                           compiler.stack_assign_if_linked(distortion_in)),
                    profile);

  compiler.add_node(__float_as_int(scale),
                    __float_as_int(detail),
                    __float_as_int(distortion),
                    __float_as_int(detail_scale));

  tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void WaveTextureNode::compile(OSLCompiler &compiler)
{
  tex_mapping.compile(compiler);

  compiler.parameter(this, "type");
  compiler.parameter(this, "profile");

  compiler.add(this, "node_wave_texture");
}

/* Magic Texture */

NODE_DEFINE(MagicTextureNode)
{
  NodeType *type = NodeType::add("magic_texture", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(MagicTextureNode);

  SOCKET_INT(depth, "Depth", 2);

  SOCKET_IN_POINT(
      vector, "Vector", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_TEXTURE_GENERATED);
  SOCKET_IN_FLOAT(scale, "Scale", 5.0f);
  SOCKET_IN_FLOAT(distortion, "Distortion", 1.0f);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

MagicTextureNode::MagicTextureNode() : TextureNode(node_type)
{
}

void MagicTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");
  ShaderInput *scale_in = input("Scale");
  ShaderInput *distortion_in = input("Distortion");
  ShaderOutput *color_out = output("Color");
  ShaderOutput *fac_out = output("Fac");

  int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

  compiler.add_node(NODE_TEX_MAGIC,
                    compiler.encode_uchar4(depth,
                                           compiler.stack_assign_if_linked(color_out),
                                           compiler.stack_assign_if_linked(fac_out)),
                    compiler.encode_uchar4(vector_offset,
                                           compiler.stack_assign_if_linked(scale_in),
                                           compiler.stack_assign_if_linked(distortion_in)));
  compiler.add_node(__float_as_int(scale), __float_as_int(distortion));

  tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void MagicTextureNode::compile(OSLCompiler &compiler)
{
  tex_mapping.compile(compiler);

  compiler.parameter(this, "depth");
  compiler.add(this, "node_magic_texture");
}

/* Checker Texture */

NODE_DEFINE(CheckerTextureNode)
{
  NodeType *type = NodeType::add("checker_texture", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(CheckerTextureNode);

  SOCKET_IN_POINT(
      vector, "Vector", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_TEXTURE_GENERATED);
  SOCKET_IN_COLOR(color1, "Color1", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_IN_COLOR(color2, "Color2", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_IN_FLOAT(scale, "Scale", 1.0f);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

CheckerTextureNode::CheckerTextureNode() : TextureNode(node_type)
{
}

void CheckerTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");
  ShaderInput *color1_in = input("Color1");
  ShaderInput *color2_in = input("Color2");
  ShaderInput *scale_in = input("Scale");

  ShaderOutput *color_out = output("Color");
  ShaderOutput *fac_out = output("Fac");

  int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

  compiler.add_node(NODE_TEX_CHECKER,
                    compiler.encode_uchar4(vector_offset,
                                           compiler.stack_assign(color1_in),
                                           compiler.stack_assign(color2_in),
                                           compiler.stack_assign_if_linked(scale_in)),
                    compiler.encode_uchar4(compiler.stack_assign_if_linked(color_out),
                                           compiler.stack_assign_if_linked(fac_out)),
                    __float_as_int(scale));

  tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void CheckerTextureNode::compile(OSLCompiler &compiler)
{
  tex_mapping.compile(compiler);

  compiler.add(this, "node_checker_texture");
}

/* Brick Texture */

NODE_DEFINE(BrickTextureNode)
{
  NodeType *type = NodeType::add("brick_texture", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(BrickTextureNode);

  SOCKET_FLOAT(offset, "Offset", 0.5f);
  SOCKET_INT(offset_frequency, "Offset Frequency", 2);
  SOCKET_FLOAT(squash, "Squash", 1.0f);
  SOCKET_INT(squash_frequency, "Squash Frequency", 2);

  SOCKET_IN_POINT(
      vector, "Vector", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_TEXTURE_GENERATED);

  SOCKET_IN_COLOR(color1, "Color1", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_IN_COLOR(color2, "Color2", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_IN_COLOR(mortar, "Mortar", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_IN_FLOAT(scale, "Scale", 5.0f);
  SOCKET_IN_FLOAT(mortar_size, "Mortar Size", 0.02f);
  SOCKET_IN_FLOAT(mortar_smooth, "Mortar Smooth", 0.0f);
  SOCKET_IN_FLOAT(bias, "Bias", 0.0f);
  SOCKET_IN_FLOAT(brick_width, "Brick Width", 0.5f);
  SOCKET_IN_FLOAT(row_height, "Row Height", 0.25f);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

BrickTextureNode::BrickTextureNode() : TextureNode(node_type)
{
}

void BrickTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");
  ShaderInput *color1_in = input("Color1");
  ShaderInput *color2_in = input("Color2");
  ShaderInput *mortar_in = input("Mortar");
  ShaderInput *scale_in = input("Scale");
  ShaderInput *mortar_size_in = input("Mortar Size");
  ShaderInput *mortar_smooth_in = input("Mortar Smooth");
  ShaderInput *bias_in = input("Bias");
  ShaderInput *brick_width_in = input("Brick Width");
  ShaderInput *row_height_in = input("Row Height");

  ShaderOutput *color_out = output("Color");
  ShaderOutput *fac_out = output("Fac");

  int vector_offset = tex_mapping.compile_begin(compiler, vector_in);

  compiler.add_node(NODE_TEX_BRICK,
                    compiler.encode_uchar4(vector_offset,
                                           compiler.stack_assign(color1_in),
                                           compiler.stack_assign(color2_in),
                                           compiler.stack_assign(mortar_in)),
                    compiler.encode_uchar4(compiler.stack_assign_if_linked(scale_in),
                                           compiler.stack_assign_if_linked(mortar_size_in),
                                           compiler.stack_assign_if_linked(bias_in),
                                           compiler.stack_assign_if_linked(brick_width_in)),
                    compiler.encode_uchar4(compiler.stack_assign_if_linked(row_height_in),
                                           compiler.stack_assign_if_linked(color_out),
                                           compiler.stack_assign_if_linked(fac_out),
                                           compiler.stack_assign_if_linked(mortar_smooth_in)));

  compiler.add_node(compiler.encode_uchar4(offset_frequency, squash_frequency),
                    __float_as_int(scale),
                    __float_as_int(mortar_size),
                    __float_as_int(bias));

  compiler.add_node(__float_as_int(brick_width),
                    __float_as_int(row_height),
                    __float_as_int(offset),
                    __float_as_int(squash));

  compiler.add_node(
      __float_as_int(mortar_smooth), SVM_STACK_INVALID, SVM_STACK_INVALID, SVM_STACK_INVALID);

  tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void BrickTextureNode::compile(OSLCompiler &compiler)
{
  tex_mapping.compile(compiler);

  compiler.parameter(this, "offset");
  compiler.parameter(this, "offset_frequency");
  compiler.parameter(this, "squash");
  compiler.parameter(this, "squash_frequency");
  compiler.add(this, "node_brick_texture");
}

/* Point Density Texture */

NODE_DEFINE(PointDensityTextureNode)
{
  NodeType *type = NodeType::add("point_density_texture", create, NodeType::SHADER);

  SOCKET_STRING(filename, "Filename", ustring());

  static NodeEnum space_enum;
  space_enum.insert("object", NODE_TEX_VOXEL_SPACE_OBJECT);
  space_enum.insert("world", NODE_TEX_VOXEL_SPACE_WORLD);
  SOCKET_ENUM(space, "Space", space_enum, NODE_TEX_VOXEL_SPACE_OBJECT);

  static NodeEnum interpolation_enum;
  interpolation_enum.insert("closest", INTERPOLATION_CLOSEST);
  interpolation_enum.insert("linear", INTERPOLATION_LINEAR);
  interpolation_enum.insert("cubic", INTERPOLATION_CUBIC);
  interpolation_enum.insert("smart", INTERPOLATION_SMART);
  SOCKET_ENUM(interpolation, "Interpolation", interpolation_enum, INTERPOLATION_LINEAR);

  SOCKET_TRANSFORM(tfm, "Transform", transform_identity());

  SOCKET_IN_POINT(vector, "Vector", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_POSITION);

  SOCKET_OUT_FLOAT(density, "Density");
  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

PointDensityTextureNode::PointDensityTextureNode() : ShaderNode(node_type)
{
  image_manager = NULL;
  slot = -1;
  builtin_data = NULL;
}

PointDensityTextureNode::~PointDensityTextureNode()
{
  if (image_manager) {
    image_manager->remove_image(
        filename.string(), builtin_data, interpolation, EXTENSION_CLIP, true, ustring());
  }
}

ShaderNode *PointDensityTextureNode::clone() const
{
  /* Increase image user count for new node. We need to ensure to not call
   * add_image again, to work around access of freed data on the Blender
   * side. A better solution should be found to avoid this. */
  if (slot != -1) {
    image_manager->add_image_user(slot);
  }
  return new PointDensityTextureNode(*this);
}

void PointDensityTextureNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_volume)
    attributes->add(ATTR_STD_GENERATED_TRANSFORM);

  ShaderNode::attributes(shader, attributes);
}

void PointDensityTextureNode::add_image()
{
  if (slot == -1) {
    ImageMetaData metadata;
    slot = image_manager->add_image(filename.string(),
                                    builtin_data,
                                    false,
                                    0,
                                    interpolation,
                                    EXTENSION_CLIP,
                                    true,
                                    u_colorspace_raw,
                                    metadata);
  }
}

void PointDensityTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");
  ShaderOutput *density_out = output("Density");
  ShaderOutput *color_out = output("Color");

  const bool use_density = !density_out->links.empty();
  const bool use_color = !color_out->links.empty();

  image_manager = compiler.image_manager;

  if (use_density || use_color) {
    add_image();

    if (slot != -1) {
      compiler.stack_assign(vector_in);
      compiler.add_node(NODE_TEX_VOXEL,
                        slot,
                        compiler.encode_uchar4(compiler.stack_assign(vector_in),
                                               compiler.stack_assign_if_linked(density_out),
                                               compiler.stack_assign_if_linked(color_out),
                                               space));
      if (space == NODE_TEX_VOXEL_SPACE_WORLD) {
        compiler.add_node(tfm.x);
        compiler.add_node(tfm.y);
        compiler.add_node(tfm.z);
      }
    }
    else {
      if (use_density) {
        compiler.add_node(NODE_VALUE_F, __float_as_int(0.0f), compiler.stack_assign(density_out));
      }
      if (use_color) {
        compiler.add_node(NODE_VALUE_V, compiler.stack_assign(color_out));
        compiler.add_node(
            NODE_VALUE_V,
            make_float3(TEX_IMAGE_MISSING_R, TEX_IMAGE_MISSING_G, TEX_IMAGE_MISSING_B));
      }
    }
  }
}

void PointDensityTextureNode::compile(OSLCompiler &compiler)
{
  ShaderOutput *density_out = output("Density");
  ShaderOutput *color_out = output("Color");

  const bool use_density = !density_out->links.empty();
  const bool use_color = !color_out->links.empty();

  image_manager = compiler.image_manager;

  if (use_density || use_color) {
    add_image();

    compiler.parameter_texture("filename", slot);
    if (space == NODE_TEX_VOXEL_SPACE_WORLD) {
      compiler.parameter("mapping", tfm);
      compiler.parameter("use_mapping", 1);
    }
    compiler.parameter(this, "interpolation");
    compiler.add(this, "node_voxel_texture");
  }
}

/* Normal */

NODE_DEFINE(NormalNode)
{
  NodeType *type = NodeType::add("normal", create, NodeType::SHADER);

  SOCKET_VECTOR(direction, "direction", make_float3(0.0f, 0.0f, 0.0f));

  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f));

  SOCKET_OUT_NORMAL(normal, "Normal");
  SOCKET_OUT_FLOAT(dot, "Dot");

  return type;
}

NormalNode::NormalNode() : ShaderNode(node_type)
{
}

void NormalNode::compile(SVMCompiler &compiler)
{
  ShaderInput *normal_in = input("Normal");
  ShaderOutput *normal_out = output("Normal");
  ShaderOutput *dot_out = output("Dot");

  compiler.add_node(NODE_NORMAL,
                    compiler.stack_assign(normal_in),
                    compiler.stack_assign(normal_out),
                    compiler.stack_assign(dot_out));
  compiler.add_node(
      __float_as_int(direction.x), __float_as_int(direction.y), __float_as_int(direction.z));
}

void NormalNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "direction");
  compiler.add(this, "node_normal");
}

/* Mapping */

NODE_DEFINE(MappingNode)
{
  NodeType *type = NodeType::add("mapping", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(MappingNode);

  SOCKET_IN_POINT(vector, "Vector", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_OUT_POINT(vector, "Vector");

  return type;
}

MappingNode::MappingNode() : ShaderNode(node_type)
{
}

void MappingNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");
  ShaderOutput *vector_out = output("Vector");

  tex_mapping.compile(
      compiler, compiler.stack_assign(vector_in), compiler.stack_assign(vector_out));
}

void MappingNode::compile(OSLCompiler &compiler)
{
  compiler.parameter("Matrix", tex_mapping.compute_transform());
  compiler.parameter_point("mapping_min", tex_mapping.min);
  compiler.parameter_point("mapping_max", tex_mapping.max);
  compiler.parameter("use_minmax", tex_mapping.use_minmax);

  compiler.add(this, "node_mapping");
}

/* RGBToBW */

NODE_DEFINE(RGBToBWNode)
{
  NodeType *type = NodeType::add("rgb_to_bw", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_OUT_FLOAT(val, "Val");

  return type;
}

RGBToBWNode::RGBToBWNode() : ShaderNode(node_type)
{
}

void RGBToBWNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    float val = folder.scene->shader_manager->linear_rgb_to_gray(color);
    folder.make_constant(val);
  }
}

void RGBToBWNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(NODE_CONVERT,
                    NODE_CONVERT_CF,
                    compiler.stack_assign(inputs[0]),
                    compiler.stack_assign(outputs[0]));
}

void RGBToBWNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_rgb_to_bw");
}

/* Convert */

const NodeType *ConvertNode::node_types[ConvertNode::MAX_TYPE][ConvertNode::MAX_TYPE];
bool ConvertNode::initialized = ConvertNode::register_types();

Node *ConvertNode::create(const NodeType *type)
{
  return new ConvertNode(type->inputs[0].type, type->outputs[0].type);
}

bool ConvertNode::register_types()
{
  const int num_types = 8;
  SocketType::Type types[num_types] = {SocketType::FLOAT,
                                       SocketType::INT,
                                       SocketType::COLOR,
                                       SocketType::VECTOR,
                                       SocketType::POINT,
                                       SocketType::NORMAL,
                                       SocketType::STRING,
                                       SocketType::CLOSURE};

  for (size_t i = 0; i < num_types; i++) {
    SocketType::Type from = types[i];
    ustring from_name(SocketType::type_name(from));
    ustring from_value_name("value_" + from_name.string());

    for (size_t j = 0; j < num_types; j++) {
      SocketType::Type to = types[j];
      ustring to_name(SocketType::type_name(to));
      ustring to_value_name("value_" + to_name.string());

      string node_name = "convert_" + from_name.string() + "_to_" + to_name.string();
      NodeType *type = NodeType::add(node_name.c_str(), create, NodeType::SHADER);

      type->register_input(from_value_name,
                           from_value_name,
                           from,
                           SOCKET_OFFSETOF(ConvertNode, value_float),
                           SocketType::zero_default_value(),
                           NULL,
                           NULL,
                           SocketType::LINKABLE);
      type->register_output(to_value_name, to_value_name, to);

      assert(from < MAX_TYPE);
      assert(to < MAX_TYPE);

      node_types[from][to] = type;
    }
  }

  return true;
}

ConvertNode::ConvertNode(SocketType::Type from_, SocketType::Type to_, bool autoconvert)
    : ShaderNode(node_types[from_][to_])
{
  from = from_;
  to = to_;

  if (from == to)
    special_type = SHADER_SPECIAL_TYPE_PROXY;
  else if (autoconvert)
    special_type = SHADER_SPECIAL_TYPE_AUTOCONVERT;
}

void ConvertNode::constant_fold(const ConstantFolder &folder)
{
  /* proxy nodes should have been removed at this point */
  assert(special_type != SHADER_SPECIAL_TYPE_PROXY);

  /* TODO(DingTo): conversion from/to int is not supported yet, don't fold in that case */

  if (folder.all_inputs_constant()) {
    if (from == SocketType::FLOAT) {
      if (SocketType::is_float3(to)) {
        folder.make_constant(make_float3(value_float, value_float, value_float));
      }
    }
    else if (SocketType::is_float3(from)) {
      if (to == SocketType::FLOAT) {
        if (from == SocketType::COLOR) {
          /* color to float */
          float val = folder.scene->shader_manager->linear_rgb_to_gray(value_color);
          folder.make_constant(val);
        }
        else {
          /* vector/point/normal to float */
          folder.make_constant(average(value_vector));
        }
      }
      else if (SocketType::is_float3(to)) {
        folder.make_constant(value_color);
      }
    }
  }
  else {
    ShaderInput *in = inputs[0];
    ShaderNode *prev = in->link->parent;

    /* no-op conversion of A to B to A */
    if (prev->type == node_types[to][from]) {
      ShaderInput *prev_in = prev->inputs[0];

      if (SocketType::is_float3(from) && (to == SocketType::FLOAT || SocketType::is_float3(to)) &&
          prev_in->link) {
        folder.bypass(prev_in->link);
      }
    }
  }
}

void ConvertNode::compile(SVMCompiler &compiler)
{
  /* proxy nodes should have been removed at this point */
  assert(special_type != SHADER_SPECIAL_TYPE_PROXY);

  ShaderInput *in = inputs[0];
  ShaderOutput *out = outputs[0];

  if (from == SocketType::FLOAT) {
    if (to == SocketType::INT)
      /* float to int */
      compiler.add_node(
          NODE_CONVERT, NODE_CONVERT_FI, compiler.stack_assign(in), compiler.stack_assign(out));
    else
      /* float to float3 */
      compiler.add_node(
          NODE_CONVERT, NODE_CONVERT_FV, compiler.stack_assign(in), compiler.stack_assign(out));
  }
  else if (from == SocketType::INT) {
    if (to == SocketType::FLOAT)
      /* int to float */
      compiler.add_node(
          NODE_CONVERT, NODE_CONVERT_IF, compiler.stack_assign(in), compiler.stack_assign(out));
    else
      /* int to vector/point/normal */
      compiler.add_node(
          NODE_CONVERT, NODE_CONVERT_IV, compiler.stack_assign(in), compiler.stack_assign(out));
  }
  else if (to == SocketType::FLOAT) {
    if (from == SocketType::COLOR)
      /* color to float */
      compiler.add_node(
          NODE_CONVERT, NODE_CONVERT_CF, compiler.stack_assign(in), compiler.stack_assign(out));
    else
      /* vector/point/normal to float */
      compiler.add_node(
          NODE_CONVERT, NODE_CONVERT_VF, compiler.stack_assign(in), compiler.stack_assign(out));
  }
  else if (to == SocketType::INT) {
    if (from == SocketType::COLOR)
      /* color to int */
      compiler.add_node(
          NODE_CONVERT, NODE_CONVERT_CI, compiler.stack_assign(in), compiler.stack_assign(out));
    else
      /* vector/point/normal to int */
      compiler.add_node(
          NODE_CONVERT, NODE_CONVERT_VI, compiler.stack_assign(in), compiler.stack_assign(out));
  }
  else {
    /* float3 to float3 */
    if (in->link) {
      /* no op in SVM */
      compiler.stack_link(in, out);
    }
    else {
      /* set 0,0,0 value */
      compiler.add_node(NODE_VALUE_V, compiler.stack_assign(out));
      compiler.add_node(NODE_VALUE_V, value_color);
    }
  }
}

void ConvertNode::compile(OSLCompiler &compiler)
{
  /* proxy nodes should have been removed at this point */
  assert(special_type != SHADER_SPECIAL_TYPE_PROXY);

  if (from == SocketType::FLOAT)
    compiler.add(this, "node_convert_from_float");
  else if (from == SocketType::INT)
    compiler.add(this, "node_convert_from_int");
  else if (from == SocketType::COLOR)
    compiler.add(this, "node_convert_from_color");
  else if (from == SocketType::VECTOR)
    compiler.add(this, "node_convert_from_vector");
  else if (from == SocketType::POINT)
    compiler.add(this, "node_convert_from_point");
  else if (from == SocketType::NORMAL)
    compiler.add(this, "node_convert_from_normal");
  else
    assert(0);
}

/* Base type for all closure-type nodes */

BsdfBaseNode::BsdfBaseNode(const NodeType *node_type) : ShaderNode(node_type)
{
  special_type = SHADER_SPECIAL_TYPE_CLOSURE;
}

bool BsdfBaseNode::has_bump()
{
  /* detect if anything is plugged into the normal input besides the default */
  ShaderInput *normal_in = input("Normal");
  return (normal_in && normal_in->link &&
          normal_in->link->parent->special_type != SHADER_SPECIAL_TYPE_GEOMETRY);
}

/* BSDF Closure */

BsdfNode::BsdfNode(const NodeType *node_type) : BsdfBaseNode(node_type)
{
}

void BsdfNode::compile(SVMCompiler &compiler,
                       ShaderInput *param1,
                       ShaderInput *param2,
                       ShaderInput *param3,
                       ShaderInput *param4)
{
  ShaderInput *color_in = input("Color");
  ShaderInput *normal_in = input("Normal");
  ShaderInput *tangent_in = input("Tangent");

  if (color_in->link)
    compiler.add_node(NODE_CLOSURE_WEIGHT, compiler.stack_assign(color_in));
  else
    compiler.add_node(NODE_CLOSURE_SET_WEIGHT, color);

  int normal_offset = (normal_in) ? compiler.stack_assign_if_linked(normal_in) : SVM_STACK_INVALID;
  int tangent_offset = (tangent_in) ? compiler.stack_assign_if_linked(tangent_in) :
                                      SVM_STACK_INVALID;
  int param3_offset = (param3) ? compiler.stack_assign(param3) : SVM_STACK_INVALID;
  int param4_offset = (param4) ? compiler.stack_assign(param4) : SVM_STACK_INVALID;

  compiler.add_node(
      NODE_CLOSURE_BSDF,
      compiler.encode_uchar4(closure,
                             (param1) ? compiler.stack_assign(param1) : SVM_STACK_INVALID,
                             (param2) ? compiler.stack_assign(param2) : SVM_STACK_INVALID,
                             compiler.closure_mix_weight_offset()),
      __float_as_int((param1) ? get_float(param1->socket_type) : 0.0f),
      __float_as_int((param2) ? get_float(param2->socket_type) : 0.0f));

  compiler.add_node(normal_offset, tangent_offset, param3_offset, param4_offset);
}

void BsdfNode::compile(SVMCompiler &compiler)
{
  compile(compiler, NULL, NULL);
}

void BsdfNode::compile(OSLCompiler & /*compiler*/)
{
  assert(0);
}

/* Anisotropic BSDF Closure */

NODE_DEFINE(AnisotropicBsdfNode)
{
  NodeType *type = NodeType::add("anisotropic_bsdf", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  static NodeEnum distribution_enum;
  distribution_enum.insert("beckmann", CLOSURE_BSDF_MICROFACET_BECKMANN_ANISO_ID);
  distribution_enum.insert("GGX", CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID);
  distribution_enum.insert("Multiscatter GGX", CLOSURE_BSDF_MICROFACET_MULTI_GGX_ANISO_ID);
  distribution_enum.insert("ashikhmin_shirley", CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ANISO_ID);
  SOCKET_ENUM(
      distribution, "Distribution", distribution_enum, CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID);

  SOCKET_IN_VECTOR(tangent, "Tangent", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_TANGENT);

  SOCKET_IN_FLOAT(roughness, "Roughness", 0.5f);
  SOCKET_IN_FLOAT(anisotropy, "Anisotropy", 0.5f);
  SOCKET_IN_FLOAT(rotation, "Rotation", 0.0f);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

AnisotropicBsdfNode::AnisotropicBsdfNode() : BsdfNode(node_type)
{
  closure = CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID;
}

void AnisotropicBsdfNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface) {
    ShaderInput *tangent_in = input("Tangent");

    if (!tangent_in->link)
      attributes->add(ATTR_STD_GENERATED);
  }

  ShaderNode::attributes(shader, attributes);
}

void AnisotropicBsdfNode::compile(SVMCompiler &compiler)
{
  closure = distribution;

  if (closure == CLOSURE_BSDF_MICROFACET_MULTI_GGX_ANISO_ID)
    BsdfNode::compile(
        compiler, input("Roughness"), input("Anisotropy"), input("Rotation"), input("Color"));
  else
    BsdfNode::compile(compiler, input("Roughness"), input("Anisotropy"), input("Rotation"));
}

void AnisotropicBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "distribution");
  compiler.add(this, "node_anisotropic_bsdf");
}

/* Glossy BSDF Closure */

NODE_DEFINE(GlossyBsdfNode)
{
  NodeType *type = NodeType::add("glossy_bsdf", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  static NodeEnum distribution_enum;
  distribution_enum.insert("sharp", CLOSURE_BSDF_REFLECTION_ID);
  distribution_enum.insert("beckmann", CLOSURE_BSDF_MICROFACET_BECKMANN_ID);
  distribution_enum.insert("GGX", CLOSURE_BSDF_MICROFACET_GGX_ID);
  distribution_enum.insert("ashikhmin_shirley", CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID);
  distribution_enum.insert("Multiscatter GGX", CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID);
  SOCKET_ENUM(distribution, "Distribution", distribution_enum, CLOSURE_BSDF_MICROFACET_GGX_ID);
  SOCKET_IN_FLOAT(roughness, "Roughness", 0.5f);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

GlossyBsdfNode::GlossyBsdfNode() : BsdfNode(node_type)
{
  closure = CLOSURE_BSDF_MICROFACET_GGX_ID;
  distribution_orig = NBUILTIN_CLOSURES;
}

void GlossyBsdfNode::simplify_settings(Scene *scene)
{
  if (distribution_orig == NBUILTIN_CLOSURES) {
    roughness_orig = roughness;
    distribution_orig = distribution;
  }
  else {
    /* By default we use original values, so we don't worry about restoring
     * defaults later one and can only do override when needed.
     */
    roughness = roughness_orig;
    distribution = distribution_orig;
  }
  Integrator *integrator = scene->integrator;
  ShaderInput *roughness_input = input("Roughness");
  if (integrator->filter_glossy == 0.0f) {
    /* Fallback to Sharp closure for Roughness close to 0.
     * Note: Keep the epsilon in sync with kernel!
     */
    if (!roughness_input->link && roughness <= 1e-4f) {
      VLOG(1) << "Using sharp glossy BSDF.";
      distribution = CLOSURE_BSDF_REFLECTION_ID;
    }
  }
  else {
    /* If filter glossy is used we replace Sharp glossy with GGX so we can
     * benefit from closure blur to remove unwanted noise.
     */
    if (roughness_input->link == NULL && distribution == CLOSURE_BSDF_REFLECTION_ID) {
      VLOG(1) << "Using GGX glossy with filter glossy.";
      distribution = CLOSURE_BSDF_MICROFACET_GGX_ID;
      roughness = 0.0f;
    }
  }
  closure = distribution;
}

bool GlossyBsdfNode::has_integrator_dependency()
{
  ShaderInput *roughness_input = input("Roughness");
  return !roughness_input->link &&
         (distribution == CLOSURE_BSDF_REFLECTION_ID || roughness <= 1e-4f);
}

void GlossyBsdfNode::compile(SVMCompiler &compiler)
{
  closure = distribution;

  if (closure == CLOSURE_BSDF_REFLECTION_ID)
    BsdfNode::compile(compiler, NULL, NULL);
  else if (closure == CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID)
    BsdfNode::compile(compiler, input("Roughness"), NULL, input("Color"));
  else
    BsdfNode::compile(compiler, input("Roughness"), NULL);
}

void GlossyBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "distribution");
  compiler.add(this, "node_glossy_bsdf");
}

/* Glass BSDF Closure */

NODE_DEFINE(GlassBsdfNode)
{
  NodeType *type = NodeType::add("glass_bsdf", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  static NodeEnum distribution_enum;
  distribution_enum.insert("sharp", CLOSURE_BSDF_SHARP_GLASS_ID);
  distribution_enum.insert("beckmann", CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID);
  distribution_enum.insert("GGX", CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID);
  distribution_enum.insert("Multiscatter GGX", CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID);
  SOCKET_ENUM(
      distribution, "Distribution", distribution_enum, CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID);
  SOCKET_IN_FLOAT(roughness, "Roughness", 0.0f);
  SOCKET_IN_FLOAT(IOR, "IOR", 0.3f);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

GlassBsdfNode::GlassBsdfNode() : BsdfNode(node_type)
{
  closure = CLOSURE_BSDF_SHARP_GLASS_ID;
  distribution_orig = NBUILTIN_CLOSURES;
}

void GlassBsdfNode::simplify_settings(Scene *scene)
{
  if (distribution_orig == NBUILTIN_CLOSURES) {
    roughness_orig = roughness;
    distribution_orig = distribution;
  }
  else {
    /* By default we use original values, so we don't worry about restoring
     * defaults later one and can only do override when needed.
     */
    roughness = roughness_orig;
    distribution = distribution_orig;
  }
  Integrator *integrator = scene->integrator;
  ShaderInput *roughness_input = input("Roughness");
  if (integrator->filter_glossy == 0.0f) {
    /* Fallback to Sharp closure for Roughness close to 0.
     * Note: Keep the epsilon in sync with kernel!
     */
    if (!roughness_input->link && roughness <= 1e-4f) {
      VLOG(1) << "Using sharp glass BSDF.";
      distribution = CLOSURE_BSDF_SHARP_GLASS_ID;
    }
  }
  else {
    /* If filter glossy is used we replace Sharp glossy with GGX so we can
     * benefit from closure blur to remove unwanted noise.
     */
    if (roughness_input->link == NULL && distribution == CLOSURE_BSDF_SHARP_GLASS_ID) {
      VLOG(1) << "Using GGX glass with filter glossy.";
      distribution = CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID;
      roughness = 0.0f;
    }
  }
  closure = distribution;
}

bool GlassBsdfNode::has_integrator_dependency()
{
  ShaderInput *roughness_input = input("Roughness");
  return !roughness_input->link &&
         (distribution == CLOSURE_BSDF_SHARP_GLASS_ID || roughness <= 1e-4f);
}

void GlassBsdfNode::compile(SVMCompiler &compiler)
{
  closure = distribution;

  if (closure == CLOSURE_BSDF_SHARP_GLASS_ID)
    BsdfNode::compile(compiler, NULL, input("IOR"));
  else if (closure == CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID)
    BsdfNode::compile(compiler, input("Roughness"), input("IOR"), input("Color"));
  else
    BsdfNode::compile(compiler, input("Roughness"), input("IOR"));
}

void GlassBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "distribution");
  compiler.add(this, "node_glass_bsdf");
}

/* Refraction BSDF Closure */

NODE_DEFINE(RefractionBsdfNode)
{
  NodeType *type = NodeType::add("refraction_bsdf", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  static NodeEnum distribution_enum;
  distribution_enum.insert("sharp", CLOSURE_BSDF_REFRACTION_ID);
  distribution_enum.insert("beckmann", CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID);
  distribution_enum.insert("GGX", CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
  SOCKET_ENUM(
      distribution, "Distribution", distribution_enum, CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);

  SOCKET_IN_FLOAT(roughness, "Roughness", 0.0f);
  SOCKET_IN_FLOAT(IOR, "IOR", 0.3f);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

RefractionBsdfNode::RefractionBsdfNode() : BsdfNode(node_type)
{
  closure = CLOSURE_BSDF_REFRACTION_ID;
  distribution_orig = NBUILTIN_CLOSURES;
}

void RefractionBsdfNode::simplify_settings(Scene *scene)
{
  if (distribution_orig == NBUILTIN_CLOSURES) {
    roughness_orig = roughness;
    distribution_orig = distribution;
  }
  else {
    /* By default we use original values, so we don't worry about restoring
     * defaults later one and can only do override when needed.
     */
    roughness = roughness_orig;
    distribution = distribution_orig;
  }
  Integrator *integrator = scene->integrator;
  ShaderInput *roughness_input = input("Roughness");
  if (integrator->filter_glossy == 0.0f) {
    /* Fallback to Sharp closure for Roughness close to 0.
     * Note: Keep the epsilon in sync with kernel!
     */
    if (!roughness_input->link && roughness <= 1e-4f) {
      VLOG(1) << "Using sharp refraction BSDF.";
      distribution = CLOSURE_BSDF_REFRACTION_ID;
    }
  }
  else {
    /* If filter glossy is used we replace Sharp glossy with GGX so we can
     * benefit from closure blur to remove unwanted noise.
     */
    if (roughness_input->link == NULL && distribution == CLOSURE_BSDF_REFRACTION_ID) {
      VLOG(1) << "Using GGX refraction with filter glossy.";
      distribution = CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
      roughness = 0.0f;
    }
  }
  closure = distribution;
}

bool RefractionBsdfNode::has_integrator_dependency()
{
  ShaderInput *roughness_input = input("Roughness");
  return !roughness_input->link &&
         (distribution == CLOSURE_BSDF_REFRACTION_ID || roughness <= 1e-4f);
}

void RefractionBsdfNode::compile(SVMCompiler &compiler)
{
  closure = distribution;

  if (closure == CLOSURE_BSDF_REFRACTION_ID)
    BsdfNode::compile(compiler, NULL, input("IOR"));
  else
    BsdfNode::compile(compiler, input("Roughness"), input("IOR"));
}

void RefractionBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "distribution");
  compiler.add(this, "node_refraction_bsdf");
}

/* Toon BSDF Closure */

NODE_DEFINE(ToonBsdfNode)
{
  NodeType *type = NodeType::add("toon_bsdf", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  static NodeEnum component_enum;
  component_enum.insert("diffuse", CLOSURE_BSDF_DIFFUSE_TOON_ID);
  component_enum.insert("glossy", CLOSURE_BSDF_GLOSSY_TOON_ID);
  SOCKET_ENUM(component, "Component", component_enum, CLOSURE_BSDF_DIFFUSE_TOON_ID);
  SOCKET_IN_FLOAT(size, "Size", 0.5f);
  SOCKET_IN_FLOAT(smooth, "Smooth", 0.0f);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

ToonBsdfNode::ToonBsdfNode() : BsdfNode(node_type)
{
  closure = CLOSURE_BSDF_DIFFUSE_TOON_ID;
}

void ToonBsdfNode::compile(SVMCompiler &compiler)
{
  closure = component;

  BsdfNode::compile(compiler, input("Size"), input("Smooth"));
}

void ToonBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "component");
  compiler.add(this, "node_toon_bsdf");
}

/* Velvet BSDF Closure */

NODE_DEFINE(VelvetBsdfNode)
{
  NodeType *type = NodeType::add("velvet_bsdf", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);
  SOCKET_IN_FLOAT(sigma, "Sigma", 1.0f);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

VelvetBsdfNode::VelvetBsdfNode() : BsdfNode(node_type)
{
  closure = CLOSURE_BSDF_ASHIKHMIN_VELVET_ID;
}

void VelvetBsdfNode::compile(SVMCompiler &compiler)
{
  BsdfNode::compile(compiler, input("Sigma"), NULL);
}

void VelvetBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_velvet_bsdf");
}

/* Diffuse BSDF Closure */

NODE_DEFINE(DiffuseBsdfNode)
{
  NodeType *type = NodeType::add("diffuse_bsdf", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);
  SOCKET_IN_FLOAT(roughness, "Roughness", 0.0f);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

DiffuseBsdfNode::DiffuseBsdfNode() : BsdfNode(node_type)
{
  closure = CLOSURE_BSDF_DIFFUSE_ID;
}

void DiffuseBsdfNode::compile(SVMCompiler &compiler)
{
  BsdfNode::compile(compiler, input("Roughness"), NULL);
}

void DiffuseBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_diffuse_bsdf");
}

/* Disney principled BSDF Closure */
NODE_DEFINE(PrincipledBsdfNode)
{
  NodeType *type = NodeType::add("principled_bsdf", create, NodeType::SHADER);

  static NodeEnum distribution_enum;
  distribution_enum.insert("GGX", CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID);
  distribution_enum.insert("Multiscatter GGX", CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID);
  SOCKET_ENUM(
      distribution, "Distribution", distribution_enum, CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID);

  static NodeEnum subsurface_method_enum;
  subsurface_method_enum.insert("burley", CLOSURE_BSSRDF_PRINCIPLED_ID);
  subsurface_method_enum.insert("random_walk", CLOSURE_BSSRDF_PRINCIPLED_RANDOM_WALK_ID);
  SOCKET_ENUM(subsurface_method,
              "Subsurface Method",
              subsurface_method_enum,
              CLOSURE_BSSRDF_PRINCIPLED_ID);

  SOCKET_IN_COLOR(base_color, "Base Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_COLOR(subsurface_color, "Subsurface Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_FLOAT(metallic, "Metallic", 0.0f);
  SOCKET_IN_FLOAT(subsurface, "Subsurface", 0.0f);
  SOCKET_IN_VECTOR(subsurface_radius, "Subsurface Radius", make_float3(0.1f, 0.1f, 0.1f));
  SOCKET_IN_FLOAT(specular, "Specular", 0.0f);
  SOCKET_IN_FLOAT(roughness, "Roughness", 0.5f);
  SOCKET_IN_FLOAT(specular_tint, "Specular Tint", 0.0f);
  SOCKET_IN_FLOAT(anisotropic, "Anisotropic", 0.0f);
  SOCKET_IN_FLOAT(sheen, "Sheen", 0.0f);
  SOCKET_IN_FLOAT(sheen_tint, "Sheen Tint", 0.0f);
  SOCKET_IN_FLOAT(clearcoat, "Clearcoat", 0.0f);
  SOCKET_IN_FLOAT(clearcoat_roughness, "Clearcoat Roughness", 0.03f);
  SOCKET_IN_FLOAT(ior, "IOR", 0.0f);
  SOCKET_IN_FLOAT(transmission, "Transmission", 0.0f);
  SOCKET_IN_FLOAT(transmission_roughness, "Transmission Roughness", 0.0f);
  SOCKET_IN_FLOAT(anisotropic_rotation, "Anisotropic Rotation", 0.0f);
  SOCKET_IN_COLOR(emission, "Emission", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_IN_FLOAT(alpha, "Alpha", 1.0f);
  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_NORMAL);
  SOCKET_IN_NORMAL(clearcoat_normal,
                   "Clearcoat Normal",
                   make_float3(0.0f, 0.0f, 0.0f),
                   SocketType::LINK_NORMAL);
  SOCKET_IN_NORMAL(tangent, "Tangent", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_TANGENT);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

PrincipledBsdfNode::PrincipledBsdfNode() : BsdfBaseNode(node_type)
{
  closure = CLOSURE_BSDF_PRINCIPLED_ID;
  distribution = CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID;
  distribution_orig = NBUILTIN_CLOSURES;
}

void PrincipledBsdfNode::expand(ShaderGraph *graph)
{
  ShaderOutput *principled_out = output("BSDF");

  ShaderInput *emission_in = input("Emission");
  if (emission_in->link || emission != make_float3(0.0f, 0.0f, 0.0f)) {
    /* Create add closure and emission. */
    AddClosureNode *add = new AddClosureNode();
    EmissionNode *emission_node = new EmissionNode();
    ShaderOutput *new_out = add->output("Closure");

    graph->add(add);
    graph->add(emission_node);

    emission_node->strength = 1.0f;
    graph->relink(emission_in, emission_node->input("Color"));
    graph->relink(principled_out, new_out);
    graph->connect(emission_node->output("Emission"), add->input("Closure1"));
    graph->connect(principled_out, add->input("Closure2"));

    principled_out = new_out;
  }

  ShaderInput *alpha_in = input("Alpha");
  if (alpha_in->link || alpha != 1.0f) {
    /* Create mix and transparent BSDF for alpha transparency. */
    MixClosureNode *mix = new MixClosureNode();
    TransparentBsdfNode *transparent = new TransparentBsdfNode();

    graph->add(mix);
    graph->add(transparent);

    graph->relink(alpha_in, mix->input("Fac"));
    graph->relink(principled_out, mix->output("Closure"));
    graph->connect(transparent->output("BSDF"), mix->input("Closure1"));
    graph->connect(principled_out, mix->input("Closure2"));
  }

  remove_input(emission_in);
  remove_input(alpha_in);
}

bool PrincipledBsdfNode::has_surface_bssrdf()
{
  ShaderInput *subsurface_in = input("Subsurface");
  return (subsurface_in->link != NULL || subsurface > CLOSURE_WEIGHT_CUTOFF);
}

void PrincipledBsdfNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface) {
    ShaderInput *tangent_in = input("Tangent");

    if (!tangent_in->link)
      attributes->add(ATTR_STD_GENERATED);
  }

  ShaderNode::attributes(shader, attributes);
}

void PrincipledBsdfNode::compile(SVMCompiler &compiler,
                                 ShaderInput *p_metallic,
                                 ShaderInput *p_subsurface,
                                 ShaderInput *p_subsurface_radius,
                                 ShaderInput *p_specular,
                                 ShaderInput *p_roughness,
                                 ShaderInput *p_specular_tint,
                                 ShaderInput *p_anisotropic,
                                 ShaderInput *p_sheen,
                                 ShaderInput *p_sheen_tint,
                                 ShaderInput *p_clearcoat,
                                 ShaderInput *p_clearcoat_roughness,
                                 ShaderInput *p_ior,
                                 ShaderInput *p_transmission,
                                 ShaderInput *p_anisotropic_rotation,
                                 ShaderInput *p_transmission_roughness)
{
  ShaderInput *base_color_in = input("Base Color");
  ShaderInput *subsurface_color_in = input("Subsurface Color");
  ShaderInput *normal_in = input("Normal");
  ShaderInput *clearcoat_normal_in = input("Clearcoat Normal");
  ShaderInput *tangent_in = input("Tangent");

  float3 weight = make_float3(1.0f, 1.0f, 1.0f);

  compiler.add_node(NODE_CLOSURE_SET_WEIGHT, weight);

  int normal_offset = compiler.stack_assign_if_linked(normal_in);
  int clearcoat_normal_offset = compiler.stack_assign_if_linked(clearcoat_normal_in);
  int tangent_offset = compiler.stack_assign_if_linked(tangent_in);
  int specular_offset = compiler.stack_assign(p_specular);
  int roughness_offset = compiler.stack_assign(p_roughness);
  int specular_tint_offset = compiler.stack_assign(p_specular_tint);
  int anisotropic_offset = compiler.stack_assign(p_anisotropic);
  int sheen_offset = compiler.stack_assign(p_sheen);
  int sheen_tint_offset = compiler.stack_assign(p_sheen_tint);
  int clearcoat_offset = compiler.stack_assign(p_clearcoat);
  int clearcoat_roughness_offset = compiler.stack_assign(p_clearcoat_roughness);
  int ior_offset = compiler.stack_assign(p_ior);
  int transmission_offset = compiler.stack_assign(p_transmission);
  int transmission_roughness_offset = compiler.stack_assign(p_transmission_roughness);
  int anisotropic_rotation_offset = compiler.stack_assign(p_anisotropic_rotation);
  int subsurface_radius_offset = compiler.stack_assign(p_subsurface_radius);

  compiler.add_node(NODE_CLOSURE_BSDF,
                    compiler.encode_uchar4(closure,
                                           compiler.stack_assign(p_metallic),
                                           compiler.stack_assign(p_subsurface),
                                           compiler.closure_mix_weight_offset()),
                    __float_as_int((p_metallic) ? get_float(p_metallic->socket_type) : 0.0f),
                    __float_as_int((p_subsurface) ? get_float(p_subsurface->socket_type) : 0.0f));

  compiler.add_node(
      normal_offset,
      tangent_offset,
      compiler.encode_uchar4(
          specular_offset, roughness_offset, specular_tint_offset, anisotropic_offset),
      compiler.encode_uchar4(
          sheen_offset, sheen_tint_offset, clearcoat_offset, clearcoat_roughness_offset));

  compiler.add_node(compiler.encode_uchar4(ior_offset,
                                           transmission_offset,
                                           anisotropic_rotation_offset,
                                           transmission_roughness_offset),
                    distribution,
                    subsurface_method,
                    SVM_STACK_INVALID);

  float3 bc_default = get_float3(base_color_in->socket_type);

  compiler.add_node(
      ((base_color_in->link) ? compiler.stack_assign(base_color_in) : SVM_STACK_INVALID),
      __float_as_int(bc_default.x),
      __float_as_int(bc_default.y),
      __float_as_int(bc_default.z));

  compiler.add_node(
      clearcoat_normal_offset, subsurface_radius_offset, SVM_STACK_INVALID, SVM_STACK_INVALID);

  float3 ss_default = get_float3(subsurface_color_in->socket_type);

  compiler.add_node(((subsurface_color_in->link) ? compiler.stack_assign(subsurface_color_in) :
                                                   SVM_STACK_INVALID),
                    __float_as_int(ss_default.x),
                    __float_as_int(ss_default.y),
                    __float_as_int(ss_default.z));
}

bool PrincipledBsdfNode::has_integrator_dependency()
{
  ShaderInput *roughness_input = input("Roughness");
  return !roughness_input->link && roughness <= 1e-4f;
}

void PrincipledBsdfNode::compile(SVMCompiler &compiler)
{
  compile(compiler,
          input("Metallic"),
          input("Subsurface"),
          input("Subsurface Radius"),
          input("Specular"),
          input("Roughness"),
          input("Specular Tint"),
          input("Anisotropic"),
          input("Sheen"),
          input("Sheen Tint"),
          input("Clearcoat"),
          input("Clearcoat Roughness"),
          input("IOR"),
          input("Transmission"),
          input("Anisotropic Rotation"),
          input("Transmission Roughness"));
}

void PrincipledBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "distribution");
  compiler.parameter(this, "subsurface_method");
  compiler.add(this, "node_principled_bsdf");
}

bool PrincipledBsdfNode::has_bssrdf_bump()
{
  return has_surface_bssrdf() && has_bump();
}

/* Translucent BSDF Closure */

NODE_DEFINE(TranslucentBsdfNode)
{
  NodeType *type = NodeType::add("translucent_bsdf", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

TranslucentBsdfNode::TranslucentBsdfNode() : BsdfNode(node_type)
{
  closure = CLOSURE_BSDF_TRANSLUCENT_ID;
}

void TranslucentBsdfNode::compile(SVMCompiler &compiler)
{
  BsdfNode::compile(compiler, NULL, NULL);
}

void TranslucentBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_translucent_bsdf");
}

/* Transparent BSDF Closure */

NODE_DEFINE(TransparentBsdfNode)
{
  NodeType *type = NodeType::add("transparent_bsdf", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(1.0f, 1.0f, 1.0f));
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

TransparentBsdfNode::TransparentBsdfNode() : BsdfNode(node_type)
{
  closure = CLOSURE_BSDF_TRANSPARENT_ID;
}

void TransparentBsdfNode::compile(SVMCompiler &compiler)
{
  BsdfNode::compile(compiler, NULL, NULL);
}

void TransparentBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_transparent_bsdf");
}

/* Subsurface Scattering Closure */

NODE_DEFINE(SubsurfaceScatteringNode)
{
  NodeType *type = NodeType::add("subsurface_scattering", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  static NodeEnum falloff_enum;
  falloff_enum.insert("cubic", CLOSURE_BSSRDF_CUBIC_ID);
  falloff_enum.insert("gaussian", CLOSURE_BSSRDF_GAUSSIAN_ID);
  falloff_enum.insert("burley", CLOSURE_BSSRDF_BURLEY_ID);
  falloff_enum.insert("random_walk", CLOSURE_BSSRDF_RANDOM_WALK_ID);
  SOCKET_ENUM(falloff, "Falloff", falloff_enum, CLOSURE_BSSRDF_BURLEY_ID);
  SOCKET_IN_FLOAT(scale, "Scale", 0.01f);
  SOCKET_IN_VECTOR(radius, "Radius", make_float3(0.1f, 0.1f, 0.1f));
  SOCKET_IN_FLOAT(sharpness, "Sharpness", 0.0f);
  SOCKET_IN_FLOAT(texture_blur, "Texture Blur", 1.0f);

  SOCKET_OUT_CLOSURE(BSSRDF, "BSSRDF");

  return type;
}

SubsurfaceScatteringNode::SubsurfaceScatteringNode() : BsdfNode(node_type)
{
  closure = falloff;
}

void SubsurfaceScatteringNode::compile(SVMCompiler &compiler)
{
  closure = falloff;
  BsdfNode::compile(
      compiler, input("Scale"), input("Texture Blur"), input("Radius"), input("Sharpness"));
}

void SubsurfaceScatteringNode::compile(OSLCompiler &compiler)
{
  closure = falloff;
  compiler.parameter(this, "falloff");
  compiler.add(this, "node_subsurface_scattering");
}

bool SubsurfaceScatteringNode::has_bssrdf_bump()
{
  /* detect if anything is plugged into the normal input besides the default */
  ShaderInput *normal_in = input("Normal");
  return (normal_in->link &&
          normal_in->link->parent->special_type != SHADER_SPECIAL_TYPE_GEOMETRY);
}

/* Emissive Closure */

NODE_DEFINE(EmissionNode)
{
  NodeType *type = NodeType::add("emission", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_FLOAT(strength, "Strength", 10.0f);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(emission, "Emission");

  return type;
}

EmissionNode::EmissionNode() : ShaderNode(node_type)
{
}

void EmissionNode::compile(SVMCompiler &compiler)
{
  ShaderInput *color_in = input("Color");
  ShaderInput *strength_in = input("Strength");

  if (color_in->link || strength_in->link) {
    compiler.add_node(
        NODE_EMISSION_WEIGHT, compiler.stack_assign(color_in), compiler.stack_assign(strength_in));
  }
  else
    compiler.add_node(NODE_CLOSURE_SET_WEIGHT, color * strength);

  compiler.add_node(NODE_CLOSURE_EMISSION, compiler.closure_mix_weight_offset());
}

void EmissionNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_emission");
}

void EmissionNode::constant_fold(const ConstantFolder &folder)
{
  ShaderInput *color_in = input("Color");
  ShaderInput *strength_in = input("Strength");

  if ((!color_in->link && color == make_float3(0.0f, 0.0f, 0.0f)) ||
      (!strength_in->link && strength == 0.0f)) {
    folder.discard();
  }
}

/* Background Closure */

NODE_DEFINE(BackgroundNode)
{
  NodeType *type = NodeType::add("background_shader", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_FLOAT(strength, "Strength", 1.0f);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(background, "Background");

  return type;
}

BackgroundNode::BackgroundNode() : ShaderNode(node_type)
{
}

void BackgroundNode::compile(SVMCompiler &compiler)
{
  ShaderInput *color_in = input("Color");
  ShaderInput *strength_in = input("Strength");

  if (color_in->link || strength_in->link) {
    compiler.add_node(
        NODE_EMISSION_WEIGHT, compiler.stack_assign(color_in), compiler.stack_assign(strength_in));
  }
  else
    compiler.add_node(NODE_CLOSURE_SET_WEIGHT, color * strength);

  compiler.add_node(NODE_CLOSURE_BACKGROUND, compiler.closure_mix_weight_offset());
}

void BackgroundNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_background");
}

void BackgroundNode::constant_fold(const ConstantFolder &folder)
{
  ShaderInput *color_in = input("Color");
  ShaderInput *strength_in = input("Strength");

  if ((!color_in->link && color == make_float3(0.0f, 0.0f, 0.0f)) ||
      (!strength_in->link && strength == 0.0f)) {
    folder.discard();
  }
}

/* Holdout Closure */

NODE_DEFINE(HoldoutNode)
{
  NodeType *type = NodeType::add("holdout", create, NodeType::SHADER);

  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);
  SOCKET_IN_FLOAT(volume_mix_weight, "VolumeMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(holdout, "Holdout");

  return type;
}

HoldoutNode::HoldoutNode() : ShaderNode(node_type)
{
}

void HoldoutNode::compile(SVMCompiler &compiler)
{
  float3 value = make_float3(1.0f, 1.0f, 1.0f);

  compiler.add_node(NODE_CLOSURE_SET_WEIGHT, value);
  compiler.add_node(NODE_CLOSURE_HOLDOUT, compiler.closure_mix_weight_offset());
}

void HoldoutNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_holdout");
}

/* Ambient Occlusion */

NODE_DEFINE(AmbientOcclusionNode)
{
  NodeType *type = NodeType::add("ambient_occlusion", create, NodeType::SHADER);

  SOCKET_INT(samples, "Samples", 16);

  SOCKET_IN_COLOR(color, "Color", make_float3(1.0f, 1.0f, 1.0f));
  SOCKET_IN_FLOAT(distance, "Distance", 1.0f);
  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_NORMAL);

  SOCKET_BOOLEAN(inside, "Inside", false);
  SOCKET_BOOLEAN(only_local, "Only Local", false);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(ao, "AO");

  return type;
}

AmbientOcclusionNode::AmbientOcclusionNode() : ShaderNode(node_type)
{
}

void AmbientOcclusionNode::compile(SVMCompiler &compiler)
{
  ShaderInput *color_in = input("Color");
  ShaderInput *distance_in = input("Distance");
  ShaderInput *normal_in = input("Normal");
  ShaderOutput *color_out = output("Color");
  ShaderOutput *ao_out = output("AO");

  int flags = (inside ? NODE_AO_INSIDE : 0) | (only_local ? NODE_AO_ONLY_LOCAL : 0);

  if (!distance_in->link && distance == 0.0f) {
    flags |= NODE_AO_GLOBAL_RADIUS;
  }

  compiler.add_node(NODE_AMBIENT_OCCLUSION,
                    compiler.encode_uchar4(flags,
                                           compiler.stack_assign_if_linked(distance_in),
                                           compiler.stack_assign_if_linked(normal_in),
                                           compiler.stack_assign(ao_out)),
                    compiler.encode_uchar4(compiler.stack_assign(color_in),
                                           compiler.stack_assign(color_out),
                                           samples),
                    __float_as_uint(distance));
}

void AmbientOcclusionNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "samples");
  compiler.parameter(this, "inside");
  compiler.parameter(this, "only_local");
  compiler.add(this, "node_ambient_occlusion");
}

/* Volume Closure */

VolumeNode::VolumeNode(const NodeType *node_type) : ShaderNode(node_type)
{
  closure = CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID;
}

void VolumeNode::compile(SVMCompiler &compiler, ShaderInput *param1, ShaderInput *param2)
{
  ShaderInput *color_in = input("Color");

  if (color_in->link)
    compiler.add_node(NODE_CLOSURE_WEIGHT, compiler.stack_assign(color_in));
  else
    compiler.add_node(NODE_CLOSURE_SET_WEIGHT, color);

  compiler.add_node(
      NODE_CLOSURE_VOLUME,
      compiler.encode_uchar4(closure,
                             (param1) ? compiler.stack_assign(param1) : SVM_STACK_INVALID,
                             (param2) ? compiler.stack_assign(param2) : SVM_STACK_INVALID,
                             compiler.closure_mix_weight_offset()),
      __float_as_int((param1) ? get_float(param1->socket_type) : 0.0f),
      __float_as_int((param2) ? get_float(param2->socket_type) : 0.0f));
}

void VolumeNode::compile(SVMCompiler &compiler)
{
  compile(compiler, NULL, NULL);
}

void VolumeNode::compile(OSLCompiler & /*compiler*/)
{
  assert(0);
}

/* Absorption Volume Closure */

NODE_DEFINE(AbsorptionVolumeNode)
{
  NodeType *type = NodeType::add("absorption_volume", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_FLOAT(density, "Density", 1.0f);
  SOCKET_IN_FLOAT(volume_mix_weight, "VolumeMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(volume, "Volume");

  return type;
}

AbsorptionVolumeNode::AbsorptionVolumeNode() : VolumeNode(node_type)
{
  closure = CLOSURE_VOLUME_ABSORPTION_ID;
}

void AbsorptionVolumeNode::compile(SVMCompiler &compiler)
{
  VolumeNode::compile(compiler, input("Density"), NULL);
}

void AbsorptionVolumeNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_absorption_volume");
}

/* Scatter Volume Closure */

NODE_DEFINE(ScatterVolumeNode)
{
  NodeType *type = NodeType::add("scatter_volume", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_FLOAT(density, "Density", 1.0f);
  SOCKET_IN_FLOAT(anisotropy, "Anisotropy", 0.0f);
  SOCKET_IN_FLOAT(volume_mix_weight, "VolumeMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(volume, "Volume");

  return type;
}

ScatterVolumeNode::ScatterVolumeNode() : VolumeNode(node_type)
{
  closure = CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID;
}

void ScatterVolumeNode::compile(SVMCompiler &compiler)
{
  VolumeNode::compile(compiler, input("Density"), input("Anisotropy"));
}

void ScatterVolumeNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_scatter_volume");
}

/* Principled Volume Closure */

NODE_DEFINE(PrincipledVolumeNode)
{
  NodeType *type = NodeType::add("principled_volume", create, NodeType::SHADER);

  SOCKET_IN_STRING(density_attribute, "Density Attribute", ustring());
  SOCKET_IN_STRING(color_attribute, "Color Attribute", ustring());
  SOCKET_IN_STRING(temperature_attribute, "Temperature Attribute", ustring());

  SOCKET_IN_COLOR(color, "Color", make_float3(0.5f, 0.5f, 0.5f));
  SOCKET_IN_FLOAT(density, "Density", 1.0f);
  SOCKET_IN_FLOAT(anisotropy, "Anisotropy", 0.0f);
  SOCKET_IN_COLOR(absorption_color, "Absorption Color", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_IN_FLOAT(emission_strength, "Emission Strength", 0.0f);
  SOCKET_IN_COLOR(emission_color, "Emission Color", make_float3(1.0f, 1.0f, 1.0f));
  SOCKET_IN_FLOAT(blackbody_intensity, "Blackbody Intensity", 0.0f);
  SOCKET_IN_COLOR(blackbody_tint, "Blackbody Tint", make_float3(1.0f, 1.0f, 1.0f));
  SOCKET_IN_FLOAT(temperature, "Temperature", 1500.0f);
  SOCKET_IN_FLOAT(volume_mix_weight, "VolumeMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(volume, "Volume");

  return type;
}

PrincipledVolumeNode::PrincipledVolumeNode() : VolumeNode(node_type)
{
  closure = CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID;
}

void PrincipledVolumeNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_volume) {
    ShaderInput *density_in = input("Density");
    ShaderInput *blackbody_in = input("Blackbody Intensity");

    if (density_in->link || density > 0.0f) {
      attributes->add_standard(density_attribute);
      attributes->add_standard(color_attribute);
    }

    if (blackbody_in->link || blackbody_intensity > 0.0f) {
      attributes->add_standard(temperature_attribute);
    }

    attributes->add(ATTR_STD_GENERATED_TRANSFORM);
  }

  ShaderNode::attributes(shader, attributes);
}

void PrincipledVolumeNode::compile(SVMCompiler &compiler)
{
  ShaderInput *color_in = input("Color");
  ShaderInput *density_in = input("Density");
  ShaderInput *anisotropy_in = input("Anisotropy");
  ShaderInput *absorption_color_in = input("Absorption Color");
  ShaderInput *emission_in = input("Emission Strength");
  ShaderInput *emission_color_in = input("Emission Color");
  ShaderInput *blackbody_in = input("Blackbody Intensity");
  ShaderInput *blackbody_tint_in = input("Blackbody Tint");
  ShaderInput *temperature_in = input("Temperature");

  if (color_in->link)
    compiler.add_node(NODE_CLOSURE_WEIGHT, compiler.stack_assign(color_in));
  else
    compiler.add_node(NODE_CLOSURE_SET_WEIGHT, color);

  compiler.add_node(NODE_PRINCIPLED_VOLUME,
                    compiler.encode_uchar4(compiler.stack_assign_if_linked(density_in),
                                           compiler.stack_assign_if_linked(anisotropy_in),
                                           compiler.stack_assign(absorption_color_in),
                                           compiler.closure_mix_weight_offset()),
                    compiler.encode_uchar4(compiler.stack_assign_if_linked(emission_in),
                                           compiler.stack_assign(emission_color_in),
                                           compiler.stack_assign_if_linked(blackbody_in),
                                           compiler.stack_assign(temperature_in)),
                    compiler.stack_assign(blackbody_tint_in));

  int attr_density = compiler.attribute_standard(density_attribute);
  int attr_color = compiler.attribute_standard(color_attribute);
  int attr_temperature = compiler.attribute_standard(temperature_attribute);

  compiler.add_node(__float_as_int(density),
                    __float_as_int(anisotropy),
                    __float_as_int(emission_strength),
                    __float_as_int(blackbody_intensity));

  compiler.add_node(attr_density, attr_color, attr_temperature);
}

void PrincipledVolumeNode::compile(OSLCompiler &compiler)
{
  if (Attribute::name_standard(density_attribute.c_str())) {
    density_attribute = ustring("geom:" + density_attribute.string());
  }
  if (Attribute::name_standard(color_attribute.c_str())) {
    color_attribute = ustring("geom:" + color_attribute.string());
  }
  if (Attribute::name_standard(temperature_attribute.c_str())) {
    temperature_attribute = ustring("geom:" + temperature_attribute.string());
  }

  compiler.add(this, "node_principled_volume");
}

/* Principled Hair BSDF Closure */

NODE_DEFINE(PrincipledHairBsdfNode)
{
  NodeType *type = NodeType::add("principled_hair_bsdf", create, NodeType::SHADER);

  /* Color parametrization specified as enum. */
  static NodeEnum parametrization_enum;
  parametrization_enum.insert("Direct coloring", NODE_PRINCIPLED_HAIR_REFLECTANCE);
  parametrization_enum.insert("Melanin concentration", NODE_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION);
  parametrization_enum.insert("Absorption coefficient", NODE_PRINCIPLED_HAIR_DIRECT_ABSORPTION);
  SOCKET_ENUM(
      parametrization, "Parametrization", parametrization_enum, NODE_PRINCIPLED_HAIR_REFLECTANCE);

  /* Initialize sockets to their default values. */
  SOCKET_IN_COLOR(color, "Color", make_float3(0.017513f, 0.005763f, 0.002059f));
  SOCKET_IN_FLOAT(melanin, "Melanin", 0.8f);
  SOCKET_IN_FLOAT(melanin_redness, "Melanin Redness", 1.0f);
  SOCKET_IN_COLOR(tint, "Tint", make_float3(1.f, 1.f, 1.f));
  SOCKET_IN_VECTOR(absorption_coefficient,
                   "Absorption Coefficient",
                   make_float3(0.245531f, 0.52f, 1.365f),
                   SocketType::VECTOR);

  SOCKET_IN_FLOAT(offset, "Offset", 2.f * M_PI_F / 180.f);
  SOCKET_IN_FLOAT(roughness, "Roughness", 0.3f);
  SOCKET_IN_FLOAT(radial_roughness, "Radial Roughness", 0.3f);
  SOCKET_IN_FLOAT(coat, "Coat", 0.0f);
  SOCKET_IN_FLOAT(ior, "IOR", 1.55f);

  SOCKET_IN_FLOAT(random_roughness, "Random Roughness", 0.0f);
  SOCKET_IN_FLOAT(random_color, "Random Color", 0.0f);
  SOCKET_IN_FLOAT(random, "Random", 0.0f);

  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

PrincipledHairBsdfNode::PrincipledHairBsdfNode() : BsdfBaseNode(node_type)
{
  closure = CLOSURE_BSDF_HAIR_PRINCIPLED_ID;
}

/* Enable retrieving Hair Info -> Random if Random isn't linked. */
void PrincipledHairBsdfNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (!input("Random")->link) {
    attributes->add(ATTR_STD_CURVE_RANDOM);
  }
  ShaderNode::attributes(shader, attributes);
}

/* Prepares the input data for the SVM shader. */
void PrincipledHairBsdfNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(NODE_CLOSURE_SET_WEIGHT, make_float3(1.0f, 1.0f, 1.0f));

  ShaderInput *roughness_in = input("Roughness");
  ShaderInput *radial_roughness_in = input("Radial Roughness");
  ShaderInput *random_roughness_in = input("Random Roughness");
  ShaderInput *offset_in = input("Offset");
  ShaderInput *coat_in = input("Coat");
  ShaderInput *ior_in = input("IOR");
  ShaderInput *melanin_in = input("Melanin");
  ShaderInput *melanin_redness_in = input("Melanin Redness");
  ShaderInput *random_color_in = input("Random Color");

  int color_ofs = compiler.stack_assign(input("Color"));
  int tint_ofs = compiler.stack_assign(input("Tint"));
  int absorption_coefficient_ofs = compiler.stack_assign(input("Absorption Coefficient"));

  ShaderInput *random_in = input("Random");
  int attr_random = random_in->link ? SVM_STACK_INVALID :
                                      compiler.attribute(ATTR_STD_CURVE_RANDOM);

  /* Encode all parameters into data nodes. */
  compiler.add_node(NODE_CLOSURE_BSDF,
                    /* Socket IDs can be packed 4 at a time into a single data packet */
                    compiler.encode_uchar4(closure,
                                           compiler.stack_assign_if_linked(roughness_in),
                                           compiler.stack_assign_if_linked(radial_roughness_in),
                                           compiler.closure_mix_weight_offset()),
                    /* The rest are stored as unsigned integers */
                    __float_as_uint(roughness),
                    __float_as_uint(radial_roughness));

  compiler.add_node(compiler.stack_assign_if_linked(input("Normal")),
                    compiler.encode_uchar4(compiler.stack_assign_if_linked(offset_in),
                                           compiler.stack_assign_if_linked(ior_in),
                                           color_ofs,
                                           parametrization),
                    __float_as_uint(offset),
                    __float_as_uint(ior));

  compiler.add_node(compiler.encode_uchar4(compiler.stack_assign_if_linked(coat_in),
                                           compiler.stack_assign_if_linked(melanin_in),
                                           compiler.stack_assign_if_linked(melanin_redness_in),
                                           absorption_coefficient_ofs),
                    __float_as_uint(coat),
                    __float_as_uint(melanin),
                    __float_as_uint(melanin_redness));

  compiler.add_node(compiler.encode_uchar4(tint_ofs,
                                           compiler.stack_assign_if_linked(random_in),
                                           compiler.stack_assign_if_linked(random_color_in),
                                           compiler.stack_assign_if_linked(random_roughness_in)),
                    __float_as_uint(random),
                    __float_as_uint(random_color),
                    __float_as_uint(random_roughness));

  compiler.add_node(
      compiler.encode_uchar4(
          SVM_STACK_INVALID, SVM_STACK_INVALID, SVM_STACK_INVALID, SVM_STACK_INVALID),
      attr_random,
      SVM_STACK_INVALID,
      SVM_STACK_INVALID);
}

/* Prepares the input data for the OSL shader. */
void PrincipledHairBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "parametrization");
  compiler.add(this, "node_principled_hair_bsdf");
}

/* Hair BSDF Closure */

NODE_DEFINE(HairBsdfNode)
{
  NodeType *type = NodeType::add("hair_bsdf", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  static NodeEnum component_enum;
  component_enum.insert("reflection", CLOSURE_BSDF_HAIR_REFLECTION_ID);
  component_enum.insert("transmission", CLOSURE_BSDF_HAIR_TRANSMISSION_ID);
  SOCKET_ENUM(component, "Component", component_enum, CLOSURE_BSDF_HAIR_REFLECTION_ID);
  SOCKET_IN_FLOAT(offset, "Offset", 0.0f);
  SOCKET_IN_FLOAT(roughness_u, "RoughnessU", 0.2f);
  SOCKET_IN_FLOAT(roughness_v, "RoughnessV", 0.2f);
  SOCKET_IN_VECTOR(tangent, "Tangent", make_float3(0.0f, 0.0f, 0.0f));

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

HairBsdfNode::HairBsdfNode() : BsdfNode(node_type)
{
  closure = CLOSURE_BSDF_HAIR_REFLECTION_ID;
}

void HairBsdfNode::compile(SVMCompiler &compiler)
{
  closure = component;

  BsdfNode::compile(compiler, input("RoughnessU"), input("RoughnessV"), input("Offset"));
}

void HairBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "component");
  compiler.add(this, "node_hair_bsdf");
}

/* Geometry */

NODE_DEFINE(GeometryNode)
{
  NodeType *type = NodeType::add("geometry", create, NodeType::SHADER);

  SOCKET_IN_NORMAL(normal_osl,
                   "NormalIn",
                   make_float3(0.0f, 0.0f, 0.0f),
                   SocketType::LINK_NORMAL | SocketType::OSL_INTERNAL);

  SOCKET_OUT_POINT(position, "Position");
  SOCKET_OUT_NORMAL(normal, "Normal");
  SOCKET_OUT_NORMAL(tangent, "Tangent");
  SOCKET_OUT_NORMAL(true_normal, "True Normal");
  SOCKET_OUT_VECTOR(incoming, "Incoming");
  SOCKET_OUT_POINT(parametric, "Parametric");
  SOCKET_OUT_FLOAT(backfacing, "Backfacing");
  SOCKET_OUT_FLOAT(pointiness, "Pointiness");

  return type;
}

GeometryNode::GeometryNode() : ShaderNode(node_type)
{
  special_type = SHADER_SPECIAL_TYPE_GEOMETRY;
}

void GeometryNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface) {
    if (!output("Tangent")->links.empty()) {
      attributes->add(ATTR_STD_GENERATED);
    }
    if (!output("Pointiness")->links.empty()) {
      attributes->add(ATTR_STD_POINTINESS);
    }
  }

  ShaderNode::attributes(shader, attributes);
}

void GeometryNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *out;
  ShaderNodeType geom_node = NODE_GEOMETRY;
  ShaderNodeType attr_node = NODE_ATTR;

  if (bump == SHADER_BUMP_DX) {
    geom_node = NODE_GEOMETRY_BUMP_DX;
    attr_node = NODE_ATTR_BUMP_DX;
  }
  else if (bump == SHADER_BUMP_DY) {
    geom_node = NODE_GEOMETRY_BUMP_DY;
    attr_node = NODE_ATTR_BUMP_DY;
  }

  out = output("Position");
  if (!out->links.empty()) {
    compiler.add_node(geom_node, NODE_GEOM_P, compiler.stack_assign(out));
  }

  out = output("Normal");
  if (!out->links.empty()) {
    compiler.add_node(geom_node, NODE_GEOM_N, compiler.stack_assign(out));
  }

  out = output("Tangent");
  if (!out->links.empty()) {
    compiler.add_node(geom_node, NODE_GEOM_T, compiler.stack_assign(out));
  }

  out = output("True Normal");
  if (!out->links.empty()) {
    compiler.add_node(geom_node, NODE_GEOM_Ng, compiler.stack_assign(out));
  }

  out = output("Incoming");
  if (!out->links.empty()) {
    compiler.add_node(geom_node, NODE_GEOM_I, compiler.stack_assign(out));
  }

  out = output("Parametric");
  if (!out->links.empty()) {
    compiler.add_node(geom_node, NODE_GEOM_uv, compiler.stack_assign(out));
  }

  out = output("Backfacing");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_PATH, NODE_LP_backfacing, compiler.stack_assign(out));
  }

  out = output("Pointiness");
  if (!out->links.empty()) {
    if (compiler.output_type() != SHADER_TYPE_VOLUME) {
      compiler.add_node(
          attr_node, ATTR_STD_POINTINESS, compiler.stack_assign(out), NODE_ATTR_FLOAT);
    }
    else {
      compiler.add_node(NODE_VALUE_F, __float_as_int(0.0f), compiler.stack_assign(out));
    }
  }
}

void GeometryNode::compile(OSLCompiler &compiler)
{
  if (bump == SHADER_BUMP_DX)
    compiler.parameter("bump_offset", "dx");
  else if (bump == SHADER_BUMP_DY)
    compiler.parameter("bump_offset", "dy");
  else
    compiler.parameter("bump_offset", "center");

  compiler.add(this, "node_geometry");
}

int GeometryNode::get_group()
{
  ShaderOutput *out;
  int result = ShaderNode::get_group();

  /* Backfacing uses NODE_LIGHT_PATH */
  out = output("Backfacing");
  if (!out->links.empty()) {
    result = max(result, NODE_GROUP_LEVEL_1);
  }

  return result;
}

/* TextureCoordinate */

NODE_DEFINE(TextureCoordinateNode)
{
  NodeType *type = NodeType::add("texture_coordinate", create, NodeType::SHADER);

  SOCKET_BOOLEAN(from_dupli, "From Dupli", false);
  SOCKET_BOOLEAN(use_transform, "Use Transform", false);
  SOCKET_TRANSFORM(ob_tfm, "Object Transform", transform_identity());

  SOCKET_IN_NORMAL(normal_osl,
                   "NormalIn",
                   make_float3(0.0f, 0.0f, 0.0f),
                   SocketType::LINK_NORMAL | SocketType::OSL_INTERNAL);

  SOCKET_OUT_POINT(generated, "Generated");
  SOCKET_OUT_NORMAL(normal, "Normal");
  SOCKET_OUT_POINT(UV, "UV");
  SOCKET_OUT_POINT(object, "Object");
  SOCKET_OUT_POINT(camera, "Camera");
  SOCKET_OUT_POINT(window, "Window");
  SOCKET_OUT_NORMAL(reflection, "Reflection");

  return type;
}

TextureCoordinateNode::TextureCoordinateNode() : ShaderNode(node_type)
{
}

void TextureCoordinateNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface) {
    if (!from_dupli) {
      if (!output("Generated")->links.empty())
        attributes->add(ATTR_STD_GENERATED);
      if (!output("UV")->links.empty())
        attributes->add(ATTR_STD_UV);
    }
  }

  if (shader->has_volume) {
    if (!from_dupli) {
      if (!output("Generated")->links.empty()) {
        attributes->add(ATTR_STD_GENERATED_TRANSFORM);
      }
    }
  }

  ShaderNode::attributes(shader, attributes);
}

void TextureCoordinateNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *out;
  ShaderNodeType texco_node = NODE_TEX_COORD;
  ShaderNodeType attr_node = NODE_ATTR;
  ShaderNodeType geom_node = NODE_GEOMETRY;

  if (bump == SHADER_BUMP_DX) {
    texco_node = NODE_TEX_COORD_BUMP_DX;
    attr_node = NODE_ATTR_BUMP_DX;
    geom_node = NODE_GEOMETRY_BUMP_DX;
  }
  else if (bump == SHADER_BUMP_DY) {
    texco_node = NODE_TEX_COORD_BUMP_DY;
    attr_node = NODE_ATTR_BUMP_DY;
    geom_node = NODE_GEOMETRY_BUMP_DY;
  }

  out = output("Generated");
  if (!out->links.empty()) {
    if (compiler.background) {
      compiler.add_node(geom_node, NODE_GEOM_P, compiler.stack_assign(out));
    }
    else {
      if (from_dupli) {
        compiler.add_node(texco_node, NODE_TEXCO_DUPLI_GENERATED, compiler.stack_assign(out));
      }
      else if (compiler.output_type() == SHADER_TYPE_VOLUME) {
        compiler.add_node(texco_node, NODE_TEXCO_VOLUME_GENERATED, compiler.stack_assign(out));
      }
      else {
        int attr = compiler.attribute(ATTR_STD_GENERATED);
        compiler.add_node(attr_node, attr, compiler.stack_assign(out), NODE_ATTR_FLOAT3);
      }
    }
  }

  out = output("Normal");
  if (!out->links.empty()) {
    compiler.add_node(texco_node, NODE_TEXCO_NORMAL, compiler.stack_assign(out));
  }

  out = output("UV");
  if (!out->links.empty()) {
    if (from_dupli) {
      compiler.add_node(texco_node, NODE_TEXCO_DUPLI_UV, compiler.stack_assign(out));
    }
    else {
      int attr = compiler.attribute(ATTR_STD_UV);
      compiler.add_node(attr_node, attr, compiler.stack_assign(out), NODE_ATTR_FLOAT3);
    }
  }

  out = output("Object");
  if (!out->links.empty()) {
    compiler.add_node(texco_node, NODE_TEXCO_OBJECT, compiler.stack_assign(out), use_transform);
    if (use_transform) {
      Transform ob_itfm = transform_inverse(ob_tfm);
      compiler.add_node(ob_itfm.x);
      compiler.add_node(ob_itfm.y);
      compiler.add_node(ob_itfm.z);
    }
  }

  out = output("Camera");
  if (!out->links.empty()) {
    compiler.add_node(texco_node, NODE_TEXCO_CAMERA, compiler.stack_assign(out));
  }

  out = output("Window");
  if (!out->links.empty()) {
    compiler.add_node(texco_node, NODE_TEXCO_WINDOW, compiler.stack_assign(out));
  }

  out = output("Reflection");
  if (!out->links.empty()) {
    if (compiler.background) {
      compiler.add_node(geom_node, NODE_GEOM_I, compiler.stack_assign(out));
    }
    else {
      compiler.add_node(texco_node, NODE_TEXCO_REFLECTION, compiler.stack_assign(out));
    }
  }
}

void TextureCoordinateNode::compile(OSLCompiler &compiler)
{
  if (bump == SHADER_BUMP_DX)
    compiler.parameter("bump_offset", "dx");
  else if (bump == SHADER_BUMP_DY)
    compiler.parameter("bump_offset", "dy");
  else
    compiler.parameter("bump_offset", "center");

  if (compiler.background)
    compiler.parameter("is_background", true);
  if (compiler.output_type() == SHADER_TYPE_VOLUME)
    compiler.parameter("is_volume", true);
  compiler.parameter(this, "use_transform");
  Transform ob_itfm = transform_inverse(ob_tfm);
  compiler.parameter("object_itfm", ob_itfm);

  compiler.parameter(this, "from_dupli");

  compiler.add(this, "node_texture_coordinate");
}

/* UV Map */

NODE_DEFINE(UVMapNode)
{
  NodeType *type = NodeType::add("uvmap", create, NodeType::SHADER);

  SOCKET_STRING(attribute, "attribute", ustring());
  SOCKET_IN_BOOLEAN(from_dupli, "from dupli", false);

  SOCKET_OUT_POINT(UV, "UV");

  return type;
}

UVMapNode::UVMapNode() : ShaderNode(node_type)
{
}

void UVMapNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface) {
    if (!from_dupli) {
      if (!output("UV")->links.empty()) {
        if (attribute != "")
          attributes->add(attribute);
        else
          attributes->add(ATTR_STD_UV);
      }
    }
  }

  ShaderNode::attributes(shader, attributes);
}

void UVMapNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *out = output("UV");
  ShaderNodeType texco_node = NODE_TEX_COORD;
  ShaderNodeType attr_node = NODE_ATTR;
  int attr;

  if (bump == SHADER_BUMP_DX) {
    texco_node = NODE_TEX_COORD_BUMP_DX;
    attr_node = NODE_ATTR_BUMP_DX;
  }
  else if (bump == SHADER_BUMP_DY) {
    texco_node = NODE_TEX_COORD_BUMP_DY;
    attr_node = NODE_ATTR_BUMP_DY;
  }

  if (!out->links.empty()) {
    if (from_dupli) {
      compiler.add_node(texco_node, NODE_TEXCO_DUPLI_UV, compiler.stack_assign(out));
    }
    else {
      if (attribute != "")
        attr = compiler.attribute(attribute);
      else
        attr = compiler.attribute(ATTR_STD_UV);

      compiler.add_node(attr_node, attr, compiler.stack_assign(out), NODE_ATTR_FLOAT3);
    }
  }
}

void UVMapNode::compile(OSLCompiler &compiler)
{
  if (bump == SHADER_BUMP_DX)
    compiler.parameter("bump_offset", "dx");
  else if (bump == SHADER_BUMP_DY)
    compiler.parameter("bump_offset", "dy");
  else
    compiler.parameter("bump_offset", "center");

  compiler.parameter(this, "from_dupli");
  compiler.parameter(this, "attribute");
  compiler.add(this, "node_uv_map");
}

/* Light Path */

NODE_DEFINE(LightPathNode)
{
  NodeType *type = NodeType::add("light_path", create, NodeType::SHADER);

  SOCKET_OUT_FLOAT(is_camera_ray, "Is Camera Ray");
  SOCKET_OUT_FLOAT(is_shadow_ray, "Is Shadow Ray");
  SOCKET_OUT_FLOAT(is_diffuse_ray, "Is Diffuse Ray");
  SOCKET_OUT_FLOAT(is_glossy_ray, "Is Glossy Ray");
  SOCKET_OUT_FLOAT(is_singular_ray, "Is Singular Ray");
  SOCKET_OUT_FLOAT(is_reflection_ray, "Is Reflection Ray");
  SOCKET_OUT_FLOAT(is_transmission_ray, "Is Transmission Ray");
  SOCKET_OUT_FLOAT(is_volume_scatter_ray, "Is Volume Scatter Ray");
  SOCKET_OUT_FLOAT(ray_length, "Ray Length");
  SOCKET_OUT_FLOAT(ray_depth, "Ray Depth");
  SOCKET_OUT_FLOAT(diffuse_depth, "Diffuse Depth");
  SOCKET_OUT_FLOAT(glossy_depth, "Glossy Depth");
  SOCKET_OUT_FLOAT(transparent_depth, "Transparent Depth");
  SOCKET_OUT_FLOAT(transmission_depth, "Transmission Depth");

  return type;
}

LightPathNode::LightPathNode() : ShaderNode(node_type)
{
}

void LightPathNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *out;

  out = output("Is Camera Ray");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_PATH, NODE_LP_camera, compiler.stack_assign(out));
  }

  out = output("Is Shadow Ray");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_PATH, NODE_LP_shadow, compiler.stack_assign(out));
  }

  out = output("Is Diffuse Ray");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_PATH, NODE_LP_diffuse, compiler.stack_assign(out));
  }

  out = output("Is Glossy Ray");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_PATH, NODE_LP_glossy, compiler.stack_assign(out));
  }

  out = output("Is Singular Ray");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_PATH, NODE_LP_singular, compiler.stack_assign(out));
  }

  out = output("Is Reflection Ray");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_PATH, NODE_LP_reflection, compiler.stack_assign(out));
  }

  out = output("Is Transmission Ray");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_PATH, NODE_LP_transmission, compiler.stack_assign(out));
  }

  out = output("Is Volume Scatter Ray");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_PATH, NODE_LP_volume_scatter, compiler.stack_assign(out));
  }

  out = output("Ray Length");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_PATH, NODE_LP_ray_length, compiler.stack_assign(out));
  }

  out = output("Ray Depth");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_PATH, NODE_LP_ray_depth, compiler.stack_assign(out));
  }

  out = output("Diffuse Depth");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_PATH, NODE_LP_ray_diffuse, compiler.stack_assign(out));
  }

  out = output("Glossy Depth");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_PATH, NODE_LP_ray_glossy, compiler.stack_assign(out));
  }

  out = output("Transparent Depth");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_PATH, NODE_LP_ray_transparent, compiler.stack_assign(out));
  }

  out = output("Transmission Depth");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_PATH, NODE_LP_ray_transmission, compiler.stack_assign(out));
  }
}

void LightPathNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_light_path");
}

/* Light Falloff */

NODE_DEFINE(LightFalloffNode)
{
  NodeType *type = NodeType::add("light_falloff", create, NodeType::SHADER);

  SOCKET_IN_FLOAT(strength, "Strength", 100.0f);
  SOCKET_IN_FLOAT(smooth, "Smooth", 0.0f);

  SOCKET_OUT_FLOAT(quadratic, "Quadratic");
  SOCKET_OUT_FLOAT(linear, "Linear");
  SOCKET_OUT_FLOAT(constant, "Constant");

  return type;
}

LightFalloffNode::LightFalloffNode() : ShaderNode(node_type)
{
}

void LightFalloffNode::compile(SVMCompiler &compiler)
{
  ShaderInput *strength_in = input("Strength");
  ShaderInput *smooth_in = input("Smooth");

  ShaderOutput *out = output("Quadratic");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_FALLOFF,
                      NODE_LIGHT_FALLOFF_QUADRATIC,
                      compiler.encode_uchar4(compiler.stack_assign(strength_in),
                                             compiler.stack_assign(smooth_in),
                                             compiler.stack_assign(out)));
  }

  out = output("Linear");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_FALLOFF,
                      NODE_LIGHT_FALLOFF_LINEAR,
                      compiler.encode_uchar4(compiler.stack_assign(strength_in),
                                             compiler.stack_assign(smooth_in),
                                             compiler.stack_assign(out)));
  }

  out = output("Constant");
  if (!out->links.empty()) {
    compiler.add_node(NODE_LIGHT_FALLOFF,
                      NODE_LIGHT_FALLOFF_CONSTANT,
                      compiler.encode_uchar4(compiler.stack_assign(strength_in),
                                             compiler.stack_assign(smooth_in),
                                             compiler.stack_assign(out)));
  }
}

void LightFalloffNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_light_falloff");
}

/* Object Info */

NODE_DEFINE(ObjectInfoNode)
{
  NodeType *type = NodeType::add("object_info", create, NodeType::SHADER);

  SOCKET_OUT_VECTOR(location, "Location");
  SOCKET_OUT_FLOAT(object_index, "Object Index");
  SOCKET_OUT_FLOAT(material_index, "Material Index");
  SOCKET_OUT_FLOAT(random, "Random");

  return type;
}

ObjectInfoNode::ObjectInfoNode() : ShaderNode(node_type)
{
}

void ObjectInfoNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *out = output("Location");
  if (!out->links.empty()) {
    compiler.add_node(NODE_OBJECT_INFO, NODE_INFO_OB_LOCATION, compiler.stack_assign(out));
  }

  out = output("Object Index");
  if (!out->links.empty()) {
    compiler.add_node(NODE_OBJECT_INFO, NODE_INFO_OB_INDEX, compiler.stack_assign(out));
  }

  out = output("Material Index");
  if (!out->links.empty()) {
    compiler.add_node(NODE_OBJECT_INFO, NODE_INFO_MAT_INDEX, compiler.stack_assign(out));
  }

  out = output("Random");
  if (!out->links.empty()) {
    compiler.add_node(NODE_OBJECT_INFO, NODE_INFO_OB_RANDOM, compiler.stack_assign(out));
  }
}

void ObjectInfoNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_object_info");
}

/* Particle Info */

NODE_DEFINE(ParticleInfoNode)
{
  NodeType *type = NodeType::add("particle_info", create, NodeType::SHADER);

  SOCKET_OUT_FLOAT(index, "Index");
  SOCKET_OUT_FLOAT(random, "Random");
  SOCKET_OUT_FLOAT(age, "Age");
  SOCKET_OUT_FLOAT(lifetime, "Lifetime");
  SOCKET_OUT_POINT(location, "Location");
#if 0 /* not yet supported */
  SOCKET_OUT_QUATERNION(rotation, "Rotation");
#endif
  SOCKET_OUT_FLOAT(size, "Size");
  SOCKET_OUT_VECTOR(velocity, "Velocity");
  SOCKET_OUT_VECTOR(angular_velocity, "Angular Velocity");

  return type;
}

ParticleInfoNode::ParticleInfoNode() : ShaderNode(node_type)
{
}

void ParticleInfoNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (!output("Index")->links.empty())
    attributes->add(ATTR_STD_PARTICLE);
  if (!output("Random")->links.empty())
    attributes->add(ATTR_STD_PARTICLE);
  if (!output("Age")->links.empty())
    attributes->add(ATTR_STD_PARTICLE);
  if (!output("Lifetime")->links.empty())
    attributes->add(ATTR_STD_PARTICLE);
  if (!output("Location")->links.empty())
    attributes->add(ATTR_STD_PARTICLE);
#if 0 /* not yet supported */
  if (!output("Rotation")->links.empty())
    attributes->add(ATTR_STD_PARTICLE);
#endif
  if (!output("Size")->links.empty())
    attributes->add(ATTR_STD_PARTICLE);
  if (!output("Velocity")->links.empty())
    attributes->add(ATTR_STD_PARTICLE);
  if (!output("Angular Velocity")->links.empty())
    attributes->add(ATTR_STD_PARTICLE);

  ShaderNode::attributes(shader, attributes);
}

void ParticleInfoNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *out;

  out = output("Index");
  if (!out->links.empty()) {
    compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_INDEX, compiler.stack_assign(out));
  }

  out = output("Random");
  if (!out->links.empty()) {
    compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_RANDOM, compiler.stack_assign(out));
  }

  out = output("Age");
  if (!out->links.empty()) {
    compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_AGE, compiler.stack_assign(out));
  }

  out = output("Lifetime");
  if (!out->links.empty()) {
    compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_LIFETIME, compiler.stack_assign(out));
  }

  out = output("Location");
  if (!out->links.empty()) {
    compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_LOCATION, compiler.stack_assign(out));
  }

  /* quaternion data is not yet supported by Cycles */
#if 0
  out = output("Rotation");
  if (!out->links.empty()) {
    compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_ROTATION, compiler.stack_assign(out));
  }
#endif

  out = output("Size");
  if (!out->links.empty()) {
    compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_SIZE, compiler.stack_assign(out));
  }

  out = output("Velocity");
  if (!out->links.empty()) {
    compiler.add_node(NODE_PARTICLE_INFO, NODE_INFO_PAR_VELOCITY, compiler.stack_assign(out));
  }

  out = output("Angular Velocity");
  if (!out->links.empty()) {
    compiler.add_node(
        NODE_PARTICLE_INFO, NODE_INFO_PAR_ANGULAR_VELOCITY, compiler.stack_assign(out));
  }
}

void ParticleInfoNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_particle_info");
}

/* Hair Info */

NODE_DEFINE(HairInfoNode)
{
  NodeType *type = NodeType::add("hair_info", create, NodeType::SHADER);

  SOCKET_OUT_FLOAT(is_strand, "Is Strand");
  SOCKET_OUT_FLOAT(intercept, "Intercept");
  SOCKET_OUT_FLOAT(thickness, "Thickness");
  SOCKET_OUT_NORMAL(tangent_normal, "Tangent Normal");
#if 0 /*output for minimum hair width transparency - deactivated */
  SOCKET_OUT_FLOAT(fade, "Fade");
#endif
  SOCKET_OUT_FLOAT(index, "Random");

  return type;
}

HairInfoNode::HairInfoNode() : ShaderNode(node_type)
{
}

void HairInfoNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface) {
    ShaderOutput *intercept_out = output("Intercept");

    if (!intercept_out->links.empty())
      attributes->add(ATTR_STD_CURVE_INTERCEPT);

    if (!output("Random")->links.empty())
      attributes->add(ATTR_STD_CURVE_RANDOM);
  }

  ShaderNode::attributes(shader, attributes);
}

void HairInfoNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *out;

  out = output("Is Strand");
  if (!out->links.empty()) {
    compiler.add_node(NODE_HAIR_INFO, NODE_INFO_CURVE_IS_STRAND, compiler.stack_assign(out));
  }

  out = output("Intercept");
  if (!out->links.empty()) {
    int attr = compiler.attribute(ATTR_STD_CURVE_INTERCEPT);
    compiler.add_node(NODE_ATTR, attr, compiler.stack_assign(out), NODE_ATTR_FLOAT);
  }

  out = output("Thickness");
  if (!out->links.empty()) {
    compiler.add_node(NODE_HAIR_INFO, NODE_INFO_CURVE_THICKNESS, compiler.stack_assign(out));
  }

  out = output("Tangent Normal");
  if (!out->links.empty()) {
    compiler.add_node(NODE_HAIR_INFO, NODE_INFO_CURVE_TANGENT_NORMAL, compiler.stack_assign(out));
  }

  /*out = output("Fade");
  if(!out->links.empty()) {
    compiler.add_node(NODE_HAIR_INFO, NODE_INFO_CURVE_FADE, compiler.stack_assign(out));
  }*/

  out = output("Random");
  if (!out->links.empty()) {
    int attr = compiler.attribute(ATTR_STD_CURVE_RANDOM);
    compiler.add_node(NODE_ATTR, attr, compiler.stack_assign(out), NODE_ATTR_FLOAT);
  }
}

void HairInfoNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_hair_info");
}

/* Value */

NODE_DEFINE(ValueNode)
{
  NodeType *type = NodeType::add("value", create, NodeType::SHADER);

  SOCKET_FLOAT(value, "Value", 0.0f);
  SOCKET_OUT_FLOAT(value, "Value");

  return type;
}

ValueNode::ValueNode() : ShaderNode(node_type)
{
}

void ValueNode::constant_fold(const ConstantFolder &folder)
{
  folder.make_constant(value);
}

void ValueNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *val_out = output("Value");

  compiler.add_node(NODE_VALUE_F, __float_as_int(value), compiler.stack_assign(val_out));
}

void ValueNode::compile(OSLCompiler &compiler)
{
  compiler.parameter("value_value", value);
  compiler.add(this, "node_value");
}

/* Color */

NODE_DEFINE(ColorNode)
{
  NodeType *type = NodeType::add("color", create, NodeType::SHADER);

  SOCKET_COLOR(value, "Value", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

ColorNode::ColorNode() : ShaderNode(node_type)
{
}

void ColorNode::constant_fold(const ConstantFolder &folder)
{
  folder.make_constant(value);
}

void ColorNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *color_out = output("Color");

  if (!color_out->links.empty()) {
    compiler.add_node(NODE_VALUE_V, compiler.stack_assign(color_out));
    compiler.add_node(NODE_VALUE_V, value);
  }
}

void ColorNode::compile(OSLCompiler &compiler)
{
  compiler.parameter_color("color_value", value);

  compiler.add(this, "node_value");
}

/* Add Closure */

NODE_DEFINE(AddClosureNode)
{
  NodeType *type = NodeType::add("add_closure", create, NodeType::SHADER);

  SOCKET_IN_CLOSURE(closure1, "Closure1");
  SOCKET_IN_CLOSURE(closure2, "Closure2");
  SOCKET_OUT_CLOSURE(closure, "Closure");

  return type;
}

AddClosureNode::AddClosureNode() : ShaderNode(node_type)
{
  special_type = SHADER_SPECIAL_TYPE_COMBINE_CLOSURE;
}

void AddClosureNode::compile(SVMCompiler & /*compiler*/)
{
  /* handled in the SVM compiler */
}

void AddClosureNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_add_closure");
}

void AddClosureNode::constant_fold(const ConstantFolder &folder)
{
  ShaderInput *closure1_in = input("Closure1");
  ShaderInput *closure2_in = input("Closure2");

  /* remove useless add closures nodes */
  if (!closure1_in->link) {
    folder.bypass_or_discard(closure2_in);
  }
  else if (!closure2_in->link) {
    folder.bypass_or_discard(closure1_in);
  }
}

/* Mix Closure */

NODE_DEFINE(MixClosureNode)
{
  NodeType *type = NodeType::add("mix_closure", create, NodeType::SHADER);

  SOCKET_IN_FLOAT(fac, "Fac", 0.5f);
  SOCKET_IN_CLOSURE(closure1, "Closure1");
  SOCKET_IN_CLOSURE(closure2, "Closure2");

  SOCKET_OUT_CLOSURE(closure, "Closure");

  return type;
}

MixClosureNode::MixClosureNode() : ShaderNode(node_type)
{
  special_type = SHADER_SPECIAL_TYPE_COMBINE_CLOSURE;
}

void MixClosureNode::compile(SVMCompiler & /*compiler*/)
{
  /* handled in the SVM compiler */
}

void MixClosureNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_mix_closure");
}

void MixClosureNode::constant_fold(const ConstantFolder &folder)
{
  ShaderInput *fac_in = input("Fac");
  ShaderInput *closure1_in = input("Closure1");
  ShaderInput *closure2_in = input("Closure2");

  /* remove useless mix closures nodes */
  if (closure1_in->link == closure2_in->link) {
    folder.bypass_or_discard(closure1_in);
  }
  /* remove unused mix closure input when factor is 0.0 or 1.0
   * check for closure links and make sure factor link is disconnected */
  else if (!fac_in->link) {
    /* factor 0.0 */
    if (fac <= 0.0f) {
      folder.bypass_or_discard(closure1_in);
    }
    /* factor 1.0 */
    else if (fac >= 1.0f) {
      folder.bypass_or_discard(closure2_in);
    }
  }
}

/* Mix Closure */

NODE_DEFINE(MixClosureWeightNode)
{
  NodeType *type = NodeType::add("mix_closure_weight", create, NodeType::SHADER);

  SOCKET_IN_FLOAT(weight, "Weight", 1.0f);
  SOCKET_IN_FLOAT(fac, "Fac", 1.0f);

  SOCKET_OUT_FLOAT(weight1, "Weight1");
  SOCKET_OUT_FLOAT(weight2, "Weight2");

  return type;
}

MixClosureWeightNode::MixClosureWeightNode() : ShaderNode(node_type)
{
}

void MixClosureWeightNode::compile(SVMCompiler &compiler)
{
  ShaderInput *weight_in = input("Weight");
  ShaderInput *fac_in = input("Fac");
  ShaderOutput *weight1_out = output("Weight1");
  ShaderOutput *weight2_out = output("Weight2");

  compiler.add_node(NODE_MIX_CLOSURE,
                    compiler.encode_uchar4(compiler.stack_assign(fac_in),
                                           compiler.stack_assign(weight_in),
                                           compiler.stack_assign(weight1_out),
                                           compiler.stack_assign(weight2_out)));
}

void MixClosureWeightNode::compile(OSLCompiler & /*compiler*/)
{
  assert(0);
}

/* Invert */

NODE_DEFINE(InvertNode)
{
  NodeType *type = NodeType::add("invert", create, NodeType::SHADER);

  SOCKET_IN_FLOAT(fac, "Fac", 1.0f);
  SOCKET_IN_COLOR(color, "Color", make_float3(0.0f, 0.0f, 0.0f));

  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

InvertNode::InvertNode() : ShaderNode(node_type)
{
}

void InvertNode::constant_fold(const ConstantFolder &folder)
{
  ShaderInput *fac_in = input("Fac");
  ShaderInput *color_in = input("Color");

  if (!fac_in->link) {
    /* evaluate fully constant node */
    if (!color_in->link) {
      folder.make_constant(interp(color, make_float3(1.0f, 1.0f, 1.0f) - color, fac));
    }
    /* remove no-op node */
    else if (fac == 0.0f) {
      folder.bypass(color_in->link);
    }
  }
}

void InvertNode::compile(SVMCompiler &compiler)
{
  ShaderInput *fac_in = input("Fac");
  ShaderInput *color_in = input("Color");
  ShaderOutput *color_out = output("Color");

  compiler.add_node(NODE_INVERT,
                    compiler.stack_assign(fac_in),
                    compiler.stack_assign(color_in),
                    compiler.stack_assign(color_out));
}

void InvertNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_invert");
}

/* Mix */

NODE_DEFINE(MixNode)
{
  NodeType *type = NodeType::add("mix", create, NodeType::SHADER);

  static NodeEnum type_enum;
  type_enum.insert("mix", NODE_MIX_BLEND);
  type_enum.insert("add", NODE_MIX_ADD);
  type_enum.insert("multiply", NODE_MIX_MUL);
  type_enum.insert("screen", NODE_MIX_SCREEN);
  type_enum.insert("overlay", NODE_MIX_OVERLAY);
  type_enum.insert("subtract", NODE_MIX_SUB);
  type_enum.insert("divide", NODE_MIX_DIV);
  type_enum.insert("difference", NODE_MIX_DIFF);
  type_enum.insert("darken", NODE_MIX_DARK);
  type_enum.insert("lighten", NODE_MIX_LIGHT);
  type_enum.insert("dodge", NODE_MIX_DODGE);
  type_enum.insert("burn", NODE_MIX_BURN);
  type_enum.insert("hue", NODE_MIX_HUE);
  type_enum.insert("saturation", NODE_MIX_SAT);
  type_enum.insert("value", NODE_MIX_VAL);
  type_enum.insert("color", NODE_MIX_COLOR);
  type_enum.insert("soft_light", NODE_MIX_SOFT);
  type_enum.insert("linear_light", NODE_MIX_LINEAR);
  SOCKET_ENUM(type, "Type", type_enum, NODE_MIX_BLEND);

  SOCKET_BOOLEAN(use_clamp, "Use Clamp", false);

  SOCKET_IN_FLOAT(fac, "Fac", 0.5f);
  SOCKET_IN_COLOR(color1, "Color1", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_IN_COLOR(color2, "Color2", make_float3(0.0f, 0.0f, 0.0f));

  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

MixNode::MixNode() : ShaderNode(node_type)
{
}

void MixNode::compile(SVMCompiler &compiler)
{
  ShaderInput *fac_in = input("Fac");
  ShaderInput *color1_in = input("Color1");
  ShaderInput *color2_in = input("Color2");
  ShaderOutput *color_out = output("Color");

  compiler.add_node(NODE_MIX,
                    compiler.stack_assign(fac_in),
                    compiler.stack_assign(color1_in),
                    compiler.stack_assign(color2_in));
  compiler.add_node(NODE_MIX, type, compiler.stack_assign(color_out));

  if (use_clamp) {
    compiler.add_node(NODE_MIX, 0, compiler.stack_assign(color_out));
    compiler.add_node(NODE_MIX, NODE_MIX_CLAMP, compiler.stack_assign(color_out));
  }
}

void MixNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "type");
  compiler.parameter(this, "use_clamp");
  compiler.add(this, "node_mix");
}

void MixNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    folder.make_constant_clamp(svm_mix(type, fac, color1, color2), use_clamp);
  }
  else {
    folder.fold_mix(type, use_clamp);
  }
}

/* Combine RGB */

NODE_DEFINE(CombineRGBNode)
{
  NodeType *type = NodeType::add("combine_rgb", create, NodeType::SHADER);

  SOCKET_IN_FLOAT(r, "R", 0.0f);
  SOCKET_IN_FLOAT(g, "G", 0.0f);
  SOCKET_IN_FLOAT(b, "B", 0.0f);

  SOCKET_OUT_COLOR(image, "Image");

  return type;
}

CombineRGBNode::CombineRGBNode() : ShaderNode(node_type)
{
}

void CombineRGBNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    folder.make_constant(make_float3(r, g, b));
  }
}

void CombineRGBNode::compile(SVMCompiler &compiler)
{
  ShaderInput *red_in = input("R");
  ShaderInput *green_in = input("G");
  ShaderInput *blue_in = input("B");
  ShaderOutput *color_out = output("Image");

  compiler.add_node(
      NODE_COMBINE_VECTOR, compiler.stack_assign(red_in), 0, compiler.stack_assign(color_out));

  compiler.add_node(
      NODE_COMBINE_VECTOR, compiler.stack_assign(green_in), 1, compiler.stack_assign(color_out));

  compiler.add_node(
      NODE_COMBINE_VECTOR, compiler.stack_assign(blue_in), 2, compiler.stack_assign(color_out));
}

void CombineRGBNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_combine_rgb");
}

/* Combine XYZ */

NODE_DEFINE(CombineXYZNode)
{
  NodeType *type = NodeType::add("combine_xyz", create, NodeType::SHADER);

  SOCKET_IN_FLOAT(x, "X", 0.0f);
  SOCKET_IN_FLOAT(y, "Y", 0.0f);
  SOCKET_IN_FLOAT(z, "Z", 0.0f);

  SOCKET_OUT_VECTOR(vector, "Vector");

  return type;
}

CombineXYZNode::CombineXYZNode() : ShaderNode(node_type)
{
}

void CombineXYZNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    folder.make_constant(make_float3(x, y, z));
  }
}

void CombineXYZNode::compile(SVMCompiler &compiler)
{
  ShaderInput *x_in = input("X");
  ShaderInput *y_in = input("Y");
  ShaderInput *z_in = input("Z");
  ShaderOutput *vector_out = output("Vector");

  compiler.add_node(
      NODE_COMBINE_VECTOR, compiler.stack_assign(x_in), 0, compiler.stack_assign(vector_out));

  compiler.add_node(
      NODE_COMBINE_VECTOR, compiler.stack_assign(y_in), 1, compiler.stack_assign(vector_out));

  compiler.add_node(
      NODE_COMBINE_VECTOR, compiler.stack_assign(z_in), 2, compiler.stack_assign(vector_out));
}

void CombineXYZNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_combine_xyz");
}

/* Combine HSV */

NODE_DEFINE(CombineHSVNode)
{
  NodeType *type = NodeType::add("combine_hsv", create, NodeType::SHADER);

  SOCKET_IN_FLOAT(h, "H", 0.0f);
  SOCKET_IN_FLOAT(s, "S", 0.0f);
  SOCKET_IN_FLOAT(v, "V", 0.0f);

  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

CombineHSVNode::CombineHSVNode() : ShaderNode(node_type)
{
}

void CombineHSVNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    folder.make_constant(hsv_to_rgb(make_float3(h, s, v)));
  }
}

void CombineHSVNode::compile(SVMCompiler &compiler)
{
  ShaderInput *hue_in = input("H");
  ShaderInput *saturation_in = input("S");
  ShaderInput *value_in = input("V");
  ShaderOutput *color_out = output("Color");

  compiler.add_node(NODE_COMBINE_HSV,
                    compiler.stack_assign(hue_in),
                    compiler.stack_assign(saturation_in),
                    compiler.stack_assign(value_in));
  compiler.add_node(NODE_COMBINE_HSV, compiler.stack_assign(color_out));
}

void CombineHSVNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_combine_hsv");
}

/* Gamma */

NODE_DEFINE(GammaNode)
{
  NodeType *type = NodeType::add("gamma", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_IN_FLOAT(gamma, "Gamma", 1.0f);
  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

GammaNode::GammaNode() : ShaderNode(node_type)
{
}

void GammaNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    folder.make_constant(svm_math_gamma_color(color, gamma));
  }
  else {
    ShaderInput *color_in = input("Color");
    ShaderInput *gamma_in = input("Gamma");

    /* 1 ^ X == X ^ 0 == 1 */
    if (folder.is_one(color_in) || folder.is_zero(gamma_in)) {
      folder.make_one();
    }
    /* X ^ 1 == X */
    else if (folder.is_one(gamma_in)) {
      folder.try_bypass_or_make_constant(color_in, false);
    }
  }
}

void GammaNode::compile(SVMCompiler &compiler)
{
  ShaderInput *color_in = input("Color");
  ShaderInput *gamma_in = input("Gamma");
  ShaderOutput *color_out = output("Color");

  compiler.add_node(NODE_GAMMA,
                    compiler.stack_assign(gamma_in),
                    compiler.stack_assign(color_in),
                    compiler.stack_assign(color_out));
}

void GammaNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_gamma");
}

/* Bright Contrast */

NODE_DEFINE(BrightContrastNode)
{
  NodeType *type = NodeType::add("brightness_contrast", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_IN_FLOAT(bright, "Bright", 0.0f);
  SOCKET_IN_FLOAT(contrast, "Contrast", 0.0f);

  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

BrightContrastNode::BrightContrastNode() : ShaderNode(node_type)
{
}

void BrightContrastNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    folder.make_constant(svm_brightness_contrast(color, bright, contrast));
  }
}

void BrightContrastNode::compile(SVMCompiler &compiler)
{
  ShaderInput *color_in = input("Color");
  ShaderInput *bright_in = input("Bright");
  ShaderInput *contrast_in = input("Contrast");
  ShaderOutput *color_out = output("Color");

  compiler.add_node(NODE_BRIGHTCONTRAST,
                    compiler.stack_assign(color_in),
                    compiler.stack_assign(color_out),
                    compiler.encode_uchar4(compiler.stack_assign(bright_in),
                                           compiler.stack_assign(contrast_in)));
}

void BrightContrastNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_brightness");
}

/* Separate RGB */

NODE_DEFINE(SeparateRGBNode)
{
  NodeType *type = NodeType::add("separate_rgb", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Image", make_float3(0.0f, 0.0f, 0.0f));

  SOCKET_OUT_FLOAT(r, "R");
  SOCKET_OUT_FLOAT(g, "G");
  SOCKET_OUT_FLOAT(b, "B");

  return type;
}

SeparateRGBNode::SeparateRGBNode() : ShaderNode(node_type)
{
}

void SeparateRGBNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    for (int channel = 0; channel < 3; channel++) {
      if (outputs[channel] == folder.output) {
        folder.make_constant(color[channel]);
        return;
      }
    }
  }
}

void SeparateRGBNode::compile(SVMCompiler &compiler)
{
  ShaderInput *color_in = input("Image");
  ShaderOutput *red_out = output("R");
  ShaderOutput *green_out = output("G");
  ShaderOutput *blue_out = output("B");

  compiler.add_node(
      NODE_SEPARATE_VECTOR, compiler.stack_assign(color_in), 0, compiler.stack_assign(red_out));

  compiler.add_node(
      NODE_SEPARATE_VECTOR, compiler.stack_assign(color_in), 1, compiler.stack_assign(green_out));

  compiler.add_node(
      NODE_SEPARATE_VECTOR, compiler.stack_assign(color_in), 2, compiler.stack_assign(blue_out));
}

void SeparateRGBNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_separate_rgb");
}

/* Separate XYZ */

NODE_DEFINE(SeparateXYZNode)
{
  NodeType *type = NodeType::add("separate_xyz", create, NodeType::SHADER);

  SOCKET_IN_COLOR(vector, "Vector", make_float3(0.0f, 0.0f, 0.0f));

  SOCKET_OUT_FLOAT(x, "X");
  SOCKET_OUT_FLOAT(y, "Y");
  SOCKET_OUT_FLOAT(z, "Z");

  return type;
}

SeparateXYZNode::SeparateXYZNode() : ShaderNode(node_type)
{
}

void SeparateXYZNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    for (int channel = 0; channel < 3; channel++) {
      if (outputs[channel] == folder.output) {
        folder.make_constant(vector[channel]);
        return;
      }
    }
  }
}

void SeparateXYZNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");
  ShaderOutput *x_out = output("X");
  ShaderOutput *y_out = output("Y");
  ShaderOutput *z_out = output("Z");

  compiler.add_node(
      NODE_SEPARATE_VECTOR, compiler.stack_assign(vector_in), 0, compiler.stack_assign(x_out));

  compiler.add_node(
      NODE_SEPARATE_VECTOR, compiler.stack_assign(vector_in), 1, compiler.stack_assign(y_out));

  compiler.add_node(
      NODE_SEPARATE_VECTOR, compiler.stack_assign(vector_in), 2, compiler.stack_assign(z_out));
}

void SeparateXYZNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_separate_xyz");
}

/* Separate HSV */

NODE_DEFINE(SeparateHSVNode)
{
  NodeType *type = NodeType::add("separate_hsv", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.0f, 0.0f, 0.0f));

  SOCKET_OUT_FLOAT(h, "H");
  SOCKET_OUT_FLOAT(s, "S");
  SOCKET_OUT_FLOAT(v, "V");

  return type;
}

SeparateHSVNode::SeparateHSVNode() : ShaderNode(node_type)
{
}

void SeparateHSVNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    float3 hsv = rgb_to_hsv(color);

    for (int channel = 0; channel < 3; channel++) {
      if (outputs[channel] == folder.output) {
        folder.make_constant(hsv[channel]);
        return;
      }
    }
  }
}

void SeparateHSVNode::compile(SVMCompiler &compiler)
{
  ShaderInput *color_in = input("Color");
  ShaderOutput *hue_out = output("H");
  ShaderOutput *saturation_out = output("S");
  ShaderOutput *value_out = output("V");

  compiler.add_node(NODE_SEPARATE_HSV,
                    compiler.stack_assign(color_in),
                    compiler.stack_assign(hue_out),
                    compiler.stack_assign(saturation_out));
  compiler.add_node(NODE_SEPARATE_HSV, compiler.stack_assign(value_out));
}

void SeparateHSVNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_separate_hsv");
}

/* Hue Saturation Value */

NODE_DEFINE(HSVNode)
{
  NodeType *type = NodeType::add("hsv", create, NodeType::SHADER);

  SOCKET_IN_FLOAT(hue, "Hue", 0.5f);
  SOCKET_IN_FLOAT(saturation, "Saturation", 1.0f);
  SOCKET_IN_FLOAT(value, "Value", 1.0f);
  SOCKET_IN_FLOAT(fac, "Fac", 1.0f);
  SOCKET_IN_COLOR(color, "Color", make_float3(0.0f, 0.0f, 0.0f));

  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

HSVNode::HSVNode() : ShaderNode(node_type)
{
}

void HSVNode::compile(SVMCompiler &compiler)
{
  ShaderInput *hue_in = input("Hue");
  ShaderInput *saturation_in = input("Saturation");
  ShaderInput *value_in = input("Value");
  ShaderInput *fac_in = input("Fac");
  ShaderInput *color_in = input("Color");
  ShaderOutput *color_out = output("Color");

  compiler.add_node(NODE_HSV,
                    compiler.encode_uchar4(compiler.stack_assign(color_in),
                                           compiler.stack_assign(fac_in),
                                           compiler.stack_assign(color_out)),
                    compiler.encode_uchar4(compiler.stack_assign(hue_in),
                                           compiler.stack_assign(saturation_in),
                                           compiler.stack_assign(value_in)));
}

void HSVNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_hsv");
}

/* Attribute */

NODE_DEFINE(AttributeNode)
{
  NodeType *type = NodeType::add("attribute", create, NodeType::SHADER);

  SOCKET_STRING(attribute, "Attribute", ustring());

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_VECTOR(vector, "Vector");
  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

AttributeNode::AttributeNode() : ShaderNode(node_type)
{
}

void AttributeNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  ShaderOutput *color_out = output("Color");
  ShaderOutput *vector_out = output("Vector");
  ShaderOutput *fac_out = output("Fac");

  if (!color_out->links.empty() || !vector_out->links.empty() || !fac_out->links.empty()) {
    attributes->add_standard(attribute);
  }

  if (shader->has_volume) {
    attributes->add(ATTR_STD_GENERATED_TRANSFORM);
  }

  ShaderNode::attributes(shader, attributes);
}

void AttributeNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *color_out = output("Color");
  ShaderOutput *vector_out = output("Vector");
  ShaderOutput *fac_out = output("Fac");
  ShaderNodeType attr_node = NODE_ATTR;
  int attr = compiler.attribute_standard(attribute);

  if (bump == SHADER_BUMP_DX)
    attr_node = NODE_ATTR_BUMP_DX;
  else if (bump == SHADER_BUMP_DY)
    attr_node = NODE_ATTR_BUMP_DY;

  if (!color_out->links.empty() || !vector_out->links.empty()) {
    if (!color_out->links.empty()) {
      compiler.add_node(attr_node, attr, compiler.stack_assign(color_out), NODE_ATTR_FLOAT3);
    }
    if (!vector_out->links.empty()) {
      compiler.add_node(attr_node, attr, compiler.stack_assign(vector_out), NODE_ATTR_FLOAT3);
    }
  }

  if (!fac_out->links.empty()) {
    compiler.add_node(attr_node, attr, compiler.stack_assign(fac_out), NODE_ATTR_FLOAT);
  }
}

void AttributeNode::compile(OSLCompiler &compiler)
{
  if (bump == SHADER_BUMP_DX)
    compiler.parameter("bump_offset", "dx");
  else if (bump == SHADER_BUMP_DY)
    compiler.parameter("bump_offset", "dy");
  else
    compiler.parameter("bump_offset", "center");

  if (Attribute::name_standard(attribute.c_str()) != ATTR_STD_NONE)
    compiler.parameter("name", (string("geom:") + attribute.c_str()).c_str());
  else
    compiler.parameter("name", attribute.c_str());

  compiler.add(this, "node_attribute");
}

/* Camera */

NODE_DEFINE(CameraNode)
{
  NodeType *type = NodeType::add("camera_info", create, NodeType::SHADER);

  SOCKET_OUT_VECTOR(view_vector, "View Vector");
  SOCKET_OUT_FLOAT(view_z_depth, "View Z Depth");
  SOCKET_OUT_FLOAT(view_distance, "View Distance");

  return type;
}

CameraNode::CameraNode() : ShaderNode(node_type)
{
}

void CameraNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *vector_out = output("View Vector");
  ShaderOutput *z_depth_out = output("View Z Depth");
  ShaderOutput *distance_out = output("View Distance");

  compiler.add_node(NODE_CAMERA,
                    compiler.stack_assign(vector_out),
                    compiler.stack_assign(z_depth_out),
                    compiler.stack_assign(distance_out));
}

void CameraNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_camera");
}

/* Fresnel */

NODE_DEFINE(FresnelNode)
{
  NodeType *type = NodeType::add("fresnel", create, NodeType::SHADER);

  SOCKET_IN_NORMAL(normal,
                   "Normal",
                   make_float3(0.0f, 0.0f, 0.0f),
                   SocketType::LINK_NORMAL | SocketType::OSL_INTERNAL);
  SOCKET_IN_FLOAT(IOR, "IOR", 1.45f);

  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

FresnelNode::FresnelNode() : ShaderNode(node_type)
{
}

void FresnelNode::compile(SVMCompiler &compiler)
{
  ShaderInput *normal_in = input("Normal");
  ShaderInput *IOR_in = input("IOR");
  ShaderOutput *fac_out = output("Fac");

  compiler.add_node(NODE_FRESNEL,
                    compiler.stack_assign(IOR_in),
                    __float_as_int(IOR),
                    compiler.encode_uchar4(compiler.stack_assign_if_linked(normal_in),
                                           compiler.stack_assign(fac_out)));
}

void FresnelNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_fresnel");
}

/* Layer Weight */

NODE_DEFINE(LayerWeightNode)
{
  NodeType *type = NodeType::add("layer_weight", create, NodeType::SHADER);

  SOCKET_IN_NORMAL(normal,
                   "Normal",
                   make_float3(0.0f, 0.0f, 0.0f),
                   SocketType::LINK_NORMAL | SocketType::OSL_INTERNAL);
  SOCKET_IN_FLOAT(blend, "Blend", 0.5f);

  SOCKET_OUT_FLOAT(fresnel, "Fresnel");
  SOCKET_OUT_FLOAT(facing, "Facing");

  return type;
}

LayerWeightNode::LayerWeightNode() : ShaderNode(node_type)
{
}

void LayerWeightNode::compile(SVMCompiler &compiler)
{
  ShaderInput *normal_in = input("Normal");
  ShaderInput *blend_in = input("Blend");
  ShaderOutput *fresnel_out = output("Fresnel");
  ShaderOutput *facing_out = output("Facing");

  if (!fresnel_out->links.empty()) {
    compiler.add_node(NODE_LAYER_WEIGHT,
                      compiler.stack_assign_if_linked(blend_in),
                      __float_as_int(blend),
                      compiler.encode_uchar4(NODE_LAYER_WEIGHT_FRESNEL,
                                             compiler.stack_assign_if_linked(normal_in),
                                             compiler.stack_assign(fresnel_out)));
  }

  if (!facing_out->links.empty()) {
    compiler.add_node(NODE_LAYER_WEIGHT,
                      compiler.stack_assign_if_linked(blend_in),
                      __float_as_int(blend),
                      compiler.encode_uchar4(NODE_LAYER_WEIGHT_FACING,
                                             compiler.stack_assign_if_linked(normal_in),
                                             compiler.stack_assign(facing_out)));
  }
}

void LayerWeightNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_layer_weight");
}

/* Wireframe */

NODE_DEFINE(WireframeNode)
{
  NodeType *type = NodeType::add("wireframe", create, NodeType::SHADER);

  SOCKET_BOOLEAN(use_pixel_size, "Use Pixel Size", false);
  SOCKET_IN_FLOAT(size, "Size", 0.01f);
  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

WireframeNode::WireframeNode() : ShaderNode(node_type)
{
}

void WireframeNode::compile(SVMCompiler &compiler)
{
  ShaderInput *size_in = input("Size");
  ShaderOutput *fac_out = output("Fac");
  NodeBumpOffset bump_offset = NODE_BUMP_OFFSET_CENTER;
  if (bump == SHADER_BUMP_DX) {
    bump_offset = NODE_BUMP_OFFSET_DX;
  }
  else if (bump == SHADER_BUMP_DY) {
    bump_offset = NODE_BUMP_OFFSET_DY;
  }
  compiler.add_node(NODE_WIREFRAME,
                    compiler.stack_assign(size_in),
                    compiler.stack_assign(fac_out),
                    compiler.encode_uchar4(use_pixel_size, bump_offset, 0, 0));
}

void WireframeNode::compile(OSLCompiler &compiler)
{
  if (bump == SHADER_BUMP_DX) {
    compiler.parameter("bump_offset", "dx");
  }
  else if (bump == SHADER_BUMP_DY) {
    compiler.parameter("bump_offset", "dy");
  }
  else {
    compiler.parameter("bump_offset", "center");
  }
  compiler.parameter(this, "use_pixel_size");
  compiler.add(this, "node_wireframe");
}

/* Wavelength */

NODE_DEFINE(WavelengthNode)
{
  NodeType *type = NodeType::add("wavelength", create, NodeType::SHADER);

  SOCKET_IN_FLOAT(wavelength, "Wavelength", 500.0f);
  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

WavelengthNode::WavelengthNode() : ShaderNode(node_type)
{
}

void WavelengthNode::compile(SVMCompiler &compiler)
{
  ShaderInput *wavelength_in = input("Wavelength");
  ShaderOutput *color_out = output("Color");

  compiler.add_node(
      NODE_WAVELENGTH, compiler.stack_assign(wavelength_in), compiler.stack_assign(color_out));
}

void WavelengthNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_wavelength");
}

/* Blackbody */

NODE_DEFINE(BlackbodyNode)
{
  NodeType *type = NodeType::add("blackbody", create, NodeType::SHADER);

  SOCKET_IN_FLOAT(temperature, "Temperature", 1200.0f);
  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

BlackbodyNode::BlackbodyNode() : ShaderNode(node_type)
{
}

void BlackbodyNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    folder.make_constant(svm_math_blackbody_color(temperature));
  }
}

void BlackbodyNode::compile(SVMCompiler &compiler)
{
  ShaderInput *temperature_in = input("Temperature");
  ShaderOutput *color_out = output("Color");

  compiler.add_node(
      NODE_BLACKBODY, compiler.stack_assign(temperature_in), compiler.stack_assign(color_out));
}

void BlackbodyNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_blackbody");
}

/* Output */

NODE_DEFINE(OutputNode)
{
  NodeType *type = NodeType::add("output", create, NodeType::SHADER);

  SOCKET_IN_CLOSURE(surface, "Surface");
  SOCKET_IN_CLOSURE(volume, "Volume");
  SOCKET_IN_VECTOR(displacement, "Displacement", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f));

  return type;
}

OutputNode::OutputNode() : ShaderNode(node_type)
{
  special_type = SHADER_SPECIAL_TYPE_OUTPUT;
}

void OutputNode::compile(SVMCompiler &compiler)
{
  if (compiler.output_type() == SHADER_TYPE_DISPLACEMENT) {
    ShaderInput *displacement_in = input("Displacement");

    if (displacement_in->link) {
      compiler.add_node(NODE_SET_DISPLACEMENT, compiler.stack_assign(displacement_in));
    }
  }
}

void OutputNode::compile(OSLCompiler &compiler)
{
  if (compiler.output_type() == SHADER_TYPE_SURFACE)
    compiler.add(this, "node_output_surface");
  else if (compiler.output_type() == SHADER_TYPE_VOLUME)
    compiler.add(this, "node_output_volume");
  else if (compiler.output_type() == SHADER_TYPE_DISPLACEMENT)
    compiler.add(this, "node_output_displacement");
}

/* Math */

NODE_DEFINE(MathNode)
{
  NodeType *type = NodeType::add("math", create, NodeType::SHADER);

  static NodeEnum type_enum;
  type_enum.insert("add", NODE_MATH_ADD);
  type_enum.insert("subtract", NODE_MATH_SUBTRACT);
  type_enum.insert("multiply", NODE_MATH_MULTIPLY);
  type_enum.insert("divide", NODE_MATH_DIVIDE);
  type_enum.insert("sine", NODE_MATH_SINE);
  type_enum.insert("cosine", NODE_MATH_COSINE);
  type_enum.insert("tangent", NODE_MATH_TANGENT);
  type_enum.insert("arcsine", NODE_MATH_ARCSINE);
  type_enum.insert("arccosine", NODE_MATH_ARCCOSINE);
  type_enum.insert("arctangent", NODE_MATH_ARCTANGENT);
  type_enum.insert("power", NODE_MATH_POWER);
  type_enum.insert("logarithm", NODE_MATH_LOGARITHM);
  type_enum.insert("minimum", NODE_MATH_MINIMUM);
  type_enum.insert("maximum", NODE_MATH_MAXIMUM);
  type_enum.insert("round", NODE_MATH_ROUND);
  type_enum.insert("less_than", NODE_MATH_LESS_THAN);
  type_enum.insert("greater_than", NODE_MATH_GREATER_THAN);
  type_enum.insert("modulo", NODE_MATH_MODULO);
  type_enum.insert("absolute", NODE_MATH_ABSOLUTE);
  type_enum.insert("arctan2", NODE_MATH_ARCTAN2);
  type_enum.insert("floor", NODE_MATH_FLOOR);
  type_enum.insert("ceil", NODE_MATH_CEIL);
  type_enum.insert("fract", NODE_MATH_FRACT);
  type_enum.insert("sqrt", NODE_MATH_SQRT);
  SOCKET_ENUM(type, "Type", type_enum, NODE_MATH_ADD);

  SOCKET_BOOLEAN(use_clamp, "Use Clamp", false);

  SOCKET_IN_FLOAT(value1, "Value1", 0.0f);
  SOCKET_IN_FLOAT(value2, "Value2", 0.0f);

  SOCKET_OUT_FLOAT(value, "Value");

  return type;
}

MathNode::MathNode() : ShaderNode(node_type)
{
}

void MathNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    folder.make_constant_clamp(svm_math(type, value1, value2), use_clamp);
  }
  else {
    folder.fold_math(type, use_clamp);
  }
}

void MathNode::compile(SVMCompiler &compiler)
{
  ShaderInput *value1_in = input("Value1");
  ShaderInput *value2_in = input("Value2");
  ShaderOutput *value_out = output("Value");

  compiler.add_node(
      NODE_MATH, type, compiler.stack_assign(value1_in), compiler.stack_assign(value2_in));
  compiler.add_node(NODE_MATH, compiler.stack_assign(value_out));

  if (use_clamp) {
    compiler.add_node(NODE_MATH, NODE_MATH_CLAMP, compiler.stack_assign(value_out));
    compiler.add_node(NODE_MATH, compiler.stack_assign(value_out));
  }
}

void MathNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "type");
  compiler.parameter(this, "use_clamp");
  compiler.add(this, "node_math");
}

/* VectorMath */

NODE_DEFINE(VectorMathNode)
{
  NodeType *type = NodeType::add("vector_math", create, NodeType::SHADER);

  static NodeEnum type_enum;
  type_enum.insert("add", NODE_VECTOR_MATH_ADD);
  type_enum.insert("subtract", NODE_VECTOR_MATH_SUBTRACT);
  type_enum.insert("average", NODE_VECTOR_MATH_AVERAGE);
  type_enum.insert("dot_product", NODE_VECTOR_MATH_DOT_PRODUCT);
  type_enum.insert("cross_product", NODE_VECTOR_MATH_CROSS_PRODUCT);
  type_enum.insert("normalize", NODE_VECTOR_MATH_NORMALIZE);
  SOCKET_ENUM(type, "Type", type_enum, NODE_VECTOR_MATH_ADD);

  SOCKET_IN_VECTOR(vector1, "Vector1", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_IN_VECTOR(vector2, "Vector2", make_float3(0.0f, 0.0f, 0.0f));

  SOCKET_OUT_FLOAT(value, "Value");
  SOCKET_OUT_VECTOR(vector, "Vector");

  return type;
}

VectorMathNode::VectorMathNode() : ShaderNode(node_type)
{
}

void VectorMathNode::constant_fold(const ConstantFolder &folder)
{
  float value;
  float3 vector;

  if (folder.all_inputs_constant()) {
    svm_vector_math(&value, &vector, type, vector1, vector2);

    if (folder.output == output("Value")) {
      folder.make_constant(value);
    }
    else if (folder.output == output("Vector")) {
      folder.make_constant(vector);
    }
  }
  else {
    folder.fold_vector_math(type);
  }
}

void VectorMathNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector1_in = input("Vector1");
  ShaderInput *vector2_in = input("Vector2");
  ShaderOutput *value_out = output("Value");
  ShaderOutput *vector_out = output("Vector");

  compiler.add_node(NODE_VECTOR_MATH,
                    type,
                    compiler.stack_assign(vector1_in),
                    compiler.stack_assign(vector2_in));
  compiler.add_node(
      NODE_VECTOR_MATH, compiler.stack_assign(value_out), compiler.stack_assign(vector_out));
}

void VectorMathNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "type");
  compiler.add(this, "node_vector_math");
}

/* VectorTransform */

NODE_DEFINE(VectorTransformNode)
{
  NodeType *type = NodeType::add("vector_transform", create, NodeType::SHADER);

  static NodeEnum type_enum;
  type_enum.insert("vector", NODE_VECTOR_TRANSFORM_TYPE_VECTOR);
  type_enum.insert("point", NODE_VECTOR_TRANSFORM_TYPE_POINT);
  type_enum.insert("normal", NODE_VECTOR_TRANSFORM_TYPE_NORMAL);
  SOCKET_ENUM(type, "Type", type_enum, NODE_VECTOR_TRANSFORM_TYPE_VECTOR);

  static NodeEnum space_enum;
  space_enum.insert("world", NODE_VECTOR_TRANSFORM_CONVERT_SPACE_WORLD);
  space_enum.insert("object", NODE_VECTOR_TRANSFORM_CONVERT_SPACE_OBJECT);
  space_enum.insert("camera", NODE_VECTOR_TRANSFORM_CONVERT_SPACE_CAMERA);
  SOCKET_ENUM(convert_from, "Convert From", space_enum, NODE_VECTOR_TRANSFORM_CONVERT_SPACE_WORLD);
  SOCKET_ENUM(convert_to, "Convert To", space_enum, NODE_VECTOR_TRANSFORM_CONVERT_SPACE_OBJECT);

  SOCKET_IN_VECTOR(vector, "Vector", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_OUT_VECTOR(vector, "Vector");

  return type;
}

VectorTransformNode::VectorTransformNode() : ShaderNode(node_type)
{
}

void VectorTransformNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");
  ShaderOutput *vector_out = output("Vector");

  compiler.add_node(
      NODE_VECTOR_TRANSFORM,
      compiler.encode_uchar4(type, convert_from, convert_to),
      compiler.encode_uchar4(compiler.stack_assign(vector_in), compiler.stack_assign(vector_out)));
}

void VectorTransformNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "type");
  compiler.parameter(this, "convert_from");
  compiler.parameter(this, "convert_to");
  compiler.add(this, "node_vector_transform");
}

/* BumpNode */

NODE_DEFINE(BumpNode)
{
  NodeType *type = NodeType::add("bump", create, NodeType::SHADER);

  SOCKET_BOOLEAN(invert, "Invert", false);
  SOCKET_BOOLEAN(use_object_space, "UseObjectSpace", false);

  /* this input is used by the user, but after graph transform it is no longer
   * used and moved to sampler center/x/y instead */
  SOCKET_IN_FLOAT(height, "Height", 1.0f);

  SOCKET_IN_FLOAT(sample_center, "SampleCenter", 0.0f);
  SOCKET_IN_FLOAT(sample_x, "SampleX", 0.0f);
  SOCKET_IN_FLOAT(sample_y, "SampleY", 0.0f);
  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(strength, "Strength", 1.0f);
  SOCKET_IN_FLOAT(distance, "Distance", 0.1f);

  SOCKET_OUT_NORMAL(normal, "Normal");

  return type;
}

BumpNode::BumpNode() : ShaderNode(node_type)
{
  special_type = SHADER_SPECIAL_TYPE_BUMP;
}

void BumpNode::compile(SVMCompiler &compiler)
{
  ShaderInput *center_in = input("SampleCenter");
  ShaderInput *dx_in = input("SampleX");
  ShaderInput *dy_in = input("SampleY");
  ShaderInput *normal_in = input("Normal");
  ShaderInput *strength_in = input("Strength");
  ShaderInput *distance_in = input("Distance");
  ShaderOutput *normal_out = output("Normal");

  /* pack all parameters in the node */
  compiler.add_node(NODE_SET_BUMP,
                    compiler.encode_uchar4(compiler.stack_assign_if_linked(normal_in),
                                           compiler.stack_assign(distance_in),
                                           invert,
                                           use_object_space),
                    compiler.encode_uchar4(compiler.stack_assign(center_in),
                                           compiler.stack_assign(dx_in),
                                           compiler.stack_assign(dy_in),
                                           compiler.stack_assign(strength_in)),
                    compiler.stack_assign(normal_out));
}

void BumpNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "invert");
  compiler.parameter(this, "use_object_space");
  compiler.add(this, "node_bump");
}

void BumpNode::constant_fold(const ConstantFolder &folder)
{
  ShaderInput *height_in = input("Height");
  ShaderInput *normal_in = input("Normal");

  if (height_in->link == NULL) {
    if (normal_in->link == NULL) {
      GeometryNode *geom = new GeometryNode();
      folder.graph->add(geom);
      folder.bypass(geom->output("Normal"));
    }
    else {
      folder.bypass(normal_in->link);
    }
  }

  /* TODO(sergey): Ignore bump with zero strength. */
}

/* Curve node */

CurvesNode::CurvesNode(const NodeType *node_type) : ShaderNode(node_type)
{
}

void CurvesNode::constant_fold(const ConstantFolder &folder, ShaderInput *value_in)
{
  ShaderInput *fac_in = input("Fac");

  /* evaluate fully constant node */
  if (folder.all_inputs_constant()) {
    if (curves.size() == 0) {
      return;
    }

    float3 pos = (value - make_float3(min_x, min_x, min_x)) / (max_x - min_x);
    float3 result;

    result[0] = rgb_ramp_lookup(curves.data(), pos[0], true, true, curves.size()).x;
    result[1] = rgb_ramp_lookup(curves.data(), pos[1], true, true, curves.size()).y;
    result[2] = rgb_ramp_lookup(curves.data(), pos[2], true, true, curves.size()).z;

    folder.make_constant(interp(value, result, fac));
  }
  /* remove no-op node */
  else if (!fac_in->link && fac == 0.0f) {
    /* link is not null because otherwise all inputs are constant */
    folder.bypass(value_in->link);
  }
}

void CurvesNode::compile(SVMCompiler &compiler,
                         int type,
                         ShaderInput *value_in,
                         ShaderOutput *value_out)
{
  if (curves.size() == 0)
    return;

  ShaderInput *fac_in = input("Fac");

  compiler.add_node(type,
                    compiler.encode_uchar4(compiler.stack_assign(fac_in),
                                           compiler.stack_assign(value_in),
                                           compiler.stack_assign(value_out)),
                    __float_as_int(min_x),
                    __float_as_int(max_x));

  compiler.add_node(curves.size());
  for (int i = 0; i < curves.size(); i++)
    compiler.add_node(float3_to_float4(curves[i]));
}

void CurvesNode::compile(OSLCompiler &compiler, const char *name)
{
  if (curves.size() == 0)
    return;

  compiler.parameter_color_array("ramp", curves);
  compiler.parameter(this, "min_x");
  compiler.parameter(this, "max_x");
  compiler.add(this, name);
}

void CurvesNode::compile(SVMCompiler & /*compiler*/)
{
  assert(0);
}

void CurvesNode::compile(OSLCompiler & /*compiler*/)
{
  assert(0);
}

/* RGBCurvesNode */

NODE_DEFINE(RGBCurvesNode)
{
  NodeType *type = NodeType::add("rgb_curves", create, NodeType::SHADER);

  SOCKET_COLOR_ARRAY(curves, "Curves", array<float3>());
  SOCKET_FLOAT(min_x, "Min X", 0.0f);
  SOCKET_FLOAT(max_x, "Max X", 1.0f);

  SOCKET_IN_FLOAT(fac, "Fac", 0.0f);
  SOCKET_IN_COLOR(value, "Color", make_float3(0.0f, 0.0f, 0.0f));

  SOCKET_OUT_COLOR(value, "Color");

  return type;
}

RGBCurvesNode::RGBCurvesNode() : CurvesNode(node_type)
{
}

void RGBCurvesNode::constant_fold(const ConstantFolder &folder)
{
  CurvesNode::constant_fold(folder, input("Color"));
}

void RGBCurvesNode::compile(SVMCompiler &compiler)
{
  CurvesNode::compile(compiler, NODE_RGB_CURVES, input("Color"), output("Color"));
}

void RGBCurvesNode::compile(OSLCompiler &compiler)
{
  CurvesNode::compile(compiler, "node_rgb_curves");
}

/* VectorCurvesNode */

NODE_DEFINE(VectorCurvesNode)
{
  NodeType *type = NodeType::add("vector_curves", create, NodeType::SHADER);

  SOCKET_VECTOR_ARRAY(curves, "Curves", array<float3>());
  SOCKET_FLOAT(min_x, "Min X", 0.0f);
  SOCKET_FLOAT(max_x, "Max X", 1.0f);

  SOCKET_IN_FLOAT(fac, "Fac", 0.0f);
  SOCKET_IN_VECTOR(value, "Vector", make_float3(0.0f, 0.0f, 0.0f));

  SOCKET_OUT_VECTOR(value, "Vector");

  return type;
}

VectorCurvesNode::VectorCurvesNode() : CurvesNode(node_type)
{
}

void VectorCurvesNode::constant_fold(const ConstantFolder &folder)
{
  CurvesNode::constant_fold(folder, input("Vector"));
}

void VectorCurvesNode::compile(SVMCompiler &compiler)
{
  CurvesNode::compile(compiler, NODE_VECTOR_CURVES, input("Vector"), output("Vector"));
}

void VectorCurvesNode::compile(OSLCompiler &compiler)
{
  CurvesNode::compile(compiler, "node_vector_curves");
}

/* RGBRampNode */

NODE_DEFINE(RGBRampNode)
{
  NodeType *type = NodeType::add("rgb_ramp", create, NodeType::SHADER);

  SOCKET_COLOR_ARRAY(ramp, "Ramp", array<float3>());
  SOCKET_FLOAT_ARRAY(ramp_alpha, "Ramp Alpha", array<float>());
  SOCKET_BOOLEAN(interpolate, "Interpolate", true);

  SOCKET_IN_FLOAT(fac, "Fac", 0.0f);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(alpha, "Alpha");

  return type;
}

RGBRampNode::RGBRampNode() : ShaderNode(node_type)
{
}

void RGBRampNode::constant_fold(const ConstantFolder &folder)
{
  if (ramp.size() == 0 || ramp.size() != ramp_alpha.size())
    return;

  if (folder.all_inputs_constant()) {
    float f = clamp(fac, 0.0f, 1.0f) * (ramp.size() - 1);

    /* clamp int as well in case of NaN */
    int i = clamp((int)f, 0, ramp.size() - 1);
    float t = f - (float)i;

    bool use_lerp = interpolate && t > 0.0f;

    if (folder.output == output("Color")) {
      float3 color = rgb_ramp_lookup(ramp.data(), fac, use_lerp, false, ramp.size());
      folder.make_constant(color);
    }
    else if (folder.output == output("Alpha")) {
      float alpha = float_ramp_lookup(ramp_alpha.data(), fac, use_lerp, false, ramp_alpha.size());
      folder.make_constant(alpha);
    }
  }
}

void RGBRampNode::compile(SVMCompiler &compiler)
{
  if (ramp.size() == 0 || ramp.size() != ramp_alpha.size())
    return;

  ShaderInput *fac_in = input("Fac");
  ShaderOutput *color_out = output("Color");
  ShaderOutput *alpha_out = output("Alpha");

  compiler.add_node(NODE_RGB_RAMP,
                    compiler.encode_uchar4(compiler.stack_assign(fac_in),
                                           compiler.stack_assign_if_linked(color_out),
                                           compiler.stack_assign_if_linked(alpha_out)),
                    interpolate);

  compiler.add_node(ramp.size());
  for (int i = 0; i < ramp.size(); i++)
    compiler.add_node(make_float4(ramp[i].x, ramp[i].y, ramp[i].z, ramp_alpha[i]));
}

void RGBRampNode::compile(OSLCompiler &compiler)
{
  if (ramp.size() == 0 || ramp.size() != ramp_alpha.size())
    return;

  compiler.parameter_color_array("ramp_color", ramp);
  compiler.parameter_array("ramp_alpha", ramp_alpha.data(), ramp_alpha.size());
  compiler.parameter(this, "interpolate");

  compiler.add(this, "node_rgb_ramp");
}

/* Set Normal Node */

NODE_DEFINE(SetNormalNode)
{
  NodeType *type = NodeType::add("set_normal", create, NodeType::SHADER);

  SOCKET_IN_VECTOR(direction, "Direction", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_OUT_NORMAL(normal, "Normal");

  return type;
}

SetNormalNode::SetNormalNode() : ShaderNode(node_type)
{
}

void SetNormalNode::compile(SVMCompiler &compiler)
{
  ShaderInput *direction_in = input("Direction");
  ShaderOutput *normal_out = output("Normal");

  compiler.add_node(NODE_CLOSURE_SET_NORMAL,
                    compiler.stack_assign(direction_in),
                    compiler.stack_assign(normal_out));
}

void SetNormalNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_set_normal");
}

/* OSLNode */

OSLNode::OSLNode() : ShaderNode(new NodeType(NodeType::SHADER))
{
  special_type = SHADER_SPECIAL_TYPE_OSL;
}

OSLNode::~OSLNode()
{
  delete type;
}

ShaderNode *OSLNode::clone() const
{
  return OSLNode::create(this->inputs.size(), this);
}

OSLNode *OSLNode::create(size_t num_inputs, const OSLNode *from)
{
  /* allocate space for the node itself and parameters, aligned to 16 bytes
   * assuming that's the most parameter types need */
  size_t node_size = align_up(sizeof(OSLNode), 16);
  size_t inputs_size = align_up(SocketType::max_size(), 16) * num_inputs;

  char *node_memory = (char *)operator new(node_size + inputs_size);
  memset(node_memory, 0, node_size + inputs_size);

  if (!from) {
    return new (node_memory) OSLNode();
  }
  else {
    /* copy input default values and node type for cloning */
    memcpy(node_memory + node_size, (char *)from + node_size, inputs_size);

    OSLNode *node = new (node_memory) OSLNode(*from);
    node->type = new NodeType(*(from->type));
    return node;
  }
}

char *OSLNode::input_default_value()
{
  /* pointer to default value storage, which is the same as our actual value */
  size_t num_inputs = type->inputs.size();
  size_t inputs_size = align_up(SocketType::max_size(), 16) * num_inputs;
  return (char *)this + align_up(sizeof(OSLNode), 16) + inputs_size;
}

void OSLNode::add_input(ustring name, SocketType::Type socket_type)
{
  char *memory = input_default_value();
  size_t offset = memory - (char *)this;
  const_cast<NodeType *>(type)->register_input(
      name, name, socket_type, offset, memory, NULL, NULL, SocketType::LINKABLE);
}

void OSLNode::add_output(ustring name, SocketType::Type socket_type)
{
  const_cast<NodeType *>(type)->register_output(name, name, socket_type);
}

void OSLNode::compile(SVMCompiler &)
{
  /* doesn't work for SVM, obviously ... */
}

void OSLNode::compile(OSLCompiler &compiler)
{
  if (!filepath.empty())
    compiler.add(this, filepath.c_str(), true);
  else
    compiler.add(this, bytecode_hash.c_str(), false);
}

/* Normal Map */

NODE_DEFINE(NormalMapNode)
{
  NodeType *type = NodeType::add("normal_map", create, NodeType::SHADER);

  static NodeEnum space_enum;
  space_enum.insert("tangent", NODE_NORMAL_MAP_TANGENT);
  space_enum.insert("object", NODE_NORMAL_MAP_OBJECT);
  space_enum.insert("world", NODE_NORMAL_MAP_WORLD);
  space_enum.insert("blender_object", NODE_NORMAL_MAP_BLENDER_OBJECT);
  space_enum.insert("blender_world", NODE_NORMAL_MAP_BLENDER_WORLD);
  SOCKET_ENUM(space, "Space", space_enum, NODE_NORMAL_MAP_TANGENT);

  SOCKET_STRING(attribute, "Attribute", ustring());

  SOCKET_IN_NORMAL(normal_osl,
                   "NormalIn",
                   make_float3(0.0f, 0.0f, 0.0f),
                   SocketType::LINK_NORMAL | SocketType::OSL_INTERNAL);
  SOCKET_IN_FLOAT(strength, "Strength", 1.0f);
  SOCKET_IN_COLOR(color, "Color", make_float3(0.5f, 0.5f, 1.0f));

  SOCKET_OUT_NORMAL(normal, "Normal");

  return type;
}

NormalMapNode::NormalMapNode() : ShaderNode(node_type)
{
}

void NormalMapNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface && space == NODE_NORMAL_MAP_TANGENT) {
    if (attribute.empty()) {
      attributes->add(ATTR_STD_UV_TANGENT);
      attributes->add(ATTR_STD_UV_TANGENT_SIGN);
    }
    else {
      attributes->add(ustring((string(attribute.c_str()) + ".tangent").c_str()));
      attributes->add(ustring((string(attribute.c_str()) + ".tangent_sign").c_str()));
    }

    attributes->add(ATTR_STD_VERTEX_NORMAL);
  }

  ShaderNode::attributes(shader, attributes);
}

void NormalMapNode::compile(SVMCompiler &compiler)
{
  ShaderInput *color_in = input("Color");
  ShaderInput *strength_in = input("Strength");
  ShaderOutput *normal_out = output("Normal");
  int attr = 0, attr_sign = 0;

  if (space == NODE_NORMAL_MAP_TANGENT) {
    if (attribute.empty()) {
      attr = compiler.attribute(ATTR_STD_UV_TANGENT);
      attr_sign = compiler.attribute(ATTR_STD_UV_TANGENT_SIGN);
    }
    else {
      attr = compiler.attribute(ustring((string(attribute.c_str()) + ".tangent").c_str()));
      attr_sign = compiler.attribute(
          ustring((string(attribute.c_str()) + ".tangent_sign").c_str()));
    }
  }

  compiler.add_node(NODE_NORMAL_MAP,
                    compiler.encode_uchar4(compiler.stack_assign(color_in),
                                           compiler.stack_assign(strength_in),
                                           compiler.stack_assign(normal_out),
                                           space),
                    attr,
                    attr_sign);
}

void NormalMapNode::compile(OSLCompiler &compiler)
{
  if (space == NODE_NORMAL_MAP_TANGENT) {
    if (attribute.empty()) {
      compiler.parameter("attr_name", ustring("geom:tangent"));
      compiler.parameter("attr_sign_name", ustring("geom:tangent_sign"));
    }
    else {
      compiler.parameter("attr_name", ustring((string(attribute.c_str()) + ".tangent").c_str()));
      compiler.parameter("attr_sign_name",
                         ustring((string(attribute.c_str()) + ".tangent_sign").c_str()));
    }
  }

  compiler.parameter(this, "space");
  compiler.add(this, "node_normal_map");
}

/* Tangent */

NODE_DEFINE(TangentNode)
{
  NodeType *type = NodeType::add("tangent", create, NodeType::SHADER);

  static NodeEnum direction_type_enum;
  direction_type_enum.insert("radial", NODE_TANGENT_RADIAL);
  direction_type_enum.insert("uv_map", NODE_TANGENT_UVMAP);
  SOCKET_ENUM(direction_type, "Direction Type", direction_type_enum, NODE_TANGENT_RADIAL);

  static NodeEnum axis_enum;
  axis_enum.insert("x", NODE_TANGENT_AXIS_X);
  axis_enum.insert("y", NODE_TANGENT_AXIS_Y);
  axis_enum.insert("z", NODE_TANGENT_AXIS_Z);
  SOCKET_ENUM(axis, "Axis", axis_enum, NODE_TANGENT_AXIS_X);

  SOCKET_STRING(attribute, "Attribute", ustring());

  SOCKET_IN_NORMAL(normal_osl,
                   "NormalIn",
                   make_float3(0.0f, 0.0f, 0.0f),
                   SocketType::LINK_NORMAL | SocketType::OSL_INTERNAL);
  SOCKET_OUT_NORMAL(tangent, "Tangent");

  return type;
}

TangentNode::TangentNode() : ShaderNode(node_type)
{
}

void TangentNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface) {
    if (direction_type == NODE_TANGENT_UVMAP) {
      if (attribute.empty())
        attributes->add(ATTR_STD_UV_TANGENT);
      else
        attributes->add(ustring((string(attribute.c_str()) + ".tangent").c_str()));
    }
    else
      attributes->add(ATTR_STD_GENERATED);
  }

  ShaderNode::attributes(shader, attributes);
}

void TangentNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *tangent_out = output("Tangent");
  int attr;

  if (direction_type == NODE_TANGENT_UVMAP) {
    if (attribute.empty())
      attr = compiler.attribute(ATTR_STD_UV_TANGENT);
    else
      attr = compiler.attribute(ustring((string(attribute.c_str()) + ".tangent").c_str()));
  }
  else
    attr = compiler.attribute(ATTR_STD_GENERATED);

  compiler.add_node(
      NODE_TANGENT,
      compiler.encode_uchar4(compiler.stack_assign(tangent_out), direction_type, axis),
      attr);
}

void TangentNode::compile(OSLCompiler &compiler)
{
  if (direction_type == NODE_TANGENT_UVMAP) {
    if (attribute.empty())
      compiler.parameter("attr_name", ustring("geom:tangent"));
    else
      compiler.parameter("attr_name", ustring((string(attribute.c_str()) + ".tangent").c_str()));
  }

  compiler.parameter(this, "direction_type");
  compiler.parameter(this, "axis");
  compiler.add(this, "node_tangent");
}

/* Bevel */

NODE_DEFINE(BevelNode)
{
  NodeType *type = NodeType::add("bevel", create, NodeType::SHADER);

  SOCKET_INT(samples, "Samples", 4);

  SOCKET_IN_FLOAT(radius, "Radius", 0.05f);
  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_NORMAL);

  SOCKET_OUT_NORMAL(bevel, "Normal");

  return type;
}

BevelNode::BevelNode() : ShaderNode(node_type)
{
}

void BevelNode::compile(SVMCompiler &compiler)
{
  ShaderInput *radius_in = input("Radius");
  ShaderInput *normal_in = input("Normal");
  ShaderOutput *normal_out = output("Normal");

  compiler.add_node(NODE_BEVEL,
                    compiler.encode_uchar4(samples,
                                           compiler.stack_assign(radius_in),
                                           compiler.stack_assign_if_linked(normal_in),
                                           compiler.stack_assign(normal_out)));
}

void BevelNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "samples");
  compiler.add(this, "node_bevel");
}

/* Displacement */

NODE_DEFINE(DisplacementNode)
{
  NodeType *type = NodeType::add("displacement", create, NodeType::SHADER);

  static NodeEnum space_enum;
  space_enum.insert("object", NODE_NORMAL_MAP_OBJECT);
  space_enum.insert("world", NODE_NORMAL_MAP_WORLD);

  SOCKET_ENUM(space, "Space", space_enum, NODE_NORMAL_MAP_OBJECT);

  SOCKET_IN_FLOAT(height, "Height", 0.0f);
  SOCKET_IN_FLOAT(midlevel, "Midlevel", 0.5f);
  SOCKET_IN_FLOAT(scale, "Scale", 1.0f);
  SOCKET_IN_NORMAL(normal, "Normal", make_float3(0.0f, 0.0f, 0.0f), SocketType::LINK_NORMAL);

  SOCKET_OUT_VECTOR(displacement, "Displacement");

  return type;
}

DisplacementNode::DisplacementNode() : ShaderNode(node_type)
{
}

void DisplacementNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    if ((height - midlevel == 0.0f) || (scale == 0.0f)) {
      folder.make_zero();
    }
  }
}

void DisplacementNode::compile(SVMCompiler &compiler)
{
  ShaderInput *height_in = input("Height");
  ShaderInput *midlevel_in = input("Midlevel");
  ShaderInput *scale_in = input("Scale");
  ShaderInput *normal_in = input("Normal");
  ShaderOutput *displacement_out = output("Displacement");

  compiler.add_node(NODE_DISPLACEMENT,
                    compiler.encode_uchar4(compiler.stack_assign(height_in),
                                           compiler.stack_assign(midlevel_in),
                                           compiler.stack_assign(scale_in),
                                           compiler.stack_assign_if_linked(normal_in)),
                    compiler.stack_assign(displacement_out),
                    space);
}

void DisplacementNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "space");
  compiler.add(this, "node_displacement");
}

/* Vector Displacement */

NODE_DEFINE(VectorDisplacementNode)
{
  NodeType *type = NodeType::add("vector_displacement", create, NodeType::SHADER);

  static NodeEnum space_enum;
  space_enum.insert("tangent", NODE_NORMAL_MAP_TANGENT);
  space_enum.insert("object", NODE_NORMAL_MAP_OBJECT);
  space_enum.insert("world", NODE_NORMAL_MAP_WORLD);

  SOCKET_ENUM(space, "Space", space_enum, NODE_NORMAL_MAP_TANGENT);
  SOCKET_STRING(attribute, "Attribute", ustring());

  SOCKET_IN_COLOR(vector, "Vector", make_float3(0.0f, 0.0f, 0.0f));
  SOCKET_IN_FLOAT(midlevel, "Midlevel", 0.0f);
  SOCKET_IN_FLOAT(scale, "Scale", 1.0f);

  SOCKET_OUT_VECTOR(displacement, "Displacement");

  return type;
}

VectorDisplacementNode::VectorDisplacementNode() : ShaderNode(node_type)
{
}

void VectorDisplacementNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    if ((vector == make_float3(0.0f, 0.0f, 0.0f) && midlevel == 0.0f) || (scale == 0.0f)) {
      folder.make_zero();
    }
  }
}

void VectorDisplacementNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface && space == NODE_NORMAL_MAP_TANGENT) {
    if (attribute.empty()) {
      attributes->add(ATTR_STD_UV_TANGENT);
      attributes->add(ATTR_STD_UV_TANGENT_SIGN);
    }
    else {
      attributes->add(ustring((string(attribute.c_str()) + ".tangent").c_str()));
      attributes->add(ustring((string(attribute.c_str()) + ".tangent_sign").c_str()));
    }

    attributes->add(ATTR_STD_VERTEX_NORMAL);
  }

  ShaderNode::attributes(shader, attributes);
}

void VectorDisplacementNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");
  ShaderInput *midlevel_in = input("Midlevel");
  ShaderInput *scale_in = input("Scale");
  ShaderOutput *displacement_out = output("Displacement");
  int attr = 0, attr_sign = 0;

  if (space == NODE_NORMAL_MAP_TANGENT) {
    if (attribute.empty()) {
      attr = compiler.attribute(ATTR_STD_UV_TANGENT);
      attr_sign = compiler.attribute(ATTR_STD_UV_TANGENT_SIGN);
    }
    else {
      attr = compiler.attribute(ustring((string(attribute.c_str()) + ".tangent").c_str()));
      attr_sign = compiler.attribute(
          ustring((string(attribute.c_str()) + ".tangent_sign").c_str()));
    }
  }

  compiler.add_node(NODE_VECTOR_DISPLACEMENT,
                    compiler.encode_uchar4(compiler.stack_assign(vector_in),
                                           compiler.stack_assign(midlevel_in),
                                           compiler.stack_assign(scale_in),
                                           compiler.stack_assign(displacement_out)),
                    attr,
                    attr_sign);

  compiler.add_node(space);
}

void VectorDisplacementNode::compile(OSLCompiler &compiler)
{
  if (space == NODE_NORMAL_MAP_TANGENT) {
    if (attribute.empty()) {
      compiler.parameter("attr_name", ustring("geom:tangent"));
      compiler.parameter("attr_sign_name", ustring("geom:tangent_sign"));
    }
    else {
      compiler.parameter("attr_name", ustring((string(attribute.c_str()) + ".tangent").c_str()));
      compiler.parameter("attr_sign_name",
                         ustring((string(attribute.c_str()) + ".tangent_sign").c_str()));
    }
  }

  compiler.parameter(this, "space");
  compiler.add(this, "node_vector_displacement");
}

CCL_NAMESPACE_END
