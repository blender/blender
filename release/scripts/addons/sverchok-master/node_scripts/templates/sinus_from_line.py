def sv_main(data=[],step=0.3):
    
    # in boilerplate - make your own sockets
    in_sockets = [
        ['v', 'vertices',  data],
        ['s', 'Step (0...3)', step],
    ]
    
    # import libreryes - your defined
    from sverchok.data_structure import sv_zip
    from math import sin, cos
    #from random import random
    # your's code here
    step_ = (step%6-3)/1.2
    #ran = random()
    
    if data:
        out_x_ = [sin(i[0]/step_) for i in data]
        out_y_ = [cos(i[0]/step_) for i in data]
        out_z = [sin(i[0]*step_) for i in data]
        out_x = [i+sin(out_x_[k]) for k, i in enumerate(out_x_)]
        out_y = [i+cos(out_y_[k]) for k, i in enumerate(out_y_)]
        out = list(sv_zip(out_x,out_y,out_z))
        edg=[[i,i-1] for i, ed in enumerate(out_x) if i>0]
    else:
        out_x = [i*step_ for i in range(100)]
        out_y = [cos(i*step_) for i in out_x]
        out_z = [sin(i*step_)*out_y[k] for k, i in enumerate(out_y)]
        out = list(sv_zip(out_x,out_y,out_z))
        edg=[[i,i-1] for i, ed in enumerate(out_x) if i>0]
        
    # out boilerplate - set your own sockets packet
    out_sockets = [
        ['v', 'ver', [out]],
        ['s', 'edg', [edg]],
    ]

    return in_sockets, out_sockets
