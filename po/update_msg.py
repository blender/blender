# $Id$
# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENSE BLOCK *****

# <pep8 compliant>

# Write out messages.txt from blender

# Execite:
#   blender --background --python po/update_msg.py

import os

CURRENT_DIR = os.path.dirname(__file__)
SOURCE_DIR = os.path.normpath(os.path.abspath(os.path.join(CURRENT_DIR, "..")))

FILE_NAME_MESSAGES = os.path.join(CURRENT_DIR, "messages.txt")


def dump_messages_rna(messages):
    import bpy

    # -------------------------------------------------------------------------
    # Function definitions

    def walkProperties(properties):
        import bpy
        for prop in properties:
            messages.add(prop.name)
            messages.add(prop.description)

            if isinstance(prop, bpy.types.EnumProperty):
                for item in prop.enum_items:
                    messages.add(item.name)
                    messages.add(item.description)

    def walkRNA(bl_rna):
        if bl_rna.name and bl_rna.name != bl_rna.identifier:
            messages.add(bl_rna.name)

        if bl_rna.description:
            messages.add(bl_rna.description)

        walkProperties(bl_rna.properties)

    def walkClass(cls):
        walkRNA(cls.bl_rna)

    def walk_keymap_hierarchy(hier):
        for lvl in hier:
            messages.add(lvl[0])

            if lvl[3]:
                walk_keymap_hierarchy(lvl[3])

    # -------------------------------------------------------------------------
    # Dump Messages

    for cls in type(bpy.context).__base__.__subclasses__():
        walkClass(cls)

    for cls in bpy.types.Space.__subclasses__():
        walkClass(cls)

    for cls in bpy.types.Operator.__subclasses__():
        walkClass(cls)

    from bpy_extras.keyconfig_utils import KM_HIERARCHY

    walk_keymap_hierarchy(KM_HIERARCHY)


    ## XXX. what is this supposed to do, we wrote the file already???
    #_walkClass(bpy.types.SpaceDopeSheetEditor)


def dump_messages_pytext(messages):
    """ dumps text inlined in the python user interface: eg.

        layout.prop("someprop", text="My Name")
    """
    import ast

    # -------------------------------------------------------------------------
    # Gather function names

    import bpy
    # key: func_id
    # val: [(arg_kw, arg_pos), (arg_kw, arg_pos), ...]
    func_translate_args = {}

    # so far only 'text' keywords, but we may want others translated later
    translate_kw = ("text", )

    for func_id, func in bpy.types.UILayout.bl_rna.functions.items():
        # check it has a 'text' argument
        for (arg_pos, (arg_kw, arg)) in enumerate(func.parameters.items()):
            if ((arg_kw in translate_kw) and
                (arg.is_output == False) and
                (arg.type == 'STRING')):

                func_translate_args.setdefault(func_id, []).append((arg_kw,
                                                                    arg_pos))
    # print(func_translate_args)

    # -------------------------------------------------------------------------
    # Function definitions

    def extract_strings(fp, node_container):
        """ Recursively get strings, needed incase we have "Blah" + "Blah",
            passed as an argument in that case it wont evaluate to a string.
        """
        for node in ast.walk(node_container):
            if type(node) == ast.Str:
                eval_str = ast.literal_eval(node)
                if eval_str:
                    # print("%s:%d: %s" % (fp, node.lineno, eval_str))  # testing
                    messages.add(eval_str)

    def extract_strings_from_file(fn):
        filedata = open(fn, 'r', encoding="utf8")
        root_node = ast.parse(filedata.read(), fn, 'exec')
        filedata.close()

        for node in ast.walk(root_node):
            if type(node) == ast.Call:
                # print("found function at")
                # print("%s:%d" % (fn, node.lineno))

                # lambda's
                if type(node.func) == ast.Name:
                    continue

                # getattr(self, con.type)(context, box, con)
                if not hasattr(node.func, "attr"):
                    continue

                translate_args = func_translate_args.get(node.func.attr, ())

                # do nothing if not found
                for arg_kw, arg_pos in translate_args:
                    if arg_pos < len(node.args):
                        extract_strings(fn, node.args[arg_pos])
                    else:
                        for kw in node.keywords:
                            if kw.arg == arg_kw:
                                extract_strings(fn, kw.value)

    # -------------------------------------------------------------------------
    # Dump Messages

    mod_dir = os.path.join(SOURCE_DIR, "release", "scripts", "startup", "bl_ui")

    files = [os.path.join(mod_dir, f)
             for f in os.listdir(mod_dir)
             if not f.startswith("_")
             if f.endswith("py")
             ]

    for fn in files:
        extract_strings_from_file(fn)


def dump_messages():
    messages = {""}

    # get strings from RNA
    dump_messages_rna(messages)

    # get strings from UI layout definitions text="..." args
    dump_messages_pytext(messages)

    messages.remove("")

    message_file = open(FILE_NAME_MESSAGES, 'w', encoding="utf8")
    message_file.writelines("\n".join(sorted(messages)))
    message_file.close()

    print("Written %d messages to: %r" % (len(messages), FILE_NAME_MESSAGES))


def main():

    try:
        import bpy
    except ImportError:
        print("This script must run from inside blender")
        return

    dump_messages()


if __name__ == "__main__":
    print("\n\n *** Running %r *** \n" % __file__)
    main()
