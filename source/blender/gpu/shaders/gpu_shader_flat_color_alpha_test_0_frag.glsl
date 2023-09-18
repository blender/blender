/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  if (finalColor.a > 0.0) {
    fragColor = finalColor;
  }
  else {
    discard;
  }
}
