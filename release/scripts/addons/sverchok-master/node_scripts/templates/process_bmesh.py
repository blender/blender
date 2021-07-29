import bpy
import bmesh
from sverchok.utils.sv_bmesh_utils import pydata_from_bmesh

def sv_main(radius=0.3):
    verts_out = []
    edges_out = []
    faces_out = []

    in_sockets = [
        ['s', 'radius', radius]
    ]

    bm = bmesh.new()
    objname = "Plane"
    obj = bpy.data.objects.get(objname)

    if obj:

        bm.from_mesh(obj.data)

        verts, edges, faces = pydata_from_bmesh(bm)

        verts_out.append([verts])
        edges_out.append([edges])
        bm.free() 

    out_sockets = [
        ['v', 'verts', verts_out],
        ['s', 'edges', edges_out],
        ['s', 'faces', faces_out]
    ]

    return in_sockets, out_sockets
