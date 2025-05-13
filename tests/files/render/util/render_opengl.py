#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy


def render(context):
    scene = context.scene
    scene.frame_start = 1
    scene.frame_end = 1
    bpy.ops.render.opengl(animation=True)
    bpy.ops.wm.quit_blender()


if __name__ == "__main__":
    render(bpy.context)
