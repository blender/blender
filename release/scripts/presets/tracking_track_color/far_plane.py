import bpy
track = bpy.context.edit_movieclip.tracking.tracks.active

track.color = (0.0, 0.0, 1.0)
track.use_custom_color = True
