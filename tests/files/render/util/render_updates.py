# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Render the final frame of the animation with persistent data, to test
# incremental updates work.

import bpy


def render_persistent_data_animation(context):
    scene = context.scene

    scene.render.use_persistent_data = True

    for frame in range(scene.frame_start, scene.frame_end + 1):
        scene.frame_set(frame)
        bpy.ops.render.render()

    # Save the last rendered frame to the path expected by the tests.
    image = bpy.data.images['Render Result']
    image.save_render(scene.render.filepath + '0001.png', scene=scene)


if __name__ == "__main__":
    render_persistent_data_animation(bpy.context)
