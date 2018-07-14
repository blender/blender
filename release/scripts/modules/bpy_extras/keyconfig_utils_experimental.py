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

__all__ = (
    "keyconfig_export_as_data",
    "keyconfig_import_from_data",
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


def keyconfig_import_from_data(name, keyconfig_data):
    # Load data in the format defined above.
    #
    # Runs at load time, keep this fast!

    def kmi_props_setattr(kmi_props, attr, value):
        if type(value) is list:
            kmi_subprop = getattr(kmi_props, attr)
            for subattr, subvalue in value:
                kmi_props_setattr(kmi_subprop, subattr, subvalue)
            return

        try:
            setattr(kmi_props, attr, value)
        except AttributeError:
            print(f"Warning: property '{attr}' not found in keymap item '{kmi_props.__class__.__name__}'")
        except Exception as ex:
            print(f"Warning: {ex!r}")

    import bpy
    wm = bpy.context.window_manager
    kc = wm.keyconfigs.new(name)
    del name

    for (km_name, km_args, km_content) in keyconfig_data:
        km = kc.keymaps.new(km_name, **km_args)
        is_modal = km_args.get("modal", False)
        new_fn = getattr(km.keymap_items, "new_modal" if is_modal else "new")
        for (kmi_idname, kmi_args, kmi_data) in km_content["items"]:
            kmi = new_fn(kmi_idname, **kmi_args)
            if kmi_data is not None:
                if not kmi_data.get("active", True):
                    kmi.active = False
                kmi_props_data = kmi_data.get("properties", None)
                if kmi_props_data is not None:
                    kmi_props = kmi.properties
                    for attr, value in kmi_props_data:
                        kmi_props_setattr(kmi_props, attr, value)
