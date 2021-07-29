from mathutils.geometry import intersect_line_plane as ILP
from mathutils import Vector
from sverchok.data_structure import Vector_generate as VG, Vector_degenerate as VD

def sv_main(v_donor=[],v_recip=[]):
    # this node takes donor point (in top of recepient object i.e. Vector((0,0,2.1105)))
    # and projects recipient to ground
    # maybe we need define matric plane to project on.
    # Best example - hemisphere bith bottom half projection as 
    # recipient from top of upper apsent hemisphere.

    in_sockets = [
        ['v', 'v_donor', v_donor],
        ['v', 'v_recip', v_recip]]

    verts = []
    connections = []

    def out_sockets():
        return [
            ['v', 'verts', verts],
            ['v', 'connections', connections]]

    if not all([v_donor,v_recip]):
        return in_sockets, out_sockets()

    ve_do = VG(v_donor)
    ve_re = VG(v_recip)
    for do,re in zip(ve_do,ve_re):
        d = do[0]
        ve_obj = []
        for r in re:
            propoi = ILP(d,r,Vector((0,0,0)),Vector((0,0,1)))
            ve_obj.append(propoi)
            connections.append([d,propoi])
        verts.append(ve_obj)
    verts = VD(verts)
    connections = VD(connections)

    return in_sockets, out_sockets()
