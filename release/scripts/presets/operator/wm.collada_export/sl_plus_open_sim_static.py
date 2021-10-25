import bpy
op = bpy.context.active_operator

op.apply_modifiers = True
op.export_mesh_type = 0
op.export_mesh_type_selection = 'view'
op.selected = True
op.include_children = False
op.include_armatures = False
op.include_shapekeys = False
op.deform_bones_only = False
op.active_uv_only = True
op.export_texture_type_selection = 'uv'
op.use_texture_copies = True
op.triangulate = True
op.use_object_instantiation = False
op.sort_by_name = True
op.open_sim = False
