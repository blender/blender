/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Set of shaders used for interface drawing.
 *
 * 2D shaders are not expected to work in 3D.
 * 3D shaders can work with 2D geometry and matrices.
 *
 * `INST` suffix means instance, which means the shader is build to leverage instancing
 * capabilities to reduce the number of draw-calls.
 *
 * For full list of parameters, search for the associated #ShaderCreateInfo.
 * Example: `GPU_SHADER_ICON` is defined by `GPU_SHADER_CREATE_INFO(gpu_shader_icon)`
 * Some parameters are builtins and are set automatically (ex: `ModelViewProjectionMatrix`).
 */

#pragma once

namespace blender::gpu {
class Shader;
}  // namespace blender::gpu

enum GPUBuiltinShader {
  /** Glyph drawing shader used by the BLF module. */
  GPU_SHADER_TEXT = 0,
  /** Draws keyframe markers. All markers shapes are supported through a single shader. */
  GPU_SHADER_KEYFRAME_SHAPE,
  /** Draw solid mesh with a single distant light using a clamped simple dot product. */
  GPU_SHADER_SIMPLE_LIGHTING,
  /** Draw an icon, leaving a semi-transparent rectangle on top of the icon. */
  GPU_SHADER_ICON,
  /** Draw a texture with a uniform color multiplied. */
  GPU_SHADER_2D_IMAGE_RECT_COLOR,
  /** Draw a texture with a desaturation factor. */
  GPU_SHADER_2D_IMAGE_DESATURATE_COLOR,
  /** Draw a group of texture rectangle with an associated color multiplied. */
  GPU_SHADER_ICON_MULTI,
  /** Draw a two color checker based on screen position (not UV coordinates). */
  GPU_SHADER_2D_CHECKER,
  /** Draw diagonal stripes with two alternating colors. */
  GPU_SHADER_2D_DIAG_STRIPES,
  /** Draw dashed lines with custom dash length and uniform color. */
  GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR,
  /** Draw triangles / lines / points with only depth output. */
  GPU_SHADER_3D_DEPTH_ONLY,
  /** Merge viewport overlay texture with the render output. */
  GPU_SHADER_2D_IMAGE_OVERLAYS_MERGE,
  GPU_SHADER_2D_IMAGE_OVERLAYS_STEREO_MERGE,
  /** Merge viewport overlay texture with the render output. */
  GPU_SHADER_2D_IMAGE_SHUFFLE_COLOR,
  /** Used for drawing of annotations (former grease pencil). */
  GPU_SHADER_GPENCIL_STROKE,
  /** Draw rounded area borders with silky smooth anti-aliasing without any over-draw. */
  GPU_SHADER_2D_AREA_BORDERS,
  /** Multi usage widget shaders for drawing buttons and other UI elements. */
  GPU_SHADER_2D_WIDGET_BASE,
  GPU_SHADER_2D_WIDGET_BASE_INST,
  GPU_SHADER_2D_WIDGET_SHADOW,
  /** Draw a node socket given it's bounding rectangle. All socket shapes are supported through
   * a single shader. */
  GPU_SHADER_2D_NODE_SOCKET,
  GPU_SHADER_2D_NODE_SOCKET_INST,
  /** Draw a node link given an input quadratic Bezier curve. */
  GPU_SHADER_2D_NODELINK,

  /** Draw round points with per vertex size and color. */
  GPU_SHADER_3D_POINT_VARYING_SIZE_VARYING_COLOR,
  /** Draw round points with a uniform size. Disabling blending will disable AA. */
  GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA,
  GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA,
  /** Draw round points with a uniform size and an outline. Disabling blending will disable AA. */
  GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA,

  /** Draw geometry with uniform color. Has an additional clip plane parameter. */
  GPU_SHADER_3D_CLIPPED_UNIFORM_COLOR,
  /** Draw wide lines with uniform color. Has an additional clip plane parameter. */
  GPU_SHADER_3D_POLYLINE_CLIPPED_UNIFORM_COLOR,

  /** Draw strip widgets in sequencer timeline. */
  GPU_SHADER_SEQUENCER_STRIPS,
  /** Draw strip thumbnails in sequencer timeline. */
  GPU_SHADER_SEQUENCER_THUMBS,
  /** Rasterize sequencer scope points into buffers via compute. */
  GPU_SHADER_SEQUENCER_SCOPE_RASTER,
  /** Resolve rasterized scope point buffers to display. */
  GPU_SHADER_SEQUENCER_SCOPE_RESOLVE,
  /** Draw sequencer zebra pattern (overexposed regions). */
  GPU_SHADER_SEQUENCER_ZEBRA,

  /** Draw xr raycast as a ruled spline surface. */
  GPU_SHADER_XR_RAYCAST,

  /** Compute shaders to generate 2d index buffers (mainly for curve drawing). */
  GPU_SHADER_INDEXBUF_POINTS,
  GPU_SHADER_INDEXBUF_LINES,
  GPU_SHADER_INDEXBUF_TRIS,

  /**
   * ----------------------- Shaders exposed through pyGPU module -----------------------
   *
   * Avoid breaking the interface of these shaders as they are used by addons.
   * Polyline versions are used for drawing wide lines (> 1px width).
   */

  /**
   * Take a 3D position and color for each vertex without color interpolation.
   *
   * \param color: in vec4
   * \param pos: in vec3
   */
  GPU_SHADER_3D_FLAT_COLOR,
  GPU_SHADER_3D_POLYLINE_FLAT_COLOR,
  GPU_SHADER_3D_POINT_FLAT_COLOR,

  /**
   * Take a 3D position and color for each vertex with perspective correct interpolation.
   *
   * \param color: in vec4
   * \param pos: in vec3
   */
  GPU_SHADER_3D_SMOOTH_COLOR,
  GPU_SHADER_3D_POLYLINE_SMOOTH_COLOR,

  /**
   * Take a single color for all the vertices and a 3D position for each vertex.
   *
   * \param color: uniform vec4
   * \param pos: in vec3
   */
  GPU_SHADER_3D_UNIFORM_COLOR,
  GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR,
  GPU_SHADER_3D_POINT_UNIFORM_COLOR,

  /**
   * Draw a sRGB color space texture in 3D.
   * Texture color space is assumed to match the framebuffer.
   * Take a 3D position and a 2D texture coordinate for each vertex.
   *
   * \param image: uniform sampler2D
   * \param texCoord: in vec2
   * \param pos: in vec3
   */
  GPU_SHADER_3D_IMAGE,
  /**
   * Draw a scene linear color space texture in 3D.
   * Texture value is transformed to the Rec.709 sRGB color space.
   * Take a 3D position and a 2D texture coordinate for each vertex.
   *
   * \param image: uniform sampler2D
   * \param texCoord: in vec2
   * \param pos: in vec3
   */
  GPU_SHADER_3D_IMAGE_SCENE_LINEAR_TO_REC709_SRGB,
  /**
   * Draw a sRGB color space (with Rec.709 primaries) texture in 3D.
   * Take a 3D position and color for each vertex with linear interpolation in window space.
   *
   * \param color: uniform vec4
   * \param image: uniform sampler2D
   * \param texCoord: in vec2
   * \param pos: in vec3
   */
  GPU_SHADER_3D_IMAGE_COLOR,
  /**
   * Draw a scene linear color space texture in 3D.
   * Texture value is transformed to the Rec.709 sRGB color space.
   * Take a 3D position and color for each vertex with linear interpolation in window space.
   *
   * \param color: uniform vec4
   * \param image: uniform sampler2D
   * \param texCoord: in vec2
   * \param pos: in vec3
   */
  GPU_SHADER_3D_IMAGE_COLOR_SCENE_LINEAR_TO_REC709_SRGB,
};
#define GPU_SHADER_BUILTIN_LEN (GPU_SHADER_3D_IMAGE_COLOR_SCENE_LINEAR_TO_REC709_SRGB + 1)

/** Support multiple configurations. */
enum GPUShaderConfig {
  GPU_SHADER_CFG_DEFAULT = 0,
  GPU_SHADER_CFG_CLIPPED = 1,
};
#define GPU_SHADER_CFG_LEN (GPU_SHADER_CFG_CLIPPED + 1)

blender::gpu::Shader *GPU_shader_get_builtin_shader_with_config(GPUBuiltinShader shader,
                                                                GPUShaderConfig sh_cfg);
blender::gpu::Shader *GPU_shader_get_builtin_shader(GPUBuiltinShader shader);

void GPU_shader_builtin_warm_up();

void GPU_shader_free_builtin_shaders();
