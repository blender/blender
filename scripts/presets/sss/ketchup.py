import bpy
material = (bpy.context.material.active_node_material if bpy.context.material.active_node_material else bpy.context.material)

material.subsurface_scattering.radius = 4.762, 0.575, 0.394
material.subsurface_scattering.color = 0.222, 0.008, 0.002
