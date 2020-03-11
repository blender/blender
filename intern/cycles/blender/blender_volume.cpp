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

/* TODO: verify this is not loading unnecessary attributes. */
class BlenderSmokeLoader : public ImageLoader {
 public:
  BlenderSmokeLoader(const BL::Object &b_ob, AttributeStandard attribute)
      : b_ob(b_ob), attribute(attribute)
  {
  }

  bool load_metadata(ImageMetaData &metadata) override
  {
    BL::FluidDomainSettings b_domain = object_fluid_gas_domain_find(b_ob);

    if (!b_domain) {
      return false;
    }

    if (attribute == ATTR_STD_VOLUME_DENSITY || attribute == ATTR_STD_VOLUME_FLAME ||
        attribute == ATTR_STD_VOLUME_HEAT || attribute == ATTR_STD_VOLUME_TEMPERATURE) {
      metadata.type = IMAGE_DATA_TYPE_FLOAT;
      metadata.channels = 1;
    }
    else if (attribute == ATTR_STD_VOLUME_COLOR) {
      metadata.type = IMAGE_DATA_TYPE_FLOAT4;
      metadata.channels = 4;
    }
    else if (attribute == ATTR_STD_VOLUME_VELOCITY) {
      metadata.type = IMAGE_DATA_TYPE_FLOAT4;
      metadata.channels = 3;
    }
    else {
      return false;
    }

    int3 resolution = get_int3(b_domain.domain_resolution());
    int amplify = (b_domain.use_noise()) ? b_domain.noise_scale() : 1;

    /* Velocity and heat data is always low-resolution. */
    if (attribute == ATTR_STD_VOLUME_VELOCITY || attribute == ATTR_STD_VOLUME_HEAT) {
      amplify = 1;
    }

    metadata.width = resolution.x * amplify;
    metadata.height = resolution.y * amplify;
    metadata.depth = resolution.z * amplify;

    return true;
  }

  bool load_pixels(const ImageMetaData &, void *pixels, const size_t, const bool) override
  {
    /* smoke volume data */
    BL::FluidDomainSettings b_domain = object_fluid_gas_domain_find(b_ob);

    if (!b_domain) {
      return false;
    }
#ifdef WITH_FLUID
    int3 resolution = get_int3(b_domain.domain_resolution());
    int length, amplify = (b_domain.use_noise()) ? b_domain.noise_scale() : 1;

    /* Velocity and heat data is always low-resolution. */
    if (attribute == ATTR_STD_VOLUME_VELOCITY || attribute == ATTR_STD_VOLUME_HEAT) {
      amplify = 1;
    }

    const int width = resolution.x * amplify;
    const int height = resolution.y * amplify;
    const int depth = resolution.z * amplify;
    const size_t num_pixels = ((size_t)width) * height * depth;

    float *fpixels = (float *)pixels;

    if (attribute == ATTR_STD_VOLUME_DENSITY) {
      FluidDomainSettings_density_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels) {
        FluidDomainSettings_density_grid_get(&b_domain.ptr, fpixels);
        return true;
      }
    }
    else if (attribute == ATTR_STD_VOLUME_FLAME) {
      /* this is in range 0..1, and interpreted by the OpenGL smoke viewer
       * as 1500..3000 K with the first part faded to zero density */
      FluidDomainSettings_flame_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels) {
        FluidDomainSettings_flame_grid_get(&b_domain.ptr, fpixels);
        return true;
      }
    }
    else if (attribute == ATTR_STD_VOLUME_COLOR) {
      /* the RGB is "premultiplied" by density for better interpolation results */
      FluidDomainSettings_color_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels * 4) {
        FluidDomainSettings_color_grid_get(&b_domain.ptr, fpixels);
        return true;
      }
    }
    else if (attribute == ATTR_STD_VOLUME_VELOCITY) {
      FluidDomainSettings_velocity_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels * 3) {
        FluidDomainSettings_velocity_grid_get(&b_domain.ptr, fpixels);
        return true;
      }
    }
    else if (attribute == ATTR_STD_VOLUME_HEAT) {
      FluidDomainSettings_heat_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels) {
        FluidDomainSettings_heat_grid_get(&b_domain.ptr, fpixels);
        return true;
      }
    }
    else if (attribute == ATTR_STD_VOLUME_TEMPERATURE) {
      FluidDomainSettings_temperature_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels) {
        FluidDomainSettings_temperature_grid_get(&b_domain.ptr, fpixels);
        return true;
      }
    }
    else {
      fprintf(stderr,
              "Cycles error: unknown volume attribute %s, skipping\n",
              Attribute::standard_name(attribute));
      fpixels[0] = 0.0f;
      return false;
    }
#else
    (void)pixels;
#endif
    fprintf(stderr, "Cycles error: unexpected smoke volume resolution, skipping\n");
    return false;
  }

  string name() const override
  {
    return Attribute::standard_name(attribute);
  }

  bool equals(const ImageLoader &other) const override
  {
    const BlenderSmokeLoader &other_loader = (const BlenderSmokeLoader &)other;
    return b_ob == other_loader.b_ob && attribute == other_loader.attribute;
  }

  BL::Object b_ob;
  AttributeStandard attribute;
};

static void sync_smoke_volume(Scene *scene, BL::Object &b_ob, Mesh *mesh, float frame)
{
  BL::FluidDomainSettings b_domain = object_fluid_gas_domain_find(b_ob);
  if (!b_domain) {
    return;
  }

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

    ImageLoader *loader = new BlenderSmokeLoader(b_ob, std);
    ImageParams params;
    params.frame = frame;

    attr->data_voxel() = scene->image_manager->add_image(loader, params);
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

/* If the voxel attributes change, we need to rebuild the bounding mesh. */
static vector<int> get_voxel_image_slots(Mesh *mesh)
{
  vector<int> slots;
  for (const Attribute &attr : mesh->attributes.attributes) {
    if (attr.element == ATTR_ELEMENT_VOXEL) {
      slots.push_back(attr.data_voxel().svm_slot());
    }
  }

  return slots;
}

void BlenderSync::sync_volume(BL::Object &b_ob, Mesh *mesh, const vector<Shader *> &used_shaders)
{
  vector<int> old_voxel_slots = get_voxel_image_slots(mesh);

  mesh->clear();
  mesh->used_shaders = used_shaders;

  /* Smoke domain. */
  if (view_layer.use_volumes) {
    sync_smoke_volume(scene, b_ob, mesh, b_scene.frame_current());
  }

  /* Tag update. */
  bool rebuild = (old_voxel_slots != get_voxel_image_slots(mesh));
  mesh->tag_update(scene, rebuild);
}

CCL_NAMESPACE_END
