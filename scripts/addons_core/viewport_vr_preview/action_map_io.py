# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# -----------------------------------------------------------------------------
# Export Functions

__all__ = (
    "actionconfig_export_as_data",
    "actionconfig_import_from_data",
    "actionconfig_init_from_data",
    "actionmap_init_from_data",
    "actionmap_item_init_from_data",
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


def ami_args_as_data(ami):
    s = [
        f"\"type\": '{ami.type}'",
    ]

    sup = f"\"user_paths\": ["
    for user_path in ami.user_paths:
        sup += f"'{user_path.path}', "
    if len(ami.user_paths) > 0:
        sup = sup[:-2]
    sup += "]"
    s.append(sup)

    if ami.type == 'FLOAT' or ami.type == 'VECTOR2D':
        s.append(f"\"op\": '{ami.op}'")
        s.append(f"\"op_mode\": '{ami.op_mode}'")
        s.append(f"\"bimanual\": '{ami.bimanual}'")
        s.append(f"\"haptic_name\": '{ami.haptic_name}'")
        s.append(f"\"haptic_match_user_paths\": '{ami.haptic_match_user_paths}'")
        s.append(f"\"haptic_duration\": '{ami.haptic_duration}'")
        s.append(f"\"haptic_frequency\": '{ami.haptic_frequency}'")
        s.append(f"\"haptic_amplitude\": '{ami.haptic_amplitude}'")
        s.append(f"\"haptic_mode\": '{ami.haptic_mode}'")
    elif ami.type == 'POSE':
        s.append(f"\"pose_is_controller_grip\": '{ami.pose_is_controller_grip}'")
        s.append(f"\"pose_is_controller_aim\": '{ami.pose_is_controller_aim}'")

    return "{" + ", ".join(s) + "}"


def ami_data_from_args(ami, args):
    ami.type = args["type"]

    for path in args["user_paths"]:
        ami.user_paths.new(path)

    if ami.type == 'FLOAT' or ami.type == 'VECTOR2D':
        ami.op = args["op"]
        ami.op_mode = args["op_mode"]
        ami.bimanual = True if (args["bimanual"] == 'True') else False
        ami.haptic_name = args["haptic_name"]
        ami.haptic_match_user_paths = True if (args["haptic_match_user_paths"] == 'True') else False
        ami.haptic_duration = float(args["haptic_duration"])
        ami.haptic_frequency = float(args["haptic_frequency"])
        ami.haptic_amplitude = float(args["haptic_amplitude"])
        ami.haptic_mode = args["haptic_mode"]
    elif ami.type == 'POSE':
        ami.pose_is_controller_grip = True if (args["pose_is_controller_grip"] == 'True') else False
        ami.pose_is_controller_aim = True if (args["pose_is_controller_aim"] == 'True') else False


def _ami_properties_to_lines_recursive(level, properties, lines):
    from bpy.types import OperatorProperties

    def string_value(value):
        if isinstance(value, (str, bool, int, set)):
            return repr(value)
        elif isinstance(value, float):
            return repr_f32(value)
        elif getattr(value, '__len__', False):
            return repr(tuple(value))
        raise Exception(f"Export action configuration: can't write {value!r}")

    for pname in properties.bl_rna.properties.keys():
        if pname != "rna_type":
            value = getattr(properties, pname)
            if isinstance(value, OperatorProperties):
                lines_test = []
                _ami_properties_to_lines_recursive(level + 2, value, lines_test)
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


def _ami_properties_to_lines(level, ami_props, lines):
    if ami_props is None:
        return

    lines_test = [f"\"op_properties\":\n" f"{indent(level + 1)}" "["]
    _ami_properties_to_lines_recursive(level, ami_props, lines_test)
    if len(lines_test) > 1:
        lines_test.append("],\n")
        lines.extend(lines_test)


def _ami_attrs_or_none(level, ami):
    lines = []
    _ami_properties_to_lines(level + 1, ami.op_properties, lines)
    if not lines:
        return None
    return "".join(lines)


def amb_args_as_data(amb, type):
    s = [
        f"\"profile\": '{amb.profile}'",
    ]

    scp = f"\"component_paths\": ["
    for component_path in amb.component_paths:
        scp += f"'{component_path.path}', "
    if len(amb.component_paths) > 0:
        scp = scp[:-2]
    scp += "]"
    s.append(scp)

    if type == 'FLOAT' or type == 'VECTOR2D':
        s.append(f"\"threshold\": '{amb.threshold}'")
        if type == 'FLOAT':
            s.append(f"\"axis_region\": '{amb.axis0_region}'")
        else:  # type == 'VECTOR2D':
            s.append(f"\"axis0_region\": '{amb.axis0_region}'")
            s.append(f"\"axis1_region\": '{amb.axis1_region}'")
    elif type == 'POSE':
        s.append(f"\"pose_location\": '{amb.pose_location.x, amb.pose_location.y, amb.pose_location.z}'")
        s.append(f"\"pose_rotation\": '{amb.pose_rotation.x, amb.pose_rotation.y, amb.pose_rotation.z}'")

    return "{" + ", ".join(s) + "}"


def amb_data_from_args(amb, args, type):
    amb.profile = args["profile"]

    for path in args["component_paths"]:
        amb.component_paths.new(path)

    if type == 'FLOAT' or type == 'VECTOR2D':
        amb.threshold = float(args["threshold"])
        if type == 'FLOAT':
            amb.axis0_region = args["axis_region"]
        else:  # type == 'VECTOR2D':
            amb.axis0_region = args["axis0_region"]
            amb.axis1_region = args["axis1_region"]
    elif type == 'POSE':
        l = args["pose_location"].strip(')(').split(', ')
        amb.pose_location.x = float(l[0])
        amb.pose_location.y = float(l[1])
        amb.pose_location.z = float(l[2])
        l = args["pose_rotation"].strip(')(').split(', ')
        amb.pose_rotation.x = float(l[0])
        amb.pose_rotation.y = float(l[1])
        amb.pose_rotation.z = float(l[2])


def actionconfig_export_as_data(session_state, filepath, *, sort=False):
    export_actionmaps = []

    for am in session_state.actionmaps:
        export_actionmaps.append(am)

    if sort:
        export_actionmaps.sort(key=lambda k: k.name)

    with open(filepath, "w", encoding="utf-8") as fh:
        fw = fh.write

        # Use the file version since it includes the sub-version
        # which we can bump multiple times between releases.
        from bpy.app import version_file
        fw(f"actionconfig_version = {version_file!r}\n")
        del version_file

        fw("actionconfig_data = \\\n[")

        for am in export_actionmaps:
            fw("(")
            fw(f"\"{am.name:s}\",\n")

            fw(f"{indent(2)}" "{")
            fw(f"\"items\":\n")
            fw(f"{indent(3)}[")
            for ami in am.actionmap_items:
                fw(f"(")
                fw(f"\"{ami.name:s}\"")
                ami_args = ami_args_as_data(ami)
                ami_data = _ami_attrs_or_none(4, ami)
                if ami_data is None:
                    fw(f", ")
                else:
                    fw(",\n" f"{indent(5)}")

                fw(ami_args)
                if ami_data is None:
                    fw(", None,\n")
                else:
                    fw(",\n")
                    fw(f"{indent(5)}" "{")
                    fw(ami_data)
                    fw(f"{indent(6)}")
                    fw("}," f"{indent(5)}")
                    fw("\n")

                fw(f"{indent(5)}" "{")
                fw(f"\"bindings\":\n")
                fw(f"{indent(6)}[")
                for amb in ami.bindings:
                    fw(f"(")
                    fw(f"\"{amb.name:s}\"")
                    fw(f", ")
                    amb_args = amb_args_as_data(amb, ami.type)
                    fw(amb_args)
                    fw("),\n" f"{indent(7)}")
                fw("],\n" f"{indent(6)}")
                fw("},\n" f"{indent(5)}")
                fw("),\n" f"{indent(4)}")

            fw("],\n" f"{indent(3)}")
            fw("},\n" f"{indent(2)}")
            fw("),\n" f"{indent(1)}")

        fw("]\n")
        fw("\n\n")
        fw("if __name__ == \"__main__\":\n")

        # We could remove this in the future, as loading new action-maps in older Blender versions
        # makes less and less sense as Blender changes.
        fw("    # Only add keywords that are supported.\n")
        fw("    from bpy.app import version as blender_version\n")
        fw("    keywords = {}\n")
        fw("    if blender_version >= (3, 0, 0):\n")
        fw("        keywords[\"actionconfig_version\"] = actionconfig_version\n")

        fw("    import os\n")
        fw("    from viewport_vr_preview.io import actionconfig_import_from_data\n")
        fw("    actionconfig_import_from_data(\n")
        fw("        os.path.splitext(os.path.basename(__file__))[0],\n")
        fw("        actionconfig_data,\n")
        fw("        **keywords,\n")
        fw("    )\n")


# -----------------------------------------------------------------------------
# Import Functions

def _ami_props_setattr(ami_name, ami_props, attr, value):
    if type(value) is list:
        ami_subprop = getattr(ami_props, attr)
        for subattr, subvalue in value:
            _ami_props_setattr(ami_subprop, subattr, subvalue)
        return

    try:
        setattr(ami_props, attr, value)
    except AttributeError:
        print(f"Warning: property '{attr}' not found in action map item '{ami_name}'")
    except Exception as ex:
        print(f"Warning: {ex!r}")


def actionmap_item_init_from_data(ami, ami_bindings):
    new_fn = getattr(ami.bindings, "new")
    for (amb_name, amb_args) in ami_bindings:
        amb = new_fn(amb_name, True)
        amb_data_from_args(amb, amb_args, ami.type)


def actionmap_init_from_data(am, am_items):
    new_fn = getattr(am.actionmap_items, "new")
    for (ami_name, ami_args, ami_data, ami_content) in am_items:
        ami = new_fn(ami_name, True)
        ami_data_from_args(ami, ami_args)
        if ami_data is not None:
            ami_props_data = ami_data.get("op_properties", None)
            if ami_props_data is not None:
                ami_props = ami.op_properties
                assert type(ami_props_data) is list
                for attr, value in ami_props_data:
                    _ami_props_setattr(ami_name, ami_props, attr, value)
        ami_bindings = ami_content["bindings"]
        assert type(ami_bindings) is list
        actionmap_item_init_from_data(ami, ami_bindings)


def actionconfig_init_from_data(session_state, actionconfig_data, actionconfig_version):
    # Load data in the format defined above.
    #
    # Runs at load time, keep this fast!
    if actionconfig_version is not None:
        from .versioning import actionconfig_update
        actionconfig_data = actionconfig_update(actionconfig_data, actionconfig_version)

    for (am_name, am_content) in actionconfig_data:
        am = session_state.actionmaps.new(session_state, am_name, True)
        am_items = am_content["items"]
        # Check here instead of inside 'actionmap_init_from_data'
        # because we want to allow both tuple & list types in that case.
        #
        # For full action maps, ensure these are always lists to allow for extending them
        # in a generic way that doesn't have to check for the type each time.
        assert type(am_items) is list
        actionmap_init_from_data(am, am_items)


def actionconfig_import_from_data(session_state, actionconfig_data, *, actionconfig_version=(0, 0, 0)):
    # Load data in the format defined above.
    #
    # Runs at load time, keep this fast!
    import bpy
    actionconfig_init_from_data(session_state, actionconfig_data, actionconfig_version)
