/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  fragColor = gl_FrontFacing ? colorFaceFront : colorFaceBack;
}
