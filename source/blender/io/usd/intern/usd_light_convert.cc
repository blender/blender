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
 *
 * The Original Code is Copyright (C) 2021 NVIDIA Corporation.
 * All rights reserved.
 */

#include "usd_light_convert.h"

#include "usd.hh"
#include "usd_asset_utils.hh"
#include "usd_reader_prim.hh"
#include "usd_writer_material.hh"

#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/vec3f.h>
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

#include <iostream>
#include <string>

namespace usdtokens {
// Attribute names.
static const pxr::TfToken color("color", pxr::TfToken::Immortal);
static const pxr::TfToken intensity("intensity", pxr::TfToken::Immortal);
static const pxr::TfToken texture_file("texture:file", pxr::TfToken::Immortal);
}  // namespace usdtokens

namespace {

template<typename T>
bool get_authored_value(const pxr::UsdAttribute attr, const double motionSampleTime, T *r_value)
{
  if (attr && attr.HasAuthoredValue()) {
    return attr.Get<T>(r_value, motionSampleTime);
  }

  return false;
}

struct WorldNtreeSearchResults {
  const blender::io::usd::USDExportParams params;
  pxr::UsdStageRefPtr stage;

  float world_color[3];
  float world_intensity;
  float tex_rot[3];

  std::string file_path;

  float color_mult[3];

  bool background_found;
  bool env_tex_found;
  bool mult_found;

  WorldNtreeSearchResults(const blender::io::usd::USDExportParams &in_params, pxr::UsdStageRefPtr in_stage)
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

static const float nits_to_watts_per_meter_sq = 0.0014641f;

static const float watts_per_meter_sq_to_nits = 1.0f / nits_to_watts_per_meter_sq;

static bool node_search(bNode *fromnode, bNode *tonode, void *userdata, const bool reversed)
{
  if (!(userdata && fromnode && tonode)) {
    return true;
  }

  /* TODO(makowalski): can we validate that node connectiona are correct? */

  WorldNtreeSearchResults *res = reinterpret_cast<WorldNtreeSearchResults *>(userdata);

  if (!res->background_found && ELEM(fromnode->type, SH_NODE_BACKGROUND)) {
    /* Get light color and intensity */
    bNodeSocketValueRGBA *color_data =
        (bNodeSocketValueRGBA *)((bNodeSocket *)BLI_findlink(&fromnode->inputs, 0))->default_value;
    bNodeSocketValueFloat *strength_data = (bNodeSocketValueFloat *)((bNodeSocket *)BLI_findlink(
                                                                         &fromnode->inputs, 1))
                                               ->default_value;

    res->background_found = true;
    res->world_intensity = strength_data->value;
    res->world_color[0] = color_data->value[0];
    res->world_color[1] = color_data->value[1];
    res->world_color[2] = color_data->value[2];
  }
  else if (!res->env_tex_found && ELEM(fromnode->type, SH_NODE_TEX_ENVIRONMENT)) {
    /* Get env tex path. */

    res->file_path = get_tex_image_asset_filepath(fromnode, res->stage, res->params);

    if (!res->file_path.empty()) {
      /* Get the rotation. */
      NodeTexEnvironment *tex = static_cast<NodeTexEnvironment *>(fromnode->storage);
      copy_v3_v3(res->tex_rot, tex->base.tex_mapping.rot);

      res->env_tex_found = true;

      if (res->params.export_textures) {
        export_texture(fromnode, res->stage, res->params.overwrite_textures);
      }
    }
  }
  else if (!res->env_tex_found && !res->mult_found && ELEM(fromnode->type, SH_NODE_VECTOR_MATH)) {

    if (fromnode->custom1 == NODE_VECTOR_MATH_MULTIPLY) {
      res->mult_found = true;

      bNodeSocket *vec_sock = nodeFindSocket(fromnode, SOCK_IN, "Vector");
      if (vec_sock) {
        vec_sock = vec_sock->next;
      }

      if (vec_sock) {
        copy_v3_v3(res->color_mult, ((bNodeSocketValueVector *)vec_sock->default_value)->value);
      }
    }
  }

  return true;
}

/* Return the scale factor to convert nits to light energy
 * (Watts or Watts per meter squared) for the given light. */
float nits_to_energy_scale_factor(const Light *light,
                                  const float meters_per_unit,
                                  const float radius_scale)
{
  if (!light) {
    return 1.0f;
  }

  /* Compute meters per unit squared. */
  const float mpu_sq = meters_per_unit * meters_per_unit;

  float scale = nits_to_watts_per_meter_sq;

  /* Scale by the light surface area, for lights other than sun. */
  switch (light->type) {
    case LA_AREA:
      switch (light->area_shape) {
        case LA_AREA_DISK:
        case LA_AREA_ELLIPSE: { /* An ellipse light will deteriorate into a disk light. */
          float r = light->area_size / 2.0f;
          scale *= 2.0f * M_PI * (r * r) * mpu_sq;
          break;
        }
        case LA_AREA_RECT: {
          scale *= light->area_size * light->area_sizey * mpu_sq;
          break;
        }
        case LA_AREA_SQUARE: {
          scale *= light->area_size * light->area_size * mpu_sq;
          break;
        }
      }
      break;
    case LA_LOCAL: {
      float r = light->radius * radius_scale;
      scale *= 4.0f * M_PI * (r * r) * mpu_sq;
      break;
    }
    case LA_SPOT: {
      float r = light->radius * radius_scale;
      float angle = light->spotsize / 2.0f;
      scale *= 2.0f * M_PI * (r * r) * (1.0f - cosf(angle)) * mpu_sq;
      break;
    }
    case LA_SUN: {
      /* Sun energy is Watts per square meter so we don't scale by area. */
      break;
    }
    default:
      break;
  }

  if (scale < .000001) {
    scale = .000001;
  }
  return scale;
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

  pxr::SdfPath light_path(std::string(params.root_prim_path) + "/lights");

  usd_define_or_over<pxr::UsdGeomScope>(stage, light_path, params.export_as_overs);

  WorldNtreeSearchResults res(params, stage);

  blender::bke::nodeChainIter(scene->world->nodetree, output, node_search, &res, true);

  if (!(res.background_found || res.env_tex_found)) {
    /* No nodes to convert */
    return;
  }

  /* Create USD dome light. */

  pxr::SdfPath env_light_path = light_path.AppendChild(pxr::TfToken("environment"));

  pxr::UsdLuxDomeLight dome_light = usd_define_or_over<pxr::UsdLuxDomeLight>(
      stage, env_light_path, params.export_as_overs);

  if (res.env_tex_found) {

    /* Convert radians to degrees. */
    mul_v3_fl(res.tex_rot, 180.0f / M_PI);

    /* Note the negative Z rotation with 180 deg offset, to match Create and Maya. */
    pxr::GfVec3f rot(-res.tex_rot[0], -res.tex_rot[1], -res.tex_rot[2] - 180.0f);

    pxr::UsdGeomXformCommonAPI xform_api(dome_light);

    /* We reverse the rotation order to convert between extrinsic and intrinsic euler angles. */
    xform_api.SetRotate(rot, pxr::UsdGeomXformCommonAPI::RotationOrderZYX);

    pxr::SdfAssetPath path(res.file_path);
    dome_light.CreateTextureFileAttr().Set(path);

    if (params.backward_compatible) {
      pxr::UsdAttribute attr = dome_light.GetPrim().CreateAttribute(
          usdtokens::texture_file, pxr::SdfValueTypeNames->Asset, true);
      if (attr) {
        attr.Set(path);
      }
    }

    if (res.mult_found) {
      pxr::GfVec3f color_val(res.color_mult[0], res.color_mult[1], res.color_mult[2]);
      dome_light.CreateColorAttr().Set(color_val);

      if (params.backward_compatible) {
        pxr::UsdAttribute attr = dome_light.GetPrim().CreateAttribute(
            usdtokens::color, pxr::SdfValueTypeNames->Color3f, true);
        if (attr) {
          attr.Set(color_val);
        }
      }
    }
  }
  else {
    pxr::GfVec3f color_val(res.world_color[0], res.world_color[1], res.world_color[2]);
    dome_light.CreateColorAttr().Set(color_val);

    if (params.backward_compatible) {
      pxr::UsdAttribute attr = dome_light.GetPrim().CreateAttribute(
          usdtokens::color, pxr::SdfValueTypeNames->Color3f, true);
      if (attr) {
        attr.Set(color_val);
      }
    }
  }

  if (res.background_found) {
    float usd_intensity = res.world_intensity * params.light_intensity_scale;

    if (params.convert_light_to_nits) {
      usd_intensity *= watts_per_meter_sq_to_nits;
    }

    dome_light.CreateIntensityAttr().Set(usd_intensity);

    if (params.backward_compatible) {
      pxr::UsdAttribute attr = dome_light.GetPrim().CreateAttribute(
          usdtokens::intensity, pxr::SdfValueTypeNames->Float, true);
      if (attr) {
        attr.Set(usd_intensity);
      }
    }
  }
}

/* Import the dome light as a world material. */

void dome_light_to_world_material(const USDImportParams &params,
                                  const ImportSettings &settings,
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
    scene->world->nodetree = ntreeAddTree(NULL, "Shader Nodetree", "ShaderNodeTree");
    if (!scene->world->nodetree) {
      std::cerr << "WARNING: couldn't create world ntree.\n";
      return;
    }
  }

  bNodeTree *ntree = scene->world->nodetree;
  bNode *output = nullptr;
  bNode *shader = nullptr;

  /* We never delete existing nodes, but we might disconnect them
   * and move them out of the way. */

  /* Look for the output and background shader nodes, which we will reuse.
   * TODO(makowalski): add logic to properly verify node connections. */
  for (bNode *node = static_cast<bNode *>(ntree->nodes.first); node; node = node->next) {
    if (ELEM(node->type, SH_NODE_OUTPUT_WORLD)) {
      output = node;
    }
    else if (ELEM(node->type, SH_NODE_BACKGROUND)) {
      shader = node;
    }
    else {
      /* Move node out of the way. */
      node->locy += 300;
    }
  }

  /* Create the output and shader nodes, if they don't exist. */
  if (!output) {
    output = nodeAddStaticNode(NULL, ntree, SH_NODE_OUTPUT_WORLD);

    if (!output) {
      std::cerr << "WARNING: couldn't create world output node.\n";
      return;
    }

    output->locx = 300.0f;
    output->locy = 300.0f;
  }

  if (!shader) {
    shader = nodeAddStaticNode(NULL, ntree, SH_NODE_BACKGROUND);

    if (!shader) {
      std::cerr << "WARNING: couldn't create world shader node.\n";
      return;
    }

    nodeAddLink(scene->world->nodetree,
                shader,
                nodeFindSocket(shader, SOCK_OUT, "Background"),
                output,
                nodeFindSocket(output, SOCK_IN, "Surface"));

    bNodeSocket *color_sock = nodeFindSocket(shader, SOCK_IN, "Color");
    copy_v3_v3(((bNodeSocketValueRGBA *)color_sock->default_value)->value, &scene->world->horr);

    shader->locx = output->locx - 200;
    shader->locy = output->locy;
  }

  /* Make sure the first input to the shader node is disconnected. */
  bNodeSocket *shader_input = static_cast<bNodeSocket *>(BLI_findlink(&shader->inputs, 0));

  if (shader_input && shader_input->link) {
    nodeRemLink(ntree, shader_input->link);
  }

  pxr::UsdAttribute intensity_attr = dome_light.GetIntensityAttr();

  float intensity = 1.0f;
  intensity_attr.Get(&intensity, time);

  if (!get_authored_value(dome_light.GetIntensityAttr(), time, &intensity)) {
    dome_light.GetPrim().GetAttribute(usdtokens::intensity).Get(&intensity, time);
  }

  intensity *= params.light_intensity_scale;

  if (params.convert_light_from_nits) {
    intensity *= nits_to_watts_per_meter_sq;
  }

  bNodeSocket *strength_sock = nodeFindSocket(shader, SOCK_IN, "Strength");
  ((bNodeSocketValueFloat *)strength_sock->default_value)->value = intensity;

  pxr::SdfAssetPath tex_path;
  bool has_tex = get_authored_value(dome_light.GetTextureFileAttr(), time, &tex_path);

  if (!has_tex) {
    has_tex = dome_light.GetPrim().GetAttribute(usdtokens::texture_file).Get(&tex_path, time);
  }

  pxr::GfVec3f color;
  bool has_color = get_authored_value(dome_light.GetColorAttr(), time, &color);

  if (!has_color) {
    has_color = dome_light.GetPrim().GetAttribute(usdtokens::color).Get(&color, time);
  }

  if (!has_tex) {
    if (has_color) {
      bNodeSocket *color_sock = nodeFindSocket(shader, SOCK_IN, "Color");
      copy_v3_v3(((bNodeSocketValueRGBA *)color_sock->default_value)->value, color.data());
    }

    nodeSetActive(ntree, output);
    BKE_ntree_update_main_tree(bmain, ntree, nullptr);

    return;
  }

  /* If the light has authored color, create the color multiply for the env texture output. */
  bNode *mult = nullptr;

  if (has_color) {
    mult = nodeAddStaticNode(NULL, ntree, SH_NODE_VECTOR_MATH);

    if (!mult) {
      std::cerr << "WARNING: couldn't create vector multiply node.\n";
      return;
    }

    nodeAddLink(scene->world->nodetree,
                mult,
                nodeFindSocket(mult, SOCK_OUT, "Vector"),
                shader,
                nodeFindSocket(shader, SOCK_IN, "Color"));

    mult->locx = shader->locx - 200;
    mult->locy = shader->locy;

    mult->custom1 = NODE_VECTOR_MATH_MULTIPLY;

    bNodeSocket *vec_sock = nodeFindSocket(mult, SOCK_IN, "Vector");
    if (vec_sock) {
      vec_sock = vec_sock->next;
    }

    if (vec_sock) {
      copy_v3_v3(((bNodeSocketValueVector *)vec_sock->default_value)->value, color.data());
    }
    else {
      std::cout << "ERROR: couldn't find vector multiply second vector input.\n";
    }
  }

  bNode *tex = nodeAddStaticNode(NULL, ntree, SH_NODE_TEX_ENVIRONMENT);

  if (!tex) {
    std::cerr << "WARNING: couldn't create world environment texture node.\n";
    return;
  }

  if (mult) {
    nodeAddLink(scene->world->nodetree,
                tex,
                nodeFindSocket(tex, SOCK_OUT, "Color"),
                mult,
                nodeFindSocket(mult, SOCK_IN, "Vector"));

    tex->locx = mult->locx - 400;
    tex->locy = mult->locy;
  }
  else {
    nodeAddLink(scene->world->nodetree,
                tex,
                nodeFindSocket(tex, SOCK_OUT, "Color"),
                shader,
                nodeFindSocket(shader, SOCK_IN, "Color"));

    tex->locx = shader->locx - 400;
    tex->locy = shader->locy;
  }

  std::string tex_path_str = tex_path.GetResolvedPath();

  if (tex_path_str.empty()) {
    std::cerr << "WARNING: Couldn't get resolved path for asset " << tex_path
              << " for Texture Image node.\n";
    return;
  }

  /* Optionally copy the asset if it's inside a USDZ package or has a URI. */

  const bool import_textures = params.import_textures_mode != USD_TEX_IMPORT_NONE &&
                               should_import_asset(tex_path_str);

  std::string imported_file_source_path;
  if (import_textures) {
    imported_file_source_path = tex_path_str;

    /* If we are packing the imported textures, we first write them
     * to a temporary directory. */
    const char *textures_dir = params.import_textures_mode == USD_TEX_IMPORT_PACK ?
                                   temp_textures_dir() :
                                   params.import_textures_dir;

    const eUSDTexNameCollisionMode name_collision_mode = params.import_textures_mode ==
                                                                 USD_TEX_IMPORT_PACK ?
                                                             USD_TEX_NAME_COLLISION_OVERWRITE :
                                                             params.tex_name_collision_mode;

    tex_path_str = import_asset(tex_path_str.c_str(), textures_dir, name_collision_mode, nullptr);
  }

  Image *image = BKE_image_load_exists(bmain, tex_path_str.c_str());
  if (!image) {
    std::cerr << "WARNING: Couldn't open image file '" << tex_path_str
              << "' for Texture Image node.\n";
    return;
  }

  tex->id = &image->id;

  if (import_textures && imported_file_source_path != tex_path_str) {
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

  /* Set the transform. */
  pxr::UsdGeomXformCache xf_cache(time);

  pxr::GfMatrix4d xf = xf_cache.GetLocalToWorldTransform(dome_light.GetPrim());

  if (settings.do_convert_mat) {
    /* Apply matrix for z-up conversion. */
    pxr::GfMatrix4d convert_xf(pxr::GfMatrix4f(settings.conversion_mat));
    xf *= convert_xf;
  }

  pxr::GfRotation rot = xf.ExtractRotation();

  pxr::GfVec3d rot_vec = rot.Decompose(
      pxr::GfVec3d::XAxis(), pxr::GfVec3d::YAxis(), pxr::GfVec3d::ZAxis());

  NodeTexEnvironment *tex_env = static_cast<NodeTexEnvironment *>(tex->storage);
  tex_env->base.tex_mapping.rot[0] = -static_cast<float>(rot_vec[0]);
  tex_env->base.tex_mapping.rot[1] = -static_cast<float>(rot_vec[1]);
  tex_env->base.tex_mapping.rot[2] = 180 - static_cast<float>(rot_vec[2]);

  /* Convert radians to degrees. */
  mul_v3_fl(tex_env->base.tex_mapping.rot, M_PI / 180.0f);

  eul_to_mat4(tex_env->base.tex_mapping.mat, tex_env->base.tex_mapping.rot);

  nodeSetActive(ntree, output);
  BKE_ntree_update_main_tree(bmain, ntree, nullptr);
}

}  // namespace blender::io::usd
