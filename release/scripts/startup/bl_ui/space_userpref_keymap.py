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


KM_HIERARCHY = [
    ('Window', 'EMPTY', 'WINDOW', []),  # file save, window change, exit
    ('Screen', 'EMPTY', 'WINDOW', [     # full screen, undo, screenshot
        ('Screen Editing', 'EMPTY', 'WINDOW', []),    # resizing, action corners
        ]),

    ('View2D', 'EMPTY', 'WINDOW', []),    # view 2d navigation (per region)
    ('View2D Buttons List', 'EMPTY', 'WINDOW', []),  # view 2d with buttons navigation
    ('Header', 'EMPTY', 'WINDOW', []),    # header stuff (per region)
    ('Grease Pencil', 'EMPTY', 'WINDOW', []),  # grease pencil stuff (per region)

    ('3D View', 'VIEW_3D', 'WINDOW', [  # view 3d navigation and generic stuff (select, transform)
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
        ('Image Paint', 'EMPTY', 'WINDOW', []),  # image and view3d
        ('Sculpt', 'EMPTY', 'WINDOW', []),

        ('Armature Sketch', 'EMPTY', 'WINDOW', []),
        ('Particle', 'EMPTY', 'WINDOW', []),

        ('Object Non-modal', 'EMPTY', 'WINDOW', []),  # mode change

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
        ('UV Editor', 'EMPTY', 'WINDOW', []),  # image (reverse order, UVEdit before Image
        ('Image Paint', 'EMPTY', 'WINDOW', []),  # image and view3d
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

    ('Property Editor', 'PROPERTIES', 'WINDOW', []),  # align context menu

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


def _merge_keymaps(kc1, kc2):
    """ note: kc1 takes priority over kc2
    """
    merged_keymaps = [(km, kc1) for km in kc1.keymaps]
    if kc1 != kc2:
        merged_keymaps.extend((km, kc2) for km in kc2.keymaps if not _km_exists_in(km, merged_keymaps))

    return merged_keymaps


class USERPREF_MT_keyconfigs(bpy.types.Menu):
    bl_label = "KeyPresets"
    preset_subdir = "keyconfig"
    preset_operator = "wm.keyconfig_activate"

    def draw(self, context):
        props = self.layout.operator("wm.context_set_value", text="Blender (default)")
        props.data_path = "window_manager.keyconfigs.active"
        props.value = "context.window_manager.keyconfigs.default"

        # now draw the presets
        bpy.types.Menu.draw_preset(self, context)


class InputKeyMapPanel:
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Input"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    def draw_entry(self, display_keymaps, entry, col, level=0):
        idname, spaceid, regionid, children = entry

        for km, kc in display_keymaps:
            if km.name == idname and km.space_type == spaceid and km.region_type == regionid:
                self.draw_km(display_keymaps, kc, km, children, col, level)

        '''
        km = kc.keymaps.find(idname, space_type=spaceid, region_type=regionid)
        if not km:
            kc = defkc
            km = kc.keymaps.find(idname, space_type=spaceid, region_type=regionid)

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

        layout.context_pointer_set("keymap", km)

        col = self.indented_layout(layout, level)

        row = col.row()
        row.prop(km, "show_expanded_children", text="", emboss=False)
        row.label(text=km.name)

        row.label()
        row.label()

        if km.is_modal:
            row.label(text="", icon='LINKED')
        if km.is_user_defined:
            row.operator("wm.keymap_restore", text="Restore")
        else:
            row.operator("wm.keymap_edit", text="Edit")

        if km.show_expanded_children:
            if children:
                # Put the Parent key map's entries in a 'global' sub-category
                # equal in hierarchy to the other children categories
                subcol = self.indented_layout(col, level + 1)
                subrow = subcol.row()
                subrow.prop(km, "show_expanded_items", text="", emboss=False)
                subrow.label(text="%s (Global)" % km.name)
            else:
                km.show_expanded_items = True

            # Key Map items
            if km.show_expanded_items:
                for kmi in km.keymap_items:
                    self.draw_kmi(display_keymaps, kc, km, kmi, col, level + 1)

                # "Add New" at end of keymap item list
                col = self.indented_layout(col, level + 1)
                subcol = col.split(percentage=0.2).column()
                subcol.enabled = km.is_user_defined
                subcol.operator("wm.keyitem_add", text="Add New", icon='ZOOMIN')

            col.separator()

            # Child key maps
            if children:
                subcol = col.column()
                row = subcol.row()

                for entry in children:
                    self.draw_entry(display_keymaps, entry, col, level + 1)

    @staticmethod
    def draw_kmi_properties(box, properties, title=None):
        box.separator()
        if title:
            box.label(text=title)
        flow = box.column_flow(columns=2)
        for pname, value in properties.bl_rna.properties.items():
            if pname != "rna_type" and not properties.is_property_hidden(pname):
                if isinstance(value, bpy.types.OperatorProperties):
                    InputKeyMapPanel.draw_kmi_properties(box, value, title=pname)
                else:
                    flow.prop(properties, pname)

    def draw_kmi(self, display_keymaps, kc, km, kmi, layout, level):
        map_type = kmi.map_type

        col = self.indented_layout(layout, level)

        if km.is_user_defined:
            col = col.column(align=True)
            box = col.box()
        else:
            box = col.column()

        split = box.split(percentage=0.05)

        # header bar
        row = split.row()
        row.prop(kmi, "show_expanded", text="", emboss=False)

        row = split.row()
        row.enabled = km.is_user_defined
        row.prop(kmi, "active", text="", emboss=False)

        if km.is_modal:
            row.prop(kmi, "propvalue", text="")
        else:
            row.label(text=kmi.name)

        row = split.row()
        row.enabled = km.is_user_defined
        row.prop(kmi, "map_type", text="")
        if map_type == 'KEYBOARD':
            row.prop(kmi, "type", text="", full_event=True)
        elif map_type == 'MOUSE':
            row.prop(kmi, "type", text="", full_event=True)
        elif map_type == 'NDOF':
            row.prop(kmi, "type", text="", full_event=True)
        elif map_type == 'TWEAK':
            subrow = row.row()
            subrow.prop(kmi, "type", text="")
            subrow.prop(kmi, "value", text="")
        elif map_type == 'TIMER':
            row.prop(kmi, "type", text="")
        else:
            row.label()

        if not kmi.is_user_defined:
            op = row.operator("wm.keyitem_restore", text="", icon='BACK')
            op.item_id = kmi.id
        op = row.operator("wm.keyitem_remove", text="", icon='X')
        op.item_id = kmi.id

        # Expanded, additional event settings
        if kmi.show_expanded:
            box = col.box()

            box.enabled = km.is_user_defined

            if map_type not in {'TEXTINPUT', 'TIMER'}:
                split = box.split(percentage=0.4)
                sub = split.row()

                if km.is_modal:
                    sub.prop(kmi, "propvalue", text="")
                else:
                    # One day...
                    # sub.prop_search(kmi, "idname", bpy.context.window_manager, "operators_all", text="")
                    sub.prop(kmi, "idname", text="")

                sub = split.column()
                subrow = sub.row(align=True)

                if map_type in {'KEYBOARD', 'NDOF'}:
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

            # Operator properties
            props = kmi.properties
            if props is not None:
                InputKeyMapPanel.draw_kmi_properties(box, props)

            # Modal key maps attached to this operator
            if not km.is_modal:
                kmm = kc.keymaps.find_modal(kmi.idname)
                if kmm:
                    self.draw_km(display_keymaps, kc, kmm, None, layout, level + 1)
                    layout.context_pointer_set("keymap", km)

    def draw_filtered(self, display_keymaps, filter_text, layout):
        for km, kc in display_keymaps:
            km = km.active()
            layout.context_pointer_set("keymap", km)

            filtered_items = [kmi for kmi in km.keymap_items if filter_text in kmi.name.lower()]

            if len(filtered_items) != 0:
                col = layout.column()

                row = col.row()
                row.label(text=km.name, icon="DOT")

                row.label()
                row.label()

                if km.is_user_defined:
                    row.operator("wm.keymap_restore", text="Restore")
                else:
                    row.operator("wm.keymap_edit", text="Edit")

                for kmi in filtered_items:
                    self.draw_kmi(display_keymaps, kc, km, kmi, col, 1)

                # "Add New" at end of keymap item list
                col = self.indented_layout(layout, 1)
                subcol = col.split(percentage=0.2).column()
                subcol.enabled = km.is_user_defined
                subcol.operator("wm.keyitem_add", text="Add New", icon='ZOOMIN')

    def draw_hierarchy(self, display_keymaps, layout):
        for entry in KM_HIERARCHY:
            self.draw_entry(display_keymaps, entry, layout)

    def draw_keymaps(self, context, layout):
        wm = context.window_manager
        kc = wm.keyconfigs.active
        defkc = wm.keyconfigs.default

        col = layout.column()
        sub = col.column()

        subsplit = sub.split()
        subcol = subsplit.column()

        row = subcol.row(align=True)

        #row.prop_search(wm.keyconfigs, "active", wm, "keyconfigs", text="Key Config:")
        text = bpy.path.display_name(context.window_manager.keyconfigs.active.name)
        if not text:
            text = "Blender (default)"
        row.menu("USERPREF_MT_keyconfigs", text=text)
        row.operator("wm.keyconfig_preset_add", text="", icon="ZOOMIN")
        row.operator("wm.keyconfig_preset_add", text="", icon="ZOOMOUT").remove_active = True

#        layout.context_pointer_set("keyconfig", wm.keyconfigs.active)
#        row.operator("wm.keyconfig_remove", text="", icon='X')

        row.prop(context.space_data, "filter_text", icon="VIEWZOOM")

        col.separator()

        display_keymaps = _merge_keymaps(kc, defkc)
        if context.space_data.filter_text != "":
            filter_text = context.space_data.filter_text.lower()
            self.draw_filtered(display_keymaps, filter_text, col)
        else:
            self.draw_hierarchy(display_keymaps, col)


from bpy.props import StringProperty, BoolProperty, IntProperty


def export_properties(prefix, properties, lines=None):
    if lines is None:
        lines = []

    for pname in properties.bl_rna.properties.keys():
        if pname != "rna_type" and not properties.is_property_hidden(pname):
            value = getattr(properties, pname)
            if isinstance(value, bpy.types.OperatorProperties):
                export_properties(prefix + "." + pname, value, lines)
            elif properties.is_property_set(pname):
                value = _string_value(value)
                if value != "":
                    lines.append("%s.%s = %s\n" % (prefix, pname, value))
    return lines


class WM_OT_keyconfig_test(bpy.types.Operator):
    "Test keyconfig for conflicts"
    bl_idname = "wm.keyconfig_test"
    bl_label = "Test Key Configuration for Conflicts"

    def testEntry(self, kc, entry, src=None, parent=None):
        result = False

        def kmistr(kmi):
            if km.is_modal:
                s = ["kmi = km.keymap_items.new_modal(\'%s\', \'%s\', \'%s\'" % (kmi.propvalue, kmi.type, kmi.value)]
            else:
                s = ["kmi = km.keymap_items.new(\'%s\', \'%s\', \'%s\'" % (kmi.idname, kmi.type, kmi.value)]

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

            props = kmi.properties

            if props is not None:
                export_properties("kmi.properties", props, s)

            return "".join(s).strip()

        idname, spaceid, regionid, children = entry

        km = kc.keymaps.find(idname, space_type=spaceid, region_type=regionid)

        if km:
            km = km.active()

            if src:
                for item in km.keymap_items:
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
                for i in range(len(km.keymap_items)):
                    src = km.keymap_items[i]

                    for child in children:
                        if self.testEntry(kc, child, src, km):
                            result = True

                    for j in range(len(km.keymap_items) - i - 1):
                        item = km.keymap_items[j + i + 1]
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
        wm = context.window_manager
        kc = wm.keyconfigs.default

        if self.testConfig(kc):
            print("CONFLICT")

        return {'FINISHED'}


def _string_value(value):
    if isinstance(value, str) or isinstance(value, bool) or isinstance(value, float) or isinstance(value, int):
        result = repr(value)
    elif getattr(value, '__len__', False):
        return repr(list(value))
    else:
        print("Export key configuration: can't write ", value)

    return result


class WM_OT_keyconfig_import(bpy.types.Operator):
    "Import key configuration from a python script"
    bl_idname = "wm.keyconfig_import"
    bl_label = "Import Key Configuration..."

    filepath = StringProperty(name="File Path", description="Filepath to write file to", default="keymap.py")
    filter_folder = BoolProperty(name="Filter folders", description="", default=True, options={'HIDDEN'})
    filter_text = BoolProperty(name="Filter text", description="", default=True, options={'HIDDEN'})
    filter_python = BoolProperty(name="Filter python", description="", default=True, options={'HIDDEN'})

    keep_original = BoolProperty(name="Keep original", description="Keep original file after copying to configuration folder", default=True)

    def execute(self, context):
        from os.path import basename
        import shutil
        if not self.filepath:
            raise Exception("Filepath not set")

        f = open(self.filepath, "r")
        if not f:
            raise Exception("Could not open file")

        config_name = basename(self.filepath)

        path = bpy.utils.user_resource('SCRIPTS', os.path.join("presets", "keyconfig"), create=True)
        path = os.path.join(path, config_name)

        if self.keep_original:
            shutil.copy(self.filepath, path)
        else:
            shutil.move(self.filepath, path)

        # sneaky way to check we're actually running the code.
        bpy.utils.keyconfig_set(path)

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}

# This operator is also used by interaction presets saving - AddPresetBase


class WM_OT_keyconfig_export(bpy.types.Operator):
    "Export key configuration to a python script"
    bl_idname = "wm.keyconfig_export"
    bl_label = "Export Key Configuration..."

    filepath = StringProperty(name="File Path", description="Filepath to write file to", default="keymap.py")
    filter_folder = BoolProperty(name="Filter folders", description="", default=True, options={'HIDDEN'})
    filter_text = BoolProperty(name="Filter text", description="", default=True, options={'HIDDEN'})
    filter_python = BoolProperty(name="Filter python", description="", default=True, options={'HIDDEN'})

    def execute(self, context):
        if not self.filepath:
            raise Exception("Filepath not set")

        f = open(self.filepath, "w")
        if not f:
            raise Exception("Could not open file")

        wm = context.window_manager
        kc = wm.keyconfigs.active

        f.write("import bpy\n")
        f.write("import os\n\n")
        f.write("wm = bpy.context.window_manager\n")
        f.write("kc = wm.keyconfigs.new(os.path.splitext(os.path.basename(__file__))[0])\n\n")  # keymap must be created by caller

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
        if kc != wm.keyconfigs.default:
            export_keymaps = _merge_keymaps(edited_kc, kc)
        else:
            export_keymaps = _merge_keymaps(edited_kc, edited_kc)

        for km, kc_x in export_keymaps:

            km = km.active()

            f.write("# Map %s\n" % km.name)
            f.write("km = kc.keymaps.new('%s', space_type='%s', region_type='%s', modal=%s)\n\n" % (km.name, km.space_type, km.region_type, km.is_modal))
            for kmi in km.keymap_items:
                if km.is_modal:
                    f.write("kmi = km.keymap_items.new_modal('%s', '%s', '%s'" % (kmi.propvalue, kmi.type, kmi.value))
                else:
                    f.write("kmi = km.keymap_items.new('%s', '%s', '%s'" % (kmi.idname, kmi.type, kmi.value))
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

                props = kmi.properties

                if props is not None:
                    f.write("".join(export_properties("kmi.properties", props)))

            f.write("\n")

        f.close()

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class WM_OT_keymap_edit(bpy.types.Operator):
    "Edit stored key map"
    bl_idname = "wm.keymap_edit"
    bl_label = "Edit Key Map"

    def execute(self, context):
        km = context.keymap
        km.copy_to_user()
        return {'FINISHED'}


class WM_OT_keymap_restore(bpy.types.Operator):
    "Restore key map(s)"
    bl_idname = "wm.keymap_restore"
    bl_label = "Restore Key Map(s)"

    all = BoolProperty(name="All Keymaps", description="Restore all keymaps to default")

    def execute(self, context):
        wm = context.window_manager

        if self.all:
            for km in wm.keyconfigs.default.keymaps:
                km.restore_to_default()
        else:
            km = context.keymap
            km.restore_to_default()

        return {'FINISHED'}


class WM_OT_keyitem_restore(bpy.types.Operator):
    "Restore key map item"
    bl_idname = "wm.keyitem_restore"
    bl_label = "Restore Key Map Item"

    item_id = IntProperty(name="Item Identifier", description="Identifier of the item to remove")

    @classmethod
    def poll(cls, context):
        keymap = getattr(context, "keymap", None)
        return keymap and keymap.is_user_defined

    def execute(self, context):
        km = context.keymap
        kmi = km.keymap_items.from_id(self.item_id)

        if not kmi.is_user_defined:
            km.restore_item_to_default(kmi)

        return {'FINISHED'}


class WM_OT_keyitem_add(bpy.types.Operator):
    "Add key map item"
    bl_idname = "wm.keyitem_add"
    bl_label = "Add Key Map Item"

    def execute(self, context):
        km = context.keymap

        if km.is_modal:
            km.keymap_items.new_modal("", 'A', 'PRESS')  # kmi
        else:
            km.keymap_items.new("none", 'A', 'PRESS')  # kmi

        # clear filter and expand keymap so we can see the newly added item
        if context.space_data.filter_text != "":
            context.space_data.filter_text = ""
            km.show_expanded_items = True
            km.show_expanded_children = True

        return {'FINISHED'}


class WM_OT_keyitem_remove(bpy.types.Operator):
    "Remove key map item"
    bl_idname = "wm.keyitem_remove"
    bl_label = "Remove Key Map Item"

    item_id = IntProperty(name="Item Identifier", description="Identifier of the item to remove")

    @classmethod
    def poll(cls, context):
        return hasattr(context, "keymap") and context.keymap.is_user_defined

    def execute(self, context):
        km = context.keymap
        kmi = km.keymap_items.from_id(self.item_id)
        km.keymap_items.remove(kmi)
        return {'FINISHED'}


class WM_OT_keyconfig_remove(bpy.types.Operator):
    "Remove key config"
    bl_idname = "wm.keyconfig_remove"
    bl_label = "Remove Key Config"

    @classmethod
    def poll(cls, context):
        wm = context.window_manager
        keyconf = wm.keyconfigs.active
        return keyconf and keyconf.is_user_defined

    def execute(self, context):
        wm = context.window_manager
        keyconfig = wm.keyconfigs.active
        wm.keyconfigs.remove(keyconfig)
        return {'FINISHED'}

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
