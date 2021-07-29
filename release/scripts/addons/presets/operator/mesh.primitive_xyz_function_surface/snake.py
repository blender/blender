import bpy
op = bpy.context.active_operator

op.x_eq = '1.2*(1 -v/(2*pi))*cos(3*v)*(1 + cos(u)) + 3*cos(3*v)'
op.y_eq = '9*v/(2*pi) + 1.2*(1 - v/(2*pi))*sin(u)'
op.z_eq = '1.2*(1 -v/(2*pi))*sin(3*v)*(1 + cos(u)) + 3*sin(3*v)'
op.range_u_min = 0.0
op.range_u_max = 6.2831854820251465
op.range_u_step = 32
op.wrap_u = False
op.range_v_min = 0.0
op.range_v_max = 6.2831854820251465
op.range_v_step = 64
op.wrap_v = False
op.close_v = False
op.n_eq = 1
op.a_eq = '0'
op.b_eq = '0'
op.c_eq = '0'
op.f_eq = '0'
op.g_eq = '0'
op.h_eq = '0'
