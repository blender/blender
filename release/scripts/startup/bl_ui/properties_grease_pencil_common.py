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
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>


from bpy.types import Menu, UIList


def gpencil_stroke_placement_settings(context, layout):
    if context.space_data.type == 'VIEW_3D':
        propname = "gpencil_stroke_placement_view3d"
    elif context.space_data.type == 'SEQUENCE_EDITOR':
        propname = "gpencil_stroke_placement_sequencer_preview"
    elif context.space_data.type == 'IMAGE_EDITOR':
        propname = "gpencil_stroke_placement_image_editor"
    else:
        propname = "gpencil_stroke_placement_view2d"

    ts = context.tool_settings

    col = layout.column(align=True)

    col.label(text="Stroke Placement:")

    row = col.row(align=True)
    row.prop_enum(ts, propname, 'VIEW')
    row.prop_enum(ts, propname, 'CURSOR')

    if context.space_data.type == 'VIEW_3D':
        row = col.row(align=True)
        row.prop_enum(ts, propname, 'SURFACE')
        row.prop_enum(ts, propname, 'STROKE')

        row = col.row(align=False)
        row.active = getattr(ts, propname) in {'SURFACE', 'STROKE'}
        row.prop(ts, "use_gpencil_stroke_endpoints")


class GreasePencilDrawingToolsPanel:
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    bl_label = "Grease Pencil"
    bl_category = "Grease Pencil"
    bl_region_type = 'TOOLS'

    @staticmethod
    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)

        col.label(text="Draw:")
        row = col.row(align=True)
        row.operator("gpencil.draw", icon='GREASEPENCIL', text="Draw").mode = 'DRAW'
        row.operator("gpencil.draw", icon='FORCE_CURVE', text="Erase").mode = 'ERASER'  # XXX: Needs a dedicated icon

        row = col.row(align=True)
        row.operator("gpencil.draw", icon='LINE_DATA', text="Line").mode = 'DRAW_STRAIGHT'
        row.operator("gpencil.draw", icon='MESH_DATA', text="Poly").mode = 'DRAW_POLY'

        sub = col.column(align=True)
        sub.prop(context.tool_settings, "use_gpencil_additive_drawing", text="Additive Drawing")
        sub.prop(context.tool_settings, "use_gpencil_continuous_drawing", text="Continuous Drawing")

        col.separator()
        col.separator()

        if context.space_data.type in {'VIEW_3D', 'CLIP_EDITOR'}:
            col.separator()
            col.label("Data Source:")
            row = col.row(align=True)
            if context.space_data.type == 'VIEW_3D':
                row.prop(context.tool_settings, "grease_pencil_source", expand=True)
            elif context.space_data.type == 'CLIP_EDITOR':
                row.prop(context.space_data, "grease_pencil_source", expand=True)

        col.separator()
        col.separator()

        gpencil_stroke_placement_settings(context, col)

        gpd = context.gpencil_data

        if gpd:
            layout.separator()
            layout.separator()

            col = layout.column(align=True)
            col.prop(gpd, "use_stroke_edit_mode", text="Enable Editing", icon='EDIT', toggle=True)

        if context.space_data.type == 'VIEW_3D':
            col.separator()
            col.separator()

            col.label(text="Tools:")
            col.operator("gpencil.convert", text="Convert...")
            col.operator("view3d.ruler")


class GreasePencilStrokeEditPanel:
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    bl_label = "Edit Strokes"
    bl_category = "Grease Pencil"
    bl_region_type = 'TOOLS'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if context.gpencil_data is None:
            return False

        gpd = context.gpencil_data
        return bool(context.editable_gpencil_strokes) and bool(gpd.use_stroke_edit_mode)

    @staticmethod
    def draw(self, context):
        layout = self.layout

        layout.label(text="Select:")
        col = layout.column(align=True)
        col.operator("gpencil.select_all", text="Select All")
        col.operator("gpencil.select_border")
        col.operator("gpencil.select_circle")

        layout.separator()

        col = layout.column(align=True)
        col.operator("gpencil.select_linked")
        col.operator("gpencil.select_more")
        col.operator("gpencil.select_less")

        layout.separator()

        layout.label(text="Edit:")
        row = layout.row(align=True)
        row.operator("gpencil.copy", text="Copy")
        row.operator("gpencil.paste", text="Paste")

        col = layout.column(align=True)
        col.operator("gpencil.delete", text="Delete")
        col.operator("gpencil.duplicate_move", text="Duplicate")
        col.operator("transform.mirror", text="Mirror")

        layout.separator()

        col = layout.column(align=True)
        col.operator("transform.translate")                # icon='MAN_TRANS'
        col.operator("transform.rotate")                   # icon='MAN_ROT'
        col.operator("transform.resize", text="Scale")     # icon='MAN_SCALE'

        layout.separator()

        col = layout.column(align=True)
        col.operator("transform.bend", text="Bend")
        col.operator("transform.shear", text="Shear")
        col.operator("transform.tosphere", text="To Sphere")


class GreasePencilStrokeSculptPanel:
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    bl_label = "Sculpt Strokes"
    bl_category = "Grease Pencil"
    bl_region_type = 'TOOLS'

    @classmethod
    def poll(cls, context):
        if context.gpencil_data is None:
            return False

        gpd = context.gpencil_data
        return bool(context.editable_gpencil_strokes) and bool(gpd.use_stroke_edit_mode)

    @staticmethod
    def draw(self, context):
        layout = self.layout

        settings = context.tool_settings.gpencil_sculpt
        tool = settings.tool
        brush = settings.brush

        layout.column().prop(settings, "tool", expand=True)

        col = layout.column()
        col.prop(brush, "size", slider=True)
        row = col.row(align=True)
        row.prop(brush, "strength", slider=True)
        row.prop(brush, "use_pressure_strength", text="")
        col.prop(brush, "use_falloff")

        layout.separator()

        if settings.tool == 'THICKNESS':
            layout.row().prop(brush, "direction", expand=True)
        elif settings.tool == 'PINCH':
            row = layout.row(align=True)
            row.prop_enum(brush, "direction", 'ADD', text="Pinch")
            row.prop_enum(brush, "direction", 'SUBTRACT', text="Inflate")
        elif settings.tool == 'TWIST':
            row = layout.row(align=True)
            row.prop_enum(brush, "direction", 'SUBTRACT', text="CW")
            row.prop_enum(brush, "direction", 'ADD', text="CCW")

        layout.separator()
        layout.prop(settings, "use_select_mask")

        if settings.tool == 'SMOOTH':
            layout.prop(brush, "affect_pressure")


###############################

class GPENCIL_PIE_tool_palette(Menu):
    """A pie menu for quick access to Grease Pencil tools"""
    bl_label = "Grease Pencil Tools"

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()
        gpd = context.gpencil_data

        # W - Drawing Types
        col = pie.column()
        col.operator("gpencil.draw", text="Draw", icon='GREASEPENCIL').mode = 'DRAW'
        col.operator("gpencil.draw", text="Straight Lines", icon='LINE_DATA').mode = 'DRAW_STRAIGHT'
        col.operator("gpencil.draw", text="Poly", icon='MESH_DATA').mode = 'DRAW_POLY'

        # E - Eraser
        # XXX: needs a dedicated icon...
        col = pie.column()
        col.operator("gpencil.draw", text="Eraser", icon='FORCE_CURVE').mode = 'ERASER'

        # E - "Settings" Palette is included here too, since it needs to be in a stable position...
        if gpd and gpd.layers.active:
            col.separator()
            col.operator("wm.call_menu_pie", text="Settings...", icon='SCRIPTWIN').name = "GPENCIL_PIE_settings_palette"

        # Editing tools
        if gpd:
            if gpd.use_stroke_edit_mode and context.editable_gpencil_strokes:
                # S - Exit Edit Mode
                pie.operator("gpencil.editmode_toggle", text="Exit Edit Mode", icon='EDIT')

                # N - Transforms
                col = pie.column()
                row = col.row(align=True)
                row.operator("transform.translate", icon='MAN_TRANS')
                row.operator("transform.rotate", icon='MAN_ROT')
                row.operator("transform.resize", text="Scale", icon='MAN_SCALE')
                row = col.row(align=True)
                row.label("Proportional Edit:")
                row.prop(context.tool_settings, "proportional_edit", text="", icon_only=True)
                row.prop(context.tool_settings, "proportional_edit_falloff", text="", icon_only=True)

                # NW - Select (Non-Modal)
                col = pie.column()
                col.operator("gpencil.select_all", text="Select All", icon='PARTICLE_POINT')
                col.operator("gpencil.select_all", text="Select Inverse", icon='BLANK1')
                col.operator("gpencil.select_linked", text="Select Linked", icon='LINKED')

                # NE - Select (Modal)
                col = pie.column()
                col.operator("gpencil.select_border", text="Border Select", icon='BORDER_RECT')
                col.operator("gpencil.select_circle", text="Circle Select", icon='META_EMPTY')
                col.operator("gpencil.select_lasso", text="Lasso Select", icon='BORDER_LASSO')

                # SW - Edit Tools
                col = pie.column()
                col.operator("gpencil.duplicate_move", icon='PARTICLE_PATH', text="Duplicate")
                col.operator("gpencil.delete", icon='X', text="Delete...")

                # SE - More Tools
                pie.operator("wm.call_menu_pie", text="More...").name = "GPENCIL_PIE_tools_more"
            else:
                # Toggle Edit Mode
                pie.operator("gpencil.editmode_toggle", text="Enable Stroke Editing", icon='EDIT')


class GPENCIL_PIE_settings_palette(Menu):
    """A pie menu for quick access to Grease Pencil settings"""
    bl_label = "Grease Pencil Settings"

    @classmethod
    def poll(cls, context):
        return bool(context.gpencil_data and context.active_gpencil_layer)

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()
        # gpd = context.gpencil_data
        gpl = context.active_gpencil_layer

        # W - Stroke draw settings
        col = pie.column(align=True)
        col.label(text="Stroke")
        col.prop(gpl, "color", text="")
        col.prop(gpl, "alpha", text="", slider=True)

        # E - Fill draw settings
        col = pie.column(align=True)
        col.label(text="Fill")
        col.prop(gpl, "fill_color", text="")
        col.prop(gpl, "fill_alpha", text="", slider=True)

        # S - Layer settings
        col = pie.column()
        col.prop(gpl, "line_width", slider=True)
        # col.prop(gpl, "use_volumetric_strokes")
        col.prop(gpl, "use_onion_skinning")

        # N - Active Layer
        col = pie.column()
        col.label("Active Layer:      ")

        row = col.row()
        row.operator_context = 'EXEC_REGION_WIN'
        row.operator_menu_enum("gpencil.layer_change", "layer", text="", icon='GREASEPENCIL')
        row.prop(gpl, "info", text="")
        row.operator("gpencil.layer_remove", text="", icon='X')

        row = col.row()
        row.prop(gpl, "lock")
        row.prop(gpl, "hide")


class GPENCIL_PIE_tools_more(Menu):
    """A pie menu for accessing more Grease Pencil tools"""
    bl_label = "More Grease Pencil Tools"

    @classmethod
    def poll(cls, context):
        gpd = context.gpencil_data
        return bool(gpd and gpd.use_stroke_edit_mode and context.editable_gpencil_strokes)

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()
        # gpd = context.gpencil_data

        col = pie.column(align=True)
        col.operator("gpencil.copy", icon='COPYDOWN', text="Copy")
        col.operator("gpencil.paste", icon='PASTEDOWN', text="Paste")

        col = pie.column(align=True)
        col.operator("gpencil.select_more", icon='ZOOMIN')
        col.operator("gpencil.select_less", icon='ZOOMOUT')

        pie.operator("transform.mirror", icon='MOD_MIRROR')
        pie.operator("transform.bend", icon='MOD_SIMPLEDEFORM')
        pie.operator("transform.shear", icon='MOD_TRIANGULATE')
        pie.operator("transform.tosphere", icon='MOD_MULTIRES')

        pie.operator("gpencil.convert", icon='OUTLINER_OB_CURVE', text="Convert...")
        pie.operator("wm.call_menu_pie", text="Back to Main Palette...").name = "GPENCIL_PIE_tool_palette"


class GPENCIL_PIE_sculpt(Menu):
    """A pie menu for accessing Grease Pencil stroke sculpting settings"""
    bl_label = "Grease Pencil Sculpt"

    @classmethod
    def poll(cls, context):
        gpd = context.gpencil_data
        return bool(gpd and gpd.use_stroke_edit_mode and context.editable_gpencil_strokes)

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()

        settings = context.tool_settings.gpencil_sculpt
        brush = settings.brush

        # W - Launch Sculpt Mode
        col = pie.column()
        # col.label("Tool:")
        col.prop(settings, "tool", text="")
        col.operator("gpencil.brush_paint", text="Sculpt", icon='SCULPTMODE_HLT')

        # E - Common Settings
        col = pie.column(align=True)
        col.prop(brush, "size", slider=True)
        row = col.row(align=True)
        row.prop(brush, "strength", slider=True)
        # row.prop(brush, "use_pressure_strength", text="", icon_only=True)
        col.prop(brush, "use_falloff")

        # S - Change Brush Type Shortcuts
        row = pie.row()
        row.prop_enum(settings, "tool", value='GRAB')
        row.prop_enum(settings, "tool", value='PUSH')
        row.prop_enum(settings, "tool", value='CLONE')

        # N - Change Brush Type Shortcuts
        row = pie.row()
        row.prop_enum(settings, "tool", value='SMOOTH')
        row.prop_enum(settings, "tool", value='THICKNESS')
        row.prop_enum(settings, "tool", value='RANDOMIZE')


###############################


class GPENCIL_MT_snap(Menu):
    bl_label = "Snap"

    def draw(self, context):
        layout = self.layout

        layout.operator("gpencil.snap_to_grid", text="Selection to Grid")
        layout.operator("gpencil.snap_to_cursor", text="Selection to Cursor").use_offset = False
        layout.operator("gpencil.snap_to_cursor", text="Selection to Cursor (Offset)").use_offset = True

        layout.separator()

        layout.operator("gpencil.snap_cursor_to_selected", text="Cursor to Selected")
        layout.operator("view3d.snap_cursor_to_center", text="Cursor to Center")
        layout.operator("view3d.snap_cursor_to_grid", text="Cursor to Grid")


###############################


class GPENCIL_UL_layer(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        # assert(isinstance(item, bpy.types.GPencilLayer)
        gpl = item

        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            if gpl.lock:
                layout.active = False

            split = layout.split(percentage=0.25)
            row = split.row(align=True)
            row.prop(gpl, "color", text="", emboss=gpl.is_stroke_visible)
            row.prop(gpl, "fill_color", text="", emboss=gpl.is_fill_visible)
            split.prop(gpl, "info", text="", emboss=False)

            row = layout.row(align=True)
            row.prop(gpl, "lock", text="", emboss=False)
            row.prop(gpl, "hide", text="", emboss=False)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class GPENCIL_MT_layer_specials(Menu):
    bl_label = "Layer"

    def draw(self, context):
        layout = self.layout

        layout.operator("gpencil.layer_duplicate", icon='COPY_ID')  # XXX: needs a dedicated icon

        layout.separator()

        layout.operator("gpencil.reveal", icon='RESTRICT_VIEW_OFF', text="Show All")
        layout.operator("gpencil.hide", icon='RESTRICT_VIEW_ON', text="Hide Others").unselected = True

        layout.separator()

        layout.operator("gpencil.lock_all", icon='LOCKED', text="Lock All")
        layout.operator("gpencil.unlock_all", icon='UNLOCKED', text="UnLock All")


class GreasePencilDataPanel:
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    bl_label = "Grease Pencil"
    bl_region_type = 'UI'

    @staticmethod
    def draw_header(self, context):
        self.layout.prop(context.space_data, "show_grease_pencil", text="")

    @staticmethod
    def draw(self, context):
        layout = self.layout

        # owner of Grease Pencil data
        gpd_owner = context.gpencil_data_owner
        gpd = context.gpencil_data

        # Owner Selector
        if context.space_data.type == 'VIEW_3D':
            layout.prop(context.tool_settings, "grease_pencil_source", expand=True)
        elif context.space_data.type == 'CLIP_EDITOR':
            layout.prop(context.space_data, "grease_pencil_source", expand=True)

        # Grease Pencil data selector
        layout.template_ID(gpd_owner, "grease_pencil", new="gpencil.data_add", unlink="gpencil.data_unlink")

        # Grease Pencil data...
        if (gpd is None) or (not gpd.layers):
            layout.operator("gpencil.layer_add", text="New Layer")
        else:
            self.draw_layers(context, layout, gpd)

    def draw_layers(self, context, layout, gpd):
        row = layout.row()

        col = row.column()
        if len(gpd.layers) >= 2:
            layer_rows = 5
        else:
            layer_rows = 2
        col.template_list("GPENCIL_UL_layer", "", gpd, "layers", gpd.layers, "active_index", rows=layer_rows)

        col = row.column()

        sub = col.column(align=True)
        sub.operator("gpencil.layer_add", icon='ZOOMIN', text="")
        sub.operator("gpencil.layer_remove", icon='ZOOMOUT', text="")

        gpl = context.active_gpencil_layer
        if gpl:
            sub.menu("GPENCIL_MT_layer_specials", icon='DOWNARROW_HLT', text="")

            if len(gpd.layers) > 1:
                col.separator()

                sub = col.column(align=True)
                sub.operator("gpencil.layer_move", icon='TRIA_UP', text="").type = 'UP'
                sub.operator("gpencil.layer_move", icon='TRIA_DOWN', text="").type = 'DOWN'

                col.separator()

                sub = col.column(align=True)
                sub.operator("gpencil.layer_isolate", icon='SOLO_OFF', text="").affect_visibility = False
                sub.operator("gpencil.layer_isolate", icon='RESTRICT_VIEW_OFF', text="").affect_visibility = True

        if gpl:
            self.draw_layer(layout, gpl)

    def draw_layer(self, layout, gpl):
        # layer settings
        split = layout.split(percentage=0.5)
        split.active = not gpl.lock

        # Column 1 - Stroke
        col = split.column(align=True)
        col.label(text="Stroke:")
        col.prop(gpl, "color", text="")
        col.prop(gpl, "alpha", slider=True)

        # Column 2 - Fill
        col = split.column(align=True)
        col.label(text="Fill:")
        col.prop(gpl, "fill_color", text="")
        col.prop(gpl, "fill_alpha", text="Opacity", slider=True)

        # Options
        split = layout.split(percentage=0.5)
        split.active = not gpl.lock

        col = split.column(align=True)
        col.prop(gpl, "line_width", slider=True)
        col.prop(gpl, "use_volumetric_strokes")
        col.prop(gpl, "use_hq_fill")

        col = split.column(align=True)
        col.prop(gpl, "show_x_ray")
        col.prop(gpl, "show_points", text="Points")

        layout.separator()

        # Full-Row - Frame Locking (and Delete Frame)
        row = layout.row(align=True)
        row.active = not gpl.lock

        if gpl.active_frame:
            lock_status = "Locked" if gpl.lock_frame else "Unlocked"
            lock_label = "Frame: %d (%s)" % (gpl.active_frame.frame_number, lock_status)
        else:
            lock_label = "Lock Frame"
        row.prop(gpl, "lock_frame", text=lock_label, icon='UNLOCKED')
        row.operator("gpencil.active_frame_delete", text="", icon='X')

        layout.separator()

        # Onion skinning
        col = layout.column(align=True)
        col.active = not gpl.lock

        row = col.row()
        row.prop(gpl, "use_onion_skinning")
        row.prop(gpl, "use_ghost_custom_colors", text="", icon='COLOR')

        split = col.split(percentage=0.5)
        split.active = gpl.use_onion_skinning

        # - Before Frames
        sub = split.column(align=True)
        row = sub.row(align=True)
        row.active = gpl.use_ghost_custom_colors
        row.prop(gpl, "before_color", text="")
        sub.prop(gpl, "ghost_before_range", text="Before")

        # - After Frames
        sub = split.column(align=True)
        row = sub.row(align=True)
        row.active = gpl.use_ghost_custom_colors
        row.prop(gpl, "after_color", text="")
        sub.prop(gpl, "ghost_after_range", text="After")

        # Smooth and subdivide new strokes
        layout.separator()
        col = layout.column(align=True)
        col.label(text="New Stroke Quality:")
        col.prop(gpl, "pen_smooth_factor")
        col.prop(gpl, "pen_subdivision_steps")


class GreasePencilToolsPanel:
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    # bl_options = {'DEFAULT_CLOSED'}
    bl_label = "Grease Pencil Settings"
    bl_region_type = 'UI'

    @classmethod
    def poll(cls, context):
        return (context.gpencil_data is not None)

    @staticmethod
    def draw(self, context):
        layout = self.layout

        # gpd_owner = context.gpencil_data_owner
        gpd = context.gpencil_data

        layout.prop(gpd, "use_stroke_edit_mode", text="Enable Editing", icon='EDIT', toggle=True)

        layout.separator()

        layout.label("Proportional Edit:")
        row = layout.row()
        row.prop(context.tool_settings, "proportional_edit", text="")
        row.prop(context.tool_settings, "proportional_edit_falloff", text="")

        layout.separator()
        layout.separator()

        gpencil_stroke_placement_settings(context, layout)
