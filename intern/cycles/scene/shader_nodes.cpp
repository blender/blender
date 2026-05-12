/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "shader_nodes.h"

#include "kernel/svm/node_types.h"
#include "kernel/svm/types.h"
#include "kernel/types.h"

#include "scene/constant_fold.h"
#include "scene/film.h"
#include "scene/image.h"
#include "scene/image_sky.h"
#include "scene/integrator.h"
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/osl.h"
#include "scene/scene.h"
#include "scene/shader_nodes.h"
#include "scene/svm.h"

#include "sky_hosek.h"
#include "sky_nishita.h"

#include "util/colorspace.h"
#include "util/log.h"
#include "util/math_base.h"
#include "util/math_float3.h"
#include "util/string.h"
#include "util/transform.h"

#include "kernel/svm/color_util.h"
#include "kernel/svm/mapping_util.h"
#include "kernel/svm/math_util.h"
#include "kernel/svm/ramp_util.h"

#include <cassert>
#include <limits>
#include <mutex>

CCL_NAMESPACE_BEGIN

/* Texture Mapping */

#define TEXTURE_MAPPING_DEFINE(TextureNode) \
  SOCKET_POINT(tex_mapping.translation, "Translation", zero_float3()); \
  SOCKET_VECTOR(tex_mapping.rotation, "Rotation", zero_float3()); \
  SOCKET_VECTOR(tex_mapping.scale, "Scale", one_float3()); \
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

TextureMapping::TextureMapping() = default;

Transform TextureMapping::compute_transform()
{
  Transform mmat = transform_scale(zero_float3());

  if (x_mapping != NONE) {
    mmat[0][x_mapping - 1] = 1.0f;
  }
  if (y_mapping != NONE) {
    mmat[1][y_mapping - 1] = 1.0f;
  }
  if (z_mapping != NONE) {
    mmat[2][z_mapping - 1] = 1.0f;
  }

  float3 scale_clamped = scale;

  if (type == TEXTURE || type == NORMAL) {
    /* keep matrix invertible */
    if (fabsf(scale.x) < 1e-5f) {
      scale_clamped.x = signf(scale.x) * 1e-5f;
    }
    if (fabsf(scale.y) < 1e-5f) {
      scale_clamped.y = signf(scale.y) * 1e-5f;
    }
    if (fabsf(scale.z) < 1e-5f) {
      scale_clamped.z = signf(scale.z) * 1e-5f;
    }
  }

  const Transform smat = transform_scale(scale_clamped);
  const Transform rmat = transform_euler(rotation);
  const Transform tmat = transform_translate(translation);

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
  if (translation != zero_float3()) {
    return false;
  }
  if (rotation != zero_float3()) {
    return false;
  }
  if (scale != one_float3()) {
    return false;
  }

  if (x_mapping != X || y_mapping != Y || z_mapping != Z) {
    return false;
  }
  if (use_minmax) {
    return false;
  }

  return true;
}

void TextureMapping::compile(SVMCompiler &compiler,
                             const SVMStackOffset offset_in,
                             const SVMStackOffset offset_out)
{
  const Transform tfm = compute_transform();
  compiler.add_node(nullptr,
                    NODE_TEXTURE_MAPPING,
                    SVMNodeTextureMapping{
                        .vec_offset = offset_in,
                        .out_offset = offset_out,
                        .tfm = tfm,
                    });

  if (use_minmax) {
    compiler.add_node(nullptr,
                      NODE_MIN_MAX,
                      SVMNodeMinMax{
                          .vec_offset = offset_out,
                          .out_offset = offset_out,
                          .mn = min,
                          .mx = max,
                      });
  }

  if (type == NORMAL) {
    compiler.add_node(nullptr,
                      NODE_VECTOR_MATH,
                      SVMNodeVectorMath{
                          .math_type = NODE_VECTOR_MATH_NORMALIZE,
                          .a = compiler.input_float3_from_offset(offset_out),
                          .b = {},
                          .c = {},
                          .param1 = {},
                          .value_offset = SVM_STACK_INVALID,
                          .vector_offset = offset_out,
                      });
  }
}

/* Convenience function for texture nodes, allocating stack space to output
 * a modified vector and returning its offset */
SVMStackOffset TextureMapping::compile_begin(SVMCompiler &compiler, ShaderInput *vector_in)
{
  if (!skip()) {
    const SVMStackOffset offset_in = compiler.stack_assign(vector_in);
    assert(vector_in->type() == SocketType::VECTOR || vector_in->type() == SocketType::POINT);
    const SVMStackOffset offset_out = compiler.stack_find_offset(vector_in);

    compile(compiler, offset_in, offset_out);

    return offset_out;
  }

  return compiler.stack_assign(vector_in);
}

void TextureMapping::compile_end(SVMCompiler &compiler,
                                 ShaderInput *vector_in,
                                 const SVMStackOffset vector_offset)
{
  if (!skip()) {
    compiler.stack_clear_offset(vector_in, vector_offset);
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

  static NodeEnum alpha_type_enum;
  alpha_type_enum.insert("auto", IMAGE_ALPHA_AUTO);
  alpha_type_enum.insert("unassociated", IMAGE_ALPHA_UNASSOCIATED);
  alpha_type_enum.insert("associated", IMAGE_ALPHA_ASSOCIATED);
  alpha_type_enum.insert("channel_packed", IMAGE_ALPHA_CHANNEL_PACKED);
  alpha_type_enum.insert("ignore", IMAGE_ALPHA_IGNORE);
  SOCKET_ENUM(alpha_type, "Alpha Type", alpha_type_enum, IMAGE_ALPHA_AUTO);

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
  extension_enum.insert("mirror", EXTENSION_MIRROR);
  SOCKET_ENUM(extension, "Extension", extension_enum, EXTENSION_REPEAT);

  static NodeEnum projection_enum;
  projection_enum.insert("flat", NODE_IMAGE_PROJ_FLAT);
  projection_enum.insert("box", NODE_IMAGE_PROJ_BOX);
  projection_enum.insert("sphere", NODE_IMAGE_PROJ_SPHERE);
  projection_enum.insert("tube", NODE_IMAGE_PROJ_TUBE);
  SOCKET_ENUM(projection, "Projection", projection_enum, NODE_IMAGE_PROJ_FLAT);

  SOCKET_FLOAT(projection_blend, "Projection Blend", 0.0f);

  SOCKET_INT_ARRAY(tiles, "Tiles", array<int>());
  SOCKET_BOOLEAN(animated, "Animated", false);

  SOCKET_IN_POINT(vector, "Vector", zero_float3(), SocketType::LINK_TEXTURE_UV);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(alpha, "Alpha");

  return type;
}

ImageTextureNode::ImageTextureNode() : ImageSlotTextureNode(get_node_type())
{
  colorspace = u_colorspace_scene_linear;
  animated = false;
}

ShaderNode *ImageTextureNode::clone(ShaderGraph *graph) const
{
  ImageTextureNode *node = graph->create_node<ImageTextureNode>(*this);
  node->handle = handle;
  return node;
}

ImageParams ImageTextureNode::image_params() const
{
  ImageParams params;
  params.animated = animated;
  params.interpolation = interpolation;
  params.extension = extension;
  params.alpha_type = alpha_type;
  params.colorspace = colorspace;
  return params;
}

void ImageTextureNode::cull_tiles(Scene *scene, ShaderGraph *graph)
{
  /* Box projection computes its own UVs that always lie in the
   * 1001 tile, so there's no point in loading any others. */
  if (projection == NODE_IMAGE_PROJ_BOX) {
    if (tiles.size()) {
      tiles.clear();
      tiles.push_back_slow(1001);
    }
    return;
  }

  if (!scene->params.background) {
    /* During interactive renders, all tiles are loaded.
     * While we could support updating this when UVs change, that could lead
     * to annoying interruptions when loading images while editing UVs. */
    return;
  }

  /* Only check UVs for tile culling when using tiles. */
  if (tiles.size() == 0) {
    return;
  }

  ShaderInput *vector_in = input("Vector");
  ustring attribute;
  if (vector_in->link) {
    ShaderNode *node = vector_in->link->parent;
    if (node->type == UVMapNode::get_node_type()) {
      UVMapNode *uvmap = (UVMapNode *)node;
      attribute = uvmap->get_attribute();
    }
    else if (node->type == TextureCoordinateNode::get_node_type()) {
      if (vector_in->link != node->output("UV")) {
        return;
      }
    }
    else {
      return;
    }
  }

  unordered_set<int> used_tiles;
  /* TODO(lukas): This is quite inefficient. A fairly simple improvement would
   * be to have a cache in each mesh that is indexed by attribute.
   * Additionally, building a graph-to-meshes list once could help. */
  for (Geometry *geom : scene->geometry) {
    for (Node *node : geom->get_used_shaders()) {
      Shader *shader = static_cast<Shader *>(node);
      if (shader->graph.get() == graph) {
        geom->get_uv_tiles(attribute, used_tiles);
      }
    }
  }

  array<int> new_tiles;
  for (const int tile : tiles) {
    if (used_tiles.contains(tile)) {
      new_tiles.push_back_slow(tile);
    }
  }
  tiles.steal_data(new_tiles);
}

void ImageTextureNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
#ifdef WITH_PTEX
  /* todo: avoid loading other texture coordinates when using ptex,
   * and hide texture coordinate socket in the UI */
  if (shader->has_surface_link() && string_endswith(filename, ".ptx")) {
    /* ptex */
    attributes->add(ATTR_STD_PTEX_FACE_ID);
    attributes->add(ATTR_STD_PTEX_UV);
  }
#endif

  ShaderNode::attributes(shader, attributes);
}

ShaderNodeType ImageTextureNode::shader_node_type() const
{
  if (projection != NODE_IMAGE_PROJ_BOX) {
    return NODE_TEX_IMAGE;
  }
  return NODE_TEX_IMAGE_BOX;
}

void ImageTextureNode::update_images(const SVMCompiler &compiler)
{
  if (handle.empty()) {
    ImageManager *image_manager = compiler.scene->image_manager.get();
    const bool use_cache = image_manager->get_use_texture_cache();

    if (!use_cache) {
      cull_tiles(compiler.scene, compiler.current_graph);
    }

    handle = image_manager->add_image(filename.string(), image_params(), tiles);

    if (use_cache && !tiles.empty() && !image_manager->get_auto_texture_cache()) {
      if (!handle.all_udim_tiled(compiler.progress)) {
        cull_tiles(compiler.scene, compiler.current_graph);
        handle = image_manager->add_image(filename.string(), image_params(), tiles);
      }
    }
  }

  const ImageMetaData metadata = handle.metadata(compiler.progress);
  if (metadata.has_tiles_and_mipmaps && compiler.scene->image_manager->get_use_texture_cache()) {
    set_need_derivatives();
  }
}

void ImageTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");
  ShaderOutput *alpha_out = output("Alpha");

  update_images(compiler);

  /* All tiles have the same metadata. */
  const ImageMetaData metadata = handle.metadata(compiler.progress);
  const bool compress_as_srgb = metadata.is_compressible_as_srgb;

  const SVMStackOffset vector_offset = tex_mapping.compile_begin(compiler, vector_in);
  uint flags = 0;

  if (compress_as_srgb) {
    flags |= NODE_IMAGE_COMPRESS_AS_SRGB;
  }
  if (!alpha_out->links.empty()) {
    const bool unassociate_alpha = !(ColorSpaceManager::colorspace_is_data(colorspace) ||
                                     alpha_type == IMAGE_ALPHA_CHANNEL_PACKED ||
                                     alpha_type == IMAGE_ALPHA_IGNORE);

    if (unassociate_alpha) {
      flags |= NODE_IMAGE_ALPHA_UNASSOCIATE;
    }
  }

  if (projection != NODE_IMAGE_PROJ_BOX) {
    compiler.add_node(this,
                      NODE_TEX_IMAGE,
                      SVMNodeTexImage{
                          .id = handle.kernel_id(),
                          .projection = uint(projection),
                          .flags = uint8_t(flags),
                          .co = vector_offset,
                          .out_offset = compiler.output("Color"),
                          .alpha_offset = compiler.output("Alpha"),
                      });
  }
  else {
    compiler.add_node(this,
                      NODE_TEX_IMAGE_BOX,
                      SVMNodeTexImageBox{
                          .id = handle.kernel_id(),
                          .blend = projection_blend,
                          .flags = uint8_t(flags),
                          .co = vector_offset,
                          .out_offset = compiler.output("Color"),
                          .alpha_offset = compiler.output("Alpha"),
                      });
  }

  tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void ImageTextureNode::compile(OSLCompiler &compiler)
{

  tex_mapping.compile(compiler);

  if (handle.empty()) {
    cull_tiles(compiler.scene, compiler.current_graph);
    ImageManager *image_manager = compiler.scene->image_manager.get();
    const bool use_cache = image_manager->get_use_texture_cache();

    if (!use_cache) {
      cull_tiles(compiler.scene, compiler.current_graph);
    }

    handle = image_manager->add_image(filename.string(), image_params(), tiles);

    if (use_cache && !tiles.empty() && !image_manager->get_auto_texture_cache()) {
      if (!handle.all_udim_tiled(compiler.progress)) {
        cull_tiles(compiler.scene, compiler.current_graph);
        handle = image_manager->add_image(filename.string(), image_params(), tiles);
      }
    }
  }

  const ImageMetaData metadata = handle.metadata(compiler.progress);
  const bool is_float = metadata.is_float();
  const bool compress_as_srgb = metadata.is_compressible_as_srgb;

  compiler.parameter_texture("filename", handle);

  const bool unassociate_alpha = !(ColorSpaceManager::colorspace_is_data(colorspace) ||
                                   alpha_type == IMAGE_ALPHA_CHANNEL_PACKED ||
                                   alpha_type == IMAGE_ALPHA_IGNORE);

  compiler.parameter(this, "projection");
  compiler.parameter(this, "projection_blend");
  compiler.parameter("compress_as_srgb", compress_as_srgb);
  compiler.parameter("ignore_alpha", alpha_type == IMAGE_ALPHA_IGNORE);
  compiler.parameter("unassociate_alpha", !output("Alpha")->links.empty() && unassociate_alpha);
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

  static NodeEnum alpha_type_enum;
  alpha_type_enum.insert("auto", IMAGE_ALPHA_AUTO);
  alpha_type_enum.insert("unassociated", IMAGE_ALPHA_UNASSOCIATED);
  alpha_type_enum.insert("associated", IMAGE_ALPHA_ASSOCIATED);
  alpha_type_enum.insert("channel_packed", IMAGE_ALPHA_CHANNEL_PACKED);
  alpha_type_enum.insert("ignore", IMAGE_ALPHA_IGNORE);
  SOCKET_ENUM(alpha_type, "Alpha Type", alpha_type_enum, IMAGE_ALPHA_AUTO);

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

  SOCKET_BOOLEAN(animated, "Animated", false);

  SOCKET_IN_POINT(vector, "Vector", zero_float3(), SocketType::LINK_POSITION);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(alpha, "Alpha");

  return type;
}

EnvironmentTextureNode::EnvironmentTextureNode() : ImageSlotTextureNode(get_node_type())
{
  colorspace = u_colorspace_scene_linear;
  animated = false;
}

ShaderNode *EnvironmentTextureNode::clone(ShaderGraph *graph) const
{
  EnvironmentTextureNode *node = graph->create_node<EnvironmentTextureNode>(*this);
  node->handle = handle;
  return node;
}

ImageParams EnvironmentTextureNode::image_params() const
{
  ImageParams params;
  params.animated = animated;
  params.interpolation = interpolation;
  params.extension = EXTENSION_REPEAT;
  params.alpha_type = alpha_type;
  params.colorspace = colorspace;
  return params;
}

void EnvironmentTextureNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
#ifdef WITH_PTEX
  if (shader->has_surface_link() && string_endswith(filename, ".ptx")) {
    /* ptex */
    attributes->add(ATTR_STD_PTEX_FACE_ID);
    attributes->add(ATTR_STD_PTEX_UV);
  }
#endif

  ShaderNode::attributes(shader, attributes);
}

void EnvironmentTextureNode::update_images(const SVMCompiler &compiler)
{
  if (handle.empty()) {
    ImageManager *image_manager = compiler.scene->image_manager.get();
    handle = image_manager->add_image(filename.string(), image_params());
  }

  const ImageMetaData metadata = handle.metadata(compiler.progress);
  if (metadata.has_tiles_and_mipmaps && compiler.scene->image_manager->get_use_texture_cache()) {
    set_need_derivatives();
  }
}

void EnvironmentTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");

  update_images(compiler);

  const ImageMetaData metadata = handle.metadata(compiler.progress);
  const bool compress_as_srgb = metadata.is_compressible_as_srgb;

  const SVMStackOffset vector_offset = tex_mapping.compile_begin(compiler, vector_in);
  uint flags = 0;

  if (compress_as_srgb) {
    flags |= NODE_IMAGE_COMPRESS_AS_SRGB;
  }

  compiler.add_node(this,
                    NODE_TEX_ENVIRONMENT,
                    SVMNodeTexEnvironment{
                        .id = handle.kernel_id(),
                        .projection = projection,
                        .flags = uint8_t(flags),
                        .co = vector_offset,
                        .out_offset = compiler.output("Color"),
                        .alpha_offset = compiler.output("Alpha"),
                    });

  tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void EnvironmentTextureNode::compile(OSLCompiler &compiler)
{
  if (handle.empty()) {
    ImageManager *image_manager = compiler.scene->image_manager.get();
    handle = image_manager->add_image(filename.string(), image_params());
  }

  tex_mapping.compile(compiler);

  const ImageMetaData metadata = handle.metadata(compiler.progress);
  const bool is_float = metadata.is_float();
  const bool compress_as_srgb = metadata.is_compressible_as_srgb;

  compiler.parameter_texture("filename", handle);
  compiler.parameter(this, "projection");
  compiler.parameter(this, "interpolation");
  compiler.parameter("compress_as_srgb", compress_as_srgb);
  compiler.parameter("ignore_alpha", alpha_type == IMAGE_ALPHA_IGNORE);
  compiler.parameter("is_float", is_float);
  compiler.add(this, "node_environment_texture");
}

/* Sky Texture */

static float2 sky_spherical_coordinates(const float3 dir)
{
  return make_float2(acosf(dir.z), atan2f(dir.x, dir.y));
}

struct SunSky {
  /* sun direction in spherical and cartesian */
  float theta, phi;

  /* Parameter */
  float radiance_x, radiance_y, radiance_z;
  float config_x[9], config_y[9], config_z[9], nishita_data[11];
};

/* Preetham model */
static float sky_perez_function(const float lam[6], float theta, const float gamma)
{
  return (1.0f + lam[0] * expf(lam[1] / cosf(theta))) *
         (1.0f + lam[2] * expf(lam[3] * gamma) + lam[4] * cosf(gamma) * cosf(gamma));
}

static void sky_texture_precompute_preetham(SunSky *sunsky,
                                            const float3 dir,
                                            const float turbidity)
{
  /*
   * We re-use the SunSky struct of the new model, to avoid extra variables
   * zenith_Y/x/y is now radiance_x/y/z
   * perez_Y/x/y is now config_x/y/z
   */

  const float2 spherical = sky_spherical_coordinates(dir);
  const float theta = spherical.x;
  const float phi = spherical.y;

  sunsky->theta = theta;
  sunsky->phi = phi;

  const float theta2 = theta * theta;
  const float theta3 = theta2 * theta;
  const float T = turbidity;
  const float T2 = T * T;

  const float chi = (4.0f / 9.0f - T / 120.0f) * (M_PI_F - 2.0f * theta);
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
static void sky_texture_precompute_hosek(SunSky *sunsky,
                                         const float3 dir,
                                         float turbidity,
                                         const float ground_albedo)
{
  /* Calculate Sun Direction and save coordinates */
  const float2 spherical = sky_spherical_coordinates(dir);
  float theta = spherical.x;
  const float phi = spherical.y;

  /* Clamp Turbidity */
  turbidity = clamp(turbidity, 0.0f, 10.0f);

  /* Clamp to Horizon */
  theta = clamp(theta, 0.0f, M_PI_2_F);

  sunsky->theta = theta;
  sunsky->phi = phi;

  const float solarElevation = M_PI_2_F - theta;

  /* Initialize Sky Model */
  SKY_ArHosekSkyModelState *sky_state;
  sky_state = SKY_arhosek_xyz_skymodelstate_alloc_init(
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
  SKY_arhosekskymodelstate_free(sky_state);
}

/* Nishita improved */
static void sky_texture_precompute_nishita(SunSky *sunsky,
                                           bool multiple_scattering,
                                           bool sun_disc,
                                           const float sun_size,
                                           const float sun_intensity,
                                           const float sun_elevation,
                                           const float sun_rotation,
                                           const float altitude,
                                           const float air_density,
                                           const float aerosol_density,
                                           const float ozone_density)
{
  /* Sample 2 Sun pixels */
  float pixel_bottom[3];
  float pixel_top[3];

  if (multiple_scattering) {
    SKY_multiple_scattering_precompute_sun(sun_elevation,
                                           sun_size,
                                           altitude,
                                           air_density,
                                           aerosol_density,
                                           ozone_density,
                                           pixel_bottom,
                                           pixel_top);
  }
  else {
    SKY_single_scattering_precompute_sun(
        sun_elevation, sun_size, altitude, air_density, aerosol_density, pixel_bottom, pixel_top);
  }

  float earth_intersection_angle = SKY_earth_intersection_angle(altitude);

  /* Send data to sky.h */
  sunsky->nishita_data[0] = pixel_bottom[0];
  sunsky->nishita_data[1] = pixel_bottom[1];
  sunsky->nishita_data[2] = pixel_bottom[2];
  sunsky->nishita_data[3] = pixel_top[0];
  sunsky->nishita_data[4] = pixel_top[1];
  sunsky->nishita_data[5] = pixel_top[2];
  sunsky->nishita_data[6] = sun_elevation;
  sunsky->nishita_data[7] = sun_rotation;
  sunsky->nishita_data[8] = sun_disc ? sun_size : -1.0f;
  sunsky->nishita_data[9] = sun_intensity;
  sunsky->nishita_data[10] = -earth_intersection_angle;
}

float SkyTextureNode::get_sun_average_radiance()
{
  const float angular_diameter = get_sun_size();
  float pix_bottom[3];
  float pix_top[3];

  if (sky_type == NODE_SKY_SINGLE_SCATTERING) {
    SKY_single_scattering_precompute_sun(sun_elevation,
                                         angular_diameter,
                                         altitude,
                                         air_density,
                                         aerosol_density,
                                         pix_bottom,
                                         pix_top);
  }
  else {
    SKY_multiple_scattering_precompute_sun(sun_elevation,
                                           angular_diameter,
                                           altitude,
                                           air_density,
                                           aerosol_density,
                                           ozone_density,
                                           pix_bottom,
                                           pix_top);
  }

  /* Sample center of Sun. */
  const float3 pixel_bottom = make_float3(pix_bottom[0], pix_bottom[1], pix_bottom[2]);
  const float3 pixel_top = make_float3(pix_top[0], pix_top[1], pix_top[2]);
  float3 xyz = interp(pixel_bottom, pixel_top, 0.5f) * sun_intensity;

  /* We first approximate the Sun's contribution by
   * multiplying the evaluated point by the square of the angular diameter.
   * Then we scale the approximation using a piecewise function (determined empirically). */
  float sun_contribution = average(xyz) * sqr(angular_diameter);

  const float first_point = 0.8f / 180.0f * M_PI_F;
  const float second_point = 1.0f / 180.0f * M_PI_F;
  const float third_point = M_PI_2_F;
  if (angular_diameter < first_point) {
    sun_contribution *= 1.0f;
  }
  else if (angular_diameter < second_point) {
    const float diff = angular_diameter - first_point;
    const float slope = (0.8f - 1.0f) / (second_point - first_point);
    sun_contribution *= 1.0f + slope * diff;
  }
  else {
    const float diff = angular_diameter - 1.0f / 180.0f * M_PI_F;
    const float slope = (0.45f - 0.8f) / (third_point - second_point);
    sun_contribution *= 0.8f + slope * diff;
  }

  return sun_contribution;
}

NODE_DEFINE(SkyTextureNode)
{
  NodeType *type = NodeType::add("sky_texture", create, NodeType::SHADER);
  TEXTURE_MAPPING_DEFINE(SkyTextureNode);
  static NodeEnum type_enum;
  type_enum.insert("preetham", NODE_SKY_PREETHAM);
  type_enum.insert("hosek_wilkie", NODE_SKY_HOSEK);
  type_enum.insert("single_scattering", NODE_SKY_SINGLE_SCATTERING);
  type_enum.insert("multiple_scattering", NODE_SKY_MULTIPLE_SCATTERING);
  SOCKET_ENUM(sky_type, "Type", type_enum, NODE_SKY_MULTIPLE_SCATTERING);

  /* Nishita parameters. */
  SOCKET_BOOLEAN(sun_disc, "Sun Disc", true);
  SOCKET_FLOAT(sun_size, "Sun Size", 0.009512f);
  SOCKET_FLOAT(sun_intensity, "Sun Intensity", 1.0f);
  SOCKET_FLOAT(sun_elevation, "Sun Elevation", 15.0f * M_PI_F / 180.0f);
  SOCKET_FLOAT(sun_rotation, "Sun Rotation", 0.0f);
  SOCKET_FLOAT(altitude, "Altitude", 100.0f);
  SOCKET_FLOAT(air_density, "Air", 1.0f);
  SOCKET_FLOAT(aerosol_density, "Aerosol", 1.0f);
  SOCKET_FLOAT(ozone_density, "Ozone", 1.0f);
  SOCKET_IN_POINT(vector, "Vector", zero_float3(), SocketType::LINK_TEXTURE_GENERATED);
  SOCKET_OUT_COLOR(color, "Color");

  /* Legacy parameters. */
  SOCKET_VECTOR(sun_direction, "Sun Direction", make_float3(0.0f, 0.0f, 1.0f));
  SOCKET_FLOAT(turbidity, "Turbidity", 2.2f);
  SOCKET_FLOAT(ground_albedo, "Ground Albedo", 0.3f);

  return type;
}

SkyTextureNode::SkyTextureNode() : TextureNode(get_node_type()) {}

void SkyTextureNode::simplify_settings(Scene * /* scene */)
{
  /* Patch Sun position so users are able to animate the daylight cycle while keeping the shading
   * code simple. */
  float new_sun_elevation = sun_elevation;
  float new_sun_rotation = sun_rotation;

  /* Wrap `new_sun_elevation` into [-2PI..2PI] range. */
  new_sun_elevation = fmodf(new_sun_elevation, M_2PI_F);
  /* Wrap `new_sun_elevation` into [-PI..PI] range. */
  if (fabsf(new_sun_elevation) >= M_PI_F) {
    new_sun_elevation -= copysignf(2.0f, new_sun_elevation) * M_PI_F;
  }
  /* Wrap `new_sun_elevation` into [-PI/2..PI/2] range while keeping the same absolute position. */
  if (new_sun_elevation >= M_PI_2_F || new_sun_elevation <= -M_PI_2_F) {
    new_sun_elevation = copysignf(M_PI_F, new_sun_elevation) - new_sun_elevation;
    new_sun_rotation += M_PI_F;
  }

  /* Wrap `new_sun_rotation` into [-2PI..2PI] range. */
  new_sun_rotation = fmodf(new_sun_rotation, M_2PI_F);
  /* Wrap `new_sun_rotation` into [0..2PI] range. */
  if (new_sun_rotation < 0.0f) {
    new_sun_rotation += M_2PI_F;
  }
  new_sun_rotation = M_2PI_F - new_sun_rotation;

  sun_elevation = new_sun_elevation;
  sun_rotation = new_sun_rotation;

  if (is_modified()) {
    handle.clear();
  }
}

void SkyTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");
  SunSky sunsky = {};

  if (sky_type == NODE_SKY_PREETHAM) {
    sky_texture_precompute_preetham(&sunsky, sun_direction, turbidity);
  }
  else if (sky_type == NODE_SKY_HOSEK) {
    sky_texture_precompute_hosek(&sunsky, sun_direction, turbidity, ground_albedo);
  }
  else {
    sky_texture_precompute_nishita(&sunsky,
                                   sky_type == NODE_SKY_MULTIPLE_SCATTERING,
                                   sun_disc,
                                   get_sun_size(),
                                   sun_intensity,
                                   sun_elevation,
                                   sun_rotation,
                                   altitude,
                                   air_density,
                                   aerosol_density,
                                   ozone_density);
    /* Sky texture image parameters */
    ImageManager *image_manager = compiler.scene->image_manager.get();
    ImageParams impar;
    impar.interpolation = INTERPOLATION_LINEAR;
    impar.extension = EXTENSION_EXTEND;

    /* Precompute sky texture */
    if (handle.empty()) {
      unique_ptr<SkyLoader> loader = make_unique<SkyLoader>(sky_type ==
                                                                NODE_SKY_MULTIPLE_SCATTERING,
                                                            sun_elevation,
                                                            altitude,
                                                            air_density,
                                                            aerosol_density,
                                                            ozone_density);
      handle = image_manager->add_image(std::move(loader), impar);
    }
  }

  const SVMStackOffset vector_offset = tex_mapping.compile_begin(compiler, vector_in);

  compiler.add_node(this,
                    NODE_TEX_SKY,
                    SVMNodeTexSky{
                        .sky_type = sky_type,
                        .dir_offset = vector_offset,
                        .out_offset = compiler.output("Color"),
                    });
  if (sky_type == NODE_SKY_PREETHAM || sky_type == NODE_SKY_HOSEK) {
    SVMNodeTexSkyPreethamData preetham_data = {};
    preetham_data.phi = sunsky.phi;
    preetham_data.theta = sunsky.theta;
    preetham_data.radiance_x = sunsky.radiance_x;
    preetham_data.radiance_y = sunsky.radiance_y;
    preetham_data.radiance_z = sunsky.radiance_z;
    memcpy(preetham_data.config_x, sunsky.config_x, sizeof(preetham_data.config_x));
    memcpy(preetham_data.config_y, sunsky.config_y, sizeof(preetham_data.config_y));
    memcpy(preetham_data.config_z, sunsky.config_z, sizeof(preetham_data.config_z));
    compiler.add_node_data(preetham_data);
  }
  else {
    compiler.add_node_data(SVMNodeTexSkyNishitaData{
        .pixel_bottom_x = sunsky.nishita_data[0],
        .pixel_bottom_y = sunsky.nishita_data[1],
        .pixel_bottom_z = sunsky.nishita_data[2],
        .pixel_top_x = sunsky.nishita_data[3],
        .pixel_top_y = sunsky.nishita_data[4],
        .pixel_top_z = sunsky.nishita_data[5],
        .sun_elevation = sunsky.nishita_data[6],
        .sun_rotation = sunsky.nishita_data[7],
        .angular_diameter = sunsky.nishita_data[8],
        .sun_intensity = sunsky.nishita_data[9],
        .earth_intersection_angle = sunsky.nishita_data[10],
        .texture_id = uint(handle.kernel_id()),
    });
  }

  tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void SkyTextureNode::compile(OSLCompiler &compiler)
{
  tex_mapping.compile(compiler);
  SunSky sunsky = {};

  if (sky_type == NODE_SKY_PREETHAM) {
    sky_texture_precompute_preetham(&sunsky, sun_direction, turbidity);
  }
  else if (sky_type == NODE_SKY_HOSEK) {
    sky_texture_precompute_hosek(&sunsky, sun_direction, turbidity, ground_albedo);
  }
  else {
    sky_texture_precompute_nishita(&sunsky,
                                   sky_type == NODE_SKY_MULTIPLE_SCATTERING,
                                   sun_disc,
                                   get_sun_size(),
                                   sun_intensity,
                                   sun_elevation,
                                   sun_rotation,
                                   altitude,
                                   air_density,
                                   aerosol_density,
                                   ozone_density);
    /* Sky texture image parameters */
    ImageManager *image_manager = compiler.scene->image_manager.get();
    ImageParams impar;
    impar.interpolation = INTERPOLATION_LINEAR;
    impar.extension = EXTENSION_EXTEND;

    /* Precompute sky texture */
    {
      unique_ptr<SkyLoader> loader = make_unique<SkyLoader>(sky_type ==
                                                                NODE_SKY_MULTIPLE_SCATTERING,
                                                            sun_elevation,
                                                            altitude,
                                                            air_density,
                                                            aerosol_density,
                                                            ozone_density);
      handle = image_manager->add_image(std::move(loader), impar);
    }

    compiler.parameter_texture("filename", handle);
  }

  compiler.parameter(this, "sky_type");
  compiler.parameter("theta", sunsky.theta);
  compiler.parameter("phi", sunsky.phi);
  compiler.parameter_color("radiance",
                           make_float3(sunsky.radiance_x, sunsky.radiance_y, sunsky.radiance_z));
  compiler.parameter_array("config_x", sunsky.config_x, 9);
  compiler.parameter_array("config_y", sunsky.config_y, 9);
  compiler.parameter_array("config_z", sunsky.config_z, 9);
  compiler.parameter_array("nishita_data", sunsky.nishita_data, 11);
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
  SOCKET_ENUM(gradient_type, "Type", type_enum, NODE_BLEND_LINEAR);

  SOCKET_IN_POINT(vector, "Vector", zero_float3(), SocketType::LINK_TEXTURE_GENERATED);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

GradientTextureNode::GradientTextureNode() : TextureNode(get_node_type()) {}

void GradientTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");

  const SVMStackOffset vector_offset = tex_mapping.compile_begin(compiler, vector_in);
  compiler.add_node(this,
                    NODE_TEX_GRADIENT,
                    SVMNodeTexGradient{
                        .gradient_type = gradient_type,
                        .co = vector_offset,
                        .fac_offset = compiler.output("Fac"),
                        .color_offset = compiler.output("Color"),
                    });

  tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void GradientTextureNode::compile(OSLCompiler &compiler)
{
  tex_mapping.compile(compiler);

  compiler.parameter(this, "gradient_type");
  compiler.add(this, "node_gradient_texture");
}

/* Noise Texture */

NODE_DEFINE(NoiseTextureNode)
{
  NodeType *type = NodeType::add("noise_texture", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(NoiseTextureNode);

  static NodeEnum dimensions_enum;
  dimensions_enum.insert("1D", 1);
  dimensions_enum.insert("2D", 2);
  dimensions_enum.insert("3D", 3);
  dimensions_enum.insert("4D", 4);
  SOCKET_ENUM(dimensions, "Dimensions", dimensions_enum, 3);

  static NodeEnum type_enum;
  type_enum.insert("multifractal", NODE_NOISE_MULTIFRACTAL);
  type_enum.insert("fBM", NODE_NOISE_FBM);
  type_enum.insert("hybrid_multifractal", NODE_NOISE_HYBRID_MULTIFRACTAL);
  type_enum.insert("ridged_multifractal", NODE_NOISE_RIDGED_MULTIFRACTAL);
  type_enum.insert("hetero_terrain", NODE_NOISE_HETERO_TERRAIN);
  SOCKET_ENUM(type, "Type", type_enum, NODE_NOISE_FBM);

  SOCKET_BOOLEAN(use_normalize, "Normalize", true);

  SOCKET_IN_POINT(vector, "Vector", zero_float3(), SocketType::LINK_TEXTURE_GENERATED);
  SOCKET_IN_FLOAT(w, "W", 0.0f);
  SOCKET_IN_FLOAT(scale, "Scale", 1.0f);
  SOCKET_IN_FLOAT(detail, "Detail", 2.0f);
  SOCKET_IN_FLOAT(roughness, "Roughness", 0.5f);
  SOCKET_IN_FLOAT(lacunarity, "Lacunarity", 2.0f);
  SOCKET_IN_FLOAT(offset, "Offset", 0.0f);
  SOCKET_IN_FLOAT(gain, "Gain", 1.0f);
  SOCKET_IN_FLOAT(distortion, "Distortion", 0.0f);

  SOCKET_OUT_FLOAT(fac, "Fac");
  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

NoiseTextureNode::NoiseTextureNode() : TextureNode(get_node_type()) {}

void NoiseTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");

  const SVMStackOffset vector_stack_offset = tex_mapping.compile_begin(compiler, vector_in);
  compiler.add_node(this,
                    NODE_TEX_NOISE,
                    SVMNodeTexNoise{
                        .dimensions = uint(dimensions),
                        .noise_type = type,
                        .normalize = uint(use_normalize),
                        .w = compiler.input_float("W"),
                        .scale = compiler.input_float("Scale"),
                        .detail = compiler.input_float("Detail"),
                        .roughness = compiler.input_float("Roughness"),
                        .lacunarity = compiler.input_float("Lacunarity"),
                        .offset = compiler.input_float("Offset"),
                        .gain = compiler.input_float("Gain"),
                        .distortion = compiler.input_float("Distortion"),
                        .vector = vector_stack_offset,
                        .value_offset = compiler.output("Fac"),
                        .color_offset = compiler.output("Color"),
                    });

  tex_mapping.compile_end(compiler, vector_in, vector_stack_offset);
}

void NoiseTextureNode::compile(OSLCompiler &compiler)
{
  tex_mapping.compile(compiler);
  compiler.parameter(this, "dimensions");
  compiler.parameter(this, "type");
  compiler.parameter(this, "use_normalize");
  compiler.add(this, "node_noise_texture");
}

/* Gabor Texture */

NODE_DEFINE(GaborTextureNode)
{
  NodeType *type = NodeType::add("gabor_texture", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(GaborTextureNode);

  static NodeEnum type_enum;
  type_enum.insert("2D", NODE_GABOR_TYPE_2D);
  type_enum.insert("3D", NODE_GABOR_TYPE_3D);
  SOCKET_ENUM(type, "Type", type_enum, NODE_GABOR_TYPE_2D);

  SOCKET_IN_POINT(vector, "Vector", zero_float3(), SocketType::LINK_TEXTURE_GENERATED);
  SOCKET_IN_FLOAT(scale, "Scale", 5.0f);
  SOCKET_IN_FLOAT(frequency, "Frequency", 2.0f);
  SOCKET_IN_FLOAT(anisotropy, "Anisotropy", 1.0f);
  SOCKET_IN_FLOAT(orientation_2d, "Orientation 2D", M_PI_F / 4.0f);
  SOCKET_IN_VECTOR(orientation_3d, "Orientation 3D", make_float3(M_SQRT2_F, M_SQRT2_F, 0.0f));

  SOCKET_OUT_FLOAT(value, "Value");
  SOCKET_OUT_FLOAT(phase, "Phase");
  SOCKET_OUT_FLOAT(intensity, "Intensity");

  return type;
}

GaborTextureNode::GaborTextureNode() : TextureNode(get_node_type()) {}

void GaborTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");

  const SVMStackOffset vector_stack_offset = tex_mapping.compile_begin(compiler, vector_in);
  compiler.add_node(this,
                    NODE_TEX_GABOR,
                    SVMNodeTexGabor{
                        .gabor_type = type,
                        .orientation_3d = compiler.input_float3("Orientation 3D"),
                        .scale = compiler.input_float("Scale"),
                        .frequency = compiler.input_float("Frequency"),
                        .anisotropy = compiler.input_float("Anisotropy"),
                        .orientation_2d = compiler.input_float("Orientation 2D"),
                        .coordinates = vector_stack_offset,
                        .value_offset = compiler.output("Value"),
                        .phase_offset = compiler.output("Phase"),
                        .intensity_offset = compiler.output("Intensity"),
                    });

  tex_mapping.compile_end(compiler, vector_in, vector_stack_offset);
}

void GaborTextureNode::compile(OSLCompiler &compiler)
{
  tex_mapping.compile(compiler);
  compiler.parameter(this, "type");
  compiler.add(this, "node_gabor_texture");
}

/* Voronoi Texture */

NODE_DEFINE(VoronoiTextureNode)
{
  NodeType *type = NodeType::add("voronoi_texture", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(VoronoiTextureNode);

  static NodeEnum dimensions_enum;
  dimensions_enum.insert("1D", 1);
  dimensions_enum.insert("2D", 2);
  dimensions_enum.insert("3D", 3);
  dimensions_enum.insert("4D", 4);
  SOCKET_ENUM(dimensions, "Dimensions", dimensions_enum, 3);

  static NodeEnum metric_enum;
  metric_enum.insert("euclidean", NODE_VORONOI_EUCLIDEAN);
  metric_enum.insert("manhattan", NODE_VORONOI_MANHATTAN);
  metric_enum.insert("chebychev", NODE_VORONOI_CHEBYCHEV);
  metric_enum.insert("minkowski", NODE_VORONOI_MINKOWSKI);
  SOCKET_ENUM(metric, "Distance Metric", metric_enum, NODE_VORONOI_EUCLIDEAN);

  static NodeEnum feature_enum;
  feature_enum.insert("f1", NODE_VORONOI_F1);
  feature_enum.insert("f2", NODE_VORONOI_F2);
  feature_enum.insert("smooth_f1", NODE_VORONOI_SMOOTH_F1);
  feature_enum.insert("distance_to_edge", NODE_VORONOI_DISTANCE_TO_EDGE);
  feature_enum.insert("n_sphere_radius", NODE_VORONOI_N_SPHERE_RADIUS);
  SOCKET_ENUM(feature, "Feature", feature_enum, NODE_VORONOI_F1);

  SOCKET_BOOLEAN(use_normalize, "Normalize", false);

  SOCKET_IN_POINT(vector, "Vector", zero_float3(), SocketType::LINK_TEXTURE_GENERATED);
  SOCKET_IN_FLOAT(w, "W", 0.0f);
  SOCKET_IN_FLOAT(scale, "Scale", 5.0f);
  SOCKET_IN_FLOAT(detail, "Detail", 0.0f);
  SOCKET_IN_FLOAT(roughness, "Roughness", 0.5f);
  SOCKET_IN_FLOAT(lacunarity, "Lacunarity", 2.0f);
  SOCKET_IN_FLOAT(smoothness, "Smoothness", 5.0f);
  SOCKET_IN_FLOAT(exponent, "Exponent", 0.5f);
  SOCKET_IN_FLOAT(randomness, "Randomness", 1.0f);

  SOCKET_OUT_FLOAT(distance, "Distance");
  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_POINT(position, "Position");
  SOCKET_OUT_FLOAT(w, "W");
  SOCKET_OUT_FLOAT(radius, "Radius");

  return type;
}

VoronoiTextureNode::VoronoiTextureNode() : TextureNode(get_node_type()) {}

void VoronoiTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");

  const SVMStackOffset vector_stack_offset = tex_mapping.compile_begin(compiler, vector_in);
  compiler.add_node(this,
                    NODE_TEX_VORONOI,
                    SVMNodeTexVoronoi{
                        .dimensions = uint(dimensions),
                        .feature = feature,
                        .metric = metric,
                        .w = compiler.input_float("W"),
                        .scale = compiler.input_float("Scale"),
                        .detail = compiler.input_float("Detail"),
                        .roughness = compiler.input_float("Roughness"),
                        .lacunarity = compiler.input_float("Lacunarity"),
                        .smoothness = compiler.input_float("Smoothness"),
                        .exponent = compiler.input_float("Exponent"),
                        .randomness = compiler.input_float("Randomness"),
                        .normalize = use_normalize,
                        .coord = vector_stack_offset,
                        .distance_offset = compiler.output("Distance"),
                        .color_offset = compiler.output("Color"),
                        .position_offset = compiler.output("Position"),
                        .w_out_offset = compiler.output("W"),
                        .radius_offset = compiler.output("Radius"),
                    });
  tex_mapping.compile_end(compiler, vector_in, vector_stack_offset);
}

void VoronoiTextureNode::compile(OSLCompiler &compiler)
{
  tex_mapping.compile(compiler);

  compiler.parameter(this, "dimensions");
  compiler.parameter(this, "feature");
  compiler.parameter(this, "metric");
  compiler.parameter(this, "use_normalize");
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
  SOCKET_IN_POINT(vector, "Vector", zero_float3(), SocketType::LINK_TEXTURE_INCOMING);

  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

IESLightNode::IESLightNode() : TextureNode(get_node_type())
{
  light_manager = nullptr;
  slot = -1;
}

ShaderNode *IESLightNode::clone(ShaderGraph *graph) const
{
  IESLightNode *node = graph->create_node<IESLightNode>(*this);

  node->light_manager = nullptr;
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
      slot = light_manager->add_ies_from_file(filename.string());
    }
    else {
      slot = light_manager->add_ies(ies.string(), true);
    }
  }
}

void IESLightNode::compile(SVMCompiler &compiler)
{
  light_manager = compiler.scene->light_manager.get();
  get_slot();
  ShaderInput *vector_in = input("Vector");

  const SVMStackOffset vector_offset = tex_mapping.compile_begin(compiler, vector_in);
  compiler.add_node(this,
                    NODE_IES,
                    SVMNodeIES{
                        .strength = compiler.input_float("Strength"),
                        .slot = uint(slot),
                        .vector_offset = vector_offset,
                        .fac_offset = compiler.output("Fac"),
                    });

  tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void IESLightNode::compile(OSLCompiler &compiler)
{
  light_manager = compiler.scene->light_manager.get();
  get_slot();

  tex_mapping.compile(compiler);

  compiler.parameter_texture_ies("filename", slot);
  compiler.add(this, "node_ies_light");
}

/* White Noise Texture */

NODE_DEFINE(WhiteNoiseTextureNode)
{
  NodeType *type = NodeType::add("white_noise_texture", create, NodeType::SHADER);

  static NodeEnum dimensions_enum;
  dimensions_enum.insert("1D", 1);
  dimensions_enum.insert("2D", 2);
  dimensions_enum.insert("3D", 3);
  dimensions_enum.insert("4D", 4);
  SOCKET_ENUM(dimensions, "Dimensions", dimensions_enum, 3);

  SOCKET_IN_POINT(vector, "Vector", zero_float3());
  SOCKET_IN_FLOAT(w, "W", 0.0f);

  SOCKET_OUT_FLOAT(value, "Value");
  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

WhiteNoiseTextureNode::WhiteNoiseTextureNode() : ShaderNode(get_node_type()) {}

void WhiteNoiseTextureNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_TEX_WHITE_NOISE,
                    SVMNodeTexWhiteNoise{
                        .dimensions = uint(dimensions),
                        .vector = compiler.input_float3("Vector"),
                        .w = compiler.input_float("W"),
                        .value_offset = compiler.output("Value"),
                        .color_offset = compiler.output("Color"),
                    });
}

void WhiteNoiseTextureNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "dimensions");
  compiler.add(this, "node_white_noise_texture");
}

/* Wave Texture */

NODE_DEFINE(WaveTextureNode)
{
  NodeType *type = NodeType::add("wave_texture", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(WaveTextureNode);

  static NodeEnum type_enum;
  type_enum.insert("bands", NODE_WAVE_BANDS);
  type_enum.insert("rings", NODE_WAVE_RINGS);
  SOCKET_ENUM(wave_type, "Type", type_enum, NODE_WAVE_BANDS);

  static NodeEnum bands_direction_enum;
  bands_direction_enum.insert("x", NODE_WAVE_BANDS_DIRECTION_X);
  bands_direction_enum.insert("y", NODE_WAVE_BANDS_DIRECTION_Y);
  bands_direction_enum.insert("z", NODE_WAVE_BANDS_DIRECTION_Z);
  bands_direction_enum.insert("diagonal", NODE_WAVE_BANDS_DIRECTION_DIAGONAL);
  SOCKET_ENUM(
      bands_direction, "Bands Direction", bands_direction_enum, NODE_WAVE_BANDS_DIRECTION_X);

  static NodeEnum rings_direction_enum;
  rings_direction_enum.insert("x", NODE_WAVE_RINGS_DIRECTION_X);
  rings_direction_enum.insert("y", NODE_WAVE_RINGS_DIRECTION_Y);
  rings_direction_enum.insert("z", NODE_WAVE_RINGS_DIRECTION_Z);
  rings_direction_enum.insert("spherical", NODE_WAVE_RINGS_DIRECTION_SPHERICAL);
  SOCKET_ENUM(
      rings_direction, "Rings Direction", rings_direction_enum, NODE_WAVE_BANDS_DIRECTION_X);

  static NodeEnum profile_enum;
  profile_enum.insert("sine", NODE_WAVE_PROFILE_SIN);
  profile_enum.insert("saw", NODE_WAVE_PROFILE_SAW);
  profile_enum.insert("tri", NODE_WAVE_PROFILE_TRI);
  SOCKET_ENUM(profile, "Profile", profile_enum, NODE_WAVE_PROFILE_SIN);

  SOCKET_IN_POINT(vector, "Vector", zero_float3(), SocketType::LINK_TEXTURE_GENERATED);
  SOCKET_IN_FLOAT(scale, "Scale", 1.0f);
  SOCKET_IN_FLOAT(distortion, "Distortion", 0.0f);
  SOCKET_IN_FLOAT(detail, "Detail", 2.0f);
  SOCKET_IN_FLOAT(detail_scale, "Detail Scale", 0.0f);
  SOCKET_IN_FLOAT(detail_roughness, "Detail Roughness", 0.5f);
  SOCKET_IN_FLOAT(phase, "Phase Offset", 0.0f);
  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

WaveTextureNode::WaveTextureNode() : TextureNode(get_node_type()) {}

void WaveTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");

  const SVMStackOffset vector_offset = tex_mapping.compile_begin(compiler, vector_in);
  compiler.add_node(this,
                    NODE_TEX_WAVE,
                    SVMNodeTexWave{
                        .wave_type = wave_type,
                        .bands_direction = bands_direction,
                        .rings_direction = rings_direction,
                        .profile = profile,
                        .scale = compiler.input_float("Scale"),
                        .distortion = compiler.input_float("Distortion"),
                        .detail = compiler.input_float("Detail"),
                        .dscale = compiler.input_float("Detail Scale"),
                        .droughness = compiler.input_float("Detail Roughness"),
                        .phase = compiler.input_float("Phase Offset"),
                        .co = vector_offset,
                        .color_offset = compiler.output("Color"),
                        .fac_offset = compiler.output("Fac"),
                    });

  tex_mapping.compile_end(compiler, vector_in, vector_offset);
}

void WaveTextureNode::compile(OSLCompiler &compiler)
{
  tex_mapping.compile(compiler);

  compiler.parameter(this, "wave_type");
  compiler.parameter(this, "bands_direction");
  compiler.parameter(this, "rings_direction");
  compiler.parameter(this, "profile");

  compiler.add(this, "node_wave_texture");
}

/* Magic Texture */

NODE_DEFINE(MagicTextureNode)
{
  NodeType *type = NodeType::add("magic_texture", create, NodeType::SHADER);

  TEXTURE_MAPPING_DEFINE(MagicTextureNode);

  SOCKET_INT(depth, "Depth", 2);

  SOCKET_IN_POINT(vector, "Vector", zero_float3(), SocketType::LINK_TEXTURE_GENERATED);
  SOCKET_IN_FLOAT(scale, "Scale", 5.0f);
  SOCKET_IN_FLOAT(distortion, "Distortion", 1.0f);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

MagicTextureNode::MagicTextureNode() : TextureNode(get_node_type()) {}

void MagicTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");

  const SVMStackOffset vector_offset = tex_mapping.compile_begin(compiler, vector_in);
  compiler.add_node(this,
                    NODE_TEX_MAGIC,
                    SVMNodeTexMagic{
                        .scale = compiler.input_float("Scale"),
                        .distortion = compiler.input_float("Distortion"),
                        .depth = uint8_t(depth),
                        .co = vector_offset,
                        .color_offset = compiler.output("Color"),
                        .fac_offset = compiler.output("Fac"),
                    });

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

  SOCKET_IN_POINT(vector, "Vector", zero_float3(), SocketType::LINK_TEXTURE_GENERATED);
  SOCKET_IN_COLOR(color1, "Color1", zero_float3());
  SOCKET_IN_COLOR(color2, "Color2", zero_float3());
  SOCKET_IN_FLOAT(scale, "Scale", 1.0f);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

CheckerTextureNode::CheckerTextureNode() : TextureNode(get_node_type()) {}

void CheckerTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");

  const SVMStackOffset vector_offset = tex_mapping.compile_begin(compiler, vector_in);
  compiler.add_node(this,
                    NODE_TEX_CHECKER,
                    SVMNodeTexChecker{
                        .color1 = compiler.input_float3("Color1"),
                        .color2 = compiler.input_float3("Color2"),
                        .scale = compiler.input_float("Scale"),
                        .co = vector_offset,
                        .color_offset = compiler.output("Color"),
                        .fac_offset = compiler.output("Fac"),
                    });

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

  SOCKET_IN_POINT(vector, "Vector", zero_float3(), SocketType::LINK_TEXTURE_GENERATED);

  SOCKET_IN_COLOR(color1, "Color1", zero_float3());
  SOCKET_IN_COLOR(color2, "Color2", zero_float3());
  SOCKET_IN_COLOR(mortar, "Mortar", zero_float3());
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

BrickTextureNode::BrickTextureNode() : TextureNode(get_node_type()) {}

void BrickTextureNode::compile(SVMCompiler &compiler)
{
  ShaderInput *vector_in = input("Vector");

  const SVMStackOffset vector_offset = tex_mapping.compile_begin(compiler, vector_in);
  compiler.add_node(this,
                    NODE_TEX_BRICK,
                    SVMNodeTexBrick{
                        .color1 = compiler.input_float3("Color1"),
                        .color2 = compiler.input_float3("Color2"),
                        .mortar = compiler.input_float3("Mortar"),
                        .scale = compiler.input_float("Scale"),
                        .mortar_size = compiler.input_float("Mortar Size"),
                        .bias = compiler.input_float("Bias"),
                        .brick_width = compiler.input_float("Brick Width"),
                        .row_height = compiler.input_float("Row Height"),
                        .mortar_smooth = compiler.input_float("Mortar Smooth"),
                        .offset_amount = offset,
                        .squash_amount = squash,
                        .offset_frequency = uint8_t(offset_frequency),
                        .squash_frequency = uint8_t(squash_frequency),
                        .co = vector_offset,
                        .color_offset = compiler.output("Color"),
                        .fac_offset = compiler.output("Fac"),
                    });

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

/* Normal */

NODE_DEFINE(NormalNode)
{
  NodeType *type = NodeType::add("normal", create, NodeType::SHADER);

  SOCKET_VECTOR(direction, "direction", zero_float3());

  SOCKET_IN_NORMAL(normal, "Normal", zero_float3());

  SOCKET_OUT_NORMAL(normal, "Normal");
  SOCKET_OUT_FLOAT(dot, "Dot");

  return type;
}

NormalNode::NormalNode() : ShaderNode(get_node_type()) {}

void NormalNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_NORMAL,
                    SVMNodeNormal{
                        .in_normal = compiler.input_float3("Normal"),
                        .out_normal_offset = compiler.output("Normal"),
                        .out_dot_offset = compiler.output("Dot"),
                        .direction_x = direction.x,
                        .direction_y = direction.y,
                        .direction_z = direction.z,
                    });
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

  static NodeEnum type_enum;
  type_enum.insert("point", NODE_MAPPING_TYPE_POINT);
  type_enum.insert("texture", NODE_MAPPING_TYPE_TEXTURE);
  type_enum.insert("vector", NODE_MAPPING_TYPE_VECTOR);
  type_enum.insert("normal", NODE_MAPPING_TYPE_NORMAL);
  SOCKET_ENUM(mapping_type, "Type", type_enum, NODE_MAPPING_TYPE_POINT);

  SOCKET_IN_POINT(vector, "Vector", zero_float3());
  SOCKET_IN_POINT(location, "Location", zero_float3());
  SOCKET_IN_POINT(rotation, "Rotation", zero_float3());
  SOCKET_IN_POINT(scale, "Scale", one_float3());

  SOCKET_OUT_POINT(vector, "Vector");

  return type;
}

MappingNode::MappingNode() : ShaderNode(get_node_type()) {}

void MappingNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    const float3 result = svm_mapping(mapping_type, vector, location, rotation, scale);
    folder.make_constant(result);
  }
  else {
    folder.fold_mapping(mapping_type);
  }
}

void MappingNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_MAPPING,
                    SVMNodeMapping{
                        .mapping_type = mapping_type,
                        .vector = compiler.input_float3("Vector"),
                        .location = compiler.input_float3("Location"),
                        .rotation = compiler.input_float3("Rotation"),
                        .scale = compiler.input_float3("Scale"),
                        .result_offset = compiler.output("Vector"),
                    });
}

void MappingNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "mapping_type");
  compiler.add(this, "node_mapping");
}

/* RGBToBW */

NODE_DEFINE(RGBToBWNode)
{
  NodeType *type = NodeType::add("rgb_to_bw", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", zero_float3());
  SOCKET_OUT_FLOAT(val, "Val");

  return type;
}

RGBToBWNode::RGBToBWNode() : ShaderNode(get_node_type()) {}

void RGBToBWNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    const float val = folder.scene->shader_manager->linear_rgb_to_gray(color);
    folder.make_constant(val);
  }
}

void RGBToBWNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_CONVERT,
                    SVMNodeConvert{
                        .convert_type = NODE_CONVERT_CF,
                        .from_offset = compiler.input_link("Color"),
                        .to_offset = compiler.output("Val"),
                    });
}

void RGBToBWNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_rgb_to_bw");
}

/* Convert */

const NodeType *(&ConvertNode::get_node_types())[ConvertNode::MAX_TYPE][ConvertNode::MAX_TYPE]
{
  static const NodeType *node_types[MAX_TYPE][MAX_TYPE];
  static std::once_flag node_types_flag;

  std::call_once(node_types_flag, [&] {
    const int num_types = 8;
    const SocketType::Type types[num_types] = {SocketType::FLOAT,
                                               SocketType::INT,
                                               SocketType::COLOR,
                                               SocketType::VECTOR,
                                               SocketType::POINT,
                                               SocketType::NORMAL,
                                               SocketType::STRING,
                                               SocketType::CLOSURE};

    for (size_t i = 0; i < num_types; i++) {
      const SocketType::Type from = types[i];
      const ustring from_name(SocketType::type_name(from));
      const ustring from_value_name("value_" + from_name.string());

      for (size_t j = 0; j < num_types; j++) {
        const SocketType::Type to = types[j];
        const ustring to_name(SocketType::type_name(to));
        const ustring to_value_name("value_" + to_name.string());

        const string node_name = "convert_" + from_name.string() + "_to_" + to_name.string();
        NodeType *type = NodeType::add(node_name.c_str(), create, NodeType::SHADER);

        type->register_input(from_value_name,
                             from_value_name,
                             from,
                             SOCKET_OFFSETOF(ConvertNode, value_float),
                             SocketType::zero_default_value(),
                             nullptr,
                             nullptr,
                             SocketType::LINKABLE);
        type->register_output(to_value_name, to_value_name, to);

        assert(from < MAX_TYPE);
        assert(to < MAX_TYPE);

        node_types[from][to] = type;
      }
    }
  });

  return node_types;
}

bool ConvertNode::register_on_init = NodeType::register_on_init([] {
  ConvertNode::get_node_types();
  return static_cast<const NodeType *>(nullptr);
});

unique_ptr<Node> ConvertNode::create(const NodeType *type)
{
  return make_unique<ConvertNode>(type->inputs[0].type, type->outputs[0].type);
}

ConvertNode::ConvertNode(SocketType::Type from_, SocketType::Type to_, bool autoconvert)
    : ShaderNode(get_node_types()[from_][to_])
{
  from = from_;
  to = to_;

  if (from == to) {
    special_type = SHADER_SPECIAL_TYPE_PROXY;
  }
  else if (autoconvert) {
    special_type = SHADER_SPECIAL_TYPE_AUTOCONVERT;
  }
}

/* Union usage requires a manual copy constructor. */
ConvertNode::ConvertNode(const ConvertNode &other)
    : ShaderNode(other),
      from(other.from),
      to(other.to),
      value_color(other.value_color),
      value_string(other.value_string)
{
}

void ConvertNode::constant_fold(const ConstantFolder &folder)
{
  /* proxy nodes should have been removed at this point */
  assert(special_type != SHADER_SPECIAL_TYPE_PROXY);

  if (folder.all_inputs_constant()) {
    if (from == SocketType::FLOAT || from == SocketType::INT) {
      float val = value_float;
      if (from == SocketType::INT) {
        val = value_int;
      }
      if (SocketType::is_float3(to)) {
        folder.make_constant(make_float3(val, val, val));
      }
      else if (to == SocketType::INT) {
        folder.make_constant((int)val);
      }
      else if (to == SocketType::FLOAT) {
        folder.make_constant(val);
      }
    }
    else if (SocketType::is_float3(from)) {
      if (to == SocketType::FLOAT || to == SocketType::INT) {
        float val;
        if (from == SocketType::COLOR) {
          /* color to scalar */
          val = folder.scene->shader_manager->linear_rgb_to_gray(value_color);
        }
        else {
          /* vector/point/normal to scalar */
          val = average(value_vector);
        }
        if (to == SocketType::INT) {
          folder.make_constant((int)val);
        }
        else if (to == SocketType::FLOAT) {
          folder.make_constant(val);
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
    if (prev->type == get_node_types()[to][from]) {
      ShaderInput *prev_in = prev->inputs[0];

      if (SocketType::is_float3(from) && (to == SocketType::FLOAT || SocketType::is_float3(to)) &&
          prev_in->link)
      {
        folder.bypass(prev_in->link);
      }
    }
  }
}

NodeConvert ConvertNode::convert_type()
{
  if (from == SocketType::FLOAT) {
    if (to == SocketType::INT) {
      /* float to int */
      return NODE_CONVERT_FI;
    }
    /* float to float3 */
    return NODE_CONVERT_FV;
  }
  if (from == SocketType::INT) {
    if (to == SocketType::FLOAT) {
      /* int to float */
      return NODE_CONVERT_IF;
    }
    /* int to vector/point/normal */
    return NODE_CONVERT_IV;
  }
  if (to == SocketType::FLOAT) {
    if (from == SocketType::COLOR) {
      /* color to float */
      return NODE_CONVERT_CF;
    }
    /* vector/point/normal to float */
    return NODE_CONVERT_VF;
  }
  if (to == SocketType::INT) {
    if (from == SocketType::COLOR) {
      /* color to int */
      return NODE_CONVERT_CI;
    }
    /* vector/point/normal to int */
    return NODE_CONVERT_VI;
  }
  return NODE_CONVERT_NONE;
}

void ConvertNode::compile(SVMCompiler &compiler)
{
  /* proxy nodes should have been removed at this point */
  assert(special_type != SHADER_SPECIAL_TYPE_PROXY);

  ShaderInput *in = inputs[0];
  ShaderOutput *out = outputs[0];

  const NodeConvert cvt_type = convert_type();
  if (cvt_type != NODE_CONVERT_NONE) {
    compiler.add_node(this,
                      NODE_CONVERT,
                      SVMNodeConvert{
                          .convert_type = cvt_type,
                          .from_offset = compiler.input_link(in->name().c_str()),
                          .to_offset = compiler.output(out->name().c_str()),
                      });
  }
  else {
    /* float3 to float3 */
    if (in->link) {
      /* no op in SVM */
      compiler.stack_link(in, out);
    }
    else {
      /* set 0,0,0 value */
      compiler.add_value_node(this, value_color, compiler.output(out->name().c_str()));
    }
  }
}

void ConvertNode::compile(OSLCompiler &compiler)
{
  /* proxy nodes should have been removed at this point */
  assert(special_type != SHADER_SPECIAL_TYPE_PROXY);

  if (from == SocketType::FLOAT) {
    compiler.add(this, "node_convert_from_float");
  }
  else if (from == SocketType::INT) {
    compiler.add(this, "node_convert_from_int");
  }
  else if (from == SocketType::COLOR) {
    compiler.add(this, "node_convert_from_color");
  }
  else if (from == SocketType::VECTOR) {
    compiler.add(this, "node_convert_from_vector");
  }
  else if (from == SocketType::POINT) {
    compiler.add(this, "node_convert_from_point");
  }
  else if (from == SocketType::NORMAL) {
    compiler.add(this, "node_convert_from_normal");
  }
  else {
    assert(0);
  }
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

BsdfNode::BsdfNode(const NodeType *node_type) : BsdfBaseNode(node_type) {}

void BsdfNode::compile(SVMCompiler &compiler)
{
  ShaderInput *color_in = input("Color");

  if (color_in->link) {
    compiler.add_node(this,
                      NODE_CLOSURE_WEIGHT,
                      SVMNodeClosureWeight{
                          .weight_offset = compiler.input_link("Color"),
                      });
  }
  else {
    compiler.add_node(this,
                      NODE_CLOSURE_SET_WEIGHT,
                      SVMNodeClosureSetWeight{
                          .rgb = color,
                      });
  }

  compiler.add_node(this,
                    NODE_CLOSURE_BSDF,
                    SVMNodeClosureBsdf{
                        .closure_type = closure,
                        .mix_weight_offset = compiler.closure_mix_weight_offset(),
                    });
}

void BsdfNode::compile(OSLCompiler & /*compiler*/)
{
  assert(0);
}

/* Metallic BSDF Closure */

NODE_DEFINE(MetallicBsdfNode)
{
  NodeType *type = NodeType::add("metallic_bsdf", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Base Color", make_float3(0.617f, 0.577f, 0.540f));
  SOCKET_IN_NORMAL(normal, "Normal", zero_float3(), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  static NodeEnum distribution_enum;
  distribution_enum.insert("beckmann", CLOSURE_BSDF_MICROFACET_BECKMANN_ID);
  distribution_enum.insert("ggx", CLOSURE_BSDF_MICROFACET_GGX_ID);
  distribution_enum.insert("multi_ggx", CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID);
  SOCKET_ENUM(
      distribution, "Distribution", distribution_enum, CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID);

  static NodeEnum fresnel_type_enum;
  fresnel_type_enum.insert("f82", CLOSURE_BSDF_F82_CONDUCTOR);
  fresnel_type_enum.insert("physical_conductor", CLOSURE_BSDF_PHYSICAL_CONDUCTOR);
  SOCKET_ENUM(fresnel_type, "fresnel_type", fresnel_type_enum, CLOSURE_BSDF_F82_CONDUCTOR);

  SOCKET_IN_COLOR(edge_tint, "Edge Tint", make_float3(0.695f, 0.726f, 0.770f));

  SOCKET_IN_VECTOR(ior, "IOR", make_float3(2.757f, 2.513f, 2.231f));
  SOCKET_IN_VECTOR(k, "Extinction", make_float3(3.867f, 3.404f, 3.009f));

  SOCKET_IN_VECTOR(tangent, "Tangent", zero_float3(), SocketType::LINK_TANGENT);

  SOCKET_IN_FLOAT(roughness, "Roughness", 0.5f);
  SOCKET_IN_FLOAT(anisotropy, "Anisotropy", 0.0f);
  SOCKET_IN_FLOAT(rotation, "Rotation", 0.0f);

  SOCKET_IN_FLOAT(thin_film_thickness, "Thin Film Thickness", 0.0f);
  SOCKET_IN_FLOAT(thin_film_ior, "Thin Film IOR", 1.33f);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

MetallicBsdfNode::MetallicBsdfNode() : BsdfNode(get_node_type())
{
  closure = CLOSURE_BSDF_PHYSICAL_CONDUCTOR;
}

bool MetallicBsdfNode::is_isotropic()
{
  /* Keep in sync with the thresholds in OSL's node_conductor_bsdf and SVM's
   * svm_node_metallic_bsdf. */
  return (!input("Anisotropy")->link && fabsf(anisotropy) <= 1e-4f);
}

void MetallicBsdfNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface_link()) {
    if (!input("Tangent")->link && !is_isotropic()) {
      attributes->add(ATTR_STD_GENERATED);
    }
  }

  ShaderNode::attributes(shader, attributes);
}

void MetallicBsdfNode::simplify_settings(Scene * /* scene */)
{
  /* If the anisotropy is close enough to zero, fall back to the isotropic case. */
  if (is_isotropic()) {
    disconnect_unused_input("Tangent");
  }
}

void MetallicBsdfNode::compile(SVMCompiler &compiler)
{
  const SVMStackOffset normal_offset = compiler.input_link("Normal");
  const SVMStackOffset tangent_offset = compiler.input_link("Tangent");

  compiler.add_node(this,
                    NODE_CLOSURE_BSDF,
                    SVMNodeClosureBsdf{
                        .closure_type = fresnel_type,
                        .mix_weight_offset = compiler.closure_mix_weight_offset(),
                    });
  compiler.add_node_data(SVMNodeMetallicBsdfData{
      .distribution = distribution,
      .base_ior = fresnel_type == CLOSURE_BSDF_PHYSICAL_CONDUCTOR ?
                      compiler.input_float3("IOR") :
                      compiler.input_float3("Base Color"),
      .edge_tint_k = fresnel_type == CLOSURE_BSDF_PHYSICAL_CONDUCTOR ?
                         compiler.input_float3("Extinction") :
                         compiler.input_float3("Edge Tint"),
      .roughness = compiler.input_float("Roughness"),
      .anisotropy = compiler.input_float("Anisotropy"),
      .rotation = compiler.input_float("Rotation"),
      .thin_film_thickness = compiler.input_float("Thin Film Thickness"),
      .thin_film_ior = compiler.input_float("Thin Film IOR"),
      .normal_offset = normal_offset,
      .tangent_offset = tangent_offset,
  });
}

void MetallicBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "distribution");
  compiler.parameter(this, "fresnel_type");
  compiler.add(this, "node_metallic_bsdf");
}

/* Glossy BSDF Closure */

NODE_DEFINE(GlossyBsdfNode)
{
  NodeType *type = NodeType::add("glossy_bsdf", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_NORMAL(normal, "Normal", zero_float3(), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  static NodeEnum distribution_enum;
  distribution_enum.insert("beckmann", CLOSURE_BSDF_MICROFACET_BECKMANN_ID);
  distribution_enum.insert("ggx", CLOSURE_BSDF_MICROFACET_GGX_ID);
  distribution_enum.insert("ashikhmin_shirley", CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID);
  distribution_enum.insert("multi_ggx", CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID);
  SOCKET_ENUM(distribution, "Distribution", distribution_enum, CLOSURE_BSDF_MICROFACET_GGX_ID);

  SOCKET_IN_VECTOR(tangent, "Tangent", zero_float3(), SocketType::LINK_TANGENT);

  SOCKET_IN_FLOAT(roughness, "Roughness", 0.5f);
  SOCKET_IN_FLOAT(anisotropy, "Anisotropy", 0.0f);
  SOCKET_IN_FLOAT(rotation, "Rotation", 0.0f);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

GlossyBsdfNode::GlossyBsdfNode() : BsdfNode(get_node_type())
{
  closure = CLOSURE_BSDF_MICROFACET_GGX_ID;
}

bool GlossyBsdfNode::is_isotropic()
{
  /* Keep in sync with the thresholds in OSL's node_glossy_bsdf and SVM's svm_node_closure_bsdf.
   */
  return (!input("Anisotropy")->link && fabsf(anisotropy) <= 1e-4f);
}

void GlossyBsdfNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface_link()) {
    if (!input("Tangent")->link && !is_isotropic()) {
      attributes->add(ATTR_STD_GENERATED);
    }
  }

  ShaderNode::attributes(shader, attributes);
}

void GlossyBsdfNode::simplify_settings(Scene * /* scene */)
{
  /* If the anisotropy is close enough to zero, fall back to the isotropic case. */
  if (is_isotropic()) {
    disconnect_unused_input("Tangent");
  }
}

void GlossyBsdfNode::compile(SVMCompiler &compiler)
{
  closure = distribution;

  /* TODO: Just use weight for legacy MultiGGX? Would also simplify OSL. */
  BsdfNode::compile(compiler);
  compiler.add_node_data(SVMNodeGlossyBsdfData{
      .color = (closure == CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID) ? compiler.input_float3("Color") :
                                                                   SVMInputFloat3{},
      .roughness = compiler.input_float("Roughness"),
      .anisotropy = compiler.input_float("Anisotropy"),
      .rotation = compiler.input_float("Rotation"),
      .normal_offset = compiler.input_link("Normal"),
      .tangent_offset = compiler.input_link("Tangent"),
  });
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
  SOCKET_IN_NORMAL(normal, "Normal", zero_float3(), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  static NodeEnum distribution_enum;
  distribution_enum.insert("beckmann", CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID);
  distribution_enum.insert("ggx", CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID);
  distribution_enum.insert("multi_ggx", CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID);
  SOCKET_ENUM(
      distribution, "Distribution", distribution_enum, CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID);
  SOCKET_IN_FLOAT(roughness, "Roughness", 0.0f);
  SOCKET_IN_FLOAT(IOR, "IOR", 1.5f);

  SOCKET_IN_FLOAT(thin_film_thickness, "Thin Film Thickness", 0.0f);
  SOCKET_IN_FLOAT(thin_film_ior, "Thin Film IOR", 1.3f);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

GlassBsdfNode::GlassBsdfNode() : BsdfNode(get_node_type())
{
  closure = CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID;
}

void GlassBsdfNode::compile(SVMCompiler &compiler)
{
  closure = distribution;
  BsdfNode::compile(compiler);
  compiler.add_node_data(SVMNodeGlassBsdfData{
      .color = compiler.input_float3("Color"),
      .roughness = compiler.input_float("Roughness"),
      .ior = compiler.input_float("IOR"),
      .thin_film_thickness = compiler.input_float("Thin Film Thickness"),
      .thin_film_ior = compiler.input_float("Thin Film IOR"),
      .normal_offset = compiler.input_link("Normal"),
  });
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
  SOCKET_IN_NORMAL(normal, "Normal", zero_float3(), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  static NodeEnum distribution_enum;
  distribution_enum.insert("beckmann", CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID);
  distribution_enum.insert("ggx", CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);
  SOCKET_ENUM(
      distribution, "Distribution", distribution_enum, CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);

  SOCKET_IN_FLOAT(roughness, "Roughness", 0.0f);
  SOCKET_IN_FLOAT(IOR, "IOR", 0.3f);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

RefractionBsdfNode::RefractionBsdfNode() : BsdfNode(get_node_type())
{
  closure = CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
}

void RefractionBsdfNode::compile(SVMCompiler &compiler)
{
  closure = distribution;
  BsdfNode::compile(compiler);
  compiler.add_node_data(SVMNodeRefractionBsdfData{
      .roughness = compiler.input_float("Roughness"),
      .ior = compiler.input_float("IOR"),
      .normal_offset = compiler.input_link("Normal"),
  });
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
  SOCKET_IN_NORMAL(normal, "Normal", zero_float3(), SocketType::LINK_NORMAL);
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

ToonBsdfNode::ToonBsdfNode() : BsdfNode(get_node_type())
{
  closure = CLOSURE_BSDF_DIFFUSE_TOON_ID;
}

void ToonBsdfNode::compile(SVMCompiler &compiler)
{
  closure = component;
  BsdfNode::compile(compiler);
  compiler.add_node_data(SVMNodeToonBsdfData{
      .size = compiler.input_float("Size"),
      .smooth = compiler.input_float("Smooth"),
      .normal_offset = compiler.input_link("Normal"),
  });
}

void ToonBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "component");
  compiler.add(this, "node_toon_bsdf");
}

/* Sheen BSDF Closure */

NODE_DEFINE(SheenBsdfNode)
{
  NodeType *type = NodeType::add("sheen_bsdf", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_NORMAL(normal, "Normal", zero_float3(), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);
  SOCKET_IN_FLOAT(roughness, "Roughness", 1.0f);

  static NodeEnum distribution_enum;
  distribution_enum.insert("ashikhmin", CLOSURE_BSDF_ASHIKHMIN_VELVET_ID);
  distribution_enum.insert("microfiber", CLOSURE_BSDF_SHEEN_ID);
  SOCKET_ENUM(distribution, "Distribution", distribution_enum, CLOSURE_BSDF_SHEEN_ID);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

SheenBsdfNode::SheenBsdfNode() : BsdfNode(get_node_type())
{
  closure = CLOSURE_BSDF_SHEEN_ID;
}

void SheenBsdfNode::compile(SVMCompiler &compiler)
{
  closure = distribution;
  BsdfNode::compile(compiler);
  compiler.add_node_data(SVMNodeSimpleBsdfData{
      .param1 = compiler.input_float("Roughness"),
      .normal_offset = compiler.input_link("Normal"),
  });
}

void SheenBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "distribution");
  compiler.add(this, "node_sheen_bsdf");
}

/* Diffuse BSDF Closure */

NODE_DEFINE(DiffuseBsdfNode)
{
  NodeType *type = NodeType::add("diffuse_bsdf", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_NORMAL(normal, "Normal", zero_float3(), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);
  SOCKET_IN_FLOAT(roughness, "Roughness", 0.0f);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

DiffuseBsdfNode::DiffuseBsdfNode() : BsdfNode(get_node_type())
{
  closure = CLOSURE_BSDF_DIFFUSE_ID;
}

void DiffuseBsdfNode::compile(SVMCompiler &compiler)
{
  BsdfNode::compile(compiler);
  compiler.add_node_data(SVMNodeDiffuseBsdfData{
      .color = compiler.input_float3("Color"),
      .roughness = compiler.input_float("Roughness"),
      .normal_offset = compiler.input_link("Normal"),
  });
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
  distribution_enum.insert("ggx", CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID);
  distribution_enum.insert("multi_ggx", CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID);
  SOCKET_ENUM(
      distribution, "Distribution", distribution_enum, CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID);

  static NodeEnum subsurface_method_enum;
  subsurface_method_enum.insert("burley", CLOSURE_BSSRDF_BURLEY_ID);
  subsurface_method_enum.insert("random_walk", CLOSURE_BSSRDF_RANDOM_WALK_ID);
  subsurface_method_enum.insert("random_walk_skin", CLOSURE_BSSRDF_RANDOM_WALK_SKIN_ID);
  subsurface_method_enum.insert("random_walk_legacy", CLOSURE_BSSRDF_RANDOM_WALK_LEGACY_ID);
  SOCKET_ENUM(subsurface_method,
              "Subsurface Method",
              subsurface_method_enum,
              CLOSURE_BSSRDF_RANDOM_WALK_ID);

  SOCKET_IN_COLOR(base_color, "Base Color", make_float3(0.8f, 0.8f, 0.8f))
  SOCKET_IN_FLOAT(metallic, "Metallic", 0.0f);
  SOCKET_IN_FLOAT(roughness, "Roughness", 0.5f);
  SOCKET_IN_FLOAT(ior, "IOR", 1.5f);
  SOCKET_IN_FLOAT(alpha, "Alpha", 1.0f);
  SOCKET_IN_NORMAL(normal, "Normal", zero_float3(), SocketType::LINK_NORMAL);

  SOCKET_IN_FLOAT(diffuse_roughness, "Diffuse Roughness", 0.0f);

  SOCKET_IN_FLOAT(subsurface_weight, "Subsurface Weight", 0.0f);
  SOCKET_IN_FLOAT(subsurface_scale, "Subsurface Scale", 0.1f);
  SOCKET_IN_VECTOR(subsurface_radius, "Subsurface Radius", make_float3(0.1f, 0.1f, 0.1f));
  SOCKET_IN_FLOAT(subsurface_ior, "Subsurface IOR", 1.4f);
  SOCKET_IN_FLOAT(subsurface_anisotropy, "Subsurface Anisotropy", 0.0f);

  SOCKET_IN_FLOAT(specular_ior_level, "Specular IOR Level", 0.5f);
  SOCKET_IN_COLOR(specular_tint, "Specular Tint", one_float3());
  SOCKET_IN_FLOAT(anisotropic, "Anisotropic", 0.0f);
  SOCKET_IN_FLOAT(anisotropic_rotation, "Anisotropic Rotation", 0.0f);
  SOCKET_IN_NORMAL(tangent, "Tangent", zero_float3(), SocketType::LINK_TANGENT);

  SOCKET_IN_FLOAT(transmission_weight, "Transmission Weight", 0.0f);

  SOCKET_IN_FLOAT(sheen_weight, "Sheen Weight", 0.0f);
  SOCKET_IN_FLOAT(sheen_roughness, "Sheen Roughness", 0.5f);
  SOCKET_IN_COLOR(sheen_tint, "Sheen Tint", one_float3());

  SOCKET_IN_FLOAT(coat_weight, "Coat Weight", 0.0f);
  SOCKET_IN_FLOAT(coat_roughness, "Coat Roughness", 0.03f);
  SOCKET_IN_FLOAT(coat_ior, "Coat IOR", 1.5f);
  SOCKET_IN_COLOR(coat_tint, "Coat Tint", one_float3());
  SOCKET_IN_NORMAL(coat_normal, "Coat Normal", zero_float3(), SocketType::LINK_NORMAL);

  SOCKET_IN_COLOR(emission_color, "Emission Color", one_float3());
  SOCKET_IN_FLOAT(emission_strength, "Emission Strength", 0.0f);

  SOCKET_IN_FLOAT(thin_film_thickness, "Thin Film Thickness", 0.0f);
  SOCKET_IN_FLOAT(thin_film_ior, "Thin Film IOR", 1.3f);

  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

PrincipledBsdfNode::PrincipledBsdfNode() : BsdfBaseNode(get_node_type())
{
  closure = CLOSURE_BSDF_PRINCIPLED_ID;
  distribution = CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID;
}

void PrincipledBsdfNode::simplify_settings(Scene * /* scene */)
{
  if (!has_surface_emission()) {
    /* Emission will be zero, so optimize away any connected emission input. */
    disconnect_unused_input("Emission Color");
    disconnect_unused_input("Emission Strength");
  }

  if (!has_surface_bssrdf()) {
    disconnect_unused_input("Subsurface Weight");
    disconnect_unused_input("Subsurface Radius");
    disconnect_unused_input("Subsurface Scale");
    disconnect_unused_input("Subsurface IOR");
    disconnect_unused_input("Subsurface Anisotropy");
  }

  if (!has_nonzero_weight("Coat Weight")) {
    disconnect_unused_input("Coat Weight");
    disconnect_unused_input("Coat IOR");
    disconnect_unused_input("Coat Roughness");
    disconnect_unused_input("Coat Tint");
  }

  if (!has_nonzero_weight("Sheen Weight")) {
    disconnect_unused_input("Sheen Weight");
    disconnect_unused_input("Sheen Roughness");
    disconnect_unused_input("Sheen Tint");
  }

  if (!has_nonzero_weight("Anisotropic")) {
    disconnect_unused_input("Anisotropic");
    disconnect_unused_input("Anisotropic Rotation");
    disconnect_unused_input("Tangent");
  }

  if (!has_nonzero_weight("Thin Film Thickness")) {
    disconnect_unused_input("Thin Film Thickness");
    disconnect_unused_input("Thin Film IOR");
  }
}

bool PrincipledBsdfNode::has_surface_transparent()
{
  return (input("Alpha")->link != nullptr || alpha < (1.0f - CLOSURE_WEIGHT_CUTOFF));
}

bool PrincipledBsdfNode::has_surface_emission()
{
  return (input("Emission Color")->link != nullptr ||
          reduce_max(emission_color) > CLOSURE_WEIGHT_CUTOFF) &&
         (input("Emission Strength")->link != nullptr ||
          emission_strength > CLOSURE_WEIGHT_CUTOFF);
}

bool PrincipledBsdfNode::has_surface_bssrdf()
{
  return (input("Subsurface Weight")->link != nullptr ||
          subsurface_weight > CLOSURE_WEIGHT_CUTOFF) &&
         (input("Subsurface Scale")->link != nullptr || subsurface_scale != 0.0f);
}

bool PrincipledBsdfNode::has_nonzero_weight(const char *name)
{
  ShaderInput *weight_in = input(name);
  if (weight_in == nullptr) {
    return true;
  }
  if (weight_in->link != nullptr) {
    return true;
  }
  return (get_float(weight_in->socket_type) >= CLOSURE_WEIGHT_CUTOFF);
}

void PrincipledBsdfNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface_link()) {

    if (!input("Tangent")->link) {
      attributes->add(ATTR_STD_GENERATED);
    }
  }

  ShaderNode::attributes(shader, attributes);
}

void PrincipledBsdfNode::compile(SVMCompiler &compiler)
{
  const SVMStackOffset normal_offset = compiler.input_link("Normal");
  const SVMStackOffset coat_normal_offset = compiler.input_link("Coat Normal");
  SVMStackOffset tangent_offset = SVM_STACK_INVALID;
  if (has_nonzero_weight("Anisotropic")) {
    tangent_offset = compiler.input_link("Tangent");
  }

  compiler.add_node(this,
                    NODE_CLOSURE_BSDF,
                    SVMNodeClosureBsdf{
                        .closure_type = closure,
                        .mix_weight_offset = compiler.closure_mix_weight_offset(),
                    });

  compiler.add_node_data(SVMNodePrincipledBsdfData{
      .distribution = distribution,
      .ior = compiler.input_float("IOR"),
      .roughness = compiler.input_float("Roughness"),
      /* Weights. */
      .sheen_weight = compiler.input_float("Sheen Weight"),
      .coat_weight = compiler.input_float("Coat Weight"),
      .metallic = compiler.input_float("Metallic"),
      .transmission_weight = compiler.input_float("Transmission Weight"),
      .subsurface_weight = compiler.input_float("Subsurface Weight"),
      /* Base. */
      .base_color = compiler.input_float3("Base Color"),
      .alpha = compiler.input_float("Alpha"),
      .diffuse_roughness = compiler.input_float("Diffuse Roughness"),
      /* Normals and tangents. */
      .normal_offset = normal_offset,
      .tangent_offset = tangent_offset,
      .coat_normal_offset = coat_normal_offset,
      /* Specular. */
      .specular_tint = compiler.input_float3("Specular Tint"),
      .specular_ior_level = compiler.input_float("Specular IOR Level"),
      .anisotropic = compiler.input_float("Anisotropic"),
      .anisotropic_rotation = compiler.input_float("Anisotropic Rotation"),
      /* Emission. */
      .emission_color = compiler.input_float3("Emission Color"),
      .emission_strength = compiler.input_float("Emission Strength"),
      /* Sheen. */
      .sheen_tint = compiler.input_float3("Sheen Tint"),
      .sheen_roughness = compiler.input_float("Sheen Roughness"),
      /* Coat. */
      .coat_tint = compiler.input_float3("Coat Tint"),
      .coat_roughness = compiler.input_float("Coat Roughness"),
      .coat_ior = compiler.input_float("Coat IOR"),
      /* Subsurface. */
      .subsurface_method = subsurface_method,
      .subsurface_radius = compiler.input_float3("Subsurface Radius"),
      .subsurface_scale = compiler.input_float("Subsurface Scale"),
      .subsurface_ior = compiler.input_float("Subsurface IOR"),
      .subsurface_anisotropy = compiler.input_float("Subsurface Anisotropy"),
      /* Thin film. */
      .thin_film_thickness = compiler.input_float("Thin Film Thickness"),
      .thin_film_ior = compiler.input_float("Thin Film IOR"),
  });
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
  SOCKET_IN_NORMAL(normal, "Normal", zero_float3(), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

TranslucentBsdfNode::TranslucentBsdfNode() : BsdfNode(get_node_type())
{
  closure = CLOSURE_BSDF_TRANSLUCENT_ID;
}

void TranslucentBsdfNode::compile(SVMCompiler &compiler)
{
  BsdfNode::compile(compiler);
  compiler.add_node_data(SVMNodeSimpleBsdfData{
      .normal_offset = compiler.input_link("Normal"),
  });
}

void TranslucentBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_translucent_bsdf");
}

/* Transparent BSDF Closure */

NODE_DEFINE(TransparentBsdfNode)
{
  NodeType *type = NodeType::add("transparent_bsdf", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", one_float3());
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

TransparentBsdfNode::TransparentBsdfNode() : BsdfNode(get_node_type())
{
  closure = CLOSURE_BSDF_TRANSPARENT_ID;
}

void TransparentBsdfNode::compile(SVMCompiler &compiler)
{
  BsdfNode::compile(compiler);
  compiler.add_node_data(SVMNodeSimpleBsdfData{});
}

void TransparentBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_transparent_bsdf");
}

/* Ray Portal BSDF Closure */

NODE_DEFINE(RayPortalBsdfNode)
{
  NodeType *type = NodeType::add("ray_portal_bsdf", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", one_float3());
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_IN_VECTOR(position, "Position", zero_float3(), SocketType::LINK_POSITION);
  SOCKET_IN_VECTOR(direction, "Direction", zero_float3());

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

RayPortalBsdfNode::RayPortalBsdfNode() : BsdfNode(get_node_type())
{
  closure = CLOSURE_BSDF_RAY_PORTAL_ID;
}

void RayPortalBsdfNode::compile(SVMCompiler &compiler)
{
  BsdfNode::compile(compiler);
  compiler.add_node_data(SVMNodeRayPortalBsdfData{
      .direction = compiler.input_float3("Direction"),
      .position_offset = compiler.input_link("Position"),
  });
}

void RayPortalBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_ray_portal_bsdf");
}

/* Subsurface Scattering Closure */

NODE_DEFINE(SubsurfaceScatteringNode)
{
  NodeType *type = NodeType::add("subsurface_scattering", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_NORMAL(normal, "Normal", zero_float3(), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  static NodeEnum method_enum;
  method_enum.insert("burley", CLOSURE_BSSRDF_BURLEY_ID);
  method_enum.insert("random_walk", CLOSURE_BSSRDF_RANDOM_WALK_ID);
  method_enum.insert("random_walk_skin", CLOSURE_BSSRDF_RANDOM_WALK_SKIN_ID);
  method_enum.insert("random_walk_legacy", CLOSURE_BSSRDF_RANDOM_WALK_LEGACY_ID);
  SOCKET_ENUM(method, "Method", method_enum, CLOSURE_BSSRDF_RANDOM_WALK_ID);

  SOCKET_IN_FLOAT(scale, "Scale", 0.01f);
  SOCKET_IN_VECTOR(radius, "Radius", make_float3(0.1f, 0.1f, 0.1f));

  SOCKET_IN_FLOAT(subsurface_ior, "IOR", 1.4f);
  SOCKET_IN_FLOAT(subsurface_roughness, "Roughness", 1.0f);
  SOCKET_IN_FLOAT(subsurface_anisotropy, "Anisotropy", 0.0f);

  SOCKET_OUT_CLOSURE(BSSRDF, "BSSRDF");

  return type;
}

SubsurfaceScatteringNode::SubsurfaceScatteringNode() : BsdfNode(get_node_type())
{
  closure = method;
}

void SubsurfaceScatteringNode::compile(SVMCompiler &compiler)
{
  closure = method;
  BsdfNode::compile(compiler);
  compiler.add_node_data(SVMNodeBssrdfData{
      .radius = compiler.input_float3("Radius"),
      .scale = compiler.input_float("Scale"),
      .ior = compiler.input_float("IOR"),
      .anisotropy = compiler.input_float("Anisotropy"),
      .roughness = compiler.input_float("Roughness"),
      .normal_offset = compiler.input_link("Normal"),
  });
}

void SubsurfaceScatteringNode::compile(OSLCompiler &compiler)
{
  closure = method;
  compiler.parameter(this, "method");
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
  SOCKET_IN_FLOAT(volume_mix_weight, "VolumeMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(emission, "Emission");

  return type;
}

EmissionNode::EmissionNode() : ShaderNode(get_node_type()) {}

void EmissionNode::compile(SVMCompiler &compiler)
{
  ShaderInput *color_in = input("Color");
  ShaderInput *strength_in = input("Strength");

  if (color_in->link || strength_in->link) {
    compiler.add_node(this,
                      NODE_EMISSION_WEIGHT,
                      SVMNodeEmissionWeight{
                          .color = compiler.input_float3("Color"),
                          .strength = compiler.input_float("Strength"),
                      });
  }
  else {
    const float3 w = color * strength;
    compiler.add_node(this,
                      NODE_CLOSURE_SET_WEIGHT,
                      SVMNodeClosureSetWeight{
                          .rgb = w,
                      });
  }

  compiler.add_node(this,
                    NODE_CLOSURE_EMISSION,
                    SVMNodeClosureEmission{
                        .mix_weight_offset = compiler.closure_mix_weight_offset(),
                    });
}

void EmissionNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_emission");
}

void EmissionNode::constant_fold(const ConstantFolder &folder)
{

  if ((!input("Color")->link && color == zero_float3()) ||
      (!input("Strength")->link && strength == 0.0f))
  {
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

BackgroundNode::BackgroundNode() : ShaderNode(get_node_type()) {}

void BackgroundNode::compile(SVMCompiler &compiler)
{
  ShaderInput *color_in = input("Color");
  ShaderInput *strength_in = input("Strength");

  if (color_in->link || strength_in->link) {
    compiler.add_node(this,
                      NODE_EMISSION_WEIGHT,
                      SVMNodeEmissionWeight{
                          .color = compiler.input_float3("Color"),
                          .strength = compiler.input_float("Strength"),
                      });
  }
  else {
    const float3 w = color * strength;
    compiler.add_node(this,
                      NODE_CLOSURE_SET_WEIGHT,
                      SVMNodeClosureSetWeight{
                          .rgb = w,
                      });
  }

  compiler.add_node(this,
                    NODE_CLOSURE_BACKGROUND,
                    SVMNodeClosureBackground{
                        .mix_weight_offset = compiler.closure_mix_weight_offset(),
                    });
}

void BackgroundNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_background");
}

void BackgroundNode::constant_fold(const ConstantFolder &folder)
{

  if ((!input("Color")->link && color == zero_float3()) ||
      (!input("Strength")->link && strength == 0.0f))
  {
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

HoldoutNode::HoldoutNode() : ShaderNode(get_node_type()) {}

void HoldoutNode::compile(SVMCompiler &compiler)
{
  const float3 value = one_float3();
  compiler.add_node(this,
                    NODE_CLOSURE_SET_WEIGHT,
                    SVMNodeClosureSetWeight{
                        .rgb = value,
                    });
  compiler.add_node(this,
                    NODE_CLOSURE_HOLDOUT,
                    SVMNodeClosureHoldout{
                        .mix_weight_offset = compiler.closure_mix_weight_offset(),
                    });
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

  SOCKET_IN_COLOR(color, "Color", one_float3());
  SOCKET_IN_FLOAT(distance, "Distance", 1.0f);
  SOCKET_IN_NORMAL(normal, "Normal", zero_float3(), SocketType::LINK_NORMAL);

  SOCKET_BOOLEAN(inside, "Inside", false);
  SOCKET_BOOLEAN(only_local, "Only Local", false);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(ao, "AO");

  return type;
}

AmbientOcclusionNode::AmbientOcclusionNode() : ShaderNode(get_node_type()) {}

void AmbientOcclusionNode::compile(SVMCompiler &compiler)
{
  ShaderInput *distance_in = input("Distance");

  int flags = (inside ? NODE_AO_INSIDE : 0) | (only_local ? NODE_AO_ONLY_LOCAL : 0);

  if (!distance_in->link && distance == 0.0f) {
    flags |= NODE_AO_GLOBAL_RADIUS;
  }

  compiler.add_node(this,
                    NODE_AMBIENT_OCCLUSION,
                    SVMNodeAmbientOcclusion{
                        .color = compiler.input_float3("Color"),
                        .dist = compiler.input_float("Distance"),
                        .flags = uint8_t(flags),
                        .samples = uint8_t(samples),
                        .normal_offset = compiler.input_link("Normal"),
                        .out_ao_offset = compiler.output("AO"),
                        .out_color_offset = compiler.output("Color"),
                    });
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

void VolumeNode::compile(SVMCompiler &compiler,
                         ShaderInput *density,
                         ShaderInput *param1,
                         ShaderInput *param2)
{
  ShaderInput *color_in = input("Color");

  if (color_in->link) {
    compiler.add_node(this,
                      NODE_CLOSURE_WEIGHT,
                      SVMNodeClosureWeight{
                          .weight_offset = compiler.input_link("Color"),
                      });
  }
  else {
    compiler.add_node(this,
                      NODE_CLOSURE_SET_WEIGHT,
                      SVMNodeClosureSetWeight{
                          .rgb = color,
                      });
  }

  /* Density and mix weight need to be stored the same way for all volume closures since there's
   * a shortcut code path if we only need the extinction value. */
  const SVMStackOffset mix_weight_ofs = compiler.closure_mix_weight_offset();

  compiler.add_node(
      this,
      NODE_CLOSURE_VOLUME,
      SVMNodeClosureVolume{
          .closure_type = closure,
          .density = (density) ? compiler.input_float(density->name().c_str()) : SVMInputFloat{0},
          .param1 = (param1) ? compiler.input_float(param1->name().c_str()) : SVMInputFloat{0},
          .param_extra = (param2) ? compiler.input_float(param2->name().c_str()) :
                                    SVMInputFloat{0},
          .mix_weight_offset = mix_weight_ofs,
      });
}

void VolumeNode::compile(SVMCompiler &compiler)
{
  compile(compiler, nullptr, nullptr, nullptr);
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

AbsorptionVolumeNode::AbsorptionVolumeNode() : VolumeNode(get_node_type())
{
  closure = CLOSURE_VOLUME_ABSORPTION_ID;
}

void AbsorptionVolumeNode::compile(SVMCompiler &compiler)
{
  VolumeNode::compile(compiler, input("Density"));
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
  SOCKET_IN_FLOAT(IOR, "IOR", 1.33f);
  SOCKET_IN_FLOAT(backscatter, "Backscatter", 0.1f);
  SOCKET_IN_FLOAT(alpha, "Alpha", 0.5f);
  SOCKET_IN_FLOAT(diameter, "Diameter", 20.0f);

  static NodeEnum phase_enum;
  phase_enum.insert("Henyey-Greenstein", CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID);
  phase_enum.insert("Fournier-Forand", CLOSURE_VOLUME_FOURNIER_FORAND_ID);
  phase_enum.insert("Draine", CLOSURE_VOLUME_DRAINE_ID);
  phase_enum.insert("Rayleigh", CLOSURE_VOLUME_RAYLEIGH_ID);
  phase_enum.insert("Mie", CLOSURE_VOLUME_MIE_ID);
  SOCKET_ENUM(phase, "Phase", phase_enum, CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID);

  SOCKET_IN_FLOAT(volume_mix_weight, "VolumeMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(volume, "Volume");

  return type;
}

ScatterVolumeNode::ScatterVolumeNode(const NodeType *node_type) : VolumeNode(node_type)
{
  closure = CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID;
}

ScatterVolumeNode::ScatterVolumeNode() : VolumeNode(get_node_type())
{
  closure = CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID;
}

void ScatterVolumeNode::compile(SVMCompiler &compiler)
{
  closure = phase;

  switch (phase) {
    case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID:
      VolumeNode::compile(compiler, input("Density"), input("Anisotropy"));
      break;
    case CLOSURE_VOLUME_FOURNIER_FORAND_ID:
      VolumeNode::compile(compiler, input("Density"), input("IOR"), input("Backscatter"));
      break;
    case CLOSURE_VOLUME_RAYLEIGH_ID:
      VolumeNode::compile(compiler, input("Density"));
      break;
    case CLOSURE_VOLUME_DRAINE_ID:
      VolumeNode::compile(compiler, input("Density"), input("Anisotropy"), input("Alpha"));
      break;
    case CLOSURE_VOLUME_MIE_ID:
      VolumeNode::compile(compiler, input("Density"), input("Diameter"));
      break;
    default:
      assert(false);
      break;
  }
}

void ScatterVolumeNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "phase");
  compiler.add(this, "node_scatter_volume");
}

/* Volume Coefficients Closure */

NODE_DEFINE(VolumeCoefficientsNode)
{
  NodeType *type = NodeType::add("volume_coefficients", create, NodeType::SHADER);

  SOCKET_IN_VECTOR(scatter_coeffs, "Scatter Coefficients", make_float3(1.0f, 1.0f, 1.0f));
  SOCKET_IN_VECTOR(absorption_coeffs, "Absorption Coefficients", make_float3(1.0f, 1.0f, 1.0f));
  SOCKET_IN_FLOAT(anisotropy, "Anisotropy", 0.0f);
  SOCKET_IN_FLOAT(IOR, "IOR", 1.33f);
  SOCKET_IN_FLOAT(backscatter, "Backscatter", 0.1f);
  SOCKET_IN_FLOAT(alpha, "Alpha", 0.5f);
  SOCKET_IN_FLOAT(diameter, "Diameter", 20.0f);
  SOCKET_IN_VECTOR(emission_coeffs, "Emission Coefficients", make_float3(0.0f, 0.0f, 0.0f));

  static NodeEnum phase_enum;
  phase_enum.insert("Henyey-Greenstein", CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID);
  phase_enum.insert("Fournier-Forand", CLOSURE_VOLUME_FOURNIER_FORAND_ID);
  phase_enum.insert("Draine", CLOSURE_VOLUME_DRAINE_ID);
  phase_enum.insert("Rayleigh", CLOSURE_VOLUME_RAYLEIGH_ID);
  phase_enum.insert("Mie", CLOSURE_VOLUME_MIE_ID);
  SOCKET_ENUM(phase, "Phase", phase_enum, CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID);

  SOCKET_IN_FLOAT(volume_mix_weight, "VolumeMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(volume, "Volume");

  return type;
}

VolumeCoefficientsNode::VolumeCoefficientsNode() : ScatterVolumeNode(get_node_type())
{
  closure = CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID;
}

void VolumeCoefficientsNode::compile(SVMCompiler &compiler)
{
  closure = phase;
  const char *param1 = nullptr;
  const char *param2 = nullptr;

  switch (phase) {
    case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID:
      param1 = "Anisotropy";
      break;
    case CLOSURE_VOLUME_FOURNIER_FORAND_ID:
      param1 = "IOR";
      param2 = "Backscatter";
      break;
    case CLOSURE_VOLUME_RAYLEIGH_ID:
      break;
    case CLOSURE_VOLUME_DRAINE_ID:
      param1 = "Anisotropy";
      param2 = "Alpha";
      break;
    case CLOSURE_VOLUME_MIE_ID:
      param1 = "Diameter";
      break;
    default:
      assert(false);
      break;
  }

  if (input("Scatter Coefficients")->link) {
    compiler.add_node(this,
                      NODE_CLOSURE_WEIGHT,
                      SVMNodeClosureWeight{
                          .weight_offset = compiler.input_link("Scatter Coefficients"),
                      });
  }
  else {
    compiler.add_node(this,
                      NODE_CLOSURE_SET_WEIGHT,
                      SVMNodeClosureSetWeight{
                          .rgb = scatter_coeffs,
                      });
  }

  const SVMStackOffset mix_weight_ofs = compiler.closure_mix_weight_offset();

  compiler.add_node(this,
                    NODE_VOLUME_COEFFICIENTS,
                    SVMNodeVolumeCoefficients{
                        .closure_type = closure,
                        .absorption_coeffs = compiler.input_float3("Absorption Coefficients"),
                        .emission_coeffs = compiler.input_float3("Emission Coefficients"),
                        .param1 = (param1) ? compiler.input_float(param1) : SVMInputFloat{0},
                        .param_extra = (param2) ? compiler.input_float(param2) : SVMInputFloat{0},
                        .mix_weight_offset = mix_weight_ofs,
                    });
}

void VolumeCoefficientsNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "phase");
  compiler.add(this, "node_volume_coefficients");
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
  SOCKET_IN_COLOR(absorption_color, "Absorption Color", zero_float3());
  SOCKET_IN_FLOAT(emission_strength, "Emission Strength", 0.0f);
  SOCKET_IN_COLOR(emission_color, "Emission Color", one_float3());
  SOCKET_IN_FLOAT(blackbody_intensity, "Blackbody Intensity", 0.0f);
  SOCKET_IN_COLOR(blackbody_tint, "Blackbody Tint", one_float3());
  SOCKET_IN_FLOAT(temperature, "Temperature", 1000.0f);
  SOCKET_IN_FLOAT(volume_mix_weight, "VolumeMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(volume, "Volume");

  return type;
}

PrincipledVolumeNode::PrincipledVolumeNode() : VolumeNode(get_node_type())
{
  closure = CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID;
  density_attribute = ustring("density");
  temperature_attribute = ustring("temperature");
}

void PrincipledVolumeNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_volume) {

    if (input("Density")->link || density > 0.0f) {
      attributes->add_standard(density_attribute);
      attributes->add_standard(color_attribute);
    }

    if (input("Blackbody Intensity")->link || blackbody_intensity > 0.0f) {
      attributes->add_standard(temperature_attribute);
    }

    attributes->add(ATTR_STD_GENERATED_TRANSFORM);
  }

  ShaderNode::attributes(shader, attributes);
}

void PrincipledVolumeNode::compile(SVMCompiler &compiler)
{
  ShaderInput *color_in = input("Color");

  if (color_in->link) {
    compiler.add_node(this,
                      NODE_CLOSURE_WEIGHT,
                      SVMNodeClosureWeight{
                          .weight_offset = compiler.input_link("Color"),
                      });
  }
  else {
    compiler.add_node(this,
                      NODE_CLOSURE_SET_WEIGHT,
                      SVMNodeClosureSetWeight{
                          .rgb = color,
                      });
  }

  compiler.add_node(
      this,
      NODE_PRINCIPLED_VOLUME,
      SVMNodePrincipledVolume{
          .absorption_color = compiler.input_float3("Absorption Color"),
          .emission_color = compiler.input_float3("Emission Color"),
          .blackbody_tint = compiler.input_float3("Blackbody Tint"),
          .density = compiler.input_float("Density"),
          .anisotropy = compiler.input_float("Anisotropy"),
          .emission = compiler.input_float("Emission Strength"),
          .blackbody = compiler.input_float("Blackbody Intensity"),
          .temperature = compiler.input_float("Temperature"),
          .attr_density = (int)compiler.attribute_standard(density_attribute),
          .attr_color = (int)compiler.attribute_standard(color_attribute),
          .attr_temperature = (int)compiler.attribute_standard(temperature_attribute),
          .mix_weight_offset = compiler.closure_mix_weight_offset(),
      });
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

  /* Scattering models. */
  static NodeEnum model_enum;
  model_enum.insert("Chiang", NODE_PRINCIPLED_HAIR_CHIANG);
  model_enum.insert("Huang", NODE_PRINCIPLED_HAIR_HUANG);
  SOCKET_ENUM(model, "Model", model_enum, NODE_PRINCIPLED_HAIR_HUANG);

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
  SOCKET_IN_VECTOR(
      absorption_coefficient, "Absorption Coefficient", make_float3(0.245531f, 0.52f, 1.365f));

  SOCKET_IN_FLOAT(aspect_ratio, "Aspect Ratio", 0.85f);

  SOCKET_IN_FLOAT(offset, "Offset", 2.f * M_PI_F / 180.f);
  SOCKET_IN_FLOAT(roughness, "Roughness", 0.3f);
  SOCKET_IN_FLOAT(radial_roughness, "Radial Roughness", 0.3f);
  SOCKET_IN_FLOAT(coat, "Coat", 0.0f);
  SOCKET_IN_FLOAT(ior, "IOR", 1.55f);

  SOCKET_IN_FLOAT(random_roughness, "Random Roughness", 0.0f);
  SOCKET_IN_FLOAT(random_color, "Random Color", 0.0f);
  SOCKET_IN_FLOAT(random, "Random", 0.0f);

  SOCKET_IN_FLOAT(R, "R lobe", 1.0f);
  SOCKET_IN_FLOAT(TT, "TT lobe", 1.0f);
  SOCKET_IN_FLOAT(TRT, "TRT lobe", 1.0f);

  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

PrincipledHairBsdfNode::PrincipledHairBsdfNode() : BsdfBaseNode(get_node_type())
{
  closure = CLOSURE_BSDF_HAIR_HUANG_ID;
}

/* Treat hair as transparent if the hit is outside of the projected width. */
bool PrincipledHairBsdfNode::has_surface_transparent()
{
  if (model == NODE_PRINCIPLED_HAIR_HUANG) {
    if (aspect_ratio != 1.0f || input("Aspect Ratio")->link) {
      return true;
    }
  }
  return false;
}

void PrincipledHairBsdfNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (has_surface_transparent()) {
    /* Make sure we have the normal for elliptical cross section tracking. */
    attributes->add(ATTR_STD_VERTEX_NORMAL);
  }

  if (!input("Random")->link) {
    /* Enable retrieving Hair Info -> Random if Random isn't linked. */
    attributes->add(ATTR_STD_CURVE_RANDOM);
  }
  ShaderNode::attributes(shader, attributes);
}

/* Prepares the input data for the SVM shader. */
void PrincipledHairBsdfNode::compile(SVMCompiler &compiler)
{
  closure = (model == NODE_PRINCIPLED_HAIR_HUANG) ? CLOSURE_BSDF_HAIR_HUANG_ID :
                                                    CLOSURE_BSDF_HAIR_CHIANG_ID;
  compiler.add_node(this,
                    NODE_CLOSURE_SET_WEIGHT,
                    SVMNodeClosureSetWeight{
                        .rgb = one_float3(),
                    });

  ShaderInput *random_in = input("Random");
  const int attr_random = random_in->link ? SVM_STACK_INVALID :
                                            compiler.attribute(ATTR_STD_CURVE_RANDOM);

  /* Encode all parameters into data nodes. */
  compiler.add_node(this,
                    NODE_CLOSURE_BSDF,
                    SVMNodeClosureBsdf{
                        .closure_type = closure,
                        .mix_weight_offset = compiler.closure_mix_weight_offset(),
                    });

  const bool is_huang = (model == NODE_PRINCIPLED_HAIR_HUANG);
  compiler.add_node_data(SVMNodePrincipledHairBsdfData{
      .parametrization = parametrization,
      .color = compiler.input_float3("Color"),
      .tint = compiler.input_float3("Tint"),
      .absorption_coefficient = compiler.input_float3("Absorption Coefficient"),
      .roughness = compiler.input_float("Roughness"),
      .random_roughness = compiler.input_float("Random Roughness"),
      .offset = compiler.input_float("Offset"),
      .ior = compiler.input_float("IOR"),
      .random = compiler.input_float("Random"),
      .melanin = compiler.input_float("Melanin"),
      .melanin_redness = compiler.input_float("Melanin Redness"),
      .coat = compiler.input_float("Coat"),
      .aspect_ratio = compiler.input_float("Aspect Ratio"),
      .radial_roughness = compiler.input_float("Radial Roughness"),
      .random_color = compiler.input_float("Random Color"),
      .R = compiler.input_float("R lobe"),
      .TT = compiler.input_float("TT lobe"),
      .TRT = compiler.input_float("TRT lobe"),
      .attr_random = attr_random,
      .attr_normal = is_huang ? (int)compiler.attribute(ATTR_STD_VERTEX_NORMAL) : 0,
  });
}

/* Prepares the input data for the OSL shader. */
void PrincipledHairBsdfNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "model");
  compiler.parameter(this, "parametrization");
  compiler.add(this, "node_principled_hair_bsdf");
}

/* Hair BSDF Closure */

NODE_DEFINE(HairBsdfNode)
{
  NodeType *type = NodeType::add("hair_bsdf", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", make_float3(0.8f, 0.8f, 0.8f));
  SOCKET_IN_FLOAT(surface_mix_weight, "SurfaceMixWeight", 0.0f, SocketType::SVM_INTERNAL);

  static NodeEnum component_enum;
  component_enum.insert("reflection", CLOSURE_BSDF_HAIR_REFLECTION_ID);
  component_enum.insert("transmission", CLOSURE_BSDF_HAIR_TRANSMISSION_ID);
  SOCKET_ENUM(component, "Component", component_enum, CLOSURE_BSDF_HAIR_REFLECTION_ID);
  SOCKET_IN_FLOAT(offset, "Offset", 0.0f);
  SOCKET_IN_FLOAT(roughness_u, "RoughnessU", 0.2f);
  SOCKET_IN_FLOAT(roughness_v, "RoughnessV", 0.2f);
  SOCKET_IN_VECTOR(tangent, "Tangent", zero_float3());

  SOCKET_OUT_CLOSURE(BSDF, "BSDF");

  return type;
}

HairBsdfNode::HairBsdfNode() : BsdfNode(get_node_type())
{
  closure = CLOSURE_BSDF_HAIR_REFLECTION_ID;
}

void HairBsdfNode::compile(SVMCompiler &compiler)
{
  closure = component;

  BsdfNode::compile(compiler);
  compiler.add_node_data(SVMNodeHairBsdfData{
      .roughness1 = compiler.input_float("RoughnessU"),
      .roughness2 = compiler.input_float("RoughnessV"),
      .offset = compiler.input_float("Offset"),
      .tangent_offset = compiler.input_link("Tangent"),
  });
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

  SOCKET_OUT_POINT(position, "Position");
  SOCKET_OUT_NORMAL(normal, "Normal");
  SOCKET_OUT_NORMAL(tangent, "Tangent");
  SOCKET_OUT_NORMAL(true_normal, "True Normal");
  SOCKET_OUT_VECTOR(incoming, "Incoming");
  SOCKET_OUT_POINT(parametric, "Parametric");
  SOCKET_OUT_FLOAT(backfacing, "Backfacing");
  SOCKET_OUT_FLOAT(pointiness, "Pointiness");
  SOCKET_OUT_FLOAT(random_per_island, "Random Per Island");

  return type;
}

GeometryNode::GeometryNode() : ShaderNode(get_node_type())
{
  special_type = SHADER_SPECIAL_TYPE_GEOMETRY;
}

void GeometryNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface_link()) {
    if (!output("Tangent")->links.empty()) {
      attributes->add(ATTR_STD_GENERATED);
    }
    if (!output("Pointiness")->links.empty()) {
      attributes->add(ATTR_STD_POINTINESS);
    }
    if (!output("Random Per Island")->links.empty()) {
      attributes->add(ATTR_STD_RANDOM_PER_ISLAND);
    }
  }

  ShaderNode::attributes(shader, attributes);
}

ShaderNodeType GeometryNode::shader_node_type() const
{
  return NODE_GEOMETRY;
}

static NodeBumpOffset shader_bump_to_node_bump_offset(ShaderBump bump)
{
  switch (bump) {
    case SHADER_BUMP_DX:
      return NODE_BUMP_OFFSET_DX;
    case SHADER_BUMP_DY:
      return NODE_BUMP_OFFSET_DY;
    default:
      return NODE_BUMP_OFFSET_CENTER;
  }
}

void GeometryNode::compile(SVMCompiler &compiler)
{
  const NodeBumpOffset bump_offset = shader_bump_to_node_bump_offset(bump);
  const bool use_derivative = need_derivatives() || (bump != SHADER_BUMP_NONE);
  const uint8_t store_derivatives = need_derivatives();
  ShaderOutput *out;

  out = output("Position");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_GEOMETRY,
                      SVMNodeGeometry{
                          .geom_type = NODE_GEOM_P,
                          .bump_offset = bump_offset,
                          .store_derivatives = store_derivatives,
                          .out_offset = compiler.output("Position"),
                          .bump_filter_width = bump_filter_width,
                      },
                      use_derivative);
  }

  /* Currently no bump offset is supported for Normal, Tangent, True Normal, and Incoming. */
  out = output("Normal");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_GEOMETRY,
                      SVMNodeGeometry{
                          .geom_type = NODE_GEOM_N,
                          .bump_offset = NODE_BUMP_OFFSET_CENTER,
                          .store_derivatives = store_derivatives,
                          .out_offset = compiler.output("Normal"),
                          .bump_filter_width = bump_filter_width,
                      });
  }

  out = output("Tangent");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_GEOMETRY,
                      SVMNodeGeometry{
                          .geom_type = NODE_GEOM_T,
                          .bump_offset = NODE_BUMP_OFFSET_CENTER,
                          .store_derivatives = store_derivatives,
                          .out_offset = compiler.output("Tangent"),
                          .bump_filter_width = bump_filter_width,
                      },
                      use_derivative);
  }

  out = output("True Normal");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_GEOMETRY,
                      SVMNodeGeometry{
                          .geom_type = NODE_GEOM_Ng,
                          .bump_offset = NODE_BUMP_OFFSET_CENTER,
                          .store_derivatives = store_derivatives,
                          .out_offset = compiler.output("True Normal"),
                          .bump_filter_width = bump_filter_width,
                      },
                      use_derivative);
  }

  out = output("Incoming");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_GEOMETRY,
                      SVMNodeGeometry{
                          .geom_type = NODE_GEOM_I,
                          .bump_offset = NODE_BUMP_OFFSET_CENTER,
                          .store_derivatives = store_derivatives,
                          .out_offset = compiler.output("Incoming"),
                          .bump_filter_width = bump_filter_width,
                      },
                      use_derivative);
  }

  out = output("Parametric");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_GEOMETRY,
                      SVMNodeGeometry{
                          .geom_type = NODE_GEOM_uv,
                          .bump_offset = bump_offset,
                          .store_derivatives = store_derivatives,
                          .out_offset = compiler.output("Parametric"),
                          .bump_filter_width = bump_filter_width,
                      },
                      use_derivative);
  }

  out = output("Backfacing");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_PATH,
                      SVMNodeLightPath{
                          .path_type = NODE_LP_backfacing,
                          .out_offset = compiler.output("Backfacing"),
                      });
  }

  out = output("Pointiness");
  if (!out->links.empty()) {
    if (compiler.output_type() != SHADER_TYPE_VOLUME) {
      compiler.add_node(this,
                        NODE_ATTR,
                        SVMNodeAttr{
                            .attr = int(ATTR_STD_POINTINESS),
                            .out_offset = compiler.output("Pointiness"),
                            .output_type = NODE_ATTR_OUTPUT_FLOAT,
                            .bump_filter_width = bump_filter_width,
                        },
                        use_derivative);
    }
    else {
      compiler.add_value_node(this, 0.0f, compiler.output("Pointiness"));
    }
  }

  out = output("Random Per Island");
  if (!out->links.empty()) {
    if (compiler.output_type() != SHADER_TYPE_VOLUME) {
      compiler.add_node(this,
                        NODE_ATTR,
                        SVMNodeAttr{
                            .attr = int(ATTR_STD_RANDOM_PER_ISLAND),
                            .out_offset = compiler.output("Random Per Island"),
                            .output_type = NODE_ATTR_OUTPUT_FLOAT,
                            .bump_filter_width = bump_filter_width,
                        },
                        use_derivative);
    }
    else {
      compiler.add_value_node(this, 0.0f, compiler.output("Random Per Island"));
    }
  }
}

void GeometryNode::compile(OSLCompiler &compiler)
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
  compiler.parameter("bump_filter_width", bump_filter_width);

  compiler.add(this, "node_geometry");
}

/* TextureCoordinate */

NODE_DEFINE(TextureCoordinateNode)
{
  NodeType *type = NodeType::add("texture_coordinate", create, NodeType::SHADER);

  SOCKET_BOOLEAN(from_dupli, "From Dupli", false);
  SOCKET_BOOLEAN(use_transform, "Use Transform", false);
  SOCKET_TRANSFORM(ob_tfm, "Object Transform", transform_identity());

  SOCKET_OUT_POINT(generated, "Generated");
  SOCKET_OUT_NORMAL(normal, "Normal");
  SOCKET_OUT_POINT(UV, "UV");
  SOCKET_OUT_POINT(object, "Object");
  SOCKET_OUT_POINT(camera, "Camera");
  SOCKET_OUT_POINT(window, "Window");
  SOCKET_OUT_NORMAL(reflection, "Reflection");

  return type;
}

TextureCoordinateNode::TextureCoordinateNode() : ShaderNode(get_node_type()) {}

void TextureCoordinateNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface_link()) {
    if (!from_dupli) {
      if (!output("Generated")->links.empty()) {
        attributes->add(ATTR_STD_GENERATED);
      }
      if (!output("UV")->links.empty()) {
        attributes->add(ATTR_STD_UV);
      }
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

ShaderNodeType TextureCoordinateNode::shader_node_type() const
{
  return NODE_TEX_COORD;
}

void TextureCoordinateNode::compile(SVMCompiler &compiler)
{
  const NodeBumpOffset bump_offset = shader_bump_to_node_bump_offset(bump);
  const bool use_derivative = need_derivatives() || (bump != SHADER_BUMP_NONE);
  const bool store_derivatives = need_derivatives();
  ShaderOutput *out;

  out = output("Generated");
  if (!out->links.empty()) {
    if (compiler.background) {
      compiler.add_node(this,
                        NODE_GEOMETRY,
                        SVMNodeGeometry{
                            .geom_type = NODE_GEOM_P,
                            .bump_offset = bump_offset,
                            .store_derivatives = store_derivatives,
                            .out_offset = compiler.output("Generated"),
                            .bump_filter_width = bump_filter_width,
                        },
                        use_derivative);
    }
    else {
      if (from_dupli) {
        /* Dupli generated coordinates are constant, no bump offset. */
        compiler.add_node(this,
                          NODE_TEX_COORD,
                          SVMNodeTexCoord{
                              .texco_type = NODE_TEXCO_DUPLI_GENERATED,
                              .bump_offset = NODE_BUMP_OFFSET_CENTER,
                              .store_derivatives = store_derivatives,
                              .out_offset = compiler.output("Generated"),
                              .bump_filter_width = bump_filter_width,
                          },
                          use_derivative);
      }
      else if (compiler.output_type() == SHADER_TYPE_VOLUME) {
        compiler.add_node(this,
                          NODE_TEX_COORD,
                          SVMNodeTexCoord{
                              .texco_type = NODE_TEXCO_VOLUME_GENERATED,
                              .bump_offset = bump_offset,
                              .store_derivatives = store_derivatives,
                              .out_offset = compiler.output("Generated"),
                              .bump_filter_width = bump_filter_width,
                          },
                          use_derivative);
      }
      else {
        compiler.add_node(this,
                          NODE_ATTR,
                          SVMNodeAttr{
                              .attr = int(compiler.attribute(ATTR_STD_GENERATED)),
                              .out_offset = compiler.output("Generated"),
                              .output_type = NODE_ATTR_OUTPUT_FLOAT3,
                              .bump_offset = bump_offset,
                              .store_derivatives = store_derivatives,
                              .bump_filter_width = bump_filter_width,
                          },
                          use_derivative);
      }
    }
  }

  out = output("Normal");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_TEX_COORD,
                      SVMNodeTexCoord{
                          .texco_type = NODE_TEXCO_NORMAL,
                          .bump_offset = bump_offset,
                          .store_derivatives = store_derivatives,
                          .out_offset = compiler.output("Normal"),
                          .bump_filter_width = bump_filter_width,
                      },
                      use_derivative);
  }

  out = output("UV");
  if (!out->links.empty()) {
    if (from_dupli) {
      /* Dupli UV coordinates aren't constant, no bump offset. */
      compiler.add_node(this,
                        NODE_TEX_COORD,
                        SVMNodeTexCoord{
                            .texco_type = NODE_TEXCO_DUPLI_UV,
                            .bump_offset = NODE_BUMP_OFFSET_CENTER,
                            .store_derivatives = store_derivatives,
                            .out_offset = compiler.output("UV"),
                            .bump_filter_width = bump_filter_width,
                        },
                        use_derivative);
    }
    else {
      compiler.add_node(this,
                        NODE_ATTR,
                        SVMNodeAttr{
                            .attr = int(compiler.attribute(ATTR_STD_UV)),
                            .out_offset = compiler.output("UV"),
                            .output_type = NODE_ATTR_OUTPUT_FLOAT3,
                            .bump_offset = bump_offset,
                            .store_derivatives = store_derivatives,
                            .bump_filter_width = bump_filter_width,
                        },
                        use_derivative);
    }
  }

  out = output("Object");
  if (!out->links.empty()) {
    compiler.add_node(
        this,
        NODE_TEX_COORD,
        SVMNodeTexCoord{
            .texco_type = (use_transform) ? NODE_TEXCO_OBJECT_WITH_TRANSFORM : NODE_TEXCO_OBJECT,
            .bump_offset = bump_offset,
            .store_derivatives = store_derivatives,
            .out_offset = compiler.output("Object"),
            .bump_filter_width = bump_filter_width,
        },
        use_derivative);
    if (use_transform) {
      const PackedTransform ob_itfm = transform_inverse(ob_tfm);
      compiler.add_node_data(ob_itfm);
    }
  }

  out = output("Camera");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_TEX_COORD,
                      SVMNodeTexCoord{
                          .texco_type = NODE_TEXCO_CAMERA,
                          .bump_offset = bump_offset,
                          .store_derivatives = store_derivatives,
                          .out_offset = compiler.output("Camera"),
                          .bump_filter_width = bump_filter_width,
                      },
                      use_derivative);
  }

  out = output("Window");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_TEX_COORD,
                      SVMNodeTexCoord{
                          .texco_type = NODE_TEXCO_WINDOW,
                          .bump_offset = bump_offset,
                          .store_derivatives = store_derivatives,
                          .out_offset = compiler.output("Window"),
                          .bump_filter_width = bump_filter_width,
                      },
                      use_derivative);
  }

  /* Reflection currently does not support bump offset. */
  out = output("Reflection");
  if (!out->links.empty()) {
    if (compiler.background) {
      compiler.add_node(this,
                        NODE_GEOMETRY,
                        SVMNodeGeometry{
                            .geom_type = NODE_GEOM_I,
                            .bump_offset = NODE_BUMP_OFFSET_CENTER,
                            .store_derivatives = store_derivatives,
                            .out_offset = compiler.output("Reflection"),
                            .bump_filter_width = bump_filter_width,
                        },
                        use_derivative);
    }
    else {
      compiler.add_node(this,
                        NODE_TEX_COORD,
                        SVMNodeTexCoord{
                            .texco_type = NODE_TEXCO_REFLECTION,
                            .bump_offset = NODE_BUMP_OFFSET_CENTER,
                            .store_derivatives = store_derivatives,
                            .out_offset = compiler.output("Reflection"),
                            .bump_filter_width = bump_filter_width,
                        },
                        use_derivative);
    }
  }
}

void TextureCoordinateNode::compile(OSLCompiler &compiler)
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
  compiler.parameter("bump_filter_width", bump_filter_width);

  if (compiler.background) {
    compiler.parameter("is_background", true);
  }
  if (compiler.output_type() == SHADER_TYPE_VOLUME) {
    compiler.parameter("is_volume", true);
  }
  compiler.parameter(this, "use_transform");
  const Transform ob_itfm = transform_inverse(ob_tfm);
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

UVMapNode::UVMapNode() : ShaderNode(get_node_type()) {}

void UVMapNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface) {
    if (!from_dupli) {
      if (!output("UV")->links.empty()) {
        if (!attribute.empty()) {
          attributes->add(attribute);
        }
        else {
          attributes->add(ATTR_STD_UV);
        }
      }
    }
  }

  ShaderNode::attributes(shader, attributes);
}

ShaderNodeType UVMapNode::shader_node_type() const
{
  return NODE_TEX_COORD;
}

void UVMapNode::compile(SVMCompiler &compiler)
{
  const NodeBumpOffset bump_offset = shader_bump_to_node_bump_offset(bump);
  const bool use_derivative = need_derivatives() || (bump != SHADER_BUMP_NONE);
  const bool store_derivatives = need_derivatives();
  ShaderOutput *out = output("UV");

  if (!out->links.empty()) {
    if (from_dupli) {
      /* Dupli UV coordinates are constant, no bump offset. */
      compiler.add_node(this,
                        NODE_TEX_COORD,
                        SVMNodeTexCoord{
                            .texco_type = NODE_TEXCO_DUPLI_UV,
                            .bump_offset = NODE_BUMP_OFFSET_CENTER,
                            .store_derivatives = store_derivatives,
                            .out_offset = compiler.output("UV"),
                            .bump_filter_width = bump_filter_width,
                        },
                        use_derivative);
    }
    else {
      int attr;
      if (!attribute.empty()) {
        attr = compiler.attribute(attribute);
      }
      else {
        attr = compiler.attribute(ATTR_STD_UV);
      }

      compiler.add_node(this,
                        NODE_ATTR,
                        SVMNodeAttr{
                            .attr = attr,
                            .out_offset = compiler.output("UV"),
                            .output_type = NODE_ATTR_OUTPUT_FLOAT3,
                            .bump_offset = bump_offset,
                            .store_derivatives = store_derivatives,
                            .bump_filter_width = bump_filter_width,
                        },
                        use_derivative);
    }
  }
}

void UVMapNode::compile(OSLCompiler &compiler)
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
  compiler.parameter("bump_filter_width", bump_filter_width);

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
  SOCKET_OUT_FLOAT(portal_depth, "Portal Depth");

  return type;
}

LightPathNode::LightPathNode() : ShaderNode(get_node_type())
{
  special_type = SHADER_SPECIAL_TYPE_LIGHT_PATH;
}

void LightPathNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *out;

  out = output("Is Camera Ray");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_PATH,
                      SVMNodeLightPath{
                          .path_type = NODE_LP_camera,
                          .out_offset = compiler.output("Is Camera Ray"),
                      });
  }

  out = output("Is Shadow Ray");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_PATH,
                      SVMNodeLightPath{
                          .path_type = NODE_LP_shadow,
                          .out_offset = compiler.output("Is Shadow Ray"),
                      });
  }

  out = output("Is Diffuse Ray");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_PATH,
                      SVMNodeLightPath{
                          .path_type = NODE_LP_diffuse,
                          .out_offset = compiler.output("Is Diffuse Ray"),
                      });
  }

  out = output("Is Glossy Ray");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_PATH,
                      SVMNodeLightPath{
                          .path_type = NODE_LP_glossy,
                          .out_offset = compiler.output("Is Glossy Ray"),
                      });
  }

  out = output("Is Singular Ray");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_PATH,
                      SVMNodeLightPath{
                          .path_type = NODE_LP_singular,
                          .out_offset = compiler.output("Is Singular Ray"),
                      });
  }

  out = output("Is Reflection Ray");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_PATH,
                      SVMNodeLightPath{
                          .path_type = NODE_LP_reflection,
                          .out_offset = compiler.output("Is Reflection Ray"),
                      });
  }

  out = output("Is Transmission Ray");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_PATH,
                      SVMNodeLightPath{
                          .path_type = NODE_LP_transmission,
                          .out_offset = compiler.output("Is Transmission Ray"),
                      });
  }

  out = output("Is Volume Scatter Ray");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_PATH,
                      SVMNodeLightPath{
                          .path_type = NODE_LP_volume_scatter,
                          .out_offset = compiler.output("Is Volume Scatter Ray"),
                      });
  }

  out = output("Ray Length");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_PATH,
                      SVMNodeLightPath{
                          .path_type = NODE_LP_ray_length,
                          .out_offset = compiler.output("Ray Length"),
                      });
  }

  out = output("Ray Depth");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_PATH,
                      SVMNodeLightPath{
                          .path_type = NODE_LP_ray_depth,
                          .out_offset = compiler.output("Ray Depth"),
                      });
  }

  out = output("Diffuse Depth");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_PATH,
                      SVMNodeLightPath{
                          .path_type = NODE_LP_ray_diffuse,
                          .out_offset = compiler.output("Diffuse Depth"),
                      });
  }

  out = output("Glossy Depth");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_PATH,
                      SVMNodeLightPath{
                          .path_type = NODE_LP_ray_glossy,
                          .out_offset = compiler.output("Glossy Depth"),
                      });
  }

  out = output("Transparent Depth");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_PATH,
                      SVMNodeLightPath{
                          .path_type = NODE_LP_ray_transparent,
                          .out_offset = compiler.output("Transparent Depth"),
                      });
  }

  out = output("Transmission Depth");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_PATH,
                      SVMNodeLightPath{
                          .path_type = NODE_LP_ray_transmission,
                          .out_offset = compiler.output("Transmission Depth"),
                      });
  }

  out = output("Portal Depth");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_PATH,
                      SVMNodeLightPath{
                          .path_type = NODE_LP_ray_portal,
                          .out_offset = compiler.output("Portal Depth"),
                      });
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

LightFalloffNode::LightFalloffNode() : ShaderNode(get_node_type()) {}

void LightFalloffNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *out = output("Quadratic");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_FALLOFF,
                      SVMNodeLightFalloff{
                          .falloff_type = NODE_LIGHT_FALLOFF_QUADRATIC,
                          .strength = compiler.input_float("Strength"),
                          .smooth = compiler.input_float("Smooth"),
                          .out_offset = compiler.output("Quadratic"),
                      });
  }

  out = output("Linear");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_FALLOFF,
                      SVMNodeLightFalloff{
                          .falloff_type = NODE_LIGHT_FALLOFF_LINEAR,
                          .strength = compiler.input_float("Strength"),
                          .smooth = compiler.input_float("Smooth"),
                          .out_offset = compiler.output("Linear"),
                      });
  }

  out = output("Constant");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_LIGHT_FALLOFF,
                      SVMNodeLightFalloff{
                          .falloff_type = NODE_LIGHT_FALLOFF_CONSTANT,
                          .strength = compiler.input_float("Strength"),
                          .smooth = compiler.input_float("Smooth"),
                          .out_offset = compiler.output("Constant"),
                      });
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
  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(alpha, "Alpha");
  SOCKET_OUT_FLOAT(object_index, "Object Index");
  SOCKET_OUT_FLOAT(material_index, "Material Index");
  SOCKET_OUT_FLOAT(random, "Random");

  return type;
}

ObjectInfoNode::ObjectInfoNode() : ShaderNode(get_node_type()) {}

void ObjectInfoNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *out = output("Location");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_OBJECT_INFO,
                      SVMNodeObjectInfo{
                          .info_type = NODE_INFO_OB_LOCATION,
                          .out_offset = compiler.output("Location"),
                      });
  }

  out = output("Color");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_OBJECT_INFO,
                      SVMNodeObjectInfo{
                          .info_type = NODE_INFO_OB_COLOR,
                          .out_offset = compiler.output("Color"),
                      });
  }

  out = output("Alpha");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_OBJECT_INFO,
                      SVMNodeObjectInfo{
                          .info_type = NODE_INFO_OB_ALPHA,
                          .out_offset = compiler.output("Alpha"),
                      });
  }

  out = output("Object Index");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_OBJECT_INFO,
                      SVMNodeObjectInfo{
                          .info_type = NODE_INFO_OB_INDEX,
                          .out_offset = compiler.output("Object Index"),
                      });
  }

  out = output("Material Index");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_OBJECT_INFO,
                      SVMNodeObjectInfo{
                          .info_type = NODE_INFO_MAT_INDEX,
                          .out_offset = compiler.output("Material Index"),
                      });
  }

  out = output("Random");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_OBJECT_INFO,
                      SVMNodeObjectInfo{
                          .info_type = NODE_INFO_OB_RANDOM,
                          .out_offset = compiler.output("Random"),
                      });
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

ParticleInfoNode::ParticleInfoNode() : ShaderNode(get_node_type()) {}

void ParticleInfoNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (!output("Index")->links.empty()) {
    attributes->add(ATTR_STD_PARTICLE);
  }
  if (!output("Random")->links.empty()) {
    attributes->add(ATTR_STD_PARTICLE);
  }
  if (!output("Age")->links.empty()) {
    attributes->add(ATTR_STD_PARTICLE);
  }
  if (!output("Lifetime")->links.empty()) {
    attributes->add(ATTR_STD_PARTICLE);
  }
  if (!output("Location")->links.empty()) {
    attributes->add(ATTR_STD_PARTICLE);
  }
#if 0 /* not yet supported */
  if (!output("Rotation")->links.empty()) {
    attributes->add(ATTR_STD_PARTICLE);
  }
#endif
  if (!output("Size")->links.empty()) {
    attributes->add(ATTR_STD_PARTICLE);
  }
  if (!output("Velocity")->links.empty()) {
    attributes->add(ATTR_STD_PARTICLE);
  }
  if (!output("Angular Velocity")->links.empty()) {
    attributes->add(ATTR_STD_PARTICLE);
  }

  ShaderNode::attributes(shader, attributes);
}

void ParticleInfoNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *out;

  out = output("Index");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_PARTICLE_INFO,
                      SVMNodeParticleInfo{
                          .info_type = NODE_INFO_PAR_INDEX,
                          .out_offset = compiler.output("Index"),
                      });
  }

  out = output("Random");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_PARTICLE_INFO,
                      SVMNodeParticleInfo{
                          .info_type = NODE_INFO_PAR_RANDOM,
                          .out_offset = compiler.output("Random"),
                      });
  }

  out = output("Age");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_PARTICLE_INFO,
                      SVMNodeParticleInfo{
                          .info_type = NODE_INFO_PAR_AGE,
                          .out_offset = compiler.output("Age"),
                      });
  }

  out = output("Lifetime");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_PARTICLE_INFO,
                      SVMNodeParticleInfo{
                          .info_type = NODE_INFO_PAR_LIFETIME,
                          .out_offset = compiler.output("Lifetime"),
                      });
  }

  out = output("Location");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_PARTICLE_INFO,
                      SVMNodeParticleInfo{
                          .info_type = NODE_INFO_PAR_LOCATION,
                          .out_offset = compiler.output("Location"),
                      });
  }

  /* quaternion data is not yet supported by Cycles */
#if 0
  out = output("Rotation");
  if (!out->links.empty()) {
    compiler.add_node(this, NODE_PARTICLE_INFO,
        SVMNodeParticleInfo{
        .info_type = NODE_INFO_PAR_ROTATION,
        .out_offset = compiler.output("Rotation"),
    });
  }
#endif

  out = output("Size");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_PARTICLE_INFO,
                      SVMNodeParticleInfo{
                          .info_type = NODE_INFO_PAR_SIZE,
                          .out_offset = compiler.output("Size"),
                      });
  }

  out = output("Velocity");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_PARTICLE_INFO,
                      SVMNodeParticleInfo{
                          .info_type = NODE_INFO_PAR_VELOCITY,
                          .out_offset = compiler.output("Velocity"),
                      });
  }

  out = output("Angular Velocity");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_PARTICLE_INFO,
                      SVMNodeParticleInfo{
                          .info_type = NODE_INFO_PAR_ANGULAR_VELOCITY,
                          .out_offset = compiler.output("Angular Velocity"),
                      });
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
  SOCKET_OUT_FLOAT(size, "Length");
  SOCKET_OUT_FLOAT(thickness, "Thickness");
  SOCKET_OUT_NORMAL(tangent_normal, "Tangent Normal");
  SOCKET_OUT_FLOAT(index, "Random");

  return type;
}

HairInfoNode::HairInfoNode() : ShaderNode(get_node_type()) {}

void HairInfoNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface_link()) {

    if (!output("Intercept")->links.empty()) {
      attributes->add(ATTR_STD_CURVE_INTERCEPT);
    }

    if (!output("Length")->links.empty()) {
      attributes->add(ATTR_STD_CURVE_LENGTH);
    }

    if (!output("Random")->links.empty()) {
      attributes->add(ATTR_STD_CURVE_RANDOM);
    }
  }

  ShaderNode::attributes(shader, attributes);
}

void HairInfoNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *out;

  out = output("Is Strand");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_HAIR_INFO,
                      SVMNodeHairInfo{
                          .info_type = NODE_INFO_CURVE_IS_STRAND,
                          .out_offset = compiler.output("Is Strand"),
                      });
  }

  out = output("Intercept");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_ATTR,
                      SVMNodeAttr{
                          .attr = int(compiler.attribute(ATTR_STD_CURVE_INTERCEPT)),
                          .out_offset = compiler.output("Intercept"),
                          .output_type = NODE_ATTR_OUTPUT_FLOAT,
                      });
  }

  out = output("Length");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_ATTR,
                      SVMNodeAttr{
                          .attr = int(compiler.attribute(ATTR_STD_CURVE_LENGTH)),
                          .out_offset = compiler.output("Length"),
                          .output_type = NODE_ATTR_OUTPUT_FLOAT,
                      });
  }

  out = output("Thickness");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_HAIR_INFO,
                      SVMNodeHairInfo{
                          .info_type = NODE_INFO_CURVE_THICKNESS,
                          .out_offset = compiler.output("Thickness"),
                      });
  }

  out = output("Tangent Normal");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_HAIR_INFO,
                      SVMNodeHairInfo{
                          .info_type = NODE_INFO_CURVE_TANGENT_NORMAL,
                          .out_offset = compiler.output("Tangent Normal"),
                      });
  }

  out = output("Random");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_ATTR,
                      SVMNodeAttr{
                          .attr = int(compiler.attribute(ATTR_STD_CURVE_RANDOM)),
                          .out_offset = compiler.output("Random"),
                          .output_type = NODE_ATTR_OUTPUT_FLOAT,
                      });
  }
}

void HairInfoNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_hair_info");
}

/* Point Info */

NODE_DEFINE(PointInfoNode)
{
  NodeType *type = NodeType::add("point_info", create, NodeType::SHADER);

  SOCKET_OUT_POINT(position, "Position");
  SOCKET_OUT_FLOAT(radius, "Radius");
  SOCKET_OUT_FLOAT(random, "Random");

  return type;
}

PointInfoNode::PointInfoNode() : ShaderNode(get_node_type()) {}

void PointInfoNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface_link()) {
    if (!output("Random")->links.empty()) {
      attributes->add(ATTR_STD_POINT_RANDOM);
    }
  }

  ShaderNode::attributes(shader, attributes);
}

void PointInfoNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *out;

  out = output("Position");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_POINT_INFO,
                      SVMNodePointInfo{
                          .info_type = NODE_INFO_POINT_POSITION,
                          .out_offset = compiler.output("Position"),
                      });
  }

  out = output("Radius");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_POINT_INFO,
                      SVMNodePointInfo{
                          .info_type = NODE_INFO_POINT_RADIUS,
                          .out_offset = compiler.output("Radius"),
                      });
  }

  out = output("Random");
  if (!out->links.empty()) {
    compiler.add_node(this,
                      NODE_ATTR,
                      SVMNodeAttr{
                          .attr = int(compiler.attribute(ATTR_STD_POINT_RANDOM)),
                          .out_offset = compiler.output("Random"),
                          .output_type = NODE_ATTR_OUTPUT_FLOAT,
                      });
  }
}

void PointInfoNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_point_info");
}

/* Volume Info */

NODE_DEFINE(VolumeInfoNode)
{
  NodeType *type = NodeType::add("volume_info", create, NodeType::SHADER);

  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(density, "Density");
  SOCKET_OUT_FLOAT(flame, "Flame");
  SOCKET_OUT_FLOAT(temperature, "Temperature");

  return type;
}

VolumeInfoNode::VolumeInfoNode() : ShaderNode(get_node_type()) {}

/* The requested attributes are not updated after node expansion.
 * So we explicitly request the required attributes.
 */
void VolumeInfoNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_volume) {
    if (!output("Color")->links.empty()) {
      attributes->add(ATTR_STD_VOLUME_COLOR);
    }
    if (!output("Density")->links.empty()) {
      attributes->add(ATTR_STD_VOLUME_DENSITY);
    }
    if (!output("Flame")->links.empty()) {
      attributes->add(ATTR_STD_VOLUME_FLAME);
    }
    if (!output("Temperature")->links.empty()) {
      attributes->add(ATTR_STD_VOLUME_TEMPERATURE);
    }
    attributes->add(ATTR_STD_GENERATED_TRANSFORM);
  }
  ShaderNode::attributes(shader, attributes);
}

void VolumeInfoNode::expand(ShaderGraph *graph)
{
  ShaderOutput *color_out = output("Color");
  if (!color_out->links.empty()) {
    AttributeNode *attr = graph->create_node<AttributeNode>();
    attr->set_attribute(ustring("color"));
    graph->relink(color_out, attr->output("Color"));
  }

  ShaderOutput *density_out = output("Density");
  if (!density_out->links.empty()) {
    AttributeNode *attr = graph->create_node<AttributeNode>();
    attr->set_attribute(ustring("density"));
    graph->relink(density_out, attr->output("Fac"));
  }

  ShaderOutput *flame_out = output("Flame");
  if (!flame_out->links.empty()) {
    AttributeNode *attr = graph->create_node<AttributeNode>();
    attr->set_attribute(ustring("flame"));
    graph->relink(flame_out, attr->output("Fac"));
  }

  ShaderOutput *temperature_out = output("Temperature");
  if (!temperature_out->links.empty()) {
    AttributeNode *attr = graph->create_node<AttributeNode>();
    attr->set_attribute(ustring("temperature"));
    graph->relink(temperature_out, attr->output("Fac"));
  }
}

void VolumeInfoNode::compile(SVMCompiler & /*compiler*/) {}

void VolumeInfoNode::compile(OSLCompiler & /*compiler*/) {}

NODE_DEFINE(VertexColorNode)
{
  NodeType *type = NodeType::add("vertex_color", create, NodeType::SHADER);

  SOCKET_STRING(layer_name, "Layer Name", ustring());
  SOCKET_OUT_COLOR(color, "Color");
  SOCKET_OUT_FLOAT(alpha, "Alpha");

  return type;
}

VertexColorNode::VertexColorNode() : ShaderNode(get_node_type()) {}

void VertexColorNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (!(output("Color")->links.empty() && output("Alpha")->links.empty())) {
    if (!layer_name.empty()) {
      attributes->add_standard(layer_name);
    }
    else {
      attributes->add(ATTR_STD_VERTEX_COLOR);
    }
  }
  ShaderNode::attributes(shader, attributes);
}

ShaderNodeType VertexColorNode::shader_node_type() const
{
  return NODE_VERTEX_COLOR;
}

void VertexColorNode::compile(SVMCompiler &compiler)
{
  const NodeBumpOffset bump_offset = shader_bump_to_node_bump_offset(bump);
  int layer_id = 0;

  if (!layer_name.empty()) {
    layer_id = compiler.attribute(layer_name);
  }
  else {
    layer_id = compiler.attribute(ATTR_STD_VERTEX_COLOR);
  }

  compiler.add_node(this,
                    NODE_VERTEX_COLOR,
                    SVMNodeVertexColor{
                        .layer_id = uint8_t(layer_id),
                        .color_offset = compiler.output("Color"),
                        .alpha_offset = compiler.output("Alpha"),
                        .bump_offset = bump_offset,
                        .bump_filter_width = bump_filter_width,
                    });
}

void VertexColorNode::compile(OSLCompiler &compiler)
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
  compiler.parameter("bump_filter_width", bump_filter_width);

  if (layer_name.empty()) {
    compiler.parameter("layer_name", ustring("geom:vertex_color"));
  }
  else {
    if (Attribute::name_standard(layer_name.c_str()) != ATTR_STD_NONE) {
      compiler.parameter("name", (string("geom:") + layer_name.c_str()).c_str());
    }
    else {
      compiler.parameter("layer_name", layer_name.c_str());
    }
  }

  compiler.add(this, "node_vertex_color");
}

/* Value */

NODE_DEFINE(ValueNode)
{
  NodeType *type = NodeType::add("value", create, NodeType::SHADER);

  SOCKET_FLOAT(value, "Value", 0.0f);
  SOCKET_OUT_FLOAT(value, "Value");

  return type;
}

ValueNode::ValueNode() : ShaderNode(get_node_type()) {}

void ValueNode::constant_fold(const ConstantFolder &folder)
{
  folder.make_constant(value);
}

void ValueNode::compile(SVMCompiler &compiler)
{
  compiler.add_value_node(this, value, compiler.output("Value"));
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

  SOCKET_COLOR(value, "Value", zero_float3());
  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

ColorNode::ColorNode() : ShaderNode(get_node_type()) {}

void ColorNode::constant_fold(const ConstantFolder &folder)
{
  folder.make_constant(value);
}

void ColorNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *color_out = output("Color");

  if (!color_out->links.empty()) {
    compiler.add_value_node(this, value, compiler.output("Color"));
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

AddClosureNode::AddClosureNode() : ShaderNode(get_node_type())
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

MixClosureNode::MixClosureNode() : ShaderNode(get_node_type())
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
  ShaderInput *closure1_in = input("Closure1");
  ShaderInput *closure2_in = input("Closure2");

  /* remove useless mix closures nodes */
  if (closure1_in->link == closure2_in->link) {
    folder.bypass_or_discard(closure1_in);
  }
  /* remove unused mix closure input when factor is 0.0 or 1.0
   * check for closure links and make sure factor link is disconnected */
  else if (!input("Fac")->link) {
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

MixClosureWeightNode::MixClosureWeightNode() : ShaderNode(get_node_type()) {}

void MixClosureWeightNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_MIX_CLOSURE,
                    SVMNodeMixClosure{
                        .fac = compiler.input_float("Fac"),
                        .in_weight_offset = compiler.input_link("Weight"),
                        .weight1_offset = compiler.output("Weight1"),
                        .weight2_offset = compiler.output("Weight2"),
                    });
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
  SOCKET_IN_COLOR(color, "Color", zero_float3());

  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

InvertNode::InvertNode() : ShaderNode(get_node_type()) {}

void InvertNode::constant_fold(const ConstantFolder &folder)
{
  ShaderInput *color_in = input("Color");

  if (!input("Fac")->link) {
    /* evaluate fully constant node */
    if (!color_in->link) {
      folder.make_constant(interp(color, one_float3() - color, fac));
    }
    /* remove no-op node */
    else if (fac == 0.0f) {
      folder.bypass(color_in->link);
    }
  }
}

void InvertNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_INVERT,
                    SVMNodeInvert{
                        .color = compiler.input_float3("Color"),
                        .fac = compiler.input_float("Fac"),
                        .out_offset = compiler.output("Color"),
                    });
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
  type_enum.insert("color", NODE_MIX_COL);
  type_enum.insert("soft_light", NODE_MIX_SOFT);
  type_enum.insert("linear_light", NODE_MIX_LINEAR);
  type_enum.insert("exclusion", NODE_MIX_EXCLUSION);
  SOCKET_ENUM(mix_type, "Type", type_enum, NODE_MIX_BLEND);

  SOCKET_BOOLEAN(use_clamp, "Use Clamp", false);

  SOCKET_IN_FLOAT(fac, "Fac", 0.5f);
  SOCKET_IN_COLOR(color1, "Color1", zero_float3());
  SOCKET_IN_COLOR(color2, "Color2", zero_float3());

  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

MixNode::MixNode() : ShaderNode(get_node_type()) {}

void MixNode::compile(SVMCompiler &compiler)
{
  const SVMStackOffset color_offset = compiler.output("Color");

  compiler.add_node(this,
                    NODE_MIX,
                    SVMNodeMix{
                        .mix_type = mix_type,
                        .c1 = compiler.input_float3("Color1"),
                        .c2 = compiler.input_float3("Color2"),
                        .fac = compiler.input_float("Fac"),
                        .result_offset = color_offset,
                    });

  if (use_clamp) {
    compiler.add_node(this,
                      NODE_MIX,
                      SVMNodeMix{
                          .mix_type = NODE_MIX_CLAMP,
                          .c1 = compiler.input_float3_from_offset(color_offset),
                          .c2 = SVMInputFloat3{{0}, {0}, {0}},
                          .fac = SVMInputFloat{0},
                          .result_offset = color_offset,
                      });
  }
}

void MixNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "mix_type");
  compiler.parameter(this, "use_clamp");
  compiler.add(this, "node_mix");
}

void MixNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    folder.make_constant_clamp(svm_mix_clamped_factor(mix_type, fac, color1, color2), use_clamp);
  }
  else {
    folder.fold_mix(mix_type, use_clamp);
  }
}

bool MixNode::is_linear_operation()
{
  switch (mix_type) {
    case NODE_MIX_BLEND:
    case NODE_MIX_ADD:
    case NODE_MIX_MUL:
    case NODE_MIX_SUB:
      break;
    default:
      return false;
  }
  return use_clamp == false && input("Factor")->link == nullptr;
}

/* Mix Color */

NODE_DEFINE(MixColorNode)
{
  NodeType *type = NodeType::add("mix_color", create, NodeType::SHADER);

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
  type_enum.insert("color", NODE_MIX_COL);
  type_enum.insert("soft_light", NODE_MIX_SOFT);
  type_enum.insert("linear_light", NODE_MIX_LINEAR);
  type_enum.insert("exclusion", NODE_MIX_EXCLUSION);
  SOCKET_ENUM(blend_type, "Type", type_enum, NODE_MIX_BLEND);

  SOCKET_IN_FLOAT(fac, "Factor", 0.5f);
  SOCKET_IN_COLOR(a, "A", zero_float3());
  SOCKET_IN_COLOR(b, "B", zero_float3());
  SOCKET_BOOLEAN(use_clamp_result, "Use Clamp Result", false);
  SOCKET_BOOLEAN(use_clamp, "Use Clamp", true);

  SOCKET_OUT_COLOR(result, "Result");

  return type;
}

MixColorNode::MixColorNode() : ShaderNode(get_node_type()) {}

void MixColorNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_MIX_COLOR,
                    SVMNodeMixColor{
                        .blend_type = blend_type,
                        .a = compiler.input_float3("A"),
                        .b = compiler.input_float3("B"),
                        .fac = compiler.input_float("Factor"),
                        .use_clamp = use_clamp,
                        .use_clamp_result = use_clamp_result,
                        .result_offset = compiler.output("Result"),
                    });
}

void MixColorNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "blend_type");
  compiler.parameter(this, "use_clamp");
  compiler.parameter(this, "use_clamp_result");
  compiler.add(this, "node_mix_color");
}

void MixColorNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    if (use_clamp) {
      fac = clamp(fac, 0.0f, 1.0f);
    }
    folder.make_constant_clamp(svm_mix(blend_type, fac, a, b), use_clamp_result);
  }
  else {
    folder.fold_mix_color(blend_type, use_clamp, use_clamp_result);
  }
}

bool MixColorNode::is_linear_operation()
{
  switch (blend_type) {
    case NODE_MIX_BLEND:
    case NODE_MIX_ADD:
    case NODE_MIX_MUL:
    case NODE_MIX_SUB:
      break;
    default:
      return false;
  }
  return use_clamp == false && use_clamp_result == false && input("Factor")->link == nullptr;
}

/* Mix Float */

NODE_DEFINE(MixFloatNode)
{
  NodeType *type = NodeType::add("mix_float", create, NodeType::SHADER);

  SOCKET_IN_FLOAT(fac, "Factor", 0.5f);
  SOCKET_IN_FLOAT(a, "A", 0.0f);
  SOCKET_IN_FLOAT(b, "B", 0.0f);
  SOCKET_BOOLEAN(use_clamp, "Use Clamp", true);
  SOCKET_OUT_FLOAT(result, "Result");

  return type;
}

MixFloatNode::MixFloatNode() : ShaderNode(get_node_type()) {}

void MixFloatNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_MIX_FLOAT,
                    SVMNodeMixFloat{
                        .fac = compiler.input_float("Factor"),
                        .a = compiler.input_float("A"),
                        .b = compiler.input_float("B"),
                        .use_clamp = use_clamp,
                        .result_offset = compiler.output("Result"),
                    });
}

void MixFloatNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "use_clamp");
  compiler.add(this, "node_mix_float");
}

void MixFloatNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    if (use_clamp) {
      fac = clamp(fac, 0.0f, 1.0f);
    }
    folder.make_constant(a * (1 - fac) + b * fac);
  }
  else {
    folder.fold_mix_float(use_clamp, false);
  }
}

bool MixFloatNode::is_linear_operation()
{
  return use_clamp == false && input("Factor")->link == nullptr;
}

/* Mix Vector */

NODE_DEFINE(MixVectorNode)
{
  NodeType *type = NodeType::add("mix_vector", create, NodeType::SHADER);

  SOCKET_IN_FLOAT(fac, "Factor", 0.5f);
  SOCKET_IN_VECTOR(a, "A", zero_float3());
  SOCKET_IN_VECTOR(b, "B", zero_float3());
  SOCKET_BOOLEAN(use_clamp, "Use Clamp", true);

  SOCKET_OUT_VECTOR(result, "Result");

  return type;
}

MixVectorNode::MixVectorNode() : ShaderNode(get_node_type()) {}

void MixVectorNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_MIX_VECTOR,
                    SVMNodeMixVector{
                        .a = compiler.input_float3("A"),
                        .b = compiler.input_float3("B"),
                        .fac = compiler.input_float("Factor"),
                        .use_clamp = use_clamp,
                        .result_offset = compiler.output("Result"),
                    });
}

void MixVectorNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "use_clamp");
  compiler.add(this, "node_mix_vector");
}

void MixVectorNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    if (use_clamp) {
      fac = clamp(fac, 0.0f, 1.0f);
    }
    folder.make_constant(a * (one_float3() - fac) + b * fac);
  }
  else {
    folder.fold_mix_color(NODE_MIX_BLEND, use_clamp, false);
  }
}

bool MixVectorNode::is_linear_operation()
{
  return use_clamp == false && input("Factor")->link == nullptr;
}

/* Mix Vector Non Uniform */

NODE_DEFINE(MixVectorNonUniformNode)
{
  NodeType *type = NodeType::add("mix_vector_non_uniform", create, NodeType::SHADER);

  SOCKET_IN_VECTOR(fac, "Factor", make_float3(0.5f, 0.5f, 0.5f));
  SOCKET_IN_VECTOR(a, "A", zero_float3());
  SOCKET_IN_VECTOR(b, "B", zero_float3());
  SOCKET_BOOLEAN(use_clamp, "Use Clamp", true);

  SOCKET_OUT_VECTOR(result, "Result");

  return type;
}

MixVectorNonUniformNode::MixVectorNonUniformNode() : ShaderNode(get_node_type()) {}

void MixVectorNonUniformNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_MIX_VECTOR_NON_UNIFORM,
                    SVMNodeMixVectorNonUniform{
                        .a = compiler.input_float3("A"),
                        .b = compiler.input_float3("B"),
                        .fac = compiler.input_float3("Factor"),
                        .use_clamp = use_clamp,
                        .result_offset = compiler.output("Result"),
                    });
}

void MixVectorNonUniformNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "use_clamp");
  compiler.add(this, "node_mix_vector_non_uniform");
}

void MixVectorNonUniformNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    if (use_clamp) {
      fac = saturate(fac);
    }
    folder.make_constant(a * (one_float3() - fac) + b * fac);
  }
}

bool MixVectorNonUniformNode::is_linear_operation()
{
  return use_clamp == false && input("Factor")->link == nullptr;
}

/* Combine Color */

NODE_DEFINE(CombineColorNode)
{
  NodeType *type = NodeType::add("combine_color", create, NodeType::SHADER);

  static NodeEnum type_enum;
  type_enum.insert("rgb", NODE_COMBSEP_COLOR_RGB);
  type_enum.insert("hsv", NODE_COMBSEP_COLOR_HSV);
  type_enum.insert("hsl", NODE_COMBSEP_COLOR_HSL);
  SOCKET_ENUM(color_type, "Type", type_enum, NODE_COMBSEP_COLOR_RGB);

  SOCKET_IN_FLOAT(r, "Red", 0.0f);
  SOCKET_IN_FLOAT(g, "Green", 0.0f);
  SOCKET_IN_FLOAT(b, "Blue", 0.0f);

  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

CombineColorNode::CombineColorNode() : ShaderNode(get_node_type()) {}

void CombineColorNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    folder.make_constant(svm_combine_color(color_type, make_float3(r, g, b)));
  }
}

void CombineColorNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_COMBINE_COLOR,
                    SVMNodeCombineColor{
                        .color_type = color_type,
                        .red = compiler.input_float("Red"),
                        .green = compiler.input_float("Green"),
                        .blue = compiler.input_float("Blue"),
                        .color_offset = compiler.output("Color"),
                    });
}

void CombineColorNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "color_type");
  compiler.add(this, "node_combine_color");
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

CombineXYZNode::CombineXYZNode() : ShaderNode(get_node_type()) {}

void CombineXYZNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    folder.make_constant(make_float3(x, y, z));
  }
}

void CombineXYZNode::compile(SVMCompiler &compiler)
{
  const SVMStackOffset vector_out = compiler.output("Vector");
  compiler.add_node(this,
                    NODE_COMBINE_VECTOR,
                    SVMNodeCombineVector{
                        .in = compiler.input_float("X"),
                        .vector_index = 0,
                        .out_offset = vector_out,
                    });
  compiler.add_node(this,
                    NODE_COMBINE_VECTOR,
                    SVMNodeCombineVector{
                        .in = compiler.input_float("Y"),
                        .vector_index = 1,
                        .out_offset = vector_out,
                    });
  compiler.add_node(this,
                    NODE_COMBINE_VECTOR,
                    SVMNodeCombineVector{
                        .in = compiler.input_float("Z"),
                        .vector_index = 2,
                        .out_offset = vector_out,
                    });
}

void CombineXYZNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_combine_xyz");
}

/* Gamma */

NODE_DEFINE(GammaNode)
{
  NodeType *type = NodeType::add("gamma", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", zero_float3());
  SOCKET_IN_FLOAT(gamma, "Gamma", 1.0f);
  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

GammaNode::GammaNode() : ShaderNode(get_node_type()) {}

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
  compiler.add_node(this,
                    NODE_GAMMA,
                    SVMNodeGamma{
                        .color = compiler.input_float3("Color"),
                        .gamma = compiler.input_float("Gamma"),
                        .out_offset = compiler.output("Color"),
                    });
}

void GammaNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_gamma");
}

/* Bright Contrast */

NODE_DEFINE(BrightContrastNode)
{
  NodeType *type = NodeType::add("brightness_contrast", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", zero_float3());
  SOCKET_IN_FLOAT(bright, "Bright", 0.0f);
  SOCKET_IN_FLOAT(contrast, "Contrast", 0.0f);

  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

BrightContrastNode::BrightContrastNode() : ShaderNode(get_node_type()) {}

void BrightContrastNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    folder.make_constant(svm_brightness_contrast(color, bright, contrast));
  }
}

void BrightContrastNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_BRIGHTCONTRAST,
                    SVMNodeBrightContrast{
                        .color = compiler.input_float3("Color"),
                        .bright = compiler.input_float("Bright"),
                        .contrast = compiler.input_float("Contrast"),
                        .out_offset = compiler.output("Color"),
                    });
}

void BrightContrastNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_brightness");
}

/* Separate Color */

NODE_DEFINE(SeparateColorNode)
{
  NodeType *type = NodeType::add("separate_color", create, NodeType::SHADER);

  static NodeEnum type_enum;
  type_enum.insert("rgb", NODE_COMBSEP_COLOR_RGB);
  type_enum.insert("hsv", NODE_COMBSEP_COLOR_HSV);
  type_enum.insert("hsl", NODE_COMBSEP_COLOR_HSL);
  SOCKET_ENUM(color_type, "Type", type_enum, NODE_COMBSEP_COLOR_RGB);

  SOCKET_IN_COLOR(color, "Color", zero_float3());

  SOCKET_OUT_FLOAT(r, "Red");
  SOCKET_OUT_FLOAT(g, "Green");
  SOCKET_OUT_FLOAT(b, "Blue");

  return type;
}

SeparateColorNode::SeparateColorNode() : ShaderNode(get_node_type()) {}

void SeparateColorNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    float3 col = svm_separate_color(color_type, color);

    for (int channel = 0; channel < 3; channel++) {
      if (outputs[channel] == folder.output) {
        folder.make_constant(col[channel]);
        return;
      }
    }
  }
}

void SeparateColorNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_SEPARATE_COLOR,
                    SVMNodeSeparateColor{
                        .color_type = color_type,
                        .color = compiler.input_float3("Color"),
                        .red_offset = compiler.output("Red"),
                        .green_offset = compiler.output("Green"),
                        .blue_offset = compiler.output("Blue"),
                    });
}

void SeparateColorNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "color_type");
  compiler.add(this, "node_separate_color");
}

/* Separate XYZ */

NODE_DEFINE(SeparateXYZNode)
{
  NodeType *type = NodeType::add("separate_xyz", create, NodeType::SHADER);

  SOCKET_IN_COLOR(vector, "Vector", zero_float3());

  SOCKET_OUT_FLOAT(x, "X");
  SOCKET_OUT_FLOAT(y, "Y");
  SOCKET_OUT_FLOAT(z, "Z");

  return type;
}

SeparateXYZNode::SeparateXYZNode() : ShaderNode(get_node_type()) {}

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
  const SVMInputFloat3 vector_in = compiler.input_float3("Vector");
  compiler.add_node(this,
                    NODE_SEPARATE_VECTOR,
                    SVMNodeSeparateVector{
                        .vector = vector_in,
                        .vector_index = 0,
                        .out_offset = compiler.output("X"),
                    });
  compiler.add_node(this,
                    NODE_SEPARATE_VECTOR,
                    SVMNodeSeparateVector{
                        .vector = vector_in,
                        .vector_index = 1,
                        .out_offset = compiler.output("Y"),
                    });
  compiler.add_node(this,
                    NODE_SEPARATE_VECTOR,
                    SVMNodeSeparateVector{
                        .vector = vector_in,
                        .vector_index = 2,
                        .out_offset = compiler.output("Z"),
                    });
}

void SeparateXYZNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_separate_xyz");
}

/* Hue/Saturation/Value */

NODE_DEFINE(HSVNode)
{
  NodeType *type = NodeType::add("hsv", create, NodeType::SHADER);

  SOCKET_IN_FLOAT(hue, "Hue", 0.5f);
  SOCKET_IN_FLOAT(saturation, "Saturation", 1.0f);
  SOCKET_IN_FLOAT(value, "Value", 1.0f);
  SOCKET_IN_FLOAT(fac, "Fac", 1.0f);
  SOCKET_IN_COLOR(color, "Color", zero_float3());

  SOCKET_OUT_COLOR(color, "Color");

  return type;
}

HSVNode::HSVNode() : ShaderNode(get_node_type()) {}

void HSVNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_HSV,
                    SVMNodeHSV{
                        .color = compiler.input_float3("Color"),
                        .hue = compiler.input_float("Hue"),
                        .sat = compiler.input_float("Saturation"),
                        .val = compiler.input_float("Value"),
                        .fac = compiler.input_float("Fac"),
                        .out_color_offset = compiler.output("Color"),
                    });
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
  SOCKET_OUT_FLOAT(alpha, "Alpha");

  return type;
}

AttributeNode::AttributeNode() : ShaderNode(get_node_type()) {}

void AttributeNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{

  if (!output("Color")->links.empty() || !output("Vector")->links.empty() ||
      !output("Fac")->links.empty() || !output("Alpha")->links.empty())
  {
    add_named_attribute_request(attributes, attribute);
  }

  if (shader->has_volume) {
    attributes->add(ATTR_STD_GENERATED_TRANSFORM);
  }

  ShaderNode::attributes(shader, attributes);
}

void AttributeNode::add_named_attribute_request(AttributeRequestSet *attributes,
                                                const ustring attribute)
{
  attributes->add_standard(attribute);

  /* Request UV if we asked for one of the attributes computed from it.
   * Ideally, this would be handled at a more generic level. */
  const AttributeStandard std = Attribute::name_standard(attribute.c_str());
  if (std == ATTR_STD_UV_TANGENT || std == ATTR_STD_UV_TANGENT_SIGN ||
      std == ATTR_STD_UV_TANGENT_UNDISPLACED || std == ATTR_STD_UV_TANGENT_SIGN_UNDISPLACED)
  {
    attributes->add(ATTR_STD_UV);
  }
  else {
    const char *suffixes[] = {
        ".tangent_sign", ".tangent", ".undisplaced_tangent", ".undisplaced_tangent_sign"};
    for (const char *suffix : suffixes) {
      if (string_endswith(attribute, suffix)) {
        attributes->add(attribute.substr(0, attribute.size() - strlen(suffix)));
      }
    }
  }
}

ShaderNodeType AttributeNode::shader_node_type() const
{
  return NODE_ATTR;
}

void AttributeNode::compile(SVMCompiler &compiler)
{
  const NodeBumpOffset bump_offset = shader_bump_to_node_bump_offset(bump);
  const bool use_derivative = need_derivatives() || (bump != SHADER_BUMP_NONE);
  const bool store_derivatives = need_derivatives();
  ShaderOutput *color_out = output("Color");
  ShaderOutput *vector_out = output("Vector");
  ShaderOutput *fac_out = output("Fac");
  ShaderOutput *alpha_out = output("Alpha");
  const int attr = compiler.attribute_standard(attribute);
  const float bump_filter_or_stochastic = (compiler.output_type() == SHADER_TYPE_VOLUME) ?
                                              __uint_as_float(uint(stochastic_sample)) :
                                              bump_filter_width;

  if (!color_out->links.empty() || !vector_out->links.empty()) {
    if (!color_out->links.empty()) {
      compiler.add_node(this,
                        NODE_ATTR,
                        SVMNodeAttr{
                            .attr = attr,
                            .out_offset = compiler.output("Color"),
                            .output_type = NODE_ATTR_OUTPUT_FLOAT3,
                            .bump_offset = bump_offset,
                            .store_derivatives = store_derivatives,
                            .bump_filter_width = bump_filter_or_stochastic,
                        },
                        use_derivative);
    }
    if (!vector_out->links.empty()) {
      compiler.add_node(this,
                        NODE_ATTR,
                        SVMNodeAttr{
                            .attr = attr,
                            .out_offset = compiler.output("Vector"),
                            .output_type = NODE_ATTR_OUTPUT_FLOAT3,
                            .bump_offset = bump_offset,
                            .store_derivatives = store_derivatives,
                            .bump_filter_width = bump_filter_or_stochastic,
                        },
                        use_derivative);
    }
  }

  if (!fac_out->links.empty()) {
    compiler.add_node(this,
                      NODE_ATTR,
                      SVMNodeAttr{
                          .attr = attr,
                          .out_offset = compiler.output("Fac"),
                          .output_type = NODE_ATTR_OUTPUT_FLOAT,
                          .bump_offset = bump_offset,
                          .store_derivatives = store_derivatives,
                          .bump_filter_width = bump_filter_or_stochastic,
                      },
                      use_derivative);
  }

  if (!alpha_out->links.empty()) {
    compiler.add_node(this,
                      NODE_ATTR,
                      SVMNodeAttr{
                          .attr = attr,
                          .out_offset = compiler.output("Alpha"),
                          .output_type = NODE_ATTR_OUTPUT_FLOAT_ALPHA,
                          .bump_offset = bump_offset,
                          .store_derivatives = store_derivatives,
                          .bump_filter_width = bump_filter_or_stochastic,
                      },
                      use_derivative);
  }
}

void AttributeNode::compile(OSLCompiler &compiler)
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
  compiler.parameter("bump_filter_width", bump_filter_width);

  if (Attribute::name_standard(attribute.c_str()) != ATTR_STD_NONE) {
    compiler.parameter("name", (string("geom:") + attribute.c_str()).c_str());
  }
  else {
    compiler.parameter("name", attribute.c_str());
  }

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

CameraNode::CameraNode() : ShaderNode(get_node_type()) {}

void CameraNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_CAMERA,
                    SVMNodeCamera{
                        .vector_offset = compiler.output("View Vector"),
                        .zdepth_offset = compiler.output("View Z Depth"),
                        .distance_offset = compiler.output("View Distance"),
                    });
}

void CameraNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_camera");
}

/* Fresnel */

NODE_DEFINE(FresnelNode)
{
  NodeType *type = NodeType::add("fresnel", create, NodeType::SHADER);

  SOCKET_IN_NORMAL(
      normal, "Normal", zero_float3(), SocketType::LINK_NORMAL | SocketType::OSL_INTERNAL);
  SOCKET_IN_FLOAT(IOR, "IOR", 1.5f);

  SOCKET_OUT_FLOAT(fac, "Fac");

  return type;
}

FresnelNode::FresnelNode() : ShaderNode(get_node_type()) {}

void FresnelNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_FRESNEL,
                    SVMNodeFresnel{
                        .ior = compiler.input_float("IOR"),
                        .normal_offset = compiler.input_link("Normal"),
                        .out_offset = compiler.output("Fac"),
                    });
}

void FresnelNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_fresnel");
}

/* Layer Weight */

NODE_DEFINE(LayerWeightNode)
{
  NodeType *type = NodeType::add("layer_weight", create, NodeType::SHADER);

  SOCKET_IN_NORMAL(
      normal, "Normal", zero_float3(), SocketType::LINK_NORMAL | SocketType::OSL_INTERNAL);
  SOCKET_IN_FLOAT(blend, "Blend", 0.5f);

  SOCKET_OUT_FLOAT(fresnel, "Fresnel");
  SOCKET_OUT_FLOAT(facing, "Facing");

  return type;
}

LayerWeightNode::LayerWeightNode() : ShaderNode(get_node_type()) {}

void LayerWeightNode::compile(SVMCompiler &compiler)
{
  ShaderOutput *fresnel_out = output("Fresnel");
  ShaderOutput *facing_out = output("Facing");

  if (!fresnel_out->links.empty()) {
    compiler.add_node(this,
                      NODE_LAYER_WEIGHT,
                      SVMNodeLayerWeight{
                          .weight_type = NODE_LAYER_WEIGHT_FRESNEL,
                          .blend = compiler.input_float("Blend"),
                          .normal_offset = compiler.input_link("Normal"),
                          .out_offset = compiler.output("Fresnel"),
                      });
  }

  if (!facing_out->links.empty()) {
    compiler.add_node(this,
                      NODE_LAYER_WEIGHT,
                      SVMNodeLayerWeight{
                          .weight_type = NODE_LAYER_WEIGHT_FACING,
                          .blend = compiler.input_float("Blend"),
                          .normal_offset = compiler.input_link("Normal"),
                          .out_offset = compiler.output("Facing"),
                      });
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

WireframeNode::WireframeNode() : ShaderNode(get_node_type()) {}

void WireframeNode::compile(SVMCompiler &compiler)
{
  NodeBumpOffset bump_offset = NODE_BUMP_OFFSET_CENTER;
  if (bump == SHADER_BUMP_DX) {
    bump_offset = NODE_BUMP_OFFSET_DX;
  }
  else if (bump == SHADER_BUMP_DY) {
    bump_offset = NODE_BUMP_OFFSET_DY;
  }
  compiler.add_node(this,
                    NODE_WIREFRAME,
                    SVMNodeWireframe{
                        .in_size = compiler.input_float("Size"),
                        .bump_filter_width = bump_filter_width,
                        .use_pixel_size = use_pixel_size,
                        .bump_offset = bump_offset,
                        .out_fac_offset = compiler.output("Fac"),
                    });
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
  compiler.parameter("bump_filter_width", bump_filter_width);

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

WavelengthNode::WavelengthNode() : ShaderNode(get_node_type()) {}

void WavelengthNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_WAVELENGTH,
                    SVMNodeWavelength{
                        .wavelength = compiler.input_float("Wavelength"),
                        .color_offset = compiler.output("Color"),
                    });
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

BlackbodyNode::BlackbodyNode() : ShaderNode(get_node_type()) {}

void BlackbodyNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    const float3 rgb_rec709 = svm_math_blackbody_color_rec709(temperature);
    const float3 rgb = folder.scene->shader_manager->rec709_to_scene_linear(rgb_rec709);
    folder.make_constant(max(rgb, zero_float3()));
  }
}

void BlackbodyNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_BLACKBODY,
                    SVMNodeBlackbody{
                        .temperature = compiler.input_float("Temperature"),
                        .color_offset = compiler.output("Color"),
                    });
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
  SOCKET_IN_VECTOR(displacement, "Displacement", zero_float3());
  SOCKET_IN_NORMAL(normal, "Normal", zero_float3());

  return type;
}

OutputNode::OutputNode() : ShaderNode(get_node_type())
{
  special_type = SHADER_SPECIAL_TYPE_OUTPUT;
}

void OutputNode::compile(SVMCompiler &compiler)
{
  if (compiler.output_type() == SHADER_TYPE_DISPLACEMENT) {
    ShaderInput *displacement_in = input("Displacement");

    if (displacement_in->link) {
      compiler.add_node(this,
                        NODE_SET_DISPLACEMENT,
                        SVMNodeSetDisplacement{.fac_offset = compiler.input_link("Displacement")});
    }
  }
}

void OutputNode::compile(OSLCompiler &compiler)
{
  if (compiler.output_type() == SHADER_TYPE_SURFACE) {
    compiler.add(this, "node_output_surface");
  }
  else if (compiler.output_type() == SHADER_TYPE_VOLUME) {
    compiler.add(this, "node_output_volume");
  }
  else if (compiler.output_type() == SHADER_TYPE_DISPLACEMENT) {
    compiler.add(this, "node_output_displacement");
  }
}

/* Map Range Node */

NODE_DEFINE(MapRangeNode)
{
  NodeType *type = NodeType::add("map_range", create, NodeType::SHADER);

  static NodeEnum type_enum;
  type_enum.insert("linear", NODE_MAP_RANGE_LINEAR);
  type_enum.insert("stepped", NODE_MAP_RANGE_STEPPED);
  type_enum.insert("smoothstep", NODE_MAP_RANGE_SMOOTHSTEP);
  type_enum.insert("smootherstep", NODE_MAP_RANGE_SMOOTHERSTEP);
  SOCKET_ENUM(range_type, "Type", type_enum, NODE_MAP_RANGE_LINEAR);

  SOCKET_IN_FLOAT(value, "Value", 1.0f);
  SOCKET_IN_FLOAT(from_min, "From Min", 0.0f);
  SOCKET_IN_FLOAT(from_max, "From Max", 1.0f);
  SOCKET_IN_FLOAT(to_min, "To Min", 0.0f);
  SOCKET_IN_FLOAT(to_max, "To Max", 1.0f);
  SOCKET_IN_FLOAT(steps, "Steps", 4.0f);
  SOCKET_IN_BOOLEAN(clamp, "Clamp", false);

  SOCKET_OUT_FLOAT(result, "Result");

  return type;
}

MapRangeNode::MapRangeNode() : ShaderNode(get_node_type()) {}

void MapRangeNode::expand(ShaderGraph *graph)
{
  if (clamp) {
    ShaderOutput *result_out = output("Result");
    if (!result_out->links.empty()) {
      ClampNode *clamp_node = graph->create_node<ClampNode>();
      clamp_node->set_clamp_type(NODE_CLAMP_RANGE);
      graph->relink(result_out, clamp_node->output("Result"));
      graph->connect(result_out, clamp_node->input("Value"));
      if (input("To Min")->link) {
        graph->connect(input("To Min")->link, clamp_node->input("Min"));
      }
      else {
        clamp_node->set_min(to_min);
      }
      if (input("To Max")->link) {
        graph->connect(input("To Max")->link, clamp_node->input("Max"));
      }
      else {
        clamp_node->set_max(to_max);
      }
    }
  }
}

bool MapRangeNode::is_linear_operation()
{
  if (range_type != NODE_MAP_RANGE_LINEAR) {
    return false;
  }
  return input("To Min")->link == nullptr && input("To Max")->link == nullptr &&
         input("To Min")->link == nullptr && input("To Max")->link == nullptr;
}

void MapRangeNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_MAP_RANGE,
                    SVMNodeMapRange{
                        .range_type = range_type,
                        .value = compiler.input_float("Value"),
                        .from_min = compiler.input_float("From Min"),
                        .from_max = compiler.input_float("From Max"),
                        .to_min = compiler.input_float("To Min"),
                        .to_max = compiler.input_float("To Max"),
                        .steps = compiler.input_float("Steps"),
                        .result_offset = compiler.output("Result"),
                    });
}

void MapRangeNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "range_type");
  compiler.add(this, "node_map_range");
}

/* Vector Map Range Node */

NODE_DEFINE(VectorMapRangeNode)
{
  NodeType *type = NodeType::add("vector_map_range", create, NodeType::SHADER);

  static NodeEnum type_enum;
  type_enum.insert("linear", NODE_MAP_RANGE_LINEAR);
  type_enum.insert("stepped", NODE_MAP_RANGE_STEPPED);
  type_enum.insert("smoothstep", NODE_MAP_RANGE_SMOOTHSTEP);
  type_enum.insert("smootherstep", NODE_MAP_RANGE_SMOOTHERSTEP);
  SOCKET_ENUM(range_type, "Type", type_enum, NODE_MAP_RANGE_LINEAR);

  SOCKET_IN_VECTOR(vector, "Vector", zero_float3());
  SOCKET_IN_VECTOR(from_min, "From_Min_FLOAT3", zero_float3());
  SOCKET_IN_VECTOR(from_max, "From_Max_FLOAT3", one_float3());
  SOCKET_IN_VECTOR(to_min, "To_Min_FLOAT3", zero_float3());
  SOCKET_IN_VECTOR(to_max, "To_Max_FLOAT3", one_float3());
  SOCKET_IN_VECTOR(steps, "Steps_FLOAT3", make_float3(4.0f));
  SOCKET_BOOLEAN(use_clamp, "Use Clamp", false);

  SOCKET_OUT_VECTOR(vector, "Vector");

  return type;
}

VectorMapRangeNode::VectorMapRangeNode() : ShaderNode(get_node_type()) {}

void VectorMapRangeNode::expand(ShaderGraph * /*graph*/) {}

bool VectorMapRangeNode::is_linear_operation()
{
  if (range_type != NODE_MAP_RANGE_LINEAR) {
    return false;
  }
  return input("From_Min_FLOAT3")->link == nullptr && input("From_Max_FLOAT3")->link == nullptr &&
         input("To_Min_FLOAT3")->link == nullptr && input("To_Max_FLOAT3")->link == nullptr;
}

void VectorMapRangeNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_VECTOR_MAP_RANGE,
                    SVMNodeVectorMapRange{
                        .range_type = range_type,
                        .use_clamp = use_clamp,
                        .value = compiler.input_float3("Vector"),
                        .from_min = compiler.input_float3("From_Min_FLOAT3"),
                        .from_max = compiler.input_float3("From_Max_FLOAT3"),
                        .to_min = compiler.input_float3("To_Min_FLOAT3"),
                        .to_max = compiler.input_float3("To_Max_FLOAT3"),
                        .steps = compiler.input_float3("Steps_FLOAT3"),
                        .result_offset = compiler.output("Vector"),
                    });
}

void VectorMapRangeNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "range_type");
  compiler.parameter(this, "use_clamp");
  compiler.add(this, "node_vector_map_range");
}

/* Clamp Node */

NODE_DEFINE(ClampNode)
{
  NodeType *type = NodeType::add("clamp", create, NodeType::SHADER);

  static NodeEnum type_enum;
  type_enum.insert("minmax", NODE_CLAMP_MINMAX);
  type_enum.insert("range", NODE_CLAMP_RANGE);
  SOCKET_ENUM(clamp_type, "Type", type_enum, NODE_CLAMP_MINMAX);

  SOCKET_IN_FLOAT(value, "Value", 1.0f);
  SOCKET_IN_FLOAT(min, "Min", 0.0f);
  SOCKET_IN_FLOAT(max, "Max", 1.0f);

  SOCKET_OUT_FLOAT(result, "Result");

  return type;
}

ClampNode::ClampNode() : ShaderNode(get_node_type()) {}

void ClampNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    if (clamp_type == NODE_CLAMP_RANGE && (min > max)) {
      folder.make_constant(clamp(value, max, min));
    }
    else {
      folder.make_constant(clamp(value, min, max));
    }
  }
}

void ClampNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_CLAMP,
                    SVMNodeClamp{
                        .clamp_type = clamp_type,
                        .min = compiler.input_float("Min"),
                        .max = compiler.input_float("Max"),
                        .value = compiler.input_float("Value"),
                        .result_offset = compiler.output("Result"),
                    });
}

void ClampNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "clamp_type");
  compiler.add(this, "node_clamp");
}

/* AOV Output */

NODE_DEFINE(OutputAOVNode)
{
  NodeType *type = NodeType::add("aov_output", create, NodeType::SHADER);

  SOCKET_IN_COLOR(color, "Color", zero_float3());
  SOCKET_IN_FLOAT(value, "Value", 0.0f);

  SOCKET_STRING(name, "AOV Name", ustring(""));

  return type;
}

OutputAOVNode::OutputAOVNode() : ShaderNode(get_node_type())
{
  special_type = SHADER_SPECIAL_TYPE_OUTPUT_AOV;
  offset = -1;
}

void OutputAOVNode::simplify_settings(Scene *scene)
{
  offset = scene->film->get_aov_offset(scene, name.string(), is_color);

  if (offset == -1 || is_color) {
    input("Value")->disconnect();
  }
  if (offset == -1 || !is_color) {
    input("Color")->disconnect();
  }
}

void OutputAOVNode::compile(SVMCompiler &compiler)
{
  assert(offset >= 0);

  if (is_color) {
    compiler.add_node(this,
                      NODE_AOV_COLOR,
                      SVMNodeAOVColor{
                          .aov_offset = offset,
                          .color = compiler.input_float3("Color"),
                      });
  }
  else {
    compiler.add_node(this,
                      NODE_AOV_VALUE,
                      SVMNodeAOVValue{
                          .aov_offset = offset,
                          .value = compiler.input_float("Value"),
                      });
  }
}

void OutputAOVNode::compile(OSLCompiler & /*compiler*/)
{
  /* TODO */
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
  type_enum.insert("multiply_add", NODE_MATH_MULTIPLY_ADD);
  type_enum.insert("sine", NODE_MATH_SINE);
  type_enum.insert("cosine", NODE_MATH_COSINE);
  type_enum.insert("tangent", NODE_MATH_TANGENT);
  type_enum.insert("sinh", NODE_MATH_SINH);
  type_enum.insert("cosh", NODE_MATH_COSH);
  type_enum.insert("tanh", NODE_MATH_TANH);
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
  type_enum.insert("floored_modulo", NODE_MATH_FLOORED_MODULO);
  type_enum.insert("absolute", NODE_MATH_ABSOLUTE);
  type_enum.insert("arctan2", NODE_MATH_ARCTAN2);
  type_enum.insert("floor", NODE_MATH_FLOOR);
  type_enum.insert("ceil", NODE_MATH_CEIL);
  type_enum.insert("fraction", NODE_MATH_FRACTION);
  type_enum.insert("trunc", NODE_MATH_TRUNC);
  type_enum.insert("snap", NODE_MATH_SNAP);
  type_enum.insert("wrap", NODE_MATH_WRAP);
  type_enum.insert("pingpong", NODE_MATH_PINGPONG);
  type_enum.insert("sqrt", NODE_MATH_SQRT);
  type_enum.insert("inversesqrt", NODE_MATH_INV_SQRT);
  type_enum.insert("sign", NODE_MATH_SIGN);
  type_enum.insert("exponent", NODE_MATH_EXPONENT);
  type_enum.insert("radians", NODE_MATH_RADIANS);
  type_enum.insert("degrees", NODE_MATH_DEGREES);
  type_enum.insert("smoothmin", NODE_MATH_SMOOTH_MIN);
  type_enum.insert("smoothmax", NODE_MATH_SMOOTH_MAX);
  type_enum.insert("compare", NODE_MATH_COMPARE);
  SOCKET_ENUM(math_type, "Type", type_enum, NODE_MATH_ADD);

  SOCKET_BOOLEAN(use_clamp, "Use Clamp", false);

  SOCKET_IN_FLOAT(value1, "Value1", 0.5f);
  SOCKET_IN_FLOAT(value2, "Value2", 0.5f);
  SOCKET_IN_FLOAT(value3, "Value3", 0.0f);

  SOCKET_OUT_FLOAT(value, "Value");

  return type;
}

MathNode::MathNode() : ShaderNode(get_node_type()) {}

void MathNode::expand(ShaderGraph *graph)
{
  if (use_clamp) {
    ShaderOutput *result_out = output("Value");
    if (!result_out->links.empty()) {
      ClampNode *clamp_node = graph->create_node<ClampNode>();
      clamp_node->set_clamp_type(NODE_CLAMP_MINMAX);
      clamp_node->set_min(0.0f);
      clamp_node->set_max(1.0f);
      graph->relink(result_out, clamp_node->output("Result"));
      graph->connect(result_out, clamp_node->input("Value"));
    }
  }
}

void MathNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    folder.make_constant(svm_math(math_type, value1, value2, value3));
  }
  else {
    folder.fold_math(math_type);
  }
}

bool MathNode::is_linear_operation()
{
  switch (math_type) {
    case NODE_MATH_ADD:
    case NODE_MATH_SUBTRACT:
    case NODE_MATH_MULTIPLY:
    case NODE_MATH_MULTIPLY_ADD:
      break;
    case NODE_MATH_DIVIDE:
      return input("Value2")->link == nullptr;
    default:
      return false;
  }

  int num_variable_inputs = 0;
  for (ShaderInput *input : inputs) {
    num_variable_inputs += (input->link) ? 1 : 0;
  }
  return num_variable_inputs <= 1;
}

void MathNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_MATH,
                    SVMNodeMath{
                        .math_type = math_type,
                        .value1 = compiler.input_float("Value1"),
                        .value2 = compiler.input_float("Value2"),
                        .value3 = compiler.input_float("Value3"),
                        .result_offset = compiler.output("Value"),
                    });
}

void MathNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "math_type");
  compiler.add(this, "node_math");
}

/* VectorMath */

NODE_DEFINE(VectorMathNode)
{
  NodeType *type = NodeType::add("vector_math", create, NodeType::SHADER);

  static NodeEnum type_enum;
  type_enum.insert("add", NODE_VECTOR_MATH_ADD);
  type_enum.insert("subtract", NODE_VECTOR_MATH_SUBTRACT);
  type_enum.insert("multiply", NODE_VECTOR_MATH_MULTIPLY);
  type_enum.insert("divide", NODE_VECTOR_MATH_DIVIDE);

  type_enum.insert("cross_product", NODE_VECTOR_MATH_CROSS_PRODUCT);
  type_enum.insert("project", NODE_VECTOR_MATH_PROJECT);
  type_enum.insert("reflect", NODE_VECTOR_MATH_REFLECT);
  type_enum.insert("refract", NODE_VECTOR_MATH_REFRACT);
  type_enum.insert("faceforward", NODE_VECTOR_MATH_FACEFORWARD);
  type_enum.insert("multiply_add", NODE_VECTOR_MATH_MULTIPLY_ADD);

  type_enum.insert("dot_product", NODE_VECTOR_MATH_DOT_PRODUCT);

  type_enum.insert("distance", NODE_VECTOR_MATH_DISTANCE);
  type_enum.insert("length", NODE_VECTOR_MATH_LENGTH);
  type_enum.insert("scale", NODE_VECTOR_MATH_SCALE);
  type_enum.insert("normalize", NODE_VECTOR_MATH_NORMALIZE);

  type_enum.insert("snap", NODE_VECTOR_MATH_SNAP);
  type_enum.insert("round", NODE_VECTOR_MATH_ROUND);
  type_enum.insert("floor", NODE_VECTOR_MATH_FLOOR);
  type_enum.insert("ceil", NODE_VECTOR_MATH_CEIL);
  type_enum.insert("modulo", NODE_VECTOR_MATH_MODULO);
  type_enum.insert("wrap", NODE_VECTOR_MATH_WRAP);
  type_enum.insert("fraction", NODE_VECTOR_MATH_FRACTION);
  type_enum.insert("absolute", NODE_VECTOR_MATH_ABSOLUTE);
  type_enum.insert("power", NODE_VECTOR_MATH_POWER);
  type_enum.insert("sign", NODE_VECTOR_MATH_SIGN);
  type_enum.insert("minimum", NODE_VECTOR_MATH_MINIMUM);
  type_enum.insert("maximum", NODE_VECTOR_MATH_MAXIMUM);

  type_enum.insert("sine", NODE_VECTOR_MATH_SINE);
  type_enum.insert("cosine", NODE_VECTOR_MATH_COSINE);
  type_enum.insert("tangent", NODE_VECTOR_MATH_TANGENT);
  SOCKET_ENUM(math_type, "Type", type_enum, NODE_VECTOR_MATH_ADD);

  SOCKET_IN_VECTOR(vector1, "Vector1", zero_float3());
  SOCKET_IN_VECTOR(vector2, "Vector2", zero_float3());
  SOCKET_IN_VECTOR(vector3, "Vector3", zero_float3());
  SOCKET_IN_FLOAT(scale, "Scale", 1.0f);

  SOCKET_OUT_FLOAT(value, "Value");
  SOCKET_OUT_VECTOR(vector, "Vector");

  return type;
}

VectorMathNode::VectorMathNode() : ShaderNode(get_node_type()) {}

void VectorMathNode::constant_fold(const ConstantFolder &folder)
{
  float value = 0.0f;
  float3 vector = zero_float3();

  if (folder.all_inputs_constant()) {
    svm_vector_math(&value, &vector, math_type, vector1, vector2, vector3, scale);
    if (folder.output == output("Value")) {
      folder.make_constant(value);
    }
    else if (folder.output == output("Vector")) {
      folder.make_constant(vector);
    }
  }
  else {
    folder.fold_vector_math(math_type);
  }
}

bool VectorMathNode::is_linear_operation()
{
  switch (math_type) {
    case NODE_VECTOR_MATH_ADD:
    case NODE_VECTOR_MATH_SUBTRACT:
    case NODE_VECTOR_MATH_MULTIPLY:
    case NODE_VECTOR_MATH_MULTIPLY_ADD:
      break;
    case NODE_VECTOR_MATH_DIVIDE:
      return input("Vector2")->link == nullptr;
    default:
      return false;
  }

  int num_variable_inputs = 0;
  for (ShaderInput *input : inputs) {
    num_variable_inputs += (input->link) ? 1 : 0;
  }
  return num_variable_inputs <= 1;
}

void VectorMathNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_VECTOR_MATH,
                    SVMNodeVectorMath{
                        .math_type = math_type,
                        .a = compiler.input_float3("Vector1"),
                        .b = compiler.input_float3("Vector2"),
                        .c = compiler.input_float3("Vector3"),
                        .param1 = compiler.input_float("Scale"),
                        .value_offset = compiler.output("Value"),
                        .vector_offset = compiler.output("Vector"),
                    });
}

void VectorMathNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "math_type");
  compiler.add(this, "node_vector_math");
}

/* Vector Rotate */

NODE_DEFINE(VectorRotateNode)
{
  NodeType *type = NodeType::add("vector_rotate", create, NodeType::SHADER);

  static NodeEnum type_enum;
  type_enum.insert("axis", NODE_VECTOR_ROTATE_TYPE_AXIS);
  type_enum.insert("x_axis", NODE_VECTOR_ROTATE_TYPE_AXIS_X);
  type_enum.insert("y_axis", NODE_VECTOR_ROTATE_TYPE_AXIS_Y);
  type_enum.insert("z_axis", NODE_VECTOR_ROTATE_TYPE_AXIS_Z);
  type_enum.insert("euler_xyz", NODE_VECTOR_ROTATE_TYPE_EULER_XYZ);
  SOCKET_ENUM(rotate_type, "Type", type_enum, NODE_VECTOR_ROTATE_TYPE_AXIS);

  SOCKET_BOOLEAN(invert, "Invert", false);

  SOCKET_IN_VECTOR(vector, "Vector", zero_float3());
  SOCKET_IN_POINT(rotation, "Rotation", zero_float3());
  SOCKET_IN_POINT(center, "Center", zero_float3());
  SOCKET_IN_VECTOR(axis, "Axis", make_float3(0.0f, 0.0f, 1.0f));
  SOCKET_IN_FLOAT(angle, "Angle", 0.0f);
  SOCKET_OUT_VECTOR(vector, "Vector");

  return type;
}

VectorRotateNode::VectorRotateNode() : ShaderNode(get_node_type()) {}

void VectorRotateNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_VECTOR_ROTATE,
                    SVMNodeVectorRotate{
                        .rotate_type = rotate_type,
                        .vector = compiler.input_float3("Vector"),
                        .center = compiler.input_float3("Center"),
                        .axis = compiler.input_float3("Axis"),
                        .rotation = compiler.input_float3("Rotation"),
                        .angle = compiler.input_float("Angle"),
                        .invert = invert,
                        .result_offset = compiler.output("Vector"),
                    });
}

void VectorRotateNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "rotate_type");
  compiler.parameter(this, "invert");
  compiler.add(this, "node_vector_rotate");
}

/* VectorTransform */

NODE_DEFINE(VectorTransformNode)
{
  NodeType *type = NodeType::add("vector_transform", create, NodeType::SHADER);

  static NodeEnum type_enum;
  type_enum.insert("vector", NODE_VECTOR_TRANSFORM_TYPE_VECTOR);
  type_enum.insert("point", NODE_VECTOR_TRANSFORM_TYPE_POINT);
  type_enum.insert("normal", NODE_VECTOR_TRANSFORM_TYPE_NORMAL);
  SOCKET_ENUM(transform_type, "Type", type_enum, NODE_VECTOR_TRANSFORM_TYPE_VECTOR);

  static NodeEnum space_enum;
  space_enum.insert("world", NODE_VECTOR_TRANSFORM_CONVERT_SPACE_WORLD);
  space_enum.insert("object", NODE_VECTOR_TRANSFORM_CONVERT_SPACE_OBJECT);
  space_enum.insert("camera", NODE_VECTOR_TRANSFORM_CONVERT_SPACE_CAMERA);
  SOCKET_ENUM(convert_from, "Convert From", space_enum, NODE_VECTOR_TRANSFORM_CONVERT_SPACE_WORLD);
  SOCKET_ENUM(convert_to, "Convert To", space_enum, NODE_VECTOR_TRANSFORM_CONVERT_SPACE_OBJECT);

  SOCKET_IN_VECTOR(vector, "Vector", zero_float3());
  SOCKET_OUT_VECTOR(vector, "Vector");

  return type;
}

VectorTransformNode::VectorTransformNode() : ShaderNode(get_node_type()) {}

void VectorTransformNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_VECTOR_TRANSFORM,
                    SVMNodeVectorTransform{
                        .transform_type = transform_type,
                        .convert_from = convert_from,
                        .convert_to = convert_to,
                        .vector_in = compiler.input_float3("Vector"),
                        .vector_out_offset = compiler.output("Vector"),
                    });
}

void VectorTransformNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "transform_type");
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
  SOCKET_IN_NORMAL(normal, "Normal", zero_float3(), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(strength, "Strength", 1.0f);
  SOCKET_IN_FLOAT(distance, "Distance", 0.1f);
  SOCKET_IN_FLOAT(filter_width, "Filter Width", 0.1f);

  SOCKET_OUT_NORMAL(normal, "Normal");

  return type;
}

BumpNode::BumpNode() : ShaderNode(get_node_type())
{
  special_type = SHADER_SPECIAL_TYPE_BUMP;
}

void BumpNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_SET_BUMP,
                    SVMNodeSetBump{.scale = compiler.input_float("Distance"),
                                   .strength = compiler.input_float("Strength"),
                                   .bump_filter_width = filter_width,
                                   .normal_offset = compiler.input_link("Normal"),
                                   .invert = invert,
                                   .use_object_space = use_object_space,
                                   .center_offset = compiler.input_link("SampleCenter"),
                                   .dx_offset = compiler.input_link("SampleX"),
                                   .dy_offset = compiler.input_link("SampleY"),
                                   .out_offset = compiler.output("Normal"),
                                   .bump_state_offset = compiler.get_bump_state_offset()});
}

void BumpNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "invert");
  compiler.parameter(this, "use_object_space");
  compiler.add(this, "node_bump");
}

void BumpNode::constant_fold(const ConstantFolder &folder)
{
  ShaderInput *normal_in = input("Normal");

  if (input("Height")->link == nullptr) {
    if (normal_in->link == nullptr) {
      GeometryNode *geom = folder.graph->create_node<GeometryNode>();
      folder.bypass(geom->output("Normal"));
    }
    else {
      folder.bypass(normal_in->link);
    }
  }

  /* TODO(sergey): Ignore bump with zero strength. */
}

/* Curves node */

CurvesNode::CurvesNode(const NodeType *node_type) : ShaderNode(node_type) {}

void CurvesNode::constant_fold(const ConstantFolder &folder, ShaderInput *value_in)
{

  /* evaluate fully constant node */
  if (folder.all_inputs_constant()) {
    if (curves.size() == 0) {
      return;
    }

    float3 pos = (value - make_float3(min_x, min_x, min_x)) / (max_x - min_x);
    float3 result;

    result[0] = rgb_ramp_lookup(curves.data(), pos[0], true, extrapolate, curves.size()).x;
    result[1] = rgb_ramp_lookup(curves.data(), pos[1], true, extrapolate, curves.size()).y;
    result[2] = rgb_ramp_lookup(curves.data(), pos[2], true, extrapolate, curves.size()).z;

    folder.make_constant(interp(value, result, fac));
  }
  /* remove no-op node */
  else if (!input("Fac")->link && fac == 0.0f) {
    /* link is not null because otherwise all inputs are constant */
    folder.bypass(value_in->link);
  }
}

void CurvesNode::compile(SVMCompiler &compiler, ShaderInput *value_in, ShaderOutput *value_out)
{
  if (curves.size() == 0) {
    return;
  }

  compiler.add_node(this,
                    NODE_CURVES,
                    SVMNodeCurves{.color = compiler.input_float3(value_in->name().c_str()),
                                  .fac = compiler.input_float("Fac"),
                                  .min_x = min_x,
                                  .max_x = max_x,
                                  .table_size = uint(curves.size()),
                                  .extrapolate = extrapolate,
                                  .out_offset = compiler.output(value_out->name().c_str())});
  for (int i = 0; i < curves.size(); i++) {
    compiler.add_node_data_float4(make_float4(curves[i]));
  }
}

void CurvesNode::compile(OSLCompiler &compiler, const char *name)
{
  if (curves.size() == 0) {
    return;
  }

  compiler.parameter_color_array("ramp", curves);
  compiler.parameter(this, "min_x");
  compiler.parameter(this, "max_x");
  compiler.parameter(this, "extrapolate");
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
  SOCKET_BOOLEAN(extrapolate, "Extrapolate", true);

  SOCKET_IN_FLOAT(fac, "Fac", 0.0f);
  SOCKET_IN_COLOR(value, "Color", zero_float3());

  SOCKET_OUT_COLOR(value, "Color");

  return type;
}

RGBCurvesNode::RGBCurvesNode() : CurvesNode(get_node_type()) {}

void RGBCurvesNode::constant_fold(const ConstantFolder &folder)
{
  CurvesNode::constant_fold(folder, input("Color"));
}

void RGBCurvesNode::compile(SVMCompiler &compiler)
{
  CurvesNode::compile(compiler, input("Color"), output("Color"));
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
  SOCKET_BOOLEAN(extrapolate, "Extrapolate", true);

  SOCKET_IN_FLOAT(fac, "Fac", 0.0f);
  SOCKET_IN_VECTOR(value, "Vector", zero_float3());

  SOCKET_OUT_VECTOR(value, "Vector");

  return type;
}

VectorCurvesNode::VectorCurvesNode() : CurvesNode(get_node_type()) {}

void VectorCurvesNode::constant_fold(const ConstantFolder &folder)
{
  CurvesNode::constant_fold(folder, input("Vector"));
}

void VectorCurvesNode::compile(SVMCompiler &compiler)
{
  CurvesNode::compile(compiler, input("Vector"), output("Vector"));
}

void VectorCurvesNode::compile(OSLCompiler &compiler)
{
  CurvesNode::compile(compiler, "node_vector_curves");
}

/* FloatCurveNode */

NODE_DEFINE(FloatCurveNode)
{
  NodeType *type = NodeType::add("float_curve", create, NodeType::SHADER);

  SOCKET_FLOAT_ARRAY(curve, "Curve", array<float>());
  SOCKET_FLOAT(min_x, "Min X", 0.0f);
  SOCKET_FLOAT(max_x, "Max X", 1.0f);
  SOCKET_BOOLEAN(extrapolate, "Extrapolate", true);

  SOCKET_IN_FLOAT(fac, "Factor", 0.0f);
  SOCKET_IN_FLOAT(value, "Value", 0.0f);

  SOCKET_OUT_FLOAT(value, "Value");

  return type;
}

FloatCurveNode::FloatCurveNode() : ShaderNode(get_node_type()) {}

void FloatCurveNode::constant_fold(const ConstantFolder &folder)
{

  /* evaluate fully constant node */
  if (folder.all_inputs_constant()) {
    if (curve.size() == 0) {
      return;
    }

    const float pos = (value - min_x) / (max_x - min_x);
    const float result = float_ramp_lookup(curve.data(), pos, true, extrapolate, curve.size());

    folder.make_constant(value + fac * (result - value));
  }
  /* remove no-op node */
  else if (!input("Factor")->link && fac == 0.0f) {
    /* link is not null because otherwise all inputs are constant */
    folder.bypass(input("Value")->link);
  }
}

void FloatCurveNode::compile(SVMCompiler &compiler)
{
  if (curve.size() == 0) {
    return;
  }

  compiler.add_node(this,
                    NODE_FLOAT_CURVE,
                    SVMNodeFloatCurve{
                        .fac = compiler.input_float("Factor"),
                        .value_in = compiler.input_float("Value"),
                        .min_x = min_x,
                        .max_x = max_x,
                        .table_size = uint(curve.size()),
                        .extrapolate = extrapolate,
                        .out_offset = compiler.output("Value"),
                    });
  for (int i = 0; i < curve.size(); i++) {
    compiler.add_node_data_float(curve[i]);
  }
}

void FloatCurveNode::compile(OSLCompiler &compiler)
{
  if (curve.size() == 0) {
    return;
  }

  compiler.parameter_array("ramp", curve.data(), curve.size());
  compiler.parameter(this, "min_x");
  compiler.parameter(this, "max_x");
  compiler.parameter(this, "extrapolate");
  compiler.add(this, "node_float_curve");
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

RGBRampNode::RGBRampNode() : ShaderNode(get_node_type()) {}

void RGBRampNode::constant_fold(const ConstantFolder &folder)
{
  if (ramp.size() == 0 || ramp.size() != ramp_alpha.size()) {
    return;
  }

  if (folder.all_inputs_constant()) {
    const float f = clamp(fac, 0.0f, 1.0f) * (ramp.size() - 1);

    /* clamp int as well in case of NaN */
    const int i = clamp((int)f, 0, ramp.size() - 1);
    const float t = f - (float)i;

    const bool use_lerp = interpolate && t > 0.0f;

    if (folder.output == output("Color")) {
      const float3 color = rgb_ramp_lookup(ramp.data(), fac, use_lerp, false, ramp.size());
      folder.make_constant(color);
    }
    else if (folder.output == output("Alpha")) {
      const float alpha = float_ramp_lookup(
          ramp_alpha.data(), fac, use_lerp, false, ramp_alpha.size());
      folder.make_constant(alpha);
    }
  }
}

void RGBRampNode::compile(SVMCompiler &compiler)
{
  if (ramp.size() == 0 || ramp.size() != ramp_alpha.size()) {
    return;
  }

  compiler.add_node(this,
                    NODE_RGB_RAMP,
                    SVMNodeRGBRamp{.table_size = uint(ramp.size()),
                                   .fac = compiler.input_float("Fac"),
                                   .interpolate = interpolate,
                                   .color_offset = compiler.output("Color"),
                                   .alpha_offset = compiler.output("Alpha")});
  for (int i = 0; i < ramp.size(); i++) {
    compiler.add_node_data_float4(make_float4(ramp[i], ramp_alpha[i]));
  }
}

void RGBRampNode::compile(OSLCompiler &compiler)
{
  if (ramp.size() == 0 || ramp.size() != ramp_alpha.size()) {
    return;
  }

  compiler.parameter_color_array("ramp_color", ramp);
  compiler.parameter_array("ramp_alpha", ramp_alpha.data(), ramp_alpha.size());
  compiler.parameter(this, "interpolate");

  compiler.add(this, "node_rgb_ramp");
}

/* Set Normal Node */

NODE_DEFINE(SetNormalNode)
{
  NodeType *type = NodeType::add("set_normal", create, NodeType::SHADER);

  SOCKET_IN_VECTOR(direction, "Direction", zero_float3());
  SOCKET_OUT_NORMAL(normal, "Normal");

  return type;
}

SetNormalNode::SetNormalNode() : ShaderNode(get_node_type()) {}

void SetNormalNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_CLOSURE_SET_NORMAL,
                    SVMNodeClosureSetNormal{
                        .direction_offset = compiler.input_link("Direction"),
                        .normal_offset = compiler.output("Normal"),
                    });
}

void SetNormalNode::compile(OSLCompiler &compiler)
{
  compiler.add(this, "node_set_normal");
}

/* OSLNode */

OSLNode::OSLNode() : ShaderNode(new NodeType(NodeType::SHADER))
{
  special_type = SHADER_SPECIAL_TYPE_OSL;
  has_emission = false;
}

OSLNode::~OSLNode()
{
  delete type;
}

ShaderNode *OSLNode::clone(ShaderGraph *graph) const
{
  return OSLNode::create(graph, this->inputs.size(), this);
}

void OSLNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  /* the added geometry node's attributes function unfortunately doesn't
   * request the need for ATTR_STD_GENERATED in-time somehow, so we request it
   * here if there are any sockets that have LINK_TANGENT or
   * LINK_TEXTURE_GENERATED flags */
  if (shader->has_surface_link()) {
    for (const ShaderInput *in : inputs) {
      if (!in->link && (in->flags() & SocketType::LINK_TANGENT ||
                        in->flags() & SocketType::LINK_TEXTURE_GENERATED))
      {
        attributes->add(ATTR_STD_GENERATED);
        break;
      }
    }
  }

  ShaderNode::attributes(shader, attributes);
}

OSLNode *OSLNode::create(ShaderGraph *graph, const size_t num_inputs, const OSLNode *from)
{
  /* allocate space for the node itself and parameters, aligned to 16 bytes
   * assuming that's the most parameter types need */
  const size_t node_size = align_up(sizeof(OSLNode), 16);
  const size_t inputs_size = align_up(SocketType::max_size(), 16) * num_inputs;

  char *node_memory = (char *)operator new(node_size + inputs_size);
  memset(node_memory, 0, node_size + inputs_size);

  if (!from) {
    return graph->create_osl_node<OSLNode>(node_memory);
  }
  /* copy input default values and node type for cloning */
  memcpy(node_memory + node_size, (char *)from + node_size, inputs_size);

  OSLNode *node = graph->create_osl_node<OSLNode>(node_memory, *from);
  node->type = new NodeType(*(from->type));
  return node;
}

char *OSLNode::input_default_value()
{
  /* pointer to default value storage, which is the same as our actual value */
  const size_t num_inputs = type->inputs.size();
  const size_t inputs_size = align_up(SocketType::max_size(), 16) * num_inputs;
  return (char *)this + align_up(sizeof(OSLNode), 16) + inputs_size;
}

void OSLNode::add_input(ustring name, SocketType::Type socket_type, const int flags)
{
  char *memory = input_default_value();
  const size_t offset = memory - (char *)this;
  const_cast<NodeType *>(type)->register_input(
      name, name, socket_type, offset, memory, nullptr, nullptr, flags | SocketType::LINKABLE);
}

void OSLNode::add_output(ustring name, SocketType::Type socket_type)
{
  const_cast<NodeType *>(type)->register_output(name, name, socket_type);
}

void OSLNode::compile(SVMCompiler & /*compiler*/)
{
  /* doesn't work for SVM, obviously ... */
}

void OSLNode::compile(OSLCompiler &compiler)
{
  if (!filepath.empty()) {
    compiler.add(this, filepath.c_str(), true);
  }
  else {
    compiler.add(this, bytecode_hash.c_str(), false);
  }
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

  static NodeEnum convention_enum;
  convention_enum.insert("opengl", NODE_NORMAL_MAP_CONVENTION_OPENGL);
  convention_enum.insert("directx", NODE_NORMAL_MAP_CONVENTION_DIRECTX);
  SOCKET_ENUM(convention, "Convention", convention_enum, NODE_NORMAL_MAP_CONVENTION_OPENGL);

  static NodeEnum base_enum;
  base_enum.insert("original", NODE_NORMAL_MAP_BASE_ORIGINAL);
  base_enum.insert("displaced", NODE_NORMAL_MAP_BASE_DISPLACED);
  SOCKET_ENUM(base, "Base", base_enum, NODE_NORMAL_MAP_BASE_ORIGINAL);

  SOCKET_STRING(attribute, "Attribute", ustring());

  SOCKET_IN_FLOAT(strength, "Strength", 1.0f);
  SOCKET_IN_COLOR(color, "Color", make_float3(0.5f, 0.5f, 1.0f));

  SOCKET_OUT_NORMAL(normal, "Normal");

  return type;
}

NormalMapNode::NormalMapNode() : ShaderNode(get_node_type()) {}

void NormalMapNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface_link() && space == NODE_NORMAL_MAP_TANGENT) {
    if (attribute.empty()) {
      /* We don't need the UV ourselves, but we need to compute the tangent from it. */
      attributes->add(ATTR_STD_UV);
      if (base == NODE_NORMAL_MAP_BASE_DISPLACED) {
        attributes->add(ATTR_STD_UV_TANGENT);
        attributes->add(ATTR_STD_UV_TANGENT_SIGN);
      }
      else {
        attributes->add(ATTR_STD_UV_TANGENT_UNDISPLACED);
        attributes->add(ATTR_STD_UV_TANGENT_SIGN_UNDISPLACED);
        attributes->add(ATTR_STD_NORMAL_UNDISPLACED);
      }
    }
    else {
      attributes->add(attribute);
      if (base == NODE_NORMAL_MAP_BASE_DISPLACED) {
        attributes->add(ustring((string(attribute.c_str()) + ".tangent").c_str()));
        attributes->add(ustring((string(attribute.c_str()) + ".tangent_sign").c_str()));
      }
      else {
        attributes->add(ustring((string(attribute.c_str()) + ".undisplaced_tangent").c_str()));
        attributes->add(
            ustring((string(attribute.c_str()) + ".undisplaced_tangent_sign").c_str()));
        attributes->add(ATTR_STD_NORMAL_UNDISPLACED);
      }
    }
  }

  ShaderNode::attributes(shader, attributes);
}

void NormalMapNode::compile(SVMCompiler &compiler)
{
  int attr_id = 0;
  int attr_sign_id = 0;

  if (space == NODE_NORMAL_MAP_TANGENT) {
    if (attribute.empty()) {
      if (base == NODE_NORMAL_MAP_BASE_DISPLACED) {
        attr_id = compiler.attribute(ATTR_STD_UV_TANGENT);
        attr_sign_id = compiler.attribute(ATTR_STD_UV_TANGENT_SIGN);
      }
      else {
        attr_id = compiler.attribute(ATTR_STD_UV_TANGENT_UNDISPLACED);
        attr_sign_id = compiler.attribute(ATTR_STD_UV_TANGENT_SIGN_UNDISPLACED);
      }
    }
    else {
      if (base == NODE_NORMAL_MAP_BASE_DISPLACED) {
        attr_id = compiler.attribute(ustring((string(attribute.c_str()) + ".tangent").c_str()));
        attr_sign_id = compiler.attribute(
            ustring((string(attribute.c_str()) + ".tangent_sign").c_str()));
      }
      else {
        attr_id = compiler.attribute(
            ustring((string(attribute.c_str()) + ".undisplaced_tangent").c_str()));
        attr_sign_id = compiler.attribute(
            ustring((string(attribute.c_str()) + ".undisplaced_tangent_sign").c_str()));
      }
    }
  }

  compiler.add_node(this,
                    NODE_NORMAL_MAP,
                    SVMNodeNormalMap{
                        .space = space,
                        .invert_green = (convention == NODE_NORMAL_MAP_CONVENTION_DIRECTX) ? 1 : 0,
                        .use_original_base = (base == NODE_NORMAL_MAP_BASE_ORIGINAL) ? 1 : 0,
                        .attr = attr_id,
                        .attr_sign = attr_sign_id,
                        .color = compiler.input_float3("Color"),
                        .strength = compiler.input_float("Strength"),
                        .normal_offset = compiler.output("Normal"),
                    });
}

void NormalMapNode::compile(OSLCompiler &compiler)
{
  if (space == NODE_NORMAL_MAP_TANGENT) {
    std::string attr_name, attr_sign_name;

    if (attribute.empty()) {
      if (base == NODE_NORMAL_MAP_BASE_DISPLACED) {
        attr_name = "geom:tangent";
        attr_sign_name = "geom:tangent_sign";
      }
      else {
        attr_name = "geom:undisplaced_tangent";
        attr_sign_name = "geom:undisplaced_tangent_sign";
      }
    }
    else {
      if (base == NODE_NORMAL_MAP_BASE_DISPLACED) {
        attr_name = string(attribute.c_str()) + ".tangent";
        attr_sign_name = string(attribute.c_str()) + ".tangent_sign";
      }
      else {
        attr_name = string(attribute.c_str()) + ".undisplaced_tangent";
        attr_sign_name = string(attribute.c_str()) + ".undisplaced_tangent_sign";
      }
    }

    compiler.parameter("attr_name", attr_name.c_str());
    compiler.parameter("attr_sign_name", attr_sign_name.c_str());
  }

  compiler.parameter(this, "space");
  compiler.parameter(this, "convention");
  compiler.parameter(this, "base");
  compiler.add(this, "node_normal_map");
}

/* Radial Tiling */

NODE_DEFINE(RadialTilingNode)
{
  NodeType *type = NodeType::add("radial_tiling", create, NodeType::SHADER);

  SOCKET_BOOLEAN(use_normalize, "Normalize", false);
  SOCKET_IN_POINT(vector, "Vector", zero_float3());
  SOCKET_IN_FLOAT(r_gon_sides, "Sides", 5.0f);
  SOCKET_IN_FLOAT(r_gon_roundness, "Roundness", 0.0f);

  SOCKET_OUT_POINT(segment_coordinates, "Segment Coordinates");
  SOCKET_OUT_FLOAT(segment_id, "Segment ID");
  SOCKET_OUT_FLOAT(max_unit_parameter, "Segment Width");
  SOCKET_OUT_FLOAT(x_axis_A_angle_bisector, "Segment Rotation");

  return type;
}

RadialTilingNode::RadialTilingNode() : ShaderNode(get_node_type()) {}

void RadialTilingNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_RADIAL_TILING,
                    SVMNodeRadialTiling{
                        .vector = compiler.input_float3("Vector"),
                        .r_gon_sides = compiler.input_float("Sides"),
                        .r_gon_roundness = compiler.input_float("Roundness"),
                        .normalize_r_gon_parameter = use_normalize,
                        .segment_coordinates_offset = compiler.output("Segment Coordinates"),
                        .segment_id_offset = compiler.output("Segment ID"),
                        .max_unit_parameter_offset = compiler.output("Segment Width"),
                        .x_axis_A_angle_bisector_offset = compiler.output("Segment Rotation"),
                    });
}

void RadialTilingNode::compile(OSLCompiler &compiler)
{
  compiler.parameter(this, "use_normalize");
  compiler.add(this, "node_radial_tiling");
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

  SOCKET_OUT_NORMAL(tangent, "Tangent");

  return type;
}

TangentNode::TangentNode() : ShaderNode(get_node_type()) {}

void TangentNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface_link()) {
    if (direction_type == NODE_TANGENT_UVMAP) {
      if (attribute.empty()) {
        /* We don't need the UV ourselves, but we need to compute the tangent from it. */
        attributes->add(ATTR_STD_UV);
        attributes->add(ATTR_STD_UV_TANGENT);
      }
      else {
        attributes->add(attribute);
        attributes->add(ustring((string(attribute.c_str()) + ".tangent").c_str()));
      }
    }
    else {
      attributes->add(ATTR_STD_GENERATED);
    }
  }

  ShaderNode::attributes(shader, attributes);
}

void TangentNode::compile(SVMCompiler &compiler)
{
  int attr;

  if (direction_type == NODE_TANGENT_UVMAP) {
    if (attribute.empty()) {
      attr = compiler.attribute(ATTR_STD_UV_TANGENT);
    }
    else {
      attr = compiler.attribute(ustring((string(attribute.c_str()) + ".tangent").c_str()));
    }
  }
  else {
    attr = compiler.attribute(ATTR_STD_GENERATED);
  }

  compiler.add_node(this,
                    NODE_TANGENT,
                    SVMNodeTangent{
                        .direction_type = direction_type,
                        .axis = axis,
                        .attr = attr,
                        .tangent_offset = compiler.output("Tangent"),
                    });
}

void TangentNode::compile(OSLCompiler &compiler)
{
  if (direction_type == NODE_TANGENT_UVMAP) {
    if (attribute.empty()) {
      compiler.parameter("attr_name", ustring("geom:tangent"));
    }
    else {
      compiler.parameter("attr_name", ustring((string(attribute.c_str()) + ".tangent").c_str()));
    }
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
  SOCKET_IN_NORMAL(normal, "Normal", zero_float3(), SocketType::LINK_NORMAL);

  SOCKET_OUT_NORMAL(bevel, "Normal");

  return type;
}

BevelNode::BevelNode() : ShaderNode(get_node_type()) {}

void BevelNode::compile(SVMCompiler &compiler)
{
  compiler.add_node(this,
                    NODE_BEVEL,
                    SVMNodeBevel{
                        .radius = compiler.input_float("Radius"),
                        .num_samples = uint8_t(samples),
                        .normal_offset = compiler.input_link("Normal"),
                        .out_offset = compiler.output("Normal"),
                    });
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
  SOCKET_IN_NORMAL(normal, "Normal", zero_float3(), SocketType::LINK_NORMAL);

  SOCKET_OUT_VECTOR(displacement, "Displacement");

  return type;
}

DisplacementNode::DisplacementNode() : ShaderNode(get_node_type()) {}

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
  compiler.add_node(this,
                    NODE_DISPLACEMENT,
                    SVMNodeDisplacement{.space = space,
                                        .height = compiler.input_float("Height"),
                                        .midlevel = compiler.input_float("Midlevel"),
                                        .scale = compiler.input_float("Scale"),
                                        .normal_offset = compiler.input_link("Normal"),
                                        .out_offset = compiler.output("Displacement")});
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

  SOCKET_IN_COLOR(vector, "Vector", zero_float3());
  SOCKET_IN_FLOAT(midlevel, "Midlevel", 0.0f);
  SOCKET_IN_FLOAT(scale, "Scale", 1.0f);

  SOCKET_OUT_VECTOR(displacement, "Displacement");

  return type;
}

VectorDisplacementNode::VectorDisplacementNode() : ShaderNode(get_node_type()) {}

void VectorDisplacementNode::constant_fold(const ConstantFolder &folder)
{
  if (folder.all_inputs_constant()) {
    if ((vector == zero_float3() && midlevel == 0.0f) || (scale == 0.0f)) {
      folder.make_zero();
    }
  }
}

void VectorDisplacementNode::attributes(Shader *shader, AttributeRequestSet *attributes)
{
  if (shader->has_surface_link() && space == NODE_NORMAL_MAP_TANGENT) {
    if (attribute.empty()) {
      attributes->add(ATTR_STD_UV);
      attributes->add(ATTR_STD_UV_TANGENT_UNDISPLACED);
      attributes->add(ATTR_STD_UV_TANGENT_SIGN_UNDISPLACED);
    }
    else {
      attributes->add(attribute);
      attributes->add(ustring((string(attribute.c_str()) + ".undisplaced_tangent").c_str()));
      attributes->add(ustring((string(attribute.c_str()) + ".undisplaced_tangent_sign").c_str()));
    }
  }

  ShaderNode::attributes(shader, attributes);
}

void VectorDisplacementNode::compile(SVMCompiler &compiler)
{
  int attr = 0;
  int attr_sign = 0;

  if (space == NODE_NORMAL_MAP_TANGENT) {
    if (attribute.empty()) {
      attr = compiler.attribute(ATTR_STD_UV_TANGENT_UNDISPLACED);
      attr_sign = compiler.attribute(ATTR_STD_UV_TANGENT_SIGN_UNDISPLACED);
    }
    else {
      attr = compiler.attribute(
          ustring((string(attribute.c_str()) + ".undisplaced_tangent").c_str()));
      attr_sign = compiler.attribute(
          ustring((string(attribute.c_str()) + ".undisplaced_tangent_sign").c_str()));
    }
  }

  compiler.add_node(
      this,
      NODE_VECTOR_DISPLACEMENT,
      SVMNodeVectorDisplacement{.space = space,
                                .vector = compiler.input_float3("Vector"),
                                .midlevel = compiler.input_float("Midlevel"),
                                .scale = compiler.input_float("Scale"),
                                .attr = attr,
                                .attr_sign = attr_sign,
                                .displacement_offset = compiler.output("Displacement")});
}

void VectorDisplacementNode::compile(OSLCompiler &compiler)
{
  if (space == NODE_NORMAL_MAP_TANGENT) {
    if (attribute.empty()) {
      compiler.parameter("attr_name", ustring("geom:undisplaced_tangent"));
      compiler.parameter("attr_sign_name", ustring("geom:undisplaced_tangent_sign"));
    }
    else {
      compiler.parameter("attr_name",
                         ustring((string(attribute.c_str()) + ".undisplaced_tangent").c_str()));
      compiler.parameter(
          "attr_sign_name",
          ustring((string(attribute.c_str()) + ".undisplaced_tangent_sign").c_str()));
    }
  }

  compiler.parameter(this, "space");
  compiler.add(this, "node_vector_displacement");
}

/* Raycast */

static SocketType::Type get_socket_type(
    const RaycastNode::AttributeOutputType attribute_output_type)
{
  switch (attribute_output_type) {
    case RaycastNode::ATTR_OUTPUT_FLOAT3:
      return SocketType::VECTOR;
    case RaycastNode::ATTR_OUTPUT_FLOAT:
      return SocketType::FLOAT;
    case RaycastNode::ATTR_OUTPUT_FLOAT_ALPHA:
      return SocketType::FLOAT;
  }
  LOG_DFATAL << "Invalid attribute output type " << int(attribute_output_type);
  return SocketType::UNDEFINED;
}

static NodeAttributeOutputType get_node_attribute_output_type(
    const RaycastNode::AttributeOutputType attribute_output_type)
{
  switch (attribute_output_type) {
    case RaycastNode::ATTR_OUTPUT_FLOAT3:
      return NODE_ATTR_OUTPUT_FLOAT3;
    case RaycastNode::ATTR_OUTPUT_FLOAT:
      return NODE_ATTR_OUTPUT_FLOAT;
    case RaycastNode::ATTR_OUTPUT_FLOAT_ALPHA:
      return NODE_ATTR_OUTPUT_FLOAT_ALPHA;
  }
  LOG_DFATAL << "Invalid attribute output type " << int(attribute_output_type);
  return NODE_ATTR_OUTPUT_FLOAT;
}

NODE_DEFINE(RaycastNode)
{
  NodeType *type = NodeType::add("raycast", create, NodeType::SHADER);

  SOCKET_IN_POINT(position, "Position", zero_float3(), SocketType::LINK_POSITION);
  SOCKET_IN_NORMAL(direction, "Direction", zero_float3(), SocketType::LINK_NORMAL);
  SOCKET_IN_FLOAT(length, "Length", 1.0f);

  SOCKET_OUT_FLOAT(is_hit, "Is Hit");
  SOCKET_OUT_FLOAT(is_self_hit, "Self Hit");
  SOCKET_OUT_FLOAT(hit_distance, "Hit Distance");
  SOCKET_OUT_POINT(hit_position, "Hit Position");
  SOCKET_OUT_NORMAL(hit_normal, "Hit Normal");

  SOCKET_BOOLEAN(only_local, "Only Local", false);

  return type;
}

RaycastNode::RaycastNode() : ShaderNode(get_node_type()) {}

RaycastNode::RaycastNode(const RaycastNode &other)
    : ShaderNode(other),
      position(other.position),
      direction(other.direction),
      length(other.length),
      only_local(other.only_local)
{
  for (const AttributeOutput &other_attribute_output : other.attribute_outputs_) {
    /* The ShaderNode() is expected to only take care of sockets that are part of the node type. */
    assert(output(other_attribute_output.socket_id) == nullptr);

    add_output_attribute_socket(other_attribute_output.attribute_name,
                                other_attribute_output.attribute_output_type,
                                other_attribute_output.socket_id);
  }
}

void RaycastNode::global_attributes(Shader *shader, AttributeRequestSet *attributes)
{
  for (const AttributeOutput &attribute_output : attribute_outputs_) {
    AttributeNode::add_named_attribute_request(attributes, attribute_output.attribute_name);
  }

  ShaderNode::global_attributes(shader, attributes);
}

void RaycastNode::add_output_attribute_socket(const ustring attribute_name,
                                              const AttributeOutputType attribute_output_type,
                                              const ustring socket_id)
{
  const SocketType::Type type = get_socket_type(attribute_output_type);
  if (type == SocketType::UNDEFINED) {
    return;
  }

  const AttributeOutput attribute_output = {
      .attribute_name = attribute_name,
      .attribute_output_type = attribute_output_type,
      .socket_id = socket_id,
  };
  attribute_outputs_.push_back(attribute_output);

  auto socket_type = std::make_unique<SocketType>();
  socket_type->name = socket_id;
  socket_type->type = type;
  socket_type->flags = SocketType::LINKABLE;
  socket_type->ui_name = socket_id;

  auto shader_output = std::make_unique<ShaderOutput>(*socket_type.get(), this);
  outputs.push_back(std::move(shader_output));

  socket_types_.push_back(std::move(socket_type));
}

void RaycastNode::compile(SVMCompiler &compiler)
{
  uint num_linked_attributes = 0;
  for (const auto &attribute_output : attribute_outputs_) {
    assert(num_linked_attributes < std::numeric_limits<uint16_t>::max() - 1);
    if (!output(attribute_output.socket_id)->links.empty()) {
      ++num_linked_attributes;
    }
  }

  compiler.add_node(
      this,
      NODE_RAYCAST,
      SVMNodeRaycast{
          .position = compiler.input_float3("Position"),
          .direction = compiler.input_float3("Direction"),
          .distance = compiler.input_float("Length"),
          .bump_filter_width = (bump == SHADER_BUMP_CENTER) ? 0.0f : bump_filter_width,
          .only_local = only_local,
          .num_attributes = uint16_t(num_linked_attributes),
          .is_hit_offset = compiler.output("Is Hit"),
          .is_self_hit_offset = compiler.output("Self Hit"),
          .hit_distance_offset = compiler.output("Hit Distance"),
          .hit_position_offset = compiler.output("Hit Position"),
          .hit_normal_offset = compiler.output("Hit Normal"),
      });

  for (const auto &attribute_output : attribute_outputs_) {
    ShaderOutput *shader_output = output(attribute_output.socket_id);
    if (shader_output->links.empty()) {
      continue;
    }

    compiler.add_node(
        this,
        NODE_ATTR,
        SVMNodeAttr{
            .attr = int(compiler.attribute_standard(attribute_output.attribute_name)),
            .out_offset = compiler.output(shader_output),
            .output_type = get_node_attribute_output_type(attribute_output.attribute_output_type),
            .bump_offset = NODE_BUMP_OFFSET_CENTER,
            .store_derivatives = false,
            .bump_filter_width = 0.0f,
        });
  }
}

void RaycastNode::compile(OSLCompiler &compiler)
{
  /* Collect and pass the names of per-output-type attributes. */
  array<ustring> float_attribute_names;
  array<ustring> alpha_attribute_names;
  array<ustring> vector_attribute_names;
  for (const auto &attribute_output : attribute_outputs_) {
    switch (attribute_output.attribute_output_type) {
      case ATTR_OUTPUT_FLOAT:
        float_attribute_names.push_back_slow(attribute_output.attribute_name);
        break;
      case ATTR_OUTPUT_FLOAT_ALPHA:
        alpha_attribute_names.push_back_slow(attribute_output.attribute_name);
        break;
      case ATTR_OUTPUT_FLOAT3:
        vector_attribute_names.push_back_slow(attribute_output.attribute_name);
        break;
    }
  }
  compiler.parameter_string_array("float_attribute_names", float_attribute_names);
  compiler.parameter_string_array("alpha_attribute_names", alpha_attribute_names);
  compiler.parameter_string_array("vector_attribute_names", vector_attribute_names);

  compiler.parameter(this, "only_local");
  compiler.parameter("bump_filter_width", (bump == SHADER_BUMP_CENTER) ? 0.0f : bump_filter_width);
  compiler.add(this, "node_raycast");

  int float_attr_index = 0;
  int alpha_attr_index = 0;
  int vector_attr_index = 0;
  for (const auto &attribute_output : attribute_outputs_) {
    switch (attribute_output.attribute_output_type) {
      case ATTR_OUTPUT_FLOAT:
        compiler.parameter("attribute_index", float_attr_index);
        compiler.add_output_converter(this,
                                      "node_raycast_attr_float",
                                      "float_attributes",
                                      "float_attributes",
                                      "value",
                                      attribute_output.socket_id);
        ++float_attr_index;
        break;
      case ATTR_OUTPUT_FLOAT_ALPHA:
        compiler.parameter("attribute_index", alpha_attr_index);
        compiler.add_output_converter(this,
                                      "node_raycast_attr_float",
                                      "alpha_attributes",
                                      "float_attributes",
                                      "value",
                                      attribute_output.socket_id);
        ++alpha_attr_index;
        break;
      case ATTR_OUTPUT_FLOAT3:
        compiler.parameter("attribute_index", vector_attr_index);
        compiler.add_output_converter(this,
                                      "node_raycast_attr_vector",
                                      "vector_attributes",
                                      "vector_attributes",
                                      "value",
                                      attribute_output.socket_id);
        ++vector_attr_index;
        break;
    }
  }
}

CCL_NAMESPACE_END
