import bpy
op = bpy.context.active_operator

op.x_eq = 'u-sin(u)*cosh(v)'
op.y_eq = '4*sin(1/2*u)*sinh(v/2)'
op.z_eq = '1-cos(u)*cosh(v)'
op.range_u_min = -3.1415927410125732
op.range_u_max = 9.42477798461914
op.range_u_step = 32
op.wrap_u = False
op.range_v_min = -2.0
op.range_v_max = 2.0
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
