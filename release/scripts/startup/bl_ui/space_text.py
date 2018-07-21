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

# <pep8-80 compliant>
import bpy
from bpy.types import Header, Menu, Panel
from bpy.app.translations import pgettext_iface as iface_


class TEXT_HT_header(Header):
    bl_space_type = 'TEXT_EDITOR'

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        text = st.text

        row = layout.row(align=True)
        row.template_header()

        TEXT_MT_editor_menus.draw_collapsible(context, layout)

        if text and text.is_modified:
            sub = row.row(align=True)
            sub.alert = True
            sub.operator("text.resolve_conflict", text="", icon='HELP')

        layout.separator_spacer()

        row = layout.row(align=True)
        row.template_ID(st, "text", new="text.new", unlink="text.unlink", open="text.open")

        layout.separator_spacer()

        row = layout.row(align=True)
        row.prop(st, "show_line_numbers", text="")
        row.prop(st, "show_word_wrap", text="")
        row.prop(st, "show_syntax_highlight", text="")

        if text:
            is_osl = text.name.endswith((".osl", ".osl"))

            row = layout.row()
            if text.filepath:
                if text.is_dirty:
                    row.label(
                        iface_(f"File: *{text.filepath:s} (unsaved)"),
                        translate=False,
                    )
                else:
                    row.label(
                        iface_(f"File: {text.filepath:s}"),
                        translate=False,
                    )
            else:
                row.label(
                    "Text: External"
                    if text.library
                    else "Text: Internal"
                )
            if is_osl:
                row = layout.row()
                row.operator("node.shader_script_update")
            else:
                row = layout.row()
                row.active = text.name.endswith(".py")
                row.prop(text, "use_module")

                row = layout.row()
                row.operator("text.run_script")


class TEXT_MT_editor_menus(Menu):
    bl_idname = "TEXT_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        st = context.space_data
        text = st.text

        layout.menu("TEXT_MT_view")
        layout.menu("TEXT_MT_text")

        if text:
            layout.menu("TEXT_MT_edit")
            layout.menu("TEXT_MT_format")

        layout.menu("TEXT_MT_templates")


class TEXT_PT_properties(Panel):
    bl_space_type = 'TEXT_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Properties"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        flow = layout.column_flow()
        flow.prop(st, "show_line_numbers")
        flow.prop(st, "show_word_wrap")
        flow.prop(st, "show_syntax_highlight")
        flow.prop(st, "show_line_highlight")
        flow.prop(st, "use_live_edit")

        flow = layout.column_flow()
        flow.prop(st, "font_size")
        flow.prop(st, "tab_width")

        text = st.text
        if text:
            flow.prop(text, "use_tabs_as_spaces")

        flow.prop(st, "show_margin")
        col = flow.column()
        col.active = st.show_margin
        col.prop(st, "margin_column")


class TEXT_PT_find(Panel):
    bl_space_type = 'TEXT_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Find"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        # find
        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(st, "find_text", text="")
        row.operator("text.find_set_selected", text="", icon='TEXT')
        col.operator("text.find")

        # replace
        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(st, "replace_text", text="")
        row.operator("text.replace_set_selected", text="", icon='TEXT')
        col.operator("text.replace")

        # settings
        layout.prop(st, "use_match_case")
        row = layout.row(align=True)
        row.prop(st, "use_find_wrap", text="Wrap")
        row.prop(st, "use_find_all", text="All")


class TEXT_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        layout.operator("text.properties", icon='MENU_PANEL')

        layout.separator()

        layout.operator("text.move",
                        text="Top of File",
                        ).type = 'FILE_TOP'
        layout.operator("text.move",
                        text="Bottom of File",
                        ).type = 'FILE_BOTTOM'

        layout.separator()

        layout.menu("INFO_MT_area")


class TEXT_MT_text(Menu):
    bl_label = "Text"

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        text = st.text

        layout.operator("text.new")
        layout.operator("text.open")

        if text:
            layout.operator("text.reload")

            layout.column()
            layout.operator("text.save")
            layout.operator("text.save_as")

            if text.filepath:
                layout.operator("text.make_internal")

            layout.column()
            layout.operator("text.run_script")


class TEXT_MT_templates_py(Menu):
    bl_label = "Python"

    def draw(self, context):
        self.path_menu(
            bpy.utils.script_paths("templates_py"),
            "text.open",
            props_default={"internal": True},
        )


class TEXT_MT_templates_osl(Menu):
    bl_label = "Open Shading Language"

    def draw(self, context):
        self.path_menu(
            bpy.utils.script_paths("templates_osl"),
            "text.open",
            props_default={"internal": True},
        )


class TEXT_MT_templates(Menu):
    bl_label = "Templates"

    def draw(self, context):
        layout = self.layout
        layout.menu("TEXT_MT_templates_py")
        layout.menu("TEXT_MT_templates_osl")


class TEXT_MT_edit_select(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("text.select_all")
        layout.operator("text.select_line")


class TEXT_MT_format(Menu):
    bl_label = "Format"

    def draw(self, context):
        layout = self.layout

        layout.operator("text.indent")
        layout.operator("text.unindent")

        layout.separator()

        layout.operator("text.comment")
        layout.operator("text.uncomment")

        layout.separator()

        layout.operator_menu_enum("text.convert_whitespace", "type")


class TEXT_MT_edit_to3d(Menu):
    bl_label = "Text To 3D Object"

    def draw(self, context):
        layout = self.layout

        layout.operator("text.to_3d_object",
                        text="One Object",
                        ).split_lines = False
        layout.operator("text.to_3d_object",
                        text="One Object Per Line",
                        ).split_lines = True


class TEXT_MT_edit(Menu):
    bl_label = "Edit"

    @classmethod
    def poll(cls, context):
        return (context.space_data.text)

    def draw(self, context):
        layout = self.layout

        layout.operator("ed.undo")
        layout.operator("ed.redo")

        layout.separator()

        layout.operator("text.cut")
        layout.operator("text.copy")
        layout.operator("text.paste")
        layout.operator("text.duplicate_line")

        layout.separator()

        layout.operator("text.move_lines",
                        text="Move line(s) up").direction = 'UP'
        layout.operator("text.move_lines",
                        text="Move line(s) down").direction = 'DOWN'

        layout.separator()

        layout.menu("TEXT_MT_edit_select")

        layout.separator()

        layout.operator("text.jump")
        layout.operator("text.start_find", text="Find...")
        layout.operator("text.autocomplete")

        layout.separator()

        layout.menu("TEXT_MT_edit_to3d")


class TEXT_MT_toolbox(Menu):
    bl_label = ""

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_DEFAULT'

        layout.operator("text.cut")
        layout.operator("text.copy")
        layout.operator("text.paste")

        layout.separator()

        layout.operator("text.run_script")


classes = (
    TEXT_HT_header,
    TEXT_MT_edit,
    TEXT_MT_editor_menus,
    TEXT_PT_properties,
    TEXT_PT_find,
    TEXT_MT_view,
    TEXT_MT_text,
    TEXT_MT_templates,
    TEXT_MT_templates_py,
    TEXT_MT_templates_osl,
    TEXT_MT_edit_select,
    TEXT_MT_format,
    TEXT_MT_edit_to3d,
    TEXT_MT_toolbox,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
