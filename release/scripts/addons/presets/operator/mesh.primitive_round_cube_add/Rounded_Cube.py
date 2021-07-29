import bpy
op = bpy.context.active_operator

op.radius = 0.25
op.arc_div = 8
op.lin_div = 0
op.size = (2.0, 2.0, 2.0)
op.div_type = 'CORNERS'
