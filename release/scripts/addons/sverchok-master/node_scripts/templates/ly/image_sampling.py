import bpy
import numpy as np




"""
Sample an image at u,v coordinates, repeatin the texture
Images can either be selected by name or int.
Outputs image data at uv as rgb
"""


def sv_main(img_name=0, p=[[[0,0,0]]]):
    in_sockets = [
       ['s', "Image name", img_name],
       ['v', 'Point', p],
    ]

    def get_name(img_name):
        if isinstance(img_name, (str, int)):
            return img_name
        elif img_name:
            return get_name(img_name[0])
        else:
            0
    img_name = get_name(img_name)
    # if you want to look to a specific image
    #img_name = "name.jpg"
    bpy_image = bpy.data.images.get(img_name)
    if bpy_image:
        dim_x, dim_y = bpy_image.size
        tmp = np.array(bpy_image.pixels)
        image = tmp.reshape(dim_y, dim_x, 4)

    out = []
    alpha = []
    if p and bpy_image:
        points = np.array(p[0])
        uv = points * (dim_x, dim_y, 0)
        #
        for u, v, _ in uv:
            color = image[v % dim_y, u % dim_x]
            out.append(color[:3].tolist())
            alpha.append(color[-1])

    out_sockets = [
        ['v', 'Color', [out]],
        ['s', 'Alpha', [alpha]]
    ]

    return in_sockets, out_sockets
