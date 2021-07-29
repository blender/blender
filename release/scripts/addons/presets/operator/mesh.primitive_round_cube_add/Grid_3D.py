import bpy
op = bpy.context.active_operator

op.radius = 0
op.size = (2, 2, 2)
op.lin_div = 5
op.div_type = 'ALL'
