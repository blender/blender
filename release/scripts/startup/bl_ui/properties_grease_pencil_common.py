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

import bpy
from bpy.types import Menu, UIList
from bpy.app.translations import pgettext_iface as iface_


def gpencil_stroke_placement_settings(context, layout):
    if context.space_data.type == 'VIEW_3D':
        propname = "annotation_stroke_placement_view3d"
    elif context.space_data.type == 'SEQUENCE_EDITOR':
        propname = "annotation_stroke_placement_sequencer_preview"
    elif context.space_data.type == 'IMAGE_EDITOR':
        propname = "annotation_stroke_placement_image_editor"
    else:
        propname = "annotation_stroke_placement_view2d"

    ts = context.tool_settings

    col = layout.column(align=True)

    if context.space_data.type != 'VIEW_3D':
        col.label(text="Stroke Placement:")
        row = col.row(align=True)
        row.prop_enum(ts, propname, 'VIEW')
        row.prop_enum(ts, propname, 'CURSOR', text="Cursor")


def gpencil_active_brush_settings_simple(context, layout):
    brush = context.active_gpencil_brush
    if brush is None:
        layout.label("No Active Brush")
        return

    col = layout.column()
    col.label("Active Brush:      ")

    row = col.row(align=True)
    row.operator_context = 'EXEC_REGION_WIN'
    row.operator_menu_enum("gpencil.brush_change", "brush", text="", icon='BRUSH_DATA')
    row.prop(brush, "name", text="")

    col.prop(brush, "size", slider=True)
    row = col.row(align=True)
    row.prop(brush, "use_random_pressure", text="", icon='RNDCURVE')
    row.prop(brush, "pen_sensitivity_factor", slider=True)
    row.prop(brush, "use_pressure", text="", icon='STYLUS_PRESSURE')
    row = col.row(align=True)
    row.prop(brush, "use_random_strength", text="", icon='RNDCURVE')
    row.prop(brush, "strength", slider=True)
    row.prop(brush, "use_strength_pressure", text="", icon='STYLUS_PRESSURE')
    row = col.row(align=True)
    row.prop(brush, "jitter", slider=True)
    row.prop(brush, "use_jitter_pressure", text="", icon='STYLUS_PRESSURE')
    row = col.row()
    row.prop(brush, "angle", slider=True)
    row.prop(brush, "angle_factor", text="Factor", slider=True)


# XXX: To be replaced with active tools
class GreasePencilDrawingToolsPanel:
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    bl_label = "Grease Pencil"
    bl_category = "Grease Pencil"
    bl_region_type = 'TOOLS'

    @classmethod
    def poll(cls, context):
        return True

    @staticmethod
    def draw(self, context):
        layout = self.layout

        is_3d_view = context.space_data.type == 'VIEW_3D'
        is_clip_editor = context.space_data.type == 'CLIP_EDITOR'

        col = layout.column(align=True)

        col.label(text="Draw:")
        row = col.row(align=True)
        row.operator("gpencil.annotate", icon='GREASEPENCIL', text="Draw").mode = 'DRAW'
        row.operator("gpencil.annotate", icon='FORCE_CURVE', text="Erase").mode = 'ERASER'  # XXX: Needs a dedicated icon

        row = col.row(align=True)
        row.operator("gpencil.annotate", icon='LINE_DATA', text="Line").mode = 'DRAW_STRAIGHT'
        row.operator("gpencil.annotate", icon='MESH_DATA', text="Poly").mode = 'DRAW_POLY'

        col.separator()

        sub = col.column(align=True)
        sub.operator("gpencil.blank_frame_add", icon='NEW')
        sub.operator("gpencil.active_frames_delete_all", icon='X', text="Delete Frame(s)")

        #sub = col.column(align=True)
        #sub.prop(context.tool_settings, "use_gpencil_additive_drawing", text="Additive Drawing")
        #sub.prop(context.tool_settings, "use_gpencil_continuous_drawing", text="Continuous Drawing")
        #sub.prop(context.tool_settings, "use_gpencil_draw_onback", text="Draw on Back")

        col.separator()
        col.separator()

        if context.space_data.type in {'CLIP_EDITOR'}:
            col.separator()
            col.label("Data Source:")
            row = col.row(align=True)
            if is_3d_view:
                row.prop(context.tool_settings, "grease_pencil_source", expand=True)
            elif is_clip_editor:
                row.prop(context.space_data, "grease_pencil_source", expand=True)

        # col.separator()
        # col.separator()

        gpencil_stroke_placement_settings(context, col)

        gpd = context.gpencil_data

        if gpd and not is_3d_view:
            layout.separator()
            layout.separator()

            col = layout.column(align=True)
            col.prop(gpd, "use_stroke_edit_mode", text="Enable Editing", icon='EDIT', toggle=True)


class GreasePencilStrokeEditPanel:
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    bl_label = "Edit Strokes"
    bl_category = "Tools"
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

        is_3d_view = context.space_data.type == 'VIEW_3D'

        if not is_3d_view:
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
            col.operator("gpencil.select_alternate")

        layout.label(text="Edit:")
        row = layout.row(align=True)
        row.operator("gpencil.copy", text="Copy")
        row.operator("gpencil.paste", text="Paste").type = 'COPY'
        row.operator("gpencil.paste", text="Paste & Merge").type = 'MERGE'

        col = layout.column(align=True)
        col.operator("gpencil.delete")
        col.operator("gpencil.duplicate_move", text="Duplicate")
        if is_3d_view:
            col.operator("gpencil.stroke_cyclical_set", text="Toggle Cyclic").type = 'TOGGLE'

        layout.separator()

        if not is_3d_view:
            col = layout.column(align=True)
            col.operator("transform.translate")                # icon='MAN_TRANS'
            col.operator("transform.rotate")                   # icon='MAN_ROT'
            col.operator("transform.resize", text="Scale")     # icon='MAN_SCALE'

            layout.separator()

        layout.separator()
        col = layout.column(align=True)
        col.operator_menu_enum("gpencil.stroke_arrange", text="Arrange Strokes...", property="direction")
        col.operator("gpencil.stroke_change_color", text="Assign Material")

        layout.separator()
        col = layout.column(align=True)
        col.operator("gpencil.stroke_subdivide", text="Subdivide")
        row = col.row(align=True)
        row.operator("gpencil.stroke_simplify_fixed", text="Simplify")
        row.operator("gpencil.stroke_simplify", text="Adaptative")

        col.separator()

        row = col.row(align=True)
        row.operator("gpencil.stroke_join", text="Join").type = 'JOIN'
        row.operator("gpencil.stroke_join", text="& Copy").type = 'JOINCOPY'

        col.operator("gpencil.stroke_flip", text="Flip Direction")

        if is_3d_view:
            layout.separator()

            col = layout.column(align=True)
            col.operator_menu_enum("gpencil.stroke_separate", text="Separate...", property="mode")
            col.operator("gpencil.stroke_split", text="Split")

            col = layout.column(align=True)
            col.label(text="Cleanup:")
            col.operator_menu_enum("gpencil.reproject", text="Reproject Strokes...", property="type")
            col.operator_menu_enum("gpencil.frame_clean_fill", text="Clean Boundary Strokes...", property="mode")


class GreasePencilStrokeSculptPanel:
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    bl_label = "Sculpt Strokes"
    bl_category = "Tools"

    @staticmethod
    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        settings = context.tool_settings.gpencil_sculpt
        tool = settings.tool
        brush = settings.brush

        layout.template_icon_view(settings, "tool", show_labels=True)

        layout.prop(brush, "size", slider=True)
        row = layout.row(align=True)
        row.prop(brush, "strength", slider=True)
        row.prop(brush, "use_pressure_strength", text="")

        layout.prop(brush, "use_falloff")

        if tool in {'SMOOTH', 'RANDOMIZE'}:
            layout.prop(settings, "affect_position", text="Affect Position")
            layout.prop(settings, "affect_strength", text="Affect Strength")
            layout.prop(settings, "affect_thickness", text="Affect Thickness")

            if tool == 'SMOOTH':
                layout.prop(brush, "affect_pressure")

            layout.prop(settings, "affect_uv", text="Affect UV")

        if tool in {'THICKNESS', 'PINCH', 'TWIST'}:
            layout.prop(brush, "direction", expand=True)


# GP Object Tool Settings
class GreasePencilAppearancePanel:
    bl_label = "Brush Appearance"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return ob and ob.type == 'GPENCIL'

    @staticmethod
    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        ob = context.active_object

        if ob.mode == 'GPENCIL_PAINT':
            brush = context.active_gpencil_brush
            gp_settings = brush.gpencil_settings

            layout.prop(gp_settings, "gpencil_brush_type", text="Brush Type")

            sub = layout.column(align=True)
            sub.enabled = not brush.use_custom_icon
            sub.prop(gp_settings, "gp_icon", text="Icon")

            layout.prop(brush, "use_custom_icon")
            sub = layout.column()
            sub.active = brush.use_custom_icon
            sub.prop(brush, "icon_filepath", text="")

            layout.prop(gp_settings, "use_cursor", text="Show Brush")

            if gp_settings.gpencil_brush_type == 'FILL':
                layout.prop(brush, "cursor_color_add", text="Color")

        elif ob.mode in ('GPENCIL_SCULPT', 'GPENCIL_WEIGHT'):
            settings = context.tool_settings.gpencil_sculpt
            brush = settings.brush

            col = layout.column(align=True)
            col.prop(brush, "use_cursor", text="Show Brush")
            col.row().prop(brush, "cursor_color_add", text="Add")
            col.row().prop(brush, "cursor_color_sub", text="Subtract")


class GPENCIL_MT_pie_tool_palette(Menu):
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
            col.operator("wm.call_menu_pie", text="Settings...", icon='SCRIPTWIN').name = "GPENCIL_MT_pie_settings_palette"

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
                col.operator("gpencil.palettecolor_select", text="Select Color", icon='COLOR')

                # NE - Select (Modal)
                col = pie.column()
                col.operator("gpencil.select_border", text="Border Select", icon='BORDER_RECT')
                col.operator("gpencil.select_circle", text="Circle Select", icon='META_EMPTY')
                col.operator("gpencil.select_lasso", text="Lasso Select", icon='BORDER_LASSO')
                col.operator("gpencil.select_alternate", text="Alternate Select", icon='BORDER_LASSO')

                # SW - Edit Tools
                col = pie.column()
                col.operator("gpencil.duplicate_move", icon='PARTICLE_PATH', text="Duplicate")
                col.operator("gpencil.delete", icon='X', text="Delete...")

                # SE - More Tools
                pie.operator("wm.call_menu_pie", text="More...").name = "GPENCIL_MT_pie_tools_more"
            else:
                # Toggle Edit Mode
                pie.operator("gpencil.editmode_toggle", text="Enable Stroke Editing", icon='EDIT')


class GPENCIL_MT_pie_settings_palette(Menu):
    """A pie menu for quick access to Grease Pencil settings"""
    bl_label = "Grease Pencil Settings"

    @classmethod
    def poll(cls, context):
        return bool(context.gpencil_data and context.active_gpencil_layer)

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()
        gpd = context.gpencil_data
        gpl = context.active_gpencil_layer
        palcolor = None  # context.active_gpencil_palettecolor
        brush = context.active_gpencil_brush

        is_editmode = bool(gpd and gpd.use_stroke_edit_mode and context.editable_gpencil_strokes)

        # W - Stroke draw settings
        col = pie.column(align=True)
        if palcolor is not None:
            col.enabled = not palcolor.lock
            col.label(text="Stroke")
            col.prop(palcolor, "color", text="")
            col.prop(palcolor, "alpha", text="", slider=True)

        # E - Fill draw settings
        col = pie.column(align=True)
        if palcolor is not None:
            col.enabled = not palcolor.lock
            col.label(text="Fill")
            col.prop(palcolor, "fill_color", text="")
            col.prop(palcolor, "fill_alpha", text="", slider=True)

        # S Brush settings
        gpencil_active_brush_settings_simple(context, pie)

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
        col.prop(gpl, "use_onion_skinning")

        # NW/NE/SW/SE - These operators are only available in editmode
        # as they require strokes to be selected to work
        if is_editmode:
            # NW - Move stroke Down
            col = pie.column(align=True)
            col.label("Arrange Strokes")
            col.operator("gpencil.stroke_arrange", text="Send to Back").direction = 'BOTTOM'
            col.operator("gpencil.stroke_arrange", text="Send Backward").direction = 'DOWN'

            # NE - Move stroke Up
            col = pie.column(align=True)
            col.label("Arrange Strokes")
            col.operator("gpencil.stroke_arrange", text="Bring to Front").direction = 'TOP'
            col.operator("gpencil.stroke_arrange", text="Bring Forward").direction = 'UP'

            # SW - Move stroke to color
            col = pie.column(align=True)
            col.operator("gpencil.stroke_change_color", text="Move to Color")

            # SE - Join strokes
            col = pie.column(align=True)
            col.label("Join Strokes")
            row = col.row()
            row.operator("gpencil.stroke_join", text="Join").type = 'JOIN'
            row.operator("gpencil.stroke_join", text="Join & Copy").type = 'JOINCOPY'
            col.operator("gpencil.stroke_flip", text="Flip Direction")

            col.prop(gpd, "show_stroke_direction", text="Show Drawing Direction")


class GPENCIL_MT_pie_tools_more(Menu):
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
        pie.operator("wm.call_menu_pie", text="Back to Main Palette...").name = "GPENCIL_MT_pie_tool_palette"


class GPENCIL_MT_pie_sculpt(Menu):
    """A pie menu for accessing Grease Pencil stroke sculpt settings"""
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
        if settings.tool in {'SMOOTH', 'RANDOMIZE'}:
            row = col.row(align=True)
            row.prop(settings, "affect_position", text="Position", icon='MESH_DATA', toggle=True)
            row.prop(settings, "affect_strength", text="Strength", icon='COLOR', toggle=True)
            row.prop(settings, "affect_thickness", text="Thickness", icon='LINE_DATA', toggle=True)

        # S - Change Brush Type Shortcuts
        row = pie.row()
        row.prop_enum(settings, "tool", value='GRAB')
        row.prop_enum(settings, "tool", value='PUSH')
        row.prop_enum(settings, "tool", value='CLONE')

        # N - Change Brush Type Shortcuts
        row = pie.row()
        row.prop_enum(settings, "tool", value='SMOOTH')
        row.prop_enum(settings, "tool", value='THICKNESS')
        row.prop_enum(settings, "tool", value='STRENGTH')
        row.prop_enum(settings, "tool", value='RANDOMIZE')


class GPENCIL_MT_snap(Menu):
    bl_label = "Snap"

    def draw(self, context):
        layout = self.layout

        layout.operator("gpencil.snap_to_grid", text="Selection to Grid")
        layout.operator("gpencil.snap_to_cursor", text="Selection to Cursor").use_offset = False
        layout.operator("gpencil.snap_to_cursor", text="Selection to Cursor (Keep Offset)").use_offset = True

        layout.separator()

        layout.operator("gpencil.snap_cursor_to_selected", text="Cursor to Selected")
        layout.operator("view3d.snap_cursor_to_center", text="Cursor to Center")
        layout.operator("view3d.snap_cursor_to_grid", text="Cursor to Grid")


class GPENCIL_MT_separate(Menu):
    bl_label = "Separate"

    def draw(self, context):
        layout = self.layout
        layout.operator("gpencil.stroke_separate", text="Selected Points").mode = 'POINT'
        layout.operator("gpencil.stroke_separate", text="Selected Strokes").mode = 'STROKE'
        layout.operator("gpencil.stroke_separate", text="Active Layer").mode = 'LAYER'


class GPENCIL_MT_gpencil_draw_specials(Menu):
    bl_label = "GPencil Draw Specials"

    def draw(self, context):
        layout = self.layout
        is_3d_view = context.space_data.type == 'VIEW_3D'

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("gpencil.frame_duplicate", text="Duplicate Active Frame")
        layout.operator("gpencil.frame_duplicate", text="Duplicate Active Frame All Layers").mode = 'ALL'

        layout.separator()
        layout.operator("gpencil.primitive", text="Line", icon='IPO_CONSTANT').type = 'LINE'
        layout.operator("gpencil.primitive", text="Rectangle", icon='UV_FACESEL').type = 'BOX'
        layout.operator("gpencil.primitive", text="Circle", icon='ANTIALIASED').type = 'CIRCLE'

        # Colors.
        layout.separator()
        layout.operator("gpencil.colorpick", text="Colors", icon="GROUP_VCOL")


class GPENCIL_MT_gpencil_draw_delete(Menu):
    bl_label = "GPencil Draw Delete"

    def draw(self, context):
        layout = self.layout
        is_3d_view = context.space_data.type == 'VIEW_3D'

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("gpencil.active_frames_delete_all", text="Delete Frame")


class GPENCIL_UL_annotation_layer(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        # assert(isinstance(item, bpy.types.GPencilLayer)
        gpl = item
        gpd = context.gpencil_data

        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            if gpl.lock:
                layout.active = False

            split = layout.split(percentage=0.2)
            split.prop(gpl, "color", text="", emboss=True)
            split.prop(gpl, "info", text="", emboss=False)

            row = layout.row(align=True)
            # row.prop(gpl, "lock", text="", emboss=False)
            row.prop(gpl, "hide", text="", emboss=False)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class GreasePencilDataPanel:
    bl_label = "Annotations"
    bl_region_type = 'UI'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        # Show this panel as long as someone that might own this exists
        # AND the owner isn't an object (e.g. GP Object)
        if context.gpencil_data_owner is None:
            return False
        elif type(context.gpencil_data_owner) is bpy.types.Object:
            return False
        else:
            return True

    @staticmethod
    def draw_header(self, context):
        if context.space_data.type != 'VIEW_3D':
            self.layout.prop(context.space_data, "show_annotation", text="")

    @staticmethod
    def draw(self, context):
        layout = self.layout
        layout.use_property_decorate = False

        # Grease Pencil owner.
        gpd_owner = context.gpencil_data_owner
        gpd = context.gpencil_data

        # Owner selector.
        if context.space_data.type == 'CLIP_EDITOR':
            layout.row().prop(context.space_data, "grease_pencil_source", expand=True)

        layout.template_ID(gpd_owner, "grease_pencil", new="gpencil.data_add", unlink="gpencil.data_unlink")

        # List of layers/notes.
        if gpd and gpd.layers:
            self.draw_layers(context, layout, gpd)

    def draw_layers(self, context, layout, gpd):
        row = layout.row()

        col = row.column()
        if len(gpd.layers) >= 2:
            layer_rows = 5
        else:
            layer_rows = 2
        col.template_list("GPENCIL_UL_annotation_layer", "", gpd, "layers", gpd.layers, "active_index", rows=layer_rows)

        col = row.column()

        sub = col.column(align=True)
        sub.operator("gpencil.layer_add", icon='ZOOMIN', text="")
        sub.operator("gpencil.layer_remove", icon='ZOOMOUT', text="")

        gpl = context.active_gpencil_layer
        if gpl:
            if len(gpd.layers) > 1:
                col.separator()

                sub = col.column(align=True)
                sub.operator("gpencil.layer_move", icon='TRIA_UP', text="").type = 'UP'
                sub.operator("gpencil.layer_move", icon='TRIA_DOWN', text="").type = 'DOWN'

        tool_settings = context.tool_settings
        if gpd and gpl:
            layout.prop(gpl, "thickness")
        else:
            layout.prop(tool_settings, "annotation_thickness", text="Thickness")

        if gpl:
            # layout.prop(gpl, "opacity", text="Opacity", slider=True)
            # Full-Row - Frame Locking (and Delete Frame)
            row = layout.row(align=True)
            row.active = not gpl.lock

            if gpl.active_frame:
                lock_status = iface_("Locked") if gpl.lock_frame else iface_("Unlocked")
                lock_label = iface_("Frame: %d (%s)") % (gpl.active_frame.frame_number, lock_status)
            else:
                lock_label = iface_("Lock Frame")
            row.prop(gpl, "lock_frame", text=lock_label, icon='UNLOCKED')
            row.operator("gpencil.active_frame_delete", text="", icon='X')



class GreasePencilOnionPanel:
    @staticmethod
    def draw_settings(layout, gp):
        col = layout.column()
        col.prop(gp, "onion_mode")
        col.prop(gp, "onion_factor", text="Opacity", slider=True)

        if gp.onion_mode in ('ABSOLUTE', 'RELATIVE'):
            col = layout.column(align=True)
            col.prop(gp, "ghost_before_range", text="Frames Before")
            col.prop(gp, "ghost_after_range", text="After")

        layout.prop(gp, "use_ghost_custom_colors", text="Use Custom Colors")

        if gp.use_ghost_custom_colors:
            col = layout.column(align=True)
            col.active = gp.use_ghost_custom_colors
            col.prop(gp, "before_color", text="Color Before")
            col.prop(gp, "after_color", text="After")

        layout.prop(gp, "use_ghosts_always", text="View In Render")

        col = layout.column(align=True)
        col.active = gp.use_onion_skinning
        col.prop(gp, "use_onion_fade", text="Fade")
        if hasattr(gp, "use_onion_loop"):  # XXX
            sub = layout.column()
            sub.active = gp.onion_mode in ('RELATIVE', 'SELECTED')
            sub.prop(gp, "use_onion_loop", text="Loop")


class GreasePencilToolsPanel:
    # For use in "2D" Editors without their own toolbar
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    bl_label = "Grease Pencil Settings"
    bl_region_type = 'UI'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        # XXX - disabled in 2.8 branch.
        return False

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

        gpencil_active_brush_settings_simple(context, layout)

        layout.separator()

        gpencil_stroke_placement_settings(context, layout)


classes = (
    GPENCIL_MT_pie_tool_palette,
    GPENCIL_MT_pie_settings_palette,
    GPENCIL_MT_pie_tools_more,
    GPENCIL_MT_pie_sculpt,

    GPENCIL_MT_snap,
    GPENCIL_MT_separate,

    GPENCIL_MT_gpencil_draw_specials,
    GPENCIL_MT_gpencil_draw_delete,

    GPENCIL_UL_annotation_layer,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
