import bpy
cycles = bpy.context.scene.cycles

cycles.max_bounces = 128
cycles.caustics_reflective = True
cycles.caustics_refractive = True
cycles.diffuse_bounces = 128
cycles.glossy_bounces = 128
cycles.transmission_bounces = 128
cycles.volume_bounces = 128
cycles.transparent_max_bounces = 128
