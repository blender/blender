import bpy
op = bpy.context.active_operator

op.x_eq = 'sinh(v)*sin(u)'
op.y_eq = '3*u'
op.z_eq = '-sinh(v)*cos(u)'
op.range_u_min = -3.1415927410125732
op.range_u_max = 3.1415927410125732
op.range_u_step = 32
op.wrap_u = False
op.range_v_min = -3.1415927410125732
op.range_v_max = 3.1415927410125732
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
