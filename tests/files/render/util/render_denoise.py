# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Test for denoising Python API.

import bpy
import os


def denoise(context):
    scene = context.scene

    filepath = bpy.data.filepath

    output_filepath = filepath + ".output.exr"

    if os.path.basename(filepath).find("merge") != -1:
        # Merge Images
        input_filepath1 = filepath + ".input1.exr"
        input_filepath2 = filepath + ".input2.exr"

        bpy.ops.cycles.merge_images(
            input_filepath1=input_filepath1,
            input_filepath2=input_filepath2,
            output_filepath=output_filepath)

        input_filepath = output_filepath
    else:
        input_filepath = filepath + ".input.exr"

    if os.path.basename(filepath).find("denoise") != -1:
        # Denoise
        bpy.ops.cycles.denoise_animation(
            input_filepath=input_filepath,
            output_filepath=output_filepath)

    image = bpy.data.images.load(filepath=output_filepath)
    image.save_render(scene.render.filepath + '0001.png', scene=scene)

    os.remove(output_filepath)


if __name__ == "__main__":
    denoise(bpy.context)
