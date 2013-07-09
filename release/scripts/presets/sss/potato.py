import bpy
material = (bpy.context.material.active_node_material if bpy.context.material.active_node_material else bpy.context.material)

material.subsurface_scattering.radius = 14.266, 7.228, 2.036
material.subsurface_scattering.color = 0.855, 0.740, 0.292
