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
            sub.itemM("TEXT_MT_text")
            if text:
                sub.itemM("TEXT_MT_edit")
                sub.itemM("TEXT_MT_format")

        if text and text.modified:
            row = layout.row()
            # row.color(redalert)
            row.itemO("text.resolve_conflict", text="", icon='ICON_HELP')

        layout.template_ID(st, "text", new="text.new", unlink="text.unlink")

        row = layout.row(align=True)
        row.itemR(st, "line_numbers", text="")
        row.itemR(st, "word_wrap", text="")
        row.itemR(st, "syntax_highlight", text="")

        if text:
            row = layout.row()
            if text.filename != "":
                if text.dirty:
                    row.itemL(text="File: *%s (unsaved)" % text.filename)
                else:
                    row.itemL(text="File: %s" % text.filename)
            else:
                if text.library:
                    row.itemL(text="Text: External")
                else:
                    row.itemL(text="Text: Internal")

        row = layout.row()
        row.itemO("text.run_script")


class TEXT_PT_properties(bpy.types.Panel):
    bl_space_type = 'TEXT_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Properties"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        flow = layout.column_flow()
        flow.itemR(st, "line_numbers")
        flow.itemR(st, "word_wrap")
        flow.itemR(st, "syntax_highlight")
        flow.itemR(st, "live_edit")

        flow = layout.column_flow()
        flow.itemR(st, "font_size")
        flow.itemR(st, "tab_width")


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
        row.itemR(st, "find_text", text="")
        row.itemO("text.find_set_selected", text="", icon='ICON_TEXT')
        col.itemO("text.find")

        # replace
        col = layout.column(align=True)
        row = col.row()
        row.itemR(st, "replace_text", text="")
        row.itemO("text.replace_set_selected", text="", icon='ICON_TEXT')
        col.itemO("text.replace")

        # mark
        layout.itemO("text.mark_all")

        # settings
        row = layout.row()
        row.itemR(st, "find_wrap", text="Wrap")
        row.itemR(st, "find_all", text="All")


class TEXT_MT_text(bpy.types.Menu):
    bl_label = "Text"

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        text = st.text

        layout.column()
        layout.itemO("text.new")
        layout.itemO("text.open")

        if text:
            layout.itemO("text.reload")

            layout.column()
            layout.itemO("text.save")
            layout.itemO("text.save_as")

            if text.filename != "":
                layout.itemO("text.make_internal")

            layout.column()
            layout.itemO("text.run_script")

            #ifndef DISABLE_PYTHON
            # XXX if(BPY_is_pyconstraint(text))
            # XXX   uiMenuItemO(head, 0, "text.refresh_pyconstraints");
            #endif

        layout.itemS()

        layout.itemO("text.properties", icon='ICON_MENU_PANEL')



        #ifndef DISABLE_PYTHON
        # XXX layout.column()
        # XXX uiDefIconTextBlockBut(block, text_template_scriptsmenu, NULL, ICON_RIGHTARROW_THIN, "Script Templates", 0, yco-=20, 120, 19, "");
        # XXX uiDefIconTextBlockBut(block, text_plugin_scriptsmenu, NULL, ICON_RIGHTARROW_THIN, "Text Plugins", 0, yco-=20, 120, 19, "");
        #endif

        layout.itemM("TEXT_MT_templates")


class TEXT_MT_templates(bpy.types.Menu):
    '''
    Creates the menu items by scanning scripts/templates
    '''
    bl_label = "Script Templates"

    def draw(self, context):
        import os

        def path_to_name(f):
            f_base = os.path.splitext(f)[0]
            f_base = f_base.replace("_", " ")
            return ' '.join([w[0].upper() + w[1:] for w in f_base.split()])

        layout = self.layout
        template_dir = os.path.join(os.path.dirname(__file__), os.path.pardir, "templates")

        for f in sorted(os.listdir(template_dir)):

            if f.startswith("."):
                continue

            path = os.path.join(template_dir, f)
            layout.item_stringO("text.open", "path", path, text=path_to_name(f))


class TEXT_MT_edit_view(bpy.types.Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        layout.item_enumO("text.move", "type", 'FILE_TOP', text="Top of File")
        layout.item_enumO("text.move", "type", 'FILE_BOTTOM', text="Bottom of File")


class TEXT_MT_edit_select(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.itemO("text.select_all")
        layout.itemO("text.select_line")


class TEXT_MT_edit_markers(bpy.types.Menu):
    bl_label = "Markers"

    def draw(self, context):
        layout = self.layout

        layout.itemO("text.markers_clear")
        layout.itemO("text.next_marker")
        layout.itemO("text.previous_marker")


class TEXT_MT_format(bpy.types.Menu):
    bl_label = "Format"

    def draw(self, context):
        layout = self.layout

        layout.itemO("text.indent")
        layout.itemO("text.unindent")

        layout.itemS()

        layout.itemO("text.comment")
        layout.itemO("text.uncomment")

        layout.itemS()

        layout.item_menu_enumO("text.convert_whitespace", "type")


class TEXT_MT_edit_to3d(bpy.types.Menu):
    bl_label = "Text To 3D Object"

    def draw(self, context):
        layout = self.layout

        layout.item_booleanO("text.to_3d_object", "split_lines", False, text="One Object")
        layout.item_booleanO("text.to_3d_object", "split_lines", True, text="One Object Per Line")


class TEXT_MT_edit(bpy.types.Menu):
    bl_label = "Edit"

    def poll(self, context):
        return (context.space_data.text)

    def draw(self, context):
        layout = self.layout

        layout.itemO("ed.undo")
        layout.itemO("ed.redo")

        layout.itemS()

        layout.itemO("text.cut")
        layout.itemO("text.copy")
        layout.itemO("text.paste")

        layout.itemS()

        layout.itemM("TEXT_MT_edit_view")
        layout.itemM("TEXT_MT_edit_select")
        layout.itemM("TEXT_MT_edit_markers")

        layout.itemS()

        layout.itemO("text.jump")
        layout.itemO("text.properties", text="Find...")

        layout.itemS()

        layout.itemM("TEXT_MT_edit_to3d")

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
