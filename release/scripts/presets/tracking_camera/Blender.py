import bpy
camera = bpy.context.edit_movieclip.tracking.camera

camera.sensor_width = 32.0
camera.units = 'MILLIMETERS'
camera.focal_length = 35.0
camera.pixel_aspect = 1
camera.k1 = 0.0
camera.k2 = 0.0
camera.k3 = 0.0
