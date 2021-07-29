import bpy
op = bpy.context.active_operator

op.x_eq = 'cos(u)*cos(v)+sin((sin(u)+1)*2*pi) '
op.y_eq = '4*sin(u) '
op.z_eq = 'cos(u)*sin(v)+cos((sin(u)+1)*2*pi) '
op.range_u_min = -1.5707963705062866
op.range_u_max = 1.5707963705062866
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
