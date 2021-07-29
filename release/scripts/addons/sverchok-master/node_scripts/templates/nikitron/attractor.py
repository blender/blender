from sverchok.data_structure import Vector_generate, Vector_degenerate
#from mathutils import geometry as G

def sv_main(verts=[], polygons=[], ver=3, hor=3):

    in_sockets = [
        ['v', 'verts', verts],
        ['s', 'polygons', polygons],
        ['s', 'vertical', ver],
        ['s', 'horisontal', hor]]

    verts_out = []
    ver = max(3,ver)
    hor = max(3,hor)
    def out_sockets():
        return [
            ['v', 'verts_out', verts_out]]

    if not all([verts, polygons]):
        return in_sockets, out_sockets()

    def make_centers(v,p):
        #gn = G.normal([v[i] for i in p])
        out = []
        vlh = v[pol[1]]-v[pol[0]]
        vlv = v[pol[3]]-v[pol[0]]
        for i in range(hor-1):
            per_h = (1/hor)*(i+1)
            for k in range (ver-1):
                per_v = (1/ver)*(k+1)
                out.append(v[pol[0]]+vlh*per_h+vlv*per_v)
        return out
            

    # paradigm change
    verts = Vector_generate(verts)
    centers = []
    for p,v in zip(polygons,verts):
        centers_ = []
        for pol in p:
            centers_.extend(make_centers(v,pol))
        centers.append(centers_)
            


    #bpy.data.shape_keys['Key'].key_blocks['Key 1'].value
    verts_out = Vector_degenerate(centers)

    return in_sockets, out_sockets()
