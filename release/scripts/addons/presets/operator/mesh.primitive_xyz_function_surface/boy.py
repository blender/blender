import bpy
op = bpy.context.active_operator

op.x_eq = '2/3* (cos(u)* cos(2*v) + sqrt(2)* sin(u)* cos(v))* cos(u) / (sqrt(2) - sin(2*u)* sin(3*v))'
op.y_eq = 'sqrt(2)* cos(u)* cos(u) / (sqrt(2) - sin(2*u)* sin(3*v))'
op.z_eq = '2/3* (cos(u)* sin(2*v) - sqrt(2)* sin(u)* sin(v))* cos(u) / (sqrt(2) - sin(2*u)* sin(3*v))'
op.range_u_min = 0.0
op.range_u_max = 3.1415927410125732
op.range_u_step = 32
op.wrap_u = False
op.range_v_min = 0.0
op.range_v_max = 3.1415927410125732
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
