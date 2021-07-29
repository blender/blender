import bpy
op = bpy.context.active_operator

op.x_eq = 'u -u**3/3  + u*v**2'
op.y_eq = 'u**2 - v**2'
op.z_eq = 'v -v**3/3  + v*u**2'
op.range_u_min = -2.0
op.range_u_max = 2.0
op.range_u_step = 32
op.wrap_u = False
op.range_v_min = -2.0
op.range_v_max = 2.0
op.range_v_step = 32
op.wrap_v = False
op.close_v = False
op.n_eq = 1
op.a_eq = '0'
op.b_eq = '0'
op.c_eq = '0'
op.f_eq = '0'
op.g_eq = '0'
op.h_eq = '0'
