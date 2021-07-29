from mathutils import Vector
from math import sin, cos, radians

def sv_main(Vecs=[],Edgs=[],Radius=1.0):
    
    in_sockets = [("v","Vecs",Vecs),
                  ("s","Edgs",Edgs),
                  ("s","Radius",Radius)]
 
    def Do_vecs(Vecs,Edgs,Radius):
        circle = [ (Vector((sin(radians(i)),cos(radians(i)),0))*Radius) \
                              for i in range(0,360,30) ]
        outv = []
        outp = []
        for E,V in zip(Edgs,Vecs):
            outv_ = []
            outp_ = []
            k = 0
            for e in E:
                v2,v1 = Vector(V[e[1]]),Vector(V[e[0]])
                vecdi = v2-v1
                matrix_rot = vecdi.rotation_difference(Vector((0,0,1))).to_matrix().to_4x4()
                verts1 = [ (ve*matrix_rot+v1)[:] for ve in circle ]
                verts2 = [ (ve*matrix_rot+v2)[:] for ve in circle ]
                outv_.extend(verts1)
                outv_.extend(verts2)
                pols = [ [k+i+0,k+i-1,k+i+11,k+i+12] for i in range(1,12,1) ]
                pols.append([k+0,k+11,k+23,k+12])
                k += 24
                outp_.extend(pols)
            outv.append(outv_)
            outp.append(outp_)
        return outv, outp

    Vecs, Edgs = Do_vecs(Vecs,Edgs,Radius)

    out_sockets = [("v","Vecs",Vecs),
                  ("s","Edgs",Edgs)]
    return in_sockets, out_sockets