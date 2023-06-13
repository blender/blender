"""Sets up FFmpeg to output files that can easily be scrubbed through.

Information was taken from https://trac.ffmpeg.org/wiki/Encode/VFX#H.264
"""

import bpy

bpy.context.scene.render.ffmpeg.format = "MKV"
bpy.context.scene.render.ffmpeg.codec = "H264"

bpy.context.scene.render.ffmpeg.gopsize = 1
bpy.context.scene.render.ffmpeg.constant_rate_factor = 'PERC_LOSSLESS'
bpy.context.scene.render.ffmpeg.use_max_b_frames = True
bpy.context.scene.render.ffmpeg.max_b_frames = 0
