import bpy
op = bpy.context.active_operator
op.apply_modifiers = True
op.selected = True
op.include_children = False
op.include_armatures = True
op.deform_bones_only = True
op.use_object_instantiation = False
op.sort_by_name = True
op.second_life = True
