import bpy
material = (bpy.context.material.active_node_material if bpy.context.material.active_node_material else bpy.context.material)

material.subsurface_scattering.radius = 8.509, 5.566, 3.951
material.subsurface_scattering.color = 0.925, 0.905, 0.884
