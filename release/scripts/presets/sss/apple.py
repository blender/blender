import bpy
material = (bpy.context.material.active_node_material if bpy.context.material.active_node_material else bpy.context.material)

material.subsurface_scattering.radius = 11.605, 3.884, 1.754
material.subsurface_scattering.color = 0.430, 0.210, 0.168
