import bpy
op = bpy.context.active_operator

op.selected = True
op.apply_modifiers = True
op.include_bone_children = False
op.use_object_instantiation = False
op.second_life = True
