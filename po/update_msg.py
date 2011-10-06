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

CURRENT_DIR = os.path.abspath(os.path.dirname(__file__))
SOURCE_DIR = os.path.normpath(os.path.abspath(os.path.join(CURRENT_DIR, "..")))

FILE_NAME_MESSAGES = os.path.join(CURRENT_DIR, "messages.txt")
COMMENT_PREFIX = "#~ "


def dump_messages_rna(messages):
    import bpy

    # -------------------------------------------------------------------------
    # Function definitions

    def walkProperties(bl_rna):
        import bpy

        # get our parents properties not to export them multiple times
        bl_rna_base = bl_rna.base
        if bl_rna_base:
            bl_rna_base_props = bl_rna_base.properties.values()
        else:
            bl_rna_base_props = ()

        for prop in bl_rna.properties:
            # only write this property is our parent hasn't got it.
            if prop in bl_rna_base_props:
                continue
            if prop.identifier == "rna_type":
                continue

            msgsrc = "bpy.types.%s.%s" % (bl_rna.identifier, prop.identifier)
            messages.setdefault(prop.name, []).append(msgsrc)
            messages.setdefault(prop.description, []).append(msgsrc)

            if isinstance(prop, bpy.types.EnumProperty):
                for item in prop.enum_items:
                    msgsrc = "bpy.types.%s.%s, '%s'" % (bl_rna.identifier,
                                                        prop.identifier,
                                                        item.identifier,
                                                        )
                    messages.setdefault(item.name, []).append(msgsrc)
                    messages.setdefault(item.description, []).append(msgsrc)

    def walkRNA(bl_rna):
        msgsrc = "bpy.types.%s" % bl_rna.identifier

        if bl_rna.name and bl_rna.name != bl_rna.identifier:
            messages.setdefault(bl_rna.name, []).append(msgsrc)

        if bl_rna.description:
            messages.setdefault(bl_rna.description, []).append(msgsrc)

        if hasattr(bl_rna, 'bl_label') and  bl_rna.bl_label:
            messages.setdefault(bl_rna.bl_label, []).append(msgsrc)

        walkProperties(bl_rna)

    def walkClass(cls):
        walkRNA(cls.bl_rna)

    def walk_keymap_hierarchy(hier, msgsrc_prev):
        for lvl in hier:
            msgsrc = "%s.%s" % (msgsrc_prev, lvl[1])
            messages.setdefault(lvl[0], []).append(msgsrc)

            if lvl[3]:
                walk_keymap_hierarchy(lvl[3], msgsrc)

    # -------------------------------------------------------------------------
    # Dump Messages

    def full_class_id(cls):
        """ gives us 'ID.Lamp.AreaLamp' which is best for sorting.
        """
        cls_id = ""
        bl_rna = cls.bl_rna
        while bl_rna:
            cls_id = "%s.%s" % (bl_rna.identifier, cls_id)
            bl_rna = bl_rna.base
        return cls_id

    cls_list = type(bpy.context).__base__.__subclasses__()
    cls_list.sort(key=full_class_id)
    for cls in cls_list:
        walkClass(cls)

    cls_list = bpy.types.Space.__subclasses__()
    cls_list.sort(key=full_class_id)
    for cls in cls_list:
        walkClass(cls)

    cls_list = bpy.types.Operator.__subclasses__()
    cls_list.sort(key=full_class_id)
    for cls in cls_list:
        walkClass(cls)

    cls_list = bpy.types.OperatorProperties.__subclasses__()
    cls_list.sort(key=full_class_id)
    for cls in cls_list:
        walkClass(cls)

    cls_list = bpy.types.Menu.__subclasses__()
    cls_list.sort(key=full_class_id)
    for cls in cls_list:
        walkClass(cls)

    from bpy_extras.keyconfig_utils import KM_HIERARCHY

    walk_keymap_hierarchy(KM_HIERARCHY, "KM_HIERARCHY")


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

    def extract_strings(fp_rel, node_container):
        """ Recursively get strings, needed incase we have "Blah" + "Blah",
            passed as an argument in that case it wont evaluate to a string.
        """

        for node in ast.walk(node_container):
            if type(node) == ast.Str:
                eval_str = ast.literal_eval(node)
                if eval_str:
                    # print("%s:%d: %s" % (fp, node.lineno, eval_str))
                    msgsrc = "%s:%s" % (fp_rel, node.lineno)
                    messages.setdefault(eval_str, []).append(msgsrc)

    def extract_strings_from_file(fp):
        filedata = open(fp, 'r', encoding="utf8")
        root_node = ast.parse(filedata.read(), fp, 'exec')
        filedata.close()

        fp_rel = os.path.relpath(fp, SOURCE_DIR)

        for node in ast.walk(root_node):
            if type(node) == ast.Call:
                # print("found function at")
                # print("%s:%d" % (fp, node.lineno))

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
                        extract_strings(fp_rel, node.args[arg_pos])
                    else:
                        for kw in node.keywords:
                            if kw.arg == arg_kw:
                                extract_strings(fp_rel, kw.value)

    # -------------------------------------------------------------------------
    # Dump Messages

    mod_dir = os.path.join(SOURCE_DIR,
                           "release",
                           "scripts",
                           "startup",
                           "bl_ui")

    files = [os.path.join(mod_dir, fn)
             for fn in sorted(os.listdir(mod_dir))
             if not fn.startswith("_")
             if fn.endswith("py")
             ]

    for fp in files:
        extract_strings_from_file(fp)


def dump_messages():

    def filter_message(msg):

        # check for strings like ": %d"
        msg_test = msg
        for ignore in ("%d", "%s", "%r",  # string formatting
                       "*", ".", "(", ")", "-", "/", "\\", "+", ":", "#", "%"
                       "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                       "x",  # used on its own eg: 100x200
                       "X", "Y", "Z",  # used alone. no need to include
                       ):
            msg_test = msg_test.replace(ignore, "")
        msg_test = msg_test.strip()
        if not msg_test:
            # print("Skipping: '%s'" % msg)
            return True

        # we could filter out different strings here

        return False

    if 1:
        import collections
        messages = collections.OrderedDict()
    else:
        messages = {}

    messages[""] = []

    # get strings from RNA
    dump_messages_rna(messages)

    # get strings from UI layout definitions text="..." args
    dump_messages_pytext(messages)

    del messages[""]

    message_file = open(FILE_NAME_MESSAGES, 'w', encoding="utf8")
    # message_file.writelines("\n".join(sorted(messages)))

    for key, value in messages.items():

        # filter out junk values
        if filter_message(key):
            continue

        for msgsrc in value:
            message_file.write("%s%s\n" % (COMMENT_PREFIX, msgsrc))
        message_file.write("%s\n" % key)

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
