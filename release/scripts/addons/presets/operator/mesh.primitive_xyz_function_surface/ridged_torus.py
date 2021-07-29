import bpy
op = bpy.context.active_operator

op.x_eq = 'a*cos(u)+(b*sin(f*u)+c)*cos(u)*cos(v)'
op.y_eq = 'a*sin(u)+(b*sin(f*u)+c)*sin(u)*cos(v)'
op.z_eq = '(b*sin(f*u)+c)*sin(v)'
op.range_u_min = 0.0
op.range_u_max = 6.2831854820251465
op.range_u_step = 128
op.wrap_u = False
op.range_v_min = 0.0
op.range_v_max = 6.2831854820251465
op.range_v_step = 32
op.wrap_v = False
op.close_v = False
op.n_eq = 1
op.a_eq = '5'
op.b_eq = '0.6'
op.c_eq = '2'
op.f_eq = '10'
op.g_eq = '0'
op.h_eq = '0'
