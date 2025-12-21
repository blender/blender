/* SPDX-FileCopyrightText: 2019-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "blender/viewport.h"
#include "blender/util.h"

#include "scene/pass.h"

#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "RNA_prototypes.hh"

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

BlenderViewportParameters::BlenderViewportParameters(::bScreen *b_screen,
                                                     ::View3D *b_v3d,
                                                     bool use_developer_ui)
    : BlenderViewportParameters()
{
  if (!b_v3d) {
    return;
  }

  ::View3DShading shading = b_v3d->shading;
  PointerRNA v3d_rna_ptr = RNA_pointer_create_discrete(&b_screen->id, &RNA_SpaceView3D, b_v3d);
  PointerRNA shading_rna_ptr = RNA_pointer_get(&v3d_rna_ptr, "shading");

  /* We only copy the shading parameters if we are in look-dev mode.
   * Otherwise defaults are being used. These defaults mimic normal render settings. */
  if (shading.type == OB_RENDER) {
    use_scene_world = shading.flag & V3D_SHADING_SCENE_WORLD_RENDER;
    use_scene_lights = shading.flag & V3D_SHADING_SCENE_LIGHTS_RENDER;

    if (!use_scene_world) {
      studiolight_rotate_z = shading.studiolight_rot_z;
      studiolight_intensity = shading.studiolight_intensity;
      studiolight_background_alpha = shading.studiolight_background;
      PointerRNA selected_studiolight_rna_ptr = RNA_pointer_get(&shading_rna_ptr,
                                                                "selected_studio_light");
      studiolight_path = RNA_string_get(&selected_studiolight_rna_ptr, "path");
    }
  }

  /* Film. */

  /* Lookup display pass based on the enum identifier.
   * This is because integer values of python enum are not aligned with the passes definition in
   * the kernel. */

  display_pass = PASS_COMBINED;

  PointerRNA cycles_shading_ptr = RNA_pointer_get(&shading_rna_ptr, "cycles");
  const string display_pass_identifier = get_enum_identifier(cycles_shading_ptr, "render_pass");
  if (!display_pass_identifier.empty()) {
    const ustring pass_type_identifier(string_to_lower(display_pass_identifier));
    const NodeEnum *pass_type_enum = Pass::get_type_enum();
    if (pass_type_enum->exists(pass_type_identifier)) {
      display_pass = static_cast<PassType>((*pass_type_enum)[pass_type_identifier]);
    }
  }

  if (use_developer_ui) {
    show_active_pixels = get_boolean(cycles_shading_ptr, "show_active_pixels");
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
