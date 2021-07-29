from mathutils import Vector as V
from math import floor

def produce(verts, edg_pol, on, off):
    # on    : is real length of stride to show
    # off   : is real length of gap between strides
    # verts, edg_pol:   polygons are cast to edges first
    
    # a property of this algorithm is that it returns verts
    # that are never used by more than 1 edge.
    v_back = []
    e_back = []
    add_v = v_back.append
    add_e = e_back.append
    vert_indices = 0
    
    def make_verts(indices):
        nonlocal vert_indices

        v1, v2 = V(verts[indices[0]]), V(verts[indices[1]])
        M = (v1 - v2).length
        real = M / (on + off)
        rounded = floor(real)
        diff = real - rounded
        
        # make rounded number of on-off segments
        _a = on/M
        _b = off/M

        if diff > 0:
            rounded +=1 

        for i in range(rounded):
            a = i*(_a+_b)
            b = a + _a
            
            va = v1.lerp(v2, a)[:]
            vb = v1.lerp(v2, min(1, b))[:]

            add_v(va)
            add_v(vb)
            add_e([vert_indices, vert_indices+1])
            
            vert_indices += 2
        

    for ep in edg_pol:
        num_items = len(ep)
        if num_items == 2:
            make_verts(ep)
        elif num_items > 2:
            m = ep + [ep[0]]
            edges = [m[i:i+2] for i in range(num_items)]
            for e in edges:
                make_verts(e)
    
    return v_back, e_back

def sv_main(verts=[[]], edg_pol=[[]], dash_on=0.2, dash_off=0.1):
    verts_out = []
    edges_out = []

    in_sockets = [
        ['v', 'some_socket_name', verts],
        ['s', 'edg_pol', edg_pol],
        ['s', 'dash_on', dash_on],
        ['s', 'dash_off', dash_off]
    ]

    out_sockets = [
        ['v', 'verts', [verts_out]],
        ['s', 'edges', [edges_out]]
    ]
    
    dash_on = max(0.02, dash_on)
    dash_off = max(0.01, dash_off)
        
    if verts and verts[0]:
        verts = verts[0]
        
        if edg_pol and edg_pol[0]:
            edg_pol = edg_pol[0]
            
            Verts, Edges = produce(verts, edg_pol, dash_on, dash_off)
            verts_out.extend(Verts)
            edges_out.extend(Edges)            

    return in_sockets, out_sockets
