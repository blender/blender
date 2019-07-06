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

"""
Dump the python API into a text file so we can generate changelogs.

output from this tool should be added into "doc/python_api/rst/change_log.rst"

# dump api blender_version.py in CWD
blender --background --python doc/python_api/sphinx_changelog_gen.py -- --dump

# create changelog
blender --background --factory-startup --python doc/python_api/sphinx_changelog_gen.py -- \
        --api_from blender_2_63_0.py \
        --api_to   blender_2_64_0.py \
        --api_out changes.rst


# Api comparison can also run without blender
python doc/python_api/sphinx_changelog_gen.py -- \
        --api_from blender_api_2_63_0.py \
        --api_to   blender_api_2_64_0.py \
        --api_out changes.rst

# Save the latest API dump in this folder, renaming it with its revision.
# This way the next person updating it doesn't need to build an old Blender only for that

"""

# format
'''
{"module.name":
    {"parent.class":
        {"basic_type", "member_name":
            ("Name", type, range, length, default, descr, f_args, f_arg_types, f_ret_types)}, ...
    }, ...
}
'''

api_names = "basic_type" "name", "type", "range", "length", "default", "descr", "f_args", "f_arg_types", "f_ret_types"

API_BASIC_TYPE = 0
API_F_ARGS = 7


def api_dunp_fname():
    import bpy
    return "blender_api_%s.py" % "_".join([str(i) for i in bpy.app.version])


def api_dump():
    dump = {}
    dump_module = dump["bpy.types"] = {}

    import rna_info
    import inspect

    struct = rna_info.BuildRNAInfo()[0]
    for struct_id, struct_info in sorted(struct.items()):

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

        # python props, tricky since we don't know much about them.
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

    import pprint

    filename = api_dunp_fname()
    filehandle = open(filename, 'w', encoding='utf-8')
    tot = filehandle.write(pprint.pformat(dump, width=1))
    filehandle.close()
    print("%s, %d bytes written" % (filename, tot))


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


def api_changelog(api_from, api_to, api_out):

    file_handle = open(api_from, 'r', encoding='utf-8')
    dict_from = eval(file_handle.read())
    file_handle.close()

    file_handle = open(api_to, 'r', encoding='utf-8')
    dict_to = eval(file_handle.read())
    file_handle.close()

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
                        if prop_id_old in set_props_old:
                            set_props_old.remove(prop_id_old)
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

    fout = open(api_out, 'w', encoding='utf-8')
    fw = fout.write
    # print(api_changes)

    # :class:`bpy_struct.id_data`

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

    fout.close()

    print("Written: %r" % api_out)


def main():
    import sys
    import os

    try:
        import argparse
    except ImportError:
        print("Old Blender, just dumping")
        api_dump()
        return

    argv = sys.argv

    if "--" not in argv:
        argv = []  # as if no args are passed
    else:
        argv = argv[argv.index("--") + 1:]  # get all args after "--"

    # When --help or no args are given, print this help
    usage_text = "Run blender in background mode with this script: "
    "blender --background --factory-startup --python %s -- [options]" % os.path.basename(__file__)

    epilog = "Run this before releases"

    parser = argparse.ArgumentParser(description=usage_text, epilog=epilog)

    parser.add_argument(
        "--dump", dest="dump", action='store_true',
        help="When set the api will be dumped into blender_version.py")

    parser.add_argument(
        "--api_from", dest="api_from", metavar='FILE',
        help="File to compare from (previous version)")
    parser.add_argument(
        "--api_to", dest="api_to", metavar='FILE',
        help="File to compare from (current)")
    parser.add_argument(
        "--api_out", dest="api_out", metavar='FILE',
        help="Output sphinx changelog")

    args = parser.parse_args(argv)  # In this example we won't use the args

    if not argv:
        print("No args given!")
        parser.print_help()
        return

    if args.dump:
        api_dump()
    else:
        if args.api_from and args.api_to and args.api_out:
            api_changelog(args.api_from, args.api_to, args.api_out)
        else:
            print("Error: --api_from/api_to/api_out args needed")
            parser.print_help()
            return

    print("batch job finished, exiting")


if __name__ == "__main__":
    main()
