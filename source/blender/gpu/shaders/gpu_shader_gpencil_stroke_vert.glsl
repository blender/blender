/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

float defaultpixsize = gpencil_stroke_data.pixsize * (1000.0 / gpencil_stroke_data.pixfactor);

void main(void)
{
  gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
  geometry_in.finalColor = color;

  if (gpencil_stroke_data.keep_size) {
    geometry_in.finalThickness = thickness;
  }
  else {
    float size = (ProjectionMatrix[3][3] == 0.0) ? (thickness / (gl_Position.z * defaultpixsize)) :
                                                   (thickness / defaultpixsize);
    geometry_in.finalThickness = max(size * gpencil_stroke_data.objscale, 1.0);
  }
}
