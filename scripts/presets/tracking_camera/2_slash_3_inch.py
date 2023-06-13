import bpy
camera = bpy.context.edit_movieclip.tracking.camera

camera.sensor_width = 8.8
camera.units = 'MILLIMETERS'
camera.pixel_aspect = 1
