import bpy
material = (bpy.context.material.active_node_material if bpy.context.material.active_node_material else bpy.context.material)

material.subsurface_scattering.radius = 15.028, 4.664, 2.541
material.subsurface_scattering.color = 0.987, 0.943, 0.827
