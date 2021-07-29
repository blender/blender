# demo blend
# https://github.com/zeffii/bpy_script/blob/master/script_node_demos/bezier_node_script_003_mod.blend

import mathutils
from mathutils.geometry import interpolate_bezier as bezlerp
from mathutils import Vector

import spline_utils
from spline_utils import get_length
from spline_utils import get_verts_n_edges


def sv_main(verts=[], num_verts=20, new_divisions=20):
    '''
    verts is expecting 4 (unique) 3d coordinates
    '''

    in_sockets = [
        ['v', 'verts', verts],
        ['s', 'num_verts', num_verts],
        ['s', 'new_divisions', new_divisions]]

    out_sockets = [ 
        ['v', 'Vecs', []], 
        ['s', 'Edges', []],
        ['v', 'Vecs ctrl', []], 
        ['s', 'Edges ctrl', []],
        ['v', 'Vecs norm', []],
        ['s', 'Edges norm', []]
    ]

    if not verts:
        return in_sockets, out_sockets

    # defend against div by zero
    new_divisions = max(new_divisions, 1)
    
    # while developing, it can be useful to uncomment this 
    if 'get_length' in globals():
        import imp
        imp.reload(spline_utils)
        from spline_utils import get_length, get_verts_n_edges


    f = list(map(Vector, verts[0]))
    knot1, ctrl_1, ctrl_2, knot2 = f
    arc_verts = bezlerp(knot1, ctrl_1, ctrl_2, knot2, num_verts)
    farc_verts = bezlerp(knot1, ctrl_1, ctrl_2, knot2, 870)

    tlen, lengths = get_length(farc_verts)
    # print(tlen)
    segment_width = tlen/new_divisions
    print('new div length = ', segment_width)
    k = get_verts_n_edges(farc_verts, lengths, segment_width)

    arc_verts = [v[:] for v in arc_verts]
    arc_edges = [(n, n+1) for n in range(len(arc_verts)-1)]
    
    norm_verts = [v[:] for v in k]
    norm_edges = [(n, n+1) for n in range(len(k)-1)]
    

    controls = verts[0]
    control_edges = [[(0,1),(2,3)]]
    
    out_sockets[0][2] = [arc_verts]
    out_sockets[1][2] = [arc_edges]

    out_sockets[2][2] = [controls]
    out_sockets[3][2] = control_edges

    out_sockets[4][2] = [norm_verts]
    out_sockets[5][2] = [norm_edges]


    return in_sockets, out_sockets


