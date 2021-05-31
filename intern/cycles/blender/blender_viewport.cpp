/*
 * Copyright 2019 Blender Foundation
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

#include "blender_viewport.h"

#include "blender_util.h"

CCL_NAMESPACE_BEGIN

BlenderViewportParameters::BlenderViewportParameters()
    : use_scene_world(true),
      use_scene_lights(true),
      studiolight_rotate_z(0.0f),
      studiolight_intensity(1.0f),
      studiolight_background_alpha(1.0f),
      display_pass(PASS_COMBINED)
{
}

BlenderViewportParameters::BlenderViewportParameters(BL::SpaceView3D &b_v3d)
    : BlenderViewportParameters()
{
  if (!b_v3d) {
    return;
  }

  BL::View3DShading shading = b_v3d.shading();
  PointerRNA cshading = RNA_pointer_get(&shading.ptr, "cycles");

  /* We only copy the shading parameters if we are in look dev mode. otherwise
   * defaults are being used. These defaults mimic normal render settings */
  if (shading.type() == BL::View3DShading::type_RENDERED) {
    use_scene_world = shading.use_scene_world_render();
    use_scene_lights = shading.use_scene_lights_render();

    if (!use_scene_world) {
      studiolight_rotate_z = shading.studiolight_rotate_z();
      studiolight_intensity = shading.studiolight_intensity();
      studiolight_background_alpha = shading.studiolight_background_alpha();
      studiolight_path = shading.selected_studio_light().path();
    }
  }

  /* Film. */
  display_pass = (PassType)get_enum(cshading, "render_pass", -1, -1);
}

bool BlenderViewportParameters::shader_modified(const BlenderViewportParameters &other) const
{
  return use_scene_world != other.use_scene_world || use_scene_lights != other.use_scene_lights ||
         studiolight_rotate_z != other.studiolight_rotate_z ||
         studiolight_intensity != other.studiolight_intensity ||
         studiolight_background_alpha != other.studiolight_background_alpha ||
         studiolight_path != other.studiolight_path;
}

bool BlenderViewportParameters::film_modified(const BlenderViewportParameters &other) const
{
  return display_pass != other.display_pass;
}

bool BlenderViewportParameters::modified(const BlenderViewportParameters &other) const
{
  return shader_modified(other) || film_modified(other);
}

bool BlenderViewportParameters::use_custom_shader() const
{
  return !(use_scene_world && use_scene_lights);
}

PassType update_viewport_display_passes(BL::SpaceView3D &b_v3d, vector<Pass> &passes)
{
  if (b_v3d) {
    const BlenderViewportParameters viewport_parameters(b_v3d);
    const PassType display_pass = viewport_parameters.display_pass;

    passes.clear();
    Pass::add(display_pass, passes);

    return display_pass;
  }
  return PASS_NONE;
}

CCL_NAMESPACE_END
