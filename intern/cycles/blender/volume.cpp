/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/volume.h"
#include "scene/colorspace.h"
#include "scene/image.h"
#include "scene/image_vdb.h"
#include "scene/object.h"

#include "blender/sync.h"
#include "blender/util.h"

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
openvdb::GridBase::ConstPtr BKE_volume_grid_openvdb_for_read(const struct Volume *volume,
                                                             const struct VolumeGrid *grid);
#endif

CCL_NAMESPACE_BEGIN

/* TODO: verify this is not loading unnecessary attributes. */
class BlenderSmokeLoader : public ImageLoader {
 public:
  BlenderSmokeLoader(BL::Object &b_ob, AttributeStandard attribute)
      : b_domain(object_fluid_gas_domain_find(b_ob)), attribute(attribute)
  {
    mesh_texture_space(
        *static_cast<const ::Mesh *>(b_ob.data().ptr.data), texspace_loc, texspace_size);
  }

  bool load_metadata(const ImageDeviceFeatures &, ImageMetaData &metadata) override
  {
    if (!b_domain) {
      return false;
    }

    if (attribute == ATTR_STD_VOLUME_DENSITY || attribute == ATTR_STD_VOLUME_FLAME ||
        attribute == ATTR_STD_VOLUME_HEAT || attribute == ATTR_STD_VOLUME_TEMPERATURE)
    {
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

    /* Create a matrix to transform from object space to mesh texture space.
     * This does not work with deformations but that can probably only be done
     * well with a volume grid mapping of coordinates. */
    metadata.transform_3d = transform_translate(-texspace_loc) * transform_scale(texspace_size);
    metadata.use_transform_3d = true;

    return true;
  }

  bool load_pixels(const ImageMetaData &, void *pixels, const size_t, const bool) override
  {
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
    return b_domain == other_loader.b_domain && attribute == other_loader.attribute;
  }

  BL::FluidDomainSettings b_domain;
  float3 texspace_loc, texspace_size;
  AttributeStandard attribute;
};

static void sync_smoke_volume(
    BL::Scene &b_scene, Scene *scene, BObjectInfo &b_ob_info, Volume *volume, float frame)
{
  if (!b_ob_info.is_real_object_data()) {
    return;
  }
  BL::FluidDomainSettings b_domain = object_fluid_gas_domain_find(b_ob_info.real_object);
  if (!b_domain) {
    return;
  }

  float velocity_scale = b_domain.velocity_scale();
  /* Motion blur attribute is relative to seconds, we need it relative to frames. */
  const bool need_motion = object_need_motion_attribute(b_ob_info, scene);
  const float motion_scale = (need_motion) ?
                                 scene->motion_shutter_time() /
                                     (b_scene.render().fps() / b_scene.render().fps_base()) :
                                 0.0f;

  velocity_scale *= motion_scale;

  volume->set_velocity_scale(velocity_scale);

  AttributeStandard attributes[] = {ATTR_STD_VOLUME_DENSITY,
                                    ATTR_STD_VOLUME_COLOR,
                                    ATTR_STD_VOLUME_FLAME,
                                    ATTR_STD_VOLUME_HEAT,
                                    ATTR_STD_VOLUME_TEMPERATURE,
                                    ATTR_STD_VOLUME_VELOCITY,
                                    ATTR_STD_NONE};

  for (int i = 0; attributes[i] != ATTR_STD_NONE; i++) {
    AttributeStandard std = attributes[i];
    if (!volume->need_attribute(scene, std)) {
      continue;
    }

    volume->set_clipping(b_domain.clipping());

    Attribute *attr = volume->attributes.add(std);

    ImageLoader *loader = new BlenderSmokeLoader(b_ob_info.real_object, std);
    ImageParams params;
    params.frame = frame;

    attr->data_voxel() = scene->image_manager->add_image(loader, params);
  }
}

class BlenderVolumeLoader : public VDBImageLoader {
 public:
  BlenderVolumeLoader(BL::BlendData &b_data,
                      BL::Volume &b_volume,
                      const string &grid_name,
                      BL::VolumeRender::precision_enum precision_)
      : VDBImageLoader(grid_name), b_volume(b_volume)
  {
    b_volume.grids.load(b_data.ptr.data);

#ifdef WITH_OPENVDB
    for (BL::VolumeGrid &b_volume_grid : b_volume.grids) {
      if (b_volume_grid.name() == grid_name) {
        const bool unload = !b_volume_grid.is_loaded();

        ::Volume *volume = (::Volume *)b_volume.ptr.data;
        const VolumeGrid *volume_grid = (VolumeGrid *)b_volume_grid.ptr.data;
        grid = BKE_volume_grid_openvdb_for_read(volume, volume_grid);

        if (unload) {
          b_volume_grid.unload();
        }

        break;
      }
    }
#endif
#ifdef WITH_NANOVDB
    switch (precision_) {
      case BL::VolumeRender::precision_FULL:
        precision = 32;
        break;
      case BL::VolumeRender::precision_HALF:
        precision = 16;
        break;
      default:
      case BL::VolumeRender::precision_VARIABLE:
        precision = 0;
        break;
    }
#else
    (void)precision_;
#endif
  }

  BL::Volume b_volume;
};

static void sync_volume_object(BL::BlendData &b_data,
                               BL::Scene &b_scene,
                               BObjectInfo &b_ob_info,
                               Scene *scene,
                               Volume *volume)
{
  BL::Volume b_volume(b_ob_info.object_data);
  b_volume.grids.load(b_data.ptr.data);

  BL::VolumeRender b_render(b_volume.render());

  volume->set_clipping(b_render.clipping());
  volume->set_step_size(b_render.step_size());
  volume->set_object_space((b_render.space() == BL::VolumeRender::space_OBJECT));

  float velocity_scale = b_volume.velocity_scale();
  if (b_volume.velocity_unit() == BL::Volume::velocity_unit_SECOND) {
    /* Motion blur attribute is relative to seconds, we need it relative to frames. */
    const bool need_motion = object_need_motion_attribute(b_ob_info, scene);
    const float motion_scale = (need_motion) ?
                                   scene->motion_shutter_time() /
                                       (b_scene.render().fps() / b_scene.render().fps_base()) :
                                   0.0f;

    velocity_scale *= motion_scale;
  }

  volume->set_velocity_scale(velocity_scale);

  /* Find grid with matching name. */
  for (BL::VolumeGrid &b_grid : b_volume.grids) {
    ustring name = ustring(b_grid.name());
    AttributeStandard std = ATTR_STD_NONE;

    if (name == Attribute::standard_name(ATTR_STD_VOLUME_DENSITY)) {
      std = ATTR_STD_VOLUME_DENSITY;
    }
    else if (name == Attribute::standard_name(ATTR_STD_VOLUME_COLOR)) {
      std = ATTR_STD_VOLUME_COLOR;
    }
    else if (name == Attribute::standard_name(ATTR_STD_VOLUME_FLAME)) {
      std = ATTR_STD_VOLUME_FLAME;
    }
    else if (name == Attribute::standard_name(ATTR_STD_VOLUME_HEAT)) {
      std = ATTR_STD_VOLUME_HEAT;
    }
    else if (name == Attribute::standard_name(ATTR_STD_VOLUME_TEMPERATURE)) {
      std = ATTR_STD_VOLUME_TEMPERATURE;
    }
    else if (name == Attribute::standard_name(ATTR_STD_VOLUME_VELOCITY) ||
             name == b_volume.velocity_grid())
    {
      std = ATTR_STD_VOLUME_VELOCITY;
    }
    else if (name == Attribute::standard_name(ATTR_STD_VOLUME_VELOCITY_X) ||
             name == b_volume.velocity_x_grid())
    {
      std = ATTR_STD_VOLUME_VELOCITY_X;
    }
    else if (name == Attribute::standard_name(ATTR_STD_VOLUME_VELOCITY_Y) ||
             name == b_volume.velocity_y_grid())
    {
      std = ATTR_STD_VOLUME_VELOCITY_Y;
    }
    else if (name == Attribute::standard_name(ATTR_STD_VOLUME_VELOCITY_Z) ||
             name == b_volume.velocity_z_grid())
    {
      std = ATTR_STD_VOLUME_VELOCITY_Z;
    }

    if ((std != ATTR_STD_NONE && volume->need_attribute(scene, std)) ||
        volume->need_attribute(scene, name))
    {
      Attribute *attr = (std != ATTR_STD_NONE) ?
                            volume->attributes.add(std) :
                            volume->attributes.add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_VOXEL);

      ImageLoader *loader = new BlenderVolumeLoader(
          b_data, b_volume, name.string(), b_render.precision());
      ImageParams params;
      params.frame = b_volume.grids.frame();

      attr->data_voxel() = scene->image_manager->add_image(loader, params, false);
    }
  }
}

void BlenderSync::sync_volume(BObjectInfo &b_ob_info, Volume *volume)
{
  volume->clear(true);

  if (view_layer.use_volumes) {
    if (b_ob_info.object_data.is_a(&RNA_Volume)) {
      /* Volume object. Create only attributes, bounding mesh will then
       * be automatically generated later. */
      sync_volume_object(b_data, b_scene, b_ob_info, scene, volume);
    }
    else {
      /* Smoke domain. */
      sync_smoke_volume(b_scene, scene, b_ob_info, volume, b_scene.frame_current());
    }
  }

  /* Tag update. */
  volume->tag_update(scene, true);
}

CCL_NAMESPACE_END
