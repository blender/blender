import math

def generate_gp3d_stroke(r=1.2):

    # get grease pencil data
    # hella clunky for testing.
    grease_pencil_name = 'tc_circle_000'
    layer_name = "TinyCad Layer"

    if grease_pencil_name not in bpy.data.grease_pencil:
        gp = bpy.data.grease_pencil.new(grease_pencil_name)
    else:
        gp = bpy.data.grease_pencil[grease_pencil_name]

    # get grease pencil layer
    if not (layer_name in gp.layers):
        layer = gp.layers.new(layer_name)
        layer.frames.new(1)
        layer.line_width = 1
    else:
        layer = gp.layers[layer_name]
        layer.frames[0].clear()
    
    layer.show_points = True
    layer.color = (0.2, 0.90, .2)
    s = layer.frames[0].strokes.new()
    s.draw_mode = '3DSPACE'

    num_verts = 14
    chain = []
    for i in range(num_verts+1):
        # mat_rot = mathutils.Matrix.Rotation(((360 / num_verts) * i), 4, axis)
        # chain.append(((v1 - p1) * mat_rot) + p1)
        theta = (2*math.pi / num_verts) * i
        coord = ((math.sin(theta) * r, math.cos(theta) * r, 0))
        print(coord)
        chain.append(coord)

    s.points.add(len(chain))
    for idx, p in enumerate(chain):
        s.points[idx].co = p


def sv_main(radius=0.3):
    verts_out = []

    in_sockets = [
        ['s', 'radius', radius]
    ]

    out_sockets = [
        ['v', 'verts', verts_out]
    ]

    generate_gp3d_stroke(radius)

    return in_sockets, out_sockets
