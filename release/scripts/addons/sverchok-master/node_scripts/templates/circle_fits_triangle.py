import mathutils 
from mathutils import Vector


def sv_main(pt_a=[[]]):
 
    in_sockets = [
        ['v', 'A',  pt_a],
    ]
    
    
    normals = []
    ir_out = []
    v_out = []
    for pa in pt_a:
        if pa:
            a,b,c = pa
            va, vb, vc = Vector(a),Vector(b),Vector(c)
            A = (vb-vc).length
            B = (va-vc).length
            C = (va-vb).length
            v_out.append([(A*v[0]+B*v[1]+C*v[2])/(A+B+C) for v in zip(a,b,c)])
            ir_out.append(mathutils.geometry.area_tri(va,vb,vc)/((A+B+C)*.5))
     
    
    out_sockets = [
        ['v', 'Incenter', [v_out]],
        ['s', 'Inradius', [ir_out]],
    ]
 
    return in_sockets, out_sockets
