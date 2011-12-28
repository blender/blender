import bpy
is_ntsc = (bpy.context.scene.render.fps != 25)

bpy.context.scene.render.ffmpeg_format = "MPEG1"
bpy.context.scene.render.resolution_x = 352

if is_ntsc:
    bpy.context.scene.render.resolution_y = 240
    bpy.context.scene.render.ffmpeg_gopsize = 18
else:
    bpy.context.scene.render.resolution_y = 288
    bpy.context.scene.render.ffmpeg_gopsize = 15

bpy.context.scene.render.ffmpeg_video_bitrate = 1150
bpy.context.scene.render.ffmpeg_maxrate = 1150
bpy.context.scene.render.ffmpeg_minrate = 1150
bpy.context.scene.render.ffmpeg_buffersize = 40 * 8
bpy.context.scene.render.ffmpeg_packetsize = 2324
bpy.context.scene.render.ffmpeg_muxrate = 2352 * 75 * 8

bpy.context.scene.render.ffmpeg_audio_bitrate = 224
bpy.context.scene.render.ffmpeg_audio_mixrate = 44100
bpy.context.scene.render.ffmpeg_audio_codec = "MP2"
bpy.context.scene.render.ffmpeg_audio_channels = "STEREO"
