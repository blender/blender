# Configuration Maya http://blenderartists.org/forum/showpost.php?p=1609106&postcount=29
import bpy

wm = bpy.context.manager
kc = wm.add_keyconfig('Maya')

# Map Window
km = kc.add_keymap('Window', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.items.add('wm.window_duplicate', 'W', 'PRESS', ctrl=True, alt=True)
kmi = km.items.add('wm.read_homefile', 'N', 'PRESS', ctrl=True)
kmi = km.items.add('wm.save_homefile', 'U', 'PRESS', ctrl=True)
kmi = km.items.add('wm.call_menu', 'O', 'PRESS', shift=True, ctrl=True)
kmi.properties.name = 'INFO_MT_file_open_recent'
kmi = km.items.add('wm.open_mainfile', 'O', 'PRESS', ctrl=True)
kmi = km.items.add('wm.open_mainfile', 'F1', 'PRESS')
kmi = km.items.add('wm.link_append', 'O', 'PRESS', ctrl=True, alt=True)
kmi = km.items.add('wm.link_append', 'F1', 'PRESS', shift=True)
kmi.properties.instance_groups = False
kmi.properties.link = False
kmi = km.items.add('wm.save_mainfile', 'S', 'PRESS', ctrl=True)
kmi = km.items.add('wm.save_mainfile', 'W', 'PRESS', ctrl=True)
kmi = km.items.add('wm.save_as_mainfile', 'S', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('wm.save_as_mainfile', 'F2', 'PRESS')
kmi = km.items.add('wm.window_fullscreen_toggle', 'F11', 'PRESS', alt=True)
kmi = km.items.add('wm.exit_blender', 'Q', 'PRESS', ctrl=True)
kmi = km.items.add('wm.redraw_timer', 'T', 'PRESS', ctrl=True, alt=True)
kmi = km.items.add('wm.debug_menu', 'D', 'PRESS', ctrl=True, alt=True)
kmi = km.items.add('wm.search_menu', 'LEFTMOUSE', 'DOUBLE_CLICK', shift=True)
kmi = km.items.add('wm.context_set_enum', 'F2', 'PRESS', shift=True)
kmi.properties.path = 'area.type'
kmi.properties.value = 'LOGIC_EDITOR'
kmi = km.items.add('wm.context_set_enum', 'F3', 'PRESS', shift=True)
kmi.properties.path = 'area.type'
kmi.properties.value = 'NODE_EDITOR'
kmi = km.items.add('wm.context_set_enum', 'F4', 'PRESS', shift=True)
kmi.properties.path = 'area.type'
kmi.properties.value = 'CONSOLE'
kmi = km.items.add('wm.context_set_enum', 'F5', 'PRESS', shift=True)
kmi.properties.path = 'area.type'
kmi.properties.value = 'VIEW_3D'
kmi = km.items.add('wm.context_set_enum', 'F6', 'PRESS', shift=True)
kmi.properties.path = 'area.type'
kmi.properties.value = 'GRAPH_EDITOR'
kmi = km.items.add('wm.context_set_enum', 'F7', 'PRESS', shift=True)
kmi.properties.path = 'area.type'
kmi.properties.value = 'PROPERTIES'
kmi = km.items.add('wm.context_set_enum', 'F8', 'PRESS', shift=True)
kmi.properties.path = 'area.type'
kmi.properties.value = 'SEQUENCE_EDITOR'
kmi = km.items.add('wm.context_set_enum', 'F9', 'PRESS', shift=True)
kmi.properties.path = 'area.type'
kmi.properties.value = 'OUTLINER'
kmi = km.items.add('wm.context_set_enum', 'F10', 'PRESS', shift=True)
kmi.properties.path = 'area.type'
kmi.properties.value = 'IMAGE_EDITOR'
kmi = km.items.add('wm.context_set_enum', 'F11', 'PRESS', shift=True)
kmi.properties.path = 'area.type'
kmi.properties.value = 'TEXT_EDITOR'
kmi = km.items.add('wm.context_set_enum', 'F12', 'PRESS', shift=True)
kmi.properties.path = 'area.type'
kmi.properties.value = 'DOPESHEET_EDITOR'

# Map Screen
km = kc.add_keymap('Screen', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.items.add('screen.animation_step', 'TIMER0', 'ANY', any=True)
kmi = km.items.add('screen.screen_set', 'RIGHT_ARROW', 'PRESS', ctrl=True)
kmi.properties.delta = 1
kmi = km.items.add('screen.screen_set', 'LEFT_ARROW', 'PRESS', ctrl=True)
kmi.properties.delta = -1
kmi = km.items.add('screen.screen_full_area', 'SPACE', 'PRESS', ctrl=True)
kmi = km.items.add('screen.screen_full_area', 'DOWN_ARROW', 'PRESS', ctrl=True)
kmi = km.items.add('screen.screen_full_area', 'SPACE', 'PRESS', shift=True)
kmi = km.items.add('screen.screenshot', 'F3', 'PRESS', ctrl=True)
kmi = km.items.add('screen.screencast', 'F3', 'PRESS', alt=True)
kmi = km.items.add('screen.region_quadview', 'SPACE', 'PRESS')
kmi = km.items.add('screen.repeat_history', 'F3', 'PRESS')
kmi = km.items.add('screen.repeat_last', 'R', 'PRESS', shift=True)
kmi = km.items.add('screen.region_flip', 'F5', 'PRESS')
kmi = km.items.add('screen.redo_last', 'F6', 'PRESS')
kmi = km.items.add('wm.reload_scripts', 'F8', 'PRESS')
kmi = km.items.add('file.execute', 'RET', 'PRESS')
kmi = km.items.add('file.execute', 'NUMPAD_ENTER', 'PRESS')
kmi = km.items.add('file.cancel', 'ESC', 'PRESS')
kmi = km.items.add('ed.undo', 'Z', 'PRESS', ctrl=True)
kmi = km.items.add('ed.redo', 'Z', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('render.render', 'F12', 'PRESS')
kmi = km.items.add('render.render', 'F12', 'PRESS', ctrl=True)
kmi.properties.animation = True
kmi = km.items.add('render.view_cancel', 'ESC', 'PRESS')
kmi = km.items.add('render.view_show', 'F11', 'PRESS')
kmi = km.items.add('render.play_rendered_anim', 'F11', 'PRESS', ctrl=True)
kmi = km.items.add('screen.userpref_show', 'U', 'PRESS', ctrl=True, alt=True)

# Map 3D View
km = kc.add_keymap('3D View', space_type='VIEW_3D', region_type='WINDOW', modal=False)

kmi = km.items.add('view3d.manipulator', 'LEFTMOUSE', 'PRESS', any=True)
kmi.properties.release_confirm = True
kmi = km.items.add('view3d.cursor3d', 'RIGHTMOUSE', 'DOUBLE_CLICK', shift=True)
kmi = km.items.add('view3d.rotate', 'LEFTMOUSE', 'PRESS', alt=True)
kmi = km.items.add('view3d.move', 'MIDDLEMOUSE', 'PRESS', alt=True)
kmi = km.items.add('view3d.zoom', 'RIGHTMOUSE', 'PRESS', alt=True)
kmi = km.items.add('view3d.view_selected', 'F', 'PRESS')
kmi = km.items.add('view3d.view_center_cursor', 'NUMPAD_PERIOD', 'PRESS', ctrl=True)
kmi = km.items.add('view3d.fly', 'F', 'PRESS', shift=True)
kmi = km.items.add('view3d.smoothview', 'TIMER1', 'ANY', any=True)
kmi = km.items.add('view3d.rotate', 'TRACKPADPAN', 'ANY', alt=True)
kmi = km.items.add('view3d.rotate', 'MOUSEROTATE', 'ANY')
kmi = km.items.add('view3d.move', 'TRACKPADPAN', 'ANY')
kmi = km.items.add('view3d.zoom', 'TRACKPADZOOM', 'ANY')
kmi = km.items.add('view3d.zoom', 'NUMPAD_PLUS', 'PRESS')
kmi.properties.delta = 1
kmi = km.items.add('view3d.zoom', 'NUMPAD_MINUS', 'PRESS')
kmi.properties.delta = -1
kmi = km.items.add('view3d.zoom', 'EQUAL', 'PRESS', ctrl=True)
kmi.properties.delta = 1
kmi = km.items.add('view3d.zoom', 'MINUS', 'PRESS', ctrl=True)
kmi.properties.delta = -1
kmi = km.items.add('view3d.zoom', 'WHEELINMOUSE', 'PRESS')
kmi.properties.delta = 1
kmi = km.items.add('view3d.zoom', 'WHEELOUTMOUSE', 'PRESS')
kmi.properties.delta = -1
kmi = km.items.add('view3d.view_all', 'A', 'PRESS')
kmi.properties.center = False
kmi = km.items.add('view3d.view_all', 'C', 'PRESS', shift=True)
kmi.properties.center = True
kmi = km.items.add('view3d.viewnumpad', 'NUMPAD_0', 'PRESS')
kmi.properties.type = 'CAMERA'
kmi = km.items.add('view3d.viewnumpad', 'NUMPAD_1', 'PRESS')
kmi.properties.type = 'FRONT'
kmi = km.items.add('view3d.view_orbit', 'NUMPAD_2', 'PRESS')
kmi.properties.type = 'ORBITDOWN'
kmi = km.items.add('view3d.viewnumpad', 'NUMPAD_3', 'PRESS')
kmi.properties.type = 'RIGHT'
kmi = km.items.add('view3d.view_orbit', 'NUMPAD_4', 'PRESS')
kmi.properties.type = 'ORBITLEFT'
kmi = km.items.add('view3d.view_persportho', 'NUMPAD_5', 'PRESS')
kmi = km.items.add('view3d.view_orbit', 'NUMPAD_6', 'PRESS')
kmi.properties.type = 'ORBITRIGHT'
kmi = km.items.add('view3d.viewnumpad', 'NUMPAD_7', 'PRESS')
kmi.properties.type = 'TOP'
kmi = km.items.add('view3d.view_orbit', 'NUMPAD_8', 'PRESS')
kmi.properties.type = 'ORBITUP'
kmi = km.items.add('view3d.viewnumpad', 'NUMPAD_1', 'PRESS', ctrl=True)
kmi.properties.type = 'BACK'
kmi = km.items.add('view3d.viewnumpad', 'NUMPAD_3', 'PRESS', ctrl=True)
kmi.properties.type = 'LEFT'
kmi = km.items.add('view3d.viewnumpad', 'NUMPAD_7', 'PRESS', ctrl=True)
kmi.properties.type = 'BOTTOM'
kmi = km.items.add('view3d.view_pan', 'NUMPAD_2', 'PRESS', ctrl=True)
kmi.properties.type = 'PANDOWN'
kmi = km.items.add('view3d.view_pan', 'NUMPAD_4', 'PRESS', ctrl=True)
kmi.properties.type = 'PANLEFT'
kmi = km.items.add('view3d.view_pan', 'NUMPAD_6', 'PRESS', ctrl=True)
kmi.properties.type = 'PANRIGHT'
kmi = km.items.add('view3d.view_pan', 'NUMPAD_8', 'PRESS', ctrl=True)
kmi.properties.type = 'PANUP'
kmi = km.items.add('view3d.view_pan', 'WHEELUPMOUSE', 'PRESS', ctrl=True)
kmi.properties.type = 'PANRIGHT'
kmi = km.items.add('view3d.view_pan', 'WHEELDOWNMOUSE', 'PRESS', ctrl=True)
kmi.properties.type = 'PANLEFT'
kmi = km.items.add('view3d.view_pan', 'WHEELUPMOUSE', 'PRESS', shift=True)
kmi.properties.type = 'PANUP'
kmi = km.items.add('view3d.view_pan', 'WHEELDOWNMOUSE', 'PRESS', shift=True)
kmi.properties.type = 'PANDOWN'
kmi = km.items.add('view3d.view_orbit', 'WHEELUPMOUSE', 'PRESS', ctrl=True, alt=True)
kmi.properties.type = 'ORBITLEFT'
kmi = km.items.add('view3d.view_orbit', 'WHEELDOWNMOUSE', 'PRESS', ctrl=True, alt=True)
kmi.properties.type = 'ORBITRIGHT'
kmi = km.items.add('view3d.view_orbit', 'WHEELUPMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.type = 'ORBITUP'
kmi = km.items.add('view3d.view_orbit', 'WHEELDOWNMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.type = 'ORBITDOWN'
kmi = km.items.add('view3d.viewnumpad', 'NUMPAD_1', 'PRESS', shift=True)
kmi.properties.align_active = True
kmi.properties.type = 'FRONT'
kmi = km.items.add('view3d.viewnumpad', 'NUMPAD_3', 'PRESS', shift=True)
kmi.properties.align_active = True
kmi.properties.type = 'RIGHT'
kmi = km.items.add('view3d.viewnumpad', 'NUMPAD_7', 'PRESS', shift=True)
kmi.properties.align_active = True
kmi.properties.type = 'TOP'
kmi = km.items.add('view3d.viewnumpad', 'NUMPAD_1', 'PRESS', shift=True, ctrl=True)
kmi.properties.align_active = True
kmi.properties.type = 'BACK'
kmi = km.items.add('view3d.viewnumpad', 'NUMPAD_3', 'PRESS', shift=True, ctrl=True)
kmi.properties.align_active = True
kmi.properties.type = 'LEFT'
kmi = km.items.add('view3d.viewnumpad', 'NUMPAD_7', 'PRESS', shift=True, ctrl=True)
kmi.properties.align_active = True
kmi.properties.type = 'BOTTOM'
kmi = km.items.add('view3d.localview', 'NUMPAD_SLASH', 'PRESS')
kmi = km.items.add('view3d.layers', 'ACCENT_GRAVE', 'PRESS')
kmi.properties.nr = 0
kmi = km.items.add('view3d.layers', 'ONE', 'PRESS', shift=True)
kmi.properties.nr = 1
kmi = km.items.add('view3d.layers', 'TWO', 'PRESS', shift=True)
kmi.properties.nr = 2
kmi = km.items.add('view3d.layers', 'THREE', 'PRESS', shift=True)
kmi.properties.nr = 3
kmi = km.items.add('view3d.layers', 'FOUR', 'PRESS', shift=True)
kmi.properties.nr = 4
kmi = km.items.add('view3d.layers', 'FIVE', 'PRESS', shift=True)
kmi.properties.nr = 5
kmi = km.items.add('view3d.layers', 'SIX', 'PRESS', shift=True)
kmi.properties.nr = 6
kmi = km.items.add('view3d.layers', 'SEVEN', 'PRESS', shift=True)
kmi.properties.nr = 7
kmi = km.items.add('view3d.layers', 'EIGHT', 'PRESS', shift=True)
kmi.properties.nr = 8
kmi = km.items.add('view3d.layers', 'NINE', 'PRESS', shift=True)
kmi.properties.nr = 9
kmi = km.items.add('view3d.layers', 'ZERO', 'PRESS', shift=True)
kmi.properties.nr = 10
kmi = km.items.add('wm.context_toggle_enum', 'FOUR', 'PRESS')
kmi.properties.path = 'space_data.viewport_shading'
kmi.properties.value_1 = 'WIREFRAME'
kmi.properties.value_2 = 'WIREFRAME'
kmi = km.items.add('wm.context_toggle_enum', 'SIX', 'PRESS')
kmi.properties.path = 'space_data.viewport_shading'
kmi.properties.value_1 = 'TEXTURED'
kmi.properties.value_2 = 'SOLID'
kmi = km.items.add('view3d.select', 'SELECTMOUSE', 'PRESS')
kmi = km.items.add('view3d.select', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.items.add('view3d.select', 'SELECTMOUSE', 'PRESS', ctrl=True)
kmi.properties.center = True
kmi = km.items.add('view3d.select', 'SELECTMOUSE', 'PRESS', alt=True)
kmi.properties.enumerate = True
kmi = km.items.add('view3d.select', 'SELECTMOUSE', 'PRESS', shift=True, ctrl=True)
kmi.properties.center = True
kmi.properties.extend = True
kmi = km.items.add('view3d.select', 'SELECTMOUSE', 'PRESS', ctrl=True, alt=True)
kmi.properties.center = True
kmi.properties.enumerate = True
kmi = km.items.add('view3d.select', 'SELECTMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.enumerate = True
kmi.properties.extend = True
kmi = km.items.add('view3d.select', 'SELECTMOUSE', 'PRESS', shift=True, ctrl=True, alt=True)
kmi.properties.center = True
kmi.properties.enumerate = True
kmi.properties.extend = True
kmi = km.items.add('view3d.select_border', 'EVT_TWEAK_L', 'ANY')
kmi.properties.extend = False
kmi = km.items.add('view3d.select_lasso', 'EVT_TWEAK_A', 'ANY', ctrl=True)
kmi = km.items.add('view3d.select_lasso', 'EVT_TWEAK_A', 'ANY', shift=True, ctrl=True)
kmi.properties.deselect = True
kmi = km.items.add('view3d.select_circle', 'C', 'PRESS')
kmi = km.items.add('view3d.clip_border', 'B', 'PRESS', alt=True)
kmi = km.items.add('view3d.zoom_border', 'B', 'PRESS', shift=True)
kmi = km.items.add('view3d.render_border', 'B', 'PRESS', shift=True)
kmi = km.items.add('view3d.camera_to_view', 'NUMPAD_0', 'PRESS', ctrl=True, alt=True)
kmi = km.items.add('view3d.object_as_camera', 'NUMPAD_0', 'PRESS', ctrl=True)
kmi = km.items.add('wm.call_menu', 'S', 'PRESS', shift=True)
kmi.properties.name = 'VIEW3D_MT_snap'
kmi = km.items.add('wm.context_set_enum', 'COMMA', 'PRESS')
kmi.properties.path = 'space_data.pivot_point'
kmi.properties.value = 'BOUNDING_BOX_CENTER'
kmi = km.items.add('wm.context_set_enum', 'COMMA', 'PRESS', ctrl=True)
kmi.properties.path = 'space_data.pivot_point'
kmi.properties.value = 'MEDIAN_POINT'
kmi = km.items.add('wm.context_toggle', 'COMMA', 'PRESS', alt=True)
kmi.properties.path = 'space_data.pivot_point_align'
kmi = km.items.add('wm.context_toggle', 'T', 'PRESS')
kmi.properties.path = 'space_data.manipulator'
kmi = km.items.add('wm.context_set_enum', 'PERIOD', 'PRESS')
kmi.properties.path = 'space_data.pivot_point'
kmi.properties.value = 'CURSOR'
kmi = km.items.add('wm.context_set_enum', 'PERIOD', 'PRESS', ctrl=True)
kmi.properties.path = 'space_data.pivot_point'
kmi.properties.value = 'INDIVIDUAL_ORIGINS'
kmi = km.items.add('wm.context_set_enum', 'PERIOD', 'PRESS', alt=True)
kmi.properties.path = 'space_data.pivot_point'
kmi.properties.value = 'ACTIVE_ELEMENT'
kmi = km.items.add('transform.translate', 'G', 'PRESS')
kmi = km.items.add('transform.translate', 'EVT_TWEAK_S', 'ANY')
kmi = km.items.add('transform.rotate', 'R', 'PRESS', shift=True)
kmi = km.items.add('transform.resize', 'S', 'PRESS')
kmi = km.items.add('transform.warp', 'W', 'PRESS', shift=True)
kmi = km.items.add('transform.tosphere', 'S', 'PRESS', shift=True, alt=True)
kmi = km.items.add('transform.shear', 'S', 'PRESS', shift=True, ctrl=True, alt=True)
kmi = km.items.add('transform.select_orientation', 'SPACE', 'PRESS', alt=True)
kmi = km.items.add('transform.create_orientation', 'SPACE', 'PRESS', ctrl=True, alt=True)
kmi.properties.use = True
kmi = km.items.add('transform.mirror', 'M', 'PRESS', ctrl=True)
kmi = km.items.add('wm.context_toggle', 'TAB', 'PRESS', shift=True)
kmi.properties.path = 'tool_settings.snap'
kmi = km.items.add('transform.snap_type', 'TAB', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('wm.context_toggle_enum', 'FIVE', 'PRESS')
kmi.properties.path = 'space_data.viewport_shading'
kmi.properties.value_1 = 'SOLID'
kmi.properties.value_2 = 'SOLID'
kmi = km.items.add('view3d.enable_manipulator', 'W', 'PRESS')
kmi.properties.translate = True
kmi = km.items.add('view3d.enable_manipulator', 'E', 'PRESS')
kmi.properties.rotate = True
kmi = km.items.add('view3d.enable_manipulator', 'R', 'PRESS')
kmi.properties.scale = True

# Map Object Mode
km = kc.add_keymap('Object Mode', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.items.add('wm.context_cycle_enum', 'O', 'PRESS', shift=True)
kmi.properties.path = 'tool_settings.proportional_editing_falloff'
kmi = km.items.add('wm.context_toggle_enum', 'O', 'PRESS')
kmi.properties.path = 'tool_settings.proportional_editing'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'ENABLED'
kmi = km.items.add('view3d.game_start', 'P', 'PRESS')
kmi = km.items.add('object.select_all', 'LEFTMOUSE', 'CLICK')
kmi.properties.action = 'DESELECT'
kmi = km.items.add('object.select_inverse', 'I', 'PRESS', ctrl=True)
kmi = km.items.add('object.select_linked', 'L', 'PRESS', shift=True)
kmi = km.items.add('object.select_grouped', 'G', 'PRESS', shift=True)
kmi = km.items.add('object.select_mirror', 'M', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('object.select_hierarchy', 'LEFT_BRACKET', 'PRESS')
kmi.properties.direction = 'PARENT'
kmi = km.items.add('object.select_hierarchy', 'LEFT_BRACKET', 'PRESS', shift=True)
kmi.properties.direction = 'PARENT'
kmi.properties.extend = True
kmi = km.items.add('object.select_hierarchy', 'RIGHT_BRACKET', 'PRESS')
kmi.properties.direction = 'CHILD'
kmi = km.items.add('object.select_hierarchy', 'RIGHT_BRACKET', 'PRESS', shift=True)
kmi.properties.direction = 'CHILD'
kmi.properties.extend = True
kmi = km.items.add('object.parent_set', 'P', 'PRESS')
kmi = km.items.add('object.parent_no_inverse_set', 'P', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('object.parent_clear', 'P', 'PRESS', alt=True)
kmi = km.items.add('object.track_set', 'T', 'PRESS', ctrl=True)
kmi = km.items.add('object.track_clear', 'T', 'PRESS', alt=True)
kmi = km.items.add('object.constraint_add_with_targets', 'C', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('object.constraints_clear', 'C', 'PRESS', ctrl=True, alt=True)
kmi = km.items.add('object.location_clear', 'G', 'PRESS', alt=True)
kmi = km.items.add('object.rotation_clear', 'R', 'PRESS', alt=True)
kmi = km.items.add('object.scale_clear', 'S', 'PRESS', alt=True)
kmi = km.items.add('object.origin_clear', 'O', 'PRESS', alt=True)
kmi = km.items.add('object.restrictview_clear', 'H', 'PRESS', alt=True)
kmi = km.items.add('object.restrictview_set', 'H', 'PRESS')
kmi = km.items.add('object.restrictview_set', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.items.add('object.move_to_layer', 'M', 'PRESS')
kmi = km.items.add('object.delete', 'X', 'PRESS')
kmi = km.items.add('object.delete', 'DEL', 'PRESS')
kmi = km.items.add('wm.call_menu', 'A', 'PRESS', shift=True)
kmi.properties.name = 'INFO_MT_add'
kmi = km.items.add('object.duplicates_make_real', 'A', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('wm.call_menu', 'A', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_object_apply'
kmi = km.items.add('wm.call_menu', 'U', 'PRESS')
kmi.properties.name = 'VIEW3D_MT_make_single_user'
kmi = km.items.add('wm.call_menu', 'L', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_make_links'
kmi = km.items.add('object.duplicate_move', 'D', 'PRESS', ctrl=True)
kmi = km.items.add('object.duplicate_move_linked', 'D', 'PRESS', alt=True)
kmi = km.items.add('object.join', 'J', 'PRESS', ctrl=True)
kmi = km.items.add('object.convert', 'C', 'PRESS', alt=True)
kmi = km.items.add('object.proxy_make', 'P', 'PRESS', ctrl=True, alt=True)
kmi = km.items.add('object.make_local', 'L', 'PRESS')
kmi = km.items.add('anim.keyframe_insert_menu', 'S', 'PRESS')
kmi = km.items.add('anim.keyframe_delete_v3d', 'I', 'PRESS', alt=True)
kmi = km.items.add('anim.keying_set_active_set', 'I', 'PRESS', shift=True, ctrl=True, alt=True)
kmi = km.items.add('group.create', 'G', 'PRESS', ctrl=True)
kmi = km.items.add('group.objects_remove', 'G', 'PRESS', ctrl=True, alt=True)
kmi = km.items.add('group.objects_add_active', 'G', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('group.objects_remove_active', 'G', 'PRESS', shift=True, alt=True)
kmi = km.items.add('wm.call_menu', 'W', 'PRESS', shift=True)
kmi.properties.name = 'VIEW3D_MT_object_specials'
kmi = km.items.add('object.subdivision_set', 'ZERO', 'PRESS', ctrl=True)
kmi.properties.level = 0
kmi = km.items.add('object.subdivision_set', 'ONE', 'PRESS', ctrl=True)
kmi.properties.level = 1
kmi = km.items.add('object.subdivision_set', 'TWO', 'PRESS', ctrl=True)
kmi.properties.level = 2
kmi = km.items.add('object.subdivision_set', 'THREE', 'PRESS', ctrl=True)
kmi.properties.level = 3
kmi = km.items.add('object.subdivision_set', 'FOUR', 'PRESS', ctrl=True)
kmi.properties.level = 4
kmi = km.items.add('object.subdivision_set', 'FIVE', 'PRESS', ctrl=True)
kmi.properties.level = 5

# Map Mesh
km = kc.add_keymap('Mesh', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.items.add('mesh.loopcut_slide', 'R', 'PRESS', ctrl=True)
kmi = km.items.add('mesh.loop_select', 'MIDDLEMOUSE', 'PRESS', ctrl=True)
kmi = km.items.add('mesh.loop_select', 'SELECTMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.extend = True
kmi = km.items.add('mesh.edgering_select', 'SELECTMOUSE', 'PRESS', ctrl=True, alt=True)
kmi = km.items.add('mesh.edgering_select', 'SELECTMOUSE', 'PRESS', shift=True, ctrl=True, alt=True)
kmi.properties.extend = True
kmi = km.items.add('mesh.select_shortest_path', 'SELECTMOUSE', 'PRESS', ctrl=True)
kmi = km.items.add('mesh.select_all', 'LEFTMOUSE', 'CLICK')
kmi.properties.action = 'DESELECT'
kmi = km.items.add('mesh.select_more', 'NUMPAD_PLUS', 'PRESS', ctrl=True)
kmi = km.items.add('mesh.select_less', 'NUMPAD_MINUS', 'PRESS', ctrl=True)
kmi = km.items.add('mesh.select_inverse', 'I', 'PRESS', ctrl=True)
kmi = km.items.add('mesh.select_non_manifold', 'M', 'PRESS', shift=True, ctrl=True, alt=True)
kmi = km.items.add('mesh.select_linked', 'L', 'PRESS', ctrl=True)
kmi = km.items.add('mesh.select_linked_pick', 'L', 'PRESS')
kmi = km.items.add('mesh.select_linked_pick', 'L', 'PRESS', shift=True)
kmi.properties.deselect = True
kmi = km.items.add('mesh.faces_select_linked_flat', 'F', 'PRESS', shift=True, ctrl=True, alt=True)
kmi.properties.sharpness = 135.0
kmi = km.items.add('mesh.select_similar', 'G', 'PRESS', shift=True)
kmi = km.items.add('wm.call_menu', 'RIGHTMOUSE', 'PRESS')
kmi.properties.name = 'VIEW3D_MT_edit_mesh_selection_mode'
kmi = km.items.add('mesh.hide', 'H', 'PRESS', ctrl=True)
kmi = km.items.add('mesh.hide', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.items.add('mesh.reveal', 'H', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('mesh.normals_make_consistent', 'N', 'PRESS', ctrl=True)
kmi = km.items.add('mesh.normals_make_consistent', 'N', 'PRESS', shift=True, ctrl=True)
kmi.properties.inside = True
kmi = km.items.add('view3d.edit_mesh_extrude_move_normal', 'E', 'PRESS', shift=True)
kmi = km.items.add('view3d.edit_mesh_extrude_individual_move', 'E', 'PRESS', ctrl=True)
kmi = km.items.add('wm.call_menu', 'E', 'PRESS', alt=True)
kmi.properties.name = 'VIEW3D_MT_edit_mesh_extrude'
kmi = km.items.add('mesh.spin', 'R', 'PRESS', alt=True)
kmi = km.items.add('mesh.fill', 'F', 'PRESS', alt=True)
kmi = km.items.add('mesh.beautify_fill', 'F', 'PRESS', shift=True, alt=True)
kmi = km.items.add('mesh.quads_convert_to_tris', 'T', 'PRESS', ctrl=True)
kmi = km.items.add('mesh.tris_convert_to_quads', 'J', 'PRESS', alt=True)
kmi = km.items.add('mesh.edge_flip', 'F', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('mesh.rip_move', 'V', 'PRESS')
kmi = km.items.add('mesh.merge', 'M', 'PRESS', alt=True)
kmi = km.items.add('transform.shrink_fatten', 'S', 'PRESS', ctrl=True, alt=True)
kmi = km.items.add('mesh.edge_face_add', 'F', 'PRESS', shift=True)
kmi = km.items.add('mesh.duplicate_move', 'D', 'PRESS', shift=True)
kmi = km.items.add('wm.call_menu', 'A', 'PRESS', shift=True)
kmi.properties.name = 'INFO_MT_mesh_add'
kmi = km.items.add('mesh.separate', 'P', 'PRESS')
kmi = km.items.add('mesh.split', 'Y', 'PRESS')
kmi = km.items.add('mesh.dupli_extrude_cursor', 'ACTIONMOUSE', 'CLICK', ctrl=True)
kmi = km.items.add('mesh.delete', 'X', 'PRESS')
kmi = km.items.add('mesh.delete', 'DEL', 'PRESS')
kmi = km.items.add('mesh.knife_cut', 'LEFTMOUSE', 'PRESS', key_modifier='K')
kmi = km.items.add('mesh.knife_cut', 'LEFTMOUSE', 'PRESS', shift=True, key_modifier='K')
kmi.properties.type = 'MIDPOINTS'
kmi = km.items.add('object.vertex_parent_set', 'P', 'PRESS', ctrl=True)
kmi = km.items.add('wm.call_menu', 'W', 'PRESS', shift=True)
kmi.properties.name = 'VIEW3D_MT_edit_mesh_specials'
kmi = km.items.add('wm.call_menu', 'F', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_edit_mesh_faces'
kmi = km.items.add('wm.call_menu', 'E', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_edit_mesh_edges'
kmi = km.items.add('wm.call_menu', 'V', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_edit_mesh_vertices'
kmi = km.items.add('wm.call_menu', 'H', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_hook'
kmi = km.items.add('wm.call_menu', 'U', 'PRESS')
kmi.properties.name = 'VIEW3D_MT_uv_map'
kmi = km.items.add('wm.call_menu', 'G', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_vertex_group'
kmi = km.items.add('wm.context_cycle_enum', 'O', 'PRESS', shift=True)
kmi.properties.path = 'tool_settings.proportional_editing_falloff'
kmi = km.items.add('wm.context_toggle_enum', 'O', 'PRESS')
kmi.properties.path = 'tool_settings.proportional_editing'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'ENABLED'
kmi = km.items.add('wm.context_toggle_enum', 'O', 'PRESS', alt=True)
kmi.properties.path = 'tool_settings.proportional_editing'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'CONNECTED'

# Map Curve
km = kc.add_keymap('Curve', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.items.add('object.curve_add', 'A', 'PRESS', shift=True)
kmi = km.items.add('curve.vertex_add', 'LEFTMOUSE', 'CLICK', ctrl=True)
kmi = km.items.add('curve.select_all', 'LEFTMOUSE', 'CLICK')
kmi.properties.action = 'DESELECT'
kmi = km.items.add('curve.select_row', 'R', 'PRESS', shift=True)
kmi = km.items.add('curve.select_more', 'NUMPAD_PLUS', 'PRESS', ctrl=True)
kmi = km.items.add('curve.select_less', 'NUMPAD_MINUS', 'PRESS', ctrl=True)
kmi = km.items.add('curve.select_linked', 'L', 'PRESS')
kmi = km.items.add('curve.select_linked', 'L', 'PRESS', shift=True)
kmi.properties.deselect = True
kmi = km.items.add('curve.separate', 'P', 'PRESS')
kmi = km.items.add('curve.extrude', 'E', 'PRESS', shift=True)
kmi = km.items.add('curve.duplicate', 'D', 'PRESS', ctrl=True)
kmi = km.items.add('curve.make_segment', 'F', 'PRESS', shift=True)
kmi = km.items.add('curve.cyclic_toggle', 'C', 'PRESS', alt=True)
kmi = km.items.add('curve.delete', 'X', 'PRESS')
kmi = km.items.add('curve.delete', 'DEL', 'PRESS')
kmi = km.items.add('curve.tilt_clear', 'T', 'PRESS', alt=True)
kmi = km.items.add('transform.tilt', 'T', 'PRESS', ctrl=True)
kmi = km.items.add('transform.transform', 'S', 'PRESS', alt=True)
kmi.properties.mode = 'CURVE_SHRINKFATTEN'
kmi = km.items.add('curve.handle_type_set', 'H', 'PRESS', shift=True)
kmi.properties.type = 'AUTOMATIC'
kmi = km.items.add('curve.handle_type_set', 'H', 'PRESS')
kmi.properties.type = 'TOGGLE_FREE_ALIGN'
kmi = km.items.add('curve.handle_type_set', 'V', 'PRESS')
kmi.properties.type = 'VECTOR'
kmi = km.items.add('curve.reveal', 'H', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('curve.hide', 'H', 'PRESS', ctrl=True)
kmi = km.items.add('curve.hide', 'H', 'PRESS', shift=True, alt=True)
kmi.properties.unselected = True
kmi = km.items.add('object.vertex_parent_set', 'P', 'PRESS', ctrl=True)
kmi = km.items.add('wm.call_menu', 'W', 'PRESS', shift=True)
kmi.properties.name = 'VIEW3D_MT_edit_curve_specials'
kmi = km.items.add('wm.call_menu', 'H', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_hook'
kmi = km.items.add('wm.context_cycle_enum', 'O', 'PRESS', shift=True)
kmi.properties.path = 'tool_settings.proportional_editing_falloff'
kmi = km.items.add('wm.context_toggle_enum', 'O', 'PRESS')
kmi.properties.path = 'tool_settings.proportional_editing'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'ENABLED'
kmi = km.items.add('wm.context_toggle_enum', 'O', 'PRESS', alt=True)
kmi.properties.path = 'tool_settings.proportional_editing'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'CONNECTED'

# Map Armature
km = kc.add_keymap('Armature', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.items.add('sketch.delete', 'X', 'PRESS')
kmi = km.items.add('sketch.delete', 'DEL', 'PRESS')
kmi = km.items.add('sketch.finish_stroke', 'SELECTMOUSE', 'PRESS')
kmi = km.items.add('sketch.cancel_stroke', 'ESC', 'PRESS')
kmi = km.items.add('sketch.select', 'SELECTMOUSE', 'PRESS')
kmi = km.items.add('sketch.gesture', 'ACTIONMOUSE', 'PRESS', shift=True)
kmi = km.items.add('sketch.draw_stroke', 'ACTIONMOUSE', 'PRESS')
kmi = km.items.add('sketch.draw_stroke', 'ACTIONMOUSE', 'PRESS', ctrl=True)
kmi.properties.snap = True
kmi = km.items.add('sketch.draw_preview', 'MOUSEMOVE', 'ANY')
kmi = km.items.add('sketch.draw_preview', 'MOUSEMOVE', 'ANY', ctrl=True)
kmi.properties.snap = True
kmi = km.items.add('armature.align', 'A', 'PRESS', ctrl=True, alt=True)
kmi = km.items.add('armature.calculate_roll', 'N', 'PRESS', ctrl=True)
kmi = km.items.add('armature.switch_direction', 'F', 'PRESS', alt=True)
kmi = km.items.add('armature.bone_primitive_add', 'A', 'PRESS', shift=True)
kmi = km.items.add('armature.parent_set', 'P', 'PRESS')
kmi = km.items.add('armature.parent_clear', 'P', 'PRESS', alt=True)
kmi = km.items.add('armature.select_all', 'LEFTMOUSE', 'CLICK')
kmi.properties.action = 'DESELECT'
kmi = km.items.add('armature.select_inverse', 'I', 'PRESS', ctrl=True)
kmi = km.items.add('armature.select_hierarchy', 'LEFT_BRACKET', 'PRESS')
kmi.properties.direction = 'PARENT'
kmi = km.items.add('armature.select_hierarchy', 'LEFT_BRACKET', 'PRESS', shift=True)
kmi.properties.direction = 'PARENT'
kmi.properties.extend = True
kmi = km.items.add('armature.select_hierarchy', 'RIGHT_BRACKET', 'PRESS')
kmi.properties.direction = 'CHILD'
kmi = km.items.add('armature.select_hierarchy', 'RIGHT_BRACKET', 'PRESS', shift=True)
kmi.properties.direction = 'CHILD'
kmi.properties.extend = True
kmi = km.items.add('armature.select_linked', 'L', 'PRESS')
kmi = km.items.add('armature.delete', 'X', 'PRESS')
kmi = km.items.add('armature.delete', 'DEL', 'PRESS')
kmi = km.items.add('armature.duplicate_move', 'D', 'PRESS', shift=True)
kmi = km.items.add('armature.extrude_move', 'E', 'PRESS', shift=True)
kmi = km.items.add('armature.extrude_forked', 'E', 'PRESS', ctrl=True)
kmi = km.items.add('armature.click_extrude', 'LEFTMOUSE', 'CLICK', ctrl=True)
kmi = km.items.add('armature.fill', 'F', 'PRESS', shift=True)
kmi = km.items.add('armature.merge', 'M', 'PRESS', alt=True)
kmi = km.items.add('armature.separate', 'P', 'PRESS', ctrl=True, alt=True)
kmi = km.items.add('armature.flags_set', 'W', 'PRESS', shift=True)
kmi.properties.mode = 'TOGGLE'
kmi = km.items.add('armature.flags_set', 'W', 'PRESS', shift=True, ctrl=True)
kmi.properties.mode = 'ENABLE'
kmi = km.items.add('armature.flags_set', 'W', 'PRESS', alt=True)
kmi.properties.mode = 'CLEAR'
kmi = km.items.add('armature.armature_layers', 'M', 'PRESS', shift=True)
kmi = km.items.add('armature.bone_layers', 'M', 'PRESS')
kmi = km.items.add('transform.transform', 'S', 'PRESS', ctrl=True, alt=True)
kmi.properties.mode = 'BONESIZE'
kmi = km.items.add('transform.transform', 'R', 'PRESS', ctrl=True)
kmi.properties.mode = 'BONE_ROLL'
kmi = km.items.add('wm.call_menu', 'W', 'PRESS', shift=True)
kmi.properties.name = 'VIEW3D_MT_armature_specials'

# Map Metaball
km = kc.add_keymap('Metaball', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.items.add('object.metaball_add', 'A', 'PRESS', shift=True)
kmi = km.items.add('mball.reveal_metaelems', 'H', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('mball.hide_metaelems', 'H', 'PRESS', ctrl=True)
kmi = km.items.add('mball.hide_metaelems', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.items.add('mball.delete_metaelems', 'X', 'PRESS')
kmi = km.items.add('mball.delete_metaelems', 'DEL', 'PRESS')
kmi = km.items.add('mball.duplicate_metaelems', 'D', 'PRESS', ctrl=True)
kmi = km.items.add('mball.select_all', 'LEFTMOUSE', 'CLICK')
kmi.properties.action = 'DESELECT'
kmi = km.items.add('mball.select_inverse_metaelems', 'I', 'PRESS', ctrl=True)

# Map Lattice
km = kc.add_keymap('Lattice', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.items.add('lattice.select_all', 'LEFTMOUSE', 'CLICK')
kmi.properties.action = 'DESELECT'
kmi = km.items.add('object.vertex_parent_set', 'P', 'PRESS', ctrl=True)
kmi = km.items.add('wm.call_menu', 'H', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_hook'
kmi = km.items.add('wm.context_cycle_enum', 'O', 'PRESS', shift=True)
kmi.properties.path = 'tool_settings.proportional_editing_falloff'
kmi = km.items.add('wm.context_toggle_enum', 'O', 'PRESS')
kmi.properties.path = 'tool_settings.proportional_editing'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'ENABLED'

# Map Pose
km = kc.add_keymap('Pose', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.items.add('object.parent_set', 'P', 'PRESS', ctrl=True)
kmi = km.items.add('pose.hide', 'H', 'PRESS', ctrl=True)
kmi = km.items.add('pose.hide', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.items.add('pose.reveal', 'H', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('wm.call_menu', 'A', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_pose_apply'
kmi = km.items.add('pose.rot_clear', 'R', 'PRESS', alt=True)
kmi = km.items.add('pose.loc_clear', 'G', 'PRESS', alt=True)
kmi = km.items.add('pose.scale_clear', 'S', 'PRESS', alt=True)
kmi = km.items.add('pose.quaternions_flip', 'F', 'PRESS', alt=True)
kmi = km.items.add('pose.copy', 'C', 'PRESS', ctrl=True)
kmi = km.items.add('pose.paste', 'V', 'PRESS', ctrl=True)
kmi = km.items.add('pose.paste', 'V', 'PRESS', shift=True, ctrl=True)
kmi.properties.flipped = True
kmi = km.items.add('pose.select_all', 'LEFTMOUSE', 'CLICK')
kmi.properties.action = 'DESELECT'
kmi = km.items.add('pose.select_inverse', 'I', 'PRESS', ctrl=True)
kmi = km.items.add('pose.select_parent', 'P', 'PRESS', shift=True)
kmi = km.items.add('pose.select_hierarchy', 'LEFT_BRACKET', 'PRESS')
kmi.properties.direction = 'PARENT'
kmi = km.items.add('pose.select_hierarchy', 'LEFT_BRACKET', 'PRESS', shift=True)
kmi.properties.direction = 'PARENT'
kmi.properties.extend = True
kmi = km.items.add('pose.select_hierarchy', 'RIGHT_BRACKET', 'PRESS')
kmi.properties.direction = 'CHILD'
kmi = km.items.add('pose.select_hierarchy', 'RIGHT_BRACKET', 'PRESS', shift=True)
kmi.properties.direction = 'CHILD'
kmi.properties.extend = True
kmi = km.items.add('pose.select_linked', 'L', 'PRESS')
kmi = km.items.add('pose.select_grouped', 'G', 'PRESS', shift=True)
kmi = km.items.add('pose.constraint_add_with_targets', 'C', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('pose.constraints_clear', 'C', 'PRESS', ctrl=True, alt=True)
kmi = km.items.add('pose.ik_add', 'I', 'PRESS', shift=True)
kmi = km.items.add('pose.ik_clear', 'I', 'PRESS', ctrl=True, alt=True)
kmi = km.items.add('wm.call_menu', 'G', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_pose_group'
kmi = km.items.add('pose.flags_set', 'W', 'PRESS', shift=True)
kmi.properties.mode = 'TOGGLE'
kmi = km.items.add('pose.flags_set', 'W', 'PRESS', shift=True, ctrl=True)
kmi.properties.mode = 'ENABLE'
kmi = km.items.add('pose.flags_set', 'W', 'PRESS', alt=True)
kmi.properties.mode = 'CLEAR'
kmi = km.items.add('pose.armature_layers', 'M', 'PRESS', shift=True)
kmi = km.items.add('pose.bone_layers', 'M', 'PRESS')
kmi = km.items.add('transform.transform', 'S', 'PRESS', ctrl=True, alt=True)
kmi.properties.mode = 'BONESIZE'
kmi = km.items.add('anim.keyframe_insert_menu', 'S', 'PRESS')
kmi = km.items.add('anim.keyframe_delete_v3d', 'I', 'PRESS', alt=True)
kmi = km.items.add('anim.keying_set_active_set', 'I', 'PRESS', shift=True, ctrl=True, alt=True)
kmi = km.items.add('poselib.browse_interactive', 'L', 'PRESS', ctrl=True)
kmi = km.items.add('poselib.pose_add', 'L', 'PRESS', shift=True)
kmi = km.items.add('poselib.pose_remove', 'L', 'PRESS', alt=True)
kmi = km.items.add('poselib.pose_rename', 'L', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('pose.push', 'E', 'PRESS', ctrl=True)
kmi = km.items.add('pose.relax', 'E', 'PRESS', alt=True)
kmi = km.items.add('pose.breakdown', 'E', 'PRESS', shift=True)

# Map Particle
km = kc.add_keymap('Particle', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.items.add('particle.select_all', 'LEFTMOUSE', 'CLICK')
kmi.properties.action = 'DESELECT'
kmi = km.items.add('particle.select_more', 'NUMPAD_PLUS', 'PRESS', ctrl=True)
kmi = km.items.add('particle.select_less', 'NUMPAD_MINUS', 'PRESS', ctrl=True)
kmi = km.items.add('particle.select_linked', 'L', 'PRESS')
kmi = km.items.add('particle.select_linked', 'L', 'PRESS', shift=True)
kmi.properties.deselect = True
kmi = km.items.add('particle.select_inverse', 'I', 'PRESS', ctrl=True)
kmi = km.items.add('particle.delete', 'X', 'PRESS')
kmi = km.items.add('particle.delete', 'DEL', 'PRESS')
kmi = km.items.add('particle.reveal', 'H', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('particle.hide', 'H', 'PRESS', ctrl=True)
kmi = km.items.add('particle.hide', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.items.add('particle.brush_edit', 'LEFTMOUSE', 'PRESS')
kmi = km.items.add('particle.brush_edit', 'LEFTMOUSE', 'PRESS', shift=True)
kmi = km.items.add('particle.brush_radial_control', 'F', 'PRESS')
kmi.properties.mode = 'SIZE'
kmi = km.items.add('particle.brush_radial_control', 'F', 'PRESS', shift=True)
kmi.properties.mode = 'STRENGTH'
kmi = km.items.add('wm.call_menu', 'W', 'PRESS', shift=True)
kmi.properties.name = 'VIEW3D_MT_particle_specials'
kmi = km.items.add('particle.weight_set', 'K', 'PRESS', shift=True)
kmi = km.items.add('wm.context_cycle_enum', 'O', 'PRESS', shift=True)
kmi.properties.path = 'tool_settings.proportional_editing_falloff'
kmi = km.items.add('wm.context_toggle_enum', 'O', 'PRESS')
kmi.properties.path = 'tool_settings.proportional_editing'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'ENABLED'

# Map 3D View Generic
km = kc.add_keymap('3D View Generic', space_type='VIEW_3D', region_type='WINDOW', modal=False)

kmi = km.items.add('view3d.properties', 'N', 'PRESS')
kmi = km.items.add('view3d.toolshelf', 'RIGHTMOUSE', 'CLICK', shift=True)

# Map Markers
km = kc.add_keymap('Markers', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.items.add('marker.add', 'M', 'PRESS')
kmi = km.items.add('marker.move', 'EVT_TWEAK_S', 'ANY')
kmi = km.items.add('marker.duplicate', 'D', 'PRESS', shift=True)
kmi = km.items.add('marker.select', 'SELECTMOUSE', 'PRESS')
kmi = km.items.add('marker.select', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.items.add('marker.select', 'SELECTMOUSE', 'PRESS', ctrl=True)
kmi.properties.camera = True
kmi = km.items.add('marker.select', 'SELECTMOUSE', 'PRESS', shift=True, ctrl=True)
kmi.properties.camera = True
kmi.properties.extend = True
kmi = km.items.add('marker.select_border', 'EVT_TWEAK_L', 'ANY')
kmi = km.items.add('marker.select_all', 'LEFTMOUSE', 'CLICK')
kmi.properties.action = 'DESELECT'
kmi = km.items.add('marker.delete', 'X', 'PRESS')
kmi = km.items.add('marker.move', 'G', 'PRESS')
kmi = km.items.add('marker.camera_bind', 'B', 'PRESS', ctrl=True)

# Map Animation Channels
km = kc.add_keymap('Animation Channels', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.items.add('anim.channels_click', 'LEFTMOUSE', 'PRESS')
kmi = km.items.add('anim.channels_click', 'LEFTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.items.add('anim.channels_click', 'LEFTMOUSE', 'PRESS', shift=True, ctrl=True)
kmi.properties.children_only = True
kmi = km.items.add('anim.channels_select_all_toggle', 'A', 'PRESS')
kmi = km.items.add('anim.channels_select_all_toggle', 'I', 'PRESS', ctrl=True)
kmi.properties.invert = True
kmi = km.items.add('anim.channels_select_border', 'EVT_TWEAK_L', 'ANY')
kmi = km.items.add('anim.channels_select_border', 'EVT_TWEAK_L', 'ANY')
kmi = km.items.add('anim.channels_delete', 'X', 'PRESS')
kmi = km.items.add('anim.channels_delete', 'DEL', 'PRESS')
kmi = km.items.add('anim.channels_setting_toggle', 'W', 'PRESS', shift=True)
kmi = km.items.add('anim.channels_setting_enable', 'W', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('anim.channels_setting_disable', 'W', 'PRESS', alt=True)
kmi = km.items.add('anim.channels_editable_toggle', 'TAB', 'PRESS')
kmi = km.items.add('anim.channels_expand', 'NUMPAD_PLUS', 'PRESS')
kmi = km.items.add('anim.channels_collapse', 'NUMPAD_MINUS', 'PRESS')
kmi = km.items.add('anim.channels_expand', 'NUMPAD_PLUS', 'PRESS', ctrl=True)
kmi.properties.all = False
kmi = km.items.add('anim.channels_collapse', 'NUMPAD_MINUS', 'PRESS', ctrl=True)
kmi.properties.all = False
kmi = km.items.add('anim.channels_visibility_set', 'V', 'PRESS')
kmi = km.items.add('anim.channels_visibility_toggle', 'V', 'PRESS', shift=True)

# Map Graph Editor
km = kc.add_keymap('Graph Editor', space_type='GRAPH_EDITOR', region_type='WINDOW', modal=False)

kmi = km.items.add('graph.handles_view_toggle', 'H', 'PRESS', ctrl=True)
kmi = km.items.add('graph.cursor_set', 'ACTIONMOUSE', 'PRESS')
kmi = km.items.add('graph.clickselect', 'SELECTMOUSE', 'PRESS')
kmi = km.items.add('graph.clickselect', 'SELECTMOUSE', 'PRESS', alt=True)
kmi.properties.column = True
kmi = km.items.add('graph.clickselect', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.items.add('graph.clickselect', 'SELECTMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.column = True
kmi.properties.extend = True
kmi = km.items.add('graph.clickselect', 'SELECTMOUSE', 'PRESS', ctrl=True)
kmi.properties.left_right = 'CHECK'
kmi = km.items.add('graph.clickselect', 'SELECTMOUSE', 'PRESS', ctrl=True, alt=True)
kmi.properties.curves = True
kmi = km.items.add('graph.clickselect', 'SELECTMOUSE', 'PRESS', shift=True, ctrl=True, alt=True)
kmi.properties.curves = True
kmi.properties.extend = True
kmi = km.items.add('graph.select_all_toggle', 'A', 'PRESS')
kmi = km.items.add('graph.select_all_toggle', 'I', 'PRESS', ctrl=True)
kmi.properties.invert = True
kmi = km.items.add('graph.select_border', 'EVT_TWEAK_L', 'ANY')
kmi = km.items.add('graph.select_border', 'B', 'PRESS', alt=True)
kmi.properties.axis_range = True
kmi = km.items.add('graph.select_border', 'B', 'PRESS', ctrl=True)
kmi.properties.include_handles = True
kmi = km.items.add('graph.select_border', 'B', 'PRESS', ctrl=True, alt=True)
kmi.properties.axis_range = True
kmi.properties.include_handles = True
kmi = km.items.add('graph.select_column', 'K', 'PRESS')
kmi.properties.mode = 'KEYS'
kmi = km.items.add('graph.select_column', 'K', 'PRESS', ctrl=True)
kmi.properties.mode = 'CFRA'
kmi = km.items.add('graph.select_column', 'K', 'PRESS', shift=True)
kmi.properties.mode = 'MARKERS_COLUMN'
kmi = km.items.add('graph.select_column', 'K', 'PRESS', alt=True)
kmi.properties.mode = 'MARKERS_BETWEEN'
kmi = km.items.add('graph.select_more', 'NUMPAD_PLUS', 'PRESS', ctrl=True)
kmi = km.items.add('graph.select_less', 'NUMPAD_MINUS', 'PRESS', ctrl=True)
kmi = km.items.add('graph.select_linked', 'L', 'PRESS')
kmi = km.items.add('graph.frame_jump', 'S', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('graph.snap', 'S', 'PRESS', shift=True)
kmi = km.items.add('graph.mirror', 'M', 'PRESS', shift=True)
kmi = km.items.add('graph.handle_type', 'H', 'PRESS')
kmi = km.items.add('graph.interpolation_type', 'T', 'PRESS', shift=True)
kmi = km.items.add('graph.extrapolation_type', 'E', 'PRESS', shift=True)
kmi = km.items.add('graph.clean', 'O', 'PRESS')
kmi = km.items.add('graph.smooth', 'O', 'PRESS', alt=True)
kmi = km.items.add('graph.sample', 'O', 'PRESS', shift=True)
kmi = km.items.add('graph.bake', 'C', 'PRESS', alt=True)
kmi = km.items.add('graph.delete', 'X', 'PRESS')
kmi = km.items.add('graph.delete', 'DEL', 'PRESS')
kmi = km.items.add('graph.duplicate', 'D', 'PRESS', ctrl=True)
kmi = km.items.add('graph.keyframe_insert', 'S', 'PRESS')
kmi = km.items.add('graph.click_insert', 'LEFTMOUSE', 'PRESS', ctrl=True)
kmi = km.items.add('graph.copy', 'C', 'PRESS', ctrl=True)
kmi = km.items.add('graph.paste', 'V', 'PRESS', ctrl=True)
kmi = km.items.add('graph.previewrange_set', 'P', 'PRESS', ctrl=True, alt=True)
kmi = km.items.add('graph.view_all', 'A', 'PRESS')
kmi = km.items.add('graph.fmodifier_add', 'M', 'PRESS', shift=True, ctrl=True)
kmi.properties.only_active = False
kmi = km.items.add('anim.channels_editable_toggle', 'TAB', 'PRESS')
kmi = km.items.add('transform.translate', 'G', 'PRESS')
kmi = km.items.add('transform.translate', 'EVT_TWEAK_S', 'ANY')
kmi = km.items.add('transform.transform', 'E', 'PRESS')
kmi.properties.mode = 'TIME_EXTEND'
kmi = km.items.add('transform.rotate', 'R', 'PRESS')
kmi = km.items.add('transform.resize', 'S', 'PRESS')

# Map Dopesheet
km = kc.add_keymap('Dopesheet', space_type='DOPESHEET_EDITOR', region_type='WINDOW', modal=False)

kmi = km.items.add('action.clickselect', 'SELECTMOUSE', 'PRESS')
kmi = km.items.add('action.clickselect', 'SELECTMOUSE', 'PRESS', alt=True)
kmi.properties.column = True
kmi = km.items.add('action.clickselect', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.items.add('action.clickselect', 'SELECTMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.column = True
kmi.properties.extend = True
kmi = km.items.add('action.clickselect', 'SELECTMOUSE', 'PRESS', ctrl=True)
kmi.properties.left_right = 'CHECK'
kmi = km.items.add('action.select_all_toggle', 'A', 'PRESS')
kmi = km.items.add('action.select_all_toggle', 'I', 'PRESS', ctrl=True)
kmi.properties.invert = True
kmi = km.items.add('action.select_border', 'EVT_TWEAK_L', 'ANY')
kmi = km.items.add('action.select_border', 'B', 'PRESS', alt=True)
kmi.properties.axis_range = True
kmi = km.items.add('action.select_column', 'K', 'PRESS')
kmi.properties.mode = 'KEYS'
kmi = km.items.add('action.select_column', 'K', 'PRESS', ctrl=True)
kmi.properties.mode = 'CFRA'
kmi = km.items.add('action.select_column', 'K', 'PRESS', shift=True)
kmi.properties.mode = 'MARKERS_COLUMN'
kmi = km.items.add('action.select_column', 'K', 'PRESS', alt=True)
kmi.properties.mode = 'MARKERS_BETWEEN'
kmi = km.items.add('action.select_more', 'NUMPAD_PLUS', 'PRESS', ctrl=True)
kmi = km.items.add('action.select_less', 'NUMPAD_MINUS', 'PRESS', ctrl=True)
kmi = km.items.add('action.select_linked', 'L', 'PRESS')
kmi = km.items.add('action.frame_jump', 'S', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('action.snap', 'S', 'PRESS', shift=True)
kmi = km.items.add('action.mirror', 'M', 'PRESS', shift=True)
kmi = km.items.add('action.handle_type', 'H', 'PRESS')
kmi = km.items.add('action.interpolation_type', 'T', 'PRESS', shift=True)
kmi = km.items.add('action.extrapolation_type', 'E', 'PRESS', shift=True)
kmi = km.items.add('action.keyframe_type', 'R', 'PRESS')
kmi = km.items.add('action.clean', 'O', 'PRESS')
kmi = km.items.add('action.sample', 'O', 'PRESS', shift=True)
kmi = km.items.add('action.delete', 'X', 'PRESS')
kmi = km.items.add('action.delete', 'DEL', 'PRESS')
kmi = km.items.add('action.duplicate', 'D', 'PRESS', ctrl=True)
kmi = km.items.add('action.keyframe_insert', 'I', 'PRESS')
kmi = km.items.add('action.copy', 'C', 'PRESS', ctrl=True)
kmi = km.items.add('action.paste', 'V', 'PRESS', ctrl=True)
kmi = km.items.add('action.previewrange_set', 'P', 'PRESS', ctrl=True, alt=True)
kmi = km.items.add('action.view_all', 'HOME', 'PRESS')
kmi = km.items.add('anim.channels_editable_toggle', 'TAB', 'PRESS')
kmi = km.items.add('transform.transform', 'G', 'PRESS')
kmi.properties.mode = 'TIME_TRANSLATE'
kmi = km.items.add('transform.transform', 'EVT_TWEAK_S', 'ANY')
kmi.properties.mode = 'TIME_TRANSLATE'
kmi = km.items.add('transform.transform', 'E', 'PRESS')
kmi.properties.mode = 'TIME_EXTEND'
kmi = km.items.add('transform.transform', 'S', 'PRESS')
kmi.properties.mode = 'TIME_SCALE'
kmi = km.items.add('transform.transform', 'T', 'PRESS')
kmi.properties.mode = 'TIME_SLIDE'

# Map NLA Editor
km = kc.add_keymap('NLA Editor', space_type='NLA_EDITOR', region_type='WINDOW', modal=False)

kmi = km.items.add('nla.click_select', 'SELECTMOUSE', 'PRESS')
kmi = km.items.add('nla.click_select', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.items.add('nla.click_select', 'SELECTMOUSE', 'PRESS', ctrl=True)
kmi.properties.left_right = 'CHECK'
kmi = km.items.add('nla.select_all_toggle', 'A', 'PRESS')
kmi = km.items.add('nla.select_all_toggle', 'I', 'PRESS', ctrl=True)
kmi.properties.invert = True
kmi = km.items.add('nla.select_border', 'B', 'PRESS')
kmi = km.items.add('nla.select_border', 'B', 'PRESS', alt=True)
kmi.properties.axis_range = True
kmi = km.items.add('nla.tweakmode_enter', 'TAB', 'PRESS')
kmi = km.items.add('nla.tweakmode_exit', 'TAB', 'PRESS')
kmi = km.items.add('nla.actionclip_add', 'A', 'PRESS', shift=True)
kmi = km.items.add('nla.transition_add', 'T', 'PRESS', shift=True)
kmi = km.items.add('nla.meta_add', 'G', 'PRESS', shift=True)
kmi = km.items.add('nla.meta_remove', 'G', 'PRESS', alt=True)
kmi = km.items.add('nla.duplicate', 'D', 'PRESS', ctrl=True)
kmi = km.items.add('nla.delete', 'X', 'PRESS')
kmi = km.items.add('nla.delete', 'DEL', 'PRESS')
kmi = km.items.add('nla.split', 'Y', 'PRESS')
kmi = km.items.add('nla.mute_toggle', 'H', 'PRESS')
kmi = km.items.add('nla.move_up', 'PAGE_UP', 'PRESS')
kmi = km.items.add('nla.move_down', 'PAGE_DOWN', 'PRESS')
kmi = km.items.add('nla.apply_scale', 'A', 'PRESS', ctrl=True)
kmi = km.items.add('nla.clear_scale', 'S', 'PRESS', alt=True)
kmi = km.items.add('nla.snap', 'S', 'PRESS', shift=True)
kmi = km.items.add('nla.fmodifier_add', 'M', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('transform.transform', 'G', 'PRESS')
kmi.properties.mode = 'TRANSLATION'
kmi = km.items.add('transform.transform', 'EVT_TWEAK_S', 'ANY')
kmi.properties.mode = 'TRANSLATION'
kmi = km.items.add('transform.transform', 'E', 'PRESS')
kmi.properties.mode = 'TIME_EXTEND'
kmi = km.items.add('transform.transform', 'S', 'PRESS')
kmi.properties.mode = 'TIME_SCALE'

# Map NLA Channels
km = kc.add_keymap('NLA Channels', space_type='NLA_EDITOR', region_type='WINDOW', modal=False)

kmi = km.items.add('nla.channels_click', 'LEFTMOUSE', 'PRESS')
kmi = km.items.add('nla.channels_click', 'LEFTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.items.add('nla.tracks_add', 'A', 'PRESS', shift=True)
kmi = km.items.add('nla.tracks_add', 'A', 'PRESS', shift=True, ctrl=True)
kmi.properties.above_selected = True
kmi = km.items.add('nla.delete_tracks', 'X', 'PRESS')
kmi = km.items.add('nla.delete_tracks', 'DEL', 'PRESS')
kmi = km.items.add('anim.channels_select_border', 'B', 'PRESS')
kmi = km.items.add('anim.channels_select_all_toggle', 'A', 'PRESS')
kmi = km.items.add('anim.channels_select_all_toggle', 'I', 'PRESS', ctrl=True)
kmi.properties.invert = True
kmi = km.items.add('anim.channels_setting_toggle', 'W', 'PRESS', shift=True)
kmi = km.items.add('anim.channels_setting_enable', 'W', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('anim.channels_setting_disable', 'W', 'PRESS', alt=True)
kmi = km.items.add('anim.channels_editable_toggle', 'TAB', 'PRESS')
kmi = km.items.add('anim.channels_expand', 'NUMPAD_PLUS', 'PRESS')
kmi = km.items.add('anim.channels_collapse', 'NUMPAD_MINUS', 'PRESS')
kmi = km.items.add('anim.channels_expand', 'NUMPAD_PLUS', 'PRESS', ctrl=True)
kmi.properties.all = True
kmi = km.items.add('anim.channels_collapse', 'NUMPAD_MINUS', 'PRESS', ctrl=True)
kmi.properties.all = True

# Map Image
km = kc.add_keymap('Image', space_type='IMAGE_EDITOR', region_type='WINDOW', modal=False)

kmi = km.items.add('image.view_all', 'A', 'PRESS')
kmi = km.items.add('image.view_selected', 'F', 'PRESS')
kmi = km.items.add('image.view_pan', 'MIDDLEMOUSE', 'PRESS')
kmi = km.items.add('image.view_pan', 'TRACKPADPAN', 'ANY')
kmi = km.items.add('image.view_zoom_in', 'WHEELINMOUSE', 'PRESS')
kmi = km.items.add('image.view_zoom_out', 'WHEELOUTMOUSE', 'PRESS')
kmi = km.items.add('image.view_zoom_in', 'NUMPAD_PLUS', 'PRESS')
kmi = km.items.add('image.view_zoom_out', 'NUMPAD_MINUS', 'PRESS')
kmi = km.items.add('image.view_zoom', 'MIDDLEMOUSE', 'PRESS', ctrl=True)
kmi = km.items.add('image.view_zoom', 'TRACKPADZOOM', 'ANY')
kmi = km.items.add('image.view_zoom_ratio', 'NUMPAD_8', 'PRESS', shift=True)
kmi.properties.ratio = 8.0
kmi = km.items.add('image.view_zoom_ratio', 'NUMPAD_4', 'PRESS', shift=True)
kmi.properties.ratio = 4.0
kmi = km.items.add('image.view_zoom_ratio', 'NUMPAD_2', 'PRESS', shift=True)
kmi.properties.ratio = 2.0
kmi = km.items.add('image.view_zoom_ratio', 'NUMPAD_1', 'PRESS')
kmi.properties.ratio = 1.0
kmi = km.items.add('image.view_zoom_ratio', 'NUMPAD_2', 'PRESS')
kmi.properties.ratio = 0.5
kmi = km.items.add('image.view_zoom_ratio', 'NUMPAD_4', 'PRESS')
kmi.properties.ratio = 0.25
kmi = km.items.add('image.view_zoom_ratio', 'NUMPAD_8', 'PRESS')
kmi.properties.ratio = 0.125
kmi = km.items.add('image.sample', 'ACTIONMOUSE', 'PRESS')
kmi = km.items.add('image.curves_point_set', 'ACTIONMOUSE', 'PRESS', ctrl=True)
kmi.properties.point = 'BLACK_POINT'
kmi = km.items.add('image.curves_point_set', 'ACTIONMOUSE', 'PRESS', shift=True)
kmi.properties.point = 'WHITE_POINT'
kmi = km.items.add('image.toolbox', 'SPACE', 'PRESS')

# Map UV Editor
km = kc.add_keymap('UV Editor', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.items.add('uv.select', 'SELECTMOUSE', 'PRESS')
kmi = km.items.add('uv.select', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.items.add('uv.select_loop', 'SELECTMOUSE', 'PRESS', alt=True)
kmi = km.items.add('uv.select_loop', 'SELECTMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.extend = True
kmi = km.items.add('uv.select_border', 'EVT_TWEAK_L', 'ANY')
kmi = km.items.add('uv.select_border', 'B', 'PRESS', shift=True)
kmi.properties.pinned = True
kmi = km.items.add('uv.circle_select', 'C', 'PRESS')
kmi = km.items.add('uv.select_linked', 'L', 'PRESS', ctrl=True)
kmi = km.items.add('uv.select_linked_pick', 'L', 'PRESS')
kmi = km.items.add('uv.select_linked', 'L', 'PRESS', shift=True, ctrl=True)
kmi.properties.extend = True
kmi = km.items.add('uv.select_linked_pick', 'L', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.items.add('uv.unlink_selection', 'L', 'PRESS', alt=True)
kmi = km.items.add('uv.select_all', 'LEFTMOUSE', 'CLICK')
kmi.properties.action = 'DESELECT'
kmi = km.items.add('uv.select_inverse', 'I', 'PRESS', ctrl=True)
kmi = km.items.add('uv.select_pinned', 'P', 'PRESS', shift=True)
kmi = km.items.add('wm.call_menu', 'W', 'PRESS')
kmi.properties.name = 'IMAGE_MT_uvs_weldalign'
kmi = km.items.add('uv.stitch', 'V', 'PRESS')
kmi = km.items.add('uv.pin', 'P', 'PRESS')
kmi = km.items.add('uv.pin', 'P', 'PRESS', alt=True)
kmi.properties.clear = True
kmi = km.items.add('uv.unwrap', 'E', 'PRESS')
kmi = km.items.add('uv.minimize_stretch', 'V', 'PRESS', ctrl=True)
kmi = km.items.add('uv.pack_islands', 'P', 'PRESS', ctrl=True)
kmi = km.items.add('uv.average_islands_scale', 'A', 'PRESS', ctrl=True)
kmi = km.items.add('uv.hide', 'H', 'PRESS', ctrl=True)
kmi = km.items.add('uv.hide', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.items.add('uv.reveal', 'H', 'PRESS', shift=True, ctrl=True)
kmi = km.items.add('uv.cursor_set', 'ACTIONMOUSE', 'PRESS')
kmi = km.items.add('uv.tile_set', 'ACTIONMOUSE', 'PRESS', shift=True)
kmi = km.items.add('wm.call_menu', 'S', 'PRESS', shift=True)
kmi.properties.name = 'IMAGE_MT_uvs_snap'
kmi = km.items.add('wm.context_cycle_enum', 'O', 'PRESS', shift=True)
kmi.properties.path = 'tool_settings.proportional_editing_falloff'
kmi = km.items.add('wm.context_toggle_enum', 'O', 'PRESS')
kmi.properties.path = 'tool_settings.proportional_editing'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'ENABLED'
kmi = km.items.add('transform.translate', 'G', 'PRESS')
kmi = km.items.add('transform.translate', 'EVT_TWEAK_S', 'ANY')
kmi = km.items.add('transform.rotate', 'R', 'PRESS')
kmi = km.items.add('transform.resize', 'S', 'PRESS')
kmi = km.items.add('transform.mirror', 'M', 'PRESS', ctrl=True)
kmi = km.items.add('wm.context_toggle', 'TAB', 'PRESS', shift=True)
kmi.properties.path = 'tool_settings.snap'

# Map Timeline
km = kc.add_keymap('Timeline', space_type='TIMELINE', region_type='WINDOW', modal=False)

kmi = km.items.add('time.start_frame_set', 'S', 'PRESS', shift=True)
kmi = km.items.add('time.end_frame_set', 'E', 'PRESS')
kmi = km.items.add('time.view_all', 'A', 'PRESS')

wm.active_keyconfig = kc

bpy.context.user_preferences.view.auto_depth = False
bpy.context.user_preferences.view.zoom_to_mouse = False
bpy.context.user_preferences.view.rotate_around_selection = False
bpy.context.user_preferences.edit.drag_immediately = True
bpy.context.user_preferences.edit.insertkey_xyz_to_rgb = False
bpy.context.user_preferences.inputs.select_mouse = 'LEFT'
bpy.context.user_preferences.inputs.zoom_style = 'DOLLY'
bpy.context.user_preferences.inputs.zoom_axis = 'HORIZONTAL'
bpy.context.user_preferences.inputs.view_rotation = 'TURNTABLE'
bpy.context.user_preferences.inputs.invert_zoom_direction = True
