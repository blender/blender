# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy

KM_HIERARCHY = [
                    ('Window', 'EMPTY', 'WINDOW', []), # file save, window change, exit
                    ('Screen', 'EMPTY', 'WINDOW', [    # full screen, undo, screenshot
                        ('Screen Editing', 'EMPTY', 'WINDOW', []),    # resizing, action corners
                        ]),

                    ('View2D', 'EMPTY', 'WINDOW', []),    # view 2d navigation (per region)
                    ('View2D Buttons List', 'EMPTY', 'WINDOW', []), # view 2d with buttons navigation
                    ('Header', 'EMPTY', 'WINDOW', []),    # header stuff (per region)
                    ('Grease Pencil', 'EMPTY', 'WINDOW', []), # grease pencil stuff (per region)

                    ('3D View', 'VIEW_3D', 'WINDOW', [ # view 3d navigation and generic stuff (select, transform)
                        ('Object Mode', 'EMPTY', 'WINDOW', []),
                        ('Mesh', 'EMPTY', 'WINDOW', []),
                        ('Curve', 'EMPTY', 'WINDOW', []),
                        ('Armature', 'EMPTY', 'WINDOW', []),
                        ('Metaball', 'EMPTY', 'WINDOW', []),
                        ('Lattice', 'EMPTY', 'WINDOW', []),
                        ('Font', 'EMPTY', 'WINDOW', []),

                        ('Pose', 'EMPTY', 'WINDOW', []),

                        ('Vertex Paint', 'EMPTY', 'WINDOW', []),
                        ('Weight Paint', 'EMPTY', 'WINDOW', []),
                        ('Face Mask', 'EMPTY', 'WINDOW', []),
                        ('Image Paint', 'EMPTY', 'WINDOW', []), # image and view3d
                        ('Sculpt', 'EMPTY', 'WINDOW', []),

                        ('Armature Sketch', 'EMPTY', 'WINDOW', []),
                        ('Particle', 'EMPTY', 'WINDOW', []),

                        ('Object Non-modal', 'EMPTY', 'WINDOW', []), # mode change

                        ('3D View Generic', 'VIEW_3D', 'WINDOW', [])    # toolbar and properties
                        ]),

                    ('Frames', 'EMPTY', 'WINDOW', []),    # frame navigation (per region)
                    ('Markers', 'EMPTY', 'WINDOW', []),    # markers (per region)
                    ('Animation', 'EMPTY', 'WINDOW', []),    # frame change on click, preview range (per region)
                    ('Animation Channels', 'EMPTY', 'WINDOW', []),
                    ('Graph Editor', 'GRAPH_EDITOR', 'WINDOW', [
                        ('Graph Editor Generic', 'GRAPH_EDITOR', 'WINDOW', [])
                        ]),
                    ('Dopesheet', 'DOPESHEET_EDITOR', 'WINDOW', []),
                    ('NLA Editor', 'NLA_EDITOR', 'WINDOW', [
                        ('NLA Channels', 'NLA_EDITOR', 'WINDOW', []),
                        ('NLA Generic', 'NLA_EDITOR', 'WINDOW', [])
                        ]),

                    ('Image', 'IMAGE_EDITOR', 'WINDOW', [
                        ('UV Editor', 'EMPTY', 'WINDOW', []), # image (reverse order, UVEdit before Image
                        ('Image Paint', 'EMPTY', 'WINDOW', []), # image and view3d
                        ('Image Generic', 'IMAGE_EDITOR', 'WINDOW', [])
                        ]),

                    ('Timeline', 'TIMELINE', 'WINDOW', []),
                    ('Outliner', 'OUTLINER', 'WINDOW', []),

                    ('Node Editor', 'NODE_EDITOR', 'WINDOW', [
                        ('Node Generic', 'NODE_EDITOR', 'WINDOW', [])
                        ]),
                    ('Sequencer', 'SEQUENCE_EDITOR', 'WINDOW', []),
                    ('Logic Editor', 'LOGIC_EDITOR', 'WINDOW', []),

                    ('File Browser', 'FILE_BROWSER', 'WINDOW', [
                        ('File Browser Main', 'FILE_BROWSER', 'WINDOW', []),
                        ('File Browser Buttons', 'FILE_BROWSER', 'WINDOW', [])
                        ]),

                    ('Property Editor', 'PROPERTIES', 'WINDOW', []), # align context menu

                    ('Script', 'SCRIPTS_WINDOW', 'WINDOW', []),
                    ('Text', 'TEXT_EDITOR', 'WINDOW', []),
                    ('Console', 'CONSOLE', 'WINDOW', []),

                    ('View3D Gesture Circle', 'EMPTY', 'WINDOW', []),
                    ('Gesture Border', 'EMPTY', 'WINDOW', []),
                    ('Standard Modal Map', 'EMPTY', 'WINDOW', []),
                    ('Transform Modal Map', 'EMPTY', 'WINDOW', []),
                    ('View3D Fly Modal', 'EMPTY', 'WINDOW', []),
                    ('View3D Rotate Modal', 'EMPTY', 'WINDOW', []),
                    ('View3D Move Modal', 'EMPTY', 'WINDOW', []),
                    ('View3D Zoom Modal', 'EMPTY', 'WINDOW', []),
                ]


class USERPREF_HT_header(bpy.types.Header):
    bl_space_type = 'USER_PREFERENCES'

    def draw(self, context):
        layout = self.layout
        layout.template_header(menus=False)

        userpref = context.user_preferences

        layout.operator_context = 'EXEC_AREA'
        layout.operator("wm.save_homefile", text="Save As Default")

        if userpref.active_section == 'INPUT':
            layout.operator_context = 'INVOKE_DEFAULT'
            layout.operator("wm.keyconfig_export", "Export Key Configuration...").path = "keymap.py"


class USERPREF_PT_tabs(bpy.types.Panel):
    bl_label = ""
    bl_space_type = 'USER_PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_show_header = False

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences

        layout.prop(userpref, "active_section", expand=True)


class USERPREF_PT_interface(bpy.types.Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Interface"
    bl_region_type = 'WINDOW'
    bl_show_header = False

    def poll(self, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'INTERFACE')

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences
        view = userpref.view

        row = layout.row()


        col = row.column()
        col.label(text="Display:")
        col.prop(view, "tooltips")
        col.prop(view, "display_object_info", text="Object Info")
        col.prop(view, "use_large_cursors")
        col.prop(view, "show_view_name", text="View Name")
        col.prop(view, "show_playback_fps", text="Playback FPS")
        col.prop(view, "global_scene")
        col.prop(view, "pin_floating_panels")
        col.prop(view, "object_origin_size")

        col.separator()
        col.separator()
        col.separator()

        col.prop(view, "show_mini_axis", text="Display Mini Axis")
        sub = col.column()
        sub.enabled = view.show_mini_axis
        sub.prop(view, "mini_axis_size", text="Size")
        sub.prop(view, "mini_axis_brightness", text="Brightness")


        row.separator()
        row.separator()

        col = row.column()
        col.label(text="View Manipulation:")
        col.prop(view, "auto_depth")
        col.prop(view, "zoom_to_mouse")
        col.prop(view, "rotate_around_selection")
        col.prop(view, "global_pivot")

        col.separator()

        col.prop(view, "auto_perspective")
        col.prop(view, "smooth_view")
        col.prop(view, "rotation_angle")

        row.separator()
        row.separator()

        col = row.column()
        #Toolbox doesn't exist yet
        #col.label(text="Toolbox:")
        #col.prop(view, "use_column_layout")
        #col.label(text="Open Toolbox Delay:")
        #col.prop(view, "open_left_mouse_delay", text="Hold LMB")
        #col.prop(view, "open_right_mouse_delay", text="Hold RMB")
        col.prop(view, "use_manipulator")
        sub = col.column()
        sub.enabled = view.use_manipulator
        sub.prop(view, "manipulator_size", text="Size")
        sub.prop(view, "manipulator_handle_size", text="Handle Size")
        sub.prop(view, "manipulator_hotspot", text="Hotspot")

        col.separator()
        col.separator()
        col.separator()

        col.label(text="Menus:")
        col.prop(view, "open_mouse_over")
        col.label(text="Menu Open Delay:")
        col.prop(view, "open_toplevel_delay", text="Top Level")
        col.prop(view, "open_sublevel_delay", text="Sub Level")


class USERPREF_PT_edit(bpy.types.Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Edit"
    bl_region_type = 'WINDOW'
    bl_show_header = False

    def poll(self, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'EDITING')

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences
        edit = userpref.edit


        row = layout.row()


        col = row.column()
        col.label(text="Link Materials To:")
        col.row().prop(edit, "material_link", expand=True)

        col.separator()
        col.separator()
        col.separator()

        col.label(text="New Objects:")
        col.prop(edit, "enter_edit_mode")
        col.label(text="Align To:")
        col.row().prop(edit, "object_align", expand=True)

        col.separator()
        col.separator()
        col.separator()

        col.label(text="Undo:")
        col.prop(edit, "global_undo")
        col.prop(edit, "undo_steps", text="Steps")
        col.prop(edit, "undo_memory_limit", text="Memory Limit")


        row.separator()
        row.separator()


        col = row.column()
        col.label(text="Snap:")
        col.prop(edit, "snap_translate", text="Translate")
        col.prop(edit, "snap_rotate", text="Rotate")
        col.prop(edit, "snap_scale", text="Scale")
        col.separator()
        col.separator()
        col.separator()
        col.label(text="Grease Pencil:")
        col.prop(edit, "grease_pencil_manhattan_distance", text="Manhattan Distance")
        col.prop(edit, "grease_pencil_euclidean_distance", text="Euclidean Distance")
        #col.prop(edit, "grease_pencil_simplify_stroke", text="Simplify Stroke")
        col.prop(edit, "grease_pencil_eraser_radius", text="Eraser Radius")
        col.prop(edit, "grease_pencil_smooth_stroke", text="Smooth Stroke")


        row.separator()
        row.separator()


        col = row.column()
        col.label(text="Keyframing:")
        col.prop(edit, "use_visual_keying")
        col.prop(edit, "keyframe_insert_needed", text="Only Insert Needed")

        col.separator()

        col.label(text="New F-Curve Defaults:")
        col.prop(edit, "new_interpolation_type", text="Interpolation")
        col.prop(edit, "insertkey_xyz_to_rgb", text="XYZ to RGB")

        col.separator()

        col.prop(edit, "auto_keying_enable", text="Auto Keyframing:")

        sub = col.column()

        sub.active = edit.auto_keying_enable
        sub.prop(edit, "auto_keyframe_insert_keyingset", text="Only Insert for Keying Set")
        sub.prop(edit, "auto_keyframe_insert_available", text="Only Insert Available")

        col.separator()
        col.separator()
        col.separator()

        col.label(text="Transform:")
        col.prop(edit, "drag_immediately")


        row.separator()
        row.separator()


        col = row.column()
        col.label(text="Duplicate Data:")
        col.prop(edit, "duplicate_mesh", text="Mesh")
        col.prop(edit, "duplicate_surface", text="Surface")
        col.prop(edit, "duplicate_curve", text="Curve")
        col.prop(edit, "duplicate_text", text="Text")
        col.prop(edit, "duplicate_metaball", text="Metaball")
        col.prop(edit, "duplicate_armature", text="Armature")
        col.prop(edit, "duplicate_lamp", text="Lamp")
        col.prop(edit, "duplicate_material", text="Material")
        col.prop(edit, "duplicate_texture", text="Texture")
        col.prop(edit, "duplicate_fcurve", text="F-Curve")
        col.prop(edit, "duplicate_action", text="Action")
        col.prop(edit, "duplicate_particle", text="Particle")


class USERPREF_PT_system(bpy.types.Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "System"
    bl_region_type = 'WINDOW'
    bl_show_header = False

    def poll(self, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'SYSTEM')

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences
        system = userpref.system
        lamp0 = system.solid_lights[0]
        lamp1 = system.solid_lights[1]
        lamp2 = system.solid_lights[2]

        split = layout.split()

        column = split.column()
        colsplit = column.split(percentage=0.85)

        col = colsplit.column()
        col.label(text="General:")
        col.prop(system, "dpi")
        col.prop(system, "frame_server_port")
        col.prop(system, "scrollback", text="Console Scrollback")
        col.prop(system, "auto_run_python_scripts")

        col.separator()
        col.separator()
        col.separator()

        col.label(text="Sound:")
        col.row().prop(system, "audio_device", expand=True)
        sub = col.column()
        sub.active = system.audio_device != 'NONE'
        #sub.prop(system, "enable_all_codecs")
        sub.prop(system, "game_sound")
        sub.prop(system, "audio_channels", text="Channels")
        sub.prop(system, "audio_mixing_buffer", text="Mixing Buffer")
        sub.prop(system, "audio_sample_rate", text="Sample Rate")
        sub.prop(system, "audio_sample_format", text="Sample Format")

        col.separator()
        col.separator()
        col.separator()



        #column = split.column()
        #colsplit = column.split(percentage=0.85)

        # No translation in 2.5 yet
        #col.prop(system, "language")
        #col.label(text="Translate:")
        #col.prop(system, "translate_tooltips", text="Tooltips")
        #col.prop(system, "translate_buttons", text="Labels")
        #col.prop(system, "translate_toolbox", text="Toolbox")

        #col.separator()

        #col.prop(system, "use_textured_fonts")

        column = split.column()
        colsplit = column.split(percentage=0.85)

        col = colsplit.column()
        col.label(text="OpenGL:")
        col.prop(system, "clip_alpha", slider=True)
        col.prop(system, "use_mipmaps")
        col.prop(system, "use_vbos")
        col.prop(system, "use_antialiasing")
        col.label(text="Window Draw Method:")
        col.row().prop(system, "window_draw_method", expand=True)
        col.label(text="Textures:")
        col.prop(system, "gl_texture_limit", text="Limit Size")
        col.prop(system, "texture_time_out", text="Time Out")
        col.prop(system, "texture_collection_rate", text="Collection Rate")

        col.separator()
        col.separator()
        col.separator()

        col.label(text="Sequencer:")
        col.prop(system, "prefetch_frames")
        col.prop(system, "memory_cache_limit")

        column = split.column()

        column.label(text="Solid OpenGL lights:")

        split = column.split(percentage=0.1)
        split.label()
        split.label(text="Colors:")
        split.label(text="Direction:")


        split = column.split(percentage=0.1)

        if lamp0.enabled == True:
            split.prop(lamp0, "enabled", text="", icon='OUTLINER_OB_LAMP')
        else:
            split.prop(lamp0, "enabled", text="", icon='LAMP_DATA')

        col = split.column()
        col.active = lamp0.enabled
        row = col.row()
        row.label(text="Diffuse:")
        row.prop(lamp0, "diffuse_color", text="")
        row = col.row()
        row.label(text="Specular:")
        row.prop(lamp0, "specular_color", text="")

        col = split.column()
        col.active = lamp0.enabled
        col.prop(lamp0, "direction", text="")


        split = column.split(percentage=0.1)

        if lamp1.enabled == True:
            split.prop(lamp1, "enabled", text="", icon='OUTLINER_OB_LAMP')
        else:
            split.prop(lamp1, "enabled", text="", icon='LAMP_DATA')

        col = split.column()
        col.active = lamp1.enabled
        row = col.row()
        row.label(text="Diffuse:")
        row.prop(lamp1, "diffuse_color", text="")
        row = col.row()
        row.label(text="Specular:")
        row.prop(lamp1, "specular_color", text="")

        col = split.column()
        col.active = lamp1.enabled
        col.prop(lamp1, "direction", text="")


        split = column.split(percentage=0.1)

        if lamp2.enabled == True:
            split.prop(lamp2, "enabled", text="", icon='OUTLINER_OB_LAMP')
        else:
            split.prop(lamp2, "enabled", text="", icon='LAMP_DATA')

        col = split.column()
        col.active = lamp2.enabled
        row = col.row()
        row.label(text="Diffuse:")
        row.prop(lamp2, "diffuse_color", text="")
        row = col.row()
        row.label(text="Specular:")
        row.prop(lamp2, "specular_color", text="")

        col = split.column()
        col.active = lamp2.enabled
        col.prop(lamp2, "direction", text="")


        column.separator()
        column.separator()
        column.separator()

        col = column.column()

        col.prop(system, "use_weight_color_range", text="Custom Weight Paint Range")
        sub = col.column()
        sub.active = system.use_weight_color_range
        sub.template_color_ramp(system, "weight_color_range", expand=True)


class USERPREF_PT_theme(bpy.types.Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Themes"
    bl_region_type = 'WINDOW'
    bl_show_header = False

    def poll(self, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'THEMES')

    def draw(self, context):
        layout = self.layout

        theme = context.user_preferences.themes[0]

        split = layout.split(percentage=0.33)
        split.prop(theme, "active_theme", text="")

        layout.separator()

        split = layout.split()

        if theme.active_theme == 'VIEW_3D':
            v3d = theme.view_3d

            col = split.column()
            col.prop(v3d, "back")
            col.prop(v3d, "button")
            col.prop(v3d, "button_title")
            col.prop(v3d, "button_text")
            col.prop(v3d, "header")

            col = split.column()
            col.prop(v3d, "grid")
            col.prop(v3d, "wire")
            col.prop(v3d, "lamp", slider=True)
            col.prop(v3d, "editmesh_active", slider=True)

            col = split.column()
            col.prop(v3d, "object_selected")
            col.prop(v3d, "object_active")
            col.prop(v3d, "object_grouped")
            col.prop(v3d, "object_grouped_active")
            col.prop(v3d, "transform")

            col = split.column()
            col.prop(v3d, "vertex")
            col.prop(v3d, "face", slider=True)
            col.prop(v3d, "normal")
            col.prop(v3d, "bone_solid")
            col.prop(v3d, "bone_pose")
            #col.prop(v3d, "edge") Doesn't seem to work

        elif theme.active_theme == 'USER_INTERFACE':
            ui = theme.user_interface.wcol_regular
            layout.label(text="Regular:")

            row = layout.row()
            sub = row.column()
            sub.prop(ui, "outline")
            sub.prop(ui, "item", slider=True)
            sub = row.column()
            sub.prop(ui, "inner", slider=True)
            sub.prop(ui, "inner_sel", slider=True)
            sub = row.column()
            sub.prop(ui, "text")
            sub.prop(ui, "text_sel")
            sub = row.column()
            sub.prop(ui, "shaded")
            subsub = sub.column(align=True)
            subsub.active = ui.shaded
            subsub.prop(ui, "shadetop")
            subsub.prop(ui, "shadedown")

            layout.separator()

            ui = theme.user_interface.wcol_tool
            layout.label(text="Tool:")

            row = layout.row()
            sub = row.column()
            sub.prop(ui, "outline")
            sub.prop(ui, "item", slider=True)
            sub = row.column()
            sub.prop(ui, "inner", slider=True)
            sub.prop(ui, "inner_sel", slider=True)
            sub = row.column()
            sub.prop(ui, "text")
            sub.prop(ui, "text_sel")
            sub = row.column()
            sub.prop(ui, "shaded")
            subsub = sub.column(align=True)
            subsub.active = ui.shaded
            subsub.prop(ui, "shadetop")
            subsub.prop(ui, "shadedown")

            ui = theme.user_interface.wcol_radio
            layout.label(text="Radio Buttons:")

            row = layout.row()
            sub = row.column()
            sub.prop(ui, "outline")
            sub.prop(ui, "item", slider=True)
            sub = row.column()
            sub.prop(ui, "inner", slider=True)
            sub.prop(ui, "inner_sel", slider=True)
            sub = row.column()
            sub.prop(ui, "text")
            sub.prop(ui, "text_sel")
            sub = row.column()
            sub.prop(ui, "shaded")
            subsub = sub.column(align=True)
            subsub.active = ui.shaded
            subsub.prop(ui, "shadetop")
            subsub.prop(ui, "shadedown")

            ui = theme.user_interface.wcol_text
            layout.label(text="Text:")

            row = layout.row()
            sub = row.column()
            sub.prop(ui, "outline")
            sub.prop(ui, "item", slider=True)
            sub = row.column()
            sub.prop(ui, "inner", slider=True)
            sub.prop(ui, "inner_sel", slider=True)
            sub = row.column()
            sub.prop(ui, "text")
            sub.prop(ui, "text_sel")
            sub = row.column()
            sub.prop(ui, "shaded")
            subsub = sub.column(align=True)
            subsub.active = ui.shaded
            subsub.prop(ui, "shadetop")
            subsub.prop(ui, "shadedown")

            ui = theme.user_interface.wcol_option
            layout.label(text="Option:")

            row = layout.row()
            sub = row.column()
            sub.prop(ui, "outline")
            sub.prop(ui, "item", slider=True)
            sub = row.column()
            sub.prop(ui, "inner", slider=True)
            sub.prop(ui, "inner_sel", slider=True)
            sub = row.column()
            sub.prop(ui, "text")
            sub.prop(ui, "text_sel")
            sub = row.column()
            sub.prop(ui, "shaded")
            subsub = sub.column(align=True)
            subsub.active = ui.shaded
            subsub.prop(ui, "shadetop")
            subsub.prop(ui, "shadedown")

            ui = theme.user_interface.wcol_toggle
            layout.label(text="Toggle:")

            row = layout.row()
            sub = row.column()
            sub.prop(ui, "outline")
            sub.prop(ui, "item", slider=True)
            sub = row.column()
            sub.prop(ui, "inner", slider=True)
            sub.prop(ui, "inner_sel", slider=True)
            sub = row.column()
            sub.prop(ui, "text")
            sub.prop(ui, "text_sel")
            sub = row.column()
            sub.prop(ui, "shaded")
            subsub = sub.column(align=True)
            subsub.active = ui.shaded
            subsub.prop(ui, "shadetop")
            subsub.prop(ui, "shadedown")

            ui = theme.user_interface.wcol_num
            layout.label(text="Number Field:")

            row = layout.row()
            sub = row.column()
            sub.prop(ui, "outline")
            sub.prop(ui, "item", slider=True)
            sub = row.column()
            sub.prop(ui, "inner", slider=True)
            sub.prop(ui, "inner_sel", slider=True)
            sub = row.column()
            sub.prop(ui, "text")
            sub.prop(ui, "text_sel")
            sub = row.column()
            sub.prop(ui, "shaded")
            subsub = sub.column(align=True)
            subsub.active = ui.shaded
            subsub.prop(ui, "shadetop")
            subsub.prop(ui, "shadedown")

            ui = theme.user_interface.wcol_numslider
            layout.label(text="Value Slider:")

            row = layout.row()
            sub = row.column()
            sub.prop(ui, "outline")
            sub.prop(ui, "item", slider=True)
            sub = row.column()
            sub.prop(ui, "inner", slider=True)
            sub.prop(ui, "inner_sel", slider=True)
            sub = row.column()
            sub.prop(ui, "text")
            sub.prop(ui, "text_sel")
            sub = row.column()
            sub.prop(ui, "shaded")
            subsub = sub.column(align=True)
            subsub.active = ui.shaded
            subsub.prop(ui, "shadetop")
            subsub.prop(ui, "shadedown")

            ui = theme.user_interface.wcol_box
            layout.label(text="Box:")

            row = layout.row()
            sub = row.column()
            sub.prop(ui, "outline")
            sub.prop(ui, "item", slider=True)
            sub = row.column()
            sub.prop(ui, "inner", slider=True)
            sub.prop(ui, "inner_sel", slider=True)
            sub = row.column()
            sub.prop(ui, "text")
            sub.prop(ui, "text_sel")
            sub = row.column()
            sub.prop(ui, "shaded")
            subsub = sub.column(align=True)
            subsub.active = ui.shaded
            subsub.prop(ui, "shadetop")
            subsub.prop(ui, "shadedown")

            ui = theme.user_interface.wcol_menu
            layout.label(text="Menu:")

            row = layout.row()
            sub = row.column()
            sub.prop(ui, "outline")
            sub.prop(ui, "item", slider=True)
            sub = row.column()
            sub.prop(ui, "inner", slider=True)
            sub.prop(ui, "inner_sel", slider=True)
            sub = row.column()
            sub.prop(ui, "text")
            sub.prop(ui, "text_sel")
            sub = row.column()
            sub.prop(ui, "shaded")
            subsub = sub.column(align=True)
            subsub.active = ui.shaded
            subsub.prop(ui, "shadetop")
            subsub.prop(ui, "shadedown")

            ui = theme.user_interface.wcol_pulldown
            layout.label(text="Pulldown:")

            row = layout.row()
            sub = row.column()
            sub.prop(ui, "outline")
            sub.prop(ui, "item", slider=True)
            sub = row.column()
            sub.prop(ui, "inner", slider=True)
            sub.prop(ui, "inner_sel", slider=True)
            sub = row.column()
            sub.prop(ui, "text")
            sub.prop(ui, "text_sel")
            sub = row.column()
            sub.prop(ui, "shaded")
            subsub = sub.column(align=True)
            subsub.active = ui.shaded
            subsub.prop(ui, "shadetop")
            subsub.prop(ui, "shadedown")

            ui = theme.user_interface.wcol_menu_back
            layout.label(text="Menu Back:")

            row = layout.row()
            sub = row.column()
            sub.prop(ui, "outline")
            sub.prop(ui, "item", slider=True)
            sub = row.column()
            sub.prop(ui, "inner", slider=True)
            sub.prop(ui, "inner_sel", slider=True)
            sub = row.column()
            sub.prop(ui, "text")
            sub.prop(ui, "text_sel")
            sub = row.column()
            sub.prop(ui, "shaded")
            subsub = sub.column(align=True)
            subsub.active = ui.shaded
            subsub.prop(ui, "shadetop")
            subsub.prop(ui, "shadedown")

            ui = theme.user_interface.wcol_menu_item
            layout.label(text="Menu Item:")

            row = layout.row()
            sub = row.column()
            sub.prop(ui, "outline")
            sub.prop(ui, "item", slider=True)
            sub = row.column()
            sub.prop(ui, "inner", slider=True)
            sub.prop(ui, "inner_sel", slider=True)
            sub = row.column()
            sub.prop(ui, "text")
            sub.prop(ui, "text_sel")
            sub = row.column()
            sub.prop(ui, "shaded")
            subsub = sub.column(align=True)
            subsub.active = ui.shaded
            subsub.prop(ui, "shadetop")
            subsub.prop(ui, "shadedown")

            ui = theme.user_interface.wcol_scroll
            layout.label(text="Scroll Bar:")

            row = layout.row()
            sub = row.column()
            sub.prop(ui, "outline")
            sub.prop(ui, "item", slider=True)
            sub = row.column()
            sub.prop(ui, "inner", slider=True)
            sub.prop(ui, "inner_sel", slider=True)
            sub = row.column()
            sub.prop(ui, "text")
            sub.prop(ui, "text_sel")
            sub = row.column()
            sub.prop(ui, "shaded")
            subsub = sub.column(align=True)
            subsub.active = ui.shaded
            subsub.prop(ui, "shadetop")
            subsub.prop(ui, "shadedown")

            ui = theme.user_interface.wcol_list_item
            layout.label(text="List Item:")

            row = layout.row()
            sub = row.column()
            sub.prop(ui, "outline")
            sub.prop(ui, "item", slider=True)
            sub = row.column()
            sub.prop(ui, "inner", slider=True)
            sub.prop(ui, "inner_sel", slider=True)
            sub = row.column()
            sub.prop(ui, "text")
            sub.prop(ui, "text_sel")
            sub = row.column()
            sub.prop(ui, "shaded")
            subsub = sub.column(align=True)
            subsub.active = ui.shaded
            subsub.prop(ui, "shadetop")
            subsub.prop(ui, "shadedown")

            ui = theme.user_interface.wcol_state
            layout.label(text="State:")

            row = layout.row()
            sub = row.column()
            sub.prop(ui, "inner_anim")
            sub.prop(ui, "inner_anim_sel")
            sub = row.column()
            sub.prop(ui, "inner_driven")
            sub.prop(ui, "inner_driven_sel")
            sub = row.column()
            sub.prop(ui, "inner_key")
            sub.prop(ui, "inner_key_sel")
            sub = row.column()
            sub.prop(ui, "blend")

            ui = theme.user_interface
            layout.separator()

            sub = layout.row()
            sub.prop(ui, "icon_file")

            layout.separator()
            layout.separator()


        elif theme.active_theme == 'GRAPH_EDITOR':
            graph = theme.graph_editor

            col = split.column()
            col.prop(graph, "back")
            col.prop(graph, "button")
            col.prop(graph, "button_title")
            col.prop(graph, "button_text")

            col = split.column()
            col.prop(graph, "header")
            col.prop(graph, "grid")
            col.prop(graph, "list")
            col.prop(graph, "channel_group")

            col = split.column()
            col.prop(graph, "active_channels_group")
            col.prop(graph, "dopesheet_channel")
            col.prop(graph, "dopesheet_subchannel")
            col.prop(graph, "vertex")

            col = split.column()
            col.prop(graph, "current_frame")
            col.prop(graph, "handle_vertex")
            col.prop(graph, "handle_vertex_select")
            col.separator()
            col.prop(graph, "handle_vertex_size")

        elif theme.active_theme == 'FILE_BROWSER':
            file_browse = theme.file_browser

            col = split.column()
            col.prop(file_browse, "back")
            col.prop(file_browse, "text")
            col.prop(file_browse, "text_hi")

            col = split.column()
            col.prop(file_browse, "header")
            col.prop(file_browse, "list")

            col = split.column()
            col.prop(file_browse, "selected_file")
            col.prop(file_browse, "tiles")

            col = split.column()
            col.prop(file_browse, "active_file")
            col.prop(file_browse, "active_file_text")

        elif theme.active_theme == 'NLA_EDITOR':
            nla = theme.nla_editor

            col = split.column()
            col.prop(nla, "back")
            col.prop(nla, "button")
            col.prop(nla, "button_title")

            col = split.column()
            col.prop(nla, "button_text")
            col.prop(nla, "text")
            col.prop(nla, "header")

            col = split.column()
            col.prop(nla, "grid")
            col.prop(nla, "bars")
            col.prop(nla, "bars_selected")

            col = split.column()
            col.prop(nla, "strips")
            col.prop(nla, "strips_selected")
            col.prop(nla, "current_frame")

        elif theme.active_theme == 'DOPESHEET_EDITOR':
            dope = theme.dopesheet_editor

            col = split.column()
            col.prop(dope, "back")
            col.prop(dope, "list")
            col.prop(dope, "text")
            col.prop(dope, "header")

            col = split.column()
            col.prop(dope, "grid")
            col.prop(dope, "channels")
            col.prop(dope, "channels_selected")
            col.prop(dope, "channel_group")

            col = split.column()
            col.prop(dope, "active_channels_group")
            col.prop(dope, "long_key")
            col.prop(dope, "long_key_selected")

            col = split.column()
            col.prop(dope, "current_frame")
            col.prop(dope, "dopesheet_channel")
            col.prop(dope, "dopesheet_subchannel")

        elif theme.active_theme == 'IMAGE_EDITOR':
            image = theme.image_editor

            col = split.column()
            col.prop(image, "back")
            col.prop(image, "button")

            col = split.column()
            col.prop(image, "button_title")
            col.prop(image, "button_text")

            col = split.column()
            col.prop(image, "header")

            col = split.column()
            col.prop(image, "editmesh_active", slider=True)

        elif theme.active_theme == 'SEQUENCE_EDITOR':
            seq = theme.sequence_editor

            col = split.column()
            col.prop(seq, "back")
            col.prop(seq, "button")
            col.prop(seq, "button_title")
            col.prop(seq, "button_text")
            col.prop(seq, "text")

            col = split.column()
            col.prop(seq, "header")
            col.prop(seq, "grid")
            col.prop(seq, "movie_strip")
            col.prop(seq, "image_strip")
            col.prop(seq, "scene_strip")

            col = split.column()
            col.prop(seq, "audio_strip")
            col.prop(seq, "effect_strip")
            col.prop(seq, "plugin_strip")
            col.prop(seq, "transition_strip")

            col = split.column()
            col.prop(seq, "meta_strip")
            col.prop(seq, "current_frame")
            col.prop(seq, "keyframe")
            col.prop(seq, "draw_action")

        elif theme.active_theme == 'PROPERTIES':
            prop = theme.properties

            col = split.column()
            col.prop(prop, "back")

            col = split.column()
            col.prop(prop, "title")

            col = split.column()
            col.prop(prop, "text")

            col = split.column()
            col.prop(prop, "header")

        elif theme.active_theme == 'TEXT_EDITOR':
            text = theme.text_editor

            col = split.column()
            col.prop(text, "back")
            col.prop(text, "button")
            col.prop(text, "button_title")
            col.prop(text, "button_text")

            col = split.column()
            col.prop(text, "text")
            col.prop(text, "text_hi")
            col.prop(text, "header")
            col.prop(text, "line_numbers_background")

            col = split.column()
            col.prop(text, "selected_text")
            col.prop(text, "cursor")
            col.prop(text, "syntax_builtin")
            col.prop(text, "syntax_special")

            col = split.column()
            col.prop(text, "syntax_comment")
            col.prop(text, "syntax_string")
            col.prop(text, "syntax_numbers")

        elif theme.active_theme == 'TIMELINE':
            time = theme.timeline

            col = split.column()
            col.prop(time, "back")
            col.prop(time, "text")

            col = split.column()
            col.prop(time, "header")

            col = split.column()
            col.prop(time, "grid")

            col = split.column()
            col.prop(time, "current_frame")

        elif theme.active_theme == 'NODE_EDITOR':
            node = theme.node_editor

            col = split.column()
            col.prop(node, "back")
            col.prop(node, "button")
            col.prop(node, "button_title")
            col.prop(node, "button_text")

            col = split.column()
            col.prop(node, "text")
            col.prop(node, "text_hi")
            col.prop(node, "header")
            col.prop(node, "wires")

            col = split.column()
            col.prop(node, "wire_select")
            col.prop(node, "selected_text")
            col.prop(node, "node_backdrop", slider=True)
            col.prop(node, "in_out_node")

            col = split.column()
            col.prop(node, "converter_node")
            col.prop(node, "operator_node")
            col.prop(node, "group_node")

        elif theme.active_theme == 'LOGIC_EDITOR':
            logic = theme.logic_editor

            col = split.column()
            col.prop(logic, "back")
            col.prop(logic, "button")

            col = split.column()
            col.prop(logic, "button_title")
            col.prop(logic, "button_text")

            col = split.column()
            col.prop(logic, "text")
            col.prop(logic, "header")

            col = split.column()
            col.prop(logic, "panel")

        elif theme.active_theme == 'OUTLINER':
            out = theme.outliner

            col = split.column()
            col.prop(out, "back")

            col = split.column()
            col.prop(out, "text")

            col = split.column()
            col.prop(out, "text_hi")

            col = split.column()
            col.prop(out, "header")

        elif theme.active_theme == 'INFO':
            info = theme.info

            col = split.column()
            col.prop(info, "back")

            col = split.column()
            col.prop(info, "header")

            col = split.column()
            col.prop(info, "header_text")

            col = split.column()

        elif theme.active_theme == 'USER_PREFERENCES':
            prefs = theme.user_preferences

            col = split.column()
            col.prop(prefs, "back")

            col = split.column()
            col.prop(prefs, "text")

            col = split.column()
            col.prop(prefs, "header")

            col = split.column()
            col.prop(prefs, "header_text")


class USERPREF_PT_file(bpy.types.Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Files"
    bl_region_type = 'WINDOW'
    bl_show_header = False

    def poll(self, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'FILES')

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences
        paths = userpref.filepaths

        split = layout.split(percentage=0.7)

        col = split.column()
        col.label(text="File Paths:")

        colsplit = col.split(percentage=0.95)
        col1 = colsplit.split(percentage=0.3)

        sub = col1.column()
        sub.label(text="Fonts:")
        sub.label(text="Textures:")
        sub.label(text="Texture Plugins:")
        sub.label(text="Sequence Plugins:")
        sub.label(text="Render Output:")
        sub.label(text="Scripts:")
        sub.label(text="Sounds:")
        sub.label(text="Temp:")
        sub.label(text="Animation Player:")

        sub = col1.column()
        sub.prop(paths, "fonts_directory", text="")
        sub.prop(paths, "textures_directory", text="")
        sub.prop(paths, "texture_plugin_directory", text="")
        sub.prop(paths, "sequence_plugin_directory", text="")
        sub.prop(paths, "render_output_directory", text="")
        sub.prop(paths, "python_scripts_directory", text="")
        sub.prop(paths, "sounds_directory", text="")
        sub.prop(paths, "temporary_directory", text="")
        subsplit = sub.split(percentage=0.3)
        subsplit.prop(paths, "animation_player_preset", text="")
        subsplit.prop(paths, "animation_player", text="")

        col = split.column()
        col.label(text="Save & Load:")
        col.prop(paths, "use_relative_paths")
        col.prop(paths, "compress_file")
        col.prop(paths, "load_ui")
        col.prop(paths, "filter_file_extensions")
        col.prop(paths, "hide_dot_files_datablocks")

        col.separator()
        col.separator()

        col.label(text="Auto Save:")
        col.prop(paths, "save_version")
        col.prop(paths, "recent_files")
        col.prop(paths, "save_preview_images")
        col.prop(paths, "auto_save_temporary_files")
        sub = col.column()
        sub.enabled = paths.auto_save_temporary_files
        sub.prop(paths, "auto_save_time", text="Timer (mins)")


class USERPREF_PT_input(bpy.types.Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Input"
    bl_region_type = 'WINDOW'
    bl_show_header = False

    def poll(self, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'INPUT')

    def draw_entry(self, kc, entry, col, level=0):
        idname, spaceid, regionid, children = entry

        km = kc.find_keymap(idname, space_type=spaceid, region_type=regionid)

        if km:
            self.draw_km(kc, km, children, col, level)

    def indented_layout(self, layout, level):
        indentpx = 16
        if level == 0:
            level = 0.0001   # Tweak so that a percentage of 0 won't split by half
        indent = level * indentpx / bpy.context.region.width

        split = layout.split(percentage=indent)
        col = split.column()
        col = split.column()
        return col

    def draw_km(self, kc, km, children, layout, level):
        km = km.active()

        layout.set_context_pointer("keymap", km)

        col = self.indented_layout(layout, level)

        row = col.row()
        row.prop(km, "children_expanded", text="", no_bg=True)
        row.label(text=km.name)

        row.label()
        row.label()

        if km.user_defined:
            row.operator("wm.keymap_restore", text="Restore")
        else:
            row.operator("wm.keymap_edit", text="Edit")

        if km.children_expanded:
            if children:
                # Put the Parent key map's entries in a 'global' sub-category
                # equal in hierarchy to the other children categories
                subcol = self.indented_layout(col, level + 1)
                subrow = subcol.row()
                subrow.prop(km, "items_expanded", text="", no_bg=True)
                subrow.label(text="%s (Global)" % km.name)
            else:
                km.items_expanded = True

            # Key Map items
            if km.items_expanded:
                for kmi in km.items:
                    self.draw_kmi(kc, km, kmi, col, level + 1)

                # "Add New" at end of keymap item list
                col = self.indented_layout(col, level + 1)
                subcol = col.split(percentage=0.2).column()
                subcol.active = km.user_defined
                subcol.operator("wm.keyitem_add", text="Add New", icon='ZOOMIN')

            col.separator()

            # Child key maps
            if children:
                subcol = col.column()
                row = subcol.row()

                for entry in children:
                    self.draw_entry(kc, entry, col, level + 1)

    def draw_kmi(self, kc, km, kmi, layout, level):
        layout.set_context_pointer("keyitem", kmi)

        col = self.indented_layout(layout, level)

        col.enabled = km.user_defined

        if km.user_defined:
            col = col.column(align=True)
            box = col.box()
        else:
            box = col.column()

        split = box.split(percentage=0.4)

        # header bar
        row = split.row()
        row.prop(kmi, "expanded", text="", no_bg=True)
        row.prop(kmi, "active", text="", no_bg=True)

        if km.modal:
            row.prop(kmi, "propvalue", text="")
        else:
            row.label(text=kmi.name)

        row = split.row()
        row.prop(kmi, "map_type", text="")
        if kmi.map_type == 'KEYBOARD':
            row.prop(kmi, "type", text="", full_event=True)
        elif kmi.map_type == 'MOUSE':
            row.prop(kmi, "type", text="", full_event=True)
        elif kmi.map_type == 'TWEAK':
            subrow = row.row()
            subrow.prop(kmi, "type", text="")
            subrow.prop(kmi, "value", text="")
        elif kmi.map_type == 'TIMER':
            row.prop(kmi, "type", text="")
        else:
            row.label()

        row.operator("wm.keyitem_restore", text="", icon='BACK')
        row.operator("wm.keyitem_remove", text="", icon='X')

        # Expanded, additional event settings
        if kmi.expanded:
            box = col.box()

            if kmi.map_type not in ('TEXTINPUT', 'TIMER'):
                split = box.split(percentage=0.4)
                sub = split.row()

                if km.modal:
                    sub.prop(kmi, "propvalue", text="")
                else:
                    sub.prop(kmi, "idname", text="")

                sub = split.column()
                subrow = sub.row(align=True)

                if kmi.map_type == 'KEYBOARD':
                    subrow.prop(kmi, "type", text="", event=True)
                    subrow.prop(kmi, "value", text="")
                elif kmi.map_type == 'MOUSE':
                    subrow.prop(kmi, "type", text="")
                    subrow.prop(kmi, "value", text="")

                subrow = sub.row()
                subrow.scale_x = 0.75
                subrow.prop(kmi, "any")
                subrow.prop(kmi, "shift")
                subrow.prop(kmi, "ctrl")
                subrow.prop(kmi, "alt")
                subrow.prop(kmi, "oskey", text="Cmd")
                subrow.prop(kmi, "key_modifier", text="", event=True)

            # Operator properties
            props = kmi.properties
            if props is not None:
                box.separator()
                flow = box.column_flow(columns=2)
                for pname in dir(props):
                    if not props.is_property_hidden(pname):
                        flow.prop(props, pname)

            # Modal key maps attached to this operator
            if not km.modal:
                kmm = kc.find_keymap_modal(kmi.idname)
                if kmm:
                    self.draw_km(kc, kmm, None, layout, level + 1)

    def draw_input_prefs(self, inputs, layout):
        # General settings
        row = layout.row()
        col = row.column()

        sub = col.column()
        sub.label(text="Mouse:")
        sub1 = sub.column()
        sub1.enabled = (inputs.select_mouse == 'RIGHT')
        sub1.prop(inputs, "emulate_3_button_mouse")
        sub.prop(inputs, "continuous_mouse")

        sub.label(text="Select With:")
        sub.row().prop(inputs, "select_mouse", expand=True)

        sub = col.column()
        sub.label(text="Double Click:")
        sub.prop(inputs, "double_click_time", text="Speed")

        sub.separator()

        sub.prop(inputs, "emulate_numpad")

        sub.separator()

        sub.label(text="Orbit Style:")
        sub.row().prop(inputs, "view_rotation", expand=True)

        sub.label(text="Zoom Style:")
        sub.row().prop(inputs, "viewport_zoom_style", expand=True)
        if inputs.viewport_zoom_style == 'DOLLY':
            sub.row().prop(inputs, "zoom_axis", expand=True)
            sub.prop(inputs, "invert_zoom_direction")

        #sub.prop(inputs, "use_middle_mouse_paste")

        #col.separator()

        #sub = col.column()
        #sub.label(text="Mouse Wheel:")
        #sub.prop(view, "wheel_scroll_lines", text="Scroll Lines")

        col.separator()

        sub = col.column()
        sub.label(text="NDOF Device:")
        sub.prop(inputs, "ndof_pan_speed", text="Pan Speed")
        sub.prop(inputs, "ndof_rotate_speed", text="Orbit Speed")

        row.separator()

    def draw_filtered(self, kc, layout):
        filter = kc.filter.lower()

        for km in kc.keymaps:
            km = km.active()

            filtered_items = [kmi for kmi in km.items if filter in kmi.name.lower()]

            if len(filtered_items) != 0:
                layout.set_context_pointer("keymap", km)
                col = layout.column()

                row = col.row()
                row.label(text=km.name, icon="DOT")

                row.label()
                row.label()

                if km.user_defined:
                    row.operator("wm.keymap_restore", text="Restore")
                else:
                    row.operator("wm.keymap_edit", text="Edit")

                for kmi in filtered_items:
                    self.draw_kmi(kc, km, kmi, col, 1)

                # "Add New" at end of keymap item list
                col = self.indented_layout(layout, 1)
                subcol = col.split(percentage=0.2).column()
                subcol.active = km.user_defined
                subcol.operator("wm.keyitem_add", text="Add New", icon='ZOOMIN')

    def draw_hierarchy(self, defkc, layout):
        for entry in KM_HIERARCHY:
            self.draw_entry(defkc, entry, layout)

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences
        wm = context.manager

        inputs = userpref.inputs

        split = layout.split(percentage=0.25)

        # Input settings
        self.draw_input_prefs(inputs, split)

        # Keymap Settings
        col = split.column()
        # kc = wm.active_keyconfig
        kc = wm.default_keyconfig

        sub = col.column()

        subsplit = sub.split()
        subcol = subsplit.column()
        subcol.prop_object(wm, "active_keyconfig", wm, "keyconfigs", text="Configuration:")

        subcol = subsplit.column()
        subcol.prop(kc, "filter", icon="VIEWZOOM")

        col.separator()

        if kc.filter != "":
            self.draw_filtered(kc, col)
        else:
            self.draw_hierarchy(kc, col)

bpy.types.register(USERPREF_HT_header)
bpy.types.register(USERPREF_PT_tabs)
bpy.types.register(USERPREF_PT_interface)
bpy.types.register(USERPREF_PT_theme)
bpy.types.register(USERPREF_PT_edit)
bpy.types.register(USERPREF_PT_system)
bpy.types.register(USERPREF_PT_file)
bpy.types.register(USERPREF_PT_input)

from bpy.props import *


class WM_OT_keyconfig_test(bpy.types.Operator):
    "Test keyconfig for conflicts."
    bl_idname = "wm.keyconfig_test"
    bl_label = "Test Key Configuration for Conflicts"

    def testEntry(self, kc, entry, src=None, parent=None):
        result = False

        def kmistr(kmi):
            if km.modal:
                s = ["kmi = km.add_modal_item(\'%s\', \'%s\', \'%s\'" % (kmi.propvalue, kmi.type, kmi.value)]
            else:
                s = ["kmi = km.add_item(\'%s\', \'%s\', \'%s\'" % (kmi.idname, kmi.type, kmi.value)]

            if kmi.any:
                s.append(", any=True")
            else:
                if kmi.shift:
                    s.append(", shift=True")
                if kmi.ctrl:
                    s.append(", ctrl=True")
                if kmi.alt:
                    s.append(", alt=True")
                if kmi.oskey:
                    s.append(", oskey=True")
            if kmi.key_modifier and kmi.key_modifier != 'NONE':
                s.append(", key_modifier=\'%s\'" % kmi.key_modifier)

            s.append(")\n")

            props = kmi.properties

            if props is not None:
                for pname in dir(props):
                    if props.is_property_set(pname) and not props.is_property_hidden(pname):
                        value = eval("props.%s" % pname)
                        value = _string_value(value)
                        if value != "":
                            s.append("kmi.properties.%s = %s\n" % (pname, value))

            return "".join(s).strip()

        idname, spaceid, regionid, children = entry

        km = kc.find_keymap(idname, space_type=spaceid, region_type=regionid)

        if km:
            km = km.active()

            if src:
                for item in km.items:
                    if src.compare(item):
                        print("===========")
                        print(parent.name)
                        print(kmistr(src))
                        print(km.name)
                        print(kmistr(item))
                        result = True

                for child in children:
                    if self.testEntry(kc, child, src, parent):
                        result = True
            else:
                for i in range(len(km.items)):
                    src = km.items[i]

                    for child in children:
                        if self.testEntry(kc, child, src, km):
                            result = True

                    for j in range(len(km.items) - i - 1):
                        item = km.items[j + i + 1]
                        if src.compare(item):
                            print("===========")
                            print(km.name)
                            print(kmistr(src))
                            print(kmistr(item))
                            result = True

                for child in children:
                    if self.testEntry(kc, child):
                        result = True

        return result

    def testConfig(self, kc):
        result = False
        for entry in KM_HIERARCHY:
            if self.testEntry(kc, entry):
                result = True
        return result

    def execute(self, context):
        wm = context.manager
        kc = wm.default_keyconfig

        if self.testConfig(kc):
            print("CONFLICT")

        return {'FINISHED'}


def _string_value(value):
    result = ""
    if isinstance(value, str):
        if value != "":
            result = "\'%s\'" % value
    elif isinstance(value, bool):
        if value:
            result = "True"
        else:
            result = "False"
    elif isinstance(value, float):
        result = "%.10f" % value
    elif isinstance(value, int):
        result = "%d" % value
    elif getattr(value, '__len__', False):
        if len(value):
            result = "["
            for i in range(0, len(value)):
                result += _string_value(value[i])
                if i != len(value)-1:
                    result += ", "
            result += "]"
    else:
        print("Export key configuration: can't write ", value)

    return result


class WM_OT_keyconfig_export(bpy.types.Operator):
    "Export key configuration to a python script."
    bl_idname = "wm.keyconfig_export"
    bl_label = "Export Key Configuration..."

    path = bpy.props.StringProperty(name="File Path", description="File path to write file to.")

    def execute(self, context):
        if not self.properties.path:
            raise Exception("File path not set.")

        f = open(self.properties.path, "w")
        if not f:
            raise Exception("Could not open file.")

        wm = context.manager
        kc = wm.active_keyconfig

        f.write('# Configuration %s\n' % kc.name)

        f.write("wm = bpy.data.windowmanagers[0]\n")
        f.write("kc = wm.add_keyconfig(\'%s\')\n\n" % kc.name)

        for km in kc.keymaps:
            km = km.active()
            f.write("# Map %s\n" % km.name)
            f.write("km = kc.add_keymap(\'%s\', space_type=\'%s\', region_type=\'%s\', modal=%s)\n\n" % (km.name, km.space_type, km.region_type, km.modal))
            for kmi in km.items:
                if km.modal:
                    f.write("kmi = km.add_modal_item(\'%s\', \'%s\', \'%s\'" % (kmi.propvalue, kmi.type, kmi.value))
                else:
                    f.write("kmi = km.add_item(\'%s\', \'%s\', \'%s\'" % (kmi.idname, kmi.type, kmi.value))
                if kmi.any:
                    f.write(", any=True")
                else:
                    if kmi.shift:
                        f.write(", shift=True")
                    if kmi.ctrl:
                        f.write(", ctrl=True")
                    if kmi.alt:
                        f.write(", alt=True")
                    if kmi.oskey:
                        f.write(", oskey=True")
                if kmi.key_modifier and kmi.key_modifier != 'NONE':
                    f.write(", key_modifier=\'%s\'" % kmi.key_modifier)
                f.write(")\n")

                props = kmi.properties

                if props is not None:
                    for pname in dir(props):
                        if props.is_property_set(pname) and not props.is_property_hidden(pname):
                            value = eval("props.%s" % pname)
                            value = _string_value(value)
                            if value != "":
                                f.write("kmi.properties.%s = %s\n" % (pname, value))

            f.write("\n")

        f.close()

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.manager
        wm.add_fileselect(self)
        return {'RUNNING_MODAL'}


class WM_OT_keymap_edit(bpy.types.Operator):
    "Edit key map."
    bl_idname = "wm.keymap_edit"
    bl_label = "Edit Key Map"

    def execute(self, context):
        wm = context.manager
        km = context.keymap
        km.copy_to_user()
        return {'FINISHED'}


class WM_OT_keymap_restore(bpy.types.Operator):
    "Restore key map(s)."
    bl_idname = "wm.keymap_restore"
    bl_label = "Restore Key Map(s)"

    all = BoolProperty(attr="all", name="All Keymaps", description="Restore all keymaps to default.")

    def execute(self, context):
        wm = context.manager

        if self.properties.all:
            for km in wm.default_keyconfig.keymaps:
                km.restore_to_default()
        else:
            km = context.keymap
            km.restore_to_default()

        return {'FINISHED'}


class WM_OT_keyitem_restore(bpy.types.Operator):
    "Restore key map item."
    bl_idname = "wm.keyitem_restore"
    bl_label = "Restore Key Map Item"

    def poll(self, context):
        kmi = context.keyitem
        km = context.keymap
        return km and kmi and kmi.id != 0

    def execute(self, context):
        wm = context.manager
        kmi = context.keyitem
        km = context.keymap

        km.restore_item_to_default(kmi)

        return {'FINISHED'}


class WM_OT_keyitem_add(bpy.types.Operator):
    "Add key map item."
    bl_idname = "wm.keyitem_add"
    bl_label = "Add Key Map Item"

    def execute(self, context):
        wm = context.manager
        km = context.keymap
        kc = wm.default_keyconfig

        if km.modal:
            km.add_modal_item("", 'A', 'PRESS') # kmi
        else:
            km.add_item("none", 'A', 'PRESS') # kmi

        # clear filter and expand keymap so we can see the newly added item
        if kc.filter != '':
            kc.filter = ''
            km.items_expanded = True
            km.children_expanded = True

        return {'FINISHED'}


class WM_OT_keyitem_remove(bpy.types.Operator):
    "Remove key map item."
    bl_idname = "wm.keyitem_remove"
    bl_label = "Remove Key Map Item"

    def execute(self, context):
        wm = context.manager
        kmi = context.keyitem
        km = context.keymap
        km.remove_item(kmi)
        return {'FINISHED'}

bpy.types.register(WM_OT_keyconfig_export)
bpy.types.register(WM_OT_keyconfig_test)
bpy.types.register(WM_OT_keymap_edit)
bpy.types.register(WM_OT_keymap_restore)
bpy.types.register(WM_OT_keyitem_add)
bpy.types.register(WM_OT_keyitem_remove)
bpy.types.register(WM_OT_keyitem_restore)
