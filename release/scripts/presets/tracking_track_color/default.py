import bpy
track = bpy.context.edit_movieclip.tracking.active_track

track.color = (0.0, 0.0, 0.0)
track.use_custom_color = False
