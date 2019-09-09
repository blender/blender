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

#ifndef __DNA_VIEW3D_DEFAULTS_H__
#define __DNA_VIEW3D_DEFAULTS_H__

/* Struct members on own line. */
/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name Viewport Struct
 * \{ */

#define _DNA_DEFAULT_View3DShading \
  { \
    .type = OB_SOLID, \
    .prev_type = OB_SOLID, \
    .flag = V3D_SHADING_SPECULAR_HIGHLIGHT | V3D_SHADING_XRAY_WIREFRAME | \
            V3D_SHADING_SCENE_LIGHTS_RENDER | V3D_SHADING_SCENE_WORLD_RENDER, \
    .light = V3D_LIGHTING_STUDIO, \
    .shadow_intensity = 0.5f, \
    .xray_alpha = 0.5f, \
    .xray_alpha_wire = 0.5f, \
    .cavity_valley_factor = 1.0f, \
    .cavity_ridge_factor = 1.0f, \
    .cavity_type = V3D_SHADING_CAVITY_CURVATURE, \
    .curvature_ridge_factor = 1.0f, \
    .curvature_valley_factor = 1.0f, \
    .single_color = {0.8f, 0.8f, 0.8f}, \
    .background_color = {0.05f, 0.05f, 0.05f}, \
    .studiolight_intensity = 1.0f, \
  }

#define _DNA_DEFAULT_View3DCursor \
  { \
    .rotation_mode = ROT_MODE_XYZ, \
    .rotation_quaternion = {1, 0, 0, 0}, \
    .rotation_axis = {0, 1, 0}, \
  }

/** \} */

/* clang-format on */

#endif /* __DNA_VIEW3D_DEFAULTS_H__ */
