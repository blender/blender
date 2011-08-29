import bpy
cycles = bpy.context.scene.cycles

cycles.max_bounces = 0
cycles.min_bounces = 0
cycles.no_caustics = False
