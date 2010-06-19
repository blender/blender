is_ntsc = (bpy.context.scene.render.fps != 25)

bpy.context.scene.render.ffmpeg_format = "OGG"
bpy.context.scene.render.ffmpeg_codec = "THEORA"

if is_ntsc:
    bpy.context.scene.render.ffmpeg_gopsize = 18
else:
    bpy.context.scene.render.ffmpeg_gopsize = 15

bpy.context.scene.render.ffmpeg_video_bitrate = 6000
bpy.context.scene.render.ffmpeg_maxrate = 9000
bpy.context.scene.render.ffmpeg_minrate = 0
bpy.context.scene.render.ffmpeg_buffersize = 224*8
bpy.context.scene.render.ffmpeg_packetsize = 2048
bpy.context.scene.render.ffmpeg_muxrate = 10080000