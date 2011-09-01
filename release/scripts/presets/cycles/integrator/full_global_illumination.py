import bpy
cycles = bpy.context.scene.cycles

cycles.max_bounces = 1024
cycles.min_bounces = 3
cycles.no_caustics = False
cycles.diffuse_bounces = 1024
cycles.glossy_bounces = 1024
cycles.transmission_bounces = 1024
cycles.transparent_min_bounces = 8
cycles.transparent_max_bounces = 1024
