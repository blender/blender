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
import os
import re
import shutil

KM_HIERARCHY = [
    ('Window', 'EMPTY', 'WINDOW', []), # file save, window change, exit
    ('Screen', 'EMPTY', 'WINDOW', [    # full screen, undo, screenshot
        ('Screen Editing', 'EMPTY', 'WINDOW', []),    # resizing, action corners
        ]),

    ('View2D', 'EMPTY', 'WINDOW', []),    # view 2d navigation (per region)
    ('View2D Buttons List', 'EMPTY', 'WINDOW', []), # view 2d with buttons navigation
    ('Header', 'EMPTY', 'WINDOW', []),    # header stuff (per region)
    ('Grease Pencil', 'EMPTY', 'WINDOW', []), # grease pencil stuff (per region)

    ('3D View', 'VIEW_3D', 'WINDOW', [ # view 3d navigation and generic stuff (select, transform)
        ('Object Mode', 'EMPTY', 'WINDOW', []),
        ('Mesh', 'EMPTY', 'WINDOW', []),
        ('Curve', 'EMPTY', 'WINDOW', []),
        ('Armature', 'EMPTY', 'WINDOW', []),
        ('Metaball', 'EMPTY', 'WINDOW', []),
        ('Lattice', 'EMPTY', 'WINDOW', []),
        ('Font', 'EMPTY', 'WINDOW', []),

        ('Pose', 'EMPTY', 'WINDOW', []),

        ('Vertex Paint', 'EMPTY', 'WINDOW', []),
        ('Weight Paint', 'EMPTY', 'WINDOW', []),
        ('Face Mask', 'EMPTY', 'WINDOW', []),
        ('Image Paint', 'EMPTY', 'WINDOW', []), # image and view3d
        ('Sculpt', 'EMPTY', 'WINDOW', []),

        ('Armature Sketch', 'EMPTY', 'WINDOW', []),
        ('Particle', 'EMPTY', 'WINDOW', []),

        ('Object Non-modal', 'EMPTY', 'WINDOW', []), # mode change

        ('3D View Generic', 'VIEW_3D', 'WINDOW', [])    # toolbar and properties
        ]),

    ('Frames', 'EMPTY', 'WINDOW', []),    # frame navigation (per region)
    ('Markers', 'EMPTY', 'WINDOW', []),    # markers (per region)
    ('Animation', 'EMPTY', 'WINDOW', []),    # frame change on click, preview range (per region)
    ('Animation Channels', 'EMPTY', 'WINDOW', []),
    ('Graph Editor', 'GRAPH_EDITOR', 'WINDOW', [
        ('Graph Editor Generic', 'GRAPH_EDITOR', 'WINDOW', [])
        ]),
    ('Dopesheet', 'DOPESHEET_EDITOR', 'WINDOW', []),
    ('NLA Editor', 'NLA_EDITOR', 'WINDOW', [
        ('NLA Channels', 'NLA_EDITOR', 'WINDOW', []),
        ('NLA Generic', 'NLA_EDITOR', 'WINDOW', [])
        ]),

    ('Image', 'IMAGE_EDITOR', 'WINDOW', [
        ('UV Editor', 'EMPTY', 'WINDOW', []), # image (reverse order, UVEdit before Image
        ('Image Paint', 'EMPTY', 'WINDOW', []), # image and view3d
        ('Image Generic', 'IMAGE_EDITOR', 'WINDOW', [])
        ]),

    ('Timeline', 'TIMELINE', 'WINDOW', []),
    ('Outliner', 'OUTLINER', 'WINDOW', []),

    ('Node Editor', 'NODE_EDITOR', 'WINDOW', [
        ('Node Generic', 'NODE_EDITOR', 'WINDOW', [])
        ]),
    ('Sequencer', 'SEQUENCE_EDITOR', 'WINDOW', []),
    ('Logic Editor', 'LOGIC_EDITOR', 'WINDOW', []),

    ('File Browser', 'FILE_BROWSER', 'WINDOW', [
        ('File Browser Main', 'FILE_BROWSER', 'WINDOW', []),
        ('File Browser Buttons', 'FILE_BROWSER', 'WINDOW', [])
        ]),

    ('Property Editor', 'PROPERTIES', 'WINDOW', []), # align context menu
    
    ('Script', 'SCRIPTS_WINDOW', 'WINDOW', []),
    ('Text', 'TEXT_EDITOR', 'WINDOW', []),
    ('Console', 'CONSOLE', 'WINDOW', []),

    ('View3D Gesture Circle', 'EMPTY', 'WINDOW', []),
    ('Gesture Border', 'EMPTY', 'WINDOW', []),
    ('Standard Modal Map', 'EMPTY', 'WINDOW', []),
    ('Transform Modal Map', 'EMPTY', 'WINDOW', []),
    ('View3D Fly Modal', 'EMPTY', 'WINDOW', []),
    ('View3D Rotate Modal', 'EMPTY', 'WINDOW', []),
    ('View3D Move Modal', 'EMPTY', 'WINDOW', []),
    ('View3D Zoom Modal', 'EMPTY', 'WINDOW', []),
    ]

def _km_exists_in(km, export_keymaps):
    for km2, kc in export_keymaps:
        if km2.name == km.name:
            return True
    return False

# kc1 takes priority over kc2
def _merge_keymaps(kc1, kc2):
    merged_keymaps = [(km, kc1) for km in kc1.keymaps]
    if kc1 != kc2:
        merged_keymaps.extend([(km, kc2) for km in kc2.keymaps if not _km_exists_in(km, merged_keymaps)])
            
    return merged_keymaps  


class InputKeyMapPanel(bpy.types.Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Input"
    bl_region_type = 'WINDOW'
    bl_show_header = False

    def draw_entry(self, display_keymaps, entry, col, level=0):
        idname, spaceid, regionid, children = entry

        for km, kc in display_keymaps:
            if km.name == idname and km.space_type == spaceid and km.region_type == regionid:
                self.draw_km(display_keymaps, kc, km, children, col, level)

        '''
        km = kc.find_keymap(idname, space_type=spaceid, region_type=regionid)
        if not km:
            kc = defkc
            km = kc.find_keymap(idname, space_type=spaceid, region_type=regionid)

        if km:
            self.draw_km(kc, km, children, col, level)
        '''

    def indented_layout(self, layout, level):
        indentpx = 16
        if level == 0:
            level = 0.0001   # Tweak so that a percentage of 0 won't split by half
        indent = level * indentpx / bpy.context.region.width

        split = layout.split(percentage=indent)
        col = split.column()
        col = split.column()
        return col

    def draw_km(self, display_keymaps, kc, km, children, layout, level):
        km = km.active()

        layout.set_context_pointer("keymap", km)

        col = self.indented_layout(layout, level)

        row = col.row()
        row.prop(km, "children_expanded", text="", no_bg=True)
        row.label(text=km.name)

        row.label()
        row.label()

        if km.modal:
            row.label(text="", icon='LINKED')
        if km.user_defined:
            op = row.operator("wm.keymap_restore", text="Restore")
        else:
            op = row.operator("wm.keymap_edit", text="Edit")

        if km.children_expanded:
            if children:
                # Put the Parent key map's entries in a 'global' sub-category
                # equal in hierarchy to the other children categories
                subcol = self.indented_layout(col, level + 1)
                subrow = subcol.row()
                subrow.prop(km, "items_expanded", text="", no_bg=True)
                subrow.label(text="%s (Global)" % km.name)
            else:
                km.items_expanded = True

            # Key Map items
            if km.items_expanded:
                for kmi in km.items:
                    self.draw_kmi(display_keymaps, kc, km, kmi, col, level + 1)

                # "Add New" at end of keymap item list
                col = self.indented_layout(col, level + 1)
                subcol = col.split(percentage=0.2).column()
                subcol.enabled = km.user_defined
                op = subcol.operator("wm.keyitem_add", text="Add New", icon='ZOOMIN')

            col.separator()

            # Child key maps
            if children:
                subcol = col.column()
                row = subcol.row()

                for entry in children:
                    self.draw_entry(display_keymaps, entry, col, level + 1)

    def draw_kmi(self, display_keymaps, kc, km, kmi, layout, level):
        map_type = kmi.map_type

        col = self.indented_layout(layout, level)

        if km.user_defined:
            col = col.column(align=True)
            box = col.box()
        else:
            box = col.column()

        split = box.split(percentage=0.05)

        # header bar
        row = split.row()
        row.prop(kmi, "expanded", text="", no_bg=True)

        row = split.row()
        row.enabled = km.user_defined
        row.prop(kmi, "active", text="", no_bg=True)

        if km.modal:
            row.prop(kmi, "propvalue", text="")
        else:
            row.label(text=kmi.name)

        row = split.row()
        row.enabled = km.user_defined
        row.prop(kmi, "map_type", text="")
        if map_type == 'KEYBOARD':
            row.prop(kmi, "type", text="", full_event=True)
        elif map_type == 'MOUSE':
            row.prop(kmi, "type", text="", full_event=True)
        elif map_type == 'TWEAK':
            subrow = row.row()
            subrow.prop(kmi, "type", text="")
            subrow.prop(kmi, "value", text="")
        elif map_type == 'TIMER':
            row.prop(kmi, "type", text="")
        else:
            row.label()

        if kmi.id:
            op = row.operator("wm.keyitem_restore", text="", icon='BACK')
            op.item_id = kmi.id
        op = row.operator("wm.keyitem_remove", text="", icon='X')
        op.item_id = kmi.id

        # Expanded, additional event settings
        if kmi.expanded:
            box = col.box()

            box.enabled = km.user_defined

            if map_type not in ('TEXTINPUT', 'TIMER'):
                split = box.split(percentage=0.4)
                sub = split.row()

                if km.modal:
                    sub.prop(kmi, "propvalue", text="")
                else:
                    sub.prop(kmi, "idname", text="")

                sub = split.column()
                subrow = sub.row(align=True)

                if map_type == 'KEYBOARD':
                    subrow.prop(kmi, "type", text="", event=True)
                    subrow.prop(kmi, "value", text="")
                elif map_type == 'MOUSE':
                    subrow.prop(kmi, "type", text="")
                    subrow.prop(kmi, "value", text="")

                subrow = sub.row()
                subrow.scale_x = 0.75
                subrow.prop(kmi, "any")
                subrow.prop(kmi, "shift")
                subrow.prop(kmi, "ctrl")
                subrow.prop(kmi, "alt")
                subrow.prop(kmi, "oskey", text="Cmd")
                subrow.prop(kmi, "key_modifier", text="", event=True)

            def display_properties(properties, title=None):
                box.separator()
                if title:
                    box.label(text=title)
                flow = box.column_flow(columns=2)
                for pname in dir(properties):
                    if not properties.is_property_hidden(pname):
                        value = eval("properties." + pname)
                        if isinstance(value, bpy.types.OperatorProperties):
                            display_properties(value, title=pname)
                        else:
                            flow.prop(properties, pname)

            # Operator properties
            props = kmi.properties
            if props is not None:
                display_properties(props)

            # Modal key maps attached to this operator
            if not km.modal:
                kmm = kc.find_keymap_modal(kmi.idname)
                if kmm:
                    self.draw_km(display_keymaps, kc, kmm, None, layout, level + 1)
                    layout.set_context_pointer("keymap", km)

    def draw_filtered(self, display_keymaps, filter, layout):
        for km, kc in display_keymaps:
            km = km.active()
            layout.set_context_pointer("keymap", km)

            filtered_items = [kmi for kmi in km.items if filter in kmi.name.lower()]

            if len(filtered_items) != 0:
                col = layout.column()

                row = col.row()
                row.label(text=km.name, icon="DOT")

                row.label()
                row.label()

                if km.user_defined:
                    op = row.operator("wm.keymap_restore", text="Restore")
                else:
                    op = row.operator("wm.keymap_edit", text="Edit")

                for kmi in filtered_items:
                    self.draw_kmi(display_keymaps, kc, km, kmi, col, 1)

                # "Add New" at end of keymap item list
                col = self.indented_layout(layout, 1)
                subcol = col.split(percentage=0.2).column()
                subcol.enabled = km.user_defined
                op = subcol.operator("wm.keyitem_add", text="Add New", icon='ZOOMIN')

    def draw_hierarchy(self, display_keymaps, layout):
        for entry in KM_HIERARCHY:
            self.draw_entry(display_keymaps, entry, layout)

    def draw_keymaps(self, context, layout):
        wm = context.manager
        kc = wm.active_keyconfig
        defkc = wm.default_keyconfig

        col = layout.column()
        sub = col.column()

        subsplit = sub.split()
        subcol = subsplit.column()

        row = subcol.row()
        row.prop_object(wm, "active_keyconfig", wm, "keyconfigs", text="Key Config:")
        layout.set_context_pointer("keyconfig", wm.active_keyconfig)
        row.operator("wm.keyconfig_remove", text="", icon='X')

        row.prop(context.space_data, "filter", icon="VIEWZOOM")

        col.separator()

        display_keymaps = _merge_keymaps(kc, defkc)
        if context.space_data.filter != "":
            filter = context.space_data.filter.lower()
            self.draw_filtered(display_keymaps, filter, col)
        else:
            self.draw_hierarchy(display_keymaps, col)


from bpy.props import *


class WM_OT_keyconfig_test(bpy.types.Operator):
    "Test keyconfig for conflicts"
    bl_idname = "wm.keyconfig_test"
    bl_label = "Test Key Configuration for Conflicts"

    def testEntry(self, kc, entry, src=None, parent=None):
        result = False

        def kmistr(kmi):
            if km.modal:
                s = ["kmi = km.items.add_modal(\'%s\', \'%s\', \'%s\'" % (kmi.propvalue, kmi.type, kmi.value)]
            else:
                s = ["kmi = km.items.add(\'%s\', \'%s\', \'%s\'" % (kmi.idname, kmi.type, kmi.value)]

            if kmi.any:
                s.append(", any=True")
            else:
                if kmi.shift:
                    s.append(", shift=True")
                if kmi.ctrl:
                    s.append(", ctrl=True")
                if kmi.alt:
                    s.append(", alt=True")
                if kmi.oskey:
                    s.append(", oskey=True")
            if kmi.key_modifier and kmi.key_modifier != 'NONE':
                s.append(", key_modifier=\'%s\'" % kmi.key_modifier)

            s.append(")\n")

            def export_properties(prefix, properties):
                for pname in dir(properties):
                    if not properties.is_property_hidden(pname):
                        value = eval("properties.%s" % pname)
                        if isinstance(value, bpy.types.OperatorProperties):
                            export_properties(prefix + "." + pname, value)
                        elif properties.is_property_set(pname):
                            value = _string_value(value)
                            if value != "":
                                s.append(prefix + ".%s = %s\n" % (pname, value))

            props = kmi.properties

            if props is not None:
                export_properties("kmi.properties", props)

            return "".join(s).strip()

        idname, spaceid, regionid, children = entry

        km = kc.find_keymap(idname, space_type=spaceid, region_type=regionid)

        if km:
            km = km.active()

            if src:
                for item in km.items:
                    if src.compare(item):
                        print("===========")
                        print(parent.name)
                        print(kmistr(src))
                        print(km.name)
                        print(kmistr(item))
                        result = True

                for child in children:
                    if self.testEntry(kc, child, src, parent):
                        result = True
            else:
                for i in range(len(km.items)):
                    src = km.items[i]

                    for child in children:
                        if self.testEntry(kc, child, src, km):
                            result = True

                    for j in range(len(km.items) - i - 1):
                        item = km.items[j + i + 1]
                        if src.compare(item):
                            print("===========")
                            print(km.name)
                            print(kmistr(src))
                            print(kmistr(item))
                            result = True

                for child in children:
                    if self.testEntry(kc, child):
                        result = True

        return result

    def testConfig(self, kc):
        result = False
        for entry in KM_HIERARCHY:
            if self.testEntry(kc, entry):
                result = True
        return result

    def execute(self, context):
        wm = context.manager
        kc = wm.default_keyconfig

        if self.testConfig(kc):
            print("CONFLICT")

        return {'FINISHED'}


def _string_value(value):
    if isinstance(value, str) or isinstance(value, bool) or isinstance(value, float) or isinstance(value, int):
        result = repr(value)
    elif getattr(value, '__len__', False):
        repr(list(value))
    else:
        print("Export key configuration: can't write ", value)

    return result


class WM_OT_keyconfig_import(bpy.types.Operator):
    "Import key configuration from a python script"
    bl_idname = "wm.keyconfig_import"
    bl_label = "Import Key Configuration..."

    path = StringProperty(name="File Path", description="File path to write file to")
    filename = StringProperty(name="File Name", description="Name of the file")
    directory = StringProperty(name="Directory", description="Directory of the file")
    filter_folder = BoolProperty(name="Filter folders", description="", default=True, options={'HIDDEN'})
    filter_text = BoolProperty(name="Filter text", description="", default=True, options={'HIDDEN'})
    filter_python = BoolProperty(name="Filter python", description="", default=True, options={'HIDDEN'})

    keep_original = BoolProperty(name="Keep original", description="Keep original file after copying to configuration folder", default=True)

    def execute(self, context):
        if not self.properties.path:
            raise Exception("File path not set")

        f = open(self.properties.path, "r")
        if not f:
            raise Exception("Could not open file")

        name_pattern = re.compile("^kc = wm.add_keyconfig\('(.*)'\)$")

        for line in f.readlines():
            match = name_pattern.match(line)

            if match:
                config_name = match.groups()[0]

        f.close()

        path = os.path.split(os.path.split(__file__)[0])[0] # remove ui/space_userpref.py
        path = os.path.join(path, "cfg")

        # create config folder if needed
        if not os.path.exists(path):
            os.mkdir(path)

        path = os.path.join(path, config_name + ".py")

        if self.properties.keep_original:
            shutil.copy(self.properties.path, path)
        else:
            shutil.move(self.properties.path, path)

        exec("import " + config_name)

        wm = bpy.context.manager
        wm.active_keyconfig = wm.keyconfigs[config_name]

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.manager
        wm.add_fileselect(self)
        return {'RUNNING_MODAL'}

# This operator is also used by interaction presets saving - AddPresetBase


class WM_OT_keyconfig_export(bpy.types.Operator):
    "Export key configuration to a python script"
    bl_idname = "wm.keyconfig_export"
    bl_label = "Export Key Configuration..."

    path = StringProperty(name="File Path", description="File path to write file to")
    filename = StringProperty(name="File Name", description="Name of the file")
    directory = StringProperty(name="Directory", description="Directory of the file")
    filter_folder = BoolProperty(name="Filter folders", description="", default=True, options={'HIDDEN'})
    filter_text = BoolProperty(name="Filter text", description="", default=True, options={'HIDDEN'})
    filter_python = BoolProperty(name="Filter python", description="", default=True, options={'HIDDEN'})
    kc_name = StringProperty(name="KeyConfig Name", description="Name to save the key config as")

    def execute(self, context):
        if not self.properties.path:
            raise Exception("File path not set")

        f = open(self.properties.path, "w")
        if not f:
            raise Exception("Could not open file")

        wm = context.manager
        kc = wm.active_keyconfig

        if self.properties.kc_name != '':
            name = self.properties.kc_name
        elif kc.name == 'Blender':
            name = os.path.splitext(os.path.basename(self.properties.path))[0]
        else:
            name = kc.name

        f.write("# Configuration %s\n" % name)

        f.write("import bpy\n\n")
        f.write("wm = bpy.context.manager\n")
        f.write("kc = wm.add_keyconfig('%s')\n\n" % name)

        # Generate a list of keymaps to export:
        #
        # First add all user_defined keymaps (found in inputs.edited_keymaps list),
        # then add all remaining keymaps from the currently active custom keyconfig.
        #
        # This will create a final list of keymaps that can be used as a 'diff' against
        # the default blender keyconfig, recreating the current setup from a fresh blender
        # without needing to export keymaps which haven't been edited.

        class FakeKeyConfig():
            keymaps = []
        edited_kc = FakeKeyConfig()
        edited_kc.keymaps.extend(context.user_preferences.inputs.edited_keymaps)
        # merge edited keymaps with non-default keyconfig, if it exists
        if kc != wm.default_keyconfig:
            export_keymaps = _merge_keymaps(edited_kc, kc)
        else:
            export_keymaps = _merge_keymaps(edited_kc, edited_kc)

        for km, kc_x in export_keymaps:

            km = km.active()

            f.write("# Map %s\n" % km.name)
            f.write("km = kc.add_keymap('%s', space_type='%s', region_type='%s', modal=%s)\n\n" % (km.name, km.space_type, km.region_type, km.modal))
            for kmi in km.items:
                if km.modal:
                    f.write("kmi = km.items.add_modal('%s', '%s', '%s'" % (kmi.propvalue, kmi.type, kmi.value))
                else:
                    f.write("kmi = km.items.add('%s', '%s', '%s'" % (kmi.idname, kmi.type, kmi.value))
                if kmi.any:
                    f.write(", any=True")
                else:
                    if kmi.shift:
                        f.write(", shift=True")
                    if kmi.ctrl:
                        f.write(", ctrl=True")
                    if kmi.alt:
                        f.write(", alt=True")
                    if kmi.oskey:
                        f.write(", oskey=True")
                if kmi.key_modifier and kmi.key_modifier != 'NONE':
                    f.write(", key_modifier='%s'" % kmi.key_modifier)
                f.write(")\n")

                def export_properties(prefix, properties):
                    for pname in dir(properties):
                        if not properties.is_property_hidden(pname):
                            value = eval("properties.%s" % pname)
                            if isinstance(value, bpy.types.OperatorProperties):
                                export_properties(prefix + "." + pname, value)
                            elif properties.is_property_set(pname):
                                value = _string_value(value)
                                if value != "":
                                    f.write(prefix + ".%s = %s\n" % (pname, value))

                props = kmi.properties

                if props is not None:
                    export_properties("kmi.properties", props)

            f.write("\n")

        f.close()

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.manager
        wm.add_fileselect(self)
        return {'RUNNING_MODAL'}


class WM_OT_keymap_edit(bpy.types.Operator):
    "Edit stored key map"
    bl_idname = "wm.keymap_edit"
    bl_label = "Edit Key Map"

    def execute(self, context):
        wm = context.manager
        km = context.keymap
        km.copy_to_user()
        return {'FINISHED'}


class WM_OT_keymap_restore(bpy.types.Operator):
    "Restore key map(s)"
    bl_idname = "wm.keymap_restore"
    bl_label = "Restore Key Map(s)"

    all = BoolProperty(attr="all", name="All Keymaps", description="Restore all keymaps to default")

    def execute(self, context):
        wm = context.manager

        if self.properties.all:
            for km in wm.default_keyconfig.keymaps:
                km.restore_to_default()
        else:
            km = context.keymap
            km.restore_to_default()

        return {'FINISHED'}


class WM_OT_keyitem_restore(bpy.types.Operator):
    "Restore key map item"
    bl_idname = "wm.keyitem_restore"
    bl_label = "Restore Key Map Item"

    item_id = IntProperty(attr="item_id", name="Item Identifier", description="Identifier of the item to remove")

    def execute(self, context):
        wm = context.manager
        km = context.keymap
        kmi = km.item_from_id(self.properties.item_id)

        km.restore_item_to_default(kmi)

        return {'FINISHED'}


class WM_OT_keyitem_add(bpy.types.Operator):
    "Add key map item"
    bl_idname = "wm.keyitem_add"
    bl_label = "Add Key Map Item"

    def execute(self, context):
        wm = context.manager
        km = context.keymap
        kc = wm.default_keyconfig

        if km.modal:
            km.items.add_modal("", 'A', 'PRESS') # kmi
        else:
            km.items.add("none", 'A', 'PRESS') # kmi

        # clear filter and expand keymap so we can see the newly added item
        if context.space_data.filter != '':
            context.space_data.filter = ''
            km.items_expanded = True
            km.children_expanded = True

        return {'FINISHED'}


class WM_OT_keyitem_remove(bpy.types.Operator):
    "Remove key map item"
    bl_idname = "wm.keyitem_remove"
    bl_label = "Remove Key Map Item"

    item_id = IntProperty(attr="item_id", name="Item Identifier", description="Identifier of the item to remove")

    def execute(self, context):
        wm = context.manager
        km = context.keymap
        kmi = km.item_from_id(self.properties.item_id)
        km.remove_item(kmi)
        return {'FINISHED'}


class WM_OT_keyconfig_remove(bpy.types.Operator):
    "Remove key config"
    bl_idname = "wm.keyconfig_remove"
    bl_label = "Remove Key Config"

    def poll(self, context):
        wm = context.manager
        return wm.active_keyconfig.user_defined

    def execute(self, context):
        wm = context.manager

        keyconfig = wm.active_keyconfig

        module = __import__(keyconfig.name)

        os.remove(module.__file__)

        compiled_path = module.__file__ + "c" # for .pyc

        if os.path.exists(compiled_path):
            os.remove(compiled_path)

        wm.remove_keyconfig(keyconfig)
        return {'FINISHED'}


classes = [
    WM_OT_keyconfig_export,
    WM_OT_keyconfig_import,
    WM_OT_keyconfig_test,
    WM_OT_keyconfig_remove,
    WM_OT_keymap_edit,
    WM_OT_keymap_restore,
    WM_OT_keyitem_add,
    WM_OT_keyitem_remove,
    WM_OT_keyitem_restore]


def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)


def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)

if __name__ == "__main__":
    register()
