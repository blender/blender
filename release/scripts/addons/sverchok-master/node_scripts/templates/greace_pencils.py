def sv_main(a=0):

    in_sockets = [
        ['s', 'nothing',  a],
        ]

    if bpy.data.grease_pencil:
        verts_out = [[[[p.co[:] for p in str.points] for str in gl.active_frame.strokes] for gl in gp.layers] for gp in bpy.data.grease_pencil]
    else:
        verts_out = [[]]

    out_sockets = [
        ['v', 'verts', verts_out]
    ]

    return in_sockets, out_sockets
