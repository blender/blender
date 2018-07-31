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

# bpy.type.KeyMap: (km.name, km.space_type, km.region_type, [...])

#    ('Script', 'EMPTY', 'WINDOW', []),


KM_HIERARCHY = [
    ('Window', 'EMPTY', 'WINDOW', []),  # file save, window change, exit
    ('Screen', 'EMPTY', 'WINDOW', [     # full screen, undo, screenshot
        ('Screen Editing', 'EMPTY', 'WINDOW', []),    # re-sizing, action corners
        ('Header', 'EMPTY', 'WINDOW', []),            # header stuff (per region)
    ]),

    ('View2D', 'EMPTY', 'WINDOW', []),    # view 2d navigation (per region)
    ('View2D Buttons List', 'EMPTY', 'WINDOW', []),  # view 2d with buttons navigation

    ('User Interface', 'EMPTY', 'WINDOW', []),

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
        ('Weight Paint Vertex Selection', 'EMPTY', 'WINDOW', []),
        ('Face Mask', 'EMPTY', 'WINDOW', []),
        ('Image Paint', 'EMPTY', 'WINDOW', []),  # image and view3d
        ('Sculpt', 'EMPTY', 'WINDOW', []),

        ('Particle', 'EMPTY', 'WINDOW', []),

        ('Knife Tool Modal Map', 'EMPTY', 'WINDOW', []),
        ('Paint Stroke Modal', 'EMPTY', 'WINDOW', []),
        ('Paint Curve', 'EMPTY', 'WINDOW', []),

        ('Object Non-modal', 'EMPTY', 'WINDOW', []),  # mode change

        ('View3D Walk Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Fly Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Rotate Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Move Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Zoom Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Dolly Modal', 'EMPTY', 'WINDOW', []),

        ('3D View Generic', 'VIEW_3D', 'WINDOW', []),    # toolbar and properties
    ]),

    ('Graph Editor', 'GRAPH_EDITOR', 'WINDOW', [
        ('Graph Editor Generic', 'GRAPH_EDITOR', 'WINDOW', []),
    ]),
    ('Dopesheet', 'DOPESHEET_EDITOR', 'WINDOW', [
        ('Dopesheet Generic', 'DOPESHEET_EDITOR', 'WINDOW', []),
    ]),
    ('NLA Editor', 'NLA_EDITOR', 'WINDOW', [
        ('NLA Channels', 'NLA_EDITOR', 'WINDOW', []),
        ('NLA Generic', 'NLA_EDITOR', 'WINDOW', []),
    ]),
    ('Timeline', 'TIMELINE', 'WINDOW', []),

    ('Image', 'IMAGE_EDITOR', 'WINDOW', [
        ('UV Editor', 'EMPTY', 'WINDOW', []),  # image (reverse order, UVEdit before Image)
        ('Image Paint', 'EMPTY', 'WINDOW', []),  # image and view3d
        ('UV Sculpt', 'EMPTY', 'WINDOW', []),
        ('Image Generic', 'IMAGE_EDITOR', 'WINDOW', []),
    ]),

    ('Outliner', 'OUTLINER', 'WINDOW', []),

    ('Node Editor', 'NODE_EDITOR', 'WINDOW', [
        ('Node Generic', 'NODE_EDITOR', 'WINDOW', []),
    ]),
    ('Sequencer', 'SEQUENCE_EDITOR', 'WINDOW', [
        ('SequencerCommon', 'SEQUENCE_EDITOR', 'WINDOW', []),
        ('SequencerPreview', 'SEQUENCE_EDITOR', 'WINDOW', []),
    ]),

    ('File Browser', 'FILE_BROWSER', 'WINDOW', [
        ('File Browser Main', 'FILE_BROWSER', 'WINDOW', []),
        ('File Browser Buttons', 'FILE_BROWSER', 'WINDOW', []),
    ]),

    ('Info', 'INFO', 'WINDOW', []),

    ('Property Editor', 'PROPERTIES', 'WINDOW', []),  # align context menu

    ('Text', 'TEXT_EDITOR', 'WINDOW', [
        ('Text Generic', 'TEXT_EDITOR', 'WINDOW', []),
    ]),
    ('Console', 'CONSOLE', 'WINDOW', []),
    ('Clip', 'CLIP_EDITOR', 'WINDOW', [
        ('Clip Editor', 'CLIP_EDITOR', 'WINDOW', []),
        ('Clip Graph Editor', 'CLIP_EDITOR', 'WINDOW', []),
        ('Clip Dopesheet Editor', 'CLIP_EDITOR', 'WINDOW', []),
    ]),

    ('Grease Pencil', 'EMPTY', 'WINDOW', [  # grease pencil stuff (per region)
        ('Grease Pencil Stroke Edit Mode', 'EMPTY', 'WINDOW', []),
        ('Grease Pencil Stroke Paint (Draw brush)', 'EMPTY', 'WINDOW', []),
        ('Grease Pencil Stroke Paint (Fill)', 'EMPTY', 'WINDOW', []),
        ('Grease Pencil Stroke Paint (Erase)', 'EMPTY', 'WINDOW', []),
        ('Grease Pencil Stroke Paint Mode', 'EMPTY', 'WINDOW', []),
        ('Grease Pencil Stroke Sculpt Mode', 'EMPTY', 'WINDOW', []),
        ('Grease Pencil Stroke Weight Mode', 'EMPTY', 'WINDOW', []),
    ]),
    ('Mask Editing', 'EMPTY', 'WINDOW', []),
    ('Frames', 'EMPTY', 'WINDOW', []),    # frame navigation (per region)
    ('Markers', 'EMPTY', 'WINDOW', []),    # markers (per region)
    ('Animation', 'EMPTY', 'WINDOW', []),    # frame change on click, preview range (per region)
    ('Animation Channels', 'EMPTY', 'WINDOW', []),

    ('View3D Gesture Circle', 'EMPTY', 'WINDOW', []),
    ('Gesture Straight Line', 'EMPTY', 'WINDOW', []),
    ('Gesture Zoom Border', 'EMPTY', 'WINDOW', []),
    ('Gesture Border', 'EMPTY', 'WINDOW', []),

    ('Standard Modal Map', 'EMPTY', 'WINDOW', []),
    ('Transform Modal Map', 'EMPTY', 'WINDOW', []),
    ('Eyedropper Modal Map', 'EMPTY', 'WINDOW', []),
    ('Eyedropper ColorBand PointSampling Map', 'EMPTY', 'WINDOW', []),
]


# -----------------------------------------------------------------------------
# Add-on helpers to properly (un)register their own keymaps.

# Example of keymaps_description:
keymaps_description_doc = """
keymaps_description is a tuple (((keymap_description), (tuple of keymap_item_descriptions))).
keymap_description is a tuple (name, space_type, region_type, is_modal).
keymap_item_description is a tuple ({kw_args_for_keymap_new}, (tuple of properties)).
kw_args_for_keymap_new is a mapping which keywords match parameters of keymap.new() function.
tuple of properties is a tuple of pairs (prop_name, prop_value) (properties being those of called operator).

Example:

KEYMAPS = (
    # First, keymap identifiers (last bool is True for modal km).
    (('Sequencer', 'SEQUENCE_EDITOR', 'WINDOW', False), (
    # Then a tuple of keymap items, defined by a dict of kwargs for the km new func, and a tuple of tuples (name, val)
    # for ops properties, if needing non-default values.
        ({"idname": export_strips.SEQExportStrip.bl_idname, "type": 'P', "value": 'PRESS', "shift": True, "ctrl": True},
         ()),
    )),
)
"""


def addon_keymap_register(wm, keymaps_description):
    """
    Register a set of keymaps for addons.

    """ + keymaps_description_doc
    kconf = wm.keyconfigs.addon
    if not kconf:
        return  # happens in background mode...
    for km_info, km_items in keymaps_description:
        km_name, km_sptype, km_regtype, km_ismodal = km_info
        kmap = [k for k in kconf.keymaps
                if k.name == km_name and k.region_type == km_regtype and
                k.space_type == km_sptype and k.is_modal == km_ismodal]
        if kmap:
            kmap = kmap[0]
        else:
            kmap = kconf.keymaps.new(km_name, region_type=km_regtype, space_type=km_sptype, modal=km_ismodal)
        for kmi_kwargs, props in km_items:
            kmi = kmap.keymap_items.new(**kmi_kwargs)
            kmi.active = True
            for prop, val in props:
                setattr(kmi.properties, prop, val)


def addon_keymap_unregister(wm, keymaps_description):
    """
    Unregister a set of keymaps for addons.

    """ + keymaps_description_doc
    # NOTE: We must also clean up user keyconfig, else, if user has customized one of add-on's shortcut, this
    #       customization remains in memory, and comes back when re-enabling the addon, causing a segfault... :/
    kconfs = wm.keyconfigs
    for kconf in (kconfs.user, kconfs.addon):
        for km_info, km_items in keymaps_description:
            km_name, km_sptype, km_regtype, km_ismodal = km_info
            kmaps = (k for k in kconf.keymaps
                     if k.name == km_name and k.region_type == km_regtype and
                     k.space_type == km_sptype and k.is_modal == km_ismodal)
            for kmap in kmaps:
                for kmi_kwargs, props in km_items:
                    idname = kmi_kwargs["idname"]
                    for kmi in kmap.keymap_items:
                        if kmi.idname == idname:
                            kmap.keymap_items.remove(kmi)
            # NOTE: We won't remove addons keymaps themselves, other addons might also use them!


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


def _export_properties(prefix, properties, kmi_id, lines=None):
    from bpy.types import OperatorProperties

    if lines is None:
        lines = []

    def string_value(value):
        if isinstance(value, (str, bool, float, int)):
            return repr(value)
        elif hasattr(value, "__len__"):
            return repr(list(value))

        print("Export key configuration: can't write ", value)
        return ""

    for pname in properties.bl_rna.properties.keys():
        if pname != "rna_type":
            value = getattr(properties, pname)
            if isinstance(value, OperatorProperties):
                _export_properties(prefix + "." + pname, value, kmi_id, lines)
            elif properties.is_property_set(pname):
                value = string_value(value)
                if value != "":
                    lines.append("kmi_props_setattr(%s, '%s', %s)\n" % (prefix, pname, value))
    return lines


def _kmistr(kmi, is_modal):
    if is_modal:
        kmi_id = kmi.propvalue
        kmi_newfunc = 'new_modal'
    else:
        kmi_id = kmi.idname
        kmi_newfunc = 'new'
    s = ["kmi = km.keymap_items.%s(\'%s\', \'%s\', \'%s\'" % (kmi_newfunc, kmi_id, kmi.type, kmi.value)]

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
        _export_properties("kmi.properties", props, kmi_id, s)

    if not kmi.active:
        s.append("kmi.active = False\n")

    return "".join(s)


def keyconfig_export(
        wm, kc, filepath, *,
        all_keymaps=False,
):
    f = open(filepath, "w")

    f.write("import bpy\n")
    f.write("import os\n\n")
    f.write("def kmi_props_setattr(kmi_props, attr, value):\n"
            "    try:\n"
            "        setattr(kmi_props, attr, value)\n"
            "    except AttributeError:\n"
            "        print(\"Warning: property '%s' not found in keymap item '%s'\" %\n"
            "              (attr, kmi_props.__class__.__name__))\n"
            "    except Exception as e:\n"
            "        print(\"Warning: %r\" % e)\n\n")
    f.write("wm = bpy.context.window_manager\n")
    # keymap must be created by caller
    f.write("kc = wm.keyconfigs.new(os.path.splitext(os.path.basename(__file__))[0])\n\n")

    # Generate a list of keymaps to export:
    #
    # First add all user_modified keymaps (found in keyconfigs.user.keymaps list),
    # then add all remaining keymaps from the currently active custom keyconfig.
    #
    # This will create a final list of keymaps that can be used as a "diff" against
    # the default blender keyconfig, recreating the current setup from a fresh blender
    # without needing to export keymaps which haven't been edited.

    class FakeKeyConfig:
        keymaps = []
    edited_kc = FakeKeyConfig()
    for km in wm.keyconfigs.user.keymaps:
        if all_keymaps or km.is_user_modified:
            edited_kc.keymaps.append(km)
    # merge edited keymaps with non-default keyconfig, if it exists
    if kc != wm.keyconfigs.default:
        export_keymaps = keyconfig_merge(edited_kc, kc)
    else:
        export_keymaps = keyconfig_merge(edited_kc, edited_kc)

    for km, kc_x in export_keymaps:

        km = km.active()

        f.write("# Map %s\n" % km.name)
        f.write("km = kc.keymaps.new('%s', space_type='%s', region_type='%s', modal=%s)\n\n" %
                (km.name, km.space_type, km.region_type, km.is_modal))
        for kmi in km.keymap_items:
            f.write(_kmistr(kmi, km.is_modal))
        f.write("\n")

    f.close()


def keyconfig_test(kc):

    def testEntry(kc, entry, src=None, parent=None):
        result = False

        idname, spaceid, regionid, children = entry

        km = kc.keymaps.find(idname, space_type=spaceid, region_type=regionid)

        if km:
            km = km.active()
            is_modal = km.is_modal

            if src:
                for item in km.keymap_items:
                    if src.compare(item):
                        print("===========")
                        print(parent.name)
                        print(_kmistr(src, is_modal).strip())
                        print(km.name)
                        print(_kmistr(item, is_modal).strip())
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
                            print(_kmistr(src, is_modal).strip())
                            print(_kmistr(item, is_modal).strip())
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


# Note, we may eventually replace existing logic with this
# so key configs are always data.
from .keyconfig_utils_experimental import (
    keyconfig_export_as_data,
    keyconfig_import_from_data,
)
