import bpy
is_ntsc = (bpy.context.scene.render.fps != 25)

bpy.context.scene.render.ffmpeg.format = "MPEG1"
bpy.context.scene.render.resolution_x = 352

if is_ntsc:
    bpy.context.scene.render.resolution_y = 240
    bpy.context.scene.render.ffmpeg.gopsize = 18
else:
    bpy.context.scene.render.resolution_y = 288
    bpy.context.scene.render.ffmpeg.gopsize = 15

bpy.context.scene.render.ffmpeg.video_bitrate = 1150
bpy.context.scene.render.ffmpeg.maxrate = 1150
bpy.context.scene.render.ffmpeg.minrate = 1150
bpy.context.scene.render.ffmpeg.buffersize = 40 * 8
bpy.context.scene.render.ffmpeg.packetsize = 2324
bpy.context.scene.render.ffmpeg.muxrate = 2352 * 75 * 8

bpy.context.scene.render.ffmpeg.audio_bitrate = 224
bpy.context.scene.render.ffmpeg.audio_mixrate = 44100
bpy.context.scene.render.ffmpeg.audio_codec = "MP2"
bpy.context.scene.render.ffmpeg.audio_channels = "STEREO"
