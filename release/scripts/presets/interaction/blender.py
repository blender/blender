# Configuration Blender
import bpy

wm = bpy.context.manager
wm.active_keyconfig = wm.keyconfigs['Blender']

bpy.context.user_preferences.view.auto_depth = False
bpy.context.user_preferences.view.zoom_to_mouse = False
bpy.context.user_preferences.view.rotate_around_selection = False
bpy.context.user_preferences.edit.drag_immediately = False
bpy.context.user_preferences.edit.insertkey_xyz_to_rgb = False
bpy.context.user_preferences.inputs.select_mouse = 'RIGHT'
bpy.context.user_preferences.inputs.zoom_style = 'DOLLY'
bpy.context.user_preferences.inputs.zoom_axis = 'VERTICAL'
bpy.context.user_preferences.inputs.view_rotation = 'TRACKBALL'
bpy.context.user_preferences.inputs.invert_zoom_direction = False
