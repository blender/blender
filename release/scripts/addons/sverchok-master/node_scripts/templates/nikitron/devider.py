from sverchok.data_structure import (Vector_generate, Vector_degenerate,
                                    Matrix_generate, Matrix_listing)
#from mathutils import geometry as G

def sv_main(verts=[], polygons=[], matrix=[], ver=3, hor=3):

    in_sockets = [
        ['v', 'verts', verts],
        ['s', 'polygons', polygons],
        ['m', 'matrix', matrix],
        ['s', 'vertical', ver],
        ['s', 'horisontal', hor]]

    verts_out = []
    matrixes = []
    ver = max(3,ver)
    hor = max(3,hor)
    def out_sockets():
        return [
            ['v', 'verts_out', verts_out],
            ['m', 'matrixes', matrixes]]

    if not all([verts, polygons]):
        return in_sockets, out_sockets()

    def make_centers(v,p,m):
        out = []
        matrixes = []
        vlh = v[pol[1]]-v[pol[0]]
        vlv = v[pol[3]]-v[pol[0]]
        for i in range(hor-1):
            per_h = (1/hor)*(i+1)
            for k in range (ver-1):
                per_v = (1/ver)*(k+1)
                v_loc = v[pol[0]]+vlh*per_h+vlv*per_v
                m.translation = v_loc
                matrixes.append(m.copy())
                out.append(v_loc)
        return out, matrixes
            

    # paradigm change
    verts = Vector_generate(verts)
    matrix = Matrix_generate(matrix)
    centers = []
    for p,v in zip(polygons,verts):
        centers_ = []
        for pol, m in zip(p,matrix):
            cout, mout = make_centers(v,pol,m)
            centers_.extend(cout)
            matrixes.extend(mout)
        centers.append(centers_)
            

    #print(centers,matrixes)
    #bpy.data.shape_keys['Key'].key_blocks['Key 1'].value
    verts_out = Vector_degenerate(centers)
    matrixes = Matrix_listing(matrixes)

    return in_sockets, out_sockets()
