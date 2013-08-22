import bpy
cycles = bpy.context.scene.cycles

cycles.use_square_samples = True

# Progressive
cycles.samples = 24
cycles.preview_samples = 12

# Non-Progressive
cycles.aa_samples = 8
cycles.preview_aa_samples = 4

cycles.diffuse_samples = 3
cycles.glossy_samples = 2
cycles.transmission_samples = 2
cycles.ao_samples = 1
cycles.mesh_light_samples = 2
cycles.subsurface_samples = 2
