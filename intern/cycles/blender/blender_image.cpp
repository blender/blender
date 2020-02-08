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

#include "render/image.h"

#include "blender/blender_sync.h"
#include "blender/blender_session.h"
#include "blender/blender_util.h"

CCL_NAMESPACE_BEGIN

/* builtin image file name is actually an image datablock name with
 * absolute sequence frame number concatenated via '@' character
 *
 * this function splits frame from builtin name
 */
int BlenderSession::builtin_image_frame(const string &builtin_name)
{
  int last = builtin_name.find_last_of('@');
  return atoi(builtin_name.substr(last + 1, builtin_name.size() - last - 1).c_str());
}

void BlenderSession::builtin_image_info(const string &builtin_name,
                                        void *builtin_data,
                                        ImageMetaData &metadata)
{
  /* empty image */
  metadata.width = 1;
  metadata.height = 1;

  if (!builtin_data)
    return;

  /* recover ID pointer */
  PointerRNA ptr;
  RNA_id_pointer_create((ID *)builtin_data, &ptr);
  BL::ID b_id(ptr);

  if (b_id.is_a(&RNA_Image)) {
    /* image data */
    BL::Image b_image(b_id);

    metadata.builtin_free_cache = !b_image.has_data();
    metadata.is_float = b_image.is_float();
    metadata.width = b_image.size()[0];
    metadata.height = b_image.size()[1];
    metadata.depth = 1;
    metadata.channels = b_image.channels();

    if (metadata.is_float) {
      /* Float images are already converted on the Blender side,
       * no need to do anything in Cycles. */
      metadata.colorspace = u_colorspace_raw;
    }
  }
  else if (b_id.is_a(&RNA_Object)) {
    /* smoke volume data */
    BL::Object b_ob(b_id);
    BL::FluidDomainSettings b_domain = object_fluid_gas_domain_find(b_ob);

    metadata.is_float = true;
    metadata.depth = 1;
    metadata.channels = 1;

    if (!b_domain)
      return;

    if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_DENSITY) ||
        builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_FLAME) ||
        builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_HEAT) ||
        builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_TEMPERATURE))
      metadata.channels = 1;
    else if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_COLOR))
      metadata.channels = 4;
    else if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_VELOCITY))
      metadata.channels = 3;
    else
      return;

    int3 resolution = get_int3(b_domain.domain_resolution());
    int amplify = (b_domain.use_noise()) ? b_domain.noise_scale() : 1;

    /* Velocity and heat data is always low-resolution. */
    if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_VELOCITY) ||
        builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_HEAT)) {
      amplify = 1;
    }

    metadata.width = resolution.x * amplify;
    metadata.height = resolution.y * amplify;
    metadata.depth = resolution.z * amplify;
  }
  else {
    /* TODO(sergey): Check we're indeed in shader node tree. */
    PointerRNA ptr;
    RNA_pointer_create(NULL, &RNA_Node, builtin_data, &ptr);
    BL::Node b_node(ptr);
    if (b_node.is_a(&RNA_ShaderNodeTexPointDensity)) {
      BL::ShaderNodeTexPointDensity b_point_density_node(b_node);
      metadata.channels = 4;
      metadata.width = b_point_density_node.resolution();
      metadata.height = metadata.width;
      metadata.depth = metadata.width;
      metadata.is_float = true;
    }
  }
}

bool BlenderSession::builtin_image_pixels(const string &builtin_name,
                                          void *builtin_data,
                                          int tile,
                                          unsigned char *pixels,
                                          const size_t pixels_size,
                                          const bool associate_alpha,
                                          const bool free_cache)
{
  if (!builtin_data) {
    return false;
  }

  const int frame = builtin_image_frame(builtin_name);

  PointerRNA ptr;
  RNA_id_pointer_create((ID *)builtin_data, &ptr);
  BL::Image b_image(ptr);

  const int width = b_image.size()[0];
  const int height = b_image.size()[1];
  const int channels = b_image.channels();

  unsigned char *image_pixels = image_get_pixels_for_frame(b_image, frame, tile);
  const size_t num_pixels = ((size_t)width) * height;

  if (image_pixels && num_pixels * channels == pixels_size) {
    memcpy(pixels, image_pixels, pixels_size * sizeof(unsigned char));
  }
  else {
    if (channels == 1) {
      memset(pixels, 0, pixels_size * sizeof(unsigned char));
    }
    else {
      const size_t num_pixels_safe = pixels_size / channels;
      unsigned char *cp = pixels;
      for (size_t i = 0; i < num_pixels_safe; i++, cp += channels) {
        cp[0] = 255;
        cp[1] = 0;
        cp[2] = 255;
        if (channels == 4) {
          cp[3] = 255;
        }
      }
    }
  }

  if (image_pixels) {
    MEM_freeN(image_pixels);
  }

  /* Free image buffers to save memory during render. */
  if (free_cache) {
    b_image.buffers_free();
  }

  if (associate_alpha) {
    /* Premultiply, byte images are always straight for Blender. */
    unsigned char *cp = pixels;
    for (size_t i = 0; i < num_pixels; i++, cp += channels) {
      cp[0] = (cp[0] * cp[3]) >> 8;
      cp[1] = (cp[1] * cp[3]) >> 8;
      cp[2] = (cp[2] * cp[3]) >> 8;
    }
  }
  return true;
}

bool BlenderSession::builtin_image_float_pixels(const string &builtin_name,
                                                void *builtin_data,
                                                int tile,
                                                float *pixels,
                                                const size_t pixels_size,
                                                const bool,
                                                const bool free_cache)
{
  if (!builtin_data) {
    return false;
  }

  PointerRNA ptr;
  RNA_id_pointer_create((ID *)builtin_data, &ptr);
  BL::ID b_id(ptr);

  if (b_id.is_a(&RNA_Image)) {
    /* image data */
    BL::Image b_image(b_id);
    int frame = builtin_image_frame(builtin_name);

    const int width = b_image.size()[0];
    const int height = b_image.size()[1];
    const int channels = b_image.channels();

    float *image_pixels;
    image_pixels = image_get_float_pixels_for_frame(b_image, frame, tile);
    const size_t num_pixels = ((size_t)width) * height;

    if (image_pixels && num_pixels * channels == pixels_size) {
      memcpy(pixels, image_pixels, pixels_size * sizeof(float));
    }
    else {
      if (channels == 1) {
        memset(pixels, 0, num_pixels * sizeof(float));
      }
      else {
        const size_t num_pixels_safe = pixels_size / channels;
        float *fp = pixels;
        for (int i = 0; i < num_pixels_safe; i++, fp += channels) {
          fp[0] = 1.0f;
          fp[1] = 0.0f;
          fp[2] = 1.0f;
          if (channels == 4) {
            fp[3] = 1.0f;
          }
        }
      }
    }

    if (image_pixels) {
      MEM_freeN(image_pixels);
    }

    /* Free image buffers to save memory during render. */
    if (free_cache) {
      b_image.buffers_free();
    }

    return true;
  }
  else if (b_id.is_a(&RNA_Object)) {
    /* smoke volume data */
    BL::Object b_ob(b_id);
    BL::FluidDomainSettings b_domain = object_fluid_gas_domain_find(b_ob);

    if (!b_domain) {
      return false;
    }
#ifdef WITH_FLUID
    int3 resolution = get_int3(b_domain.domain_resolution());
    int length, amplify = (b_domain.use_noise()) ? b_domain.noise_scale() : 1;

    /* Velocity and heat data is always low-resolution. */
    if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_VELOCITY) ||
        builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_HEAT)) {
      amplify = 1;
    }

    const int width = resolution.x * amplify;
    const int height = resolution.y * amplify;
    const int depth = resolution.z * amplify;
    const size_t num_pixels = ((size_t)width) * height * depth;

    if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_DENSITY)) {
      FluidDomainSettings_density_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels) {
        FluidDomainSettings_density_grid_get(&b_domain.ptr, pixels);
        return true;
      }
    }
    else if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_FLAME)) {
      /* this is in range 0..1, and interpreted by the OpenGL smoke viewer
       * as 1500..3000 K with the first part faded to zero density */
      FluidDomainSettings_flame_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels) {
        FluidDomainSettings_flame_grid_get(&b_domain.ptr, pixels);
        return true;
      }
    }
    else if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_COLOR)) {
      /* the RGB is "premultiplied" by density for better interpolation results */
      FluidDomainSettings_color_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels * 4) {
        FluidDomainSettings_color_grid_get(&b_domain.ptr, pixels);
        return true;
      }
    }
    else if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_VELOCITY)) {
      FluidDomainSettings_velocity_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels * 3) {
        FluidDomainSettings_velocity_grid_get(&b_domain.ptr, pixels);
        return true;
      }
    }
    else if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_HEAT)) {
      FluidDomainSettings_heat_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels) {
        FluidDomainSettings_heat_grid_get(&b_domain.ptr, pixels);
        return true;
      }
    }
    else if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_TEMPERATURE)) {
      FluidDomainSettings_temperature_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels) {
        FluidDomainSettings_temperature_grid_get(&b_domain.ptr, pixels);
        return true;
      }
    }
    else {
      fprintf(
          stderr, "Cycles error: unknown volume attribute %s, skipping\n", builtin_name.c_str());
      pixels[0] = 0.0f;
      return false;
    }
#endif
    fprintf(stderr, "Cycles error: unexpected smoke volume resolution, skipping\n");
  }
  else {
    /* We originally were passing view_layer here but in reality we need a
     * a depsgraph to pass to the RE_point_density_minmax() function.
     */
    /* TODO(sergey): Check we're indeed in shader node tree. */
    PointerRNA ptr;
    RNA_pointer_create(NULL, &RNA_Node, builtin_data, &ptr);
    BL::Node b_node(ptr);
    if (b_node.is_a(&RNA_ShaderNodeTexPointDensity)) {
      BL::ShaderNodeTexPointDensity b_point_density_node(b_node);
      int length;
      b_point_density_node.calc_point_density(b_depsgraph, &length, &pixels);
    }
  }

  return false;
}

void BlenderSession::builtin_images_load()
{
  /* Force builtin images to be loaded along with Blender data sync. This
   * is needed because we may be reading from depsgraph evaluated data which
   * can be freed by Blender before Cycles reads it.
   *
   * TODO: the assumption that no further access to builtin image data will
   * happen is really weak, and likely to break in the future. We should find
   * a better solution to hand over the data directly to the image manager
   * instead of through callbacks whose timing is difficult to control. */
  ImageManager *manager = session->scene->image_manager;
  Device *device = session->device;
  manager->device_load_builtin(device, session->scene, session->progress);
}

CCL_NAMESPACE_END
