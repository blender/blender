import bpy
material = (bpy.context.material.active_node_material if bpy.context.material.active_node_material else bpy.context.material)

material.subsurface_scattering.radius = 10.899, 6.575, 2.508
material.subsurface_scattering.color = 0.947, 0.931, 0.852
