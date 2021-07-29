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


bl_info = {
    "name": "Icon Viewer",
    "description": "Click an icon to copy its name to the clipboard",
    "author": "roaoao",
    "version": (1, 3, 2),
    "blender": (2, 75, 0),
    "location": "Spacebar > Icon Viewer, Text Editor > Properties",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6"
                "/Py/Scripts/Development/Display_All_Icons",
    "category": "Development"
}

import bpy
import math

DPI = 72
POPUP_PADDING = 10
PANEL_PADDING = 44
WIN_PADDING = 32
ICON_SIZE = 20
HISTORY_SIZE = 100
HISTORY = []


def ui_scale():
    prefs = bpy.context.user_preferences.system
    return prefs.dpi * prefs.pixel_size / DPI


def prefs():
    return bpy.context.user_preferences.addons[__name__].preferences


class Icons:
    def __init__(self, is_popup=False):
        self._filtered_icons = None
        self._filter = ""
        self.filter = ""
        self.selected_icon = ""
        self.is_popup = is_popup

    @property
    def filter(self):
        return self._filter

    @filter.setter
    def filter(self, value):
        if self._filter == value:
            return

        self._filter = value
        self.update()

    @property
    def filtered_icons(self):
        if self._filtered_icons is None:
            self._filtered_icons = []
            icon_filter = self._filter.upper()
            self.filtered_icons.clear()
            pr = prefs()

            icons = bpy.types.UILayout.bl_rna.functions[
                "prop"].parameters["icon"].enum_items.keys()
            for icon in icons:
                if icon == 'NONE' or \
                        icon_filter and icon_filter not in icon or \
                        not pr.show_brush_icons and "BRUSH_" in icon and \
                        icon != 'BRUSH_DATA' or \
                        not pr.show_matcap_icons and "MATCAP_" in icon or \
                        not pr.show_colorset_icons and "COLORSET_" in icon:
                    continue
                self._filtered_icons.append(icon)

        return self._filtered_icons

    @property
    def num_icons(self):
        return len(self.filtered_icons)

    def update(self):
        if self._filtered_icons is not None:
            self._filtered_icons.clear()
            self._filtered_icons = None

    def draw(self, layout, num_cols=0, icons=None):
        if icons:
            filtered_icons = reversed(icons)
        else:
            filtered_icons = self.filtered_icons

        column = layout.column(True)
        row = column.row(True)
        row.alignment = 'CENTER'

        selected_icon = self.selected_icon if self.is_popup else \
            bpy.context.window_manager.clipboard
        col_idx = 0
        for i, icon in enumerate(filtered_icons):
            p = row.operator(
                IV_OT_icon_select.bl_idname, "",
                icon=icon, emboss=icon == selected_icon)
            p.icon = icon
            p.force_copy_on_select = not self.is_popup

            col_idx += 1
            if col_idx > num_cols - 1:
                if icons:
                    break
                col_idx = 0
                if i < len(filtered_icons) - 1:
                    row = column.row(True)
                    row.alignment = 'CENTER'

        if col_idx != 0 and not icons and i >= num_cols:
            sub = row.row(True)
            sub.scale_x = num_cols - col_idx
            sub.label("", icon='BLANK1')

        if not filtered_icons:
            row.label("No icons were found")


class IV_Preferences(bpy.types.AddonPreferences):
    bl_idname = __name__

    panel_icons = Icons()
    popup_icons = Icons(is_popup=True)

    def update_icons(self, context):
        self.panel_icons.update()
        self.popup_icons.update()

    def set_panel_filter(self, value):
        self.panel_icons.filter = value

    panel_filter = bpy.props.StringProperty(
        description="Filter",
        default="",
        get=lambda s: s.panel_icons.filter,
        set=set_panel_filter,
        options={'TEXTEDIT_UPDATE'})
    show_panel_icons = bpy.props.BoolProperty(
        name="Show Icons",
        description="Show icons", default=True)
    show_history = bpy.props.BoolProperty(
        name="Show History",
        description="Show history", default=True)
    show_brush_icons = bpy.props.BoolProperty(
        name="Show Brush Icons",
        description="Show brush icons", default=True,
        update=update_icons)
    show_matcap_icons = bpy.props.BoolProperty(
        name="Show Matcap Icons",
        description="Show matcap icons", default=True,
        update=update_icons)
    show_colorset_icons = bpy.props.BoolProperty(
        name="Show Colorset Icons",
        description="Show colorset icons", default=True,
        update=update_icons)
    copy_on_select = bpy.props.BoolProperty(
        name="Copy Icon On Click",
        description="Copy icon on click", default=True)
    close_on_select = bpy.props.BoolProperty(
        name="Close Popup On Click",
        description=(
            "Close the popup on click.\n"
            "Not supported by some windows (User Preferences, Render)"
            ),
        default=False)
    auto_focus_filter = bpy.props.BoolProperty(
        name="Auto Focus Input Field",
        description="Auto focus input field", default=True)
    show_panel = bpy.props.BoolProperty(
        name="Show Panel",
        description="Show the panel in the Text Editor", default=True)
    show_header = bpy.props.BoolProperty(
        name="Show Header",
        description="Show the header in the Python Console",
        default=True)

    def draw(self, context):
        layout = self.layout
        row = layout.row()
        row.scale_y = 1.5
        row.operator(IV_OT_icons_show.bl_idname)

        row = layout.row()

        col = row.column(True)
        col.label("Icons:")
        col.prop(self, "show_matcap_icons")
        col.prop(self, "show_brush_icons")
        col.prop(self, "show_colorset_icons")
        col.separator()
        col.prop(self, "show_history")

        col = row.column(True)
        col.label("Popup:")
        col.prop(self, "auto_focus_filter")
        col.prop(self, "copy_on_select")
        if self.copy_on_select:
            col.prop(self, "close_on_select")

        col = row.column(True)
        col.label("Panel:")
        col.prop(self, "show_panel")
        if self.show_panel:
            col.prop(self, "show_panel_icons")

        col.separator()
        col.label("Header:")
        col.prop(self, "show_header")


class IV_PT_icons(bpy.types.Panel):
    bl_space_type = "TEXT_EDITOR"
    bl_region_type = "UI"
    bl_label = "Icon Viewer"

    @staticmethod
    def tag_redraw():
        wm = bpy.context.window_manager
        if not wm:
            return

        for w in wm.windows:
            for a in w.screen.areas:
                if a.type == 'TEXT_EDITOR':
                    for r in a.regions:
                        if r.type == 'UI':
                            r.tag_redraw()

    def draw(self, context):
        pr = prefs()
        row = self.layout.row(True)
        if pr.show_panel_icons:
            row.prop(pr, "panel_filter", "", icon='VIEWZOOM')
        else:
            row.operator(IV_OT_icons_show.bl_idname)
        row.operator(IV_OT_panel_menu_call.bl_idname, "", icon='COLLAPSEMENU')

        _, y0 = context.region.view2d.region_to_view(0, 0)
        _, y1 = context.region.view2d.region_to_view(0, 10)
        region_scale = 10 / abs(y0 - y1)

        num_cols = max(
            1,
            (context.region.width - PANEL_PADDING) //
            math.ceil(ui_scale() * region_scale * ICON_SIZE))

        col = None
        if HISTORY and pr.show_history:
            col = self.layout.column(True)
            pr.panel_icons.draw(col.box(), num_cols, HISTORY)

        if pr.show_panel_icons:
            col = col or self.layout.column(True)
            pr.panel_icons.draw(col.box(), num_cols)

    @classmethod
    def poll(cls, context):
        return prefs().show_panel


class IV_HT_icons(bpy.types.Header):
    bl_space_type = 'CONSOLE'

    def draw(self, context):
        if not prefs().show_header:
            return
        layout = self.layout
        layout.separator()
        layout.operator(IV_OT_icons_show.bl_idname)


class IV_OT_panel_menu_call(bpy.types.Operator):
    bl_idname = "iv.panel_menu_call"
    bl_label = ""
    bl_description = "Menu"
    bl_options = {'INTERNAL'}

    def menu(self, menu, context):
        pr = prefs()
        layout = menu.layout
        layout.prop(pr, "show_panel_icons")
        layout.prop(pr, "show_history")

        if not pr.show_panel_icons:
            return

        layout.separator()
        layout.prop(pr, "show_matcap_icons")
        layout.prop(pr, "show_brush_icons")
        layout.prop(pr, "show_colorset_icons")

    def execute(self, context):
        context.window_manager.popup_menu(self.menu, "Icon Viewer")
        return {'FINISHED'}


class IV_OT_icon_select(bpy.types.Operator):
    bl_idname = "iv.icon_select"
    bl_label = ""
    bl_description = "Select the icon"
    bl_options = {'INTERNAL'}

    icon = bpy.props.StringProperty()
    force_copy_on_select = bpy.props.BoolProperty()

    def execute(self, context):
        pr = prefs()
        pr.popup_icons.selected_icon = self.icon
        if pr.copy_on_select or self.force_copy_on_select:
            context.window_manager.clipboard = self.icon
            self.report({'INFO'}, self.icon)

            if pr.close_on_select and IV_OT_icons_show.instance:
                IV_OT_icons_show.instance.close()

        if pr.show_history:
            if self.icon in HISTORY:
                HISTORY.remove(self.icon)
            if len(HISTORY) >= HISTORY_SIZE:
                HISTORY.pop(0)
            HISTORY.append(self.icon)
        return {'FINISHED'}


class IV_OT_icons_show(bpy.types.Operator):
    bl_idname = "iv.icons_show"
    bl_label = "Icon Viewer"
    bl_description = "Icon viewer"
    bl_property = "filter_auto_focus"

    instance = None

    def set_filter(self, value):
        prefs().popup_icons.filter = value

    def set_selected_icon(self, value):
        if IV_OT_icons_show.instance:
            IV_OT_icons_show.instance.auto_focusable = False

    filter_auto_focus = bpy.props.StringProperty(
        description="Filter",
        get=lambda s: prefs().popup_icons.filter,
        set=set_filter,
        options={'TEXTEDIT_UPDATE', 'SKIP_SAVE'})
    filter = bpy.props.StringProperty(
        description="Filter",
        get=lambda s: prefs().popup_icons.filter,
        set=set_filter,
        options={'TEXTEDIT_UPDATE'})
    selected_icon = bpy.props.StringProperty(
        description="Selected Icon",
        get=lambda s: prefs().popup_icons.selected_icon,
        set=set_selected_icon)

    def get_num_cols(self, num_icons):
        return round(1.3 * math.sqrt(num_icons))

    def draw_header(self, layout):
        pr = prefs()
        header = layout.box()
        header = header.split(0.75) if self.selected_icon else header.row()
        row = header.row(True)
        row.prop(pr, "show_matcap_icons", "", icon='SMOOTH')
        row.prop(pr, "show_brush_icons", "", icon='BRUSH_DATA')
        row.prop(pr, "show_colorset_icons", "", icon='COLOR')
        row.separator()

        row.prop(
            pr, "copy_on_select", "",
            icon='BORDER_RECT', toggle=True)
        if pr.copy_on_select:
            sub = row.row(True)
            if bpy.context.window.screen.name == "temp":
                sub.alert = True
            sub.prop(
                pr, "close_on_select", "",
                icon='RESTRICT_SELECT_OFF', toggle=True)
        row.prop(
            pr, "auto_focus_filter", "",
            icon='OUTLINER_DATA_FONT', toggle=True)
        row.separator()

        if self.auto_focusable and pr.auto_focus_filter:
            row.prop(self, "filter_auto_focus", "", icon='VIEWZOOM')
        else:
            row.prop(self, "filter", "", icon='VIEWZOOM')

        if self.selected_icon:
            row = header.row()
            row.prop(self, "selected_icon", "", icon=self.selected_icon)

    def draw(self, context):
        pr = prefs()
        col = self.layout
        self.draw_header(col)

        history_num_cols = int(
            (self.width - POPUP_PADDING) / (ui_scale() * ICON_SIZE))
        num_cols = min(
            self.get_num_cols(len(pr.popup_icons.filtered_icons)),
            history_num_cols)

        subcol = col.column(True)

        if HISTORY and pr.show_history:
            pr.popup_icons.draw(subcol.box(), history_num_cols, HISTORY)

        pr.popup_icons.draw(subcol.box(), num_cols)

    def close(self):
        bpy.context.window.screen = bpy.context.window.screen

    def check(self, context):
        return True

    def cancel(self, context):
        IV_OT_icons_show.instance = None
        IV_PT_icons.tag_redraw()

    def execute(self, context):
        if not IV_OT_icons_show.instance:
            return {'CANCELLED'}
        IV_OT_icons_show.instance = None

        pr = prefs()
        if self.selected_icon and not pr.copy_on_select:
            context.window_manager.clipboard = self.selected_icon
            self.report({'INFO'}, self.selected_icon)
        pr.popup_icons.selected_icon = ""

        IV_PT_icons.tag_redraw()
        return {'FINISHED'}

    def invoke(self, context, event):
        pr = prefs()
        pr.popup_icons.selected_icon = ""
        pr.popup_icons.filter = ""
        IV_OT_icons_show.instance = self
        self.auto_focusable = True

        num_cols = self.get_num_cols(len(pr.popup_icons.filtered_icons))
        self.width = min(
            ui_scale() * (num_cols * ICON_SIZE + POPUP_PADDING),
            context.window.width - WIN_PADDING)

        return context.window_manager.invoke_props_dialog(self, self.width)


def register():
    if bpy.app.background:
        return

    bpy.utils.register_module(__name__)


def unregister():
    if bpy.app.background:
        return

    bpy.utils.unregister_module(__name__)
