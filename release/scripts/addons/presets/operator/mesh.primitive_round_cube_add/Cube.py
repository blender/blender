import bpy
op = bpy.context.active_operator

op.radius = 0.0
op.arc_div = 1
op.lin_div = 0
op.size = (2.0, 2.0, 2.0)
op.div_type = 'CORNERS'
op.odd_axis_align = False
