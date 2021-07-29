import bpy
op = bpy.context.active_operator

op.x_eq = 'cos(u)*(6-(5./4. + sin(3*v))*sin(v-3*u))'
op.y_eq = '(6-(5./4. + sin(3*v))*sin(v-3*u))*sin(u)'
op.z_eq = '-cos(v-3*u)*(5./4.+sin(3*v))'
op.range_u_min = 0.0
op.range_u_max = 6.2831854820251465
op.range_u_step = 128
op.wrap_u = True
op.range_v_min = 0.0
op.range_v_max = 6.2831854820251465
op.range_v_step = 32
op.wrap_v = True
op.close_v = True
op.n_eq = 1
op.a_eq = '0'
op.b_eq = '0'
op.c_eq = '0'
op.f_eq = '0'
op.g_eq = '0'
op.h_eq = '0'
