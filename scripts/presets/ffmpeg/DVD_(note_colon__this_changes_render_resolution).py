import bpy
is_ntsc = (bpy.context.scene.render.fps != 25)

bpy.context.scene.render.ffmpeg.format = "MPEG2"
bpy.context.scene.render.resolution_x = 720

if is_ntsc:
    bpy.context.scene.render.resolution_y = 480
    bpy.context.scene.render.ffmpeg.gopsize = 18
else:
    bpy.context.scene.render.resolution_y = 576
    bpy.context.scene.render.ffmpeg.gopsize = 15

bpy.context.scene.render.ffmpeg.video_bitrate = 6000
bpy.context.scene.render.ffmpeg.maxrate = 9000
bpy.context.scene.render.ffmpeg.minrate = 0
bpy.context.scene.render.ffmpeg.buffersize = 224 * 8
bpy.context.scene.render.ffmpeg.packetsize = 2048
bpy.context.scene.render.ffmpeg.muxrate = 10080000

bpy.context.scene.render.ffmpeg.audio_codec = "AC3"
bpy.context.scene.render.ffmpeg.audio_bitrate = 448
bpy.context.scene.render.ffmpeg.audio_mixrate = 48000
bpy.context.scene.render.ffmpeg.audio_channels = "SURROUND51"
