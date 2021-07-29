import bpy
op = bpy.context.active_operator

op.x_eq = 'u'
op.y_eq = 'cos(u)*sin(v)'
op.z_eq = 'cos(u)*cos(v)'
op.range_u_min = 0.0
op.range_u_max = 6.2831854820251465
op.range_u_step = 32
op.wrap_u = False
op.range_v_min = 0.0
op.range_v_max = 6.2831854820251465
op.range_v_step = 128
op.wrap_v = False
op.close_v = False
op.n_eq = 1
op.a_eq = '0'
op.b_eq = '0'
op.c_eq = '0'
op.f_eq = '0'
op.g_eq = '0'
op.h_eq = '0'
