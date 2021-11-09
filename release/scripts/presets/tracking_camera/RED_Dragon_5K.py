import bpy
camera = bpy.context.edit_movieclip.tracking.camera

camera.sensor_width = 25.60
camera.units = 'MILLIMETERS'
camera.pixel_aspect = 1
