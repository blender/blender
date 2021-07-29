import bpy
import numpy as np



def sv_main(obj_name=0):
    in_sockets = [
       ['s', "Object name", obj_name]
    ]

    def get_name(img_name):
        if isinstance(img_name, (str, int)):
            return img_name
        elif img_name:
            return get_name(img_name[0])
        else:
            0
    obj_name = get_name(obj_name)
    # if you want to lock to a specific obj
    #obj_name = "Suzanne"
    loops = None
    if str(obj_name) in bpy.data.objects:

        obj = bpy.data.objects[obj_name]
        mesh = obj.data
        loops = mesh.loops
    out_normal = []
    out_tangent = []
    if loops:
        loop_count = len(loops)
        polygon_count = len(mesh.polygons)
        mesh.calc_tangents()
        normals = np.empty((loop_count * 3), dtype=np.float32)
        tangets = np.empty((loop_count * 3), dtype=np.float32)
        loops.foreach_get("normal", normals)
        loops.foreach_get("tangent", tangets)
        normals.shape = (loop_count, 3)
        tangets.shape = (loop_count, 3)
        out_normal = normals.tolist()
        out_tangent = tangets.tolist()

    out_sockets = [
        ['v', 'Normal', [out_normal]],
        ['v', 'Tangent', [out_tangent]],
    ]

    return in_sockets, out_sockets
