import bpy
material = (bpy.context.material.active_node_material if bpy.context.material.active_node_material else bpy.context.material)

material.subsurface_scattering.radius = 4.821, 1.694, 1.090
material.subsurface_scattering.color = 0.749, 0.571, 0.467
