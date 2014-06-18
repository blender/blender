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

# Used for generating API diffs between releases
#  ./blender.bin --background -noaudio --python tests/python/rna_info_dump.py

import bpy


def api_dump(use_properties=True, use_functions=True):

    def prop_type(prop):
        if prop.type == "pointer":
            return prop.fixed_type.identifier
        else:
            return prop.type

    def func_to_str(struct_id_str, func_id, func):

        args = []
        for prop in func.args:
            data_str = "%s %s" % (prop_type(prop), prop.identifier)
            if prop.array_length:
                data_str += "[%d]" % prop.array_length
            if not prop.is_required:
                data_str += "=%s" % prop.default_str
            args.append(data_str)

        data_str = "%s.%s(%s)" % (struct_id_str, func_id, ", ".join(args))
        if func.return_values:
            return_args = ", ".join(prop_type(arg) for arg in func.return_values)
            if len(func.return_values) > 1:
                data_str += "  -->  (%s)" % return_args
            else:
                data_str += "  -->  %s" % return_args
        return data_str

    def prop_to_str(struct_id_str, prop_id, prop):

        prop_str = "  <--  %s" % prop_type(prop)
        if prop.array_length:
            prop_str += "[%d]" % prop.array_length

        data_str = "%s.%s %s" % (struct_id_str, prop_id, prop_str)
        return data_str

    def struct_full_id(v):
        struct_id_str = v.identifier  # "".join(sid for sid in struct_id if struct_id)

        for base in v.get_bases():
            struct_id_str = base.identifier + "|" + struct_id_str

        return struct_id_str

    def dump_funcs():
        data = []
        for struct_id, v in sorted(struct.items()):
            struct_id_str = struct_full_id(v)

            funcs = [(func.identifier, func) for func in v.functions]

            for func_id, func in funcs:
                data.append(func_to_str(struct_id_str, func_id, func))

            for prop in v.properties:
                if prop.collection_type:
                    funcs = [(prop.identifier + "." + func.identifier, func) for func in prop.collection_type.functions]
                    for func_id, func in funcs:
                        data.append(func_to_str(struct_id_str, func_id, func))
        data.sort()
        data.append("# * functions *")
        return data

    def dump_props():
        data = []
        for struct_id, v in sorted(struct.items()):
            struct_id_str = struct_full_id(v)

            props = [(prop.identifier, prop) for prop in v.properties]

            for prop_id, prop in props:
                data.append(prop_to_str(struct_id_str, prop_id, prop))

            for prop in v.properties:
                if prop.collection_type:
                    props = [(prop.identifier + "." + prop_sub.identifier, prop_sub) for prop_sub in prop.collection_type.properties]
                    for prop_sub_id, prop_sub in props:
                        data.append(prop_to_str(struct_id_str, prop_sub_id, prop_sub))
        data.sort()
        data.insert(0, "# * properties *")
        return data

    import rna_info
    struct = rna_info.BuildRNAInfo()[0]
    data = []

    if use_functions:
        data.extend(dump_funcs())

    if use_properties:
        data.extend(dump_props())

    if bpy.app.background:
        import sys
        sys.stderr.write("\n".join(data))
        sys.stderr.write("\n\nEOF\n")
    else:
        text = bpy.data.texts.new(name="api.py")
        text.from_string(data)

    print("END")

if __name__ == "__main__":
    api_dump()
