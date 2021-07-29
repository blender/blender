import bpy
op = bpy.context.active_operator

op.x_eq = 'cos(v)*(1+cos(u))*sin(v/8)'
op.y_eq = 'sin(u)*sin(v/8)+cos(v/8)*1.5'
op.z_eq = 'sin(v)*(1+cos(u))*sin(v/8)'
op.range_u_min = 0.0
op.range_u_max = 6.2831854820251465
op.range_u_step = 32
op.wrap_u = True
op.range_v_min = 0.0
op.range_v_max = 12.566370964050293
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
