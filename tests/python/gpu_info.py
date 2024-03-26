# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Prints GPU back-end information to the console and exits.

Use this script as `blender --background --python gpu_info.py`.
"""
import bpy
import sys

# Render with workbench to initialize the GPU backend otherwise it would fail when running in
# background mode as the GPU backend won't be initialized.
scene = bpy.context.scene
scene.render.resolution_x = 1
scene.render.resolution_y = 1
scene.render.engine = "BLENDER_WORKBENCH"
bpy.ops.render.render(animation=False, write_still=False)

# Import GPU module only after GPU backend has been initialized.
import gpu

print('GPU_VENDOR:' + gpu.platform.vendor_get())
print('GPU_RENDERER:' + gpu.platform.renderer_get())
print('GPU_VERSION:' + gpu.platform.version_get())
print('GPU_DEVICE_TYPE:' + gpu.platform.device_type_get())

sys.exit(0)
