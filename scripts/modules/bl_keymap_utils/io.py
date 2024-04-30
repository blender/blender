# SPDX-FileCopyrightText: 2018-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# -----------------------------------------------------------------------------
# Export Functions

__all__ = (
    "_init_properties_from_data",  # Shared with gizmo default property initialization.
    "keyconfig_export_as_data",
    "keyconfig_import_from_data",
    "keyconfig_init_from_data",
    "keyconfig_merge",
    "keymap_init_from_data",
)


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
            return "{:.{:d}f}".format(f_test, i)
    return f_str


def kmi_args_as_data(kmi):
    s = [
        f"\"type\": '{kmi.type}'",
        f"\"value\": '{kmi.value}'"
    ]

    if kmi.any:
        s.append("\"any\": True")
    else:
        for attr in ("shift", "ctrl", "alt", "oskey"):
            if mod := getattr(kmi, attr):
                s.append(f"\"{attr:s}\": " + ("-1" if mod == -1 else "True"))
    if (mod := kmi.key_modifier) and (mod != 'NONE'):
        s.append(f"\"key_modifier\": '{mod:s}'")
    if (direction := kmi.direction) and (direction != 'ANY'):
        s.append(f"\"direction\": '{direction:s}'")

    if kmi.repeat:
        if (
                (kmi.map_type == 'KEYBOARD' and kmi.value in {'PRESS', 'ANY'}) or
                (kmi.map_type == 'TEXTINPUT')
        ):
            s.append("\"repeat\": True")

    return "{" + ", ".join(s) + "}"


def _kmi_properties_to_lines_recursive(level, properties, lines):
    from bpy.types import OperatorProperties

    def string_value(value):
        if isinstance(value, (str, bool, int, set)):
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
                    lines.append("(")
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
    # Alternate format

    # Generate a list of keymaps to export:
    #
    # First add all user_modified keymaps (found in keyconfigs.user.keymaps list),
    # then add all remaining keymaps from the currently active custom keyconfig.
    #
    # Sort the resulting list according to top context name,
    # while this isn't essential, it makes comparing keymaps simpler.
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

    # Sort the keymap list by top context name before exporting,
    # not essential, just convenient to order them predictably.
    export_keymaps.sort(key=lambda k: k[0].name)

    with open(filepath, "w", encoding="utf-8") as fh:
        fw = fh.write

        # Use the file version since it includes the sub-version
        # which we can bump multiple times between releases.
        from bpy.app import version_file
        fw(f"keyconfig_version = {version_file!r}\n")
        del version_file

        fw("keyconfig_data = \\\n[")

        for km, _kc_x in export_keymaps:
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
            fw("\"items\":\n")
            fw(f"{indent(3)}[")
            for kmi in km.keymap_items:
                if is_modal:
                    kmi_id = kmi.propvalue
                else:
                    kmi_id = kmi.idname
                fw("(")
                kmi_args = kmi_args_as_data(kmi)
                kmi_data = _kmi_attrs_or_none(4, kmi)
                fw(f"\"{kmi_id:s}\"")
                if kmi_data is None:
                    fw(", ")
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

        # We could remove this in the future, as loading new key-maps in older Blender versions
        # makes less and less sense as Blender changes.
        fw("    # Only add keywords that are supported.\n")
        fw("    from bpy.app import version as blender_version\n")
        fw("    keywords = {}\n")
        fw("    if blender_version >= (2, 92, 0):\n")
        fw("        keywords[\"keyconfig_version\"] = keyconfig_version\n")

        fw("    import os\n")
        fw("    from bl_keymap_utils.io import keyconfig_import_from_data\n")
        fw("    keyconfig_import_from_data(\n")
        fw("        os.path.splitext(os.path.basename(__file__))[0],\n")
        fw("        keyconfig_data,\n")
        fw("        **keywords,\n")
        fw("    )\n")


# -----------------------------------------------------------------------------
# Import Functions
#
# NOTE: unlike export, this runs on startup.
# Take care making changes that could impact performance.

def _init_properties_from_data(base_props, base_value):
    assert type(base_value) is list
    for attr, value in base_value:
        if type(value) is list:
            base_props.property_unset(attr)
            props = getattr(base_props, attr)
            _init_properties_from_data(props, value)
        else:
            try:
                setattr(base_props, attr, value)
            except AttributeError:
                print(f"Warning: property '{attr}' not found in item '{base_props.__class__.__name__}'")
            except BaseException as ex:
                print(f"Warning: {ex!r}")


def keymap_init_from_data(km, km_items, is_modal=False):
    new_fn = getattr(km.keymap_items, "new_modal" if is_modal else "new")
    for (kmi_idname, kmi_args, kmi_data) in km_items:
        kmi = new_fn(kmi_idname, **kmi_args)
        if kmi_data is not None:
            if not kmi_data.get("active", True):
                kmi.active = False
            kmi_props_data = kmi_data.get("properties", None)
            if kmi_props_data is not None:
                kmi_props = kmi.properties
                assert type(kmi_props_data) is list
                _init_properties_from_data(kmi_props, kmi_props_data)


def keyconfig_init_from_data(kc, keyconfig_data):
    # Load data in the format defined above.
    #
    # Runs at load time, keep this fast!
    for (km_name, km_args, km_content) in keyconfig_data:
        km = kc.keymaps.new(km_name, **km_args)
        km_items = km_content["items"]
        # Check here instead of inside 'keymap_init_from_data'
        # because we want to allow both tuple & list types in that case.
        #
        # For full keymaps, ensure these are always lists to allow for extending them
        # in a generic way that doesn't have to check for the type each time.
        assert type(km_items) is list
        keymap_init_from_data(km, km_items, is_modal=km_args.get("modal", False))


def keyconfig_import_from_data(name, keyconfig_data, *, keyconfig_version=(0, 0, 0)):
    # Load data in the format defined above.
    #
    # Runs at load time, keep this fast!

    import bpy
    wm = bpy.context.window_manager
    kc = wm.keyconfigs.new(name)
    if keyconfig_version is not None:
        from .versioning import keyconfig_update
        keyconfig_data = keyconfig_update(keyconfig_data, keyconfig_version)
    keyconfig_init_from_data(kc, keyconfig_data)
    return kc


# -----------------------------------------------------------------------------
# Utility Functions

def keyconfig_merge(kc1, kc2):
    """ note: kc1 takes priority over kc2
    """
    kc1_names = {km.name for km in kc1.keymaps}
    merged_keymaps = [(km, kc1) for km in kc1.keymaps]
    if kc1 != kc2:
        merged_keymaps.extend(
            (km, kc2)
            for km in kc2.keymaps
            if km.name not in kc1_names
        )
    return merged_keymaps
