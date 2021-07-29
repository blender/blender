import bpy
op = bpy.context.active_operator

op.x_eq = '(1+0.5*cos(u))*cos(v)'
op.y_eq = '0.5*sin(u)'
op.z_eq = '(1+0.5*cos(u))*sin(v)'
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
