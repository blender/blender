# Configuration 3dsmax
import bpy

wm = bpy.context.window_manager
kc = wm.keyconfigs.new('3dsmax')

# Map 3D View
km = kc.keymaps.new('3D View', space_type='VIEW_3D', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('view3d.manipulator', 'LEFTMOUSE', 'PRESS', any=True)
kmi.properties.release_confirm = True
kmi = km.keymap_items.new('view3d.cursor3d', 'ACTIONMOUSE', 'PRESS')
kmi = km.keymap_items.new('view3d.rotate', 'MIDDLEMOUSE', 'PRESS', alt=True)
kmi = km.keymap_items.new('view3d.move', 'MIDDLEMOUSE', 'PRESS')
kmi = km.keymap_items.new('view3d.zoom', 'MIDDLEMOUSE', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('view3d.dolly', 'MIDDLEMOUSE', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('view3d.view_selected', 'NUMPAD_PERIOD', 'PRESS', ctrl=True)
kmi.properties.use_all_regions = True
kmi = km.keymap_items.new('view3d.view_selected', 'NUMPAD_PERIOD', 'PRESS')
kmi.properties.use_all_regions = False
kmi = km.keymap_items.new('view3d.view_lock_to_active', 'NUMPAD_PERIOD', 'PRESS', shift=True)
kmi = km.keymap_items.new('view3d.view_lock_clear', 'NUMPAD_PERIOD', 'PRESS', alt=True)
kmi = km.keymap_items.new('view3d.fly', 'F', 'PRESS', shift=True)
kmi = km.keymap_items.new('view3d.smoothview', 'TIMER1', 'ANY', any=True)
kmi = km.keymap_items.new('view3d.rotate', 'TRACKPADPAN', 'ANY')
kmi = km.keymap_items.new('view3d.rotate', 'MOUSEROTATE', 'ANY')
kmi = km.keymap_items.new('view3d.move', 'TRACKPADPAN', 'ANY', shift=True)
kmi = km.keymap_items.new('view3d.zoom', 'TRACKPADZOOM', 'ANY')
kmi = km.keymap_items.new('view3d.zoom', 'TRACKPADPAN', 'ANY', ctrl=True)
kmi = km.keymap_items.new('view3d.zoom', 'NUMPAD_PLUS', 'PRESS')
kmi.properties.delta = 1
kmi = km.keymap_items.new('view3d.zoom', 'NUMPAD_MINUS', 'PRESS')
kmi.properties.delta = -1
kmi = km.keymap_items.new('view3d.zoom', 'EQUAL', 'PRESS', ctrl=True)
kmi.properties.delta = 1
kmi = km.keymap_items.new('view3d.zoom', 'MINUS', 'PRESS', ctrl=True)
kmi.properties.delta = -1
kmi = km.keymap_items.new('view3d.zoom', 'WHEELINMOUSE', 'PRESS')
kmi.properties.delta = 1
kmi = km.keymap_items.new('view3d.zoom', 'WHEELOUTMOUSE', 'PRESS')
kmi.properties.delta = -1
kmi = km.keymap_items.new('view3d.zoom_camera_1_to_1', 'NUMPAD_ENTER', 'PRESS', shift=True)
kmi = km.keymap_items.new('view3d.view_center_camera', 'HOME', 'PRESS')
kmi = km.keymap_items.new('view3d.view_center_cursor', 'HOME', 'PRESS', alt=True)
kmi = km.keymap_items.new('view3d.view_all', 'HOME', 'PRESS')
kmi.properties.center = False
kmi = km.keymap_items.new('view3d.view_all', 'HOME', 'PRESS', ctrl=True)
kmi.properties.use_all_regions = True
kmi.properties.center = False
kmi = km.keymap_items.new('view3d.view_all', 'C', 'PRESS', shift=True)
kmi.properties.center = True
kmi = km.keymap_items.new('view3d.viewnumpad', 'C', 'PRESS')
kmi.properties.type = 'CAMERA'
kmi = km.keymap_items.new('view3d.viewnumpad', 'F', 'PRESS')
kmi.properties.type = 'FRONT'
kmi = km.keymap_items.new('view3d.view_orbit', 'NUMPAD_2', 'PRESS')
kmi.properties.type = 'ORBITDOWN'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_3', 'PRESS')
kmi.properties.type = 'RIGHT'
kmi = km.keymap_items.new('view3d.view_orbit', 'NUMPAD_4', 'PRESS')
kmi.properties.type = 'ORBITLEFT'
kmi = km.keymap_items.new('view3d.view_persportho', 'NUMPAD_5', 'PRESS')
kmi = km.keymap_items.new('view3d.view_orbit', 'NUMPAD_6', 'PRESS')
kmi.properties.type = 'ORBITRIGHT'
kmi = km.keymap_items.new('view3d.viewnumpad', 'T', 'PRESS')
kmi.properties.type = 'TOP'
kmi = km.keymap_items.new('view3d.view_orbit', 'NUMPAD_8', 'PRESS')
kmi.properties.type = 'ORBITUP'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_1', 'PRESS', ctrl=True)
kmi.properties.type = 'BACK'
kmi = km.keymap_items.new('view3d.viewnumpad', 'L', 'PRESS')
kmi.properties.type = 'LEFT'
kmi = km.keymap_items.new('view3d.viewnumpad', 'B', 'PRESS')
kmi.properties.type = 'BOTTOM'
kmi = km.keymap_items.new('view3d.view_pan', 'NUMPAD_2', 'PRESS', ctrl=True)
kmi.properties.type = 'PANDOWN'
kmi = km.keymap_items.new('view3d.view_pan', 'NUMPAD_4', 'PRESS', ctrl=True)
kmi.properties.type = 'PANLEFT'
kmi = km.keymap_items.new('view3d.view_pan', 'NUMPAD_6', 'PRESS', ctrl=True)
kmi.properties.type = 'PANRIGHT'
kmi = km.keymap_items.new('view3d.view_pan', 'NUMPAD_8', 'PRESS', ctrl=True)
kmi.properties.type = 'PANUP'
kmi = km.keymap_items.new('view3d.view_pan', 'WHEELUPMOUSE', 'PRESS', ctrl=True)
kmi.properties.type = 'PANRIGHT'
kmi = km.keymap_items.new('view3d.view_pan', 'WHEELDOWNMOUSE', 'PRESS', ctrl=True)
kmi.properties.type = 'PANLEFT'
kmi = km.keymap_items.new('view3d.view_pan', 'WHEELUPMOUSE', 'PRESS', shift=True)
kmi.properties.type = 'PANUP'
kmi = km.keymap_items.new('view3d.view_pan', 'WHEELDOWNMOUSE', 'PRESS', shift=True)
kmi.properties.type = 'PANDOWN'
kmi = km.keymap_items.new('view3d.view_orbit', 'WHEELUPMOUSE', 'PRESS', ctrl=True, alt=True)
kmi.properties.type = 'ORBITLEFT'
kmi = km.keymap_items.new('view3d.view_orbit', 'WHEELDOWNMOUSE', 'PRESS', ctrl=True, alt=True)
kmi.properties.type = 'ORBITRIGHT'
kmi = km.keymap_items.new('view3d.view_orbit', 'WHEELUPMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.type = 'ORBITUP'
kmi = km.keymap_items.new('view3d.view_orbit', 'WHEELDOWNMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.type = 'ORBITDOWN'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_1', 'PRESS', shift=True)
kmi.properties.type = 'FRONT'
kmi.properties.align_active = True
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_3', 'PRESS', shift=True)
kmi.properties.type = 'RIGHT'
kmi.properties.align_active = True
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_7', 'PRESS', shift=True)
kmi.properties.type = 'TOP'
kmi.properties.align_active = True
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_1', 'PRESS', shift=True, ctrl=True)
kmi.properties.type = 'BACK'
kmi.properties.align_active = True
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_3', 'PRESS', shift=True, ctrl=True)
kmi.properties.type = 'LEFT'
kmi.properties.align_active = True
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_7', 'PRESS', shift=True, ctrl=True)
kmi.properties.type = 'BOTTOM'
kmi.properties.align_active = True
kmi = km.keymap_items.new('view3d.localview', 'NUMPAD_SLASH', 'PRESS')
kmi = km.keymap_items.new('view3d.ndof_orbit_zoom', 'NDOF_MOTION', 'ANY')
kmi = km.keymap_items.new('view3d.ndof_orbit', 'NDOF_MOTION', 'ANY', ctrl=True)
kmi = km.keymap_items.new('view3d.ndof_pan', 'NDOF_MOTION', 'ANY', shift=True)
kmi = km.keymap_items.new('view3d.ndof_all', 'NDOF_MOTION', 'ANY', shift=True, ctrl=True)
kmi = km.keymap_items.new('view3d.view_selected', 'NDOF_BUTTON_FIT', 'PRESS')
kmi = km.keymap_items.new('view3d.viewnumpad', 'NDOF_BUTTON_FRONT', 'PRESS')
kmi.properties.type = 'FRONT'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NDOF_BUTTON_BACK', 'PRESS')
kmi.properties.type = 'BACK'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NDOF_BUTTON_LEFT', 'PRESS')
kmi.properties.type = 'LEFT'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NDOF_BUTTON_RIGHT', 'PRESS')
kmi.properties.type = 'RIGHT'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NDOF_BUTTON_TOP', 'PRESS')
kmi.properties.type = 'TOP'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NDOF_BUTTON_BOTTOM', 'PRESS')
kmi.properties.type = 'BOTTOM'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NDOF_BUTTON_FRONT', 'PRESS', shift=True)
kmi.properties.type = 'FRONT'
kmi.properties.align_active = True
kmi = km.keymap_items.new('view3d.viewnumpad', 'NDOF_BUTTON_RIGHT', 'PRESS', shift=True)
kmi.properties.type = 'RIGHT'
kmi.properties.align_active = True
kmi = km.keymap_items.new('view3d.viewnumpad', 'NDOF_BUTTON_TOP', 'PRESS', shift=True)
kmi.properties.type = 'TOP'
kmi.properties.align_active = True
kmi = km.keymap_items.new('view3d.layers', 'ACCENT_GRAVE', 'PRESS')
kmi.properties.nr = 0
kmi = km.keymap_items.new('view3d.layers', 'ONE', 'PRESS', any=True)
kmi.properties.nr = 1
kmi = km.keymap_items.new('view3d.layers', 'TWO', 'PRESS', any=True)
kmi.properties.nr = 2
kmi = km.keymap_items.new('view3d.layers', 'THREE', 'PRESS', any=True)
kmi.properties.nr = 3
kmi = km.keymap_items.new('view3d.layers', 'FOUR', 'PRESS', any=True)
kmi.properties.nr = 4
kmi = km.keymap_items.new('view3d.layers', 'FIVE', 'PRESS', any=True)
kmi.properties.nr = 5
kmi = km.keymap_items.new('view3d.layers', 'SIX', 'PRESS', any=True)
kmi.properties.nr = 6
kmi = km.keymap_items.new('view3d.layers', 'SEVEN', 'PRESS', any=True)
kmi.properties.nr = 7
kmi = km.keymap_items.new('view3d.layers', 'EIGHT', 'PRESS', any=True)
kmi.properties.nr = 8
kmi = km.keymap_items.new('view3d.layers', 'NINE', 'PRESS', any=True)
kmi.properties.nr = 9
kmi = km.keymap_items.new('view3d.layers', 'ZERO', 'PRESS', any=True)
kmi.properties.nr = 10
kmi = km.keymap_items.new('wm.context_toggle_enum', 'Z', 'PRESS')
kmi.properties.data_path = 'space_data.viewport_shade'
kmi.properties.value_1 = 'SOLID'
kmi.properties.value_2 = 'WIREFRAME'
kmi = km.keymap_items.new('wm.context_toggle_enum', 'Z', 'PRESS', alt=True)
kmi.properties.data_path = 'space_data.viewport_shade'
kmi.properties.value_1 = 'SOLID'
kmi.properties.value_2 = 'TEXTURED'
kmi = km.keymap_items.new('view3d.select', 'SELECTMOUSE', 'RELEASE')
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = False
kmi.properties.center = False
kmi.properties.enumerate = False
kmi.properties.object = False
kmi = km.keymap_items.new('view3d.select', 'SELECTMOUSE', 'RELEASE', ctrl=True)
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = True
kmi.properties.center = False
kmi.properties.enumerate = False
kmi.properties.object = False
kmi = km.keymap_items.new('view3d.select', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = False
kmi.properties.center = True
kmi.properties.enumerate = False
kmi.properties.object = True
kmi = km.keymap_items.new('view3d.select', 'SELECTMOUSE', 'RELEASE', alt=True)
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = False
kmi.properties.center = False
kmi.properties.enumerate = True
kmi.properties.object = False
kmi = km.keymap_items.new('view3d.select', 'SELECTMOUSE', 'RELEASE', shift=True, ctrl=True)
kmi.properties.extend = True
kmi.properties.deselect = False
kmi.properties.toggle = True
kmi.properties.center = True
kmi.properties.enumerate = False
kmi.properties.object = False
kmi = km.keymap_items.new('view3d.select', 'SELECTMOUSE', 'RELEASE', ctrl=True, alt=True)
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = False
kmi.properties.center = True
kmi.properties.enumerate = True
kmi.properties.object = False
kmi = km.keymap_items.new('view3d.select', 'SELECTMOUSE', 'RELEASE', shift=True, alt=True)
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = True
kmi.properties.center = False
kmi.properties.enumerate = True
kmi.properties.object = False
kmi = km.keymap_items.new('view3d.select', 'SELECTMOUSE', 'RELEASE', shift=True, ctrl=True, alt=True)
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = True
kmi.properties.center = True
kmi.properties.enumerate = True
kmi.properties.object = False
kmi = km.keymap_items.new('view3d.select_border', 'EVT_TWEAK_L', 'ANY')
kmi.properties.extend = False
kmi = km.keymap_items.new('view3d.select_lasso', 'EVT_TWEAK_A', 'ANY', ctrl=True)
kmi.properties.deselect = False
kmi = km.keymap_items.new('view3d.select_lasso', 'EVT_TWEAK_A', 'ANY', shift=True, ctrl=True)
kmi.properties.deselect = True
kmi = km.keymap_items.new('view3d.select_circle', 'C', 'PRESS', alt=True)
kmi = km.keymap_items.new('view3d.clip_border', 'B', 'PRESS', alt=True)
kmi = km.keymap_items.new('view3d.zoom_border', 'B', 'PRESS', shift=True)
kmi = km.keymap_items.new('view3d.render_border', 'B', 'PRESS', shift=True)
kmi.properties.camera_only = True
kmi = km.keymap_items.new('view3d.render_border', 'B', 'PRESS', ctrl=True)
kmi.properties.camera_only = False
kmi = km.keymap_items.new('view3d.clear_render_border', 'B', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('view3d.camera_to_view', 'NUMPAD_0', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('view3d.object_as_camera', 'NUMPAD_0', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.call_menu', 'S', 'PRESS', shift=True)
kmi.properties.name = 'VIEW3D_MT_snap'
kmi = km.keymap_items.new('view3d.copybuffer', 'C', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('view3d.pastebuffer', 'V', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.context_set_enum', 'COMMA', 'PRESS')
kmi.properties.data_path = 'space_data.pivot_point'
kmi.properties.value = 'BOUNDING_BOX_CENTER'
kmi = km.keymap_items.new('wm.context_set_enum', 'COMMA', 'PRESS', ctrl=True)
kmi.properties.data_path = 'space_data.pivot_point'
kmi.properties.value = 'MEDIAN_POINT'
kmi = km.keymap_items.new('wm.context_toggle', 'COMMA', 'PRESS', alt=True)
kmi.properties.data_path = 'space_data.use_pivot_point_align'
kmi = km.keymap_items.new('wm.context_toggle', 'SPACE', 'PRESS', ctrl=True)
kmi.properties.data_path = 'space_data.show_manipulator'
kmi = km.keymap_items.new('wm.context_set_enum', 'PERIOD', 'PRESS')
kmi.properties.data_path = 'space_data.pivot_point'
kmi.properties.value = 'CURSOR'
kmi = km.keymap_items.new('wm.context_set_enum', 'PERIOD', 'PRESS', ctrl=True)
kmi.properties.data_path = 'space_data.pivot_point'
kmi.properties.value = 'INDIVIDUAL_ORIGINS'
kmi = km.keymap_items.new('wm.context_set_enum', 'PERIOD', 'PRESS', alt=True)
kmi.properties.data_path = 'space_data.pivot_point'
kmi.properties.value = 'ACTIVE_ELEMENT'
kmi = km.keymap_items.new('transform.translate', 'W', 'PRESS', shift=True)
kmi = km.keymap_items.new('transform.translate', 'EVT_TWEAK_S', 'ANY')
kmi = km.keymap_items.new('transform.rotate', 'E', 'PRESS', shift=True)
kmi = km.keymap_items.new('transform.resize', 'R', 'PRESS', shift=True)
kmi = km.keymap_items.new('transform.warp', 'Q', 'PRESS', shift=True)
kmi = km.keymap_items.new('transform.tosphere', 'S', 'PRESS', shift=True, alt=True)
kmi = km.keymap_items.new('transform.shear', 'S', 'PRESS', shift=True, ctrl=True, alt=True)
kmi = km.keymap_items.new('transform.select_orientation', 'SPACE', 'PRESS', alt=True)
kmi = km.keymap_items.new('transform.create_orientation', 'SPACE', 'PRESS', ctrl=True, alt=True)
kmi.properties.use = True
kmi = km.keymap_items.new('transform.mirror', 'M', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.context_toggle', 'TAB', 'PRESS', shift=True)
kmi.properties.data_path = 'tool_settings.use_snap'
kmi = km.keymap_items.new('wm.context_menu_enum', 'TAB', 'PRESS', shift=True, ctrl=True)
kmi.properties.data_path = 'tool_settings.snap_element'
kmi = km.keymap_items.new('transform.translate', 'T', 'PRESS', shift=True)
kmi.properties.texture_space = True
kmi = km.keymap_items.new('transform.resize', 'T', 'PRESS', shift=True, alt=True)
kmi.properties.texture_space = True
kmi = km.keymap_items.new('transform.skin_resize', 'A', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('view3d.enable_manipulator', 'W', 'PRESS')
kmi.properties.translate = True
kmi = km.keymap_items.new('view3d.enable_manipulator', 'E', 'PRESS')
kmi.properties.rotate = True
kmi = km.keymap_items.new('view3d.enable_manipulator', 'R', 'PRESS')
kmi.properties.scale = True
kmi = km.keymap_items.new('wm.context_toggle', 'S', 'PRESS')
kmi.properties.data_path = 'tool_settings.use_snap'
kmi = km.keymap_items.new('wm.context_toggle_enum', 'A', 'PRESS')
kmi.properties.data_path = 'tool_settings.snap_element'
kmi.properties.value_1 = 'VERTEX'
kmi.properties.value_2 = 'INCREMENT'
kmi = km.keymap_items.new('view3d.select_border', 'EVT_TWEAK_L', 'ANY', ctrl=True)
kmi = km.keymap_items.new('wm.context_toggle', 'G', 'PRESS')
kmi.properties.data_path = 'space_data.show_floor'

# Map 3D View Generic
km = kc.keymaps.new('3D View Generic', space_type='VIEW_3D', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('view3d.properties', 'N', 'PRESS')
kmi = km.keymap_items.new('view3d.toolshelf', 'D', 'PRESS')

# Map Weight Paint Vertex Selection
km = kc.keymaps.new('Weight Paint Vertex Selection', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('paint.vert_select_all', 'A', 'PRESS')
kmi = km.keymap_items.new('paint.vert_select_inverse', 'I', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('view3d.select_border', 'B', 'PRESS')
kmi = km.keymap_items.new('view3d.select_lasso', 'EVT_TWEAK_A', 'ANY', ctrl=True)
kmi.properties.deselect = False
kmi = km.keymap_items.new('view3d.select_lasso', 'EVT_TWEAK_A', 'ANY', shift=True, ctrl=True)
kmi.properties.deselect = True
kmi = km.keymap_items.new('view3d.select_circle', 'C', 'PRESS', alt=True)

# Map Pose
km = kc.keymaps.new('Pose', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('object.parent_set', 'P', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.call_menu', 'A', 'PRESS', shift=True)
kmi.properties.name = 'INFO_MT_add'
kmi = km.keymap_items.new('pose.hide', 'H', 'PRESS')
kmi.properties.unselected = False
kmi = km.keymap_items.new('pose.hide', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.keymap_items.new('pose.reveal', 'H', 'PRESS', alt=True)
kmi = km.keymap_items.new('wm.call_menu', 'A', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_pose_apply'
kmi = km.keymap_items.new('pose.rot_clear', 'R', 'PRESS', alt=True)
kmi = km.keymap_items.new('pose.loc_clear', 'G', 'PRESS', alt=True)
kmi = km.keymap_items.new('pose.scale_clear', 'S', 'PRESS', alt=True)
kmi = km.keymap_items.new('pose.quaternions_flip', 'F', 'PRESS', alt=True)
kmi = km.keymap_items.new('pose.rotation_mode_set', 'R', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('pose.copy', 'C', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('pose.paste', 'V', 'PRESS', ctrl=True)
kmi.properties.flipped = False
kmi = km.keymap_items.new('pose.paste', 'V', 'PRESS', shift=True, ctrl=True)
kmi.properties.flipped = True
kmi = km.keymap_items.new('pose.select_all', 'LEFTMOUSE', 'CLICK')
kmi.properties.action = 'TOGGLE'
kmi = km.keymap_items.new('pose.select_all', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
kmi = km.keymap_items.new('pose.select_parent', 'P', 'PRESS', shift=True)
kmi = km.keymap_items.new('pose.select_hierarchy', 'LEFT_BRACKET', 'PRESS')
kmi.properties.direction = 'PARENT'
kmi.properties.extend = False
kmi = km.keymap_items.new('pose.select_hierarchy', 'LEFT_BRACKET', 'PRESS', shift=True)
kmi.properties.direction = 'PARENT'
kmi.properties.extend = True
kmi = km.keymap_items.new('pose.select_hierarchy', 'RIGHT_BRACKET', 'PRESS')
kmi.properties.direction = 'CHILD'
kmi.properties.extend = False
kmi = km.keymap_items.new('pose.select_hierarchy', 'RIGHT_BRACKET', 'PRESS', shift=True)
kmi.properties.direction = 'CHILD'
kmi.properties.extend = True
kmi = km.keymap_items.new('pose.select_linked', 'L', 'PRESS', alt=True)
kmi = km.keymap_items.new('pose.select_grouped', 'G', 'PRESS', shift=True)
kmi = km.keymap_items.new('pose.select_flip_active', 'F', 'PRESS', shift=True)
kmi = km.keymap_items.new('pose.constraint_add_with_targets', 'C', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('pose.constraints_clear', 'C', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('pose.ik_add', 'I', 'PRESS', shift=True)
kmi = km.keymap_items.new('pose.ik_clear', 'I', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('wm.call_menu', 'G', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_pose_group'
kmi = km.keymap_items.new('wm.call_menu', 'W', 'PRESS', shift=True)
kmi.properties.name = 'VIEW3D_MT_bone_options_toggle'
kmi = km.keymap_items.new('wm.call_menu', 'W', 'PRESS', shift=True, ctrl=True)
kmi.properties.name = 'VIEW3D_MT_bone_options_enable'
kmi = km.keymap_items.new('wm.call_menu', 'W', 'PRESS', alt=True)
kmi.properties.name = 'VIEW3D_MT_bone_options_disable'
kmi = km.keymap_items.new('armature.layers_show_all', 'ACCENT_GRAVE', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('pose.armature_layers', 'M', 'PRESS', shift=True)
kmi = km.keymap_items.new('pose.bone_layers', 'M', 'PRESS')
kmi = km.keymap_items.new('transform.transform', 'S', 'PRESS', ctrl=True, alt=True)
kmi.properties.mode = 'BONE_SIZE'
kmi = km.keymap_items.new('anim.keyframe_insert_menu', 'I', 'PRESS')
kmi = km.keymap_items.new('anim.keyframe_delete_v3d', 'I', 'PRESS', alt=True)
kmi = km.keymap_items.new('anim.keying_set_active_set', 'I', 'PRESS', shift=True, ctrl=True, alt=True)
kmi = km.keymap_items.new('poselib.browse_interactive', 'L', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('poselib.pose_add', 'L', 'PRESS', shift=True)
kmi = km.keymap_items.new('poselib.pose_remove', 'L', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('poselib.pose_rename', 'L', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('pose.push', 'E', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('pose.relax', 'E', 'PRESS', alt=True)
kmi = km.keymap_items.new('pose.breakdown', 'E', 'PRESS', shift=True)
kmi = km.keymap_items.new('wm.call_menu', 'Q', 'PRESS')
kmi.properties.name = 'VIEW3D_MT_pose_specials'

# Map Object Mode
km = kc.keymaps.new('Object Mode', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('wm.context_cycle_enum', 'O', 'PRESS', shift=True)
kmi.properties.data_path = 'tool_settings.proportional_edit_falloff'
kmi = km.keymap_items.new('wm.context_toggle', 'O', 'PRESS')
kmi.properties.data_path = 'tool_settings.use_proportional_edit_objects'
kmi = km.keymap_items.new('view3d.game_start', 'P', 'PRESS')
kmi = km.keymap_items.new('object.select_all', 'LEFTMOUSE', 'CLICK')
kmi.properties.action = 'DESELECT'
kmi = km.keymap_items.new('object.select_all', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
kmi = km.keymap_items.new('object.select_linked', 'L', 'PRESS', shift=True)
kmi = km.keymap_items.new('object.select_grouped', 'G', 'PRESS', shift=True)
kmi = km.keymap_items.new('object.select_mirror', 'M', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('object.select_hierarchy', 'LEFT_BRACKET', 'PRESS')
kmi.properties.direction = 'PARENT'
kmi.properties.extend = False
kmi = km.keymap_items.new('object.select_hierarchy', 'LEFT_BRACKET', 'PRESS', shift=True)
kmi.properties.direction = 'PARENT'
kmi.properties.extend = True
kmi = km.keymap_items.new('object.select_hierarchy', 'RIGHT_BRACKET', 'PRESS')
kmi.properties.direction = 'CHILD'
kmi.properties.extend = False
kmi = km.keymap_items.new('object.select_hierarchy', 'RIGHT_BRACKET', 'PRESS', shift=True)
kmi.properties.direction = 'CHILD'
kmi.properties.extend = True
kmi = km.keymap_items.new('object.parent_set', 'P', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('object.parent_no_inverse_set', 'P', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('object.parent_clear', 'P', 'PRESS', alt=True)
kmi = km.keymap_items.new('object.track_set', 'T', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('object.track_clear', 'T', 'PRESS', alt=True)
kmi = km.keymap_items.new('object.constraint_add_with_targets', 'C', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('object.constraints_clear', 'C', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('object.location_clear', 'G', 'PRESS', alt=True)
kmi = km.keymap_items.new('object.rotation_clear', 'R', 'PRESS', alt=True)
kmi = km.keymap_items.new('object.scale_clear', 'S', 'PRESS', alt=True)
kmi = km.keymap_items.new('object.origin_clear', 'O', 'PRESS', alt=True)
kmi = km.keymap_items.new('object.hide_view_clear', 'H', 'PRESS', alt=True)
kmi = km.keymap_items.new('object.hide_view_set', 'H', 'PRESS')
kmi.properties.unselected = False
kmi = km.keymap_items.new('object.hide_view_set', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.keymap_items.new('object.hide_render_clear', 'H', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('object.hide_render_set', 'H', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('object.move_to_layer', 'M', 'PRESS')
kmi = km.keymap_items.new('object.delete', 'X', 'PRESS')
kmi.properties.use_global = False
kmi = km.keymap_items.new('object.delete', 'X', 'PRESS', shift=True)
kmi.properties.use_global = False
kmi = km.keymap_items.new('object.delete', 'DEL', 'PRESS')
kmi = km.keymap_items.new('object.delete', 'DEL', 'PRESS', shift=True)
kmi.properties.use_global = True
kmi = km.keymap_items.new('wm.call_menu', 'A', 'PRESS', shift=True)
kmi.properties.name = 'INFO_MT_add'
kmi = km.keymap_items.new('object.duplicates_make_real', 'A', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('wm.call_menu', 'A', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_object_apply'
kmi = km.keymap_items.new('wm.call_menu', 'U', 'PRESS')
kmi.properties.name = 'VIEW3D_MT_make_single_user'
kmi = km.keymap_items.new('wm.call_menu', 'L', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_make_links'
kmi = km.keymap_items.new('object.duplicate_move', 'D', 'PRESS', shift=True)
kmi = km.keymap_items.new('object.duplicate_move_linked', 'D', 'PRESS', alt=True)
kmi = km.keymap_items.new('object.join', 'J', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('object.convert', 'C', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('object.proxy_make', 'P', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('object.make_local', 'L', 'PRESS', alt=True)
kmi = km.keymap_items.new('anim.keyframe_insert_menu', 'I', 'PRESS')
kmi = km.keymap_items.new('anim.keyframe_delete_v3d', 'I', 'PRESS', alt=True)
kmi = km.keymap_items.new('anim.keying_set_active_set', 'I', 'PRESS', shift=True, ctrl=True, alt=True)
kmi = km.keymap_items.new('group.create', 'G', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('group.objects_remove', 'G', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('group.objects_remove_all', 'G', 'PRESS', shift=True, ctrl=True, alt=True)
kmi = km.keymap_items.new('group.objects_add_active', 'G', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('group.objects_remove_active', 'G', 'PRESS', shift=True, alt=True)
kmi = km.keymap_items.new('rigidbody.objects_add', 'R', 'PRESS', ctrl=True)
kmi.properties.type = 'ACTIVE'
kmi = km.keymap_items.new('rigidbody.objects_add', 'R', 'PRESS', shift=True, ctrl=True)
kmi.properties.type = 'PASSIVE'
kmi = km.keymap_items.new('rigidbody.objects_remove', 'R', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('wm.call_menu', 'Q', 'PRESS')
kmi.properties.name = 'VIEW3D_MT_object_specials'
kmi = km.keymap_items.new('object.subdivision_set', 'ZERO', 'PRESS', ctrl=True)
kmi.properties.level = 0
kmi = km.keymap_items.new('object.subdivision_set', 'ONE', 'PRESS', ctrl=True)
kmi.properties.level = 1
kmi = km.keymap_items.new('object.subdivision_set', 'TWO', 'PRESS', ctrl=True)
kmi.properties.level = 2
kmi = km.keymap_items.new('object.subdivision_set', 'THREE', 'PRESS', ctrl=True)
kmi.properties.level = 3
kmi = km.keymap_items.new('object.subdivision_set', 'FOUR', 'PRESS', ctrl=True)
kmi.properties.level = 4
kmi = km.keymap_items.new('object.subdivision_set', 'FIVE', 'PRESS', ctrl=True)
kmi.properties.level = 5

# Map Image Paint
km = kc.keymaps.new('Image Paint', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('paint.image_paint', 'LEFTMOUSE', 'PRESS')
kmi = km.keymap_items.new('paint.grab_clone', 'RIGHTMOUSE', 'PRESS')
kmi = km.keymap_items.new('paint.sample_color', 'RIGHTMOUSE', 'PRESS')
kmi = km.keymap_items.new('paint.clone_cursor_set', 'LEFTMOUSE', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('brush.active_index_set', 'ONE', 'PRESS')
kmi.properties.mode = 'image_paint'
kmi.properties.index = 0
kmi = km.keymap_items.new('brush.active_index_set', 'TWO', 'PRESS')
kmi.properties.mode = 'image_paint'
kmi.properties.index = 1
kmi = km.keymap_items.new('brush.active_index_set', 'THREE', 'PRESS')
kmi.properties.mode = 'image_paint'
kmi.properties.index = 2
kmi = km.keymap_items.new('brush.active_index_set', 'FOUR', 'PRESS')
kmi.properties.mode = 'image_paint'
kmi.properties.index = 3
kmi = km.keymap_items.new('brush.active_index_set', 'FIVE', 'PRESS')
kmi.properties.mode = 'image_paint'
kmi.properties.index = 4
kmi = km.keymap_items.new('brush.active_index_set', 'SIX', 'PRESS')
kmi.properties.mode = 'image_paint'
kmi.properties.index = 5
kmi = km.keymap_items.new('brush.active_index_set', 'SEVEN', 'PRESS')
kmi.properties.mode = 'image_paint'
kmi.properties.index = 6
kmi = km.keymap_items.new('brush.active_index_set', 'EIGHT', 'PRESS')
kmi.properties.mode = 'image_paint'
kmi.properties.index = 7
kmi = km.keymap_items.new('brush.active_index_set', 'NINE', 'PRESS')
kmi.properties.mode = 'image_paint'
kmi.properties.index = 8
kmi = km.keymap_items.new('brush.active_index_set', 'ZERO', 'PRESS')
kmi.properties.mode = 'image_paint'
kmi.properties.index = 9
kmi = km.keymap_items.new('brush.active_index_set', 'ONE', 'PRESS', shift=True)
kmi.properties.mode = 'image_paint'
kmi.properties.index = 10
kmi = km.keymap_items.new('brush.active_index_set', 'TWO', 'PRESS', shift=True)
kmi.properties.mode = 'image_paint'
kmi.properties.index = 11
kmi = km.keymap_items.new('brush.active_index_set', 'THREE', 'PRESS', shift=True)
kmi.properties.mode = 'image_paint'
kmi.properties.index = 12
kmi = km.keymap_items.new('brush.active_index_set', 'FOUR', 'PRESS', shift=True)
kmi.properties.mode = 'image_paint'
kmi.properties.index = 13
kmi = km.keymap_items.new('brush.active_index_set', 'FIVE', 'PRESS', shift=True)
kmi.properties.mode = 'image_paint'
kmi.properties.index = 14
kmi = km.keymap_items.new('brush.active_index_set', 'SIX', 'PRESS', shift=True)
kmi.properties.mode = 'image_paint'
kmi.properties.index = 15
kmi = km.keymap_items.new('brush.active_index_set', 'SEVEN', 'PRESS', shift=True)
kmi.properties.mode = 'image_paint'
kmi.properties.index = 16
kmi = km.keymap_items.new('brush.active_index_set', 'EIGHT', 'PRESS', shift=True)
kmi.properties.mode = 'image_paint'
kmi.properties.index = 17
kmi = km.keymap_items.new('brush.active_index_set', 'NINE', 'PRESS', shift=True)
kmi.properties.mode = 'image_paint'
kmi.properties.index = 18
kmi = km.keymap_items.new('brush.active_index_set', 'ZERO', 'PRESS', shift=True)
kmi.properties.mode = 'image_paint'
kmi.properties.index = 19
kmi = km.keymap_items.new('brush.scale_size', 'LEFT_BRACKET', 'PRESS')
kmi.properties.scalar = 0.8999999761581421
kmi = km.keymap_items.new('brush.scale_size', 'RIGHT_BRACKET', 'PRESS')
kmi.properties.scalar = 1.1111111640930176
kmi = km.keymap_items.new('wm.radial_control', 'F', 'PRESS', ctrl=True)
kmi.properties.data_path_primary = 'tool_settings.image_paint.brush.size'
kmi.properties.data_path_secondary = 'tool_settings.unified_paint_settings.size'
kmi.properties.use_secondary = 'tool_settings.unified_paint_settings.use_unified_size'
kmi.properties.rotation_path = 'tool_settings.image_paint.brush.texture_slot.angle'
kmi.properties.color_path = 'tool_settings.image_paint.brush.cursor_color_add'
kmi.properties.fill_color_path = 'tool_settings.image_paint.brush.color'
kmi.properties.zoom_path = 'space_data.zoom'
kmi.properties.image_id = 'tool_settings.image_paint.brush'
kmi = km.keymap_items.new('wm.radial_control', 'F', 'PRESS', shift=True)
kmi.properties.data_path_primary = 'tool_settings.image_paint.brush.strength'
kmi.properties.data_path_secondary = 'tool_settings.unified_paint_settings.strength'
kmi.properties.use_secondary = 'tool_settings.unified_paint_settings.use_unified_strength'
kmi.properties.rotation_path = 'tool_settings.image_paint.brush.texture_slot.angle'
kmi.properties.color_path = 'tool_settings.image_paint.brush.cursor_color_add'
kmi.properties.fill_color_path = 'tool_settings.image_paint.brush.color'
kmi.properties.zoom_path = ''
kmi.properties.image_id = 'tool_settings.image_paint.brush'
kmi = km.keymap_items.new('wm.radial_control', 'W', 'PRESS')
kmi.properties.data_path_primary = 'tool_settings.image_paint.brush.weight'
kmi.properties.data_path_secondary = 'tool_settings.unified_paint_settings.weight'
kmi.properties.use_secondary = 'tool_settings.unified_paint_settings.use_unified_weight'
kmi.properties.rotation_path = 'tool_settings.image_paint.brush.texture_slot.angle'
kmi.properties.color_path = 'tool_settings.image_paint.brush.cursor_color_add'
kmi.properties.fill_color_path = 'tool_settings.image_paint.brush.color'
kmi.properties.zoom_path = ''
kmi.properties.image_id = 'tool_settings.image_paint.brush'
kmi = km.keymap_items.new('wm.context_toggle', 'M', 'PRESS')
kmi.properties.data_path = 'image_paint_object.data.use_paint_mask'

# Map Vertex Paint
km = kc.keymaps.new('Vertex Paint', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('paint.vertex_paint', 'LEFTMOUSE', 'PRESS')
kmi = km.keymap_items.new('paint.sample_color', 'RIGHTMOUSE', 'PRESS')
kmi = km.keymap_items.new('paint.vertex_color_set', 'K', 'PRESS', shift=True)
kmi = km.keymap_items.new('brush.active_index_set', 'ONE', 'PRESS')
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 0
kmi = km.keymap_items.new('brush.active_index_set', 'TWO', 'PRESS')
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 1
kmi = km.keymap_items.new('brush.active_index_set', 'THREE', 'PRESS')
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 2
kmi = km.keymap_items.new('brush.active_index_set', 'FOUR', 'PRESS')
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 3
kmi = km.keymap_items.new('brush.active_index_set', 'FIVE', 'PRESS')
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 4
kmi = km.keymap_items.new('brush.active_index_set', 'SIX', 'PRESS')
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 5
kmi = km.keymap_items.new('brush.active_index_set', 'SEVEN', 'PRESS')
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 6
kmi = km.keymap_items.new('brush.active_index_set', 'EIGHT', 'PRESS')
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 7
kmi = km.keymap_items.new('brush.active_index_set', 'NINE', 'PRESS')
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 8
kmi = km.keymap_items.new('brush.active_index_set', 'ZERO', 'PRESS')
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 9
kmi = km.keymap_items.new('brush.active_index_set', 'ONE', 'PRESS', shift=True)
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 10
kmi = km.keymap_items.new('brush.active_index_set', 'TWO', 'PRESS', shift=True)
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 11
kmi = km.keymap_items.new('brush.active_index_set', 'THREE', 'PRESS', shift=True)
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 12
kmi = km.keymap_items.new('brush.active_index_set', 'FOUR', 'PRESS', shift=True)
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 13
kmi = km.keymap_items.new('brush.active_index_set', 'FIVE', 'PRESS', shift=True)
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 14
kmi = km.keymap_items.new('brush.active_index_set', 'SIX', 'PRESS', shift=True)
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 15
kmi = km.keymap_items.new('brush.active_index_set', 'SEVEN', 'PRESS', shift=True)
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 16
kmi = km.keymap_items.new('brush.active_index_set', 'EIGHT', 'PRESS', shift=True)
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 17
kmi = km.keymap_items.new('brush.active_index_set', 'NINE', 'PRESS', shift=True)
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 18
kmi = km.keymap_items.new('brush.active_index_set', 'ZERO', 'PRESS', shift=True)
kmi.properties.mode = 'vertex_paint'
kmi.properties.index = 19
kmi = km.keymap_items.new('brush.scale_size', 'LEFT_BRACKET', 'PRESS')
kmi.properties.scalar = 0.8999999761581421
kmi = km.keymap_items.new('brush.scale_size', 'RIGHT_BRACKET', 'PRESS')
kmi.properties.scalar = 1.1111111640930176
kmi = km.keymap_items.new('wm.radial_control', 'F', 'PRESS', ctrl=True)
kmi.properties.data_path_primary = 'tool_settings.vertex_paint.brush.size'
kmi.properties.data_path_secondary = 'tool_settings.unified_paint_settings.size'
kmi.properties.use_secondary = 'tool_settings.unified_paint_settings.use_unified_size'
kmi.properties.rotation_path = 'tool_settings.vertex_paint.brush.texture_slot.angle'
kmi.properties.color_path = 'tool_settings.vertex_paint.brush.cursor_color_add'
kmi.properties.fill_color_path = 'tool_settings.vertex_paint.brush.color'
kmi.properties.zoom_path = ''
kmi.properties.image_id = 'tool_settings.vertex_paint.brush'
kmi = km.keymap_items.new('wm.radial_control', 'F', 'PRESS', shift=True)
kmi.properties.data_path_primary = 'tool_settings.vertex_paint.brush.strength'
kmi.properties.data_path_secondary = 'tool_settings.unified_paint_settings.strength'
kmi.properties.use_secondary = 'tool_settings.unified_paint_settings.use_unified_strength'
kmi.properties.rotation_path = 'tool_settings.vertex_paint.brush.texture_slot.angle'
kmi.properties.color_path = 'tool_settings.vertex_paint.brush.cursor_color_add'
kmi.properties.fill_color_path = 'tool_settings.vertex_paint.brush.color'
kmi.properties.zoom_path = ''
kmi.properties.image_id = 'tool_settings.vertex_paint.brush'
kmi = km.keymap_items.new('wm.radial_control', 'Q', 'PRESS')
kmi.properties.data_path_primary = 'tool_settings.vertex_paint.brush.weight'
kmi.properties.data_path_secondary = 'tool_settings.unified_paint_settings.weight'
kmi.properties.use_secondary = 'tool_settings.unified_paint_settings.use_unified_weight'
kmi.properties.rotation_path = 'tool_settings.vertex_paint.brush.texture_slot.angle'
kmi.properties.color_path = 'tool_settings.vertex_paint.brush.cursor_color_add'
kmi.properties.fill_color_path = 'tool_settings.vertex_paint.brush.color'
kmi.properties.zoom_path = ''
kmi.properties.image_id = 'tool_settings.vertex_paint.brush'
kmi = km.keymap_items.new('wm.context_toggle', 'M', 'PRESS')
kmi.properties.data_path = 'vertex_paint_object.data.use_paint_mask'

# Map Weight Paint
km = kc.keymaps.new('Weight Paint', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('paint.weight_paint', 'LEFTMOUSE', 'PRESS')
kmi = km.keymap_items.new('paint.weight_sample', 'ACTIONMOUSE', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('paint.weight_sample_group', 'ACTIONMOUSE', 'PRESS', shift=True)
kmi = km.keymap_items.new('paint.weight_gradient', 'LEFTMOUSE', 'PRESS', alt=True)
kmi.properties.type = 'LINEAR'
kmi = km.keymap_items.new('paint.weight_gradient', 'LEFTMOUSE', 'PRESS', ctrl=True, alt=True)
kmi.properties.type = 'RADIAL'
kmi = km.keymap_items.new('paint.weight_set', 'K', 'PRESS', shift=True)
kmi = km.keymap_items.new('brush.active_index_set', 'ONE', 'PRESS')
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 0
kmi = km.keymap_items.new('brush.active_index_set', 'TWO', 'PRESS')
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 1
kmi = km.keymap_items.new('brush.active_index_set', 'THREE', 'PRESS')
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 2
kmi = km.keymap_items.new('brush.active_index_set', 'FOUR', 'PRESS')
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 3
kmi = km.keymap_items.new('brush.active_index_set', 'FIVE', 'PRESS')
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 4
kmi = km.keymap_items.new('brush.active_index_set', 'SIX', 'PRESS')
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 5
kmi = km.keymap_items.new('brush.active_index_set', 'SEVEN', 'PRESS')
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 6
kmi = km.keymap_items.new('brush.active_index_set', 'EIGHT', 'PRESS')
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 7
kmi = km.keymap_items.new('brush.active_index_set', 'NINE', 'PRESS')
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 8
kmi = km.keymap_items.new('brush.active_index_set', 'ZERO', 'PRESS')
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 9
kmi = km.keymap_items.new('brush.active_index_set', 'ONE', 'PRESS', shift=True)
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 10
kmi = km.keymap_items.new('brush.active_index_set', 'TWO', 'PRESS', shift=True)
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 11
kmi = km.keymap_items.new('brush.active_index_set', 'THREE', 'PRESS', shift=True)
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 12
kmi = km.keymap_items.new('brush.active_index_set', 'FOUR', 'PRESS', shift=True)
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 13
kmi = km.keymap_items.new('brush.active_index_set', 'FIVE', 'PRESS', shift=True)
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 14
kmi = km.keymap_items.new('brush.active_index_set', 'SIX', 'PRESS', shift=True)
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 15
kmi = km.keymap_items.new('brush.active_index_set', 'SEVEN', 'PRESS', shift=True)
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 16
kmi = km.keymap_items.new('brush.active_index_set', 'EIGHT', 'PRESS', shift=True)
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 17
kmi = km.keymap_items.new('brush.active_index_set', 'NINE', 'PRESS', shift=True)
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 18
kmi = km.keymap_items.new('brush.active_index_set', 'ZERO', 'PRESS', shift=True)
kmi.properties.mode = 'weight_paint'
kmi.properties.index = 19
kmi = km.keymap_items.new('brush.scale_size', 'LEFT_BRACKET', 'PRESS')
kmi.properties.scalar = 0.8999999761581421
kmi = km.keymap_items.new('brush.scale_size', 'RIGHT_BRACKET', 'PRESS')
kmi.properties.scalar = 1.1111111640930176
kmi = km.keymap_items.new('wm.radial_control', 'F', 'PRESS', ctrl=True)
kmi.properties.data_path_primary = 'tool_settings.weight_paint.brush.size'
kmi.properties.data_path_secondary = 'tool_settings.unified_paint_settings.size'
kmi.properties.use_secondary = 'tool_settings.unified_paint_settings.use_unified_size'
kmi.properties.rotation_path = 'tool_settings.weight_paint.brush.texture_slot.angle'
kmi.properties.color_path = 'tool_settings.weight_paint.brush.cursor_color_add'
kmi.properties.fill_color_path = ''
kmi.properties.zoom_path = ''
kmi.properties.image_id = 'tool_settings.weight_paint.brush'
kmi = km.keymap_items.new('wm.radial_control', 'F', 'PRESS', shift=True)
kmi.properties.data_path_primary = 'tool_settings.weight_paint.brush.strength'
kmi.properties.data_path_secondary = 'tool_settings.unified_paint_settings.strength'
kmi.properties.use_secondary = 'tool_settings.unified_paint_settings.use_unified_strength'
kmi.properties.rotation_path = 'tool_settings.weight_paint.brush.texture_slot.angle'
kmi.properties.color_path = 'tool_settings.weight_paint.brush.cursor_color_add'
kmi.properties.fill_color_path = ''
kmi.properties.zoom_path = ''
kmi.properties.image_id = 'tool_settings.weight_paint.brush'
kmi = km.keymap_items.new('wm.radial_control', 'Q', 'PRESS')
kmi.properties.data_path_primary = 'tool_settings.weight_paint.brush.weight'
kmi.properties.data_path_secondary = 'tool_settings.unified_paint_settings.weight'
kmi.properties.use_secondary = 'tool_settings.unified_paint_settings.use_unified_weight'
kmi.properties.rotation_path = 'tool_settings.weight_paint.brush.texture_slot.angle'
kmi.properties.color_path = 'tool_settings.weight_paint.brush.cursor_color_add'
kmi.properties.fill_color_path = ''
kmi.properties.zoom_path = ''
kmi.properties.image_id = 'tool_settings.weight_paint.brush'
kmi = km.keymap_items.new('wm.context_toggle', 'M', 'PRESS')
kmi.properties.data_path = 'weight_paint_object.data.use_paint_mask'
kmi = km.keymap_items.new('wm.context_toggle', 'V', 'PRESS')
kmi.properties.data_path = 'weight_paint_object.data.use_paint_mask_vertex'
kmi = km.keymap_items.new('paint.weight_from_bones', 'Q', 'PRESS')

# Map Sculpt
km = kc.keymaps.new('Sculpt', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('sculpt.brush_stroke', 'LEFTMOUSE', 'PRESS')
kmi.properties.mode = 'NORMAL'
kmi = km.keymap_items.new('sculpt.brush_stroke', 'LEFTMOUSE', 'PRESS', ctrl=True)
kmi.properties.mode = 'INVERT'
kmi = km.keymap_items.new('sculpt.brush_stroke', 'LEFTMOUSE', 'PRESS', shift=True)
kmi.properties.mode = 'SMOOTH'
kmi = km.keymap_items.new('paint.hide_show', 'H', 'PRESS', shift=True)
kmi.properties.action = 'SHOW'
kmi.properties.area = 'INSIDE'
kmi = km.keymap_items.new('paint.hide_show', 'H', 'PRESS')
kmi.properties.action = 'HIDE'
kmi.properties.area = 'INSIDE'
kmi = km.keymap_items.new('paint.hide_show', 'H', 'PRESS', alt=True)
kmi.properties.action = 'SHOW'
kmi.properties.area = 'ALL'
kmi = km.keymap_items.new('object.subdivision_set', 'ZERO', 'PRESS', ctrl=True)
kmi.properties.level = 0
kmi = km.keymap_items.new('object.subdivision_set', 'ONE', 'PRESS', ctrl=True)
kmi.properties.level = 1
kmi = km.keymap_items.new('object.subdivision_set', 'TWO', 'PRESS', ctrl=True)
kmi.properties.level = 2
kmi = km.keymap_items.new('object.subdivision_set', 'THREE', 'PRESS', ctrl=True)
kmi.properties.level = 3
kmi = km.keymap_items.new('object.subdivision_set', 'FOUR', 'PRESS', ctrl=True)
kmi.properties.level = 4
kmi = km.keymap_items.new('object.subdivision_set', 'FIVE', 'PRESS', ctrl=True)
kmi.properties.level = 5
kmi = km.keymap_items.new('paint.mask_flood_fill', 'M', 'PRESS', alt=True)
kmi.properties.mode = 'VALUE'
kmi.properties.value = 0.0
kmi = km.keymap_items.new('paint.mask_flood_fill', 'I', 'PRESS', ctrl=True)
kmi.properties.mode = 'INVERT'
kmi = km.keymap_items.new('sculpt.dynamic_topology_toggle', 'D', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.radial_control', 'D', 'PRESS', shift=True)
kmi.properties.data_path_primary = 'tool_settings.sculpt.detail_size'
kmi.properties.data_path_secondary = ''
kmi.properties.use_secondary = ''
kmi.properties.rotation_path = 'tool_settings.sculpt.brush.texture_slot.angle'
kmi.properties.color_path = 'tool_settings.sculpt.brush.cursor_color_add'
kmi.properties.fill_color_path = ''
kmi.properties.zoom_path = ''
kmi.properties.image_id = 'tool_settings.sculpt.brush'
kmi = km.keymap_items.new('object.subdivision_set', 'PAGE_UP', 'PRESS')
kmi.properties.level = 1
kmi.properties.relative = True
kmi = km.keymap_items.new('object.subdivision_set', 'PAGE_DOWN', 'PRESS')
kmi.properties.level = -1
kmi.properties.relative = True
kmi = km.keymap_items.new('brush.active_index_set', 'ONE', 'PRESS')
kmi.properties.mode = 'sculpt'
kmi.properties.index = 0
kmi = km.keymap_items.new('brush.active_index_set', 'TWO', 'PRESS')
kmi.properties.mode = 'sculpt'
kmi.properties.index = 1
kmi = km.keymap_items.new('brush.active_index_set', 'THREE', 'PRESS')
kmi.properties.mode = 'sculpt'
kmi.properties.index = 2
kmi = km.keymap_items.new('brush.active_index_set', 'FOUR', 'PRESS')
kmi.properties.mode = 'sculpt'
kmi.properties.index = 3
kmi = km.keymap_items.new('brush.active_index_set', 'FIVE', 'PRESS')
kmi.properties.mode = 'sculpt'
kmi.properties.index = 4
kmi = km.keymap_items.new('brush.active_index_set', 'SIX', 'PRESS')
kmi.properties.mode = 'sculpt'
kmi.properties.index = 5
kmi = km.keymap_items.new('brush.active_index_set', 'SEVEN', 'PRESS')
kmi.properties.mode = 'sculpt'
kmi.properties.index = 6
kmi = km.keymap_items.new('brush.active_index_set', 'EIGHT', 'PRESS')
kmi.properties.mode = 'sculpt'
kmi.properties.index = 7
kmi = km.keymap_items.new('brush.active_index_set', 'NINE', 'PRESS')
kmi.properties.mode = 'sculpt'
kmi.properties.index = 8
kmi = km.keymap_items.new('brush.active_index_set', 'ZERO', 'PRESS')
kmi.properties.mode = 'sculpt'
kmi.properties.index = 9
kmi = km.keymap_items.new('brush.active_index_set', 'ONE', 'PRESS', shift=True)
kmi.properties.mode = 'sculpt'
kmi.properties.index = 10
kmi = km.keymap_items.new('brush.active_index_set', 'TWO', 'PRESS', shift=True)
kmi.properties.mode = 'sculpt'
kmi.properties.index = 11
kmi = km.keymap_items.new('brush.active_index_set', 'THREE', 'PRESS', shift=True)
kmi.properties.mode = 'sculpt'
kmi.properties.index = 12
kmi = km.keymap_items.new('brush.active_index_set', 'FOUR', 'PRESS', shift=True)
kmi.properties.mode = 'sculpt'
kmi.properties.index = 13
kmi = km.keymap_items.new('brush.active_index_set', 'FIVE', 'PRESS', shift=True)
kmi.properties.mode = 'sculpt'
kmi.properties.index = 14
kmi = km.keymap_items.new('brush.active_index_set', 'SIX', 'PRESS', shift=True)
kmi.properties.mode = 'sculpt'
kmi.properties.index = 15
kmi = km.keymap_items.new('brush.active_index_set', 'SEVEN', 'PRESS', shift=True)
kmi.properties.mode = 'sculpt'
kmi.properties.index = 16
kmi = km.keymap_items.new('brush.active_index_set', 'EIGHT', 'PRESS', shift=True)
kmi.properties.mode = 'sculpt'
kmi.properties.index = 17
kmi = km.keymap_items.new('brush.active_index_set', 'NINE', 'PRESS', shift=True)
kmi.properties.mode = 'sculpt'
kmi.properties.index = 18
kmi = km.keymap_items.new('brush.active_index_set', 'ZERO', 'PRESS', shift=True)
kmi.properties.mode = 'sculpt'
kmi.properties.index = 19
kmi = km.keymap_items.new('brush.scale_size', 'LEFT_BRACKET', 'PRESS')
kmi.properties.scalar = 0.8999999761581421
kmi = km.keymap_items.new('brush.scale_size', 'RIGHT_BRACKET', 'PRESS')
kmi.properties.scalar = 1.1111111640930176
kmi = km.keymap_items.new('wm.radial_control', 'F', 'PRESS', ctrl=True)
kmi.properties.data_path_primary = 'tool_settings.sculpt.brush.size'
kmi.properties.data_path_secondary = 'tool_settings.unified_paint_settings.size'
kmi.properties.use_secondary = 'tool_settings.unified_paint_settings.use_unified_size'
kmi.properties.rotation_path = 'tool_settings.sculpt.brush.texture_slot.angle'
kmi.properties.color_path = 'tool_settings.sculpt.brush.cursor_color_add'
kmi.properties.fill_color_path = ''
kmi.properties.zoom_path = ''
kmi.properties.image_id = 'tool_settings.sculpt.brush'
kmi = km.keymap_items.new('wm.radial_control', 'F', 'PRESS', shift=True)
kmi.properties.data_path_primary = 'tool_settings.sculpt.brush.strength'
kmi.properties.data_path_secondary = 'tool_settings.unified_paint_settings.strength'
kmi.properties.use_secondary = 'tool_settings.unified_paint_settings.use_unified_strength'
kmi.properties.rotation_path = 'tool_settings.sculpt.brush.texture_slot.angle'
kmi.properties.color_path = 'tool_settings.sculpt.brush.cursor_color_add'
kmi.properties.fill_color_path = ''
kmi.properties.zoom_path = ''
kmi.properties.image_id = 'tool_settings.sculpt.brush'
kmi = km.keymap_items.new('wm.radial_control', 'W', 'PRESS')
kmi.properties.data_path_primary = 'tool_settings.sculpt.brush.weight'
kmi.properties.data_path_secondary = 'tool_settings.unified_paint_settings.weight'
kmi.properties.use_secondary = 'tool_settings.unified_paint_settings.use_unified_weight'
kmi.properties.rotation_path = 'tool_settings.sculpt.brush.texture_slot.angle'
kmi.properties.color_path = 'tool_settings.sculpt.brush.cursor_color_add'
kmi.properties.fill_color_path = ''
kmi.properties.zoom_path = ''
kmi.properties.image_id = 'tool_settings.sculpt.brush'
kmi = km.keymap_items.new('wm.radial_control', 'F', 'PRESS', ctrl=True)
kmi.properties.data_path_primary = 'tool_settings.sculpt.brush.texture_slot.angle'
kmi.properties.data_path_secondary = ''
kmi.properties.use_secondary = ''
kmi.properties.rotation_path = 'tool_settings.sculpt.brush.texture_slot.angle'
kmi.properties.color_path = 'tool_settings.sculpt.brush.cursor_color_add'
kmi.properties.fill_color_path = ''
kmi.properties.zoom_path = ''
kmi.properties.image_id = 'tool_settings.sculpt.brush'
kmi = km.keymap_items.new('paint.brush_select', 'D', 'PRESS')
kmi.properties.paint_mode = 'SCULPT'
kmi.properties.sculpt_tool = 'DRAW'
kmi = km.keymap_items.new('paint.brush_select', 'S', 'PRESS')
kmi.properties.paint_mode = 'SCULPT'
kmi.properties.sculpt_tool = 'SMOOTH'
kmi = km.keymap_items.new('paint.brush_select', 'P', 'PRESS')
kmi.properties.paint_mode = 'SCULPT'
kmi.properties.sculpt_tool = 'PINCH'
kmi = km.keymap_items.new('paint.brush_select', 'I', 'PRESS')
kmi.properties.paint_mode = 'SCULPT'
kmi.properties.sculpt_tool = 'INFLATE'
kmi = km.keymap_items.new('paint.brush_select', 'G', 'PRESS')
kmi.properties.paint_mode = 'SCULPT'
kmi.properties.sculpt_tool = 'GRAB'
kmi = km.keymap_items.new('paint.brush_select', 'L', 'PRESS')
kmi.properties.paint_mode = 'SCULPT'
kmi.properties.sculpt_tool = 'LAYER'
kmi = km.keymap_items.new('paint.brush_select', 'T', 'PRESS', shift=True)
kmi.properties.paint_mode = 'SCULPT'
kmi.properties.sculpt_tool = 'FLATTEN'
kmi = km.keymap_items.new('paint.brush_select', 'C', 'PRESS')
kmi.properties.paint_mode = 'SCULPT'
kmi.properties.sculpt_tool = 'CLAY'
kmi = km.keymap_items.new('paint.brush_select', 'C', 'PRESS', shift=True)
kmi.properties.paint_mode = 'SCULPT'
kmi.properties.sculpt_tool = 'CREASE'
kmi = km.keymap_items.new('paint.brush_select', 'K', 'PRESS')
kmi.properties.paint_mode = 'SCULPT'
kmi.properties.sculpt_tool = 'SNAKE_HOOK'
kmi = km.keymap_items.new('paint.brush_select', 'M', 'PRESS')
kmi.properties.paint_mode = 'SCULPT'
kmi.properties.sculpt_tool = 'MASK'
kmi.properties.toggle = True
kmi.properties.create_missing = True
kmi = km.keymap_items.new('wm.context_menu_enum', 'A', 'PRESS')
kmi.properties.data_path = 'tool_settings.sculpt.brush.stroke_method'
kmi = km.keymap_items.new('wm.context_toggle', 'S', 'PRESS', shift=True)
kmi.properties.data_path = 'tool_settings.sculpt.brush.use_smooth_stroke'
kmi = km.keymap_items.new('wm.context_menu_enum', 'R', 'PRESS')
kmi.properties.data_path = 'tool_settings.sculpt.brush.texture_angle_source_random'

# Map Mesh
km = kc.keymaps.new('Mesh', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('mesh.loopcut_slide', 'R', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('mesh.inset', 'I', 'PRESS')
kmi = km.keymap_items.new('mesh.bevel', 'B', 'PRESS', ctrl=True)
kmi.properties.vertex_only = False
kmi = km.keymap_items.new('mesh.bevel', 'B', 'PRESS', shift=True, ctrl=True)
kmi.properties.vertex_only = True
kmi = km.keymap_items.new('mesh.loop_select', 'SELECTMOUSE', 'PRESS', alt=True)
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = False
kmi = km.keymap_items.new('mesh.loop_select', 'SELECTMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = True
kmi = km.keymap_items.new('mesh.edgering_select', 'SELECTMOUSE', 'PRESS', ctrl=True, alt=True)
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = False
kmi = km.keymap_items.new('mesh.edgering_select', 'SELECTMOUSE', 'PRESS', shift=True, ctrl=True, alt=True)
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = True
kmi = km.keymap_items.new('mesh.select_shortest_path', 'SELECTMOUSE', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('mesh.select_all', 'LEFTMOUSE', 'CLICK')
kmi.properties.action = 'DESELECT'
kmi = km.keymap_items.new('mesh.select_all', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
kmi = km.keymap_items.new('mesh.select_more', 'NUMPAD_PLUS', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('mesh.select_less', 'NUMPAD_MINUS', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('mesh.select_non_manifold', 'M', 'PRESS', shift=True, ctrl=True, alt=True)
kmi = km.keymap_items.new('mesh.select_linked', 'L', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('mesh.select_linked_pick', 'L', 'PRESS', alt=True)
kmi.properties.deselect = False
kmi = km.keymap_items.new('mesh.select_linked_pick', 'L', 'PRESS', shift=True)
kmi.properties.deselect = True
kmi = km.keymap_items.new('mesh.faces_select_linked_flat', 'F', 'PRESS', shift=True, ctrl=True, alt=True)
kmi = km.keymap_items.new('mesh.select_similar', 'G', 'PRESS', shift=True)
kmi = km.keymap_items.new('wm.call_menu', 'TAB', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_edit_mesh_select_mode'
kmi = km.keymap_items.new('mesh.hide', 'H', 'PRESS')
kmi.properties.unselected = False
kmi = km.keymap_items.new('mesh.hide', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.keymap_items.new('mesh.reveal', 'H', 'PRESS', alt=True)
kmi = km.keymap_items.new('mesh.normals_make_consistent', 'N', 'PRESS', ctrl=True)
kmi.properties.inside = False
kmi = km.keymap_items.new('mesh.normals_make_consistent', 'N', 'PRESS', shift=True, ctrl=True)
kmi.properties.inside = True
kmi = km.keymap_items.new('view3d.edit_mesh_extrude_move_normal', 'E', 'PRESS')
kmi = km.keymap_items.new('wm.call_menu', 'E', 'PRESS', alt=True)
kmi.properties.name = 'VIEW3D_MT_edit_mesh_extrude'
kmi = km.keymap_items.new('transform.edge_crease', 'E', 'PRESS', shift=True)
kmi = km.keymap_items.new('mesh.spin', 'R', 'PRESS', alt=True)
kmi = km.keymap_items.new('mesh.fill', 'F', 'PRESS', alt=True)
kmi = km.keymap_items.new('mesh.beautify_fill', 'F', 'PRESS', shift=True, alt=True)
kmi = km.keymap_items.new('mesh.quads_convert_to_tris', 'T', 'PRESS', ctrl=True)
kmi.properties.use_beauty = True
kmi = km.keymap_items.new('mesh.quads_convert_to_tris', 'T', 'PRESS', shift=True, ctrl=True)
kmi.properties.use_beauty = False
kmi = km.keymap_items.new('mesh.tris_convert_to_quads', 'J', 'PRESS', alt=True)
kmi = km.keymap_items.new('mesh.rip_move', 'V', 'PRESS')
kmi = km.keymap_items.new('mesh.rip_move_fill', 'V', 'PRESS', alt=True)
kmi = km.keymap_items.new('mesh.merge', 'M', 'PRESS', alt=True)
kmi = km.keymap_items.new('transform.shrink_fatten', 'S', 'PRESS', alt=True)
kmi = km.keymap_items.new('mesh.edge_face_add', 'F', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('mesh.duplicate_move', 'D', 'PRESS', shift=True)
kmi = km.keymap_items.new('wm.call_menu', 'A', 'PRESS', shift=True)
kmi.properties.name = 'INFO_MT_mesh_add'
kmi = km.keymap_items.new('mesh.separate', 'P', 'PRESS')
kmi = km.keymap_items.new('mesh.split', 'Y', 'PRESS')
kmi = km.keymap_items.new('mesh.vert_connect', 'J', 'PRESS')
kmi = km.keymap_items.new('transform.vert_slide', 'V', 'PRESS', shift=True)
kmi = km.keymap_items.new('mesh.dupli_extrude_cursor', 'ACTIONMOUSE', 'CLICK', ctrl=True)
kmi.properties.rotate_source = True
kmi = km.keymap_items.new('mesh.dupli_extrude_cursor', 'ACTIONMOUSE', 'CLICK', shift=True, ctrl=True)
kmi.properties.rotate_source = False
kmi = km.keymap_items.new('wm.call_menu', 'X', 'PRESS')
kmi.properties.name = 'VIEW3D_MT_edit_mesh_delete'
kmi = km.keymap_items.new('wm.call_menu', 'DEL', 'PRESS')
kmi.properties.name = 'VIEW3D_MT_edit_mesh_delete'
kmi = km.keymap_items.new('mesh.knife_tool', 'K', 'PRESS')
kmi.properties.use_occlude_geometry = True
kmi.properties.only_selected = False
kmi = km.keymap_items.new('mesh.knife_tool', 'K', 'PRESS', shift=True)
kmi.properties.use_occlude_geometry = False
kmi.properties.only_selected = True
kmi = km.keymap_items.new('object.vertex_parent_set', 'P', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.call_menu', 'Q', 'PRESS')
kmi.properties.name = 'VIEW3D_MT_edit_mesh_specials'
kmi = km.keymap_items.new('wm.call_menu', 'F', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_edit_mesh_faces'
kmi = km.keymap_items.new('wm.call_menu', 'E', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_edit_mesh_edges'
kmi = km.keymap_items.new('wm.call_menu', 'V', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_edit_mesh_vertices'
kmi = km.keymap_items.new('wm.call_menu', 'H', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_hook'
kmi = km.keymap_items.new('wm.call_menu', 'U', 'PRESS')
kmi.properties.name = 'VIEW3D_MT_uv_map'
kmi = km.keymap_items.new('wm.call_menu', 'G', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_vertex_group'
kmi = km.keymap_items.new('object.subdivision_set', 'ZERO', 'PRESS', ctrl=True)
kmi.properties.level = 0
kmi = km.keymap_items.new('object.subdivision_set', 'ONE', 'PRESS', ctrl=True)
kmi.properties.level = 1
kmi = km.keymap_items.new('object.subdivision_set', 'TWO', 'PRESS', ctrl=True)
kmi.properties.level = 2
kmi = km.keymap_items.new('object.subdivision_set', 'THREE', 'PRESS', ctrl=True)
kmi.properties.level = 3
kmi = km.keymap_items.new('object.subdivision_set', 'FOUR', 'PRESS', ctrl=True)
kmi.properties.level = 4
kmi = km.keymap_items.new('object.subdivision_set', 'FIVE', 'PRESS', ctrl=True)
kmi.properties.level = 5
kmi = km.keymap_items.new('wm.context_cycle_enum', 'O', 'PRESS', shift=True)
kmi.properties.data_path = 'tool_settings.proportional_edit_falloff'
kmi = km.keymap_items.new('wm.context_toggle_enum', 'O', 'PRESS')
kmi.properties.data_path = 'tool_settings.proportional_edit'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'ENABLED'
kmi = km.keymap_items.new('wm.context_toggle_enum', 'O', 'PRESS', alt=True)
kmi.properties.data_path = 'tool_settings.proportional_edit'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'CONNECTED'
kmi = km.keymap_items.new('wm.context_set_value', 'ONE', 'PRESS')
kmi.properties.data_path = 'tool_settings.mesh_select_mode'
kmi.properties.value = '(True,False,False)'
kmi = km.keymap_items.new('wm.context_set_value', 'TWO', 'PRESS')
kmi.properties.data_path = 'tool_settings.mesh_select_mode'
kmi.properties.value = '(False,True,False)'
kmi = km.keymap_items.new('wm.context_set_value', 'THREE', 'PRESS')
kmi.properties.data_path = 'tool_settings.mesh_select_mode'
kmi.properties.value = '(False,False,True)'

# Map Curve
km = kc.keymaps.new('Curve', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('wm.call_menu', 'A', 'PRESS', shift=True)
kmi.properties.name = 'INFO_MT_edit_curve_add'
kmi = km.keymap_items.new('curve.handle_type_set', 'V', 'PRESS')
kmi = km.keymap_items.new('curve.vertex_add', 'LEFTMOUSE', 'CLICK', ctrl=True)
kmi = km.keymap_items.new('curve.select_all', 'LEFTMOUSE', 'CLICK')
kmi.properties.action = 'TOGGLE'
kmi = km.keymap_items.new('curve.select_all', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
kmi = km.keymap_items.new('curve.select_row', 'R', 'PRESS', shift=True)
kmi = km.keymap_items.new('curve.select_more', 'NUMPAD_PLUS', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('curve.select_less', 'NUMPAD_MINUS', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('curve.select_linked', 'L', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('curve.select_linked_pick', 'L', 'PRESS')
kmi.properties.deselect = False
kmi = km.keymap_items.new('curve.select_linked_pick', 'L', 'PRESS', shift=True)
kmi.properties.deselect = True
kmi = km.keymap_items.new('curve.separate', 'P', 'PRESS')
kmi = km.keymap_items.new('curve.extrude_move', 'E', 'PRESS')
kmi = km.keymap_items.new('curve.duplicate_move', 'D', 'PRESS', shift=True)
kmi = km.keymap_items.new('curve.make_segment', 'F', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('curve.cyclic_toggle', 'C', 'PRESS', alt=True)
kmi = km.keymap_items.new('curve.delete', 'X', 'PRESS')
kmi = km.keymap_items.new('curve.delete', 'DEL', 'PRESS')
kmi = km.keymap_items.new('curve.tilt_clear', 'T', 'PRESS', alt=True)
kmi = km.keymap_items.new('transform.tilt', 'T', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('transform.transform', 'S', 'PRESS', alt=True)
kmi.properties.mode = 'CURVE_SHRINKFATTEN'
kmi = km.keymap_items.new('curve.reveal', 'H', 'PRESS', alt=True)
kmi = km.keymap_items.new('curve.hide', 'H', 'PRESS')
kmi.properties.unselected = False
kmi = km.keymap_items.new('curve.hide', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.keymap_items.new('object.vertex_parent_set', 'P', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.call_menu', 'Q', 'PRESS')
kmi.properties.name = 'VIEW3D_MT_edit_curve_specials'
kmi = km.keymap_items.new('wm.call_menu', 'H', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_hook'
kmi = km.keymap_items.new('wm.context_cycle_enum', 'O', 'PRESS', shift=True)
kmi.properties.data_path = 'tool_settings.proportional_edit_falloff'
kmi = km.keymap_items.new('wm.context_toggle_enum', 'O', 'PRESS')
kmi.properties.data_path = 'tool_settings.proportional_edit'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'ENABLED'
kmi = km.keymap_items.new('wm.context_toggle_enum', 'O', 'PRESS', alt=True)
kmi.properties.data_path = 'tool_settings.proportional_edit'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'CONNECTED'

# Map Armature
km = kc.keymaps.new('Armature', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('sketch.delete', 'X', 'PRESS')
kmi = km.keymap_items.new('sketch.delete', 'DEL', 'PRESS')
kmi = km.keymap_items.new('sketch.finish_stroke', 'RIGHTMOUSE', 'PRESS')
kmi = km.keymap_items.new('sketch.cancel_stroke', 'ESC', 'PRESS')
kmi = km.keymap_items.new('sketch.gesture', 'LEFTMOUSE', 'PRESS', shift=True)
kmi = km.keymap_items.new('sketch.draw_stroke', 'LEFTMOUSE', 'PRESS')
kmi = km.keymap_items.new('sketch.draw_stroke', 'LEFTMOUSE', 'PRESS', ctrl=True)
kmi.properties.snap = True
kmi = km.keymap_items.new('sketch.draw_preview', 'MOUSEMOVE', 'ANY')
kmi = km.keymap_items.new('sketch.draw_preview', 'MOUSEMOVE', 'ANY', ctrl=True)
kmi.properties.snap = True
kmi = km.keymap_items.new('armature.hide', 'H', 'PRESS')
kmi.properties.unselected = False
kmi = km.keymap_items.new('armature.hide', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.keymap_items.new('armature.reveal', 'H', 'PRESS', alt=True)
kmi = km.keymap_items.new('armature.align', 'A', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('armature.calculate_roll', 'N', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('armature.switch_direction', 'F', 'PRESS', alt=True)
kmi = km.keymap_items.new('armature.bone_primitive_add', 'A', 'PRESS', shift=True)
kmi = km.keymap_items.new('armature.parent_set', 'P', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('armature.parent_clear', 'P', 'PRESS', alt=True)
kmi = km.keymap_items.new('armature.select_all', 'LEFTMOUSE', 'CLICK')
kmi.properties.action = 'TOGGLE'
kmi = km.keymap_items.new('armature.select_all', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
kmi = km.keymap_items.new('armature.select_hierarchy', 'LEFT_BRACKET', 'PRESS')
kmi.properties.direction = 'PARENT'
kmi.properties.extend = False
kmi = km.keymap_items.new('armature.select_hierarchy', 'LEFT_BRACKET', 'PRESS', shift=True)
kmi.properties.direction = 'PARENT'
kmi.properties.extend = True
kmi = km.keymap_items.new('armature.select_hierarchy', 'RIGHT_BRACKET', 'PRESS')
kmi.properties.direction = 'CHILD'
kmi.properties.extend = False
kmi = km.keymap_items.new('armature.select_hierarchy', 'RIGHT_BRACKET', 'PRESS', shift=True)
kmi.properties.direction = 'CHILD'
kmi.properties.extend = True
kmi = km.keymap_items.new('armature.select_similar', 'G', 'PRESS', shift=True)
kmi = km.keymap_items.new('armature.select_linked', 'L', 'PRESS')
kmi = km.keymap_items.new('armature.delete', 'X', 'PRESS')
kmi = km.keymap_items.new('armature.delete', 'DEL', 'PRESS')
kmi = km.keymap_items.new('armature.duplicate_move', 'D', 'PRESS', shift=True)
kmi = km.keymap_items.new('armature.extrude_move', 'E', 'PRESS')
kmi = km.keymap_items.new('armature.extrude_forked', 'E', 'PRESS', shift=True)
kmi = km.keymap_items.new('armature.click_extrude', 'LEFTMOUSE', 'CLICK', ctrl=True)
kmi = km.keymap_items.new('armature.fill', 'F', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('armature.merge', 'M', 'PRESS', alt=True)
kmi = km.keymap_items.new('armature.separate', 'P', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('wm.call_menu', 'W', 'PRESS', shift=True)
kmi.properties.name = 'VIEW3D_MT_bone_options_toggle'
kmi = km.keymap_items.new('wm.call_menu', 'W', 'PRESS', shift=True, ctrl=True)
kmi.properties.name = 'VIEW3D_MT_bone_options_enable'
kmi = km.keymap_items.new('wm.call_menu', 'W', 'PRESS', alt=True)
kmi.properties.name = 'VIEW3D_MT_bone_options_disable'
kmi = km.keymap_items.new('armature.layers_show_all', 'ACCENT_GRAVE', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('armature.armature_layers', 'M', 'PRESS', shift=True)
kmi = km.keymap_items.new('armature.bone_layers', 'M', 'PRESS')
kmi = km.keymap_items.new('transform.transform', 'S', 'PRESS', ctrl=True, alt=True)
kmi.properties.mode = 'BONE_SIZE'
kmi = km.keymap_items.new('transform.transform', 'R', 'PRESS', ctrl=True)
kmi.properties.mode = 'BONE_ROLL'
kmi = km.keymap_items.new('wm.call_menu', 'Q', 'PRESS')
kmi.properties.name = 'VIEW3D_MT_armature_specials'

# Map Metaball
km = kc.keymaps.new('Metaball', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('object.metaball_add', 'A', 'PRESS', shift=True)
kmi = km.keymap_items.new('mball.reveal_metaelems', 'H', 'PRESS', alt=True)
kmi = km.keymap_items.new('mball.hide_metaelems', 'H', 'PRESS')
kmi.properties.unselected = False
kmi = km.keymap_items.new('mball.hide_metaelems', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.keymap_items.new('mball.delete_metaelems', 'X', 'PRESS')
kmi = km.keymap_items.new('mball.delete_metaelems', 'DEL', 'PRESS')
kmi = km.keymap_items.new('mball.duplicate_metaelems', 'D', 'PRESS', shift=True)
kmi = km.keymap_items.new('mball.select_all', 'LEFTMOUSE', 'CLICK')
kmi.properties.action = 'TOGGLE'
kmi = km.keymap_items.new('mball.select_all', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
kmi = km.keymap_items.new('wm.context_cycle_enum', 'O', 'PRESS', shift=True)
kmi.properties.data_path = 'tool_settings.proportional_edit_falloff'
kmi = km.keymap_items.new('wm.context_toggle_enum', 'O', 'PRESS')
kmi.properties.data_path = 'tool_settings.proportional_edit'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'ENABLED'
kmi = km.keymap_items.new('wm.context_toggle_enum', 'O', 'PRESS', alt=True)
kmi.properties.data_path = 'tool_settings.proportional_edit'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'CONNECTED'

# Map Lattice
km = kc.keymaps.new('Lattice', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('lattice.select_all', 'LEFTMOUSE', 'CLICK')
kmi.properties.action = 'TOGGLE'
kmi = km.keymap_items.new('lattice.select_all', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
kmi = km.keymap_items.new('object.vertex_parent_set', 'P', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('lattice.flip', 'F', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.call_menu', 'H', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_hook'
kmi = km.keymap_items.new('wm.context_cycle_enum', 'O', 'PRESS', shift=True)
kmi.properties.data_path = 'tool_settings.proportional_edit_falloff'
kmi = km.keymap_items.new('wm.context_toggle_enum', 'O', 'PRESS')
kmi.properties.data_path = 'tool_settings.proportional_edit'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'ENABLED'

# Map Particle
km = kc.keymaps.new('Particle', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('particle.select_all', 'A', 'PRESS')
kmi.properties.action = 'TOGGLE'
kmi = km.keymap_items.new('particle.select_all', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
kmi = km.keymap_items.new('particle.select_more', 'NUMPAD_PLUS', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('particle.select_less', 'NUMPAD_MINUS', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('particle.select_linked', 'L', 'PRESS')
kmi.properties.deselect = False
kmi = km.keymap_items.new('particle.select_linked', 'L', 'PRESS', shift=True)
kmi.properties.deselect = True
kmi = km.keymap_items.new('particle.delete', 'X', 'PRESS')
kmi = km.keymap_items.new('particle.delete', 'DEL', 'PRESS')
kmi = km.keymap_items.new('particle.reveal', 'H', 'PRESS', alt=True)
kmi = km.keymap_items.new('particle.hide', 'H', 'PRESS')
kmi.properties.unselected = False
kmi = km.keymap_items.new('particle.hide', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.keymap_items.new('view3d.manipulator', 'LEFTMOUSE', 'PRESS', any=True)
kmi.properties.release_confirm = True
kmi = km.keymap_items.new('particle.brush_edit', 'LEFTMOUSE', 'PRESS')
kmi = km.keymap_items.new('particle.brush_edit', 'LEFTMOUSE', 'PRESS', shift=True)
kmi = km.keymap_items.new('wm.radial_control', 'F', 'PRESS', ctrl=True)
kmi.properties.data_path_primary = 'tool_settings.particle_edit.brush.size'
kmi = km.keymap_items.new('wm.radial_control', 'F', 'PRESS', shift=True)
kmi.properties.data_path_primary = 'tool_settings.particle_edit.brush.strength'
kmi = km.keymap_items.new('wm.call_menu', 'W', 'PRESS')
kmi.properties.name = 'VIEW3D_MT_particle_specials'
kmi = km.keymap_items.new('particle.weight_set', 'K', 'PRESS', shift=True)
kmi = km.keymap_items.new('wm.context_cycle_enum', 'O', 'PRESS', shift=True)
kmi.properties.data_path = 'tool_settings.proportional_edit_falloff'
kmi = km.keymap_items.new('wm.context_toggle_enum', 'O', 'PRESS')
kmi.properties.data_path = 'tool_settings.proportional_edit'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'ENABLED'

# Map Transform Modal Map
km = kc.keymaps.new('Transform Modal Map', space_type='EMPTY', region_type='WINDOW', modal=True)

kmi = km.keymap_items.new_modal('CANCEL', 'ESC', 'PRESS', any=True)
kmi = km.keymap_items.new_modal('CONFIRM', 'LEFTMOUSE', 'RELEASE', any=True)
kmi = km.keymap_items.new_modal('CONFIRM', 'RET', 'PRESS', any=True)
kmi = km.keymap_items.new_modal('CONFIRM', 'NUMPAD_ENTER', 'PRESS', any=True)
kmi = km.keymap_items.new_modal('TRANSLATE', 'W', 'PRESS')
kmi = km.keymap_items.new_modal('ROTATE', 'E', 'PRESS')
kmi = km.keymap_items.new_modal('RESIZE', 'R', 'PRESS')
kmi = km.keymap_items.new_modal('SNAP_TOGGLE', 'TAB', 'PRESS', shift=True)
kmi = km.keymap_items.new_modal('SNAP_INV_ON', 'LEFT_CTRL', 'PRESS', any=True)
kmi = km.keymap_items.new_modal('SNAP_INV_OFF', 'LEFT_CTRL', 'RELEASE', any=True)
kmi = km.keymap_items.new_modal('SNAP_INV_ON', 'RIGHT_CTRL', 'PRESS', any=True)
kmi = km.keymap_items.new_modal('SNAP_INV_OFF', 'RIGHT_CTRL', 'RELEASE', any=True)
kmi = km.keymap_items.new_modal('ADD_SNAP', 'A', 'PRESS')
kmi = km.keymap_items.new_modal('REMOVE_SNAP', 'A', 'PRESS', alt=True)
kmi = km.keymap_items.new_modal('PROPORTIONAL_SIZE_UP', 'PAGE_UP', 'PRESS')
kmi = km.keymap_items.new_modal('PROPORTIONAL_SIZE_DOWN', 'PAGE_DOWN', 'PRESS')
kmi = km.keymap_items.new_modal('PROPORTIONAL_SIZE_UP', 'WHEELDOWNMOUSE', 'PRESS')
kmi = km.keymap_items.new_modal('PROPORTIONAL_SIZE_DOWN', 'WHEELUPMOUSE', 'PRESS')
kmi = km.keymap_items.new_modal('PROPORTIONAL_SIZE', 'TRACKPADPAN', 'ANY')
kmi = km.keymap_items.new_modal('EDGESLIDE_EDGE_NEXT', 'WHEELDOWNMOUSE', 'PRESS', alt=True)
kmi = km.keymap_items.new_modal('EDGESLIDE_PREV_NEXT', 'WHEELUPMOUSE', 'PRESS', alt=True)
kmi = km.keymap_items.new_modal('AUTOIK_CHAIN_LEN_UP', 'PAGE_UP', 'PRESS', shift=True)
kmi = km.keymap_items.new_modal('AUTOIK_CHAIN_LEN_DOWN', 'PAGE_DOWN', 'PRESS', shift=True)
kmi = km.keymap_items.new_modal('AUTOIK_CHAIN_LEN_UP', 'WHEELDOWNMOUSE', 'PRESS', shift=True)
kmi = km.keymap_items.new_modal('AUTOIK_CHAIN_LEN_DOWN', 'WHEELUPMOUSE', 'PRESS', shift=True)

