import bpy
render = bpy.context.scene.render
cycles = bpy.context.scene.cycles

render.threads_mode = 'AUTO'
render.use_persistent_data = False
cycles.debug_use_spatial_splits = False
cycles.debug_use_compact_bvh = False
cycles.debug_use_hair_bvh = True
cycles.debug_bvh_time_steps = 0
cycles.tile_size = 2048
