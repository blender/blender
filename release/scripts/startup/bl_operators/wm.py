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
from bpy.props import StringProperty, BoolProperty, IntProperty, FloatProperty
from rna_prop_ui import rna_idprop_ui_prop_get, rna_idprop_ui_prop_clear


class MESH_OT_delete_edgeloop(bpy.types.Operator):
    '''Delete an edge loop by merging the faces on each side to a single face loop'''
    bl_idname = "mesh.delete_edgeloop"
    bl_label = "Delete Edge Loop"

    def execute(self, context):
        if 'FINISHED' in bpy.ops.transform.edge_slide(value=1.0):
            bpy.ops.mesh.select_more()
            bpy.ops.mesh.remove_doubles()
            return {'FINISHED'}

        return {'CANCELLED'}

rna_path_prop = StringProperty(name="Context Attributes",
        description="rna context string", maxlen=1024, default="")

rna_reverse_prop = BoolProperty(name="Reverse",
        description="Cycle backwards", default=False)

rna_relative_prop = BoolProperty(name="Relative",
        description="Apply relative to the current value (delta)",
        default=False)


def context_path_validate(context, data_path):
    import sys
    try:
        value = eval("context.%s" % data_path) if data_path else Ellipsis
    except AttributeError:
        if "'NoneType'" in str(sys.exc_info()[1]):
            # One of the items in the rna path is None, just ignore this
            value = Ellipsis
        else:
            # We have a real error in the rna path, dont ignore that
            raise

    return value


def execute_context_assign(self, context):
    if context_path_validate(context, self.data_path) is Ellipsis:
        return {'PASS_THROUGH'}

    if getattr(self, "relative", False):
        exec("context.%s+=self.value" % self.data_path)
    else:
        exec("context.%s=self.value" % self.data_path)

    return {'FINISHED'}


class BRUSH_OT_set_active_number(bpy.types.Operator):
    '''Set active sculpt/paint brush from it's number'''
    bl_idname = "brush.set_active_number"
    bl_label = "Set Brush Number"

    mode = StringProperty(name="mode",
            description="Paint mode to set brush for", maxlen=1024)
    number = IntProperty(name="number",
            description="Brush number")

    _attr_dict = {"sculpt": "use_paint_sculpt",
                  "vertex_paint": "use_paint_vertex",
                  "weight_paint": "use_paint_weight",
                  "image_paint": "use_paint_texture"}

    def execute(self, context):
        attr = self._attr_dict.get(self.mode)
        if attr is None:
            return {'CANCELLED'}

        for i, brush in enumerate((cur for cur in bpy.data.brushes if getattr(cur, attr))):
            if i == self.number:
                getattr(context.tool_settings, self.mode).brush = brush
                return {'FINISHED'}

        return {'CANCELLED'}


class WM_OT_context_set_boolean(bpy.types.Operator):
    '''Set a context value.'''
    bl_idname = "wm.context_set_boolean"
    bl_label = "Context Set Boolean"
    bl_options = {'UNDO'}

    data_path = rna_path_prop
    value = BoolProperty(name="Value",
            description="Assignment value", default=True)

    execute = execute_context_assign


class WM_OT_context_set_int(bpy.types.Operator):  # same as enum
    '''Set a context value.'''
    bl_idname = "wm.context_set_int"
    bl_label = "Context Set"
    bl_options = {'UNDO'}

    data_path = rna_path_prop
    value = IntProperty(name="Value", description="Assign value", default=0)
    relative = rna_relative_prop

    execute = execute_context_assign


class WM_OT_context_scale_int(bpy.types.Operator):
    '''Scale an int context value.'''
    bl_idname = "wm.context_scale_int"
    bl_label = "Context Set"
    bl_options = {'UNDO'}

    data_path = rna_path_prop
    value = FloatProperty(name="Value", description="Assign value", default=1.0)
    always_step = BoolProperty(name="Always Step",
        description="Always adjust the value by a minimum of 1 when 'value' is not 1.0.",
        default=True)

    def execute(self, context):
        if context_path_validate(context, self.data_path) is Ellipsis:
            return {'PASS_THROUGH'}

        value = self.value
        data_path = self.data_path

        if value == 1.0:  # nothing to do
            return {'CANCELLED'}

        if getattr(self, "always_step", False):
            if value > 1.0:
                add = "1"
                func = "max"
            else:
                add = "-1"
                func = "min"
            exec("context.%s = %s(round(context.%s * value), context.%s + %s)" % (data_path, func, data_path, data_path, add))
        else:
            exec("context.%s *= value" % self.data_path)

        return {'FINISHED'}


class WM_OT_context_set_float(bpy.types.Operator):  # same as enum
    '''Set a context value.'''
    bl_idname = "wm.context_set_float"
    bl_label = "Context Set Float"
    bl_options = {'UNDO'}

    data_path = rna_path_prop
    value = FloatProperty(name="Value",
            description="Assignment value", default=0.0)
    relative = rna_relative_prop

    execute = execute_context_assign


class WM_OT_context_set_string(bpy.types.Operator):  # same as enum
    '''Set a context value.'''
    bl_idname = "wm.context_set_string"
    bl_label = "Context Set String"
    bl_options = {'UNDO'}

    data_path = rna_path_prop
    value = StringProperty(name="Value",
            description="Assign value", maxlen=1024, default="")

    execute = execute_context_assign


class WM_OT_context_set_enum(bpy.types.Operator):
    '''Set a context value.'''
    bl_idname = "wm.context_set_enum"
    bl_label = "Context Set Enum"
    bl_options = {'UNDO'}

    data_path = rna_path_prop
    value = StringProperty(name="Value",
            description="Assignment value (as a string)",
            maxlen=1024, default="")

    execute = execute_context_assign


class WM_OT_context_set_value(bpy.types.Operator):
    '''Set a context value.'''
    bl_idname = "wm.context_set_value"
    bl_label = "Context Set Value"
    bl_options = {'UNDO'}

    data_path = rna_path_prop
    value = StringProperty(name="Value",
            description="Assignment value (as a string)",
            maxlen=1024, default="")

    def execute(self, context):
        if context_path_validate(context, self.data_path) is Ellipsis:
            return {'PASS_THROUGH'}
        exec("context.%s=%s" % (self.data_path, self.value))
        return {'FINISHED'}


class WM_OT_context_toggle(bpy.types.Operator):
    '''Toggle a context value.'''
    bl_idname = "wm.context_toggle"
    bl_label = "Context Toggle"
    bl_options = {'UNDO'}

    data_path = rna_path_prop

    def execute(self, context):

        if context_path_validate(context, self.data_path) is Ellipsis:
            return {'PASS_THROUGH'}

        exec("context.%s=not (context.%s)" %
            (self.data_path, self.data_path))

        return {'FINISHED'}


class WM_OT_context_toggle_enum(bpy.types.Operator):
    '''Toggle a context value.'''
    bl_idname = "wm.context_toggle_enum"
    bl_label = "Context Toggle Values"
    bl_options = {'UNDO'}

    data_path = rna_path_prop
    value_1 = StringProperty(name="Value", \
                description="Toggle enum", maxlen=1024, default="")

    value_2 = StringProperty(name="Value", \
                description="Toggle enum", maxlen=1024, default="")

    def execute(self, context):

        if context_path_validate(context, self.data_path) is Ellipsis:
            return {'PASS_THROUGH'}

        exec("context.%s = ['%s', '%s'][context.%s!='%s']" % \
            (self.data_path, self.value_1,\
             self.value_2, self.data_path,
             self.value_2))

        return {'FINISHED'}


class WM_OT_context_cycle_int(bpy.types.Operator):
    '''Set a context value. Useful for cycling active material, '''
    '''vertex keys, groups' etc.'''
    bl_idname = "wm.context_cycle_int"
    bl_label = "Context Int Cycle"
    bl_options = {'UNDO'}

    data_path = rna_path_prop
    reverse = rna_reverse_prop

    def execute(self, context):
        data_path = self.data_path
        value = context_path_validate(context, data_path)
        if value is Ellipsis:
            return {'PASS_THROUGH'}

        if self.reverse:
            value -= 1
        else:
            value += 1

        exec("context.%s=value" % data_path)

        if value != eval("context.%s" % data_path):
            # relies on rna clamping int's out of the range
            if self.reverse:
                value = (1 << 31) - 1
            else:
                value = -1 << 31

            exec("context.%s=value" % data_path)

        return {'FINISHED'}


class WM_OT_context_cycle_enum(bpy.types.Operator):
    '''Toggle a context value.'''
    bl_idname = "wm.context_cycle_enum"
    bl_label = "Context Enum Cycle"
    bl_options = {'UNDO'}

    data_path = rna_path_prop
    reverse = rna_reverse_prop

    def execute(self, context):

        value = context_path_validate(context, self.data_path)
        if value is Ellipsis:
            return {'PASS_THROUGH'}

        orig_value = value

        # Have to get rna enum values
        rna_struct_str, rna_prop_str = self.data_path.rsplit('.', 1)
        i = rna_prop_str.find('[')

        # just incse we get "context.foo.bar[0]"
        if i != -1:
            rna_prop_str = rna_prop_str[0:i]

        rna_struct = eval("context.%s.rna_type" % rna_struct_str)

        rna_prop = rna_struct.properties[rna_prop_str]

        if type(rna_prop) != bpy.types.EnumProperty:
            raise Exception("expected an enum property")

        enums = rna_struct.properties[rna_prop_str].items.keys()
        orig_index = enums.index(orig_value)

        # Have the info we need, advance to the next item
        if self.reverse:
            if orig_index == 0:
                advance_enum = enums[-1]
            else:
                advance_enum = enums[orig_index - 1]
        else:
            if orig_index == len(enums) - 1:
                advance_enum = enums[0]
            else:
                advance_enum = enums[orig_index + 1]

        # set the new value
        exec("context.%s=advance_enum" % self.data_path)
        return {'FINISHED'}


class WM_OT_context_cycle_array(bpy.types.Operator):
    '''Set a context array value.
    Useful for cycling the active mesh edit mode.'''
    bl_idname = "wm.context_cycle_array"
    bl_label = "Context Array Cycle"
    bl_options = {'UNDO'}

    data_path = rna_path_prop
    reverse = rna_reverse_prop

    def execute(self, context):
        data_path = self.data_path
        value = context_path_validate(context, data_path)
        if value is Ellipsis:
            return {'PASS_THROUGH'}

        def cycle(array):
            if self.reverse:
                array.insert(0, array.pop())
            else:
                array.append(array.pop(0))
            return array

        exec("context.%s=cycle(context.%s[:])" % (data_path, data_path))

        return {'FINISHED'}


class WM_MT_context_menu_enum(bpy.types.Menu):
    bl_label = ""
    data_path = ""  # BAD DESIGN, set from operator below.

    def draw(self, context):
        data_path = self.data_path
        value = context_path_validate(bpy.context, data_path)
        if value is Ellipsis:
            return {'PASS_THROUGH'}
        base_path, prop_string = data_path.rsplit(".", 1)
        value_base = context_path_validate(context, base_path)

        values = [(i.name, i.identifier) for i in value_base.bl_rna.properties[prop_string].items]

        for name, identifier in values:
            prop = self.layout.operator("wm.context_set_enum", text=name)
            prop.data_path = data_path
            prop.value = identifier


class WM_OT_context_menu_enum(bpy.types.Operator):
    bl_idname = "wm.context_menu_enum"
    bl_label = "Context Enum Menu"
    bl_options = {'UNDO'}
    data_path = rna_path_prop

    def execute(self, context):
        data_path = self.data_path
        WM_MT_context_menu_enum.data_path = data_path
        bpy.ops.wm.call_menu(name="WM_MT_context_menu_enum")
        return {'PASS_THROUGH'}


class WM_OT_context_set_id(bpy.types.Operator):
    '''Toggle a context value.'''
    bl_idname = "wm.context_set_id"
    bl_label = "Set Library ID"
    bl_options = {'UNDO'}

    data_path = rna_path_prop
    value = StringProperty(name="Value",
            description="Assign value", maxlen=1024, default="")

    def execute(self, context):
        value = self.value
        data_path = self.data_path

        # match the pointer type from the target property to bpy.data.*
        # so we lookup the correct list.
        data_path_base, data_path_prop = data_path.rsplit(".", 1)
        data_prop_rna = eval("context.%s" % data_path_base).rna_type.properties[data_path_prop]
        data_prop_rna_type = data_prop_rna.fixed_type

        id_iter = None

        for prop in bpy.data.rna_type.properties:
            if prop.rna_type.identifier == "CollectionProperty":
                if prop.fixed_type == data_prop_rna_type:
                    id_iter = prop.identifier
                    break

        if id_iter:
            value_id = getattr(bpy.data, id_iter).get(value)
            exec("context.%s=value_id" % data_path)

        return {'FINISHED'}


doc_id = StringProperty(name="Doc ID",
        description="", maxlen=1024, default="", options={'HIDDEN'})

doc_new = StringProperty(name="Edit Description",
        description="", maxlen=1024, default="")


class WM_OT_context_modal_mouse(bpy.types.Operator):
    '''Adjust arbitrary values with mouse input'''
    bl_idname = "wm.context_modal_mouse"
    bl_label = "Context Modal Mouse"

    data_path_iter = StringProperty(description="The data path relative to the context, must point to an iterable.")
    data_path_item = StringProperty(description="The data path from each iterable to the value (int or float)")
    input_scale = FloatProperty(default=0.01, description="Scale the mouse movement by this value before applying the delta")
    invert = BoolProperty(default=False, description="Invert the mouse input")
    initial_x = IntProperty(options={'HIDDEN'})

    def _values_store(self, context):
        data_path_iter = self.data_path_iter
        data_path_item = self.data_path_item

        self._values = values = {}

        for item in getattr(context, data_path_iter):
            try:
                value_orig = eval("item." + data_path_item)
            except:
                continue

            # check this can be set, maybe this is library data.
            try:
                exec("item.%s = %s" % (data_path_item, value_orig))
            except:
                continue

            values[item] = value_orig

    def _values_delta(self, delta):
        delta *= self.input_scale
        if self.invert:
            delta = - delta

        data_path_item = self.data_path_item
        for item, value_orig in self._values.items():
            if type(value_orig) == int:
                exec("item.%s = int(%d)" % (data_path_item, round(value_orig + delta)))
            else:
                exec("item.%s = %f" % (data_path_item, value_orig + delta))

    def _values_restore(self):
        data_path_item = self.data_path_item
        for item, value_orig in self._values.items():
            exec("item.%s = %s" % (data_path_item, value_orig))

        self._values.clear()

    def _values_clear(self):
        self._values.clear()

    def modal(self, context, event):
        event_type = event.type

        if event_type == 'MOUSEMOVE':
            delta = event.mouse_x - self.initial_x
            self._values_delta(delta)

        elif 'LEFTMOUSE' == event_type:
            self._values_clear()
            return {'FINISHED'}

        elif event_type in ('RIGHTMOUSE', 'ESC'):
            self._values_restore()
            return {'FINISHED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        self._values_store(context)

        if not self._values:
            self.report({'WARNING'}, "Nothing to operate on: %s[ ].%s" %
                    (self.data_path_iter, self.data_path_item))

            return {'CANCELLED'}
        else:
            self.initial_x = event.mouse_x

            context.window_manager.modal_handler_add(self)
            return {'RUNNING_MODAL'}


class WM_OT_url_open(bpy.types.Operator):
    "Open a website in the Webbrowser"
    bl_idname = "wm.url_open"
    bl_label = ""

    url = StringProperty(name="URL", description="URL to open")

    def execute(self, context):
        import webbrowser
        _webbrowser_bug_fix()
        webbrowser.open(self.url)
        return {'FINISHED'}


class WM_OT_path_open(bpy.types.Operator):
    "Open a path in a file browser"
    bl_idname = "wm.path_open"
    bl_label = ""

    filepath = StringProperty(name="File Path", maxlen=1024, subtype='FILE_PATH')

    def execute(self, context):
        import sys
        import os
        import subprocess

        filepath = bpy.path.abspath(self.filepath)
        filepath = os.path.normpath(filepath)

        if not os.path.exists(filepath):
            self.report({'ERROR'}, "File '%s' not found" % filepath)
            return {'CANCELLED'}

        if sys.platform[:3] == "win":
            subprocess.Popen(['start', filepath], shell=True)
        elif sys.platform == 'darwin':
            subprocess.Popen(['open', filepath])
        else:
            try:
                subprocess.Popen(['xdg-open', filepath])
            except OSError:
                # xdg-open *should* be supported by recent Gnome, KDE, Xfce
                pass

        return {'FINISHED'}


class WM_OT_doc_view(bpy.types.Operator):
    '''Load online reference docs'''
    bl_idname = "wm.doc_view"
    bl_label = "View Documentation"

    doc_id = doc_id
    _prefix = "http://www.blender.org/documentation/blender_python_api_%s" % "_".join(str(v) for v in bpy.app.version)

    def _nested_class_string(self, class_string):
        ls = []
        class_obj = getattr(bpy.types, class_string, None).bl_rna
        while class_obj:
            ls.insert(0, class_obj)
            class_obj = class_obj.nested
        return '.'.join(class_obj.identifier for class_obj in ls)

    def execute(self, context):
        id_split = self.doc_id.split('.')
        if len(id_split) == 1:  # rna, class
            url = '%s/bpy.types.%s.html' % (self._prefix, id_split[0])
        elif len(id_split) == 2:  # rna, class.prop
            class_name, class_prop = id_split

            if hasattr(bpy.types, class_name.upper() + '_OT_' + class_prop):
                url = '%s/bpy.ops.%s.html#bpy.ops.%s.%s' % \
                        (self._prefix, class_name, class_name, class_prop)
            else:

                # detect if this is a inherited member and use that name instead
                rna_parent = getattr(bpy.types, class_name).bl_rna
                rna_prop = rna_parent.properties[class_prop]
                rna_parent = rna_parent.base
                while rna_parent and rna_prop == rna_parent.properties.get(class_prop):
                    class_name = rna_parent.identifier
                    rna_parent = rna_parent.base

                # It so happens that epydoc nests these, not sphinx
                # class_name_full = self._nested_class_string(class_name)
                url = '%s/bpy.types.%s.html#bpy.types.%s.%s' % \
                        (self._prefix, class_name, class_name, class_prop)

        else:
            return {'PASS_THROUGH'}

        import webbrowser
        _webbrowser_bug_fix()
        webbrowser.open(url)

        return {'FINISHED'}


class WM_OT_doc_edit(bpy.types.Operator):
    '''Load online reference docs'''
    bl_idname = "wm.doc_edit"
    bl_label = "Edit Documentation"

    doc_id = doc_id
    doc_new = doc_new

    _url = "http://www.mindrones.com/blender/svn/xmlrpc.php"

    def _send_xmlrpc(self, data_dict):
        print("sending data:", data_dict)

        import xmlrpc.client
        user = 'blenderuser'
        pwd = 'blender>user'

        docblog = xmlrpc.client.ServerProxy(self._url)
        docblog.metaWeblog.newPost(1, user, pwd, data_dict, 1)

    def execute(self, context):

        doc_id = self.doc_id
        doc_new = self.doc_new

        class_name, class_prop = doc_id.split('.')

        if not doc_new:
            self.report({'ERROR'}, "No input given for '%s'" % doc_id)
            return {'CANCELLED'}

        # check if this is an operator
        op_name = class_name.upper() + '_OT_' + class_prop
        op_class = getattr(bpy.types, op_name, None)

        # Upload this to the web server
        upload = {}

        if op_class:
            rna = op_class.bl_rna
            doc_orig = rna.description
            if doc_orig == doc_new:
                return {'RUNNING_MODAL'}

            print("op - old:'%s' -> new:'%s'" % (doc_orig, doc_new))
            upload["title"] = 'OPERATOR %s:%s' % (doc_id, doc_orig)
        else:
            rna = getattr(bpy.types, class_name).bl_rna
            doc_orig = rna.properties[class_prop].description
            if doc_orig == doc_new:
                return {'RUNNING_MODAL'}

            print("rna - old:'%s' -> new:'%s'" % (doc_orig, doc_new))
            upload["title"] = 'RNA %s:%s' % (doc_id, doc_orig)

        upload["description"] = doc_new

        self._send_xmlrpc(upload)

        return {'FINISHED'}

    def draw(self, context):
        layout = self.layout
        layout.label(text="Descriptor ID: '%s'" % self.doc_id)
        layout.prop(self, "doc_new", text="")

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=600)


rna_path = StringProperty(name="Property Edit",
    description="Property data_path edit", maxlen=1024, default="", options={'HIDDEN'})

rna_value = StringProperty(name="Property Value",
    description="Property value edit", maxlen=1024, default="")

rna_property = StringProperty(name="Property Name",
    description="Property name edit", maxlen=1024, default="")

rna_min = FloatProperty(name="Min", default=0.0, precision=3)
rna_max = FloatProperty(name="Max", default=1.0, precision=3)


class WM_OT_properties_edit(bpy.types.Operator):
    '''Internal use (edit a property data_path)'''
    bl_idname = "wm.properties_edit"
    bl_label = "Edit Property"
    bl_options = {'REGISTER'}  # only because invoke_props_popup requires.

    data_path = rna_path
    property = rna_property
    value = rna_value
    min = rna_min
    max = rna_max
    description = StringProperty(name="Tip", default="")

    def execute(self, context):
        data_path = self.data_path
        value = self.value
        prop = self.property

        prop_old = getattr(self, "_last_prop", [None])[0]

        if prop_old is None:
            self.report({'ERROR'}, "Direct execution not supported")
            return {'CANCELLED'}

        try:
            value_eval = eval(value)
        except:
            value_eval = value

        # First remove
        item = eval("context.%s" % data_path)

        rna_idprop_ui_prop_clear(item, prop_old)
        exec_str = "del item['%s']" % prop_old
        # print(exec_str)
        exec(exec_str)

        # Reassign
        exec_str = "item['%s'] = %s" % (prop, repr(value_eval))
        # print(exec_str)
        exec(exec_str)
        self._last_prop[:] = [prop]

        prop_type = type(item[prop])

        prop_ui = rna_idprop_ui_prop_get(item, prop)

        if prop_type in (float, int):

            prop_ui['soft_min'] = prop_ui['min'] = prop_type(self.min)
            prop_ui['soft_max'] = prop_ui['max'] = prop_type(self.max)

        prop_ui['description'] = self.description

        # otherwise existing buttons which reference freed
        # memory may crash blender [#26510]
        # context.area.tag_redraw()
        for win in context.window_manager.windows:
            for area in win.screen.areas:
                area.tag_redraw()

        return {'FINISHED'}

    def invoke(self, context, event):

        if not self.data_path:
            self.report({'ERROR'}, "Data path not set")
            return {'CANCELLED'}

        self._last_prop = [self.property]

        item = eval("context.%s" % self.data_path)

        # setup defaults
        prop_ui = rna_idprop_ui_prop_get(item, self.property, False)  # dont create
        if prop_ui:
            self.min = prop_ui.get("min", -1000000000)
            self.max = prop_ui.get("max", 1000000000)
            self.description = prop_ui.get("description", "")

        wm = context.window_manager
        return wm.invoke_props_dialog(self)


class WM_OT_properties_add(bpy.types.Operator):
    '''Internal use (edit a property data_path)'''
    bl_idname = "wm.properties_add"
    bl_label = "Add Property"

    data_path = rna_path

    def execute(self, context):
        item = eval("context.%s" % self.data_path)

        def unique_name(names):
            prop = 'prop'
            prop_new = prop
            i = 1
            while prop_new in names:
                prop_new = prop + str(i)
                i += 1

            return prop_new

        property = unique_name(item.keys())

        item[property] = 1.0
        return {'FINISHED'}


class WM_OT_properties_remove(bpy.types.Operator):
    '''Internal use (edit a property data_path)'''
    bl_idname = "wm.properties_remove"
    bl_label = "Remove Property"

    data_path = rna_path
    property = rna_property

    def execute(self, context):
        item = eval("context.%s" % self.data_path)
        del item[self.property]
        return {'FINISHED'}


class WM_OT_keyconfig_activate(bpy.types.Operator):
    bl_idname = "wm.keyconfig_activate"
    bl_label = "Activate Keyconfig"

    filepath = StringProperty(name="File Path", maxlen=1024)

    def execute(self, context):
        bpy.utils.keyconfig_set(self.filepath)
        return {'FINISHED'}


class WM_OT_sysinfo(bpy.types.Operator):
    '''Generate System Info'''
    bl_idname = "wm.sysinfo"
    bl_label = "System Info"

    def execute(self, context):
        import sys_info
        sys_info.write_sysinfo(self)
        return {'FINISHED'}


def _webbrowser_bug_fix():
    # test for X11
    import os

    if os.environ.get("DISPLAY"):

        # BSD licenced code copied from python, temp fix for bug
        # http://bugs.python.org/issue11432, XXX == added code
        def _invoke(self, args, remote, autoraise):
            # XXX, added imports
            import io
            import subprocess
            import time

            raise_opt = []
            if remote and self.raise_opts:
                # use autoraise argument only for remote invocation
                autoraise = int(autoraise)
                opt = self.raise_opts[autoraise]
                if opt:
                    raise_opt = [opt]

            cmdline = [self.name] + raise_opt + args

            if remote or self.background:
                inout = io.open(os.devnull, "r+")
            else:
                # for TTY browsers, we need stdin/out
                inout = None
            # if possible, put browser in separate process group, so
            # keyboard interrupts don't affect browser as well as Python
            setsid = getattr(os, 'setsid', None)
            if not setsid:
                setsid = getattr(os, 'setpgrp', None)

            p = subprocess.Popen(cmdline, close_fds=True,  # XXX, stdin=inout,
                                 stdout=(self.redirect_stdout and inout or None),
                                 stderr=inout, preexec_fn=setsid)
            if remote:
                # wait five secons. If the subprocess is not finished, the
                # remote invocation has (hopefully) started a new instance.
                time.sleep(1)
                rc = p.poll()
                if rc is None:
                    time.sleep(4)
                    rc = p.poll()
                    if rc is None:
                        return True
                # if remote call failed, open() will try direct invocation
                return not rc
            elif self.background:
                if p.poll() is None:
                    return True
                else:
                    return False
            else:
                return not p.wait()

        import webbrowser
        webbrowser.UnixBrowser._invoke = _invoke
