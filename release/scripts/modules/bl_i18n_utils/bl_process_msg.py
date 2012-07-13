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

# <pep8-80 compliant>

# Write out messages.txt from Blender.
# XXX: This script is meant to be used from inside Blender!
#      You should not directly use this script, rather use update_msg.py!

import os

# XXX Relative import does not work here when used from Blender...
from bl_i18n_utils import settings


#classes = set()


SOURCE_DIR = settings.SOURCE_DIR

CUSTOM_PY_UI_FILES = [os.path.abspath(os.path.join(SOURCE_DIR, p))
                      for p in settings.CUSTOM_PY_UI_FILES]
FILE_NAME_MESSAGES = settings.FILE_NAME_MESSAGES
COMMENT_PREFIX = settings.COMMENT_PREFIX
CONTEXT_PREFIX = settings.CONTEXT_PREFIX
CONTEXT_DEFAULT = settings.CONTEXT_DEFAULT
UNDOC_OPS_STR = settings.UNDOC_OPS_STR

NC_ALLOWED = settings.WARN_MSGID_NOT_CAPITALIZED_ALLOWED

def check(check_ctxt, messages, key, msgsrc):
    if check_ctxt is None:
        return
    multi_rnatip = check_ctxt.get("multi_rnatip")
    multi_lines = check_ctxt.get("multi_lines")
    py_in_rna = check_ctxt.get("py_in_rna")
    not_capitalized = check_ctxt.get("not_capitalized")
    end_point = check_ctxt.get("end_point")
    undoc_ops = check_ctxt.get("undoc_ops")

    if multi_rnatip is not None:
        if key in messages and key not in multi_rnatip:
            multi_rnatip.add(key)
    if multi_lines is not None:
        if '\n' in key[1]:
            multi_lines.add(key)
    if py_in_rna is not None:
        if key in py_in_rna[1]:
            py_in_rna[0].add(key)
    if not_capitalized is not None:
        if(key[1] not in NC_ALLOWED and key[1][0].isalpha() and
           not key[1][0].isupper()):
            not_capitalized.add(key)
    if end_point is not None:
        if key[1].strip().endswith('.'):
            end_point.add(key)
    if undoc_ops is not None:
        if key[1] == UNDOC_OPS_STR:
            undoc_ops.add(key)


def dump_messages_rna(messages, check_ctxt):
    import bpy

    def classBlackList():
        blacklist_rna_class = [# core classes
                               "Context", "Event", "Function", "UILayout",
                               "BlendData",
                               # registerable classes
                               "Panel", "Menu", "Header", "RenderEngine",
                               "Operator", "OperatorMacro", "Macro",
                               "KeyingSetInfo", "UnknownType",
                               # window classes
                               "Window",
                               ]

        # ---------------------------------------------------------------------
        # Collect internal operators

        # extend with all internal operators
        # note that this uses internal api introspection functions
        # all possible operator names
        op_ids = set(cls.bl_rna.identifier for cls in
                     bpy.types.OperatorProperties.__subclasses__()) | \
                 set(cls.bl_rna.identifier for cls in
                     bpy.types.Operator.__subclasses__()) | \
                 set(cls.bl_rna.identifier for cls in
                     bpy.types.OperatorMacro.__subclasses__())

        get_instance = __import__("_bpy").ops.get_instance
        path_resolve = type(bpy.context).__base__.path_resolve
        for idname in op_ids:
            op = get_instance(idname)
            # XXX Do not skip INTERNAL's anymore, some of those ops
            #     show up in UI now!
#            if 'INTERNAL' in path_resolve(op, "bl_options"):
#                blacklist_rna_class.append(idname)

        # ---------------------------------------------------------------------
        # Collect builtin classes we don't need to doc
        blacklist_rna_class.append("Property")
        blacklist_rna_class.extend(
                [cls.__name__ for cls in
                 bpy.types.Property.__subclasses__()])

        # ---------------------------------------------------------------------
        # Collect classes which are attached to collections, these are api
        # access only.
        collection_props = set()
        for cls_id in dir(bpy.types):
            cls = getattr(bpy.types, cls_id)
            for prop in cls.bl_rna.properties:
                if prop.type == 'COLLECTION':
                    prop_cls = prop.srna
                    if prop_cls is not None:
                        collection_props.add(prop_cls.identifier)
        blacklist_rna_class.extend(sorted(collection_props))

        return blacklist_rna_class

    blacklist_rna_class = classBlackList()

    def filterRNA(bl_rna):
        rid = bl_rna.identifier
        if rid in blacklist_rna_class:
            print("  skipping", rid)
            return True
        return False

    check_ctxt_rna = check_ctxt_rna_tip = None
    if check_ctxt:
        check_ctxt_rna = {"multi_lines": check_ctxt.get("multi_lines"),
                          "not_capitalized": check_ctxt.get("not_capitalized"),
                          "end_point": check_ctxt.get("end_point"),
                          "undoc_ops": check_ctxt.get("undoc_ops")}
        check_ctxt_rna_tip = check_ctxt_rna
        check_ctxt_rna_tip["multi_rnatip"] = check_ctxt.get("multi_rnatip")

    # -------------------------------------------------------------------------
    # Function definitions

    def walkProperties(bl_rna):
        import bpy

        # Get our parents' properties, to not export them multiple times.
        bl_rna_base = bl_rna.base
        if bl_rna_base:
            bl_rna_base_props = bl_rna_base.properties.values()
        else:
            bl_rna_base_props = ()

        for prop in bl_rna.properties:
            # Only write this property if our parent hasn't got it.
            if prop in bl_rna_base_props:
                continue
            if prop.identifier == "rna_type":
                continue

            msgsrc = "bpy.types.{}.{}".format(bl_rna.identifier, prop.identifier)
            context = getattr(prop, "translation_context", CONTEXT_DEFAULT)
            if prop.name and (prop.name != prop.identifier or context):
                key = (context, prop.name)
                check(check_ctxt_rna, messages, key, msgsrc)
                messages.setdefault(key, []).append(msgsrc)
            if prop.description:
                key = (CONTEXT_DEFAULT, prop.description)
                check(check_ctxt_rna_tip, messages, key, msgsrc)
                messages.setdefault(key, []).append(msgsrc)
            if isinstance(prop, bpy.types.EnumProperty):
                for item in prop.enum_items:
                    msgsrc = "bpy.types.{}.{}:'{}'".format(bl_rna.identifier,
                                                            prop.identifier,
                                                            item.identifier)
                    if item.name and item.name != item.identifier:
                        key = (CONTEXT_DEFAULT, item.name)
                        check(check_ctxt_rna, messages, key, msgsrc)
                        messages.setdefault(key, []).append(msgsrc)
                    if item.description:
                        key = (CONTEXT_DEFAULT, item.description)
                        check(check_ctxt_rna_tip, messages, key, msgsrc)
                        messages.setdefault(key, []).append(msgsrc)

    def walkRNA(bl_rna):
        if filterRNA(bl_rna):
            return

        msgsrc = ".".join(("bpy.types", bl_rna.identifier))
        context = getattr(bl_rna, "translation_context", CONTEXT_DEFAULT)

        if bl_rna.name and (bl_rna.name != bl_rna.identifier or context):
            key = (context, bl_rna.name)
            check(check_ctxt_rna, messages, key, msgsrc)
            messages.setdefault(key, []).append(msgsrc)

        if bl_rna.description:
            key = (CONTEXT_DEFAULT, bl_rna.description)
            check(check_ctxt_rna_tip, messages, key, msgsrc)
            messages.setdefault(key, []).append(msgsrc)

        if hasattr(bl_rna, 'bl_label') and  bl_rna.bl_label:
            key = (context, bl_rna.bl_label)
            check(check_ctxt_rna, messages, key, msgsrc)
            messages.setdefault(key, []).append(msgsrc)

        walkProperties(bl_rna)

    def walkClass(cls):
        walkRNA(cls.bl_rna)

    def walk_keymap_hierarchy(hier, msgsrc_prev):
        for lvl in hier:
            msgsrc = "{}.{}".format(msgsrc_prev, lvl[1])
            messages.setdefault((CONTEXT_DEFAULT, lvl[0]), []).append(msgsrc)

            if lvl[3]:
                walk_keymap_hierarchy(lvl[3], msgsrc)

    # -------------------------------------------------------------------------
    # Dump Messages

    def process_cls_list(cls_list):
        if not cls_list:
            return 0

        def full_class_id(cls):
            """ gives us 'ID.Lamp.AreaLamp' which is best for sorting.
            """
            cls_id = ""
            bl_rna = cls.bl_rna
            while bl_rna:
                cls_id = "{}.{}".format(bl_rna.identifier, cls_id)
                bl_rna = bl_rna.base
            return cls_id

        cls_list.sort(key=full_class_id)
        processed = 0
        for cls in cls_list:
            walkClass(cls)
#            classes.add(cls)
            # Recursively process subclasses.
            processed += process_cls_list(cls.__subclasses__()) + 1
        return processed

    # Parse everything (recursively parsing from bpy_struct "class"...).
    processed = process_cls_list(type(bpy.context).__base__.__subclasses__())
    print("{} classes processed!".format(processed))
#    import pickle
#    global classes
#    classes = {str(c) for c in classes}
#    with open("/home/i7deb64/Bureau/tpck_2", "wb") as f:
#        pickle.dump(classes, f, protocol=0)

    from bpy_extras.keyconfig_utils import KM_HIERARCHY

    walk_keymap_hierarchy(KM_HIERARCHY, "KM_HIERARCHY")



def dump_messages_pytext(messages, check_ctxt):
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

    # Break recursive nodes look up on some kind of nodes.
    # E.g. we don’t want to get strings inside subscripts (blah["foo"])!
    stopper_nodes = {ast.Subscript,}

    for func_id, func in bpy.types.UILayout.bl_rna.functions.items():
        # check it has a 'text' argument
        for (arg_pos, (arg_kw, arg)) in enumerate(func.parameters.items()):
            if ((arg_kw in translate_kw) and
                (arg.is_output == False) and
                (arg.type == 'STRING')):

                func_translate_args.setdefault(func_id, []).append((arg_kw,
                                                                    arg_pos))
    # print(func_translate_args)

    check_ctxt_py = None
    if check_ctxt:
        check_ctxt_py = {"py_in_rna": (check_ctxt["py_in_rna"], messages.copy()),
                         "multi_lines": check_ctxt["multi_lines"],
                         "not_capitalized": check_ctxt["not_capitalized"],
                         "end_point": check_ctxt["end_point"]}

    # -------------------------------------------------------------------------
    # Function definitions

    def extract_strings(fp_rel, node):
        """ Recursively get strings, needed in case we have "Blah" + "Blah",
            passed as an argument in that case it wont evaluate to a string.
            However, break on some kind of stopper nodes, like e.g. Subscript.
        """

        if type(node) == ast.Str:
            eval_str = ast.literal_eval(node)
            if eval_str:
                key = (CONTEXT_DEFAULT, eval_str)
                msgsrc = "{}:{}".format(fp_rel, node.lineno)
                check(check_ctxt_py, messages, key, msgsrc)
                messages.setdefault(key, []).append(msgsrc)
            return

        for nd in ast.iter_child_nodes(node):
            if type(nd) not in stopper_nodes:
                extract_strings(fp_rel, nd)

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

    # Dummy Cycles has its py addon in its own dir!
    files += CUSTOM_PY_UI_FILES

    for fp in files:
        extract_strings_from_file(fp)


def dump_messages(do_messages, do_checks):
    import collections

    def enable_addons():
        """For now, enable all official addons, before extracting msgids."""
        import addon_utils
        import bpy

        userpref = bpy.context.user_preferences
        used_ext = {ext.module for ext in userpref.addons}
        support = {"OFFICIAL"}
        # collect the categories that can be filtered on
        addons = [(mod, addon_utils.module_bl_info(mod)) for mod in
                  addon_utils.modules(addon_utils.addons_fake_modules)]

        for mod, info in addons:
            module_name = mod.__name__
            if module_name in used_ext or info["support"] not in support:
                continue
            print("    Enabling module ", module_name)
            bpy.ops.wm.addon_enable(module=module_name)

        # XXX There are currently some problems with bpy/rna...
        #     *Very* tricky to solve!
        #     So this is a hack to make all newly added operator visible by
        #     bpy.types.OperatorProperties.__subclasses__()
        for cat in dir(bpy.ops):
            cat = getattr(bpy.ops, cat)
            for op in dir(cat):
                getattr(cat, op).get_rna()

    # check for strings like ": %d"
    ignore = ("%d", "%f", "%s", "%r",  # string formatting
              "*", ".", "(", ")", "-", "/", "\\", "+", ":", "#", "%"
              "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
              "x",  # used on its own eg: 100x200
              "X", "Y", "Z", "W",  # used alone. no need to include
              )

    def filter_message(msg):
        msg_tmp = msg
        for ign in ignore:
            msg_tmp = msg_tmp.replace(ign, "")
        if not msg_tmp.strip():
            return True
        # we could filter out different strings here
        return False

    messages = getattr(collections, 'OrderedDict', dict)()

    messages[(CONTEXT_DEFAULT, "")] = []

    # Enable all wanted addons.
    enable_addons()

    check_ctxt = None
    if do_checks:
        check_ctxt = {"multi_rnatip": set(),
                      "multi_lines": set(),
                      "py_in_rna": set(),
                      "not_capitalized": set(),
                      "end_point": set(),
                      "undoc_ops": set()}

    # get strings from RNA
    dump_messages_rna(messages, check_ctxt)

    # get strings from UI layout definitions text="..." args
    dump_messages_pytext(messages, check_ctxt)

    del messages[(CONTEXT_DEFAULT, "")]

    if do_checks:
        print("WARNINGS:")
        keys = set()
        for c in check_ctxt.values():
            keys |= c
        # XXX Temp, see below
        keys -= check_ctxt["multi_rnatip"]
        for key in keys:
            if key in check_ctxt["undoc_ops"]:
                print("\tThe following operators are undocumented:")
            else:
                print("\t“{}”|“{}”:".format(*key))
                if key in check_ctxt["multi_lines"]:
                    print("\t\t-> newline in this message!")
                if key in check_ctxt["not_capitalized"]:
                    print("\t\t-> message not capitalized!")
                if key in check_ctxt["end_point"]:
                    print("\t\t-> message with endpoint!")
                # XXX Hide this one for now, too much false positives.
#                if key in check_ctxt["multi_rnatip"]:
#                    print("\t\t-> tip used in several RNA items")
                if key in check_ctxt["py_in_rna"]:
                    print("\t\t-> RNA message also used in py UI code:")
            print("\t\t{}".format("\n\t\t".join(messages[key])))

    if do_messages:
        print("Writing messages…")
        num_written = 0
        num_filtered = 0
        with open(FILE_NAME_MESSAGES, 'w', encoding="utf8") as message_file:
            for (ctx, key), value in messages.items():
                # filter out junk values
                if filter_message(key):
                    num_filtered += 1
                    continue

                # Remove newlines in key and values!
                message_file.write("\n".join(COMMENT_PREFIX + msgsrc.replace("\n", "") for msgsrc in value))
                message_file.write("\n")
                if ctx:
                    message_file.write(CONTEXT_PREFIX + ctx.replace("\n", "") + "\n")
                message_file.write(key.replace("\n", "") + "\n")
                num_written += 1

        print("Written {} messages to: {} ({} were filtered out)." \
              "".format(num_written, FILE_NAME_MESSAGES, num_filtered))


def main():
    try:
        import bpy
    except ImportError:
        print("This script must run from inside blender")
        return

    import sys
    back_argv = sys.argv
    sys.argv = sys.argv[sys.argv.index("--") + 1:]

    import argparse
    parser = argparse.ArgumentParser(description="Process UI messages " \
                                                 "from inside Blender.")
    parser.add_argument('-c', '--no_checks', default=True,
                        action="store_false",
                        help="No checks over UI messages.")
    parser.add_argument('-m', '--no_messages', default=True,
                        action="store_false",
                        help="No export of UI messages.")
    parser.add_argument('-o', '--output', help="Output messages file path.")
    args = parser.parse_args()

    if args.output:
        global FILE_NAME_MESSAGES
        FILE_NAME_MESSAGES = args.output

    dump_messages(do_messages=args.no_messages, do_checks=args.no_checks)

    sys.argv = back_argv


if __name__ == "__main__":
    print("\n\n *** Running {} *** \n".format(__file__))
    main()
