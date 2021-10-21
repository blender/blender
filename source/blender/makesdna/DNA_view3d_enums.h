/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup DNA
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Settings for off-screen rendering. */
typedef enum eV3DOffscreenDrawFlag {
  V3D_OFSDRAW_NONE = (0),
  V3D_OFSDRAW_SHOW_ANNOTATION = (1 << 0),
  V3D_OFSDRAW_OVERRIDE_SCENE_SETTINGS = (1 << 1),
  V3D_OFSDRAW_SHOW_GRIDFLOOR = (1 << 2),
  V3D_OFSDRAW_SHOW_SELECTION = (1 << 3),
  V3D_OFSDRAW_XR_SHOW_CONTROLLERS = (1 << 4),
  V3D_OFSDRAW_XR_SHOW_CUSTOM_OVERLAYS = (1 << 5),
} eV3DOffscreenDrawFlag;

/** #View3DShading.light */
typedef enum eV3DShadingLightingMode {
  V3D_LIGHTING_FLAT = 0,
  V3D_LIGHTING_STUDIO = 1,
  V3D_LIGHTING_MATCAP = 2,
} eV3DShadingLightingMode;

/** #View3DShading.color_type, #View3DShading.wire_color_type */
typedef enum eV3DShadingColorType {
  V3D_SHADING_MATERIAL_COLOR = 0,
  V3D_SHADING_RANDOM_COLOR = 1,
  V3D_SHADING_SINGLE_COLOR = 2,
  V3D_SHADING_TEXTURE_COLOR = 3,
  V3D_SHADING_OBJECT_COLOR = 4,
  V3D_SHADING_VERTEX_COLOR = 5,
} eV3DShadingColorType;

/** #View3DShading.background_type */
typedef enum eV3DShadingBackgroundType {
  V3D_SHADING_BACKGROUND_THEME = 0,
  V3D_SHADING_BACKGROUND_WORLD = 1,
  V3D_SHADING_BACKGROUND_VIEWPORT = 2,
} eV3DShadingBackgroundType;

#ifdef __cplusplus
}
#endif
