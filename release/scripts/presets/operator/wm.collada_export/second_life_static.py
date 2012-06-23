import bpy
op = bpy.context.active_operator

op.apply_modifiers = True
op.export_mesh_type = 0
op.export_mesh_type_selection = 'view'
op.selected = True
op.include_children = False
op.include_armatures = False
op.deform_bones_only = False
op.active_uv_only = True
op.include_uv_textures = True
op.use_texture_copies = True
op.use_object_instantiation = False
op.sort_by_name = True
op.second_life = False
