import bpy
cycles = bpy.context.scene.cycles

cycles.max_bounces = 8
cycles.min_bounces = 8
cycles.no_caustics = True
cycles.diffuse_bounces = 0
cycles.glossy_bounces = 1
cycles.transmission_bounces = 2
cycles.volume_bounces = 0
cycles.transparent_min_bounces = 8
cycles.transparent_max_bounces = 8
