# Configuration Maya
import bpy

wm = bpy.context.window_manager
kc = wm.keyconfigs.new('maya')

# Map 3D View
km = kc.keymaps.new('3D View', space_type='VIEW_3D', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('view3d.manipulator', 'LEFTMOUSE', 'PRESS', any=True)
kmi.properties.release_confirm = True
kmi = km.keymap_items.new('view3d.cursor3d', 'ACTIONMOUSE', 'PRESS')
kmi = km.keymap_items.new('view3d.rotate', 'LEFTMOUSE', 'PRESS', alt=True)
kmi = km.keymap_items.new('view3d.move', 'MIDDLEMOUSE', 'PRESS', alt=True)
kmi = km.keymap_items.new('view3d.zoom', 'RIGHTMOUSE', 'PRESS', alt=True)
kmi = km.keymap_items.new('view3d.view_selected', 'NUMPAD_PERIOD', 'PRESS')
kmi = km.keymap_items.new('view3d.view_center_cursor', 'NUMPAD_PERIOD', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('view3d.fly', 'F', 'PRESS', shift=True)
kmi = km.keymap_items.new('view3d.smoothview', 'TIMER1', 'ANY', any=True)
kmi = km.keymap_items.new('view3d.rotate', 'TRACKPADPAN', 'ANY', alt=True)
kmi = km.keymap_items.new('view3d.rotate', 'MOUSEROTATE', 'ANY')
kmi = km.keymap_items.new('view3d.move', 'TRACKPADPAN', 'ANY')
kmi = km.keymap_items.new('view3d.zoom', 'TRACKPADZOOM', 'ANY')
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
kmi = km.keymap_items.new('view3d.view_all', 'HOME', 'PRESS')
kmi.properties.center = False
kmi = km.keymap_items.new('view3d.view_all', 'C', 'PRESS', shift=True)
kmi.properties.center = True
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_0', 'PRESS')
kmi.properties.type = 'CAMERA'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_1', 'PRESS')
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
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_7', 'PRESS')
kmi.properties.type = 'TOP'
kmi = km.keymap_items.new('view3d.view_orbit', 'NUMPAD_8', 'PRESS')
kmi.properties.type = 'ORBITUP'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_1', 'PRESS', ctrl=True)
kmi.properties.type = 'BACK'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_3', 'PRESS', ctrl=True)
kmi.properties.type = 'LEFT'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_7', 'PRESS', ctrl=True)
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
kmi.properties.align_active = True
kmi.properties.type = 'FRONT'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_3', 'PRESS', shift=True)
kmi.properties.align_active = True
kmi.properties.type = 'RIGHT'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_7', 'PRESS', shift=True)
kmi.properties.align_active = True
kmi.properties.type = 'TOP'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_1', 'PRESS', shift=True, ctrl=True)
kmi.properties.align_active = True
kmi.properties.type = 'BACK'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_3', 'PRESS', shift=True, ctrl=True)
kmi.properties.align_active = True
kmi.properties.type = 'LEFT'
kmi = km.keymap_items.new('view3d.viewnumpad', 'NUMPAD_7', 'PRESS', shift=True, ctrl=True)
kmi.properties.align_active = True
kmi.properties.type = 'BOTTOM'
kmi = km.keymap_items.new('view3d.localview', 'NUMPAD_SLASH', 'PRESS')
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
kmi.properties.value_1 = 'TEXTURED'
kmi.properties.value_2 = 'SOLID'
kmi = km.keymap_items.new('view3d.select', 'SELECTMOUSE', 'PRESS')
kmi.properties.extend = False
kmi.properties.center = False
kmi.properties.object = False
kmi.properties.enumerate = False
kmi = km.keymap_items.new('view3d.select', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi.properties.center = False
kmi.properties.object = False
kmi.properties.enumerate = False
kmi = km.keymap_items.new('view3d.select', 'SELECTMOUSE', 'PRESS', ctrl=True)
kmi.properties.extend = False
kmi.properties.center = True
kmi.properties.object = False
kmi.properties.enumerate = False
kmi = km.keymap_items.new('view3d.select', 'SELECTMOUSE', 'PRESS', alt=True)
kmi.properties.extend = False
kmi.properties.center = False
kmi.properties.object = False
kmi.properties.enumerate = True
kmi = km.keymap_items.new('view3d.select', 'SELECTMOUSE', 'PRESS', shift=True, ctrl=True)
kmi.properties.extend = True
kmi.properties.center = True
kmi.properties.object = False
kmi.properties.enumerate = False
kmi = km.keymap_items.new('view3d.select', 'SELECTMOUSE', 'PRESS', ctrl=True, alt=True)
kmi.properties.extend = False
kmi.properties.center = True
kmi.properties.object = False
kmi.properties.enumerate = True
kmi = km.keymap_items.new('view3d.select', 'SELECTMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.extend = True
kmi.properties.center = False
kmi.properties.object = False
kmi.properties.enumerate = True
kmi = km.keymap_items.new('view3d.select', 'SELECTMOUSE', 'PRESS', shift=True, ctrl=True, alt=True)
kmi.properties.extend = True
kmi.properties.center = True
kmi.properties.object = False
kmi.properties.enumerate = True
kmi = km.keymap_items.new('view3d.select_border', 'EVT_TWEAK_S', 'ANY')
kmi.properties.extend = False
kmi = km.keymap_items.new('view3d.select_lasso', 'EVT_TWEAK_A', 'ANY', ctrl=True)
kmi = km.keymap_items.new('view3d.select_lasso', 'EVT_TWEAK_A', 'ANY', shift=True, ctrl=True)
kmi.properties.deselect = True
kmi = km.keymap_items.new('view3d.select_circle', 'C', 'PRESS')
kmi = km.keymap_items.new('view3d.clip_border', 'B', 'PRESS', alt=True)
kmi = km.keymap_items.new('view3d.zoom_border', 'B', 'PRESS', shift=True)
kmi = km.keymap_items.new('view3d.render_border', 'B', 'PRESS', shift=True)
kmi = km.keymap_items.new('view3d.camera_to_view', 'NUMPAD_0', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('view3d.object_as_camera', 'NUMPAD_0', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.call_menu', 'S', 'PRESS', shift=True, ctrl=True)
kmi.properties.name = 'VIEW3D_MT_snap'
kmi = km.keymap_items.new('wm.context_set_enum', 'COMMA', 'PRESS')
kmi.properties.data_path = 'space_data.pivot_point'
kmi.properties.value = 'BOUNDING_BOX_CENTER'
kmi = km.keymap_items.new('wm.context_set_enum', 'COMMA', 'PRESS', ctrl=True)
kmi.properties.data_path = 'space_data.pivot_point'
kmi.properties.value = 'MEDIAN_POINT'
kmi = km.keymap_items.new('wm.context_toggle', 'COMMA', 'PRESS', alt=True)
kmi.properties.data_path = 'space_data.use_pivot_point'
kmi = km.keymap_items.new('wm.context_toggle', 'Q', 'PRESS')
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
kmi = km.keymap_items.new('transform.translate', 'G', 'PRESS', shift=True)
kmi = km.keymap_items.new('transform.translate', 'EVT_TWEAK_S', 'ANY')
kmi = km.keymap_items.new('transform.rotate', 'R', 'PRESS', shift=True)
kmi = km.keymap_items.new('transform.resize', 'S', 'PRESS', shift=True)
kmi = km.keymap_items.new('transform.warp', 'W', 'PRESS', shift=True)
kmi = km.keymap_items.new('transform.tosphere', 'S', 'PRESS', shift=True, alt=True)
kmi = km.keymap_items.new('transform.shear', 'S', 'PRESS', shift=True, ctrl=True, alt=True)
kmi = km.keymap_items.new('transform.select_orientation', 'SPACE', 'PRESS', alt=True)
kmi = km.keymap_items.new('transform.create_orientation', 'SPACE', 'PRESS', ctrl=True, alt=True)
kmi.properties.use = True
kmi = km.keymap_items.new('transform.mirror', 'M', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.context_toggle', 'TAB', 'PRESS', shift=True)
kmi.properties.data_path = 'tool_settings.use_snap'
kmi = km.keymap_items.new('transform.snap_type', 'TAB', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('view3d.enable_manipulator', 'W', 'PRESS')
kmi.properties.translate = True
kmi = km.keymap_items.new('view3d.enable_manipulator', 'E', 'PRESS')
kmi.properties.rotate = True
kmi = km.keymap_items.new('view3d.enable_manipulator', 'R', 'PRESS')
kmi.properties.scale = True
kmi = km.keymap_items.new('view3d.select_border', 'EVT_TWEAK_S', 'ANY', shift=True)
kmi.properties.extend = True

# Map Object Mode
km = kc.keymaps.new('Object Mode', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('wm.context_cycle_enum', 'O', 'PRESS', shift=True)
kmi.properties.data_path = 'tool_settings.proportional_edit_falloff'
kmi = km.keymap_items.new('wm.context_toggle_enum', 'O', 'PRESS')
kmi.properties.data_path = 'tool_settings.proportional_edit'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'ENABLED'
kmi = km.keymap_items.new('view3d.game_start', 'P', 'PRESS')
kmi = km.keymap_items.new('object.select_all', 'A', 'PRESS')
kmi = km.keymap_items.new('object.select_all', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
kmi = km.keymap_items.new('object.select_linked', 'L', 'PRESS', shift=True)
kmi = km.keymap_items.new('object.select_grouped', 'G', 'PRESS', shift=True)
kmi = km.keymap_items.new('object.select_mirror', 'M', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('object.select_hierarchy', 'LEFT_BRACKET', 'PRESS')
kmi.properties.direction = 'PARENT'
kmi = km.keymap_items.new('object.select_hierarchy', 'LEFT_BRACKET', 'PRESS', shift=True)
kmi.properties.direction = 'PARENT'
kmi.properties.extend = True
kmi = km.keymap_items.new('object.select_hierarchy', 'RIGHT_BRACKET', 'PRESS')
kmi.properties.direction = 'CHILD'
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
kmi = km.keymap_items.new('object.hide_view_set', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.keymap_items.new('object.move_to_layer', 'M', 'PRESS')
kmi = km.keymap_items.new('object.delete', 'X', 'PRESS')
kmi = km.keymap_items.new('object.delete', 'DEL', 'PRESS')
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
kmi = km.keymap_items.new('object.convert', 'C', 'PRESS', alt=True)
kmi = km.keymap_items.new('object.proxy_make', 'P', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('object.make_local', 'L', 'PRESS')
kmi = km.keymap_items.new('anim.keyframe_insert_menu', 'I', 'PRESS')
kmi = km.keymap_items.new('anim.keyframe_delete_v3d', 'I', 'PRESS', alt=True)
kmi = km.keymap_items.new('anim.keying_set_active_set', 'I', 'PRESS', shift=True, ctrl=True, alt=True)
kmi = km.keymap_items.new('group.create', 'G', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('group.objects_remove', 'G', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('group.objects_add_active', 'G', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('group.objects_remove_active', 'G', 'PRESS', shift=True, alt=True)
kmi = km.keymap_items.new('wm.call_menu', 'W', 'PRESS', ctrl=True)
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
kmi = km.keymap_items.new('object.select_all', 'SELECTMOUSE', 'CLICK')
kmi.properties.action = 'DESELECT'

# Map Mesh
km = kc.keymaps.new('Mesh', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('mesh.loopcut_slide', 'R', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('mesh.loop_select', 'SELECTMOUSE', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('mesh.loop_select', 'SELECTMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('mesh.edgering_select', 'SELECTMOUSE', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('mesh.edgering_select', 'SELECTMOUSE', 'PRESS', shift=True, ctrl=True, alt=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('mesh.select_shortest_path', 'SELECTMOUSE', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('mesh.select_all', 'A', 'PRESS')
kmi = km.keymap_items.new('mesh.select_more', 'NUMPAD_PLUS', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('mesh.select_less', 'NUMPAD_MINUS', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('mesh.select_all', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
kmi = km.keymap_items.new('mesh.select_non_manifold', 'M', 'PRESS', shift=True, ctrl=True, alt=True)
kmi = km.keymap_items.new('mesh.select_linked', 'L', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('mesh.select_linked_pick', 'L', 'PRESS')
kmi = km.keymap_items.new('mesh.select_linked_pick', 'L', 'PRESS', shift=True)
kmi.properties.deselect = True
kmi = km.keymap_items.new('mesh.faces_select_linked_flat', 'F', 'PRESS', shift=True, ctrl=True, alt=True)
kmi.properties.sharpness = 135.0
kmi = km.keymap_items.new('mesh.select_similar', 'G', 'PRESS', shift=True)
kmi = km.keymap_items.new('wm.call_menu', 'TAB', 'PRESS', ctrl=True)
kmi.properties.name = 'VIEW3D_MT_edit_mesh_select_mode'
kmi = km.keymap_items.new('mesh.hide', 'H', 'PRESS')
kmi = km.keymap_items.new('mesh.hide', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.keymap_items.new('mesh.reveal', 'H', 'PRESS', alt=True)
kmi = km.keymap_items.new('mesh.normals_make_consistent', 'N', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('mesh.normals_make_consistent', 'N', 'PRESS', shift=True, ctrl=True)
kmi.properties.inside = True
kmi = km.keymap_items.new('view3d.edit_mesh_extrude_move_normal', 'E', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('view3d.edit_mesh_extrude_individual_move', 'E', 'PRESS', shift=True)
kmi = km.keymap_items.new('wm.call_menu', 'E', 'PRESS', alt=True)
kmi.properties.name = 'VIEW3D_MT_edit_mesh_extrude'
kmi = km.keymap_items.new('mesh.spin', 'R', 'PRESS', alt=True)
kmi = km.keymap_items.new('mesh.fill', 'F', 'PRESS', alt=True)
kmi = km.keymap_items.new('mesh.beautify_fill', 'F', 'PRESS', shift=True, alt=True)
kmi = km.keymap_items.new('mesh.quads_convert_to_tris', 'T', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('mesh.tris_convert_to_quads', 'J', 'PRESS', alt=True)
# kmi = km.keymap_items.new('mesh.edge_flip', 'F', 'PRESS', shift=True, ctrl=True) # removed since bmesh, can be made into something else?
kmi = km.keymap_items.new('mesh.rip_move', 'V', 'PRESS')
kmi = km.keymap_items.new('mesh.merge', 'M', 'PRESS', alt=True)
kmi = km.keymap_items.new('transform.shrink_fatten', 'S', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('mesh.edge_face_add', 'F', 'PRESS')
kmi = km.keymap_items.new('mesh.duplicate_move', 'D', 'PRESS', shift=True)
kmi = km.keymap_items.new('wm.call_menu', 'A', 'PRESS', shift=True)
kmi.properties.name = 'INFO_MT_mesh_add'
kmi = km.keymap_items.new('mesh.separate', 'P', 'PRESS')
kmi = km.keymap_items.new('mesh.split', 'Y', 'PRESS')
kmi = km.keymap_items.new('mesh.dupli_extrude_cursor', 'ACTIONMOUSE', 'CLICK', ctrl=True)
kmi = km.keymap_items.new('mesh.delete', 'X', 'PRESS')
kmi = km.keymap_items.new('mesh.delete', 'DEL', 'PRESS')
kmi = km.keymap_items.new('mesh.knifetool', 'LEFTMOUSE', 'PRESS', key_modifier='K')
# BMESH_TODO: midpoints for knife were moved to modal keymap
#kmi = km.keymap_items.new('mesh.knifetool', 'LEFTMOUSE', 'PRESS', shift=True, key_modifier='K')
#kmi.properties.type = 'MIDPOINTS'
kmi = km.keymap_items.new('object.vertex_parent_set', 'P', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.call_menu', 'W', 'PRESS', ctrl=True)
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
kmi = km.keymap_items.new('mesh.select_all', 'SELECTMOUSE', 'CLICK')
kmi.properties.action = 'DESELECT'
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

wm.keyconfigs.active = kc
