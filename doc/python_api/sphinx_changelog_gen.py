# SPDX-FileCopyrightText: 2011-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
---------------

Dump the Python API into a JSON file, or generate changelogs from those JSON API dumps.

Typically, changelog output from this tool should be added into "doc/python_api/rst/change_log.rst"

API dump files are saved together with the generated API doc on the server, with a general index file.
This way the changelog generation simply needs to re-download the previous version's dump for the diffing process.

---------------

# Dump API blender_version.json in CWD:
blender --background  --factory-startup --python doc/python_api/sphinx_changelog_gen.py -- \
        --indexpath="path/to/api/docs/api_dump_index.json" \
        dump --filepath-out="path/to/api/docs/<version>/api_dump.json"

# Create changelog:
blender --background --factory-startup --python doc/python_api/sphinx_changelog_gen.py -- \
        --indexpath="path/to/api/docs/api_dump_index.json" \
        changelog --filepath-out doc/python_api/rst/change_log.rst

# Api comparison can also run without Blender,
# will by default generate changeloig between the last two available versions listed in the index,
# unless input files are provided explicitly:
python doc/python_api/sphinx_changelog_gen.py -- \
        --indexpath="path/to/api/docs/api_dump_index.json" \
        changelog --filepath-in-from blender_api_2_63_0.json \
                  --filepath-in-to   blender_api_2_64_0.json \
                  --filepath-out changes.rst

--------------

API dump index format:

{[version_main, version_sub]: "<version>/api_dump.json", ...
}

API dump format:

[
    [version_main, vserion_sub, version_path],
    {"module.name":
        {"parent.class":
            {"basic_type", "member_name":
                ["Name", type, range, length, default, descr, f_args, f_arg_types, f_ret_types]}, ...
        }, ...
    }
]

"""
__all__ = (
    "main",
)

import json
import os


api_names = "basic_type" "name", "type", "range", "length", "default", "descr", "f_args", "f_arg_types", "f_ret_types"
API_BASIC_TYPE = 0
API_F_ARGS = 7


def api_version():
    try:
        import bpy
    except ModuleNotFoundError:
        return None, None
    version = tuple(bpy.app.version[:2])
    version_key = "%d.%d" % (version[0], version[1])
    return version, version_key


def api_version_previous_in_index(index, version):
    print("Searching for previous version to %s in %r" % (version, index))
    version_prev = (version[0], version[1])
    while True:
        version_prev = (version_prev[0], version_prev[1] - 1)
        if version_prev[1] < 0:
            version_prev = (version_prev[0] - 1, 99)
        if version_prev[0] < 0:
            return None, None
        version_prev_key = "%d.%d" % (version_prev[0], version_prev[1])
        if version_prev_key in index:
            print("Found previous version %s: %r" % (version_prev, index[version_prev_key]))
            return version_prev, version_prev_key


class JSONEncoderAPIDump(json.JSONEncoder):
    def default(self, o):
        if o is ...:
            return "..."
        if isinstance(o, set):
            return tuple(o)
        return json.JSONEncoder.default(self, o)


def api_dump(args):
    import _rna_info as rna_info
    import inspect

    version, version_key = api_version()
    if version is None:
        raise ValueError("API dumps can only be generated from within Blender.")

    dump = {}
    dump_module = dump["bpy.types"] = {}

    struct = rna_info.BuildRNAInfo()[0]
    for _struct_id, struct_info in sorted(struct.items()):

        struct_id_str = struct_info.identifier

        if rna_info.rna_id_ignore(struct_id_str):
            continue

        for base in struct_info.get_bases():
            struct_id_str = base.identifier + "." + struct_id_str

        dump_class = dump_module[struct_id_str] = {}

        props = [(prop.identifier, prop) for prop in struct_info.properties]
        for prop_id, prop in sorted(props):
            # if prop.type == 'boolean':
            #     continue
            prop_type = prop.type
            prop_length = prop.array_length
            prop_range = round(prop.min, 4), round(prop.max, 4)
            prop_default = prop.default
            if type(prop_default) is float:
                prop_default = round(prop_default, 4)

            if prop_range[0] == -1 and prop_range[1] == -1:
                prop_range = None

            dump_class[prop_id] = (
                "prop_rna",                 # basic_type
                prop.name,                  # name
                prop_type,                  # type
                prop_range,                 # range
                prop_length,                # length
                prop.default,               # default
                prop.description,           # descr
                Ellipsis,                   # f_args
                Ellipsis,                   # f_arg_types
                Ellipsis,                   # f_ret_types
            )
        del props

        # Python properties, tricky since we don't know much about them.
        for prop_id, attr in struct_info.get_py_properties():

            dump_class[prop_id] = (
                "prop_py",                  # basic_type
                Ellipsis,                   # name
                Ellipsis,                   # type
                Ellipsis,                   # range
                Ellipsis,                   # length
                Ellipsis,                   # default
                attr.__doc__,               # descr
                Ellipsis,                   # f_args
                Ellipsis,                   # f_arg_types
                Ellipsis,                   # f_ret_types
            )

        # kludge func -> props
        funcs = [(func.identifier, func) for func in struct_info.functions]
        for func_id, func in funcs:

            func_ret_types = tuple([prop.type for prop in func.return_values])
            func_args_ids = tuple([prop.identifier for prop in func.args])
            func_args_type = tuple([prop.type for prop in func.args])

            dump_class[func_id] = (
                "func_rna",                 # basic_type
                Ellipsis,                   # name
                Ellipsis,                   # type
                Ellipsis,                   # range
                Ellipsis,                   # length
                Ellipsis,                   # default
                func.description,           # descr
                func_args_ids,              # f_args
                func_args_type,             # f_arg_types
                func_ret_types,             # f_ret_types
            )
        del funcs

        # kludge func -> props
        funcs = struct_info.get_py_functions()
        for func_id, attr in funcs:
            # arg_str = inspect.formatargspec(*inspect.getargspec(py_func))

            sig = inspect.signature(attr)
            func_args_ids = [k for k, v in sig.parameters.items()]

            dump_class[func_id] = (
                "func_py",                  # basic_type
                Ellipsis,                   # name
                Ellipsis,                   # type
                Ellipsis,                   # range
                Ellipsis,                   # length
                Ellipsis,                   # default
                attr.__doc__,               # descr
                func_args_ids,              # f_args
                Ellipsis,                   # f_arg_types
                Ellipsis,                   # f_ret_types
            )
        del funcs

    filepath_out = args.filepath_out
    with open(filepath_out, 'w', encoding='utf-8') as file_handle:
        json.dump((version, dump), file_handle, cls=JSONEncoderAPIDump)

    indexpath = args.indexpath
    rootpath = os.path.dirname(indexpath)
    if os.path.exists(indexpath):
        with open(indexpath, 'r', encoding='utf-8') as file_handle:
            index = json.load(file_handle)
    else:
        index = {}
    index[version_key] = os.path.relpath(filepath_out, rootpath)
    with open(indexpath, 'w', encoding='utf-8') as file_handle:
        json.dump(index, file_handle)

    print("API version %s dumped into %r, and index %r has been updated" % (version_key, filepath_out, indexpath))


def compare_props(a, b, fuzz=0.75):
    # must be same basic_type, function != property
    if a[0] != b[0]:
        return False

    tot = 0
    totlen = 0
    for i in range(1, len(a)):
        if not (Ellipsis is a[i] is b[i]):
            tot += (a[i] == b[i])
            totlen += 1

    return ((tot / totlen) >= fuzz)


def api_changelog(args):
    indexpath = args.indexpath
    filepath_in_from = args.filepath_in_from
    filepath_in_to = args.filepath_in_to
    filepath_out = args.filepath_out

    rootpath = os.path.dirname(indexpath)

    version, version_key = api_version()
    if version is None and (filepath_in_from is None or filepath_in_to is None):
        raise ValueError("API dumps files must be given when ran outside of Blender.")

    with open(indexpath, 'r', encoding='utf-8') as file_handle:
        index = json.load(file_handle)

    if filepath_in_to is None:
        filepath_in_to = index.get(version_key, None)
    if filepath_in_to is None:
        raise ValueError("Cannot find API dump file for Blender version " + str(version) + " in index file.")

    print("Found to file: %r" % filepath_in_to)

    if filepath_in_from is None:
        version_from, version_from_key = api_version_previous_in_index(index, version)
        if version_from is None:
            raise ValueError("No previous version of Blender could be found in the index.")
        filepath_in_from = index.get(version_from_key, None)
    if filepath_in_from is None:
        raise ValueError(
            "Cannot find API dump file for previous Blender version " +
            str(version_from) +
            " in index file."
        )

    print("Found from file: %r" % filepath_in_from)

    with open(os.path.join(rootpath, filepath_in_from), 'r', encoding='utf-8') as file_handle:
        _, dict_from = json.load(file_handle)

    with open(os.path.join(rootpath, filepath_in_to), 'r', encoding='utf-8') as file_handle:
        dump_version, dict_to = json.load(file_handle)
        assert tuple(dump_version) == version

    api_changes = []

    # first work out what moved
    for mod_id, mod_data in dict_to.items():
        mod_data_other = dict_from[mod_id]
        for class_id, class_data in mod_data.items():
            class_data_other = mod_data_other.get(class_id)
            if class_data_other is None:
                # TODO, document new structs
                continue

            # find the props which are not in either
            set_props_new = set(class_data.keys())
            set_props_other = set(class_data_other.keys())
            set_props_shared = set_props_new & set_props_other

            props_moved = []
            props_new = []
            props_old = []
            func_args = []

            set_props_old = set_props_other - set_props_shared
            set_props_new = set_props_new - set_props_shared

            # first find settings which have been moved old -> new
            for prop_id_old in set_props_old.copy():
                prop_data_other = class_data_other[prop_id_old]
                for prop_id_new in set_props_new.copy():
                    prop_data = class_data[prop_id_new]
                    if compare_props(prop_data_other, prop_data):
                        props_moved.append((prop_id_old, prop_id_new))

                        # remove
                        set_props_old.discard(prop_id_old)
                        set_props_new.remove(prop_id_new)

            # func args
            for prop_id in set_props_shared:
                prop_data = class_data[prop_id]
                prop_data_other = class_data_other[prop_id]
                if prop_data[API_BASIC_TYPE] == prop_data_other[API_BASIC_TYPE]:
                    if prop_data[API_BASIC_TYPE].startswith("func"):
                        args_new = prop_data[API_F_ARGS]
                        args_old = prop_data_other[API_F_ARGS]

                        if args_new != args_old:
                            func_args.append((prop_id, args_old, args_new))

            if props_moved or set_props_new or set_props_old or func_args:
                props_moved.sort()
                props_new[:] = sorted(set_props_new)
                props_old[:] = sorted(set_props_old)
                func_args.sort()

                api_changes.append((mod_id, class_id, props_moved, props_new, props_old, func_args))

    # also document function argument changes

    with open(filepath_out, 'w', encoding='utf-8') as fout:
        fw = fout.write

        # Write header.
        fw(""
           ":tocdepth: 2\n"
           "\n"
           "Change Log\n"
           "**********\n"
           "\n"
           "Changes in Blender's Python API between releases.\n"
           "\n"
           ".. note, this document is auto generated by sphinx_changelog_gen.py\n"
           "\n"
           "\n"
           "%s to %s\n"
           "============\n"
           "\n" % (version_from_key, version_key))

        def write_title(title, title_char):
            fw("%s\n%s\n\n" % (title, title_char * len(title)))

        for mod_id, class_id, props_moved, props_new, props_old, func_args in api_changes:
            class_name = class_id.split(".")[-1]
            title = mod_id + "." + class_name
            write_title(title, "-")

            if props_new:
                write_title("Added", "^")
                for prop_id in props_new:
                    fw("* :class:`%s.%s.%s`\n" % (mod_id, class_name, prop_id))
                fw("\n")

            if props_old:
                write_title("Removed", "^")
                for prop_id in props_old:
                    fw("* **%s**\n" % prop_id)  # can't link to removed docs
                fw("\n")

            if props_moved:
                write_title("Renamed", "^")
                for prop_id_old, prop_id in props_moved:
                    fw("* **%s** -> :class:`%s.%s.%s`\n" % (prop_id_old, mod_id, class_name, prop_id))
                fw("\n")

            if func_args:
                write_title("Function Arguments", "^")
                for func_id, args_old, args_new in func_args:
                    args_new = ", ".join(args_new)
                    args_old = ", ".join(args_old)
                    fw("* :class:`%s.%s.%s` (%s), *was (%s)*\n" % (mod_id, class_name, func_id, args_new, args_old))
                fw("\n")

    print("Written: %r" % filepath_out)


def main(argv=None):
    import sys
    import argparse

    if argv is None:
        argv = sys.argv

    if "--" not in argv:
        argv = []  # as if no args are passed
    else:
        argv = argv[argv.index("--") + 1:]  # get all args after "--"

    # When --help or no args are given, print this help
    usage_text = "Run Blender in background mode with this script: "
    "blender --background --factory-startup --python %s -- [options]" % os.path.basename(__file__)

    parser = argparse.ArgumentParser(description=usage_text,
                                     epilog=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "--indexpath", dest="indexpath", metavar='FILE', required=True,
        help="Path of the JSON file containing the index of all available API dumps.")

    parser_commands = parser.add_subparsers(required=True)

    parser_dump = parser_commands.add_parser('dump', help="Dump the current Blender Python API into a JSON file.")
    parser_dump.add_argument(
        "--filepath-out", dest="filepath_out", metavar='FILE', required=True,
        help="Path of the JSON file containing the dump of the API.")
    parser_dump.set_defaults(func=api_dump)

    parser_changelog = parser_commands.add_parser(
        'changelog',
        help="Generate the RST changelog page based on two Blender Python API JSON dumps.",
    )

    parser_changelog.add_argument(
        "--filepath-in-from", dest="filepath_in_from", metavar='FILE', default=None,
        help="JSON dump file to compare from (typically, previous version). "
             "If not given, will be automatically determined from current Blender version and index file.")
    parser_changelog.add_argument(
        "--filepath-in-to", dest="filepath_in_to", metavar='FILE', default=None,
        help="JSON dump file to compare to (typically, current version). "
             "If not given, will be automatically determined from current Blender version and index file.")
    parser_changelog.add_argument(
        "--filepath-out", dest="filepath_out", metavar='FILE', required=True,
        help="Output sphinx changelog RST file.")
    parser_changelog.set_defaults(func=api_changelog)

    args = parser.parse_args(argv)

    args.func(args)


if __name__ == "__main__":
    main()
