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
    "name": "UI Classes Overview",
    "author": "lijenstina",
    "version": (1, 0, 1),
    "blender": (2, 78, 0),
    "location": "Text Editor > Properties",
    "description": "Print the UI classes in a text-block",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/"
                "Py/Scripts/Development/Classes_Overview",
    "category": "Development"
    }

import bpy
from bpy.types import (
        Operator,
        Panel,
        PropertyGroup,
        )
from bpy.props import (
        BoolProperty,
        EnumProperty,
        PointerProperty,
        )


class TEXT_PT_ui_cheat_sheet(Panel):
    bl_space_type = "TEXT_EDITOR"
    bl_region_type = "UI"
    bl_label = "UI Cheat Sheet"
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context):
        layout = self.layout
        scene = context.scene.dev_text_ui

        col = layout.column(align=True)
        col.prop(scene, "dev_ui_cheat_type", expand=True)

        row = layout.row(align=True)
        split = row.split(percentage=0.8, align=True)
        split.operator("text.ui_cheat_sheet")
        split.prop(scene, "searchable", text="", icon='SHORTDISPLAY')


class TEXT_OT_ui_cheat_sheet(Operator):
    bl_idname = "text.ui_cheat_sheet"
    bl_label = "Generate UI list"
    bl_description = ("List the UI Menus, Panels and Headers in a textblock\n"
                      "The newly generated list will be made active in the Text Editor\n"
                      "To access the previous ones, select them from the Header dropdown")

    def ui_list_name(self, context):
        new_name, def_name, ext = "", "UIList", ".txt"
        suffix = 1
        try:
            # first slap a simple linear count + 1 for numeric suffix, if it fails
            # harvest for the rightmost numbers and append the max value
            list_txt = []
            data_txt = bpy.data.texts
            list_txt = [txt.name for txt in data_txt if txt.name.startswith("UIList")]
            new_name = "{}_{}{}".format(def_name, len(list_txt) + 1, ext)

            if new_name in list_txt:
                from re import findall
                test_num = [findall("\d+", words) for words in list_txt]
                suffix += max([int(l[-1]) for l in test_num])
                new_name = "{}_{}{}".format(def_name, suffix, ext)
            return new_name
        except:
            return None

    def execute(self, context):
        from collections import defaultdict

        scene = context.scene.dev_text_ui
        ui_type = scene.dev_ui_cheat_type
        op_string_ui = defaultdict(list)
        searchable = scene.searchable

        try:
            file_name = self.ui_list_name(context) or "UIList.txt"
            classes = ["Panel", "Menu", "Header"] if ui_type in "all" else [ui_type]
            string_line = "---\n%s\nModule path: %s" if not searchable else "\n'%s': '%s',"
            print_line = '\n\n\n[%s]\nitems: %d\n' if not searchable else '\n\n%s = {\n# items: %d\n'
            close_line = '\n' if not searchable else '\n}'
            start_note = (
                "\n\n# Classes starting with 'NODE_PT_category_' and 'NODE_MT_category_'\n"
                "# are generated in startup\\nodeitems_builtins.py" if ui_type not in "Header" else ""
                )

            for cls_name in classes:
                cls = getattr(bpy.types, cls_name)
                for op_module in cls.__subclasses__():
                    if op_module:
                        module_name = op_module.__module__
                        module_name = module_name.replace('.', '\\')
                        text = (string_line % (op_module.__name__, module_name))
                        op_string_ui[cls_name].append(text)

            textblock = bpy.data.texts.new(file_name)
            total = sum([len(op_string_ui[cls_name]) for cls_name in classes])
            textblock.write('# %d Total Items' % (total))
            textblock.write(start_note)

            for cls_name in classes:
                textblock.write(print_line % (cls_name, len(op_string_ui[cls_name])))
                textblock.write(('\n' if not searchable else '').join(sorted(op_string_ui[cls_name])))
                textblock.write(close_line)

            context.space_data.text = bpy.data.texts[file_name]
            self.report({'INFO'}, "See %s textblock" % file_name)

            return {'FINISHED'}
        except:
            self.report({'WARNING'},
                        "Failure to write the UI list (Check the console for more info)")
            import traceback
            traceback.print_exc()

            return {'CANCELLED'}


# Properties
class text_ui_cheat_props(PropertyGroup):
    dev_ui_cheat_type = EnumProperty(
        name="Choose a Type",
        description="Choose what Classes to include in a Text-Block",
        items=(('all', "All", "Print all the types"),
               ('Panel', "Panels", "Print Panel UI types"),
               ('Menu', "Menus", "Print Menu UI types"),
               ('Header', "Headers", "Print Header UI types"),
               ),
        default='all',
        )
    searchable = BoolProperty(
        name="Format searchable",
        description="Generate the list as Python dictionary,\n"
                    "using the format Class: Path",
        default=False
        )


# Register
classes = (
        TEXT_OT_ui_cheat_sheet,
        TEXT_PT_ui_cheat_sheet,
        text_ui_cheat_props
        )


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.Scene.dev_text_ui = PointerProperty(type=text_ui_cheat_props)


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)

    del bpy.types.Scene.dev_text_ui


if __name__ == "__main__":
    register()
