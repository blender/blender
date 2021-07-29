import bpy
op = bpy.context.active_operator

op.x_eq = '2.*u/(u*u+v*v+1.)'
op.y_eq = '(u*u+v*v-1.)/(u*u+v*v+1.)'
op.z_eq = '2.*v/(u*u+v*v+1.)'
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
