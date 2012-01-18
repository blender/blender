import bpy
is_ntsc = (bpy.context.scene.render.fps != 25)

bpy.context.scene.render.ffmpeg.format = "XVID"

if is_ntsc:
    bpy.context.scene.render.ffmpeg.gopsize = 18
else:
    bpy.context.scene.render.ffmpeg.gopsize = 15

bpy.context.scene.render.ffmpeg.video_bitrate = 6000
bpy.context.scene.render.ffmpeg.maxrate = 9000
bpy.context.scene.render.ffmpeg.minrate = 0
bpy.context.scene.render.ffmpeg.buffersize = 224 * 8
bpy.context.scene.render.ffmpeg.packetsize = 2048
bpy.context.scene.render.ffmpeg.muxrate = 10080000
