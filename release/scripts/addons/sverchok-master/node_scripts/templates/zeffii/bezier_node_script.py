# https://github.com/zeffii/bpy_script/blob/master/script_node_demos/bezier_node_script_001.blend

import mathutils
from mathutils.geometry import interpolate_bezier as bezlerp
from mathutils import Vector

def sv_main(verts=[], num_verts=20):

    in_sockets = [
        ['v', 'verts', verts],
        ['s', 'divisions', num_verts]]

    out_sockets = [ 
        ['v', 'Vecs', []], 
        ['s', 'Edges', []],
        ['v', 'Vecs ctrl', []], 
        ['s', 'Edges ctrl', []]
    ]

    if not verts:
        return in_sockets, out_sockets

    f = list(map(Vector, verts[0]))
    knot1, ctrl_1, ctrl_2, knot2 = f
    arc_verts = bezlerp(knot1, ctrl_1, ctrl_2, knot2, num_verts)

    arc_verts = [v[:] for v in arc_verts]
    arc_edges = [(n, n+1) for n in range(len(arc_verts)-1)]

    controls = verts[0]
    control_edges = [[(0,1),(2,3)]]
    
    out_sockets[0][2] = [arc_verts]
    out_sockets[1][2] = [arc_edges]

    out_sockets[2][2] = controls
    out_sockets[3][2] = control_edges


    return in_sockets, out_sockets


