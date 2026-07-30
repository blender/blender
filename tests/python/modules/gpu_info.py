# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Prints GPU back-end information to the console and exits.

Use this script as `blender --background --python gpu_info.py`.
"""
import bpy
import sys
import json

# Render with workbench to initialize the GPU backend otherwise it would fail when running in
# background mode as the GPU backend won't be initialized.
scene = bpy.context.scene
scene.render.resolution_x = 1
scene.render.resolution_y = 1
scene.render.engine = "BLENDER_WORKBENCH"
bpy.ops.render.render(animation=False, write_still=False)

# Import GPU module only after GPU backend has been initialized.
import gpu

print('<GPU_INFO>')
print(json.dumps({
    "VENDOR": gpu.platform.vendor_get(),
    "RENDERER": gpu.platform.renderer_get(),
    "VERSION": gpu.platform.version_get(),
    "DEVICE_TYPE": gpu.platform.device_type_get(),
    "RAY_QUERY_SUPPORT": gpu.capabilities.ray_query_support_get(),
}))
print('</GPU_INFO>')

sys.exit(0)
