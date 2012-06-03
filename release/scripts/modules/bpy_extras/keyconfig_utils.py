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

KM_HIERARCHY = [
    ('Window', 'EMPTY', 'WINDOW', []),  # file save, window change, exit
    ('Screen', 'EMPTY', 'WINDOW', [     # full screen, undo, screenshot
        ('Screen Editing', 'EMPTY', 'WINDOW', []),    # re-sizing, action corners
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
        ('UV Sculpt', 'EMPTY', 'WINDOW', []),
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

    ('Text', 'TEXT_EDITOR', 'WINDOW', []),
    ('Console', 'CONSOLE', 'WINDOW', []),
    ('Clip', 'CLIP_EDITOR', 'WINDOW', [
        ('Clip Editor', 'CLIP_EDITOR', 'WINDOW', []),
        ('Clip Graph Editor', 'CLIP_EDITOR', 'WINDOW', []),
        ]),

    ('View3D Gesture Circle', 'EMPTY', 'WINDOW', []),
    ('Gesture Border', 'EMPTY', 'WINDOW', []),
    ('Standard Modal Map', 'EMPTY', 'WINDOW', []),
    ('Transform Modal Map', 'EMPTY', 'WINDOW', []),
    ('View3D Fly Modal', 'EMPTY', 'WINDOW', []),
    ('View3D Rotate Modal', 'EMPTY', 'WINDOW', []),
    ('View3D Move Modal', 'EMPTY', 'WINDOW', []),
    ('View3D Zoom Modal', 'EMPTY', 'WINDOW', []),
    ]


# -----------------------------------------------------------------------------
# Utility functions

def km_exists_in(km, export_keymaps):
    for km2, kc in export_keymaps:
        if km2.name == km.name:
            return True
    return False


def keyconfig_merge(kc1, kc2):
    """ note: kc1 takes priority over kc2
    """
    merged_keymaps = [(km, kc1) for km in kc1.keymaps]
    if kc1 != kc2:
        merged_keymaps.extend((km, kc2) for km in kc2.keymaps if not km_exists_in(km, merged_keymaps))

    return merged_keymaps


def _export_properties(prefix, properties, lines=None):
    from bpy.types import OperatorProperties

    if lines is None:
        lines = []

    def string_value(value):
        if isinstance(value, str) or isinstance(value, bool) or isinstance(value, float) or isinstance(value, int):
            result = repr(value)
        elif getattr(value, '__len__', False):
            return repr(list(value))
        else:
            print("Export key configuration: can't write ", value)

        return result

    for pname in properties.bl_rna.properties.keys():
        if pname != "rna_type":
            value = getattr(properties, pname)
            if isinstance(value, OperatorProperties):
                _export_properties(prefix + "." + pname, value, lines)
            elif properties.is_property_set(pname):
                value = string_value(value)
                if value != "":
                    lines.append("%s.%s = %s\n" % (prefix, pname, value))
    return lines


def keyconfig_export(wm, kc, filepath):

    f = open(filepath, "w")

    f.write("import bpy\n")
    f.write("import os\n\n")
    f.write("wm = bpy.context.window_manager\n")
    f.write("kc = wm.keyconfigs.new(os.path.splitext(os.path.basename(__file__))[0])\n\n")  # keymap must be created by caller

    # Generate a list of keymaps to export:
    #
    # First add all user_modified keymaps (found in keyconfigs.user.keymaps list),
    # then add all remaining keymaps from the currently active custom keyconfig.
    #
    # This will create a final list of keymaps that can be used as a 'diff' against
    # the default blender keyconfig, recreating the current setup from a fresh blender
    # without needing to export keymaps which haven't been edited.

    class FakeKeyConfig():
        keymaps = []
    edited_kc = FakeKeyConfig()
    for km in wm.keyconfigs.user.keymaps:
        if km.is_user_modified:
            edited_kc.keymaps.append(km)
    # merge edited keymaps with non-default keyconfig, if it exists
    if kc != wm.keyconfigs.default:
        export_keymaps = keyconfig_merge(edited_kc, kc)
    else:
        export_keymaps = keyconfig_merge(edited_kc, edited_kc)

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
                f.write("".join(_export_properties("kmi.properties", props)))

        f.write("\n")

    f.close()


def keyconfig_test(kc):

    def testEntry(kc, entry, src=None, parent=None):
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
                _export_properties("kmi.properties", props, s)

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
                    if testEntry(kc, child, src, parent):
                        result = True
            else:
                for i in range(len(km.keymap_items)):
                    src = km.keymap_items[i]

                    for child in children:
                        if testEntry(kc, child, src, km):
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
                    if testEntry(kc, child):
                        result = True

        return result

    # -------------------------------------------------------------------------
    # Function body

    result = False
    for entry in KM_HIERARCHY:
        if testEntry(kc, entry):
            result = True
    return result
