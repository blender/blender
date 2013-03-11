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

# Populate a template file (POT format currently) from Blender RNA/py/C data.
# XXX: This script is meant to be used from inside Blender!
#      You should not directly use this script, rather use update_msg.py!

import collections
import copy
import datetime
import os
import re
import sys

# XXX Relative import does not work here when used from Blender...
from bl_i18n_utils import settings as i18n_settings, utils

import bpy

##### Utils #####

# check for strings like "+%f°"
ignore_reg = re.compile(r"^(?:[-*.()/\\+%°0-9]|%d|%f|%s|%r|\s)*$")
filter_message = ignore_reg.match


def init_spell_check(settings, lang="en_US"):
    try:
        from bl_i18n_utils import spell_check_utils
        return spell_check_utils.SpellChecker(settings, lang)
    except Exception as e:
        print("Failed to import spell_check_utils ({})".format(str(e)))
        return None


def _gen_check_ctxt(settings):
    return {
        "multi_rnatip": set(),
        "multi_lines": set(),
        "py_in_rna": set(),
        "not_capitalized": set(),
        "end_point": set(),
        "undoc_ops": set(),
        "spell_checker": init_spell_check(settings),
        "spell_errors": {},
    }


def _gen_reports(check_ctxt):
    return {
        "check_ctxt": check_ctxt,
        "rna_structs": [],
        "rna_structs_skipped": [],
        "rna_props": [],
        "rna_props_skipped": [],
        "py_messages": [],
        "py_messages_skipped": [],
        "src_messages": [],
        "src_messages_skipped": [],
        "messages_skipped": set(),
    }


def check(check_ctxt, msgs, key, msgsrc, settings):
    """
    Performs a set of checks over the given key (context, message)...
    """
    if check_ctxt is None:
        return
    multi_rnatip = check_ctxt.get("multi_rnatip")
    multi_lines = check_ctxt.get("multi_lines")
    py_in_rna = check_ctxt.get("py_in_rna")
    not_capitalized = check_ctxt.get("not_capitalized")
    end_point = check_ctxt.get("end_point")
    undoc_ops = check_ctxt.get("undoc_ops")
    spell_checker = check_ctxt.get("spell_checker")
    spell_errors = check_ctxt.get("spell_errors")

    if multi_rnatip is not None:
        if key in msgs and key not in multi_rnatip:
            multi_rnatip.add(key)
    if multi_lines is not None:
        if '\n' in key[1]:
            multi_lines.add(key)
    if py_in_rna is not None:
        if key in py_in_rna[1]:
            py_in_rna[0].add(key)
    if not_capitalized is not None:
        if(key[1] not in settings.WARN_MSGID_NOT_CAPITALIZED_ALLOWED and
           key[1][0].isalpha() and not key[1][0].isupper()):
            not_capitalized.add(key)
    if end_point is not None:
        if (key[1].strip().endswith('.') and not key[1].strip().endswith('...') and
            key[1] not in settings.WARN_MSGID_END_POINT_ALLOWED):
            end_point.add(key)
    if undoc_ops is not None:
        if key[1] == settings.UNDOC_OPS_STR:
            undoc_ops.add(key)
    if spell_checker is not None and spell_errors is not None:
        err = spell_checker.check(key[1])
        if err:
            spell_errors[key] = err


def print_info(reports, pot):
    def _print(*args, **kwargs):
        kwargs["file"] = sys.stderr
        print(*args, **kwargs)

    pot.update_info()

    _print("{} RNA structs were processed (among which {} were skipped), containing {} RNA properties "
           "(among which {} were skipped).".format(len(reports["rna_structs"]), len(reports["rna_structs_skipped"]),
                                                   len(reports["rna_props"]), len(reports["rna_props_skipped"])))
    _print("{} messages were extracted from Python UI code (among which {} were skipped), and {} from C source code "
           "(among which {} were skipped).".format(len(reports["py_messages"]), len(reports["py_messages_skipped"]),
                                                   len(reports["src_messages"]), len(reports["src_messages_skipped"])))
    _print("{} messages were rejected.".format(len(reports["messages_skipped"])))
    _print("\n")
    _print("Current POT stats:")
    pot.print_stats(prefix="\t", output=_print)
    _print("\n")

    check_ctxt = reports["check_ctxt"]
    if check_ctxt is None:
        return
    multi_rnatip = check_ctxt.get("multi_rnatip")
    multi_lines = check_ctxt.get("multi_lines")
    py_in_rna = check_ctxt.get("py_in_rna")
    not_capitalized = check_ctxt.get("not_capitalized")
    end_point = check_ctxt.get("end_point")
    undoc_ops = check_ctxt.get("undoc_ops")
    spell_errors = check_ctxt.get("spell_errors")

    # XXX Temp, no multi_rnatip nor py_in_rna, see below.
    keys = multi_lines | not_capitalized | end_point | undoc_ops | spell_errors.keys()
    if keys:
        _print("WARNINGS:")
        for key in keys:
            if undoc_ops and key in undoc_ops:
                _print("\tThe following operators are undocumented!")
            else:
                _print("\t“{}”|“{}”:".format(*key))
                if multi_lines and key in multi_lines:
                    _print("\t\t-> newline in this message!")
                if not_capitalized and key in not_capitalized:
                    _print("\t\t-> message not capitalized!")
                if end_point and key in end_point:
                    _print("\t\t-> message with endpoint!")
                # XXX Hide this one for now, too much false positives.
#                if multi_rnatip and key in multi_rnatip:
#                    _print("\t\t-> tip used in several RNA items")
#                if py_in_rna and key in py_in_rna:
#                    _print("\t\t-> RNA message also used in py UI code!")
                if spell_errors and spell_errors.get(key):
                    lines = ["\t\t-> {}: misspelled, suggestions are ({})".format(w, "'" + "', '".join(errs) + "'")
                             for w, errs in  spell_errors[key]]
                    _print("\n".join(lines))
            _print("\t\t{}".format("\n\t\t".join(pot.msgs[key].sources)))


def enable_addons(addons={}, support={}, disable=False):
    """
    Enable (or disable) addons based either on a set of names, or a set of 'support' types.
    Returns the list of all affected addons (as fake modules)!
    """
    import addon_utils

    userpref = bpy.context.user_preferences
    used_ext = {ext.module for ext in userpref.addons}

    ret = [mod for mod in addon_utils.modules(addon_utils.addons_fake_modules)
               if ((addons and mod.__name__ in addons) or
                   (not addons and addon_utils.module_bl_info(mod)["support"] in support))]

    for mod in ret:
        module_name = mod.__name__
        if disable:
            if module_name not in used_ext:
                continue
            print("    Disabling module ", module_name)
            bpy.ops.wm.addon_disable(module=module_name)
        else:
            if module_name in used_ext:
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

    return ret


def process_msg(msgs, msgctxt, msgid, msgsrc, reports, check_ctxt, settings):
    if filter_message(msgid):
        reports["messages_skipped"].add((msgid, msgsrc))
        return
    if not msgctxt:
        # We do *not* want any "" context!
        msgctxt = settings.DEFAULT_CONTEXT
    # Always unescape keys!
    msgctxt = utils.I18nMessage.do_unescape(msgctxt)
    msgid = utils.I18nMessage.do_unescape(msgid)
    key = (msgctxt, msgid)
    check(check_ctxt, msgs, key, msgsrc, settings)
    msgsrc = settings.PO_COMMENT_PREFIX_SOURCE_CUSTOM + msgsrc
    if key not in msgs:
        msgs[key] = utils.I18nMessage([msgctxt], [msgid], [], [msgsrc], settings=settings)
    else:
        msgs[key].comment_lines.append(msgsrc)


##### RNA #####
def dump_messages_rna(msgs, reports, settings):
    """
    Dump into messages dict all RNA-defined UI messages (labels en tooltips).
    """
    def class_blacklist():
        blacklist_rna_class = [
            # core classes
            "Context", "Event", "Function", "UILayout", "UnknownType",
            # registerable classes
            "Panel", "Menu", "Header", "RenderEngine", "Operator", "OperatorMacro", "Macro", "KeyingSetInfo",
            # window classes
            "Window",
        ]

        # Collect internal operators
        # extend with all internal operators
        # note that this uses internal api introspection functions
        # all possible operator names
        op_ids = set(cls.bl_rna.identifier for cls in bpy.types.OperatorProperties.__subclasses__()) | \
                 set(cls.bl_rna.identifier for cls in bpy.types.Operator.__subclasses__()) | \
                 set(cls.bl_rna.identifier for cls in bpy.types.OperatorMacro.__subclasses__())

        get_instance = __import__("_bpy").ops.get_instance
#        path_resolve = type(bpy.context).__base__.path_resolve
        for idname in op_ids:
            op = get_instance(idname)
            # XXX Do not skip INTERNAL's anymore, some of those ops show up in UI now!
#            if 'INTERNAL' in path_resolve(op, "bl_options"):
#                blacklist_rna_class.append(idname)

        # Collect builtin classes we don't need to doc
        blacklist_rna_class.append("Property")
        blacklist_rna_class.extend([cls.__name__ for cls in bpy.types.Property.__subclasses__()])

        # Collect classes which are attached to collections, these are api access only.
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

    check_ctxt_rna = check_ctxt_rna_tip = None
    check_ctxt = reports["check_ctxt"]
    if check_ctxt:
        check_ctxt_rna = {
            "multi_lines": check_ctxt.get("multi_lines"),
            "not_capitalized": check_ctxt.get("not_capitalized"),
            "end_point": check_ctxt.get("end_point"),
            "undoc_ops": check_ctxt.get("undoc_ops"),
            "spell_checker": check_ctxt.get("spell_checker"),
            "spell_errors": check_ctxt.get("spell_errors"),
        }
        check_ctxt_rna_tip = check_ctxt_rna
        check_ctxt_rna_tip["multi_rnatip"] = check_ctxt.get("multi_rnatip")

    default_context = settings.DEFAULT_CONTEXT

    # Function definitions
    def walk_properties(cls):
        bl_rna = cls.bl_rna
        # Get our parents' properties, to not export them multiple times.
        bl_rna_base = bl_rna.base
        if bl_rna_base:
            bl_rna_base_props = set(bl_rna_base.properties.values())
        else:
            bl_rna_base_props = set()

        for prop in bl_rna.properties:
            # Only write this property if our parent hasn't got it.
            if prop in bl_rna_base_props:
                continue
            if prop.identifier == "rna_type":
                continue
            reports["rna_props"].append((cls, prop))

            msgsrc = "bpy.types.{}.{}".format(bl_rna.identifier, prop.identifier)
            msgctxt = prop.translation_context or default_context

            if prop.name and (prop.name != prop.identifier or msgctxt != default_context):
                process_msg(msgs, msgctxt, prop.name, msgsrc, reports, check_ctxt_rna, settings)
            if prop.description:
                process_msg(msgs, default_context, prop.description, msgsrc, reports, check_ctxt_rna_tip, settings)

            if isinstance(prop, bpy.types.EnumProperty):
                for item in prop.enum_items:
                    msgsrc = "bpy.types.{}.{}:'{}'".format(bl_rna.identifier, prop.identifier, item.identifier)
                    if item.name and item.name != item.identifier:
                        process_msg(msgs, msgctxt, item.name, msgsrc, reports, check_ctxt_rna, settings)
                    if item.description:
                        process_msg(msgs, default_context, item.description, msgsrc, reports, check_ctxt_rna_tip,
                                    settings)

    blacklist_rna_class = class_blacklist()

    def walk_class(cls):
        bl_rna = cls.bl_rna
        reports["rna_structs"].append(cls)
        if bl_rna.identifier in blacklist_rna_class:
            reports["rna_structs_skipped"].append(cls)
            return

        # XXX translation_context of Operator sub-classes are not "good"!
        #     So ignore those Operator sub-classes (anyway, will get the same from OperatorProperties sub-classes!)...
        if issubclass(cls, bpy.types.Operator):
            reports["rna_structs_skipped"].append(cls)
            return

        msgsrc = "bpy.types." + bl_rna.identifier
        msgctxt = bl_rna.translation_context or default_context

        if bl_rna.name and (bl_rna.name != bl_rna.identifier or msgctxt != default_context):
            process_msg(msgs, msgctxt, bl_rna.name, msgsrc, reports, check_ctxt_rna, settings)

        if bl_rna.description:
            process_msg(msgs, default_context, bl_rna.description, msgsrc, reports, check_ctxt_rna_tip, settings)
        elif cls.__doc__:  # XXX Some classes (like KeyingSetInfo subclasses) have void description... :(
            process_msg(msgs, default_context, cls.__doc__, msgsrc, reports, check_ctxt_rna_tip, settings)

        if hasattr(bl_rna, 'bl_label') and  bl_rna.bl_label:
            process_msg(msgs, msgctxt, bl_rna.bl_label, msgsrc, reports, check_ctxt_rna, settings)

        walk_properties(cls)

    def walk_keymap_hierarchy(hier, msgsrc_prev):
        for lvl in hier:
            msgsrc = msgsrc_prev + "." + lvl[1]
            process_msg(msgs, default_context, lvl[0], msgsrc, reports, None, settings)
            if lvl[3]:
                walk_keymap_hierarchy(lvl[3], msgsrc)

    # Dump Messages
    def process_cls_list(cls_list):
        if not cls_list:
            return

        def full_class_id(cls):
            """ gives us 'ID.Lamp.AreaLamp' which is best for sorting."""
            cls_id = ""
            bl_rna = cls.bl_rna
            while bl_rna:
                cls_id = bl_rna.identifier + "." + cls_id
                bl_rna = bl_rna.base
            return cls_id

        cls_list.sort(key=full_class_id)
        for cls in cls_list:
            walk_class(cls)
            # Recursively process subclasses.
            process_cls_list(cls.__subclasses__())

    # Parse everything (recursively parsing from bpy_struct "class"...).
    process_cls_list(bpy.types.ID.__base__.__subclasses__())

    # And parse keymaps!
    from bpy_extras.keyconfig_utils import KM_HIERARCHY

    walk_keymap_hierarchy(KM_HIERARCHY, "KM_HIERARCHY")


##### Python source code #####
def dump_py_messages_from_files(msgs, reports, files, settings):
    """
    Dump text inlined in the python files given, e.g. 'My Name' in:
        layout.prop("someprop", text="My Name")
    """
    import ast

    bpy_struct = bpy.types.ID.__base__

    root_paths = tuple(bpy.utils.resource_path(t) for t in ('USER', 'LOCAL', 'SYSTEM'))
    def make_rel(path):
        for rp in root_paths:
            if path.startswith(rp):
                return os.path.relpath(path, rp)
        # Use binary's dir as fallback...
        return os.path.relpath(path, os.path.dirname(bpy.app.binary_path))

    # Helper function
    def extract_strings_ex(node, is_split=False):
        """
        Recursively get strings, needed in case we have "Blah" + "Blah", passed as an argument in that case it won't
        evaluate to a string. However, break on some kind of stopper nodes, like e.g. Subscript.
        """
        if type(node) == ast.Str:
            eval_str = ast.literal_eval(node)
            if eval_str:
                yield (is_split, eval_str, (node,))
        else:
            is_split = (type(node) in separate_nodes)
            for nd in ast.iter_child_nodes(node):
                if type(nd) not in stopper_nodes:
                    yield from extract_strings_ex(nd, is_split=is_split)

    def _extract_string_merge(estr_ls, nds_ls):
        return "".join(s for s in estr_ls if s is not None), tuple(n for n in nds_ls if n is not None)

    def extract_strings(node):
        estr_ls = []
        nds_ls = []
        for is_split, estr, nds in extract_strings_ex(node):
            estr_ls.append(estr)
            nds_ls.extend(nds)
        ret = _extract_string_merge(estr_ls, nds_ls)
        return ret
    
    def extract_strings_split(node):
        """
        Returns a list args as returned by 'extract_strings()', But split into groups based on separate_nodes, this way
        expressions like ("A" if test else "B") wont be merged but "A" + "B" will.
        """
        estr_ls = []
        nds_ls = []
        bag = []
        for is_split, estr, nds in extract_strings_ex(node):
            if is_split:
                bag.append((estr_ls, nds_ls))
                estr_ls = []
                nds_ls = []

            estr_ls.append(estr)
            nds_ls.extend(nds)

        bag.append((estr_ls, nds_ls))

        return [_extract_string_merge(estr_ls, nds_ls) for estr_ls, nds_ls in bag]


    def _ctxt_to_ctxt(node):
        return extract_strings(node)[0]

    def _op_to_ctxt(node):
        opname, _ = extract_strings(node)
        if not opname:
            return settings.DEFAULT_CONTEXT
        op = bpy.ops
        for n in opname.split('.'):
            op = getattr(op, n)
        try:
            return op.get_rna().bl_rna.translation_context
        except Exception as e:
            default_op_context = bpy.app.translations.contexts.operator_default
            print("ERROR: ", str(e))
            print("       Assuming default operator context '{}'".format(default_op_context))
            return default_op_context

    # Gather function names.
    # In addition of UI func, also parse pgettext ones...
    # Tuples of (module name, (short names, ...)).
    pgettext_variants = (
        ("pgettext", ("_",)),
        ("pgettext_iface", ("iface_",)),
        ("pgettext_tip", ("tip_",))
    )
    pgettext_variants_args = {"msgid": (0, {"msgctxt": 1})}

    # key: msgid keywords.
    # val: tuples of ((keywords,), context_getter_func) to get a context for that msgid.
    #      Note: order is important, first one wins!
    translate_kw = {
        "text": ((("text_ctxt",), _ctxt_to_ctxt),
                 (("operator",), _op_to_ctxt),
                ),
        "msgid": ((("msgctxt",), _ctxt_to_ctxt),
                 ),
        "message": (),
    }

    context_kw_set = {}
    for k, ctxts in translate_kw.items():
        s = set()
        for c, _ in ctxts:
            s |= set(c)
        context_kw_set[k] = s

    # {func_id: {msgid: (arg_pos,
    #                    {msgctxt: arg_pos,
    #                     ...
    #                    }
    #                   ),
    #            ...
    #           },
    #  ...
    # }
    func_translate_args = {}

    # First, functions from UILayout
    # First loop is for msgid args, second one is for msgctxt args.
    for func_id, func in bpy.types.UILayout.bl_rna.functions.items():
        # check it has one or more arguments as defined in translate_kw
        for arg_pos, (arg_kw, arg) in enumerate(func.parameters.items()):
            if ((arg_kw in translate_kw) and (not arg.is_output) and (arg.type == 'STRING')):
                func_translate_args.setdefault(func_id, {})[arg_kw] = (arg_pos, {})
    for func_id, func in bpy.types.UILayout.bl_rna.functions.items():
        if func_id not in func_translate_args:
            continue
        for arg_pos, (arg_kw, arg) in enumerate(func.parameters.items()):
            if (not arg.is_output) and (arg.type == 'STRING'):
                for msgid, msgctxts in context_kw_set.items():
                    if arg_kw in msgctxts:
                        func_translate_args[func_id][msgid][1][arg_kw] = arg_pos
    # The report() func of operators.
    for func_id, func in bpy.types.Operator.bl_rna.functions.items():
        # check it has one or more arguments as defined in translate_kw
        for arg_pos, (arg_kw, arg) in enumerate(func.parameters.items()):
            if ((arg_kw in translate_kw) and (not arg.is_output) and (arg.type == 'STRING')):
                func_translate_args.setdefault(func_id, {})[arg_kw] = (arg_pos, {})
    # We manually add funcs from bpy.app.translations
    for func_id, func_ids in pgettext_variants:
        func_translate_args[func_id] = pgettext_variants_args
        for func_id in func_ids:
            func_translate_args[func_id] = pgettext_variants_args
    #print(func_translate_args)

    # Break recursive nodes look up on some kind of nodes.
    # E.g. we don’t want to get strings inside subscripts (blah["foo"])!
    stopper_nodes = {ast.Subscript}
    # Consider strings separate: ("a" if test else "b")
    separate_nodes = {ast.IfExp}

    check_ctxt_py = None
    if reports["check_ctxt"]:
        check_ctxt = reports["check_ctxt"]
        check_ctxt_py = {
            "py_in_rna": (check_ctxt.get("py_in_rna"), set(msgs.keys())),
            "multi_lines": check_ctxt.get("multi_lines"),
            "not_capitalized": check_ctxt.get("not_capitalized"),
            "end_point": check_ctxt.get("end_point"),
            "spell_checker": check_ctxt.get("spell_checker"),
            "spell_errors": check_ctxt.get("spell_errors"),
        }

    for fp in files:
        with open(fp, 'r', encoding="utf8") as filedata:
            root_node = ast.parse(filedata.read(), fp, 'exec')

        fp_rel = make_rel(fp)

        for node in ast.walk(root_node):
            if type(node) == ast.Call:
                # print("found function at")
                # print("%s:%d" % (fp, node.lineno))

                # We can't skip such situations! from blah import foo\nfoo("bar") would also be an ast.Name func!
                if type(node.func) == ast.Name:
                    func_id = node.func.id
                elif hasattr(node.func, "attr"):
                    func_id = node.func.attr
                # Ugly things like getattr(self, con.type)(context, box, con)
                else:
                    continue

                func_args = func_translate_args.get(func_id, {})

                # First try to get i18n contexts, for every possible msgid id.
                msgctxts = dict.fromkeys(func_args.keys(), "")
                for msgid, (_, context_args) in func_args.items():
                    context_elements = {}
                    for arg_kw, arg_pos in context_args.items():
                        if arg_pos < len(node.args):
                            context_elements[arg_kw] = node.args[arg_pos]
                        else:
                            for kw in node.keywords:
                                if kw.arg == arg_kw:
                                    context_elements[arg_kw] = kw.value
                                    break
                    #print(context_elements)
                    for kws, proc in translate_kw[msgid]:
                        if set(kws) <= context_elements.keys():
                            args = tuple(context_elements[k] for k in kws)
                            #print("running ", proc, " with ", args)
                            ctxt = proc(*args)
                            if ctxt:
                                msgctxts[msgid] = ctxt
                                break

                #print(translate_args)
                # do nothing if not found
                for arg_kw, (arg_pos, _) in func_args.items():
                    msgctxt = msgctxts[arg_kw]
                    estr_lst = [(None, ())]
                    if arg_pos < len(node.args):
                        estr_lst = extract_strings_split(node.args[arg_pos])
                        #print(estr, nds)
                    else:
                        for kw in node.keywords:
                            if kw.arg == arg_kw:
                                estr_lst = extract_strings_split(kw.value)
                                break
                        #print(estr, nds)
                    for estr, nds in estr_lst:
                        if estr:
                            if nds:
                                msgsrc = "{}:{}".format(fp_rel, sorted({nd.lineno for nd in nds})[0])
                            else:
                                msgsrc = "{}:???".format(fp_rel)
                            process_msg(msgs, msgctxt, estr, msgsrc, reports, check_ctxt_py, settings)
                            reports["py_messages"].append((msgctxt, estr, msgsrc))


def dump_py_messages(msgs, reports, addons, settings, addons_only=False):
    def _get_files(path):
        if not os.path.exists(path):
            return []
        if os.path.isdir(path):
            return [os.path.join(dpath, fn) for dpath, _, fnames in os.walk(path) for fn in fnames
                                            if not fn.startswith("_") and fn.endswith(".py")]
        return [path]

    files = []
    if not addons_only:
        for path in settings.CUSTOM_PY_UI_FILES:
            for root in (bpy.utils.resource_path(t) for t in ('USER', 'LOCAL', 'SYSTEM')):
                files += _get_files(os.path.join(root, path))

    # Add all given addons.
    for mod in addons:
        fn = mod.__file__
        if os.path.basename(fn) == "__init__.py":
            files += _get_files(os.path.dirname(fn))
        else:
            files.append(fn)

    dump_py_messages_from_files(msgs, reports, sorted(files), settings)


##### C source code #####
def dump_src_messages(msgs, reports, settings):
    def get_contexts():
        """Return a mapping {C_CTXT_NAME: ctxt_value}."""
        return {k: getattr(bpy.app.translations.contexts, n) for k, n in bpy.app.translations.contexts_C_to_py.items()}

    contexts = get_contexts()

    # Build regexes to extract messages (with optional contexts) from C source.
    pygettexts = tuple(re.compile(r).search for r in settings.PYGETTEXT_KEYWORDS)

    _clean_str = re.compile(settings.str_clean_re).finditer
    clean_str = lambda s: "".join(m.group("clean") for m in _clean_str(s))

    def dump_src_file(path, rel_path, msgs, reports, settings):
        def process_entry(_msgctxt, _msgid):
            # Context.
            msgctxt = settings.DEFAULT_CONTEXT
            if _msgctxt:
                if _msgctxt in contexts:
                    msgctxt = contexts[_msgctxt]
                elif '"' in _msgctxt or "'" in _msgctxt:
                    msgctxt = clean_str(_msgctxt)
                else:
                    print("WARNING: raw context “{}” couldn’t be resolved!".format(_msgctxt))
            # Message.
            msgid = ""
            if _msgid:
                if '"' in _msgid or "'" in _msgid:
                    msgid = clean_str(_msgid)
                else:
                    print("WARNING: raw message “{}” couldn’t be resolved!".format(_msgid))
            return msgctxt, msgid

        check_ctxt_src = None
        if reports["check_ctxt"]:
            check_ctxt = reports["check_ctxt"]
            check_ctxt_src = {
                "multi_lines": check_ctxt.get("multi_lines"),
                "not_capitalized": check_ctxt.get("not_capitalized"),
                "end_point": check_ctxt.get("end_point"),
                "spell_checker": check_ctxt.get("spell_checker"),
                "spell_errors": check_ctxt.get("spell_errors"),
            }

        data = ""
        with open(path) as f:
            data = f.read()
        for srch in pygettexts:
            m = srch(data)
            line = pos = 0
            while m:
                d = m.groupdict()
                # Line.
                line += data[pos:m.start()].count('\n')
                msgsrc = rel_path + ":" + str(line)
                _msgid = d.get("msg_raw")
                # First, try the "multi-contexts" stuff!
                _msgctxts = tuple(d.get("ctxt_raw{}".format(i)) for i in range(settings.PYGETTEXT_MAX_MULTI_CTXT))
                if _msgctxts[0]:
                    for _msgctxt in _msgctxts:
                        if not _msgctxt:
                            break
                        msgctxt, msgid = process_entry(_msgctxt, _msgid)
                        process_msg(msgs, msgctxt, msgid, msgsrc, reports, check_ctxt_src, settings)
                        reports["src_messages"].append((msgctxt, msgid, msgsrc))
                else:
                    _msgctxt = d.get("ctxt_raw")
                    msgctxt, msgid = process_entry(_msgctxt, _msgid)
                    process_msg(msgs, msgctxt, msgid, msgsrc, reports, check_ctxt_src, settings)
                    reports["src_messages"].append((msgctxt, msgid, msgsrc))

                pos = m.end()
                line += data[m.start():pos].count('\n')
                m = srch(data, pos)

    forbidden = set()
    forced = set()
    if os.path.isfile(settings.SRC_POTFILES):
        with open(settings.SRC_POTFILES) as src:
            for l in src:
                if l[0] == '-':
                    forbidden.add(l[1:].rstrip('\n'))
                elif l[0] != '#':
                    forced.add(l.rstrip('\n'))
    for root, dirs, files in os.walk(settings.POTFILES_SOURCE_DIR):
        if "/.svn" in root:
            continue
        for fname in files:
            if os.path.splitext(fname)[1] not in settings.PYGETTEXT_ALLOWED_EXTS:
                continue
            path = os.path.join(root, fname)
            rel_path = os.path.relpath(path, settings.SOURCE_DIR)
            if rel_path in forbidden:
                continue
            elif rel_path not in forced:
                forced.add(rel_path)
    for rel_path in sorted(forced):
        path = os.path.join(settings.SOURCE_DIR, rel_path)
        if os.path.exists(path):
            dump_src_file(path, rel_path, msgs, reports, settings)


##### Main functions! #####
def dump_messages(do_messages, do_checks, settings):
    bl_ver = "Blender " + bpy.app.version_string
    bl_rev = bpy.app.build_revision
    bl_date = datetime.datetime.strptime(bpy.app.build_date.decode() + "T" + bpy.app.build_time.decode(),
                                         "%Y-%m-%dT%H:%M:%S")
    pot = utils.I18nMessages.gen_empty_messages(settings.PARSER_TEMPLATE_ID, bl_ver, bl_rev, bl_date, bl_date.year,
                                                settings=settings)
    msgs = pot.msgs

    # Enable all wanted addons.
    # For now, enable all official addons, before extracting msgids.
    addons = enable_addons(support={"OFFICIAL"})
    # Note this is not needed if we have been started with factory settings, but just in case...
    enable_addons(support={"COMMUNITY", "TESTING"}, disable=True)

    reports = _gen_reports(_gen_check_ctxt(settings) if do_checks else None)

    # Get strings from RNA.
    dump_messages_rna(msgs, reports, settings)

    # Get strings from UI layout definitions text="..." args.
    dump_py_messages(msgs, reports, addons, settings)

    # Get strings from C source code.
    dump_src_messages(msgs, reports, settings)

    # Get strings from addons' categories.
    for uid, label, tip in bpy.types.WindowManager.addon_filter[1]['items'](bpy.context.window_manager, bpy.context):
        process_msg(msgs, settings.DEFAULT_CONTEXT, label, "Addons' categories", reports, None, settings)
        if tip:
            process_msg(msgs, settings.DEFAULT_CONTEXT, tip, "Addons' categories", reports, None, settings)

    # Get strings specific to translations' menu.
    for lng in settings.LANGUAGES:
        process_msg(msgs, settings.DEFAULT_CONTEXT, lng[1], "Languages’ labels from bl_i18n_utils/settings.py",
                    reports, None, settings)
    for cat in settings.LANGUAGES_CATEGORIES:
        process_msg(msgs, settings.DEFAULT_CONTEXT, cat[1],
                    "Language categories’ labels from bl_i18n_utils/settings.py", reports, None, settings)

    #pot.check()
    pot.unescape()  # Strings gathered in py/C source code may contain escaped chars...
    print_info(reports, pot)
    #pot.check()

    if do_messages:
        print("Writing messages…")
        pot.write('PO', settings.FILE_NAME_POT)

    print("Finished extracting UI messages!")


def dump_addon_messages(module_name, messages_formats, do_checks, settings):
    # Enable our addon and get strings from RNA.
    addon = enable_addons(addons={module_name})[0]

    addon_info = addon_utils.module_bl_info(addon)
    ver = addon_info.name + " " + ".".join(addon_info.version)
    rev = "???"
    date = datetime.datetime()
    pot = utils.I18nMessages.gen_empty_messages(settings.PARSER_TEMPLATE_ID, ver, rev, date, date.year,
                                                settings=settings)
    msgs = pot.msgs

    minus_msgs = copy.deepcopy(msgs)

    check_ctxt = _gen_check_ctxt(settings) if do_checks else None
    minus_check_ctxt = _gen_check_ctxt(settings) if do_checks else None

    # Get current addon state (loaded or not):
    was_loaded = addon_utils.check(module_name)[1]

    # Enable our addon and get strings from RNA.
    addons = enable_addons(addons={module_name})
    reports = _gen_reports(check_ctxt)
    dump_messages_rna(msgs, reports, settings)

    # Now disable our addon, and rescan RNA.
    enable_addons(addons={module_name}, disable=True)
    reports["check_ctxt"] = minus_check_ctxt
    dump_messages_rna(minus_msgs, reports, settings)

    # Restore previous state if needed!
    if was_loaded:
        enable_addons(addons={module_name})

    # and make the diff!
    for key in minus_msgs:
        if key == settings.PO_HEADER_KEY:
            continue
        del msgs[key]

    if check_ctxt:
        for key in check_ctxt:
            for warning in minus_check_ctxt[key]:
                check_ctxt[key].remove(warning)

    # and we are done with those!
    del minus_msgs
    del minus_check_ctxt

    # get strings from UI layout definitions text="..." args
    reports["check_ctxt"] = check_ctxt
    dump_messages_pytext(msgs, reports, addons, settings, addons_only=True)

    print_info(reports, pot)

    return pot


def main():
    try:
        import bpy
    except ImportError:
        print("This script must run from inside blender")
        return

    import sys
    back_argv = sys.argv
    # Get rid of Blender args!
    sys.argv = sys.argv[sys.argv.index("--") + 1:]

    import argparse
    parser = argparse.ArgumentParser(description="Process UI messages from inside Blender.")
    parser.add_argument('-c', '--no_checks', default=True, action="store_false", help="No checks over UI messages.")
    parser.add_argument('-m', '--no_messages', default=True, action="store_false", help="No export of UI messages.")
    parser.add_argument('-o', '--output', default=None, help="Output POT file path.")
    parser.add_argument('-s', '--settings', default=None,
                        help="Override (some) default settings. Either a JSon file name, or a JSon string.")
    args = parser.parse_args()

    settings = i18n_settings.I18nSettings()
    settings.from_json(args.settings)

    if args.output:
        settings.FILE_NAME_POT = args.output

    dump_messages(do_messages=args.no_messages, do_checks=args.no_checks, settings=settings)

    sys.argv = back_argv


if __name__ == "__main__":
    print("\n\n *** Running {} *** \n".format(__file__))
    main()
