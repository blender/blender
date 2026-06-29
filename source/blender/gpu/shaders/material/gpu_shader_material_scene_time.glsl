/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void node_scene_time(float &seconds, float &frame)
{
  scene_time_uniforms(seconds, frame);
}
