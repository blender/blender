/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/volume.h"
#include "scene/image.h"
#include "scene/image_vdb.h"
#include "scene/object.h"

#include "blender/sync.h"
#include "blender/util.h"

#include "util/log.h"
#include "util/vector.h"

#include "BKE_volume_grid.hh"

CCL_NAMESPACE_BEGIN

/* TODO: verify this is not loading unnecessary attributes. */
class BlenderSmokeLoader : public VDBImageLoader {
 public:
  BlenderSmokeLoader(BL::Object &b_ob, AttributeStandard attribute, const float clipping)
      : VDBImageLoader(Attribute::standard_name(attribute), clipping),
        b_domain(object_fluid_gas_domain_find(b_ob)),
        attribute(attribute)
  {
    mesh_texture_space(
        *static_cast<const ::Mesh *>(b_ob.data().ptr.data), texspace_loc, texspace_size);
  }

  void load_grid() override
  {
    if (!b_domain) {
      return;
    }

    int channels;
    if (attribute == ATTR_STD_VOLUME_DENSITY || attribute == ATTR_STD_VOLUME_FLAME ||
        attribute == ATTR_STD_VOLUME_HEAT || attribute == ATTR_STD_VOLUME_TEMPERATURE)
    {
      channels = 1;
    }
    else if (attribute == ATTR_STD_VOLUME_COLOR) {
      channels = 4;
    }
    else if (attribute == ATTR_STD_VOLUME_VELOCITY) {
      channels = 3;
    }
    else {
      return;
    }

    const int3 resolution = get_int3(b_domain.domain_resolution());
    int amplify = (b_domain.use_noise()) ? b_domain.noise_scale() : 1;

    /* Velocity and heat data is always low-resolution. */
    if (attribute == ATTR_STD_VOLUME_VELOCITY || attribute == ATTR_STD_VOLUME_HEAT) {
      amplify = 1;
    }

    const size_t width = resolution.x * amplify;
    const size_t height = resolution.y * amplify;
    const size_t depth = resolution.z * amplify;

    /* Create a matrix to transform from object space to mesh texture space.
     * This does not work with deformations but that can probably only be done
     * well with a volume grid mapping of coordinates. */
    Transform transform_3d = transform_translate(-texspace_loc) * transform_scale(texspace_size);

    vector<float> voxels;
    if (!get_voxels(width, height, depth, channels, voxels)) {
      return;
    }

    grid_from_dense_voxels(width, height, depth, channels, voxels.data(), transform_3d);
  }

  bool get_voxels(const size_t width,
                  const size_t height,
                  const size_t depth,
                  const int channels,
                  vector<float> &voxels)
  {
    if (!b_domain) {
      return false;
    }

#ifdef WITH_FLUID
    int length;
    voxels.resize(width * height * depth * channels);

    if (attribute == ATTR_STD_VOLUME_DENSITY) {
      FluidDomainSettings_density_grid_get_length(&b_domain.ptr, &length);
      if (length == voxels.size()) {
        FluidDomainSettings_density_grid_get(&b_domain.ptr, voxels.data());
        return true;
      }
    }
    else if (attribute == ATTR_STD_VOLUME_FLAME) {
      /* this is in range 0..1, and interpreted by the OpenGL smoke viewer
       * as 1500..3000 K with the first part faded to zero density */
      FluidDomainSettings_flame_grid_get_length(&b_domain.ptr, &length);
      if (length == voxels.size()) {
        FluidDomainSettings_flame_grid_get(&b_domain.ptr, voxels.data());
        return true;
      }
    }
    else if (attribute == ATTR_STD_VOLUME_COLOR) {
      /* the RGB is "premultiplied" by density for better interpolation results */
      FluidDomainSettings_color_grid_get_length(&b_domain.ptr, &length);
      if (length == voxels.size()) {
        FluidDomainSettings_color_grid_get(&b_domain.ptr, voxels.data());
        return true;
      }
    }
    else if (attribute == ATTR_STD_VOLUME_VELOCITY) {
      FluidDomainSettings_velocity_grid_get_length(&b_domain.ptr, &length);
      if (length == voxels.size()) {
        FluidDomainSettings_velocity_grid_get(&b_domain.ptr, voxels.data());
        return true;
      }
    }
    else if (attribute == ATTR_STD_VOLUME_HEAT) {
      FluidDomainSettings_heat_grid_get_length(&b_domain.ptr, &length);
      if (length == voxels.size()) {
        FluidDomainSettings_heat_grid_get(&b_domain.ptr, voxels.data());
        return true;
      }
    }
    else if (attribute == ATTR_STD_VOLUME_TEMPERATURE) {
      FluidDomainSettings_temperature_grid_get_length(&b_domain.ptr, &length);
      if (length == voxels.size()) {
        FluidDomainSettings_temperature_grid_get(&b_domain.ptr, voxels.data());
        return true;
      }
    }
    else {
      LOG_ERROR << "Unknown volume attribute " << Attribute::standard_name(attribute)
                << "skipping ";
      voxels[0] = 0.0f;
      return false;
    }
    LOG_ERROR << "Unexpected smoke volume resolution, skipping";
#else
    (void)voxels;
    (void)width;
    (void)height;
    (void)depth;
    (void)channels;
#endif
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
    BL::Scene &b_scene, Scene *scene, BObjectInfo &b_ob_info, Volume *volume, const float frame)
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

  const AttributeStandard attributes[] = {ATTR_STD_VOLUME_DENSITY,
                                          ATTR_STD_VOLUME_COLOR,
                                          ATTR_STD_VOLUME_FLAME,
                                          ATTR_STD_VOLUME_HEAT,
                                          ATTR_STD_VOLUME_TEMPERATURE,
                                          ATTR_STD_VOLUME_VELOCITY,
                                          ATTR_STD_NONE};

  const Interval<int> frame_interval = {b_domain.cache_frame_start(), b_domain.cache_frame_end()};

  for (int i = 0; attributes[i] != ATTR_STD_NONE; i++) {
    const AttributeStandard std = attributes[i];
    if (!volume->need_attribute(scene, std)) {
      continue;
    }

    const float clipping = b_domain.clipping();

    Attribute *attr = volume->attributes.add(std);

    if (!frame_interval.contains(frame)) {
      attr->data_voxel().clear();
      continue;
    }

    unique_ptr<ImageLoader> loader = make_unique<BlenderSmokeLoader>(
        b_ob_info.real_object, std, clipping);
    ImageParams params;
    params.frame = frame;

    attr->data_voxel() = scene->image_manager->add_image(std::move(loader), params);
  }
}

class BlenderVolumeLoader : public VDBImageLoader {
 public:
  BlenderVolumeLoader(BL::BlendData &b_data,
                      BL::Volume &b_volume,
                      const string &grid_name,
                      BL::VolumeRender::precision_enum precision_,
                      const float clipping)
      : VDBImageLoader(grid_name, clipping), b_volume(b_volume)
  {
    b_volume.grids.load(b_data.ptr.data);

#ifdef WITH_OPENVDB
    for (BL::VolumeGrid &b_volume_grid : b_volume.grids) {
      if (b_volume_grid.name() == grid_name) {
        const auto *grid_data = static_cast<const blender::bke::VolumeGridData *>(
            b_volume_grid.ptr.data);
        grid_data->add_user();
        volume_grid = blender::bke::GVolumeGrid{grid_data};
        grid = volume_grid->grid_ptr(tree_access_token);
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
#ifdef WITH_OPENVDB
  /* Store tree user so that the OPENVDB grid that is shared with Blender is not unloaded. */
  blender::bke::GVolumeGrid volume_grid;
  blender::bke::VolumeTreeAccessToken tree_access_token;
#endif
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
    const ustring name = ustring(b_grid.name());
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
                            volume->attributes.add(name, TypeFloat, ATTR_ELEMENT_VOXEL);

      unique_ptr<ImageLoader> loader = make_unique<BlenderVolumeLoader>(
          b_data, b_volume, name.string(), b_render.precision(), b_render.clipping());
      ImageParams params;
      params.frame = b_volume.grids.frame();

      attr->data_voxel() = scene->image_manager->add_image(std::move(loader), params, false);
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

  volume->merge_grids(scene);

  /* Tag update. */
  volume->tag_update(scene, true);
}

CCL_NAMESPACE_END
