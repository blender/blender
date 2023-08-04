/* SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation */

#include "world.h"

#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/vt/array.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/usdLux/tokens.h>

#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BLI_math_rotation.h"
#include "BLI_path_util.h"

#include "BKE_node.h"
#include "BKE_node_runtime.hh"
#include "BKE_studiolight.h"

#include "NOD_shader.h"

#include "hydra_scene_delegate.h"
#include "image.h"

/* TODO : add custom tftoken "transparency"? */

/* NOTE: opacity and blur aren't supported by USD */

namespace blender::io::hydra {

WorldData::WorldData(HydraSceneDelegate *scene_delegate, pxr::SdfPath const &prim_id)
    : LightData(scene_delegate, nullptr, prim_id)
{
  prim_type_ = pxr::HdPrimTypeTokens->domeLight;
}

void WorldData::init()
{
  data_.clear();
  data_[pxr::UsdLuxTokens->orientToStageUpAxis] = true;

  float intensity = 1.0f;
  float exposure = 1.0f;
  pxr::GfVec3f color(1.0f, 1.0f, 1.0f);
  pxr::SdfAssetPath texture_file;

  if (scene_delegate_->shading_settings.use_scene_world) {
    World *world = scene_delegate_->scene->world;
    ID_LOG(1, "%s", world->id.name);

    exposure = world->exposure;
    if (world->use_nodes) {
      /* TODO: Create nodes parsing system */

      bNode *output_node = ntreeShaderOutputNode(world->nodetree, SHD_OUTPUT_ALL);
      blender::Span<bNodeSocket *> input_sockets = output_node->input_sockets();
      bNodeSocket *input_socket = nullptr;

      for (auto socket : input_sockets) {
        if (STREQ(socket->name, "Surface")) {
          input_socket = socket;
          break;
        }
      }
      if (!input_socket) {
        return;
      }
      bNodeLink const *link = input_socket->directly_linked_links()[0];
      if (input_socket->directly_linked_links().is_empty()) {
        return;
      }

      bNode *input_node = link->fromnode;
      if (input_node->type != SH_NODE_BACKGROUND) {
        return;
      }

      const bNodeSocket &color_input = input_node->input_by_identifier("Color");
      const bNodeSocket &strength_input = input_node->input_by_identifier("Strength");

      float const *strength = strength_input.default_value_typed<float>();
      float const *input_color = color_input.default_value_typed<float>();
      intensity = strength[1];
      color = pxr::GfVec3f(input_color[0], input_color[1], input_color[2]);

      if (!color_input.directly_linked_links().is_empty()) {
        bNode *color_input_node = color_input.directly_linked_links()[0]->fromnode;
        if (ELEM(color_input_node->type, SH_NODE_TEX_IMAGE, SH_NODE_TEX_ENVIRONMENT)) {
          NodeTexImage *tex = static_cast<NodeTexImage *>(color_input_node->storage);
          Image *image = (Image *)color_input_node->id;
          if (image) {
            std::string image_path = cache_or_get_image_file(
                scene_delegate_->bmain, scene_delegate_->scene, image, &tex->iuser);
            if (!image_path.empty()) {
              texture_file = pxr::SdfAssetPath(image_path, image_path);
            }
          }
        }
      }
    }
    else {
      intensity = 1.0f;
      color = pxr::GfVec3f(world->horr, world->horg, world->horb);
    }

    if (texture_file.GetAssetPath().empty()) {
      float fill_color[4] = {color[0], color[1], color[2], 1.0f};
      std::string image_path = cache_image_color(fill_color);
      texture_file = pxr::SdfAssetPath(image_path, image_path);
    }
  }
  else {
    ID_LOG(1, "studiolight: %s", scene_delegate_->shading_settings.studiolight_name.c_str());

    StudioLight *sl = BKE_studiolight_find(
        scene_delegate_->shading_settings.studiolight_name.c_str(),
        STUDIOLIGHT_ORIENTATIONS_MATERIAL_MODE);
    if (sl != NULL && sl->flag & STUDIOLIGHT_TYPE_WORLD) {
      texture_file = pxr::SdfAssetPath(sl->filepath, sl->filepath);
      /* coefficient to follow Cycles result */
      intensity = scene_delegate_->shading_settings.studiolight_intensity / 2;
    }
  }

  data_[pxr::HdLightTokens->intensity] = intensity;
  data_[pxr::HdLightTokens->exposure] = exposure;
  data_[pxr::HdLightTokens->color] = color;
  data_[pxr::HdLightTokens->textureFile] = texture_file;

  write_transform();
}

void WorldData::update()
{
  ID_LOG(1, "");
  init();
  scene_delegate_->GetRenderIndex().GetChangeTracker().MarkSprimDirty(prim_id,
                                                                      pxr::HdLight::AllDirty);
}

void WorldData::write_transform()
{
  transform = pxr::GfMatrix4d().SetRotate(pxr::GfRotation(pxr::GfVec3d(1.0, 0.0, 0.0), 90.0)) *
              pxr::GfMatrix4d().SetRotate(pxr::GfRotation(pxr::GfVec3d(0.0, 0.0, 1.0), 90.0));
  if (!scene_delegate_->shading_settings.use_scene_world) {
    transform *= pxr::GfMatrix4d().SetRotate(
        pxr::GfRotation(pxr::GfVec3d(0.0, 0.0, -1.0),
                        RAD2DEGF(scene_delegate_->shading_settings.studiolight_rotation)));
  }
}

}  // namespace blender::io::hydra
