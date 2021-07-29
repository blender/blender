import bmesh
from sverchok.utils.sv_bmesh_utils import *
import numpy as np

def sv_main(object=[],layout=-1):

    in_sockets = [
        ['s', 'object', object],
        ['s', 'layout', layout],
    ]

    def out_sockets(*args):
        out_sockets = [
            ['v', 'vertices', args[0]],
            ['s', 'polygons', args[1]]
        ]
        return out_sockets

    if not object:
        return in_sockets, out_sockets([],[])

    def UV(object):
        # makes UV from layout texture area to sverchok vertices and polygons.
        mesh = object.data
        bm = bmesh.new()
        bm.from_mesh(mesh)
        uv_layer = bm.loops.layers.uv[min(layout, len(bm.loops.layers.uv)-1)]

        nFaces = len(bm.faces)
        bm.verts.ensure_lookup_table()
        bm.faces.ensure_lookup_table()

        vertices_dict = {}
        polygons_new = []
        areas = []
        for fi in range(nFaces):
            polygons_new_pol = []
            areas.append(bm.faces[fi].calc_area())
            
            for loop in bm.faces[fi].loops:
                li = loop.index
                polygons_new_pol.append(li)
                vertices_dict[li] = list(loop[uv_layer].uv[:])+[0]
            polygons_new.append(polygons_new_pol)

        vertices_new = [i for i in vertices_dict.values()]

        bm_roll = bmesh_from_pydata(verts=vertices_new,edges=[],faces=polygons_new)
        bm_roll.verts.ensure_lookup_table()
        bm_roll.faces.ensure_lookup_table()
        areas_roll = []
        for fi in range(nFaces):
            areas_roll.append(bm_roll.faces[fi].calc_area())

        np_area_origin = np.array(areas).mean()
        np_area_roll = np.array(areas_roll).mean()
        mult = np.sqrt(np_area_origin/np_area_roll)

        np_ver = np.array(vertices_new)
        #(print(np_area_origin,np_area_roll,mult,'плориг, плразв, множитель'))
        vertices_new = (np_ver*mult).tolist()
        bm.clear()
        del bm
        bm_roll.clear()
        del bm_roll
        return [vertices_new], [polygons_new]
    print(object)
    v,p = UV(object[1][0])
    return in_sockets, out_sockets(v, p)
    
    

