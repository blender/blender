# ##### BEGIN GPL LICENSE BLOCK #####
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENCE BLOCK #####

bl_info = {
    "name": "API Navigator",
    "author": "Dany Lebel (Axon_D)",
    "version": (1, 0, 4),
    "blender": (2, 57, 0),
    "location": "Text Editor > Properties > API Navigator Panel",
    "description": "Allows exploration of the python api via the user interface",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Text_Editor/API_Navigator",
    "category": "Development",
}

"""
    You can browse through the tree structure of the api. Each child object appears in a list
that tries to be representative of its type. These lists are :

    * Items (for an iterable object)
    * Item Values (for an iterable object wich only supports index)
    * Modules
    * Types
    * Properties
    * Structs and Functions
    * Methods and Functions
    * Attributes
    * Inaccessible (some objects may be listed but inaccessible)

    The lists can be filtered to help searching in the tree. Just enter the text in the
filter section. It is also possible to explore other modules. Go the the root and select
it in the list of available modules. It will be imported dynamically.

    In the text section, some informations are displayed. The type of the object,
what it returns, and its docstring. We could hope that these docstrings will be as
descriptive as possible. This text data block named api_doc_ can be toggled on and off
with the Escape key. (but a bug prevent the keymap to register correctly at start)

"""

import bpy
from bpy.types import (
        Operator,
        Panel,
        PropertyGroup,
        )
from bpy.props import (
        BoolVectorProperty,
        StringProperty,
        IntProperty,
        PointerProperty,
        )
from console.complete_import import get_root_modules


# ########## Global Variables ##########

last_text = None       # last text data block
root_module = None     # root module of the tree
root_m_path = ''       # root_module + path as a string
current_module = None  # the object itself in the tree structure
tree_level = None      # the list of objects from the current_module


def init_tree_level():
    global tree_level
    tree_level = [[], [], [], [], [], [], [], [], []]


init_tree_level()

api_doc_ = ''       # the documentation formated for the API Navigator
module_type = None  # the type of current_module
return_report = ''  # what current_module returns
filter_mem = {}     # remember last filters entered for each path
too_long = False    # is tree_level list too long to display in a panel?


# ##########   Functions   ############
def get_root_module(path):
    global root_module
    if '.' in path:
        root = path[:path.find('.')]
    else:
        root = path
    try:
        root_module = __import__(root)
    except:
        root_module = None


def evaluate(module):
    global root_module, tree_level, root_m_path

    try:
        len_name = root_module.__name__.__len__()
        root_m_path = 'root_module' + module[len_name:]
        current_module = eval(root_m_path)
        return current_module
    except:
        init_tree_level
        return None


def get_tree_level():

    path = bpy.context.window_manager.api_nav_props.path

    def object_list():
        global current_module, root_m_path

        itm, val, mod, typ, props, struct, met, att, bug = [], [], [], [], [], [], [], [], []
        iterable = isiterable(current_module)
        if iterable:
            iter(current_module)
            current_type = str(module_type)
            if current_type != "<class 'str'>":
                if iterable == 'a':
                    itm = list(current_module.keys())
                    if not itm:
                        val = list(current_module)
                else:
                    val = list(current_module)

        for i in dir(current_module):
            try:
                t = str(type(eval(root_m_path + '.' + i)))
            except (AttributeError, SyntaxError):
                bug += [i]
                continue

            if t == "<class 'module'>":
                mod += [i]
            elif t[0:16] == "<class 'bpy_prop":
                props += [i]
            elif t[8:11] == 'bpy':
                struct += [i]
            elif t == "<class 'builtin_function_or_method'>":
                met += [i]
            elif t == "<class 'type'>":
                typ += [i]
            else:
                att += [i]

        return [itm, val, mod, typ, props, struct, met, att, bug]

    if not path:
        return [[], [], [i for i in get_root_modules()], [], [], [], [], [], []]
    return object_list()


def parent(path):
    """Returns the parent path"""
    parent = path
    if parent[-1] == ']' and '[' in parent:
        while parent[-1] != '[':
            parent = parent[:-1]
    elif '.' in parent:
        while parent[-1] != '.':
            parent = parent[:-1]
    else:
        return ''
    parent = parent[:-1]
    return parent


def update_filter():
    """Update the filter according to the current path"""
    global filter_mem

    try:
        bpy.context.window_manager.api_nav_props.filters = filter_mem[
                                                            bpy.context.window_manager.api_nav_props.path
                                                            ]
    except:
        bpy.context.window_manager.api_nav_props.filters = ''


def isiterable(mod):
    try:
        iter(mod)
    except:
        return False
    try:
        mod['']
        return 'a'
    except KeyError:
        return 'a'
    except (AttributeError, TypeError):
        return 'b'


def fill_filter_mem():
    global filter_mem

    filters = bpy.context.window_manager.api_nav_props.filters
    if filters:
        filter_mem[bpy.context.window_manager.api_nav_props.old_path] = \
                bpy.context.window_manager.api_nav_props.filters
    else:
        filter_mem.pop(bpy.context.window_manager.api_nav_props.old_path, None)


# #### API Navigator parent class ######
class ApiNavigator():
    """Parent class for API Navigator"""

    @staticmethod
    def generate_global_values():
        """Populate the level attributes to display the panel buttons and the documentation"""
        global tree_level, current_module, module_type, return_report, last_text

        text = bpy.context.space_data.text
        if text:
            if text.name != 'api_doc_':
                last_text = bpy.context.space_data.text.name
            elif bpy.data.texts.__len__() < 2:
                last_text = None
        else:
            last_text = None
        bpy.context.window_manager.api_nav_props.pages = 0
        get_root_module(bpy.context.window_manager.api_nav_props.path)
        current_module = evaluate(bpy.context.window_manager.api_nav_props.path)
        module_type = str(type(current_module))
        return_report = str(current_module)
        tree_level = get_tree_level()

        if tree_level.__len__() > 30:
            global too_long
            too_long = True
        else:
            too_long = False

        ApiNavigator.generate_api_doc()
        return {'FINISHED'}

    @staticmethod
    def generate_api_doc():
        """Format the doc string for API Navigator"""
        global current_module, api_doc_, return_report, module_type

        path = bpy.context.window_manager.api_nav_props.path
        line = "-" * (path.__len__() + 2)
        header = """\n\n\n\t\t%s\n\t   %s\n\
_____________________________________________\n\
\n\
Type : %s\n\
\n\
\n\
Return : %s\n\
_____________________________________________\n\
\n\
Doc:
\n\
""" % (path, line, module_type, return_report)
        footer = "\n\
_____________________________________________\n\
\n\
\n\
\n\
#############################################\n\
#                  api_doc_                 #\n\
#            Escape to toggle text          #\n\
#  (F8 to reload modules if doesn't work)   #\n\
#############################################"
        doc = current_module.__doc__
        api_doc_ = header + str(doc) + footer
        return {'FINISHED'}

    @staticmethod
    def doc_text_datablock():
        """Create the text databloc or overwrite it if it already exist"""
        global api_doc_

        space_data = bpy.context.space_data

        try:
            doc_text = bpy.data.texts['api_doc_']
            space_data.text = doc_text
            doc_text.clear()
        except:
            bpy.data.texts.new(name='api_doc_')
            doc_text = bpy.data.texts['api_doc_']
            space_data.text = doc_text

        doc_text.write(text=api_doc_)
        return {'FINISHED'}


# ######### Operators ###########
def api_update(context):
    if bpy.context.window_manager.api_nav_props.path != bpy.context.window_manager.api_nav_props.old_path:
        fill_filter_mem()
        bpy.context.window_manager.api_nav_props.old_path = bpy.context.window_manager.api_nav_props.path
        update_filter()
        ApiNavigator.generate_global_values()
        ApiNavigator.doc_text_datablock()


class Update(ApiNavigator, Operator):
    """Update the tree structure"""
    bl_idname = "api_navigator.update"
    bl_label = "API Navigator Update"

    def execute(self, context):
        api_update()
        return {'FINISHED'}


class BackToBpy(ApiNavigator, Operator):
    """Go back to module bpy"""
    bl_idname = "api_navigator.back_to_bpy"
    bl_label = "Back to bpy"

    def execute(self, context):
        fill_filter_mem()
        if not bpy.context.window_manager.api_nav_props.path:
            bpy.context.window_manager.api_nav_props.old_path = \
                    bpy.context.window_manager.api_nav_props.path = 'bpy'
        else:
            bpy.context.window_manager.api_nav_props.old_path = \
                    bpy.context.window_manager.api_nav_props.path = 'bpy'
        update_filter()
        self.generate_global_values()
        self.doc_text_datablock()
        return {'FINISHED'}


class Down(ApiNavigator, Operator):
    """Go to this Module"""
    bl_idname = "api_navigator.down"
    bl_label = "API Navigator Down"

    pointed_module = StringProperty(
                        name="Current Module",
                        default=""
                        )

    def execute(self, context):
        fill_filter_mem()

        if not bpy.context.window_manager.api_nav_props.path:
            bpy.context.window_manager.api_nav_props.old_path = \
            bpy.context.window_manager.api_nav_props.path = \
            bpy.context.window_manager.api_nav_props.path + self.pointed_module
        else:
            bpy.context.window_manager.api_nav_props.old_path = \
            bpy.context.window_manager.api_nav_props.path = \
            bpy.context.window_manager.api_nav_props.path + '.' + self.pointed_module

        update_filter()
        self.generate_global_values()
        self.doc_text_datablock()
        return {'FINISHED'}


class Parent(ApiNavigator, Operator):
    """Go to Parent Module"""
    bl_idname = "api_navigator.parent"
    bl_label = "API Navigator Parent"

    def execute(self, context):
        path = bpy.context.window_manager.api_nav_props.path

        if path:
            fill_filter_mem()
            bpy.context.window_manager.api_nav_props.old_path = \
            bpy.context.window_manager.api_nav_props.path = \
            parent(bpy.context.window_manager.api_nav_props.path)
            update_filter()
            self.generate_global_values()
            self.doc_text_datablock()

        return {'FINISHED'}


class ClearFilter(ApiNavigator, Operator):
    """Clear the filter"""
    bl_idname = "api_navigator.clear_filter"
    bl_label = "API Nav clear filter"

    def execute(self, context):
        bpy.context.window_manager.api_nav_props.filters = ''
        return {'FINISHED'}


class Subscript(ApiNavigator, Operator):
    """Subscript to this Item"""
    bl_idname = "api_navigator.subscript"
    bl_label = "API Navigator Subscript"

    subscription = StringProperty(
                        name="",
                        default=""
                        )

    def execute(self, context):
        fill_filter_mem()
        bpy.context.window_manager.api_nav_props.old_path = \
        bpy.context.window_manager.api_nav_props.path = \
        bpy.context.window_manager.api_nav_props.path + '[' + self.subscription + ']'
        update_filter()
        self.generate_global_values()
        self.doc_text_datablock()
        return {'FINISHED'}


class Toggle_doc(ApiNavigator, Operator):
    """Toggle on or off api_doc_ Text"""
    bl_idname = "api_navigator.toggle_doc"
    bl_label = "Toggle api_doc_"

    def execute(self, context):
        global last_text

        try:
            if bpy.context.space_data.text.name != "api_doc_":
                last_text = bpy.context.space_data.text.name
        except:
            pass

        try:
            text = bpy.data.texts["api_doc_"]
            bpy.data.texts["api_doc_"].clear()
            bpy.data.texts.remove(text)
        except KeyError:
            self.doc_text_datablock()
            return {'FINISHED'}

        try:
            text = bpy.data.texts[last_text]
            bpy.context.space_data.text = text
            return {'FINISHED'}
        except:
            pass

        bpy.context.space_data.text = None

        return {'FINISHED'}


# ######### UI Panels ############
class OBJECT_PT_api_navigator(ApiNavigator, Panel):
    bl_idname = 'api_navigator'
    bl_space_type = "TEXT_EDITOR"
    bl_region_type = "UI"
    bl_label = "API Navigator"
    bl_options = {'DEFAULT_CLOSED'}

    columns = 3

    def iterable_draw(self):
        # Note: Currently unused method
        global tree_level, current_module

        iterable = isiterable(current_module)

        if iterable:
            iter(current_module)
            current_type = str(module_type)

            if current_type == "<class 'str'>":
                return {'FINISHED'}

            col = self.layout
            reduce_to = bpy.context.window_manager.api_nav_props.reduce_to * self.columns
            pages = bpy.context.window_manager.api_nav_props.pages
            page_index = reduce_to * pages
            count = 0
            i = 0
            filtered = 0

            if iterable == 'a':
                current_type.__iter__()
                collection = list(current_module.keys())
                end = collection.__len__()
                box = self.layout.box()
                row = box.row()
                row.label(text="Items", icon="DOTSDOWN")
                box = box.box()
                col = box.column(align=True)

                while count < reduce_to and i < end:
                    mod = collection[i]
                    if filtered < page_index:
                        filtered += 1
                        i += 1
                        continue

                    if not (i % self.columns):
                        row = col.row()
                    row.operator("api_navigator.subscript",
                                 text=mod, emboss=True).subscription = '"' + mod + '"'
                    filtered += 1
                    i += 1
                    count += 1

            elif iterable == 'b':
                box = self.layout.box()
                row = box.row()
                row.label(text="Item Values", icon="OOPS")
                box = box.box()
                col = box.column(align=True)
                collection = list(current_module)
                end = collection.__len__()

                while count < reduce_to and i < end:
                    mod = str(collection[i])
                    if filtered < page_index:
                        filtered += 1
                        i += 1
                        continue

                    if not (i % self.columns):
                        row = col.row()
                    row.operator("api_navigator.subscript",
                                 text=mod, emboss=True).subscription = str(i)
                    filtered += 1
                    i += 1
                    count += 1

            too_long = end > 30

            if too_long:
                row = col.row()
                row.prop(bpy.context.window_manager.api_nav_props, "reduce_to")
                row.label(text="", icon="DOTSDOWN")
                row.prop(bpy.context.window_manager.api_nav_props, "pages", text="Pages")

        return {'FINISHED'}

    def list_draw(self, t, pages, icon, label=None, emboss=True):
        global tree_level, current_module

        def reduced(row, too_long):
            if row and too_long:
                sub_row = row.row(align=True)
                sub_row.prop(bpy.context.window_manager.api_nav_props, "reduce_to")
                sub_row.prop(bpy.context.window_manager.api_nav_props, "pages", text="Pages")

        layout = self.layout
        filters = bpy.context.window_manager.api_nav_props.filters
        reduce_to = bpy.context.window_manager.api_nav_props.reduce_to * self.columns
        page_index = reduce_to * pages
        show_panel_elements = bpy.context.window_manager.api_nav_props.panel_toggle[t] if \
                              0 <= t < 10 else True

        lenght = tree_level[t].__len__()
        too_long = lenght > reduce_to

        if lenght:
            col = layout.column()
            box = col.box()

            title_box = box.row(align=True)
            title_box.prop(bpy.context.window_manager.api_nav_props,
                           "panel_toggle", text="", index=t if 0 <= t < 10 else 0)
            title_box.label(text=label, icon=icon)
            reduced(box, too_long)

            if show_panel_elements:
                if t < 2:
                    box = box.box()
                row = box.row()
                col = row.column(align=True)
                i = 0
                objects, count, filtered = 0, 0, 0

                while count < reduce_to and i < lenght:
                    obj = tree_level[t][i]

                    if filters and filters not in obj:
                        i += 1
                        continue
                    elif filtered < page_index:
                        filtered += 1
                        i += 1
                        continue

                    if not (objects % self.columns):
                        row = col.row(align=True)
                    if t > 1:
                        row.operator("api_navigator.down",
                                      text=obj, emboss=emboss).pointed_module = obj
                    elif t == 0:
                        row.operator("api_navigator.subscript",
                                     text=str(obj), emboss=emboss).subscription = '"' + obj + '"'
                    else:
                        row.operator("api_navigator.subscript",
                                     text=str(obj), emboss=emboss).subscription = str(i)
                    filtered += 1
                    i += 1
                    objects += 1
                    count += 1

        return {'FINISHED'}

    def draw(self, context):
        global tree_level, current_module, module_type, return_report

        api_update(context)

        layout = self.layout
        layout.label(text="Tree Structure:")

        col = layout.column(align=True)
        col.prop(bpy.context.window_manager.api_nav_props, 'path', text='')

        row = col.row(align=True)
        row.operator("api_navigator.parent", text="Parent", icon="BACK")
        row.operator("api_navigator.back_to_bpy", text='', emboss=True, icon="FILE_PARENT")

        col = layout.column()
        row = col.row(align=True)
        row.prop(bpy.context.window_manager.api_nav_props, "filters", text="Filter")
        row.operator("api_navigator.clear_filter", text="", icon="PANEL_CLOSE")

        col = layout.column()

        pages = bpy.context.window_manager.api_nav_props.pages
        self.list_draw(0, pages, "DOTSDOWN", label="Items")
        self.list_draw(1, pages, "DOTSDOWN", label="Item Values")
        self.list_draw(2, pages, "PACKAGE", label="Modules", emboss=True)
        self.list_draw(3, pages, "WORDWRAP_ON", label="Types", emboss=True)
        self.list_draw(4, pages, "BUTS", label="Properties", emboss=True)
        self.list_draw(5, pages, "OOPS", label="Structs and Functions")
        self.list_draw(6, pages, "SCRIPTWIN", label="Methods and Functions")
        self.list_draw(7, pages, "INFO", label="Attributes")
        self.list_draw(8, pages, "ERROR", label="Inaccessible")


# ###### Properties #######
class ApiNavProps(PropertyGroup):
    """
    Fake module like class.

    bpy.context.window_manager.api_nav_props
    """
    path = StringProperty(
            name="Path",
            description="Enter bpy.ops.api_navigator to see the documentation",
            default="bpy"
            )
    old_path = StringProperty(
            name="Old Path",
            default=""
            )
    filters = StringProperty(
            name="Filters",
            description="Filter the resulting modules",
            default=""
            )
    reduce_to = IntProperty(
            name="Reduce to",
            description="Display a maximum number of x entries by pages",
            default=10,
            min=1
            )
    pages = IntProperty(
            name="Pages",
            description="Display a Page",
            default=0,
            min=0
            )
    panel_toggle = BoolVectorProperty(
            name="Tab",
            description="Expand/Collapse UI elements",
            default=(True,) * 9,
            size=9,
            )


# ######## Register #########
def register_keymaps():
    kc = bpy.context.window_manager.keyconfigs.addon
    if kc:
        km = kc.keymaps.new(name="Text", space_type='TEXT_EDITOR')
        km.keymap_items.new('api_navigator.toggle_doc', 'ESC', 'PRESS')


def unregister_keymaps():
    kc = bpy.context.window_manager.keyconfigs.addon
    if kc:
        km = kc.keymaps["Text"]
        kmi = km.keymap_items["api_navigator.toggle_doc"]
        km.keymap_items.remove(kmi)


def register():
    bpy.utils.register_module(__name__)
    bpy.types.WindowManager.api_nav_props = PointerProperty(
                                                type=ApiNavProps,
                                                name="API Nav Props",
                                                description=""
                                                )
    register_keymaps()


def unregister():
    unregister_keymaps()
    del bpy.types.WindowManager.api_nav_props

    bpy.utils.unregister_module(__name__)


if __name__ == '__main__':
    register()
