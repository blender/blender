is_ntsc = (bpy.context.scene.render.fps != 25)

bpy.context.scene.render.ffmpeg_format = "MPEG2"
bpy.context.scene.render.resolution_x = 720

if is_ntsc:
    bpy.context.scene.render.resolution_y = 480
    bpy.context.scene.render.ffmpeg_gopsize = 18
else:
    bpy.context.scene.render.resolution_y = 576
    bpy.context.scene.render.ffmpeg_gopsize = 15

bpy.context.scene.render.ffmpeg_video_bitrate = 6000
bpy.context.scene.render.ffmpeg_maxrate = 9000
bpy.context.scene.render.ffmpeg_minrate = 0
bpy.context.scene.render.ffmpeg_buffersize = 224*8
bpy.context.scene.render.ffmpeg_packetsize = 2048
bpy.context.scene.render.ffmpeg_muxrate = 10080000