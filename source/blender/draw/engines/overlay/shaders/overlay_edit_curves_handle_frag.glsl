/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  /* In the layout of index buffer for curves handles is:
   *  [ left bezier handles, right bezier handles, NURBS handles].
   * So first bezier_point_count lines will use leftColor. All other will be using finalColor as
   * vertex shader stores right handles color in finalColor variable.
   */
  int bezier_point_count = curvesInfoBlock[0];
  fragColor = gl_PrimitiveID < bezier_point_count ? leftColor : finalColor;
}
