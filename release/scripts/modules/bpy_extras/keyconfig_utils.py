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


def _km_expand_from_toolsystem(space_type, context_mode):
    def _fn():
        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        for cls in ToolSelectPanelHelper.__subclasses__():
            if cls.bl_space_type == space_type:
                return cls.keymap_ui_hierarchy(context_mode)
        raise Exception("keymap not found")
    return _fn


def _km_hierarchy_iter_recursive(items):
    for sub in items:
        if callable(sub):
            yield from sub()
        else:
            yield (*sub[:3], list(_km_hierarchy_iter_recursive(sub[3])))


def km_hierarchy():
    return list(_km_hierarchy_iter_recursive(_km_hierarchy))


# bpy.type.KeyMap: (km.name, km.space_type, km.region_type, [...])

#    ('Script', 'EMPTY', 'WINDOW', []),


# Access via 'km_hierarchy'.
_km_hierarchy = [
    ('Window', 'EMPTY', 'WINDOW', []),  # file save, window change, exit
    ('Screen', 'EMPTY', 'WINDOW', [     # full screen, undo, screenshot
        ('Screen Editing', 'EMPTY', 'WINDOW', []),    # re-sizing, action corners
        ('Header', 'EMPTY', 'WINDOW', []),            # header stuff (per region)
    ]),

    ('View2D', 'EMPTY', 'WINDOW', []),    # view 2d navigation (per region)
    ('View2D Buttons List', 'EMPTY', 'WINDOW', []),  # view 2d with buttons navigation

    ('User Interface', 'EMPTY', 'WINDOW', []),

    ('3D View', 'VIEW_3D', 'WINDOW', [  # view 3d navigation and generic stuff (select, transform)
        ('Object Mode', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'OBJECT'),
        ]),
        ('Mesh', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_MESH'),
        ]),
        ('Curve', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_CURVE'),
        ]),
        ('Armature', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_ARMATURE'),
        ]),
        ('Metaball', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_METABALL'),
        ]),
        ('Lattice', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_LATTICE'),
        ]),
        ('Font', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_TEXT'),
        ]),

        ('Pose', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'POSE'),
        ]),

        ('Vertex Paint', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'PAINT_VERTEX'),
        ]),
        ('Weight Paint', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'PAINT_WEIGHT'),
        ]),
        ('Weight Paint Vertex Selection', 'EMPTY', 'WINDOW', []),
        ('Face Mask', 'EMPTY', 'WINDOW', []),
        # image and view3d
        ('Image Paint', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'PAINT_TEXTURE'),
        ]),
        ('Sculpt', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'SCULPT'),
        ]),

        ('Particle', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'PARTICLE'),
        ]),

        ('Knife Tool Modal Map', 'EMPTY', 'WINDOW', []),
        ('Custom Normals Modal Map', 'EMPTY', 'WINDOW', []),
        ('Paint Stroke Modal', 'EMPTY', 'WINDOW', []),
        ('Paint Curve', 'EMPTY', 'WINDOW', []),

        ('Object Non-modal', 'EMPTY', 'WINDOW', []),  # mode change

        ('View3D Walk Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Fly Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Rotate Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Move Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Zoom Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Dolly Modal', 'EMPTY', 'WINDOW', []),

        # toolbar and properties
        ('3D View Generic', 'VIEW_3D', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', None),
        ]),
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
        ('Image Generic', 'IMAGE_EDITOR', 'WINDOW', [
            _km_expand_from_toolsystem('IMAGE_EDITOR', None),
        ]),
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
    ('Gesture Box', 'EMPTY', 'WINDOW', []),

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
# Import/Export Functions


def indent(levels):
    return levels * " "


def round_float_32(f):
    from struct import pack, unpack
    return unpack("f", pack("f", f))[0]


def repr_f32(f):
    f_round = round_float_32(f)
    f_str = repr(f)
    f_str_frac = f_str.partition(".")[2]
    if not f_str_frac:
        return f_str
    for i in range(1, len(f_str_frac)):
        f_test = round(f, i)
        f_test_round = round_float_32(f_test)
        if f_test_round == f_round:
            return "%.*f" % (i, f_test)
    return f_str


def kmi_args_as_data(kmi):
    s = [
        f"\"type\": '{kmi.type}'",
        f"\"value\": '{kmi.value}'"
    ]

    if kmi.any:
        s.append("\"any\": True")
    else:
        if kmi.shift:
            s.append("\"shift\": True")
        if kmi.ctrl:
            s.append("\"ctrl\": True")
        if kmi.alt:
            s.append("\"alt\": True")
        if kmi.oskey:
            s.append("\"oskey\": True")
    if kmi.key_modifier and kmi.key_modifier != 'NONE':
        s.append(f"\"key_modifier\": '{kmi.key_modifier}'")

    return "{" + ", ".join(s) + "}"


def _kmi_properties_to_lines_recursive(level, properties, lines):
    from bpy.types import OperatorProperties

    def string_value(value):
        if isinstance(value, (str, bool, int)):
            return repr(value)
        elif isinstance(value, float):
            return repr_f32(value)
        elif getattr(value, '__len__', False):
            return repr(tuple(value))
        raise Exception(f"Export key configuration: can't write {value!r}")

    for pname in properties.bl_rna.properties.keys():
        if pname != "rna_type":
            value = getattr(properties, pname)
            if isinstance(value, OperatorProperties):
                lines_test = []
                _kmi_properties_to_lines_recursive(level + 2, value, lines_test)
                if lines_test:
                    lines.append(f"(")
                    lines.append(f"\"{pname}\",\n")
                    lines.append(f"{indent(level + 3)}" "[")
                    lines.extend(lines_test)
                    lines.append("],\n")
                    lines.append(f"{indent(level + 3)}" "),\n" f"{indent(level + 2)}")
                del lines_test
            elif properties.is_property_set(pname):
                value = string_value(value)
                lines.append((f"(\"{pname}\", {value:s}),\n" f"{indent(level + 2)}"))


def _kmi_properties_to_lines(level, kmi_props, lines):
    if kmi_props is None:
        return

    lines_test = [f"\"properties\":\n" f"{indent(level + 1)}" "["]
    _kmi_properties_to_lines_recursive(level, kmi_props, lines_test)
    if len(lines_test) > 1:
        lines_test.append("],\n")
        lines.extend(lines_test)


def _kmi_attrs_or_none(level, kmi):
    lines = []
    _kmi_properties_to_lines(level + 1, kmi.properties, lines)
    if kmi.active is False:
        lines.append(f"{indent(level)}\"active\":" "False,\n")
    if not lines:
        return None
    return "".join(lines)


def keyconfig_export_as_data(wm, kc, filepath, *, all_keymaps=False):
    # Alternate foramt

    # Generate a list of keymaps to export:
    #
    # First add all user_modified keymaps (found in keyconfigs.user.keymaps list),
    # then add all remaining keymaps from the currently active custom keyconfig.
    #
    # This will create a final list of keymaps that can be used as a "diff" against
    # the default blender keyconfig, recreating the current setup from a fresh blender
    # without needing to export keymaps which haven't been edited.

    from .keyconfig_utils import keyconfig_merge

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

    with open(filepath, "w") as fh:
        fw = fh.write
        fw("keyconfig_data = \\\n[")

        for km, kc_x in export_keymaps:
            km = km.active()
            fw("(")
            fw(f"\"{km.name:s}\",\n")
            fw(f"{indent(2)}" "{")
            fw(f"\"space_type\": '{km.space_type:s}'")
            fw(f", \"region_type\": '{km.region_type:s}'")
            # We can detect from the kind of items.
            if km.is_modal:
                fw(", \"modal\": True")
            fw("},\n")
            fw(f"{indent(2)}" "{")
            is_modal = km.is_modal
            fw(f"\"items\":\n")
            fw(f"{indent(3)}[")
            for kmi in km.keymap_items:
                if is_modal:
                    kmi_id = kmi.propvalue
                else:
                    kmi_id = kmi.idname
                fw(f"(")
                kmi_args = kmi_args_as_data(kmi)
                kmi_data = _kmi_attrs_or_none(4, kmi)
                fw(f"\"{kmi_id:s}\"")
                if kmi_data is None:
                    fw(f", ")
                else:
                    fw(",\n" f"{indent(5)}")

                fw(kmi_args)
                if kmi_data is None:
                    fw(", None),\n")
                else:
                    fw(",\n")
                    fw(f"{indent(5)}" "{")
                    fw(kmi_data)
                    fw(f"{indent(6)}")
                    fw("},\n" f"{indent(5)}")
                    fw("),\n")
                fw(f"{indent(4)}")
            fw("],\n" f"{indent(3)}")
            fw("},\n" f"{indent(2)}")
            fw("),\n" f"{indent(1)}")

        fw("]\n")
        fw("\n\n")
        fw("if __name__ == \"__main__\":\n")
        fw("    import os\n")
        fw("    from bpy_extras.keyconfig_utils import keyconfig_import_from_data\n")
        fw("    keyconfig_import_from_data(os.path.splitext(os.path.basename(__file__))[0], keyconfig_data)\n")


def _kmi_props_setattr(kmi_props, attr, value):
    if type(value) is list:
        kmi_subprop = getattr(kmi_props, attr)
        for subattr, subvalue in value:
            _kmi_props_setattr(kmi_subprop, subattr, subvalue)
        return

    try:
        setattr(kmi_props, attr, value)
    except AttributeError:
        print(f"Warning: property '{attr}' not found in keymap item '{kmi_props.__class__.__name__}'")
    except Exception as ex:
        print(f"Warning: {ex!r}")


def keymap_items_from_data(km, km_items, is_modal=False):
    new_fn = getattr(km.keymap_items, "new_modal" if is_modal else "new")
    for (kmi_idname, kmi_args, kmi_data) in km_items:
        kmi = new_fn(kmi_idname, **kmi_args)
        if kmi_data is not None:
            if not kmi_data.get("active", True):
                kmi.active = False
            kmi_props_data = kmi_data.get("properties", None)
            if kmi_props_data is not None:
                kmi_props = kmi.properties
                for attr, value in kmi_props_data:
                    _kmi_props_setattr(kmi_props, attr, value)


def keyconfig_import_from_data(name, keyconfig_data):
    # Load data in the format defined above.
    #
    # Runs at load time, keep this fast!

    import bpy
    wm = bpy.context.window_manager
    kc = wm.keyconfigs.new(name)
    for (km_name, km_args, km_content) in keyconfig_data:
        km = kc.keymaps.new(km_name, **km_args)
        keymap_items_from_data(km, km_content["items"], is_modal=km_args.get("modal", False))


def keyconfig_module_from_preset(name, preset_reference_filename=None):
    import os
    import importlib.util
    if preset_reference_filename is not None:
        preset_path = os.path.join(os.path.dirname(preset_reference_filename), name + ".py")
    else:
        preset_path = None

    # External presets may want to re-use other presets too.
    if not (preset_path and os.path.exists(preset_path)):
        preset_path = bpy.utils.preset_find(name, "keyconfig")

    # module name isn't used or added to 'sys.modules'.
    mod_spec = importlib.util.spec_from_file_location("__main__", preset_path)
    mod = importlib.util.module_from_spec(mod_spec)
    mod_spec.loader.exec_module(mod)
    return mod


# -----------------------------------------------------------------------------
# Utility Functions

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
    for entry in km_hierarchy():
        if testEntry(kc, entry):
            result = True
    return result
