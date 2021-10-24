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

#include "blender/viewport.h"
#include "blender/util.h"

#include "scene/pass.h"

#include "util/log.h"

CCL_NAMESPACE_BEGIN

BlenderViewportParameters::BlenderViewportParameters()
    : use_scene_world(true),
      use_scene_lights(true),
      studiolight_rotate_z(0.0f),
      studiolight_intensity(1.0f),
      studiolight_background_alpha(1.0f),
      display_pass(PASS_COMBINED),
      show_active_pixels(false)
{
}

BlenderViewportParameters::BlenderViewportParameters(BL::SpaceView3D &b_v3d, bool use_developer_ui)
    : BlenderViewportParameters()
{
  if (!b_v3d) {
    return;
  }

  BL::View3DShading shading = b_v3d.shading();
  PointerRNA cshading = RNA_pointer_get(&shading.ptr, "cycles");

  /* We only copy the shading parameters if we are in look-dev mode.
   * Otherwise defaults are being used. These defaults mimic normal render settings. */
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

  /* Lookup display pass based on the enum identifier.
   * This is because integer values of python enum are not aligned with the passes definition in
   * the kernel. */

  display_pass = PASS_COMBINED;

  const string display_pass_identifier = get_enum_identifier(cshading, "render_pass");
  if (!display_pass_identifier.empty()) {
    const ustring pass_type_identifier(string_to_lower(display_pass_identifier));
    const NodeEnum *pass_type_enum = Pass::get_type_enum();
    if (pass_type_enum->exists(pass_type_identifier)) {
      display_pass = static_cast<PassType>((*pass_type_enum)[pass_type_identifier]);
    }
  }

  if (use_developer_ui) {
    show_active_pixels = get_boolean(cshading, "show_active_pixels");
  }
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
  return display_pass != other.display_pass || show_active_pixels != other.show_active_pixels;
}

bool BlenderViewportParameters::modified(const BlenderViewportParameters &other) const
{
  return shader_modified(other) || film_modified(other);
}

bool BlenderViewportParameters::use_custom_shader() const
{
  return !(use_scene_world && use_scene_lights);
}

CCL_NAMESPACE_END
