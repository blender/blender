# Configuration 3dsmax
import bpy

wm = bpy.context.window_manager
kc = wm.keyconfigs.new('3dsmax')

# Map Window
km = kc.keymaps.new('Window', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('wm.window_duplicate', 'W', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('wm.read_homefile', 'N', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.save_homefile', 'U', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.call_menu', 'O', 'PRESS', shift=True, ctrl=True)
kmi.properties.name = 'INFO_MT_file_open_recent'
kmi = km.keymap_items.new('wm.open_mainfile', 'O', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.open_mainfile', 'F1', 'PRESS')
kmi = km.keymap_items.new('wm.link_append', 'O', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('wm.link_append', 'F1', 'PRESS', shift=True)
kmi.properties.link = False
kmi.properties.instance_groups = False
kmi = km.keymap_items.new('wm.save_mainfile', 'S', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.save_mainfile', 'W', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.save_as_mainfile', 'S', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('wm.save_as_mainfile', 'F2', 'PRESS')
kmi = km.keymap_items.new('wm.save_as_mainfile', 'S', 'PRESS', ctrl=True, alt=True)
kmi.properties.copy = True
kmi = km.keymap_items.new('wm.window_fullscreen_toggle', 'F11', 'PRESS', alt=True)
kmi = km.keymap_items.new('wm.quit_blender', 'Q', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.redraw_timer', 'T', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('wm.debug_menu', 'D', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('wm.search_menu', 'SPACE', 'PRESS')
kmi = km.keymap_items.new('wm.call_menu', 'NDOF_BUTTON_MENU', 'PRESS')
kmi.properties.name = 'USERPREF_MT_ndof_settings'
kmi = km.keymap_items.new('wm.context_set_enum', 'F2', 'PRESS', shift=True)
kmi.properties.data_path = 'area.type'
kmi.properties.value = 'LOGIC_EDITOR'
kmi = km.keymap_items.new('wm.context_set_enum', 'F3', 'PRESS', shift=True, ctrl=True)
kmi.properties.data_path = 'area.type'
kmi.properties.value = 'NODE_EDITOR'
kmi = km.keymap_items.new('wm.context_set_enum', 'F4', 'PRESS', shift=True)
kmi.properties.data_path = 'area.type'
kmi.properties.value = 'CONSOLE'
kmi = km.keymap_items.new('wm.context_set_enum', 'F5', 'PRESS', shift=True)
kmi.properties.data_path = 'area.type'
kmi.properties.value = 'VIEW_3D'
kmi = km.keymap_items.new('wm.context_set_enum', 'F6', 'PRESS', shift=True)
kmi.properties.data_path = 'area.type'
kmi.properties.value = 'GRAPH_EDITOR'
kmi = km.keymap_items.new('wm.context_set_enum', 'F7', 'PRESS', shift=True)
kmi.properties.data_path = 'area.type'
kmi.properties.value = 'PROPERTIES'
kmi = km.keymap_items.new('wm.context_set_enum', 'F8', 'PRESS', shift=True)
kmi.properties.data_path = 'area.type'
kmi.properties.value = 'SEQUENCE_EDITOR'
kmi = km.keymap_items.new('wm.context_set_enum', 'F9', 'PRESS', shift=True)
kmi.properties.data_path = 'area.type'
kmi.properties.value = 'OUTLINER'
kmi = km.keymap_items.new('wm.context_set_enum', 'F10', 'PRESS', shift=True, ctrl=True)
kmi.properties.data_path = 'area.type'
kmi.properties.value = 'IMAGE_EDITOR'
kmi = km.keymap_items.new('wm.context_set_enum', 'F11', 'PRESS', shift=True)
kmi.properties.data_path = 'area.type'
kmi.properties.value = 'TEXT_EDITOR'
kmi = km.keymap_items.new('wm.context_set_enum', 'F12', 'PRESS', shift=True)
kmi.properties.data_path = 'area.type'
kmi.properties.value = 'DOPESHEET_EDITOR'
kmi = km.keymap_items.new('wm.context_scale_float', 'NDOF_BUTTON_PLUS', 'PRESS')
kmi.properties.data_path = 'user_preferences.inputs.ndof_sensitivity'
kmi.properties.value = 1.1
kmi = km.keymap_items.new('wm.context_scale_float', 'NDOF_BUTTON_MINUS', 'PRESS')
kmi.properties.data_path = 'user_preferences.inputs.ndof_sensitivity'
kmi.properties.value = 1.0 / 1.1
kmi = km.keymap_items.new('wm.context_scale_float', 'NDOF_BUTTON_PLUS', 'PRESS', shift=True)
kmi.properties.data_path = 'user_preferences.inputs.ndof_sensitivity'
kmi.properties.value = 1.5
kmi = km.keymap_items.new('wm.context_scale_float', 'NDOF_BUTTON_MINUS', 'PRESS', shift=True)
kmi.properties.data_path = 'user_preferences.inputs.ndof_sensitivity'
kmi.properties.value = 1.0 / 1.5
kmi = km.keymap_items.new('info.reports_display_update', 'TIMER_REPORT', 'ANY', any=True)

# Map Screen
km = kc.keymaps.new('Screen', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('screen.animation_step', 'TIMER0', 'ANY', any=True)
kmi = km.keymap_items.new('screen.region_blend', 'TIMERREGION', 'ANY', any=True)
kmi = km.keymap_items.new('screen.screen_set', 'RIGHT_ARROW', 'PRESS', ctrl=True)
kmi.properties.delta = 1
kmi = km.keymap_items.new('screen.screen_set', 'LEFT_ARROW', 'PRESS', ctrl=True)
kmi.properties.delta = -1
kmi = km.keymap_items.new('screen.screen_full_area', 'UP_ARROW', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('screen.screen_full_area', 'DOWN_ARROW', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('screen.screen_full_area', 'SPACE', 'PRESS', shift=True)
kmi = km.keymap_items.new('screen.screenshot', 'F3', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('screen.screencast', 'F3', 'PRESS', alt=True)
kmi = km.keymap_items.new('screen.region_quadview', 'W', 'PRESS', alt=True)
kmi = km.keymap_items.new('screen.repeat_history', 'F3', 'PRESS', shift=True)
kmi = km.keymap_items.new('screen.repeat_last', 'R', 'PRESS', shift=True)
kmi = km.keymap_items.new('screen.region_flip', 'F5', 'PRESS')
kmi = km.keymap_items.new('screen.redo_last', 'F6', 'PRESS')
kmi = km.keymap_items.new('script.reload', 'F8', 'PRESS')
kmi = km.keymap_items.new('file.execute', 'RET', 'PRESS')
kmi = km.keymap_items.new('file.execute', 'NUMPAD_ENTER', 'PRESS')
kmi = km.keymap_items.new('file.cancel', 'ESC', 'PRESS')
kmi = km.keymap_items.new('ed.undo', 'Z', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('ed.redo', 'Z', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('ed.undo_history', 'Z', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('render.render', 'F10', 'PRESS')
kmi = km.keymap_items.new('render.render', 'F10', 'PRESS', ctrl=True)
kmi.properties.animation = True
kmi = km.keymap_items.new('render.view_cancel', 'ESC', 'PRESS')
kmi = km.keymap_items.new('render.view_show', 'F11', 'PRESS')
kmi = km.keymap_items.new('render.play_rendered_anim', 'F11', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('screen.userpref_show', 'U', 'PRESS', ctrl=True, alt=True)

# Map Markers
km = kc.keymaps.new('Markers', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('marker.add', 'M', 'PRESS')
kmi = km.keymap_items.new('marker.move', 'EVT_TWEAK_S', 'ANY')
kmi = km.keymap_items.new('marker.duplicate', 'D', 'PRESS', shift=True)
kmi = km.keymap_items.new('marker.select', 'SELECTMOUSE', 'PRESS')
kmi.properties.extend = False
kmi = km.keymap_items.new('marker.select', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('marker.select', 'SELECTMOUSE', 'PRESS', ctrl=True)
kmi.properties.extend = False
kmi.properties.camera = True
kmi = km.keymap_items.new('marker.select', 'SELECTMOUSE', 'PRESS', shift=True, ctrl=True)
kmi.properties.extend = True
kmi.properties.camera = True
kmi = km.keymap_items.new('marker.select_border', 'B', 'PRESS')
kmi = km.keymap_items.new('marker.select_all', 'A', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('marker.delete', 'X', 'PRESS')
kmi = km.keymap_items.new('marker.rename', 'M', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('marker.move', 'G', 'PRESS')
kmi = km.keymap_items.new('marker.camera_bind', 'B', 'PRESS', ctrl=True)

# Map Outliner
km = kc.keymaps.new('Outliner', space_type='OUTLINER', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('outliner.item_rename', 'LEFTMOUSE', 'DOUBLE_CLICK')
kmi = km.keymap_items.new('outliner.item_activate', 'LEFTMOUSE', 'CLICK')
kmi.properties.extend = False
kmi.properties.recursive = False
kmi = km.keymap_items.new('outliner.item_activate', 'LEFTMOUSE', 'CLICK', shift=True)
kmi.properties.extend = True
kmi.properties.recursive = False
kmi = km.keymap_items.new('outliner.item_activate', 'LEFTMOUSE', 'CLICK', ctrl=True)
kmi.properties.extend = False
kmi.properties.recursive = True
kmi = km.keymap_items.new('outliner.item_activate', 'LEFTMOUSE', 'CLICK', shift=True, ctrl=True)
kmi.properties.extend = True
kmi.properties.recursive = True
kmi = km.keymap_items.new('outliner.select_border', 'B', 'PRESS')
kmi = km.keymap_items.new('outliner.item_openclose', 'RET', 'PRESS')
kmi.properties.all = False
kmi = km.keymap_items.new('outliner.item_openclose', 'RET', 'PRESS', shift=True)
kmi.properties.all = True
kmi = km.keymap_items.new('outliner.item_rename', 'LEFTMOUSE', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('outliner.operation', 'RIGHTMOUSE', 'PRESS')
kmi = km.keymap_items.new('outliner.show_hierarchy', 'HOME', 'PRESS')
kmi = km.keymap_items.new('outliner.show_active', 'PERIOD', 'PRESS')
kmi = km.keymap_items.new('outliner.show_active', 'NUMPAD_PERIOD', 'PRESS')
kmi = km.keymap_items.new('outliner.scroll_page', 'PAGE_DOWN', 'PRESS')
kmi = km.keymap_items.new('outliner.scroll_page', 'PAGE_UP', 'PRESS')
kmi.properties.up = True
kmi = km.keymap_items.new('outliner.show_one_level', 'NUMPAD_PLUS', 'PRESS')
kmi = km.keymap_items.new('outliner.show_one_level', 'NUMPAD_MINUS', 'PRESS')
kmi.properties.open = False
kmi = km.keymap_items.new('outliner.selected_toggle', 'A', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('outliner.expanded_toggle', 'A', 'PRESS', shift=True)
kmi = km.keymap_items.new('outliner.renderability_toggle', 'R', 'PRESS')
kmi = km.keymap_items.new('outliner.selectability_toggle', 'S', 'PRESS')
kmi = km.keymap_items.new('outliner.visibility_toggle', 'V', 'PRESS')
kmi = km.keymap_items.new('outliner.keyingset_add_selected', 'K', 'PRESS')
kmi = km.keymap_items.new('outliner.keyingset_remove_selected', 'K', 'PRESS', alt=True)
kmi = km.keymap_items.new('anim.keyframe_insert', 'I', 'PRESS')
kmi = km.keymap_items.new('anim.keyframe_delete', 'I', 'PRESS', alt=True)
kmi = km.keymap_items.new('outliner.drivers_add_selected', 'D', 'PRESS')
kmi = km.keymap_items.new('outliner.drivers_delete_selected', 'D', 'PRESS', alt=True)

# Map 3D View Generic
km = kc.keymaps.new('3D View Generic', space_type='VIEW_3D', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('view3d.properties', 'F12', 'PRESS')
kmi = km.keymap_items.new('view3d.toolshelf', 'D', 'PRESS')

# Map Face Mask
km = kc.keymaps.new('Face Mask', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('paint.face_select_all', 'A', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('paint.face_select_all', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
kmi = km.keymap_items.new('paint.face_select_hide', 'H', 'PRESS')
kmi.properties.unselected = False
kmi = km.keymap_items.new('paint.face_select_hide', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.keymap_items.new('paint.face_select_reveal', 'H', 'PRESS', alt=True)
kmi = km.keymap_items.new('paint.face_select_linked', 'L', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('paint.face_select_linked_pick', 'L', 'PRESS')

# Map Weight Paint Vertex Selection
km = kc.keymaps.new('Weight Paint Vertex Selection', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('paint.vert_select_all', 'A', 'PRESS', ctrl=True)
kmi.properties.action = 'SELECT'
kmi = km.keymap_items.new('paint.vert_select_all', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
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
kmi = km.keymap_items.new('wm.call_menu', 'A', 'PRESS', shift=True, ctrl=True)
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
kmi = km.keymap_items.new('pose.select_mirror', 'F', 'PRESS', shift=True)
kmi.properties.only_active = True
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
kmi = km.keymap_items.new('armature.armature_layers', 'M', 'PRESS', shift=True)
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
kmi = km.keymap_items.new('view3d.game_start', 'P', 'PRESS', shift=True, ctrl=True, alt=True)
kmi = km.keymap_items.new('object.select_all', 'A', 'PRESS', ctrl=True)
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
kmi = km.keymap_items.new('wm.call_menu', 'A', 'PRESS', shift=True, ctrl=True)
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

# Map Particle
km = kc.keymaps.new('Particle', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('particle.select_all', 'A', 'PRESS', ctrl=True)
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
kmi = km.keymap_items.new('view3d.view_selected', 'Z', 'PRESS')
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
kmi = km.keymap_items.new('view3d.view_persportho', 'P', 'PRESS')
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
kmi = km.keymap_items.new('wm.context_toggle_enum', 'F3', 'PRESS')
kmi.properties.data_path = 'space_data.viewport_shade'
kmi.properties.value_1 = 'SOLID'
kmi.properties.value_2 = 'WIREFRAME'
kmi = km.keymap_items.new('wm.context_toggle_enum', 'Z', 'PRESS', alt=True)
kmi.properties.data_path = 'space_data.viewport_shade'
kmi.properties.value_1 = 'SOLID'
kmi.properties.value_2 = 'TEXTURED'
kmi = km.keymap_items.new('view3d.select_or_deselect_all', 'SELECTMOUSE', 'CLICK')
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = False
kmi.properties.center = False
kmi.properties.enumerate = False
kmi.properties.object = False
kmi = km.keymap_items.new('view3d.select_or_deselect_all', 'SELECTMOUSE', 'CLICK', ctrl=True)
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = True
kmi.properties.center = False
kmi.properties.enumerate = False
kmi.properties.object = False
kmi = km.keymap_items.new('view3d.select_or_deselect_all', 'SELECTMOUSE', 'CLICK', shift=True)
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = False
kmi.properties.center = True
kmi.properties.enumerate = False
kmi.properties.object = True
kmi = km.keymap_items.new('view3d.select_or_deselect_all', 'SELECTMOUSE', 'CLICK', alt=True)
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = False
kmi.properties.center = False
kmi.properties.enumerate = True
kmi.properties.object = False
kmi = km.keymap_items.new('view3d.select_or_deselect_all', 'SELECTMOUSE', 'CLICK', shift=True, ctrl=True)
kmi.properties.extend = True
kmi.properties.deselect = False
kmi.properties.toggle = True
kmi.properties.center = True
kmi.properties.enumerate = False
kmi.properties.object = False
kmi = km.keymap_items.new('view3d.select_or_deselect_all', 'SELECTMOUSE', 'CLICK', ctrl=True, alt=True)
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = False
kmi.properties.center = True
kmi.properties.enumerate = True
kmi.properties.object = False
kmi = km.keymap_items.new('view3d.select_or_deselect_all', 'SELECTMOUSE', 'CLICK', shift=True, alt=True)
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = True
kmi.properties.center = False
kmi.properties.enumerate = True
kmi.properties.object = False
kmi = km.keymap_items.new('view3d.select_or_deselect_all', 'SELECTMOUSE', 'CLICK', shift=True, ctrl=True, alt=True)
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
kmi = km.keymap_items.new('transform.bend', 'Q', 'PRESS', shift=True)
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
kmi = km.keymap_items.new('transform.skin_resize', 'A', 'PRESS', shift=True, ctrl=True)
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

# Map Animation Channels
km = kc.keymaps.new('Animation Channels', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('anim.channels_click', 'LEFTMOUSE', 'PRESS')
kmi.properties.extend = False
kmi = km.keymap_items.new('anim.channels_click', 'LEFTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('anim.channels_click', 'LEFTMOUSE', 'PRESS', shift=True, ctrl=True)
kmi.properties.children_only = True
kmi = km.keymap_items.new('anim.channels_rename', 'LEFTMOUSE', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('anim.channels_select_all_toggle', 'A', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('anim.channels_select_all_toggle', 'I', 'PRESS', ctrl=True)
kmi.properties.invert = True
kmi = km.keymap_items.new('anim.channels_select_border', 'B', 'PRESS')
kmi = km.keymap_items.new('anim.channels_select_border', 'EVT_TWEAK_L', 'ANY')
kmi = km.keymap_items.new('anim.channels_delete', 'X', 'PRESS')
kmi = km.keymap_items.new('anim.channels_delete', 'DEL', 'PRESS')
kmi = km.keymap_items.new('anim.channels_setting_toggle', 'W', 'PRESS', shift=True)
kmi = km.keymap_items.new('anim.channels_setting_enable', 'W', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('anim.channels_setting_disable', 'W', 'PRESS', alt=True)
kmi = km.keymap_items.new('anim.channels_editable_toggle', 'TAB', 'PRESS')
kmi = km.keymap_items.new('anim.channels_expand', 'NUMPAD_PLUS', 'PRESS')
kmi = km.keymap_items.new('anim.channels_collapse', 'NUMPAD_MINUS', 'PRESS')
kmi = km.keymap_items.new('anim.channels_expand', 'NUMPAD_PLUS', 'PRESS', ctrl=True)
kmi.properties.all = False
kmi = km.keymap_items.new('anim.channels_collapse', 'NUMPAD_MINUS', 'PRESS', ctrl=True)
kmi.properties.all = False
kmi = km.keymap_items.new('anim.channels_move', 'PAGE_UP', 'PRESS')
kmi.properties.direction = 'UP'
kmi = km.keymap_items.new('anim.channels_move', 'PAGE_DOWN', 'PRESS')
kmi.properties.direction = 'DOWN'
kmi = km.keymap_items.new('anim.channels_move', 'PAGE_UP', 'PRESS', shift=True)
kmi.properties.direction = 'TOP'
kmi = km.keymap_items.new('anim.channels_move', 'PAGE_DOWN', 'PRESS', shift=True)
kmi.properties.direction = 'BOTTOM'
kmi = km.keymap_items.new('anim.channels_group', 'G', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('anim.channels_ungroup', 'G', 'PRESS', alt=True)
kmi = km.keymap_items.new('anim.channels_visibility_set', 'V', 'PRESS')
kmi = km.keymap_items.new('anim.channels_visibility_toggle', 'V', 'PRESS', shift=True)

# Map UV Editor
km = kc.keymaps.new('UV Editor', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('wm.context_toggle', 'Q', 'PRESS')
kmi.properties.data_path = 'tool_settings.use_uv_sculpt'
kmi = km.keymap_items.new('uv.mark_seam', 'E', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('uv.select', 'SELECTMOUSE', 'PRESS')
kmi.properties.extend = False
kmi = km.keymap_items.new('uv.select', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('uv.select_loop', 'SELECTMOUSE', 'PRESS', alt=True)
kmi.properties.extend = False
kmi = km.keymap_items.new('uv.select_loop', 'SELECTMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('uv.select_split', 'Y', 'PRESS')
kmi = km.keymap_items.new('uv.select_border', 'B', 'PRESS')
kmi.properties.pinned = False
kmi = km.keymap_items.new('uv.select_border', 'B', 'PRESS', shift=True)
kmi.properties.pinned = True
kmi = km.keymap_items.new('uv.circle_select', 'C', 'PRESS')
kmi = km.keymap_items.new('uv.select_lasso', 'EVT_TWEAK_A', 'ANY', ctrl=True)
kmi.properties.deselect = False
kmi = km.keymap_items.new('uv.select_lasso', 'EVT_TWEAK_A', 'ANY', shift=True, ctrl=True)
kmi.properties.deselect = True
kmi = km.keymap_items.new('uv.select_linked', 'L', 'PRESS', ctrl=True)
kmi.properties.extend = False
kmi = km.keymap_items.new('uv.select_linked_pick', 'L', 'PRESS')
kmi.properties.extend = False
kmi = km.keymap_items.new('uv.select_linked', 'L', 'PRESS', shift=True, ctrl=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('uv.select_linked_pick', 'L', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('uv.select_all', 'A', 'PRESS', ctrl=True)
kmi.properties.action = 'TOGGLE'
kmi = km.keymap_items.new('uv.select_all', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
kmi = km.keymap_items.new('uv.select_pinned', 'P', 'PRESS', shift=True)
kmi = km.keymap_items.new('wm.call_menu', 'W', 'PRESS')
kmi.properties.name = 'IMAGE_MT_uvs_weldalign'
kmi = km.keymap_items.new('uv.stitch', 'V', 'PRESS')
kmi = km.keymap_items.new('uv.pin', 'P', 'PRESS')
kmi.properties.clear = False
kmi = km.keymap_items.new('uv.pin', 'P', 'PRESS', alt=True)
kmi.properties.clear = True
kmi = km.keymap_items.new('uv.unwrap', 'E', 'PRESS')
kmi = km.keymap_items.new('uv.minimize_stretch', 'V', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('uv.pack_islands', 'P', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('uv.average_islands_scale', 'A', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('uv.hide', 'H', 'PRESS')
kmi.properties.unselected = False
kmi = km.keymap_items.new('uv.hide', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.keymap_items.new('uv.reveal', 'H', 'PRESS', alt=True)
kmi = km.keymap_items.new('uv.cursor_set', 'ACTIONMOUSE', 'PRESS')
kmi = km.keymap_items.new('uv.tile_set', 'ACTIONMOUSE', 'PRESS', shift=True)
kmi = km.keymap_items.new('wm.call_menu', 'S', 'PRESS', shift=True)
kmi.properties.name = 'IMAGE_MT_uvs_snap'
kmi = km.keymap_items.new('wm.call_menu', 'TAB', 'PRESS', ctrl=True)
kmi.properties.name = 'IMAGE_MT_uvs_select_mode'
kmi = km.keymap_items.new('wm.context_cycle_enum', 'O', 'PRESS', shift=True)
kmi.properties.data_path = 'tool_settings.proportional_edit_falloff'
kmi = km.keymap_items.new('wm.context_toggle_enum', 'O', 'PRESS')
kmi.properties.data_path = 'tool_settings.proportional_edit'
kmi.properties.value_1 = 'DISABLED'
kmi.properties.value_2 = 'ENABLED'
kmi = km.keymap_items.new('transform.translate', 'G', 'PRESS')
kmi = km.keymap_items.new('transform.translate', 'EVT_TWEAK_S', 'ANY')
kmi = km.keymap_items.new('transform.rotate', 'R', 'PRESS')
kmi = km.keymap_items.new('transform.resize', 'S', 'PRESS')
kmi = km.keymap_items.new('transform.shear', 'S', 'PRESS', shift=True, ctrl=True, alt=True)
kmi = km.keymap_items.new('transform.mirror', 'M', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.context_toggle', 'TAB', 'PRESS', shift=True)
kmi.properties.data_path = 'tool_settings.use_snap'
kmi = km.keymap_items.new('wm.context_menu_enum', 'TAB', 'PRESS', shift=True, ctrl=True)
kmi.properties.data_path = 'tool_settings.snap_uv_element'

# Map Mask Editing
km = kc.keymaps.new('Mask Editing', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('mask.new', 'N', 'PRESS', alt=True)
kmi = km.keymap_items.new('wm.context_cycle_enum', 'O', 'PRESS', shift=True)
kmi.properties.data_path = 'tool_settings.proportional_edit_falloff'
kmi = km.keymap_items.new('wm.context_toggle', 'O', 'PRESS')
kmi.properties.data_path = 'tool_settings.use_proportional_edit_mask'
kmi = km.keymap_items.new('mask.add_vertex_slide', 'ACTIONMOUSE', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('mask.add_feather_vertex_slide', 'ACTIONMOUSE', 'PRESS', shift=True)
kmi = km.keymap_items.new('mask.delete', 'X', 'PRESS')
kmi = km.keymap_items.new('mask.delete', 'DEL', 'PRESS')
kmi = km.keymap_items.new('mask.select', 'SELECTMOUSE', 'PRESS')
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = False
kmi = km.keymap_items.new('mask.select', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = False
kmi.properties.deselect = False
kmi.properties.toggle = True
kmi = km.keymap_items.new('mask.select_all', 'A', 'PRESS', ctrl=True)
kmi.properties.action = 'TOGGLE'
kmi = km.keymap_items.new('mask.select_all', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
kmi = km.keymap_items.new('mask.select_linked', 'L', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('mask.select_linked_pick', 'L', 'PRESS')
kmi.properties.deselect = False
kmi = km.keymap_items.new('mask.select_linked_pick', 'L', 'PRESS', shift=True)
kmi.properties.deselect = True
kmi = km.keymap_items.new('mask.select_border', 'B', 'PRESS')
kmi = km.keymap_items.new('mask.select_circle', 'C', 'PRESS')
kmi = km.keymap_items.new('mask.select_lasso', 'EVT_TWEAK_A', 'ANY', ctrl=True, alt=True)
kmi.properties.deselect = False
kmi = km.keymap_items.new('mask.select_lasso', 'EVT_TWEAK_A', 'ANY', shift=True, ctrl=True, alt=True)
kmi.properties.deselect = True
kmi = km.keymap_items.new('mask.hide_view_clear', 'H', 'PRESS', alt=True)
kmi = km.keymap_items.new('mask.hide_view_set', 'H', 'PRESS')
kmi.properties.unselected = False
kmi = km.keymap_items.new('mask.hide_view_set', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.keymap_items.new('clip.select', 'SELECTMOUSE', 'PRESS', ctrl=True)
kmi.properties.extend = False
kmi = km.keymap_items.new('mask.cyclic_toggle', 'C', 'PRESS', alt=True)
kmi = km.keymap_items.new('mask.slide_point', 'ACTIONMOUSE', 'PRESS')
kmi = km.keymap_items.new('mask.handle_type_set', 'V', 'PRESS')
kmi = km.keymap_items.new('mask.normals_make_consistent', 'N', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('mask.parent_set', 'P', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('mask.parent_clear', 'P', 'PRESS', alt=True)
kmi = km.keymap_items.new('mask.shape_key_insert', 'I', 'PRESS')
kmi = km.keymap_items.new('mask.shape_key_clear', 'I', 'PRESS', alt=True)
kmi = km.keymap_items.new('uv.cursor_set', 'ACTIONMOUSE', 'PRESS')
kmi = km.keymap_items.new('transform.translate', 'G', 'PRESS')
kmi = km.keymap_items.new('transform.translate', 'EVT_TWEAK_S', 'ANY')
kmi = km.keymap_items.new('transform.resize', 'S', 'PRESS')
kmi = km.keymap_items.new('transform.rotate', 'R', 'PRESS')
kmi = km.keymap_items.new('transform.transform', 'S', 'PRESS', alt=True)
kmi.properties.mode = 'MASK_SHRINKFATTEN'

# Map Graph Editor
km = kc.keymaps.new('Graph Editor', space_type='GRAPH_EDITOR', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('wm.context_toggle', 'H', 'PRESS', ctrl=True)
kmi.properties.data_path = 'space_data.show_handles'
kmi = km.keymap_items.new('graph.cursor_set', 'ACTIONMOUSE', 'PRESS')
kmi = km.keymap_items.new('graph.clickselect', 'SELECTMOUSE', 'PRESS')
kmi.properties.extend = False
kmi.properties.column = False
kmi.properties.curves = False
kmi = km.keymap_items.new('graph.clickselect', 'SELECTMOUSE', 'PRESS', alt=True)
kmi.properties.extend = False
kmi.properties.column = True
kmi.properties.curves = False
kmi = km.keymap_items.new('graph.clickselect', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi.properties.column = False
kmi.properties.curves = False
kmi = km.keymap_items.new('graph.clickselect', 'SELECTMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.extend = True
kmi.properties.column = True
kmi.properties.curves = False
kmi = km.keymap_items.new('graph.clickselect', 'SELECTMOUSE', 'PRESS', ctrl=True, alt=True)
kmi.properties.extend = False
kmi.properties.column = False
kmi.properties.curves = True
kmi = km.keymap_items.new('graph.clickselect', 'SELECTMOUSE', 'PRESS', shift=True, ctrl=True, alt=True)
kmi.properties.extend = True
kmi.properties.column = False
kmi.properties.curves = True
kmi = km.keymap_items.new('graph.select_leftright', 'SELECTMOUSE', 'PRESS', ctrl=True)
kmi.properties.mode = 'CHECK'
kmi.properties.extend = False
kmi = km.keymap_items.new('graph.select_leftright', 'SELECTMOUSE', 'PRESS', shift=True, ctrl=True)
kmi.properties.mode = 'CHECK'
kmi.properties.extend = True
kmi = km.keymap_items.new('graph.select_leftright', 'LEFT_BRACKET', 'PRESS')
kmi.properties.mode = 'LEFT'
kmi.properties.extend = False
kmi = km.keymap_items.new('graph.select_leftright', 'RIGHT_BRACKET', 'PRESS')
kmi.properties.mode = 'RIGHT'
kmi.properties.extend = False
kmi = km.keymap_items.new('graph.select_all_toggle', 'A', 'PRESS', ctrl=True)
kmi.properties.invert = False
kmi = km.keymap_items.new('graph.select_all_toggle', 'I', 'PRESS', ctrl=True)
kmi.properties.invert = True
kmi = km.keymap_items.new('graph.select_border', 'B', 'PRESS')
kmi.properties.axis_range = False
kmi.properties.include_handles = False
kmi = km.keymap_items.new('graph.select_border', 'B', 'PRESS', alt=True)
kmi.properties.axis_range = True
kmi.properties.include_handles = False
kmi = km.keymap_items.new('graph.select_border', 'B', 'PRESS', ctrl=True)
kmi.properties.axis_range = False
kmi.properties.include_handles = True
kmi = km.keymap_items.new('graph.select_border', 'B', 'PRESS', ctrl=True, alt=True)
kmi.properties.axis_range = True
kmi.properties.include_handles = True
kmi = km.keymap_items.new('graph.select_column', 'K', 'PRESS')
kmi.properties.mode = 'KEYS'
kmi = km.keymap_items.new('graph.select_column', 'K', 'PRESS', ctrl=True)
kmi.properties.mode = 'CFRA'
kmi = km.keymap_items.new('graph.select_column', 'K', 'PRESS', shift=True)
kmi.properties.mode = 'MARKERS_COLUMN'
kmi = km.keymap_items.new('graph.select_column', 'K', 'PRESS', alt=True)
kmi.properties.mode = 'MARKERS_BETWEEN'
kmi = km.keymap_items.new('graph.select_more', 'NUMPAD_PLUS', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('graph.select_less', 'NUMPAD_MINUS', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('graph.select_linked', 'L', 'PRESS')
kmi = km.keymap_items.new('graph.frame_jump', 'G', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('graph.snap', 'S', 'PRESS', shift=True)
kmi = km.keymap_items.new('graph.mirror', 'M', 'PRESS', shift=True)
kmi = km.keymap_items.new('graph.handle_type', 'V', 'PRESS')
kmi = km.keymap_items.new('graph.interpolation_type', 'T', 'PRESS')
kmi = km.keymap_items.new('graph.clean', 'O', 'PRESS')
kmi = km.keymap_items.new('graph.smooth', 'O', 'PRESS', alt=True)
kmi = km.keymap_items.new('graph.sample', 'O', 'PRESS', shift=True)
kmi = km.keymap_items.new('graph.bake', 'C', 'PRESS', alt=True)
kmi = km.keymap_items.new('graph.delete', 'X', 'PRESS')
kmi = km.keymap_items.new('graph.delete', 'DEL', 'PRESS')
kmi = km.keymap_items.new('graph.duplicate_move', 'D', 'PRESS', shift=True)
kmi = km.keymap_items.new('graph.keyframe_insert', 'I', 'PRESS')
kmi = km.keymap_items.new('graph.click_insert', 'ACTIONMOUSE', 'CLICK', ctrl=True)
kmi = km.keymap_items.new('graph.copy', 'C', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('graph.paste', 'V', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('graph.previewrange_set', 'P', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('graph.view_all', 'HOME', 'PRESS')
kmi = km.keymap_items.new('graph.view_selected', 'NUMPAD_PERIOD', 'PRESS')
kmi = km.keymap_items.new('graph.fmodifier_add', 'M', 'PRESS', shift=True, ctrl=True)
kmi.properties.only_active = False
kmi = km.keymap_items.new('anim.channels_editable_toggle', 'TAB', 'PRESS')
kmi = km.keymap_items.new('transform.translate', 'G', 'PRESS')
kmi = km.keymap_items.new('transform.translate', 'EVT_TWEAK_S', 'ANY')
kmi = km.keymap_items.new('transform.transform', 'E', 'PRESS')
kmi.properties.mode = 'TIME_EXTEND'
kmi = km.keymap_items.new('transform.rotate', 'R', 'PRESS')
kmi = km.keymap_items.new('transform.resize', 'S', 'PRESS')
kmi = km.keymap_items.new('marker.add', 'M', 'PRESS')
kmi = km.keymap_items.new('marker.rename', 'M', 'PRESS', ctrl=True)

# Map Node Editor
km = kc.keymaps.new('Node Editor', space_type='NODE_EDITOR', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('node.select', 'ACTIONMOUSE', 'PRESS')
kmi.properties.extend = False
kmi = km.keymap_items.new('node.select', 'SELECTMOUSE', 'PRESS')
kmi.properties.extend = False
kmi = km.keymap_items.new('node.select', 'ACTIONMOUSE', 'PRESS', ctrl=True)
kmi.properties.extend = False
kmi = km.keymap_items.new('node.select', 'SELECTMOUSE', 'PRESS', ctrl=True)
kmi.properties.extend = False
kmi = km.keymap_items.new('node.select', 'ACTIONMOUSE', 'PRESS', alt=True)
kmi.properties.extend = False
kmi = km.keymap_items.new('node.select', 'SELECTMOUSE', 'PRESS', alt=True)
kmi.properties.extend = False
kmi = km.keymap_items.new('node.select', 'ACTIONMOUSE', 'PRESS', ctrl=True, alt=True)
kmi.properties.extend = False
kmi = km.keymap_items.new('node.select', 'SELECTMOUSE', 'PRESS', ctrl=True, alt=True)
kmi.properties.extend = False
kmi = km.keymap_items.new('node.select', 'ACTIONMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('node.select', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('node.select', 'ACTIONMOUSE', 'PRESS', shift=True, ctrl=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('node.select', 'SELECTMOUSE', 'PRESS', shift=True, ctrl=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('node.select', 'ACTIONMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('node.select', 'SELECTMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('node.select', 'ACTIONMOUSE', 'PRESS', shift=True, ctrl=True, alt=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('node.select', 'SELECTMOUSE', 'PRESS', shift=True, ctrl=True, alt=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('node.select_border', 'EVT_TWEAK_S', 'ANY')
kmi.properties.tweak = True
kmi = km.keymap_items.new('node.select_lasso', 'EVT_TWEAK_A', 'ANY', ctrl=True, alt=True)
kmi.properties.deselect = False
kmi = km.keymap_items.new('node.select_lasso', 'EVT_TWEAK_A', 'ANY', shift=True, ctrl=True, alt=True)
kmi.properties.deselect = True
kmi = km.keymap_items.new('node.link', 'LEFTMOUSE', 'PRESS')
kmi.properties.detach = False
kmi = km.keymap_items.new('node.link', 'LEFTMOUSE', 'PRESS', ctrl=True)
kmi.properties.detach = True
kmi = km.keymap_items.new('node.resize', 'LEFTMOUSE', 'PRESS')
kmi = km.keymap_items.new('node.add_reroute', 'LEFTMOUSE', 'PRESS', shift=True)
kmi = km.keymap_items.new('node.links_cut', 'LEFTMOUSE', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('node.select_link_viewer', 'LEFTMOUSE', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('node.backimage_move', 'MIDDLEMOUSE', 'PRESS', alt=True)
kmi = km.keymap_items.new('node.backimage_zoom', 'V', 'PRESS')
kmi.properties.factor = 0.833329975605011
kmi = km.keymap_items.new('node.backimage_zoom', 'V', 'PRESS', alt=True)
kmi.properties.factor = 1.2000000476837158
kmi = km.keymap_items.new('node.backimage_sample', 'ACTIONMOUSE', 'PRESS', alt=True)
kmi = km.keymap_items.new('node.link_make', 'F', 'PRESS')
kmi.properties.replace = False
kmi = km.keymap_items.new('node.link_make', 'F', 'PRESS', shift=True)
kmi.properties.replace = True
kmi = km.keymap_items.new('wm.call_menu', 'A', 'PRESS', shift=True)
kmi.properties.name = 'NODE_MT_add'
kmi = km.keymap_items.new('node.duplicate_move', 'D', 'PRESS', shift=True)
kmi = km.keymap_items.new('node.duplicate_move_keep_inputs', 'D', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('node.parent_set', 'P', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('node.parent_clear', 'P', 'PRESS', alt=True)
kmi = km.keymap_items.new('node.join', 'J', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('node.hide_toggle', 'H', 'PRESS')
kmi = km.keymap_items.new('node.mute_toggle', 'M', 'PRESS')
kmi = km.keymap_items.new('node.preview_toggle', 'H', 'PRESS', shift=True)
kmi = km.keymap_items.new('node.hide_socket_toggle', 'H', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('node.view_all', 'HOME', 'PRESS')
kmi = km.keymap_items.new('node.view_selected', 'NUMPAD_PERIOD', 'PRESS')
kmi = km.keymap_items.new('node.select_border', 'B', 'PRESS')
kmi.properties.tweak = False
kmi = km.keymap_items.new('node.delete', 'X', 'PRESS')
kmi = km.keymap_items.new('node.delete', 'DEL', 'PRESS')
kmi = km.keymap_items.new('node.delete_reconnect', 'X', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('node.select_all', 'A', 'PRESS', ctrl=True)
kmi.properties.action = 'TOGGLE'
kmi = km.keymap_items.new('node.select_all', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
kmi = km.keymap_items.new('node.select_linked_to', 'L', 'PRESS', shift=True)
kmi = km.keymap_items.new('node.select_linked_from', 'L', 'PRESS')
kmi = km.keymap_items.new('node.select_grouped', 'G', 'PRESS', shift=True)
kmi = km.keymap_items.new('node.select_same_type_step', 'RIGHT_BRACKET', 'PRESS', shift=True)
kmi.properties.prev = False
kmi = km.keymap_items.new('node.select_same_type_step', 'LEFT_BRACKET', 'PRESS', shift=True)
kmi.properties.prev = True
kmi = km.keymap_items.new('node.find_node', 'F', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('node.group_make', 'G', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('node.group_ungroup', 'G', 'PRESS', alt=True)
kmi = km.keymap_items.new('node.group_separate', 'P', 'PRESS')
kmi = km.keymap_items.new('node.group_edit', 'TAB', 'PRESS')
kmi.properties.exit = False
kmi = km.keymap_items.new('node.group_edit', 'TAB', 'PRESS', shift=True)
kmi.properties.exit = True
kmi = km.keymap_items.new('node.group_make', 'G', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('node.group_ungroup', 'G', 'PRESS', alt=True)
kmi = km.keymap_items.new('node.group_separate', 'P', 'PRESS')
kmi = km.keymap_items.new('node.group_edit', 'TAB', 'PRESS')
kmi.properties.exit = False
kmi = km.keymap_items.new('node.group_edit', 'TAB', 'PRESS', shift=True)
kmi.properties.exit = True
kmi = km.keymap_items.new('node.group_make', 'G', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('node.group_ungroup', 'G', 'PRESS', alt=True)
kmi = km.keymap_items.new('node.group_separate', 'P', 'PRESS')
kmi = km.keymap_items.new('node.group_edit', 'TAB', 'PRESS')
kmi.properties.exit = False
kmi = km.keymap_items.new('node.group_edit', 'TAB', 'PRESS', shift=True)
kmi.properties.exit = True
kmi = km.keymap_items.new('node.read_renderlayers', 'R', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('node.read_fullsamplelayers', 'R', 'PRESS', shift=True)
kmi = km.keymap_items.new('node.render_changed', 'Z', 'PRESS')
kmi = km.keymap_items.new('node.clipboard_copy', 'C', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('node.clipboard_paste', 'V', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('node.viewer_border', 'B', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('node.translate_attach', 'G', 'PRESS')
kmi = km.keymap_items.new('node.translate_attach', 'EVT_TWEAK_A', 'ANY')
kmi = km.keymap_items.new('node.translate_attach', 'EVT_TWEAK_S', 'ANY')
kmi = km.keymap_items.new('transform.rotate', 'R', 'PRESS')
kmi = km.keymap_items.new('transform.resize', 'S', 'PRESS')
kmi = km.keymap_items.new('node.move_detach_links', 'D', 'PRESS', alt=True)
kmi = km.keymap_items.new('node.move_detach_links_release', 'EVT_TWEAK_A', 'ANY', alt=True)
kmi = km.keymap_items.new('node.move_detach_links', 'EVT_TWEAK_S', 'ANY', alt=True)
kmi = km.keymap_items.new('node.detach_translate_attach', 'F', 'PRESS', alt=True)

# Map Info
km = kc.keymaps.new('Info', space_type='INFO', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('info.select_pick', 'SELECTMOUSE', 'PRESS')
kmi = km.keymap_items.new('info.select_all_toggle', 'A', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('info.select_border', 'B', 'PRESS')
kmi = km.keymap_items.new('info.report_replay', 'R', 'PRESS')
kmi = km.keymap_items.new('info.report_delete', 'X', 'PRESS')
kmi = km.keymap_items.new('info.report_delete', 'DEL', 'PRESS')
kmi = km.keymap_items.new('info.report_copy', 'C', 'PRESS', ctrl=True)

# Map File Browser Main
km = kc.keymaps.new('File Browser Main', space_type='FILE_BROWSER', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('file.execute', 'LEFTMOUSE', 'DOUBLE_CLICK')
kmi.properties.need_active = True
kmi = km.keymap_items.new('file.select', 'LEFTMOUSE', 'CLICK')
kmi = km.keymap_items.new('file.select', 'LEFTMOUSE', 'CLICK', shift=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('file.select', 'LEFTMOUSE', 'CLICK', alt=True)
kmi.properties.extend = True
kmi.properties.fill = True
kmi = km.keymap_items.new('file.select', 'RIGHTMOUSE', 'CLICK')
kmi.properties.open = False
kmi = km.keymap_items.new('file.select', 'RIGHTMOUSE', 'CLICK', shift=True)
kmi.properties.extend = True
kmi.properties.open = False
kmi = km.keymap_items.new('file.select', 'RIGHTMOUSE', 'CLICK', alt=True)
kmi.properties.extend = True
kmi.properties.fill = True
kmi.properties.open = False
kmi = km.keymap_items.new('file.select_all_toggle', 'A', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('file.refresh', 'NUMPAD_PERIOD', 'PRESS')
kmi = km.keymap_items.new('file.select_border', 'B', 'PRESS')
kmi = km.keymap_items.new('file.select_border', 'EVT_TWEAK_L', 'ANY')
kmi = km.keymap_items.new('file.rename', 'LEFTMOUSE', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('file.highlight', 'MOUSEMOVE', 'ANY', any=True)
kmi = km.keymap_items.new('file.filenum', 'NUMPAD_PLUS', 'PRESS')
kmi.properties.increment = 1
kmi = km.keymap_items.new('file.filenum', 'NUMPAD_PLUS', 'PRESS', shift=True)
kmi.properties.increment = 10
kmi = km.keymap_items.new('file.filenum', 'NUMPAD_PLUS', 'PRESS', ctrl=True)
kmi.properties.increment = 100
kmi = km.keymap_items.new('file.filenum', 'NUMPAD_MINUS', 'PRESS')
kmi.properties.increment = -1
kmi = km.keymap_items.new('file.filenum', 'NUMPAD_MINUS', 'PRESS', shift=True)
kmi.properties.increment = -10
kmi = km.keymap_items.new('file.filenum', 'NUMPAD_MINUS', 'PRESS', ctrl=True)
kmi.properties.increment = -100

# Map Dopesheet
km = kc.keymaps.new('Dopesheet', space_type='DOPESHEET_EDITOR', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('action.clickselect', 'SELECTMOUSE', 'PRESS')
kmi.properties.extend = False
kmi.properties.column = False
kmi = km.keymap_items.new('action.clickselect', 'SELECTMOUSE', 'PRESS', alt=True)
kmi.properties.extend = False
kmi.properties.column = True
kmi = km.keymap_items.new('action.clickselect', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi.properties.column = False
kmi = km.keymap_items.new('action.clickselect', 'SELECTMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.extend = True
kmi.properties.column = True
kmi = km.keymap_items.new('action.select_leftright', 'SELECTMOUSE', 'PRESS', ctrl=True)
kmi.properties.mode = 'CHECK'
kmi.properties.extend = False
kmi = km.keymap_items.new('action.select_leftright', 'SELECTMOUSE', 'PRESS', shift=True, ctrl=True)
kmi.properties.mode = 'CHECK'
kmi.properties.extend = True
kmi = km.keymap_items.new('action.select_leftright', 'LEFT_BRACKET', 'PRESS')
kmi.properties.mode = 'LEFT'
kmi.properties.extend = False
kmi = km.keymap_items.new('action.select_leftright', 'RIGHT_BRACKET', 'PRESS')
kmi.properties.mode = 'RIGHT'
kmi.properties.extend = False
kmi = km.keymap_items.new('action.select_all_toggle', 'A', 'PRESS', ctrl=True)
kmi.properties.invert = False
kmi = km.keymap_items.new('action.select_all_toggle', 'I', 'PRESS', ctrl=True)
kmi.properties.invert = True
kmi = km.keymap_items.new('action.select_border', 'B', 'PRESS')
kmi.properties.axis_range = False
kmi = km.keymap_items.new('action.select_border', 'B', 'PRESS', alt=True)
kmi.properties.axis_range = True
kmi = km.keymap_items.new('action.select_column', 'K', 'PRESS')
kmi.properties.mode = 'KEYS'
kmi = km.keymap_items.new('action.select_column', 'K', 'PRESS', ctrl=True)
kmi.properties.mode = 'CFRA'
kmi = km.keymap_items.new('action.select_column', 'K', 'PRESS', shift=True)
kmi.properties.mode = 'MARKERS_COLUMN'
kmi = km.keymap_items.new('action.select_column', 'K', 'PRESS', alt=True)
kmi.properties.mode = 'MARKERS_BETWEEN'
kmi = km.keymap_items.new('action.select_more', 'NUMPAD_PLUS', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('action.select_less', 'NUMPAD_MINUS', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('action.select_linked', 'L', 'PRESS')
kmi = km.keymap_items.new('action.frame_jump', 'G', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('action.snap', 'S', 'PRESS', shift=True)
kmi = km.keymap_items.new('action.mirror', 'M', 'PRESS', shift=True)
kmi = km.keymap_items.new('action.handle_type', 'V', 'PRESS')
kmi = km.keymap_items.new('action.interpolation_type', 'T', 'PRESS')
kmi = km.keymap_items.new('action.extrapolation_type', 'E', 'PRESS', shift=True)
kmi = km.keymap_items.new('action.keyframe_type', 'R', 'PRESS')
kmi = km.keymap_items.new('action.clean', 'O', 'PRESS')
kmi = km.keymap_items.new('action.sample', 'O', 'PRESS', shift=True)
kmi = km.keymap_items.new('action.delete', 'X', 'PRESS')
kmi = km.keymap_items.new('action.delete', 'DEL', 'PRESS')
kmi = km.keymap_items.new('action.duplicate_move', 'D', 'PRESS', shift=True)
kmi = km.keymap_items.new('action.keyframe_insert', 'I', 'PRESS')
kmi = km.keymap_items.new('action.copy', 'C', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('action.paste', 'V', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('action.previewrange_set', 'P', 'PRESS', ctrl=True, alt=True)
kmi = km.keymap_items.new('action.view_all', 'HOME', 'PRESS')
kmi = km.keymap_items.new('action.view_selected', 'NUMPAD_PERIOD', 'PRESS')
kmi = km.keymap_items.new('anim.channels_editable_toggle', 'TAB', 'PRESS')
kmi = km.keymap_items.new('transform.transform', 'G', 'PRESS')
kmi.properties.mode = 'TIME_TRANSLATE'
kmi = km.keymap_items.new('transform.transform', 'EVT_TWEAK_S', 'ANY')
kmi.properties.mode = 'TIME_TRANSLATE'
kmi = km.keymap_items.new('transform.transform', 'E', 'PRESS')
kmi.properties.mode = 'TIME_EXTEND'
kmi = km.keymap_items.new('transform.transform', 'S', 'PRESS')
kmi.properties.mode = 'TIME_SCALE'
kmi = km.keymap_items.new('transform.transform', 'T', 'PRESS', shift=True)
kmi.properties.mode = 'TIME_SLIDE'
kmi = km.keymap_items.new('marker.add', 'M', 'PRESS')
kmi = km.keymap_items.new('marker.rename', 'M', 'PRESS', ctrl=True)

# Map NLA Editor
km = kc.keymaps.new('NLA Editor', space_type='NLA_EDITOR', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('nla.click_select', 'SELECTMOUSE', 'PRESS')
kmi.properties.extend = False
kmi = km.keymap_items.new('nla.click_select', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('nla.select_leftright', 'SELECTMOUSE', 'PRESS', ctrl=True)
kmi.properties.mode = 'CHECK'
kmi.properties.extend = False
kmi = km.keymap_items.new('nla.select_leftright', 'SELECTMOUSE', 'PRESS', shift=True, ctrl=True)
kmi.properties.mode = 'CHECK'
kmi.properties.extend = True
kmi = km.keymap_items.new('nla.select_leftright', 'LEFT_BRACKET', 'PRESS')
kmi.properties.mode = 'LEFT'
kmi.properties.extend = False
kmi = km.keymap_items.new('nla.select_leftright', 'RIGHT_BRACKET', 'PRESS')
kmi.properties.mode = 'RIGHT'
kmi.properties.extend = False
kmi = km.keymap_items.new('nla.select_all_toggle', 'A', 'PRESS', ctrl=True)
kmi.properties.invert = False
kmi = km.keymap_items.new('nla.select_all_toggle', 'I', 'PRESS', ctrl=True)
kmi.properties.invert = True
kmi = km.keymap_items.new('nla.select_border', 'B', 'PRESS')
kmi.properties.axis_range = False
kmi = km.keymap_items.new('nla.select_border', 'B', 'PRESS', alt=True)
kmi.properties.axis_range = True
kmi = km.keymap_items.new('nla.view_all', 'HOME', 'PRESS')
kmi = km.keymap_items.new('nla.view_selected', 'NUMPAD_PERIOD', 'PRESS')
kmi = km.keymap_items.new('nla.tweakmode_enter', 'TAB', 'PRESS')
kmi = km.keymap_items.new('nla.tweakmode_exit', 'TAB', 'PRESS')
kmi = km.keymap_items.new('nla.actionclip_add', 'A', 'PRESS', shift=True)
kmi = km.keymap_items.new('nla.transition_add', 'T', 'PRESS', shift=True)
kmi = km.keymap_items.new('nla.soundclip_add', 'K', 'PRESS', shift=True)
kmi = km.keymap_items.new('nla.meta_add', 'G', 'PRESS', shift=True)
kmi = km.keymap_items.new('nla.meta_remove', 'G', 'PRESS', alt=True)
kmi = km.keymap_items.new('nla.duplicate', 'D', 'PRESS', shift=True)
kmi = km.keymap_items.new('nla.delete', 'X', 'PRESS')
kmi = km.keymap_items.new('nla.delete', 'DEL', 'PRESS')
kmi = km.keymap_items.new('nla.split', 'Y', 'PRESS')
kmi = km.keymap_items.new('nla.mute_toggle', 'H', 'PRESS')
kmi = km.keymap_items.new('nla.swap', 'F', 'PRESS', alt=True)
kmi = km.keymap_items.new('nla.move_up', 'PAGE_UP', 'PRESS')
kmi = km.keymap_items.new('nla.move_down', 'PAGE_DOWN', 'PRESS')
kmi = km.keymap_items.new('nla.apply_scale', 'A', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('nla.clear_scale', 'S', 'PRESS', alt=True)
kmi = km.keymap_items.new('nla.snap', 'S', 'PRESS', shift=True)
kmi = km.keymap_items.new('nla.fmodifier_add', 'M', 'PRESS', shift=True, ctrl=True)
kmi = km.keymap_items.new('transform.transform', 'G', 'PRESS')
kmi.properties.mode = 'TRANSLATION'
kmi = km.keymap_items.new('transform.transform', 'EVT_TWEAK_S', 'ANY')
kmi.properties.mode = 'TRANSLATION'
kmi = km.keymap_items.new('transform.transform', 'E', 'PRESS')
kmi.properties.mode = 'TIME_EXTEND'
kmi = km.keymap_items.new('transform.transform', 'S', 'PRESS')
kmi.properties.mode = 'TIME_SCALE'
kmi = km.keymap_items.new('marker.add', 'M', 'PRESS')
kmi = km.keymap_items.new('marker.rename', 'M', 'PRESS', ctrl=True)

# Map Sequencer
km = kc.keymaps.new('Sequencer', space_type='SEQUENCE_EDITOR', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('sequencer.select_all', 'A', 'PRESS', ctrl=True)
kmi.properties.action = 'TOGGLE'
kmi = km.keymap_items.new('sequencer.select_all', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
kmi = km.keymap_items.new('sequencer.cut', 'K', 'PRESS')
kmi.properties.type = 'SOFT'
kmi = km.keymap_items.new('sequencer.cut', 'K', 'PRESS', shift=True)
kmi.properties.type = 'HARD'
kmi = km.keymap_items.new('sequencer.mute', 'H', 'PRESS')
kmi.properties.unselected = False
kmi = km.keymap_items.new('sequencer.mute', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.keymap_items.new('sequencer.unmute', 'H', 'PRESS', alt=True)
kmi.properties.unselected = False
kmi = km.keymap_items.new('sequencer.unmute', 'H', 'PRESS', shift=True, alt=True)
kmi.properties.unselected = True
kmi = km.keymap_items.new('sequencer.lock', 'L', 'PRESS', shift=True)
kmi = km.keymap_items.new('sequencer.unlock', 'L', 'PRESS', shift=True, alt=True)
kmi = km.keymap_items.new('sequencer.reassign_inputs', 'R', 'PRESS')
kmi = km.keymap_items.new('sequencer.reload', 'R', 'PRESS', alt=True)
kmi = km.keymap_items.new('sequencer.reload', 'R', 'PRESS', shift=True, alt=True)
kmi.properties.adjust_length = True
kmi = km.keymap_items.new('sequencer.offset_clear', 'O', 'PRESS', alt=True)
kmi = km.keymap_items.new('sequencer.duplicate_move', 'D', 'PRESS', shift=True)
kmi = km.keymap_items.new('sequencer.delete', 'X', 'PRESS')
kmi = km.keymap_items.new('sequencer.delete', 'DEL', 'PRESS')
kmi = km.keymap_items.new('sequencer.copy', 'C', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('sequencer.paste', 'V', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('sequencer.images_separate', 'Y', 'PRESS')
kmi = km.keymap_items.new('sequencer.meta_toggle', 'TAB', 'PRESS')
kmi = km.keymap_items.new('sequencer.meta_make', 'G', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('sequencer.meta_separate', 'G', 'PRESS', alt=True)
kmi = km.keymap_items.new('sequencer.view_all', 'HOME', 'PRESS')
kmi = km.keymap_items.new('sequencer.view_selected', 'NUMPAD_PERIOD', 'PRESS')
kmi = km.keymap_items.new('sequencer.strip_jump', 'PAGE_UP', 'PRESS')
kmi.properties.next = True
kmi.properties.center = False
kmi = km.keymap_items.new('sequencer.strip_jump', 'PAGE_DOWN', 'PRESS')
kmi.properties.next = False
kmi.properties.center = False
kmi = km.keymap_items.new('sequencer.strip_jump', 'PAGE_UP', 'PRESS', alt=True)
kmi.properties.next = True
kmi.properties.center = True
kmi = km.keymap_items.new('sequencer.strip_jump', 'PAGE_DOWN', 'PRESS', alt=True)
kmi.properties.next = False
kmi.properties.center = True
kmi = km.keymap_items.new('sequencer.swap', 'LEFT_ARROW', 'PRESS', alt=True)
kmi.properties.side = 'LEFT'
kmi = km.keymap_items.new('sequencer.swap', 'RIGHT_ARROW', 'PRESS', alt=True)
kmi.properties.side = 'RIGHT'
kmi = km.keymap_items.new('sequencer.gap_remove', 'BACK_SPACE', 'PRESS')
kmi.properties.all = False
kmi = km.keymap_items.new('sequencer.gap_remove', 'BACK_SPACE', 'PRESS', shift=True)
kmi.properties.all = True
kmi = km.keymap_items.new('sequencer.gap_insert', 'EQUAL', 'PRESS', shift=True)
kmi = km.keymap_items.new('sequencer.snap', 'S', 'PRESS', shift=True)
kmi = km.keymap_items.new('sequencer.swap_inputs', 'S', 'PRESS', alt=True)
kmi = km.keymap_items.new('sequencer.cut_multicam', 'ONE', 'PRESS')
kmi.properties.camera = 1
kmi = km.keymap_items.new('sequencer.cut_multicam', 'TWO', 'PRESS')
kmi.properties.camera = 2
kmi = km.keymap_items.new('sequencer.cut_multicam', 'THREE', 'PRESS')
kmi.properties.camera = 3
kmi = km.keymap_items.new('sequencer.cut_multicam', 'FOUR', 'PRESS')
kmi.properties.camera = 4
kmi = km.keymap_items.new('sequencer.cut_multicam', 'FIVE', 'PRESS')
kmi.properties.camera = 5
kmi = km.keymap_items.new('sequencer.cut_multicam', 'SIX', 'PRESS')
kmi.properties.camera = 6
kmi = km.keymap_items.new('sequencer.cut_multicam', 'SEVEN', 'PRESS')
kmi.properties.camera = 7
kmi = km.keymap_items.new('sequencer.cut_multicam', 'EIGHT', 'PRESS')
kmi.properties.camera = 8
kmi = km.keymap_items.new('sequencer.cut_multicam', 'NINE', 'PRESS')
kmi.properties.camera = 9
kmi = km.keymap_items.new('sequencer.cut_multicam', 'ZERO', 'PRESS')
kmi.properties.camera = 10
kmi = km.keymap_items.new('sequencer.select', 'SELECTMOUSE', 'PRESS')
kmi.properties.extend = False
kmi.properties.linked_handle = False
kmi.properties.left_right = False
kmi.properties.linked_time = False
kmi = km.keymap_items.new('sequencer.select', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi.properties.linked_handle = False
kmi.properties.left_right = False
kmi.properties.linked_time = False
kmi = km.keymap_items.new('sequencer.select', 'SELECTMOUSE', 'PRESS', alt=True)
kmi.properties.extend = False
kmi.properties.linked_handle = True
kmi.properties.left_right = False
kmi.properties.linked_time = False
kmi = km.keymap_items.new('sequencer.select', 'SELECTMOUSE', 'PRESS', shift=True, alt=True)
kmi.properties.extend = True
kmi.properties.linked_handle = True
kmi.properties.left_right = False
kmi.properties.linked_time = False
kmi = km.keymap_items.new('sequencer.select', 'SELECTMOUSE', 'PRESS', ctrl=True)
kmi.properties.extend = False
kmi.properties.linked_handle = False
kmi.properties.left_right = True
kmi.properties.linked_time = True
kmi = km.keymap_items.new('sequencer.select', 'SELECTMOUSE', 'PRESS', shift=True, ctrl=True)
kmi.properties.extend = True
kmi.properties.linked_handle = False
kmi.properties.left_right = False
kmi.properties.linked_time = True
kmi = km.keymap_items.new('sequencer.select_more', 'NUMPAD_PLUS', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('sequencer.select_less', 'NUMPAD_MINUS', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('sequencer.select_linked_pick', 'L', 'PRESS')
kmi.properties.extend = False
kmi = km.keymap_items.new('sequencer.select_linked_pick', 'L', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('sequencer.select_linked', 'L', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('sequencer.select_border', 'B', 'PRESS')
kmi = km.keymap_items.new('sequencer.select_grouped', 'G', 'PRESS', shift=True)
kmi = km.keymap_items.new('wm.call_menu', 'A', 'PRESS', shift=True)
kmi.properties.name = 'SEQUENCER_MT_add'
kmi = km.keymap_items.new('wm.call_menu', 'C', 'PRESS')
kmi.properties.name = 'SEQUENCER_MT_change'
kmi = km.keymap_items.new('wm.context_set_int', 'O', 'PRESS')
kmi.properties.data_path = 'scene.sequence_editor.overlay_frame'
kmi.properties.value = 0
kmi = km.keymap_items.new('transform.seq_slide', 'G', 'PRESS')
kmi = km.keymap_items.new('transform.seq_slide', 'EVT_TWEAK_S', 'ANY')
kmi = km.keymap_items.new('transform.transform', 'E', 'PRESS')
kmi.properties.mode = 'TIME_EXTEND'
kmi = km.keymap_items.new('marker.add', 'M', 'PRESS')
kmi = km.keymap_items.new('marker.rename', 'M', 'PRESS', ctrl=True)

# Map Clip Editor
km = kc.keymaps.new('Clip Editor', space_type='CLIP_EDITOR', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('clip.view_pan', 'MIDDLEMOUSE', 'PRESS')
kmi = km.keymap_items.new('clip.view_pan', 'MIDDLEMOUSE', 'PRESS', shift=True)
kmi = km.keymap_items.new('clip.view_pan', 'TRACKPADPAN', 'ANY')
kmi = km.keymap_items.new('clip.view_zoom', 'MIDDLEMOUSE', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('clip.view_zoom', 'TRACKPADZOOM', 'ANY')
kmi = km.keymap_items.new('clip.view_zoom', 'TRACKPADPAN', 'ANY', ctrl=True)
kmi = km.keymap_items.new('clip.view_zoom_in', 'WHEELINMOUSE', 'PRESS')
kmi = km.keymap_items.new('clip.view_zoom_out', 'WHEELOUTMOUSE', 'PRESS')
kmi = km.keymap_items.new('clip.view_zoom_in', 'NUMPAD_PLUS', 'PRESS')
kmi = km.keymap_items.new('clip.view_zoom_out', 'NUMPAD_MINUS', 'PRESS')
kmi = km.keymap_items.new('clip.view_zoom_ratio', 'NUMPAD_8', 'PRESS', ctrl=True)
kmi.properties.ratio = 8.0
kmi = km.keymap_items.new('clip.view_zoom_ratio', 'NUMPAD_4', 'PRESS', ctrl=True)
kmi.properties.ratio = 4.0
kmi = km.keymap_items.new('clip.view_zoom_ratio', 'NUMPAD_2', 'PRESS', ctrl=True)
kmi.properties.ratio = 2.0
kmi = km.keymap_items.new('clip.view_zoom_ratio', 'NUMPAD_8', 'PRESS', shift=True)
kmi.properties.ratio = 8.0
kmi = km.keymap_items.new('clip.view_zoom_ratio', 'NUMPAD_4', 'PRESS', shift=True)
kmi.properties.ratio = 4.0
kmi = km.keymap_items.new('clip.view_zoom_ratio', 'NUMPAD_2', 'PRESS', shift=True)
kmi.properties.ratio = 2.0
kmi = km.keymap_items.new('clip.view_zoom_ratio', 'NUMPAD_1', 'PRESS')
kmi.properties.ratio = 1.0
kmi = km.keymap_items.new('clip.view_zoom_ratio', 'NUMPAD_2', 'PRESS')
kmi.properties.ratio = 0.5
kmi = km.keymap_items.new('clip.view_zoom_ratio', 'NUMPAD_4', 'PRESS')
kmi.properties.ratio = 0.25
kmi = km.keymap_items.new('clip.view_zoom_ratio', 'NUMPAD_8', 'PRESS')
kmi.properties.ratio = 0.125
kmi = km.keymap_items.new('clip.view_all', 'HOME', 'PRESS')
kmi = km.keymap_items.new('clip.view_all', 'F', 'PRESS')
kmi.properties.fit_view = True
kmi = km.keymap_items.new('clip.view_selected', 'NUMPAD_PERIOD', 'PRESS')
kmi = km.keymap_items.new('clip.view_all', 'NDOF_BUTTON_FIT', 'PRESS')
kmi = km.keymap_items.new('clip.view_ndof', 'NDOF_MOTION', 'ANY')
kmi = km.keymap_items.new('clip.frame_jump', 'LEFT_ARROW', 'PRESS', shift=True, ctrl=True)
kmi.properties.position = 'PATHSTART'
kmi = km.keymap_items.new('clip.frame_jump', 'RIGHT_ARROW', 'PRESS', shift=True, ctrl=True)
kmi.properties.position = 'PATHEND'
kmi = km.keymap_items.new('clip.frame_jump', 'LEFT_ARROW', 'PRESS', shift=True, alt=True)
kmi.properties.position = 'FAILEDPREV'
kmi = km.keymap_items.new('clip.frame_jump', 'RIGHT_ARROW', 'PRESS', shift=True, alt=True)
kmi.properties.position = 'PATHSTART'
kmi = km.keymap_items.new('clip.change_frame', 'LEFTMOUSE', 'PRESS')
kmi = km.keymap_items.new('clip.select', 'SELECTMOUSE', 'PRESS')
kmi.properties.extend = False
kmi = km.keymap_items.new('clip.select', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('clip.select_all', 'A', 'PRESS', ctrl=True)
kmi.properties.action = 'TOGGLE'
kmi = km.keymap_items.new('clip.select_all', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
kmi = km.keymap_items.new('clip.select_border', 'B', 'PRESS')
kmi = km.keymap_items.new('clip.select_circle', 'C', 'PRESS')
kmi = km.keymap_items.new('wm.call_menu', 'G', 'PRESS', shift=True)
kmi.properties.name = 'CLIP_MT_select_grouped'
kmi = km.keymap_items.new('clip.select_lasso', 'EVT_TWEAK_A', 'ANY', ctrl=True, alt=True)
kmi.properties.deselect = False
kmi = km.keymap_items.new('clip.select_lasso', 'EVT_TWEAK_A', 'ANY', shift=True, ctrl=True, alt=True)
kmi.properties.deselect = True
kmi = km.keymap_items.new('clip.add_marker_slide', 'LEFTMOUSE', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('clip.delete_marker', 'DEL', 'PRESS', shift=True)
kmi = km.keymap_items.new('clip.delete_marker', 'X', 'PRESS', shift=True)
kmi = km.keymap_items.new('clip.slide_marker', 'LEFTMOUSE', 'PRESS')
kmi = km.keymap_items.new('clip.disable_markers', 'D', 'PRESS', shift=True)
kmi.properties.action = 'TOGGLE'
kmi = km.keymap_items.new('clip.delete_track', 'DEL', 'PRESS')
kmi = km.keymap_items.new('clip.delete_track', 'X', 'PRESS')
kmi = km.keymap_items.new('clip.lock_tracks', 'L', 'PRESS', ctrl=True)
kmi.properties.action = 'LOCK'
kmi = km.keymap_items.new('clip.lock_tracks', 'L', 'PRESS', alt=True)
kmi.properties.action = 'UNLOCK'
kmi = km.keymap_items.new('clip.hide_tracks', 'H', 'PRESS')
kmi.properties.unselected = False
kmi = km.keymap_items.new('clip.hide_tracks', 'H', 'PRESS', shift=True)
kmi.properties.unselected = True
kmi = km.keymap_items.new('clip.hide_tracks_clear', 'H', 'PRESS', alt=True)
kmi = km.keymap_items.new('clip.join_tracks', 'J', 'PRESS', ctrl=True)
kmi = km.keymap_items.new('wm.call_menu', 'W', 'PRESS')
kmi.properties.name = 'CLIP_MT_tracking_specials'
kmi = km.keymap_items.new('wm.context_toggle', 'L', 'PRESS')
kmi.properties.data_path = 'space_data.lock_selection'
kmi = km.keymap_items.new('wm.context_toggle', 'D', 'PRESS', alt=True)
kmi.properties.data_path = 'space_data.show_disabled'
kmi = km.keymap_items.new('wm.context_toggle', 'S', 'PRESS', alt=True)
kmi.properties.data_path = 'space_data.show_marker_search'
kmi = km.keymap_items.new('wm.context_toggle', 'M', 'PRESS')
kmi.properties.data_path = 'space_data.use_mute_footage'
kmi = km.keymap_items.new('transform.translate', 'G', 'PRESS')
kmi = km.keymap_items.new('transform.translate', 'EVT_TWEAK_S', 'ANY')
kmi = km.keymap_items.new('transform.resize', 'S', 'PRESS')
kmi = km.keymap_items.new('transform.rotate', 'R', 'PRESS')
kmi = km.keymap_items.new('clip.clear_track_path', 'T', 'PRESS', alt=True)
kmi.properties.action = 'REMAINED'
kmi.properties.clear_active = False
kmi = km.keymap_items.new('clip.clear_track_path', 'T', 'PRESS', shift=True)
kmi.properties.action = 'UPTO'
kmi.properties.clear_active = False
kmi = km.keymap_items.new('clip.clear_track_path', 'T', 'PRESS', shift=True, alt=True)
kmi.properties.action = 'ALL'
kmi.properties.clear_active = False

# Map Clip Graph Editor
km = kc.keymaps.new('Clip Graph Editor', space_type='CLIP_EDITOR', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('clip.change_frame', 'ACTIONMOUSE', 'PRESS')
kmi = km.keymap_items.new('clip.graph_select', 'SELECTMOUSE', 'PRESS')
kmi.properties.extend = False
kmi = km.keymap_items.new('clip.graph_select', 'SELECTMOUSE', 'PRESS', shift=True)
kmi.properties.extend = True
kmi = km.keymap_items.new('clip.graph_select_all_markers', 'A', 'PRESS', ctrl=True)
kmi.properties.action = 'TOGGLE'
kmi = km.keymap_items.new('clip.graph_select_all_markers', 'I', 'PRESS', ctrl=True)
kmi.properties.action = 'INVERT'
kmi = km.keymap_items.new('clip.graph_select_border', 'B', 'PRESS')
kmi = km.keymap_items.new('clip.graph_delete_curve', 'DEL', 'PRESS')
kmi = km.keymap_items.new('clip.graph_delete_curve', 'X', 'PRESS')
kmi = km.keymap_items.new('clip.graph_delete_knot', 'DEL', 'PRESS', shift=True)
kmi = km.keymap_items.new('clip.graph_delete_knot', 'X', 'PRESS', shift=True)
kmi = km.keymap_items.new('clip.graph_view_all', 'HOME', 'PRESS')
kmi = km.keymap_items.new('clip.graph_center_current_frame', 'NUMPAD_PERIOD', 'PRESS')
kmi = km.keymap_items.new('wm.context_toggle', 'L', 'PRESS')
kmi.properties.data_path = 'space_data.lock_time_cursor'
kmi = km.keymap_items.new('clip.clear_track_path', 'T', 'PRESS', alt=True)
kmi.properties.action = 'REMAINED'
kmi.properties.clear_active = True
kmi = km.keymap_items.new('clip.clear_track_path', 'T', 'PRESS', shift=True)
kmi.properties.action = 'UPTO'
kmi.properties.clear_active = True
kmi = km.keymap_items.new('clip.clear_track_path', 'T', 'PRESS', shift=True, alt=True)
kmi.properties.action = 'ALL'
kmi.properties.clear_active = True
kmi = km.keymap_items.new('clip.graph_disable_markers', 'D', 'PRESS', shift=True)
kmi.properties.action = 'TOGGLE'
kmi = km.keymap_items.new('transform.translate', 'G', 'PRESS')
kmi = km.keymap_items.new('transform.translate', 'EVT_TWEAK_S', 'ANY')
kmi = km.keymap_items.new('transform.resize', 'S', 'PRESS')
kmi = km.keymap_items.new('transform.rotate', 'R', 'PRESS')

# Map Image Paint
km = kc.keymaps.new('Image Paint', space_type='EMPTY', region_type='WINDOW', modal=False)

kmi = km.keymap_items.new('paint.image_paint', 'LEFTMOUSE', 'PRESS')
kmi.properties.mode = 'NORMAL'
kmi = km.keymap_items.new('paint.image_paint', 'LEFTMOUSE', 'PRESS', ctrl=True)
kmi.properties.mode = 'INVERT'
kmi = km.keymap_items.new('paint.grab_clone', 'RIGHTMOUSE', 'PRESS')
kmi = km.keymap_items.new('paint.sample_color', 'RIGHTMOUSE', 'PRESS')
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
kmi = km.keymap_items.new('mesh.shortest_path_pick', 'SELECTMOUSE', 'PRESS', ctrl=True)
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
kmi.properties.quad_method = 'BEAUTY'
kmi.properties.ngon_method = 'BEAUTY'
kmi = km.keymap_items.new('mesh.quads_convert_to_tris', 'T', 'PRESS', shift=True, ctrl=True)
kmi.properties.quad_method = 'FIXED'
kmi.properties.ngon_method = 'CLIP'
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

# Map View3D Gesture Circle
km = kc.keymaps.new('View3D Gesture Circle', space_type='EMPTY', region_type='WINDOW', modal=True)

kmi = km.keymap_items.new_modal('CANCEL', 'ESC', 'PRESS', any=True)
kmi = km.keymap_items.new_modal('CANCEL', 'RIGHTMOUSE', 'ANY', any=True)
kmi = km.keymap_items.new_modal('CONFIRM', 'RET', 'PRESS', any=True)
kmi = km.keymap_items.new_modal('CONFIRM', 'NUMPAD_ENTER', 'PRESS')
kmi = km.keymap_items.new_modal('SELECT', 'LEFTMOUSE', 'PRESS')
kmi = km.keymap_items.new_modal('DESELECT', 'MIDDLEMOUSE', 'PRESS')
kmi = km.keymap_items.new_modal('NOP', 'MIDDLEMOUSE', 'RELEASE')
kmi = km.keymap_items.new_modal('NOP', 'LEFTMOUSE', 'RELEASE')
kmi = km.keymap_items.new_modal('SUBTRACT', 'WHEELUPMOUSE', 'PRESS')
kmi = km.keymap_items.new_modal('SUBTRACT', 'NUMPAD_MINUS', 'PRESS')
kmi = km.keymap_items.new_modal('ADD', 'WHEELDOWNMOUSE', 'PRESS')
kmi = km.keymap_items.new_modal('ADD', 'NUMPAD_PLUS', 'PRESS')
kmi = km.keymap_items.new_modal('SIZE', 'TRACKPADPAN', 'ANY')

# Map Gesture Border
km = kc.keymaps.new('Gesture Border', space_type='EMPTY', region_type='WINDOW', modal=True)

kmi = km.keymap_items.new_modal('CANCEL', 'ESC', 'PRESS', any=True)
kmi = km.keymap_items.new_modal('CANCEL', 'RIGHTMOUSE', 'PRESS', any=True)
kmi = km.keymap_items.new_modal('BEGIN', 'LEFTMOUSE', 'PRESS')
kmi = km.keymap_items.new_modal('SELECT', 'LEFTMOUSE', 'RELEASE', any=True)
kmi = km.keymap_items.new_modal('SELECT', 'RIGHTMOUSE', 'RELEASE', any=True)
kmi = km.keymap_items.new_modal('BEGIN', 'MIDDLEMOUSE', 'PRESS')
kmi = km.keymap_items.new_modal('DESELECT', 'MIDDLEMOUSE', 'RELEASE')

