import bpy
op = bpy.context.active_operator

op.x_eq = 'a*cos(u)*sin(v)'
op.y_eq = 'a*sin(u)*sin(v)'
op.z_eq = '(cos(v)+log(tan(v/2)+1e-2)) + b*u'
op.range_u_min = 0.0
op.range_u_max = 12.566370964050293
op.range_u_step = 128
op.wrap_u = False
op.range_v_min = 0.0
op.range_v_max = 2.0
op.range_v_step = 128
op.wrap_v = False
op.close_v = False
op.n_eq = 1
op.a_eq = '1'
op.b_eq = '0.2'
op.c_eq = '0'
op.f_eq = '0'
op.g_eq = '0'
op.h_eq = '0'
