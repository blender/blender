bcsr = bpy.context.scene.render


def sv_main(st=[], size=12):

    in_sockets = [
        ['s', 'Formula2:"txt"+str(x)', st],
        ['s', 'Size of Note(int8-64)', size]
        ]

    if st:

        bcsr.use_stamp = 1
        bcsr.use_stamp_note = 1
        bpy.context.scene.use_stamp_note = str(st[0][0][0])

        if size:
            bcsr.stamp_font_size = size

    def func1():
        bcsr.use_stamp_camera = 0
        bcsr.use_stamp_date = 0
        bcsr.use_stamp_filename = 0
        bcsr.use_stamp_frame = 0
        bcsr.use_stamp_lens = 0
        bcsr.use_stamp_marker = 0
        bcsr.use_stamp_render_time = 0
        bcsr.use_stamp_scene = 0
        bcsr.use_stamp_sequencer_strip = 0
        bcsr.use_stamp_time = 0

    out_sockets = [
        ['v', 'Void', []]
    ]

    ui_operators = [
        ['Unchek_Others', func1]
    ]

    return in_sockets, out_sockets, ui_operators
