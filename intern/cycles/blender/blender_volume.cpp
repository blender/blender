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
#include "render/mesh.h"
#include "render/object.h"

#include "blender/blender_sync.h"
#include "blender/blender_util.h"

CCL_NAMESPACE_BEGIN

static void sync_smoke_volume(Scene *scene, BL::Object &b_ob, Mesh *mesh, float frame)
{
  BL::FluidDomainSettings b_domain = object_fluid_gas_domain_find(b_ob);
  if (!b_domain) {
    return;
  }

  ImageManager *image_manager = scene->image_manager;
  AttributeStandard attributes[] = {ATTR_STD_VOLUME_DENSITY,
                                    ATTR_STD_VOLUME_COLOR,
                                    ATTR_STD_VOLUME_FLAME,
                                    ATTR_STD_VOLUME_HEAT,
                                    ATTR_STD_VOLUME_TEMPERATURE,
                                    ATTR_STD_VOLUME_VELOCITY,
                                    ATTR_STD_NONE};

  for (int i = 0; attributes[i] != ATTR_STD_NONE; i++) {
    AttributeStandard std = attributes[i];
    if (!mesh->need_attribute(scene, std)) {
      continue;
    }

    mesh->volume_isovalue = b_domain.clipping();

    Attribute *attr = mesh->attributes.add(std);
    VoxelAttribute *volume_data = attr->data_voxel();
    ImageMetaData metadata;

    ImageKey key;
    key.filename = Attribute::standard_name(std);
    key.builtin_data = b_ob.ptr.data;

    volume_data->manager = image_manager;
    volume_data->slot = image_manager->add_image(key, frame, metadata);
  }

  /* Create a matrix to transform from object space to mesh texture space.
   * This does not work with deformations but that can probably only be done
   * well with a volume grid mapping of coordinates. */
  if (mesh->need_attribute(scene, ATTR_STD_GENERATED_TRANSFORM)) {
    Attribute *attr = mesh->attributes.add(ATTR_STD_GENERATED_TRANSFORM);
    Transform *tfm = attr->data_transform();

    BL::Mesh b_mesh(b_ob.data());
    float3 loc, size;
    mesh_texture_space(b_mesh, loc, size);

    *tfm = transform_translate(-loc) * transform_scale(size);
  }
}

void BlenderSync::sync_volume(BL::Object &b_ob, Mesh *mesh, const vector<Shader *> &used_shaders)
{
  bool old_has_voxel_attributes = mesh->has_voxel_attributes();

  mesh->clear();
  mesh->used_shaders = used_shaders;

  /* Smoke domain. */
  if (view_layer.use_volumes) {
    sync_smoke_volume(scene, b_ob, mesh, b_scene.frame_current());
  }

  /* Tag update. */
  bool rebuild = (old_has_voxel_attributes != mesh->has_voxel_attributes());
  mesh->tag_update(scene, rebuild);
}

CCL_NAMESPACE_END
