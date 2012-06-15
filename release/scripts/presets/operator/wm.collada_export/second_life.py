import bpy
op = bpy.context.active_operator

op.selected = True
op.apply_modifiers = True
op.include_armatures = False
op.include_children = False
op.use_object_instantiation = False
op.sort_by_name = True
op.second_life = True
