"""
in verts_in v d=[] n=1
in obj_verts_in v d=[] n=1
in obj_faces_in s d=[] n=1
in shrink s d=0.9 n=2
in bounding_n s d=14 n=2
out new_verts v
out new_faces s
"""
from itertools import combinations
import bmesh
import mathutils
from mathutils import Vector
from collections import defaultdict

# from sverchok.utils.sv_bmesh_utils import bmesh_join
from sverchok.utils.sv_mesh_utils import mesh_join

locators = defaultdict(list)

size = len(verts_in)
kd = mathutils.kdtree.KDTree(size)
for i, xyz in enumerate(verts_in):
    kd.insert(xyz, i)
kd.balance()

for idx, vtx in enumerate(verts_in):
    n_list = kd.find_n(vtx, bounding_n)
    for co, index, dist in n_list:
        if index == idx:
            continue
        pt = (((Vector(co)-Vector(vtx))/2)*shrink) + Vector(vtx)   #((.... ) * distor) + Vector(vtx)
        normal = Vector(co)-Vector(vtx)
        locators[idx].append((pt, normal))

__new_verts = []
__new_faces = []
for k, v in locators.items():

    working_geom = bmesh_from_pydata(obj_verts_in, None, obj_faces_in, normal_update=True)
    for pt, normal in v:
        geom_in = working_geom.verts[:] + working_geom.edges[:] + working_geom.faces[:]

        res = bmesh.ops.bisect_plane(
            working_geom, geom=geom_in, dist=0.00001,
            plane_co=pt, plane_no=normal, use_snap_center=False,
            clear_outer=True, clear_inner=False
        )

        surround = [e for e in res['geom_cut'] if isinstance(e, bmesh.types.BMEdge)]
        fres = bmesh.ops.edgenet_prepare(working_geom, edges=surround)
        bmesh.ops.edgeloop_fill(working_geom, edges=fres['edges'])  

    ___v, _, ___f = pydata_from_bmesh(working_geom)
    __new_verts.append(___v)
    __new_faces.append(___f)

_v, _e, _f = mesh_join(__new_verts, [], __new_faces)

new_verts.append(_v)
new_faces.append(_f)
