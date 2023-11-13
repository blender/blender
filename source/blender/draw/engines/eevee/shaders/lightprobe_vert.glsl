/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  vert_iface.vPos = vec4(pos, 1.0);
  vert_iface_flat.face = gl_InstanceID;
}
