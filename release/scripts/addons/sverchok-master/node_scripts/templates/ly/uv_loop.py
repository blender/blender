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
    # if you want to look to a specific image
    #obj_name = "Suzanne"
    uv_layer = None
    if str(obj_name) in bpy.data.objects:

        obj = bpy.data.objects[obj_name]
        mesh = obj.data
        uv_layer = mesh.uv_layers.active
    out = []
    if uv_layer:
        uv_count = len(uv_layer.data)
        uvs = np.zeros((uv_count, 3), dtype=np.float32)
        uvs_tmp = np.empty((uv_count, 2), dtype=np.float32)
        uvs_tmp.shape = uv_count * 2
        uv_layer.data.foreach_get("uv", uvs_tmp)
        uvs_tmp.shape = (uv_count, 2)
        uvs[:, :2] = uvs_tmp
        out = uvs.tolist()

    out_sockets = [
        ['v', 'UV loops', [out]]
    ]

    return in_sockets, out_sockets
