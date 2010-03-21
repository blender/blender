is_ntsc = (bpy.context.scene.render.fps != 25)

bpy.context.scene.render.ffmpeg_format = "DV"
bpy.context.scene.render.resolution_x = 720

if is_ntsc:
    bpy.context.scene.render.resolution_y = 480
else:
    bpy.context.scene.render.resolution_y = 576
