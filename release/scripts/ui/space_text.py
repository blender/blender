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


class TEXT_HT_header(bpy.types.Header):
    bl_space_type = 'TEXT_EDITOR'

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        text = st.text

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            sub = row.row(align=True)
            sub.menu("TEXT_MT_text")
            if text:
                sub.menu("TEXT_MT_edit")
                sub.menu("TEXT_MT_format")

        if text and text.modified:
            row = layout.row()
            # row.color(redalert)
            row.operator("text.resolve_conflict", text="", icon='ICON_HELP')

        layout.template_ID(st, "text", new="text.new", unlink="text.unlink")

        row = layout.row(align=True)
        row.prop(st, "line_numbers", text="")
        row.prop(st, "word_wrap", text="")
        row.prop(st, "syntax_highlight", text="")

        if text:
            row = layout.row()
            row.operator("text.run_script")
            row.prop(text, "use_module")

            row = layout.row()
            if text.filename != "":
                if text.dirty:
                    row.label(text="File: *%s (unsaved)" % text.filename)
                else:
                    row.label(text="File: %s" % text.filename)
            else:
                if text.library:
                    row.label(text="Text: External")
                else:
                    row.label(text="Text: Internal")


class TEXT_PT_properties(bpy.types.Panel):
    bl_space_type = 'TEXT_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Properties"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        flow = layout.column_flow()
        flow.prop(st, "line_numbers")
        flow.prop(st, "word_wrap")
        flow.prop(st, "syntax_highlight")
        flow.prop(st, "live_edit")

        flow = layout.column_flow()
        flow.prop(st, "font_size")
        flow.prop(st, "tab_width")


class TEXT_PT_find(bpy.types.Panel):
    bl_space_type = 'TEXT_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Find"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        # find
        col = layout.column(align=True)
        row = col.row()
        row.prop(st, "find_text", text="")
        row.operator("text.find_set_selected", text="", icon='ICON_TEXT')
        col.operator("text.find")

        # replace
        col = layout.column(align=True)
        row = col.row()
        row.prop(st, "replace_text", text="")
        row.operator("text.replace_set_selected", text="", icon='ICON_TEXT')
        col.operator("text.replace")

        # mark
        layout.operator("text.mark_all")

        # settings
        row = layout.row()
        row.prop(st, "find_wrap", text="Wrap")
        row.prop(st, "find_all", text="All")


class TEXT_MT_text(bpy.types.Menu):
    bl_label = "Text"

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        text = st.text

        layout.column()
        layout.operator("text.new")
        layout.operator("text.open")

        if text:
            layout.operator("text.reload")

            layout.column()
            layout.operator("text.save")
            layout.operator("text.save_as")

            if text.filename != "":
                layout.operator("text.make_internal")

            layout.column()
            layout.operator("text.run_script")

            #ifndef DISABLE_PYTHON
            # XXX if(BPY_is_pyconstraint(text))
            # XXX   uiMenuItemO(head, 0, "text.refresh_pyconstraints");
            #endif

        layout.separator()

        layout.operator("text.properties", icon='ICON_MENU_PANEL')

        layout.menu("TEXT_MT_templates")


class TEXT_MT_templates(bpy.types.Menu):
    '''
    Creates the menu items by scanning scripts/templates
    '''
    bl_label = "Script Templates"

    def draw(self, context):
        import os
        self.path_menu(bpy.utils.script_paths("templates"), "text.open")


class TEXT_MT_edit_view(bpy.types.Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        layout.operator_enum("text.move", "type", 'FILE_TOP', text="Top of File")
        layout.operator_enum("text.move", "type", 'FILE_BOTTOM', text="Bottom of File")


class TEXT_MT_edit_select(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("text.select_all")
        layout.operator("text.select_line")


class TEXT_MT_edit_markers(bpy.types.Menu):
    bl_label = "Markers"

    def draw(self, context):
        layout = self.layout

        layout.operator("text.markers_clear")
        layout.operator("text.next_marker")
        layout.operator("text.previous_marker")


class TEXT_MT_format(bpy.types.Menu):
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


class TEXT_MT_edit_to3d(bpy.types.Menu):
    bl_label = "Text To 3D Object"

    def draw(self, context):
        layout = self.layout

        layout.operator_boolean("text.to_3d_object", "split_lines", False, text="One Object")
        layout.operator_boolean("text.to_3d_object", "split_lines", True, text="One Object Per Line")


class TEXT_MT_edit(bpy.types.Menu):
    bl_label = "Edit"

    def poll(self, context):
        return (context.space_data.text)

    def draw(self, context):
        layout = self.layout

        layout.operator("ed.undo")
        layout.operator("ed.redo")

        layout.separator()

        layout.operator("text.cut")
        layout.operator("text.copy")
        layout.operator("text.paste")

        layout.separator()

        layout.menu("TEXT_MT_edit_view")
        layout.menu("TEXT_MT_edit_select")
        layout.menu("TEXT_MT_edit_markers")

        layout.separator()

        layout.operator("text.jump")
        layout.operator("text.properties", text="Find...")

        layout.separator()

        layout.menu("TEXT_MT_edit_to3d")

bpy.types.register(TEXT_HT_header)
bpy.types.register(TEXT_PT_properties)
bpy.types.register(TEXT_PT_find)
bpy.types.register(TEXT_MT_text)
bpy.types.register(TEXT_MT_templates)
bpy.types.register(TEXT_MT_format)
bpy.types.register(TEXT_MT_edit)
bpy.types.register(TEXT_MT_edit_view)
bpy.types.register(TEXT_MT_edit_select)
bpy.types.register(TEXT_MT_edit_markers)
bpy.types.register(TEXT_MT_edit_to3d)
