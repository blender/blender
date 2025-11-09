/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_light_convert.hh"

#include "usd.hh"
#include "usd_asset_utils.hh"
#include "usd_private.hh"
#include "usd_utils.hh"
#include "usd_writer_material.hh"

#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdGeom/xformCommonAPI.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/tokens.h>

#include "BKE_image.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"

#include "BLI_fileops.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "DNA_image_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include <cmath>
#include <cstdint>
#include <string>

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.usd"};

namespace usdtokens {
// Attribute values.
static const pxr::TfToken pole_axis_z("Z", pxr::TfToken::Immortal);
}  // namespace usdtokens

namespace {

struct WorldNtreeSearchPayload {
  const blender::io::usd::USDExportParams &params;
  pxr::UsdStageRefPtr stage;

  WorldNtreeSearchPayload(const blender::io::usd::USDExportParams &in_params,
                          pxr::UsdStageRefPtr in_stage)
      : params(in_params), stage(in_stage)
  {
  }
};

}  // End anonymous namespace.

namespace blender::io::usd {

/**
 * Load the image at the given path.  Handle packing and copying based in the import options.
 * Return the opened image on success or a nullptr on failure.
 */
static Image *load_image(std::string tex_path, Main *bmain, const USDImportParams &params)
{
  /* Optionally copy the asset if it's inside a USDZ package. */
  const bool import_textures = params.import_textures_mode != USD_TEX_IMPORT_NONE &&
                               should_import_asset(tex_path);

  std::string imported_file_source_path = tex_path;

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

    tex_path = import_asset(tex_path, textures_dir, name_collision_mode, nullptr);
  }

  Image *image = BKE_image_load_exists(bmain, tex_path.c_str());
  if (!image) {
    return nullptr;
  }

  if (import_textures && imported_file_source_path != tex_path) {
    ensure_usd_source_path_prop(imported_file_source_path, &image->id);
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
                          const StringRef out_sock,
                          const StringRef in_sock,
                          bNodeTree *ntree,
                          float offset)
{
  bNode *src_node = bke::node_add_static_node(nullptr, *ntree, new_node_type);
  bke::node_add_link(*ntree,
                     *src_node,
                     *bke::node_find_socket(*src_node, SOCK_OUT, out_sock),
                     *dst_node,
                     *bke::node_find_socket(*dst_node, SOCK_IN, in_sock));

  src_node->location[0] = dst_node->location[0] - offset;
  src_node->location[1] = dst_node->location[1];

  return src_node;
}

void world_material_to_dome_light(const USDExportParams &params,
                                  const Scene *scene,
                                  pxr::UsdStageRefPtr stage)
{
  if (!(stage && scene && scene->world)) {
    return;
  }

  WorldToDomeLight res;
  world_material_to_dome_light(scene, res);

  if (!(res.color_found || res.image)) {
    /* No nodes to convert */
    return;
  }

  std::string image_filepath;
  if (res.image) {
    /* Compute image filepath and export if needed. */
    image_filepath = get_tex_image_asset_filepath(res.image, stage, params);
    if (image_filepath.empty()) {
      return;
    }
    if (params.export_textures) {
      export_texture(res.image, stage, params.overwrite_textures);
    }
  }

  /* Create USD dome light. */
  pxr::SdfPath env_light_path = get_unique_path(stage, params.root_prim_path + "/env_light");
  pxr::UsdLuxDomeLight dome_light = pxr::UsdLuxDomeLight::Define(stage, env_light_path);

  if (res.image) {
    /* Use existing image texture file. */
    dome_light.CreateTextureFileAttr().Set(pxr::SdfAssetPath(image_filepath));

    /* Set optional color multiplication. */
    if (res.mult_found) {
      pxr::GfVec3f color_val(res.color_mult[0], res.color_mult[1], res.color_mult[2]);
      dome_light.CreateColorAttr().Set(color_val);
    }

    /* Set transform. */
    pxr::GfVec3d angles = res.transform.DecomposeRotation(
        pxr::GfVec3d::ZAxis(), pxr::GfVec3d::YAxis(), pxr::GfVec3d::XAxis());
    pxr::GfVec3f rot_vec(angles[2], angles[1], angles[0]);
    pxr::UsdGeomXformCommonAPI xform_api(dome_light);
    xform_api.SetRotate(rot_vec, pxr::UsdGeomXformCommonAPI::RotationOrderXYZ);
  }
  else if (res.color_found) {
    /* If no texture is found export a solid color texture as a stand-in so that Hydra
     * renderers don't throw errors. */
    dome_light.CreateIntensityAttr().Set(res.intensity);

    std::string source_path = cache_image_color(res.color);
    const std::string base_path = stage->GetRootLayer()->GetRealPath();

    char file_name[FILE_MAX];
    BLI_path_split_file_part(source_path.c_str(), file_name, FILE_MAX);
    char dest_path[FILE_MAX];
    BLI_path_split_dir_part(base_path.c_str(), dest_path, FILE_MAX);

    BLI_path_append_dir(dest_path, FILE_MAX, "textures");
    BLI_dir_create_recursive(dest_path);

    BLI_path_append(dest_path, FILE_MAX, file_name);

    if (BLI_copy(source_path.c_str(), dest_path) != 0) {
      CLOG_WARN(&LOG, "USD Export: Couldn't write world color image to %s", dest_path);
    }
    else {
      BLI_path_join(dest_path, FILE_MAX, ".", "textures", file_name);
      BLI_string_replace_char(dest_path, '\\', '/');
      dome_light.CreateTextureFileAttr().Set(pxr::SdfAssetPath(dest_path));
    }
  }
}

/* Import the dome light as a world material. */

void dome_light_to_world_material(const USDImportParams &params,
                                  Scene *scene,
                                  Main *bmain,
                                  const USDImportDomeLightData &dome_light_data,
                                  const pxr::UsdPrim &prim,
                                  const pxr::UsdTimeCode time)
{
  if (!(scene && scene->world && prim)) {
    return;
  }

  if (!scene->world->nodetree) {
    scene->world->nodetree = bke::node_tree_add_tree_embedded(
        nullptr, &scene->world->id, "Shader Nodetree", "ShaderNodeTree");
  }

  bNodeTree *ntree = scene->world->nodetree;
  bNode *output = nullptr;
  bNode *bgshader = nullptr;

  /* We never delete existing nodes, but we might disconnect them
   * and move them out of the way. */

  /* Look for the output and background shader nodes, which we will reuse. */
  for (bNode *node : ntree->all_nodes()) {
    if (node->type_legacy == SH_NODE_OUTPUT_WORLD) {
      output = node;
    }
    else if (node->type_legacy == SH_NODE_BACKGROUND) {
      bgshader = node;
    }
    else {
      /* Move existing node out of the way. */
      node->location[1] += 300;
    }
  }

  /* Create the output and background shader nodes, if they don't exist. */
  if (!output) {
    output = bke::node_add_static_node(nullptr, *ntree, SH_NODE_OUTPUT_WORLD);
    output->location[0] = 300.0f;
    output->location[1] = 300.0f;
  }

  if (!bgshader) {
    bgshader = append_node(output, SH_NODE_BACKGROUND, "Background", "Surface", ntree, 200);

    /* Set the default background color. */
    bNodeSocket *color_sock = bke::node_find_socket(*bgshader, SOCK_IN, "Color");
    copy_v3_v3(((bNodeSocketValueRGBA *)color_sock->default_value)->value, &scene->world->horr);
  }

  /* Make sure the first input to the shader node is disconnected. */
  bNodeSocket *shader_input = bke::node_find_socket(*bgshader, SOCK_IN, "Color");

  if (shader_input && shader_input->link) {
    bke::node_remove_link(ntree, *shader_input->link);
  }

  /* Set the background shader intensity. */
  float intensity = dome_light_data.intensity * params.light_intensity_scale;

  bNodeSocket *strength_sock = bke::node_find_socket(*bgshader, SOCK_IN, "Strength");
  ((bNodeSocketValueFloat *)strength_sock->default_value)->value = intensity;

  if (!dome_light_data.has_tex) {
    /* No texture file is authored on the dome light.  Set the color, if it was authored,
     * and return early. */
    if (dome_light_data.has_color) {
      bNodeSocket *color_sock = bke::node_find_socket(*bgshader, SOCK_IN, "Color");
      copy_v3_v3(((bNodeSocketValueRGBA *)color_sock->default_value)->value,
                 dome_light_data.color.data());
    }

    bke::node_set_active(*ntree, *output);
    BKE_ntree_update_after_single_tree_change(*bmain, *ntree);

    return;
  }

  /* If the light has authored color, create a color multiply node for the environment
   * texture output. */
  bNode *mult = nullptr;

  if (dome_light_data.has_color) {
    mult = append_node(bgshader, SH_NODE_VECTOR_MATH, "Vector", "Color", ntree, 200);
    mult->custom1 = NODE_VECTOR_MATH_MULTIPLY;

    /* Set the color in the vector math node's second socket. */
    bNodeSocket *vec_sock = bke::node_find_socket(*mult, SOCK_IN, "Vector");
    if (vec_sock) {
      vec_sock = vec_sock->next;
    }

    if (vec_sock) {
      copy_v3_v3(((bNodeSocketValueVector *)vec_sock->default_value)->value,
                 dome_light_data.color.data());
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

  bNode *mapping = append_node(tex, SH_NODE_MAPPING, "Vector", "Vector", ntree, 200);

  append_node(mapping, SH_NODE_TEX_COORD, "Generated", "Vector", ntree, 200);

  /* Load the texture image. */
  const std::string &resolved_path = dome_light_data.tex_path.GetResolvedPath();
  if (resolved_path.empty()) {
    CLOG_WARN(&LOG,
              "Couldn't get resolved path for asset %s",
              dome_light_data.tex_path.GetAssetPath().c_str());
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
  pxr::GfMatrix4d xf = xf_cache.GetLocalToWorldTransform(prim);

  pxr::UsdStageRefPtr stage = prim.GetStage();

  if (!stage) {
    CLOG_WARN(&LOG, "Couldn't get stage for dome light %s", prim.GetPath().GetText());
    return;
  }

  /* Note: This logic tries to produce identical results to `usdview` as of USD 25.05.
   * However, `usdview` seems to handle Y-Up stages differently; some scenes match while others
   * do not unless we keep the second conditional below (+90 on x-axis). */
  const pxr::TfToken stage_up = pxr::UsdGeomGetStageUpAxis(stage);
  const bool needs_stage_z_adjust = stage_up == pxr::UsdGeomTokens->z &&
                                    ELEM(dome_light_data.pole_axis,
                                         pxr::UsdLuxTokens->Z,
                                         pxr::UsdLuxTokens->scene);
  const bool needs_stage_y_adjust = stage_up == pxr::UsdGeomTokens->y &&
                                    ELEM(dome_light_data.pole_axis, pxr::UsdLuxTokens->Z);
  if (needs_stage_z_adjust || needs_stage_y_adjust) {
    xf *= pxr::GfMatrix4d().SetRotate(pxr::GfRotation(pxr::GfVec3d(0.0, 1.0, 0.0), 90.0));
  }
  else if (stage_up == pxr::UsdGeomTokens->y) {
    /* Convert from Y-up to Z-up with a 90 degree rotation about the X-axis. */
    xf *= pxr::GfMatrix4d().SetRotate(pxr::GfRotation(pxr::GfVec3d(1.0, 0.0, 0.0), 90.0));
  }

  /* Rotate into Blender's frame of reference. */
  xf = pxr::GfMatrix4d().SetRotate(pxr::GfRotation(pxr::GfVec3d(0.0, 0.0, 1.0), -90.0)) *
       pxr::GfMatrix4d().SetRotate(pxr::GfRotation(pxr::GfVec3d(1.0, 0.0, 0.0), -90.0)) * xf;

  pxr::GfVec3d angles = xf.DecomposeRotation(
      pxr::GfVec3d::XAxis(), pxr::GfVec3d::YAxis(), pxr::GfVec3d::ZAxis());
  pxr::GfVec3f rot_vec(-angles[0], -angles[1], -angles[2]);

  /* Convert degrees to radians. */
  rot_vec *= M_PI / 180.0f;

  if (bNodeSocket *socket = bke::node_find_socket(*mapping, SOCK_IN, "Rotation")) {
    bNodeSocketValueVector *rot_value = static_cast<bNodeSocketValueVector *>(
        socket->default_value);
    copy_v3_v3(rot_value->value, rot_vec.data());
  }

  bke::node_set_active(*ntree, *output);
  DEG_id_tag_update(&ntree->id, ID_RECALC_NTREE_OUTPUT);
  BKE_ntree_update_after_single_tree_change(*bmain, *ntree);
}

static bool node_search(bNode *fromnode, bNode * /*tonode*/, void *userdata, bool /*reversed*/)
{
  if (!(userdata && fromnode)) {
    return true;
  }

  WorldToDomeLight &res = *static_cast<WorldToDomeLight *>(userdata);

  if (!res.color_found && fromnode->type_legacy == SH_NODE_BACKGROUND) {
    /* Get light color and intensity */
    const bNodeSocketValueRGBA *color_data = bke::node_find_socket(*fromnode, SOCK_IN, "Color")
                                                 ->default_value_typed<bNodeSocketValueRGBA>();
    const bNodeSocketValueFloat *strength_data =
        bke::node_find_socket(*fromnode, SOCK_IN, "Strength")
            ->default_value_typed<bNodeSocketValueFloat>();

    res.color_found = true;
    res.intensity = strength_data->value;
    res.color[0] = color_data->value[0];
    res.color[1] = color_data->value[1];
    res.color[2] = color_data->value[2];
    res.color[3] = 1.0f;
  }
  else if (!res.image && fromnode->type_legacy == SH_NODE_TEX_ENVIRONMENT) {
    NodeTexImage *tex = static_cast<NodeTexImage *>(fromnode->storage);
    res.image = reinterpret_cast<Image *>(fromnode->id);
    res.iuser = &tex->iuser;
  }
  else if (!res.image && !res.mult_found && fromnode->type_legacy == SH_NODE_VECTOR_MATH) {
    if (fromnode->custom1 == NODE_VECTOR_MATH_MULTIPLY) {
      res.mult_found = true;

      bNodeSocket *vec_sock = bke::node_find_socket(*fromnode, SOCK_IN, "Vector");
      if (vec_sock) {
        vec_sock = vec_sock->next;
      }

      if (vec_sock) {
        copy_v3_v3(res.color_mult, ((bNodeSocketValueVector *)vec_sock->default_value)->value);
      }
    }
  }
  else if (res.image && fromnode->type_legacy == SH_NODE_MAPPING) {
    if (bNodeSocket *socket = bke::node_find_socket(*fromnode, SOCK_IN, "Rotation")) {
      const bNodeSocketValueVector *rot_value = static_cast<bNodeSocketValueVector *>(
          socket->default_value);
      /* Convert radians to degrees. */
      pxr::GfVec3f rot(rot_value->value);
      mul_v3_fl(rot.data(), 180.0f / M_PI);
      res.transform =
          pxr::GfMatrix4d().SetRotate(pxr::GfRotation(pxr::GfVec3d(1.0, 0.0, 0.0), 90.0)) *
          pxr::GfMatrix4d().SetRotate(pxr::GfRotation(pxr::GfVec3d(0.0, 0.0, 1.0), 90.0)) *
          pxr::GfMatrix4d().SetRotate(pxr::GfRotation(pxr::GfVec3d(0.0, 0.0, 1.0), -rot[2])) *
          pxr::GfMatrix4d().SetRotate(pxr::GfRotation(pxr::GfVec3d(0.0, 1.0, 0.0), -rot[1])) *
          pxr::GfMatrix4d().SetRotate(pxr::GfRotation(pxr::GfVec3d(1.0, 0.0, 0.0), -rot[0]));
    }
  }
  return true;
}

void world_material_to_dome_light(const Scene *scene, WorldToDomeLight &res)
{
  /* Find the world output. */
  scene->world->nodetree->ensure_topology_cache();
  const Span<const bNode *> bsdf_nodes = scene->world->nodetree->nodes_by_type(
      "ShaderNodeOutputWorld");

  for (const bNode *node : bsdf_nodes) {
    if (node->flag & NODE_DO_OUTPUT) {
      bke::node_chain_iterator(scene->world->nodetree, node, node_search, &res, true);
      break;
    }
  }
}

}  // namespace blender::io::usd
