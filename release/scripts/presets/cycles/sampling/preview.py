import bpy
cycles = bpy.context.scene.cycles

# Path Trace
cycles.samples = 128
cycles.preview_samples = 32

# Branched Path Trace
cycles.aa_samples = 32
cycles.preview_aa_samples = 4

cycles.diffuse_samples = 4
cycles.glossy_samples = 4
cycles.transmission_samples = 4
cycles.ao_samples = 1
cycles.mesh_light_samples = 4
cycles.subsurface_samples = 4
cycles.volume_samples = 4
