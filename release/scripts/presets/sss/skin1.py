import bpy
material = (bpy.context.material.active_node_material if bpy.context.material.active_node_material else bpy.context.material)

material.subsurface_scattering.radius = 3.673, 1.367, 0.683
material.subsurface_scattering.color = 0.574, 0.313, 0.174
