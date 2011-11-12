import bpy
cycles = bpy.context.scene.cycles

cycles.max_bounces = 128
cycles.min_bounces = 3
cycles.no_caustics = False
cycles.diffuse_bounces = 128
cycles.glossy_bounces = 128
cycles.transmission_bounces = 128
cycles.transparent_min_bounces = 8
cycles.transparent_max_bounces = 128
