/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_light_convert.hh"

#include "usd.hh"
#include "usd_asset_utils.hh"
#include "usd_reader_prim.hh"
#include "usd_writer_material.hh"

#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/vec3f.h>

#include <pxr/usd/ar/packageUtils.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdGeom/xformCommonAPI.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

#include "BKE_image.h"
#include "BKE_light.h"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_scene.hh"
#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "DNA_light_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"
#include "ED_node.hh"

#include <string>

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.usd"};

namespace usdtokens {
// Attribute names.
static const pxr::TfToken color("color", pxr::TfToken::Immortal);
static const pxr::TfToken intensity("intensity", pxr::TfToken::Immortal);
static const pxr::TfToken texture_file("texture:file", pxr::TfToken::Immortal);
}  // namespace usdtokens

namespace {

/* If the given attribute has an authored value, return its value in the r_value
 * out parameter.
 *
 * We wish to support older UsdLux APIs in older versions of USD.  For example,
 * in previous versions of the API, shader input attibutes did not have the
 * "inputs:" prefix.  One can provide the older input attibute name in the
 * 'fallback_attr_name' argument, and that attribute will be queried if 'attr'
 * doesn't exist or doesn't have an authored value.
 */
template<typename T>
bool get_authored_value(const pxr::UsdAttribute attr,
                        const double motionSampleTime,
                        T *r_value,
                        const pxr::UsdPrim prim = pxr::UsdPrim(),
                        const pxr::TfToken fallback_attr_name = pxr::TfToken())
{
  if (attr && attr.HasAuthoredValue()) {
    return attr.Get<T>(r_value, motionSampleTime);
  }

  if (!prim || fallback_attr_name.IsEmpty()) {
    return false;
  }

  pxr::UsdAttribute fallback_attr = prim.GetAttribute(fallback_attr_name);
  if (fallback_attr && fallback_attr.HasAuthoredValue()) {
    return fallback_attr.Get<T>(r_value, motionSampleTime);
  }

  return false;
}

/* Helper struct for retrieving shader information when traversing a world material
 * node chain, provided as user data for bke::nodeChainIter(). */
struct WorldNtreeSearchResults {
  const blender::io::usd::USDExportParams params;
  pxr::UsdStageRefPtr stage;

  float world_color[3];
  float world_intensity;
  float mapping_rot[3];

  std::string file_path;

  float color_mult[3];

  bool background_found;
  bool env_tex_found;
  bool mult_found;
  bool mapping_found;

  WorldNtreeSearchResults(const blender::io::usd::USDExportParams &in_params,
                          pxr::UsdStageRefPtr in_stage)
      : params(in_params),
        stage(in_stage),
        world_intensity(0.0f),
        background_found(false),
        env_tex_found(false),
        mult_found(false)
  {
  }
};

}  // End anonymous namespace.

namespace blender::io::usd {

/* If the given path already exists on the given stage, return the path with
 * a numerical suffix appende to the name that ensures the path is unique. If
 * the path does not exist on the stage, it will be returned unchanged. */
static pxr::SdfPath get_unique_path(pxr::UsdStageRefPtr stage, const std::string &path)
{
  std::string unique_path = path;
  int suffix = 2;
  while (stage->GetPrimAtPath(pxr::SdfPath(unique_path)).IsValid()) {
    unique_path = path + std::to_string(suffix++);
  }
  return pxr::SdfPath(unique_path);
}

/* Load the image at the given path.  Handle packing and copying based in the import options.
 * Return the opened image on succsss or a nullptr on failure. */
static Image *load_image(std::string tex_path, Main *bmain, const USDImportParams &params)
{
  /* Optionally copy the asset if it's inside a USDZ package. */
  const bool import_textures = params.import_textures_mode != USD_TEX_IMPORT_NONE &&
                               pxr::ArIsPackageRelativePath(tex_path);

  if (import_textures) {
    /* If we are packing the imported textures, we first write them
     * to a temporary directory. */
    const char *textures_dir = params.import_textures_mode == USD_TEX_IMPORT_PACK ?
                                   temp_textures_dir() :
                                   params.import_textures_dir;

    const eUSDTexNameCollisionMode name_collision_mode = params.import_textures_mode ==
                                                                 USD_TEX_IMPORT_PACK ?
                                                             USD_TEX_NAME_COLLISION_OVERWRITE :
                                                             params.tex_name_collision_mode;

    tex_path = import_asset(tex_path.c_str(), textures_dir, name_collision_mode, nullptr);
  }

  Image *image = BKE_image_load_exists(bmain, tex_path.c_str());
  if (!image) {
    return nullptr;
  }

  if (import_textures && params.import_textures_mode == USD_TEX_IMPORT_PACK &&
      !BKE_image_has_packedfile(image))
  {
    BKE_image_packfiles(nullptr, image, ID_BLEND_PATH(bmain, &image->id));
    if (BLI_is_dir(temp_textures_dir())) {
      BLI_delete(temp_textures_dir(), true, true);
    }
  }

  return image;
}

/* Create a new node of type 'new_node_type' and connect it
 * as an upstream source to 'dst_node' with the given sockets. */
static bNode *append_node(bNode *dst_node,
                          int16_t new_node_type,
                          const char *out_sock,
                          const char *in_sock,
                          bNodeTree *ntree,
                          float offset)
{
  bNode *src_node = bke::nodeAddStaticNode(NULL, ntree, new_node_type);

  if (!src_node) {
    return nullptr;
  }

  bke::nodeAddLink(ntree,
                   src_node,
                   bke::nodeFindSocket(src_node, SOCK_OUT, out_sock),
                   dst_node,
                   bke::nodeFindSocket(dst_node, SOCK_IN, in_sock));

  src_node->locx = dst_node->locx - offset;
  src_node->locy = dst_node->locy;

  return src_node;
}

/* Callback function for iterating over a shader node chain to retrieve data
 * necessary for converting a world material to a USD dome light. It also
 * handles copying textures, if required. */
static bool node_search(bNode *fromnode,
                        bNode * /* tonode */,
                        void *userdata,
                        const bool /*reversed*/)
{
  if (!(userdata && fromnode)) {
    return true;
  }

  WorldNtreeSearchResults *res = reinterpret_cast<WorldNtreeSearchResults *>(userdata);

  if (!res->background_found && fromnode->type == SH_NODE_BACKGROUND) {
    /* Get light color and intensity */
    bNodeSocketValueRGBA *color_data = bke::nodeFindSocket(fromnode, SOCK_IN, "Color")
                                           ->default_value_typed<bNodeSocketValueRGBA>();
    bNodeSocketValueFloat *strength_data = bke::nodeFindSocket(fromnode, SOCK_IN, "Strength")
                                               ->default_value_typed<bNodeSocketValueFloat>();

    res->background_found = true;
    res->world_intensity = strength_data->value;
    res->world_color[0] = color_data->value[0];
    res->world_color[1] = color_data->value[1];
    res->world_color[2] = color_data->value[2];
  }
  else if (!res->env_tex_found && fromnode->type == SH_NODE_TEX_ENVIRONMENT) {
    /* Get env tex path. */

    res->file_path = get_tex_image_asset_filepath(fromnode, res->stage, res->params);

    if (!res->file_path.empty()) {
      res->env_tex_found = true;
      if (res->params.export_textures) {
        export_texture(fromnode, res->stage, res->params.overwrite_textures);
      }
    }
  }
  else if (!res->env_tex_found && !res->mult_found && fromnode->type == SH_NODE_VECTOR_MATH) {

    if (fromnode->custom1 == NODE_VECTOR_MATH_MULTIPLY) {
      res->mult_found = true;

      bNodeSocket *vec_sock = bke::nodeFindSocket(fromnode, SOCK_IN, "Vector");
      if (vec_sock) {
        vec_sock = vec_sock->next;
      }

      if (vec_sock) {
        copy_v3_v3(res->color_mult, ((bNodeSocketValueVector *)vec_sock->default_value)->value);
      }
    }
  }
  else if (res->env_tex_found && fromnode->type == SH_NODE_MAPPING) {
    res->mapping_found = true;
    copy_v3_fl(res->mapping_rot, 0.0f);
    if (bNodeSocket *socket = bke::nodeFindSocket(fromnode, SOCK_IN, "Rotation")) {
      bNodeSocketValueVector *rot_value = static_cast<bNodeSocketValueVector *>(
          socket->default_value);
      copy_v3_v3(res->mapping_rot, rot_value->value);
    }
  }

  return true;
}

/* If the Blender scene has an environment texture,
 * export it as a USD dome light. */
void world_material_to_dome_light(const USDExportParams &params,
                                  const Scene *scene,
                                  pxr::UsdStageRefPtr stage)
{
  if (!(stage && scene && scene->world && scene->world->use_nodes && scene->world->nodetree)) {
    return;
  }

  /* Find the world output. */
  const bNodeTree *ntree = scene->world->nodetree;
  ntree->ensure_topology_cache();
  const blender::Span<const bNode *> bsdf_nodes = ntree->nodes_by_type("ShaderNodeOutputWorld");
  const bNode *output = bsdf_nodes.is_empty() ? nullptr : bsdf_nodes.first();

  if (!output) {
    /* No output, no valid network to convert. */
    return;
  }

  WorldNtreeSearchResults res(params, stage);

  bke::nodeChainIter(scene->world->nodetree, output, node_search, &res, true);

  if (!(res.background_found || res.env_tex_found)) {
    /* No nodes to convert */
    return;
  }

  /* Create USD dome light. */

  pxr::SdfPath env_light_path = get_unique_path(stage,
                                                std::string(params.root_prim_path) + "/env_light");

  pxr::UsdLuxDomeLight dome_light = pxr::UsdLuxDomeLight::Define(stage, env_light_path);

  if (res.env_tex_found) {
    pxr::SdfAssetPath path(res.file_path);
    dome_light.CreateTextureFileAttr().Set(path);

    if (res.mult_found) {
      pxr::GfVec3f color_val(res.color_mult[0], res.color_mult[1], res.color_mult[2]);
      dome_light.CreateColorAttr().Set(color_val);
    }
  }
  else {
    pxr::GfVec3f color_val(res.world_color[0], res.world_color[1], res.world_color[2]);
    dome_light.CreateColorAttr().Set(color_val);
  }

  if (res.background_found) {
    dome_light.CreateIntensityAttr().Set(res.world_intensity);
  }

  /* We always set a default rotation on the light, whether or not res.mapping_found
   * is true, since res.mapping_rot defaults to zeros. */

  /* Convert radians to degrees. */
  mul_v3_fl(res.mapping_rot, 180.0f / M_PI);

  pxr::GfMatrix4d xf =
      pxr::GfMatrix4d().SetRotate(pxr::GfRotation(pxr::GfVec3d(1.0, 0.0, 0.0), 90.0)) *
      pxr::GfMatrix4d().SetRotate(pxr::GfRotation(pxr::GfVec3d(0.0, 0.0, 1.0), 90.0)) *
      pxr::GfMatrix4d().SetRotate(
          pxr::GfRotation(pxr::GfVec3d(0.0, 0.0, 1.0), -res.mapping_rot[2])) *
      pxr::GfMatrix4d().SetRotate(
          pxr::GfRotation(pxr::GfVec3d(0.0, 1.0, 0.0), -res.mapping_rot[1])) *
      pxr::GfMatrix4d().SetRotate(
          pxr::GfRotation(pxr::GfVec3d(1.0, 0.0, 0.0), -res.mapping_rot[0]));

  pxr::GfVec3d angles = xf.DecomposeRotation(
      pxr::GfVec3d::ZAxis(), pxr::GfVec3d::YAxis(), pxr::GfVec3d::XAxis());

  pxr::GfVec3f rot_vec(angles[2], angles[1], angles[0]);

  pxr::UsdGeomXformCommonAPI xform_api(dome_light);
  xform_api.SetRotate(rot_vec, pxr::UsdGeomXformCommonAPI::RotationOrderXYZ);
}

/* Import the dome light as a world material. */

void dome_light_to_world_material(const USDImportParams &params,
                                  const ImportSettings & /*settings*/,
                                  Scene *scene,
                                  Main *bmain,
                                  const pxr::UsdLuxDomeLight &dome_light,
                                  const double time)
{
  if (!(scene && scene->world && dome_light)) {
    return;
  }

  if (!scene->world->use_nodes) {
    scene->world->use_nodes = true;
  }

  if (!scene->world->nodetree) {
    scene->world->nodetree = bke::ntreeAddTree(NULL, "Shader Nodetree", "ShaderNodeTree");
    if (!scene->world->nodetree) {
      CLOG_WARN(&LOG, "Couldn't create world ntree");
      return;
    }
  }

  bNodeTree *ntree = scene->world->nodetree;
  bNode *output = nullptr;
  bNode *bgshader = nullptr;

  /* We never delete existing nodes, but we might disconnect them
   * and move them out of the way. */

  /* Look for the output and background shader nodes, which we will reuse. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type == SH_NODE_OUTPUT_WORLD) {
      output = node;
    }
    else if (node->type == SH_NODE_BACKGROUND) {
      bgshader = node;
    }
    else {
      /* Move existing node out of the way. */
      node->locy += 300;
    }
  }

  /* Create the output and background shader nodes, if they don't exist. */
  if (!output) {
    output = bke::nodeAddStaticNode(NULL, ntree, SH_NODE_OUTPUT_WORLD);

    if (!output) {
      CLOG_WARN(&LOG, "Couldn't create world output node");
      return;
    }

    output->locx = 300.0f;
    output->locy = 300.0f;
  }

  if (!bgshader) {
    bgshader = append_node(output, SH_NODE_BACKGROUND, "Background", "Surface", ntree, 200);

    if (!bgshader) {
      CLOG_WARN(&LOG, "Couldn't create world shader node");
      return;
    }

    /* Set the default background color. */
    bNodeSocket *color_sock = bke::nodeFindSocket(bgshader, SOCK_IN, "Color");
    copy_v3_v3(((bNodeSocketValueRGBA *)color_sock->default_value)->value, &scene->world->horr);
  }

  /* Make sure the first input to the shader node is disconnected. */
  bNodeSocket *shader_input = bke::nodeFindSocket(bgshader, SOCK_IN, "Color");

  if (shader_input && shader_input->link) {
    bke::nodeRemLink(ntree, shader_input->link);
  }

  /* Set the background shader intensity. */
  float intensity = 1.0f;
  get_authored_value(
      dome_light.GetIntensityAttr(), time, &intensity, dome_light.GetPrim(), usdtokens::intensity);

  intensity *= params.light_intensity_scale;

  bNodeSocket *strength_sock = bke::nodeFindSocket(bgshader, SOCK_IN, "Strength");
  ((bNodeSocketValueFloat *)strength_sock->default_value)->value = intensity;

  /* Get the dome light texture file and color. */
  pxr::SdfAssetPath tex_path;
  bool has_tex = get_authored_value(dome_light.GetTextureFileAttr(),
                                    time,
                                    &tex_path,
                                    dome_light.GetPrim(),
                                    usdtokens::texture_file);

  pxr::GfVec3f color;
  bool has_color = get_authored_value(
      dome_light.GetColorAttr(), time, &color, dome_light.GetPrim(), usdtokens::color);

  if (!has_tex) {
    /* No texture file is authored on the dome light.  Set the color, if it was authored,
     * and return early. */
    if (has_color) {
      bNodeSocket *color_sock = bke::nodeFindSocket(bgshader, SOCK_IN, "Color");
      copy_v3_v3(((bNodeSocketValueRGBA *)color_sock->default_value)->value, color.data());
    }

    bke::nodeSetActive(ntree, output);
    BKE_ntree_update_main_tree(bmain, ntree, nullptr);

    return;
  }

  /* If the light has authored color, create a color multiply node for the environment
   * texture output. */
  bNode *mult = nullptr;

  if (has_color) {
    mult = append_node(bgshader, SH_NODE_VECTOR_MATH, "Vector", "Color", ntree, 200);

    if (!mult) {
      CLOG_WARN(&LOG, "Couldn't create vector multiply node");
      return;
    }

    mult->custom1 = NODE_VECTOR_MATH_MULTIPLY;

    /* Set the color in the vector math node's second socket. */
    bNodeSocket *vec_sock = bke::nodeFindSocket(mult, SOCK_IN, "Vector");
    if (vec_sock) {
      vec_sock = vec_sock->next;
    }

    if (vec_sock) {
      copy_v3_v3(((bNodeSocketValueVector *)vec_sock->default_value)->value, color.data());
    }
    else {
      CLOG_WARN(&LOG, "Couldn't find vector multiply second vector socket");
    }
  }

  bNode *tex = nullptr;

  /* Append an environment texture node to the mult node, if it was created, or directly to
   * the background shader. */
  if (mult) {
    tex = append_node(mult, SH_NODE_TEX_ENVIRONMENT, "Color", "Vector", ntree, 400);
  }
  else {
    tex = append_node(bgshader, SH_NODE_TEX_ENVIRONMENT, "Color", "Color", ntree, 400);
  }

  if (!tex) {
    CLOG_WARN(&LOG, "Couldn't create world environment texture node");
    return;
  }

  bNode *mapping = append_node(tex, SH_NODE_MAPPING, "Vector", "Vector", ntree, 200);

  if (!mapping) {
    CLOG_WARN(&LOG, "Couldn't create mapping node");
    return;
  }

  bNode *tex_coord = append_node(mapping, SH_NODE_TEX_COORD, "Generated", "Vector", ntree, 200);

  if (!tex_coord) {
    CLOG_WARN(&LOG, "Couldn't create texture coordinate node");
    return;
  }

  /* Load the texture image. */
  std::string resolved_path = tex_path.GetResolvedPath();

  if (resolved_path.empty()) {
    CLOG_WARN(&LOG, "Couldn't get resolved path for asset %s", tex_path.GetAssetPath().c_str());
    return;
  }

  Image *image = load_image(resolved_path, bmain, params);
  if (!image) {
    CLOG_WARN(&LOG, "Couldn't load image file %s", resolved_path.c_str());
    return;
  }

  tex->id = &image->id;

  /* Set the transform. */
  pxr::UsdGeomXformCache xf_cache(time);
  pxr::GfMatrix4d xf = xf_cache.GetLocalToWorldTransform(dome_light.GetPrim());

  xf = pxr::GfMatrix4d().SetRotate(pxr::GfRotation(pxr::GfVec3d(0.0, 0.0, 1.0), -90.0)) *
       pxr::GfMatrix4d().SetRotate(pxr::GfRotation(pxr::GfVec3d(1.0, 0.0, 0.0), -90.0)) * xf;

  pxr::GfVec3d angles = xf.DecomposeRotation(
      pxr::GfVec3d::XAxis(), pxr::GfVec3d::YAxis(), pxr::GfVec3d::ZAxis());
  pxr::GfVec3f rot_vec(-angles[0], -angles[1], -angles[2]);

  /* Convert degrees to radians. */
  rot_vec *= M_PI / 180.0f;

  if (bNodeSocket *socket = bke::nodeFindSocket(mapping, SOCK_IN, "Rotation")) {
    bNodeSocketValueVector *rot_value = static_cast<bNodeSocketValueVector *>(
        socket->default_value);
    copy_v3_v3(rot_value->value, rot_vec.data());
  }

  bke::nodeSetActive(ntree, output);
  DEG_id_tag_update(&ntree->id, ID_RECALC_NTREE_OUTPUT);
  BKE_ntree_update_main_tree(bmain, ntree, nullptr);
}

}  // namespace blender::io::usd
