# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Header, Menu


class CONSOLE_HT_header(Header):
    bl_space_type = 'CONSOLE'

    def draw(self, context):
        layout = self.layout
        layout.template_header()

        CONSOLE_MT_editor_menus.draw_collapsible(context, layout)


class CONSOLE_MT_editor_menus(Menu):
    bl_idname = "CONSOLE_MT_editor_menus"
    bl_label = ""

    def draw(self, _context):
        layout = self.layout
        layout.menu("CONSOLE_MT_view")
        layout.menu("CONSOLE_MT_console")


class CONSOLE_MT_view(Menu):
    bl_label = "View"

    def draw(self, _context):
        layout = self.layout

        props = layout.operator("wm.context_cycle_int", text="Zoom In")
        props.data_path = "space_data.font_size"
        props.reverse = False
        props = layout.operator("wm.context_cycle_int", text="Zoom Out")
        props.data_path = "space_data.font_size"
        props.reverse = True

        layout.separator()

        layout.operator("console.move", text="Move to Previous Word").type = 'PREVIOUS_WORD'
        layout.operator("console.move", text="Move to Next Word").type = 'NEXT_WORD'
        layout.operator("console.move", text="Move to Line Begin").type = 'LINE_BEGIN'
        layout.operator("console.move", text="Move to Line End").type = 'LINE_END'

        layout.separator()

        layout.menu("CONSOLE_MT_language")

        layout.separator()

        layout.menu("INFO_MT_area")


class CONSOLE_MT_language(Menu):
    bl_label = "Languages..."

    def draw(self, _context):
        import sys

        layout = self.layout
        layout.column()

        # Collect modules with `console_*.execute`.
        languages = []
        for modname, mod in sys.modules.items():
            if modname.startswith("console_") and hasattr(mod, "execute"):
                languages.append(modname.split("_", 1)[-1])

        languages.sort()

        for language in languages:
            layout.operator("console.language",
                            text=language.title(),
                            translate=False).language = language


class CONSOLE_MT_console(Menu):
    bl_label = "Console"

    def draw(self, _context):
        layout = self.layout

        layout.operator("console.clear")
        layout.operator("console.clear_line")
        layout.operator("console.delete", text="Delete Previous Word").type = 'PREVIOUS_WORD'
        layout.operator("console.delete", text="Delete Next Word").type = 'NEXT_WORD'

        layout.separator()

        layout.operator("console.copy_as_script", text="Copy as Script")
        layout.operator("console.copy", text="Cut").delete = True
        layout.operator("console.copy", text="Copy")
        layout.operator("console.paste", text="Paste")

        layout.separator()

        layout.operator("console.indent")
        layout.operator("console.unindent")

        layout.separator()

        layout.operator("console.history_cycle", text="Backward in History").reverse = True
        layout.operator("console.history_cycle", text="Forward in History").reverse = False

        layout.separator()

        layout.operator("console.autocomplete", text="Autocomplete")


class CONSOLE_MT_context_menu(Menu):
    bl_label = "Console"

    def draw(self, _context):
        layout = self.layout

        layout.operator("console.clear")
        layout.operator("console.clear_line")
        layout.operator("console.delete", text="Delete Previous Word").type = 'PREVIOUS_WORD'
        layout.operator("console.delete", text="Delete Next Word").type = 'NEXT_WORD'

        layout.separator()

        layout.operator("console.copy_as_script", text="Copy as Script")
        layout.operator("console.copy", text="Cut").delete = True
        layout.operator("console.copy", text="Copy")
        layout.operator("console.paste", text="Paste")

        layout.separator()

        layout.operator("console.indent")
        layout.operator("console.unindent")

        layout.separator()

        layout.operator("console.history_cycle", text="Backward in History").reverse = True
        layout.operator("console.history_cycle", text="Forward in History").reverse = False

        layout.separator()

        layout.operator("console.autocomplete", text="Autocomplete")


def add_scrollback(text, text_type):
    for l in text.split("\n"):
        bpy.ops.console.scrollback_append(text=l.expandtabs(4), type=text_type)


classes = (
    CONSOLE_HT_header,
    CONSOLE_MT_editor_menus,
    CONSOLE_MT_view,
    CONSOLE_MT_language,
    CONSOLE_MT_console,
    CONSOLE_MT_context_menu,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
