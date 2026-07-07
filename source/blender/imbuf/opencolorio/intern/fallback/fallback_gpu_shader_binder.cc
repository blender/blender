/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "fallback_gpu_shader_binder.hh"

#include <string>

#include "../gpu_shader_binder_internal.hh"

namespace blender::ocio {

namespace {

static std::string generate_display_fragment_source(
    const internal::GPUDisplayShader &display_shader)
{
  std::string source;

  /* Generate OCIO_to_scene_linear(). */
  if (display_shader.from_colorspace == "sRGB") {
    /* Use Blender's default sRGB->Linear conversion.
     * Expect that the alpha association is handled in the caller. */
    source +=
        "vec4 OCIO_to_scene_linear(vec4 pixel) {\n"
        "  return vec4(srgb_to_linear_rgb(pixel.rgb), pixel.a);\n"
        "}\n";
  }
  else {
    /* Linear or Non-Color: no need to perform any conversion. */
    source += "vec4 OCIO_to_scene_linear(vec4 pixel) { return pixel; }\n";
  }

  /* Generate OCIO_to_display(). */
  if (display_shader.display == "sRGB") {
    source +=
        "vec4 OCIO_to_display(vec4 pixel) {\n"
        "  return vec4(linear_rgb_to_srgb(pixel.rgb), pixel.a);\n"
        "}\n";
  }
  else {
    /* Linear or Non-Color: no need to perform any conversion. */
    source += "vec4 OCIO_to_display(vec4 pixel) { return pixel; }\n";
  }

  return source;
}

static std::string generate_scene_linear_fragment_source(
    const internal::GPUDisplayShader &display_shader)
{
  std::string source;

  /* Generate OCIO_to_scene_linear(). */
  if (display_shader.from_colorspace == "sRGB") {
    /* Use Blender's default sRGB->Linear conversion.
     * Expect that the alpha association is handled in the caller. */
    source +=
        "vec4 OCIO_to_scene_linear(vec4 pixel) {\n"
        "  return vec4(srgb_to_linear_rgb(pixel.rgb), pixel.a);\n"
        "}\n";
  }
  else {
    /* Linear or Non-Color: no need to perform any conversion. */
    source += "vec4 OCIO_to_scene_linear(vec4 pixel) { return pixel; }\n";
  }

  /* Generate OCIO_to_display(). */
  source += "vec4 OCIO_to_display(vec4 pixel) { return pixel; }\n";

  return source;
}

}  // namespace

void FallbackGPUShaderBinder::construct_display_shader(
    internal::GPUDisplayShader &display_shader) const
{
  const std::string fragment_source = generate_display_fragment_source(display_shader);

  if (!create_gpu_shader(display_shader, fragment_source, {})) {
    display_shader.is_valid = false;
    return;
  }

  display_shader.is_valid = true;
}

void FallbackGPUShaderBinder::construct_scene_linear_shader(
    internal::GPUDisplayShader &display_shader) const
{
  display_shader.is_valid = true;

  const std::string fragment_source = generate_scene_linear_fragment_source(display_shader);

  if (!create_gpu_shader(display_shader,
                         fragment_source,
                         {{"USE_TO_SCENE_LINEAR_ONLY", ""}, {"OUTPUT_PREMULTIPLIED", ""}}))
  {
    display_shader.is_valid = false;
  }
}

}  // namespace blender::ocio
