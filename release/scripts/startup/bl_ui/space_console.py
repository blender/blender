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
from bpy.types import Header, Menu


class CONSOLE_HT_header(Header):
    bl_space_type = 'CONSOLE'

    def draw(self, context):
        layout = self.layout.row()

        layout.template_header()

        CONSOLE_MT_editor_menus.draw_collapsible(context, layout)

        layout.operator("console.autocomplete", text="Autocomplete")


class CONSOLE_MT_editor_menus(Menu):
    bl_idname = "CONSOLE_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        layout.menu("CONSOLE_MT_console")


class CONSOLE_MT_console(Menu):
    bl_label = "Console"

    def draw(self, context):
        layout = self.layout

        layout.operator("console.indent")
        layout.operator("console.unindent")

        layout.separator()

        layout.operator("console.clear")
        layout.operator("console.clear_line")

        layout.separator()

        layout.operator("console.copy_as_script")
        layout.operator("console.copy")
        layout.operator("console.paste")
        layout.menu("CONSOLE_MT_language")

        layout.separator()

        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area", text="Toggle Maximize Area")
        layout.operator("screen.screen_full_area").use_hide_panels = True


class CONSOLE_MT_language(Menu):
    bl_label = "Languages..."

    def draw(self, context):
        import sys

        layout = self.layout
        layout.column()

        # Collect modules with 'console_*.execute'
        languages = []
        for modname, mod in sys.modules.items():
            if modname.startswith("console_") and hasattr(mod, "execute"):
                languages.append(modname.split("_", 1)[-1])

        languages.sort()

        for language in languages:
            layout.operator("console.language",
                            text=language.title(),
                            translate=False).language = language


def add_scrollback(text, text_type):
    for l in text.split("\n"):
        bpy.ops.console.scrollback_append(text=l.expandtabs(4),
                                          type=text_type)

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
