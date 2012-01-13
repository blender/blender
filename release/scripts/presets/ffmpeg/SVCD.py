import bpy
is_ntsc = (bpy.context.scene.render.fps != 25)

bpy.context.scene.render.ffmpeg.format = "MPEG2"
bpy.context.scene.render.resolution_x = 480

if is_ntsc:
    bpy.context.scene.render.resolution_y = 480
    bpy.context.scene.render.ffmpeg.gopsize = 18
else:
    bpy.context.scene.render.resolution_y = 576
    bpy.context.scene.render.ffmpeg.gopsize = 15

bpy.context.scene.render.ffmpeg.video_bitrate = 2040
bpy.context.scene.render.ffmpeg.maxrate = 2516
bpy.context.scene.render.ffmpeg.minrate = 0
bpy.context.scene.render.ffmpeg.buffersize = 224 * 8
bpy.context.scene.render.ffmpeg.packetsize = 2324
bpy.context.scene.render.ffmpeg.muxrate = 0

bpy.context.scene.render.ffmpeg.audio_bitrate = 224
bpy.context.scene.render.ffmpeg.audio_mixrate = 44100
bpy.context.scene.render.ffmpeg.audio_codec = "MP2"
bpy.context.scene.render.ffmpeg.audio_channels = "STEREO"
