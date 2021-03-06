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

import datetime
import os
import re
import sys

# XXX Relative import does not work here when used from Blender...
from bl_i18n_utils import settings as settings_i18n, utils

import bpy

##### Utils #####

# check for strings like "+%f°"
ignore_reg = re.compile(r"^(?:[-*.()/\\+%°0-9]|%d|%f|%s|%r|\s)*$")
filter_message = ignore_reg.match


def init_spell_check(settings, lang="en_US"):
    try:
        from bl_i18n_utils import utils_spell_check
        return utils_spell_check.SpellChecker(settings, lang)
    except Exception as e:
        print("Failed to import utils_spell_check ({})".format(str(e)))
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


def _diff_check_ctxt(check_ctxt, minus_check_ctxt):
    """Removes minus_check_ctxt from check_ctxt"""
    for key in check_ctxt:
        if isinstance(check_ctxt[key], set):
            for warning in minus_check_ctxt[key]:
                if warning in check_ctxt[key]:
                    check_ctxt[key].remove(warning)
        elif isinstance(check_ctxt[key], dict):
            for warning in minus_check_ctxt[key]:
                if warning in check_ctxt[key]:
                    del check_ctxt[key][warning]


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
        if (
                key[1].strip().endswith('.') and
                (not key[1].strip().endswith('...')) and
                key[1] not in settings.WARN_MSGID_END_POINT_ALLOWED
        ):
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
    pot.print_info(prefix="\t", output=_print)
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
    # Also, multi-lines tooltips are valid now.
    keys = not_capitalized | end_point | undoc_ops | spell_errors.keys()
    if keys:
        _print("WARNINGS:")
        for key in keys:
            if undoc_ops and key in undoc_ops:
                _print("\tThe following operators are undocumented!")
            else:
                _print("\t“{}”|“{}”:".format(*key))
                # We support multi-lines tooltips now...
                # ~ if multi_lines and key in multi_lines:
                # ~     _print("\t\t-> newline in this message!")
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
                    lines = [
                        "\t\t-> {}: misspelled, suggestions are ({})".format(w, "'" + "', '".join(errs) + "'")
                        for w, errs in spell_errors[key]
                    ]
                    _print("\n".join(lines))
            _print("\t\t{}".format("\n\t\t".join(pot.msgs[key].sources)))


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
def dump_rna_messages(msgs, reports, settings, verbose=False):
    """
    Dump into messages dict all RNA-defined UI messages (labels en tooltips).
    """
    def class_blacklist():
        blacklist_rna_class = {getattr(bpy.types, cls_id) for cls_id in (
            # core classes
            "Context", "Event", "Function", "UILayout", "UnknownType", "Property", "Struct",
            # registerable classes
            "Panel", "Menu", "Header", "RenderEngine", "Operator", "OperatorMacro", "Macro", "KeyingSetInfo",
            # window classes
            "Window",
        )
        }

        # More builtin classes we don't need to parse.
        blacklist_rna_class |= {cls for cls in bpy.types.Property.__subclasses__()}

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

        props = sorted(bl_rna.properties, key=lambda p: p.identifier)
        for prop in props:
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
                done_items = set()
                for item in prop.enum_items:
                    msgsrc = "bpy.types.{}.{}:'{}'".format(bl_rna.identifier, prop.identifier, item.identifier)
                    done_items.add(item.identifier)
                    if item.name and item.name != item.identifier:
                        process_msg(msgs, msgctxt, item.name, msgsrc, reports, check_ctxt_rna, settings)
                    if item.description:
                        process_msg(msgs, default_context, item.description, msgsrc, reports, check_ctxt_rna_tip,
                                    settings)
                for item in prop.enum_items_static:
                    if item.identifier in done_items:
                        continue
                    msgsrc = "bpy.types.{}.{}:'{}'".format(bl_rna.identifier, prop.identifier, item.identifier)
                    done_items.add(item.identifier)
                    if item.name and item.name != item.identifier:
                        process_msg(msgs, msgctxt, item.name, msgsrc, reports, check_ctxt_rna, settings)
                    if item.description:
                        process_msg(msgs, default_context, item.description, msgsrc, reports, check_ctxt_rna_tip,
                                    settings)

    def walk_tools_definitions(cls):
        from bl_ui.space_toolsystem_common import ToolDef

        bl_rna = cls.bl_rna
        op_default_context = bpy.app.translations.contexts.operator_default

        def process_tooldef(tool_context, tool):
            if not isinstance(tool, ToolDef):
                if callable(tool):
                    for t in tool(None):
                        process_tooldef(tool_context, t)
                return
            msgsrc = "bpy.types.{} Tools: '{}', '{}'".format(bl_rna.identifier, tool_context, tool.idname)
            if tool.label:
                process_msg(msgs, op_default_context, tool.label, msgsrc, reports, check_ctxt_rna, settings)
            # Callable (function) descriptions must handle their translations themselves.
            if tool.description and not callable(tool.description):
                process_msg(msgs, default_context, tool.description, msgsrc, reports, check_ctxt_rna_tip, settings)

        for tool_context, tools_defs in cls.tools_all():
            for tools_group in tools_defs:
                if tools_group is None:
                    continue
                elif isinstance(tools_group, tuple) and not isinstance(tools_group, ToolDef):
                    for tool in tools_group:
                        process_tooldef(tool_context, tool)
                else:
                    process_tooldef(tool_context, tools_group)

    blacklist_rna_class = class_blacklist()

    def walk_class(cls):
        bl_rna = cls.bl_rna
        msgsrc = "bpy.types." + bl_rna.identifier
        msgctxt = bl_rna.translation_context or default_context

        if bl_rna.name and (bl_rna.name != bl_rna.identifier or msgctxt != default_context):
            process_msg(msgs, msgctxt, bl_rna.name, msgsrc, reports, check_ctxt_rna, settings)

        if bl_rna.description:
            process_msg(msgs, default_context, bl_rna.description, msgsrc, reports, check_ctxt_rna_tip, settings)
        elif cls.__doc__:  # XXX Some classes (like KeyingSetInfo subclasses) have void description... :(
            process_msg(msgs, default_context, cls.__doc__, msgsrc, reports, check_ctxt_rna_tip, settings)

        # Panels' "tabs" system.
        if hasattr(bl_rna, 'bl_category') and bl_rna.bl_category:
            process_msg(msgs, default_context, bl_rna.bl_category, msgsrc, reports, check_ctxt_rna, settings)

        if hasattr(bl_rna, 'bl_label') and bl_rna.bl_label:
            process_msg(msgs, msgctxt, bl_rna.bl_label, msgsrc, reports, check_ctxt_rna, settings)

        # Tools Panels definitions.
        if hasattr(bl_rna, 'tools_all') and bl_rna.tools_all:
            walk_tools_definitions(cls)

        walk_properties(cls)

    def walk_keymap_hierarchy(hier, msgsrc_prev):
        km_i18n_context = bpy.app.translations.contexts.id_windowmanager
        for lvl in hier:
            msgsrc = msgsrc_prev + "." + lvl[1]
            if isinstance(lvl[0], str):  # Can be a function too, now, with tool system...
                process_msg(msgs, km_i18n_context, lvl[0], msgsrc, reports, None, settings)
            if lvl[3]:
                walk_keymap_hierarchy(lvl[3], msgsrc)

    # Dump Messages
    operator_categories = {}

    def process_cls_list(cls_list):
        if not cls_list:
            return

        def full_class_id(cls):
            """Gives us 'ID.Light.AreaLight' which is best for sorting."""
            # Always the same issue, some classes listed in blacklist should actually no more exist (they have been
            # unregistered), but are still listed by __subclasses__() calls... :/
            if cls in blacklist_rna_class:
                return cls.__name__
            cls_id = ""
            bl_rna = cls.bl_rna
            while bl_rna:
                cls_id = bl_rna.identifier + "." + cls_id
                bl_rna = bl_rna.base
            return cls_id

        def operator_category(cls):
            """Extract operators' categories, as displayed in 'search' space menu."""
            # NOTE: keep in sync with C code in ui_searchbox_region_draw_cb__operator().
            if issubclass(cls, bpy.types.OperatorProperties) and "_OT_" in cls.__name__:
                cat_id = cls.__name__.split("_OT_")[0]
                if cat_id not in operator_categories:
                    cat_str = cat_id.capitalize() + ":"
                    operator_categories[cat_id] = cat_str

        if verbose:
            print(cls_list)
        cls_list.sort(key=full_class_id)
        for cls in cls_list:
            if verbose:
                print(cls)
            reports["rna_structs"].append(cls)
            # Ignore those Operator sub-classes (anyway, will get the same from OperatorProperties sub-classes!)...
            if (cls in blacklist_rna_class) or issubclass(cls, bpy.types.Operator):
                reports["rna_structs_skipped"].append(cls)
            else:
                operator_category(cls)
                walk_class(cls)
            # Recursively process subclasses.
            process_cls_list(cls.__subclasses__())

    # Parse everything (recursively parsing from bpy_struct "class"...).
    process_cls_list(bpy.types.ID.__base__.__subclasses__())

    # Finalize generated 'operator categories' messages.
    for cat_str in operator_categories.values():
        process_msg(msgs, bpy.app.translations.contexts.operator_default, cat_str, "Generated operator category",
                    reports, check_ctxt_rna, settings)

    # And parse keymaps!
    from bl_keymap_utils import keymap_hierarchy
    walk_keymap_hierarchy(keymap_hierarchy.generate(), "KM_HIERARCHY")


##### Python source code #####
def dump_py_messages_from_files(msgs, reports, files, settings):
    """
    Dump text inlined in the python files given, e.g. 'My Name' in:
        layout.prop("someprop", text="My Name")
    """
    import ast

    bpy_struct = bpy.types.ID.__base__
    i18n_contexts = bpy.app.translations.contexts

    root_paths = tuple(bpy.utils.resource_path(t) for t in ('USER', 'LOCAL', 'SYSTEM'))

    def make_rel(path):
        for rp in root_paths:
            if path.startswith(rp):
                try:  # can't always find the relative path (between drive letters on windows)
                    return os.path.relpath(path, rp)
                except ValueError:
                    return path
        # Use binary's dir as fallback...
        try:  # can't always find the relative path (between drive letters on windows)
            return os.path.relpath(path, os.path.dirname(bpy.app.binary_path))
        except ValueError:
            return path

    # Helper function
    def extract_strings_ex(node, is_split=False):
        """
        Recursively get strings, needed in case we have "Blah" + "Blah", passed as an argument in that case it won't
        evaluate to a string. However, break on some kind of stopper nodes, like e.g. Subscript.
        """
        # New in py 3.8: all constants are of type 'ast.Constant'.
        # 'ast.Str' will have to be removed when we officially switch to this version.
        if type(node) in {ast.Str, getattr(ast, "Constant", None)}:
            eval_str = ast.literal_eval(node)
            if eval_str and type(eval_str) == str:
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
        Returns a list args as returned by 'extract_strings()', but split into groups based on separate_nodes, this way
        expressions like ("A" if test else "B") won't be merged but "A" + "B" will.
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

    i18n_ctxt_ids = {v for v in bpy.app.translations.contexts_C_to_py.values()}

    def _ctxt_to_ctxt(node):
        # We must try, to some extend, to get contexts from vars instead of only literal strings...
        ctxt = extract_strings(node)[0]
        if ctxt:
            return ctxt
        # Basically, we search for attributes matching py context names, for now.
        # So non-literal contexts should be used that way:
        #     i18n_ctxt = bpy.app.translations.contexts
        #     foobar(text="Foo", text_ctxt=i18n_ctxt.id_object)
        if type(node) == ast.Attribute:
            if node.attr in i18n_ctxt_ids:
                #print(node, node.attr, getattr(i18n_contexts, node.attr))
                return getattr(i18n_contexts, node.attr)
        return i18n_contexts.default

    def _op_to_ctxt(node):
        # Some smart coders like things like:
        #    >>> row.operator("preferences.addon_disable" if is_enabled else "preferences.addon_enable", ...)
        # We only take first arg into account here!
        bag = extract_strings_split(node)
        opname, _ = bag[0]
        if not opname:
            return i18n_contexts.default
        op = bpy.ops
        for n in opname.split('.'):
            op = getattr(op, n)
        try:
            return op.get_rna_type().translation_context
        except Exception as e:
            default_op_context = i18n_contexts.operator_default
            print("ERROR: ", str(e))
            print("       Assuming default operator context '{}'".format(default_op_context))
            return default_op_context

    # Gather function names.
    # In addition of UI func, also parse pgettext ones...
    # Tuples of (module name, (short names, ...)).
    pgettext_variants = (
        ("pgettext", ("_",)),
        ("pgettext_iface", ("iface_",)),
        ("pgettext_tip", ("tip_",)),
        ("pgettext_data", ("data_",)),
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
        "heading": (),
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
        for sub_func_id in func_ids:
            func_translate_args[sub_func_id] = pgettext_variants_args
    # print(func_translate_args)

    # Break recursive nodes look up on some kind of nodes.
    # E.g. we don't want to get strings inside subscripts (blah["foo"])!
    #      we don't want to get strings from comparisons (foo.type == 'BAR').
    stopper_nodes = {ast.Subscript, ast.Compare}
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
        # ~ print("Checking File ", fp)
        with open(fp, 'r', encoding="utf8") as filedata:
            root_node = ast.parse(filedata.read(), fp, 'exec')

        fp_rel = make_rel(fp)

        for node in ast.walk(root_node):
            if type(node) == ast.Call:
                # ~ print("found function at")
                # ~ print("%s:%d" % (fp, node.lineno))

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
                    # ~ print(context_elements)
                    for kws, proc in translate_kw[msgid]:
                        if set(kws) <= context_elements.keys():
                            args = tuple(context_elements[k] for k in kws)
                            # ~ print("running ", proc, " with ", args)
                            ctxt = proc(*args)
                            if ctxt:
                                msgctxts[msgid] = ctxt
                                break

                # ~ print(func_args)
                # do nothing if not found
                for arg_kw, (arg_pos, _) in func_args.items():
                    msgctxt = msgctxts[arg_kw]
                    estr_lst = [(None, ())]
                    if arg_pos < len(node.args):
                        estr_lst = extract_strings_split(node.args[arg_pos])
                    else:
                        for kw in node.keywords:
                            if kw.arg == arg_kw:
                                # ~ print(kw.arg, kw.value)
                                estr_lst = extract_strings_split(kw.value)
                                break
                    for estr, nds in estr_lst:
                        # ~ print(estr, nds)
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

    def clean_str(s):
        # The encode/decode to/from 'raw_unicode_escape' allows to transform the C-type unicode hexadecimal escapes
        # (like '\u2715' for the '×' symbol) back into a proper unicode character.
        return "".join(m.group("clean") for m in _clean_str(s)).encode('raw_unicode_escape').decode('raw_unicode_escape')

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
        with open(path, encoding="utf8") as f:
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
        with open(settings.SRC_POTFILES, encoding="utf8") as src:
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
            try:  # can't always find the relative path (between drive letters on windows)
                rel_path = os.path.relpath(path, settings.SOURCE_DIR)
            except ValueError:
                rel_path = path
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
    bl_hash = bpy.app.build_hash
    bl_date = datetime.datetime.strptime(bpy.app.build_date.decode() + "T" + bpy.app.build_time.decode(),
                                         "%Y-%m-%dT%H:%M:%S")
    pot = utils.I18nMessages.gen_empty_messages(settings.PARSER_TEMPLATE_ID, bl_ver, bl_hash, bl_date, bl_date.year,
                                                settings=settings)
    msgs = pot.msgs

    # Enable all wanted addons.
    # For now, enable all official addons, before extracting msgids.
    addons = utils.enable_addons(support={"OFFICIAL"})
    # Note this is not needed if we have been started with factory settings, but just in case...
    # XXX This is not working well, spent a whole day trying to understand *why* we still have references of
    #     those removed calsses in things like `bpy.types.OperatorProperties.__subclasses__()`
    #     (could not even reproduce it from regular py console in Blender with UI...).
    #     For some reasons, cleanup does not happen properly, *and* we have no way to tell which class is valid
    #     and which has been unregistered. So for now, just go for the dirty, easy way: do not disable add-ons. :(
    # ~ utils.enable_addons(support={"COMMUNITY", "TESTING"}, disable=True)

    reports = _gen_reports(_gen_check_ctxt(settings) if do_checks else None)

    # Get strings from RNA.
    dump_rna_messages(msgs, reports, settings)

    # Get strings from UI layout definitions text="..." args.
    dump_py_messages(msgs, reports, addons, settings)

    # Get strings from C source code.
    dump_src_messages(msgs, reports, settings)

    # Get strings from addons' categories.
    for uid, label, tip in bpy.types.WindowManager.addon_filter.keywords['items'](bpy.context.window_manager, bpy.context):
        process_msg(msgs, settings.DEFAULT_CONTEXT, label, "Add-ons' categories", reports, None, settings)
        if tip:
            process_msg(msgs, settings.DEFAULT_CONTEXT, tip, "Add-ons' categories", reports, None, settings)

    # Get strings specific to translations' menu.
    for lng in settings.LANGUAGES:
        process_msg(msgs, settings.DEFAULT_CONTEXT, lng[1], "Languages’ labels from bl_i18n_utils/settings.py",
                    reports, None, settings)
    for cat in settings.LANGUAGES_CATEGORIES:
        process_msg(msgs, settings.DEFAULT_CONTEXT, cat[1],
                    "Language categories’ labels from bl_i18n_utils/settings.py", reports, None, settings)

    # pot.check()
    pot.unescape()  # Strings gathered in py/C source code may contain escaped chars...
    print_info(reports, pot)
    # pot.check()

    if do_messages:
        print("Writing messages…")
        pot.write('PO', settings.FILE_NAME_POT)

    print("Finished extracting UI messages!")

    return pot  # Not used currently, but may be useful later (and to be consistent with dump_addon_messages!).


def dump_addon_messages(module_name, do_checks, settings):
    import addon_utils

    # Get current addon state (loaded or not):
    was_loaded = addon_utils.check(module_name)[1]

    # Enable our addon.
    addon = utils.enable_addons(addons={module_name})[0]

    addon_info = addon_utils.module_bl_info(addon)
    ver = addon_info["name"] + " " + ".".join(str(v) for v in addon_info["version"])
    rev = 0
    date = datetime.datetime.now()
    pot = utils.I18nMessages.gen_empty_messages(settings.PARSER_TEMPLATE_ID, ver, rev, date, date.year,
                                                settings=settings)
    msgs = pot.msgs

    minus_pot = utils.I18nMessages.gen_empty_messages(settings.PARSER_TEMPLATE_ID, ver, rev, date, date.year,
                                                      settings=settings)
    minus_msgs = minus_pot.msgs

    check_ctxt = _gen_check_ctxt(settings) if do_checks else None
    minus_check_ctxt = _gen_check_ctxt(settings) if do_checks else None

    # Get strings from RNA, our addon being enabled.
    print("A")
    reports = _gen_reports(check_ctxt)
    print("B")
    dump_rna_messages(msgs, reports, settings)
    print("C")

    # Now disable our addon, and rescan RNA.
    utils.enable_addons(addons={module_name}, disable=True)
    print("D")
    reports["check_ctxt"] = minus_check_ctxt
    print("E")
    dump_rna_messages(minus_msgs, reports, settings)
    print("F")

    # Restore previous state if needed!
    if was_loaded:
        utils.enable_addons(addons={module_name})

    # and make the diff!
    for key in minus_msgs:
        if key != settings.PO_HEADER_KEY:
            del msgs[key]

    if check_ctxt:
        _diff_check_ctxt(check_ctxt, minus_check_ctxt)

    # and we are done with those!
    del minus_pot
    del minus_msgs
    del minus_check_ctxt

    # get strings from UI layout definitions text="..." args
    reports["check_ctxt"] = check_ctxt
    dump_py_messages(msgs, reports, {addon}, settings, addons_only=True)

    pot.unescape()  # Strings gathered in py/C source code may contain escaped chars...
    print_info(reports, pot)

    print("Finished extracting UI messages!")

    return pot


def main():
    try:
        import bpy
    except ImportError:
        print("This script must run from inside blender")
        return

    import sys
    import argparse

    # Get rid of Blender args!
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []

    parser = argparse.ArgumentParser(description="Process UI messages from inside Blender.")
    parser.add_argument('-c', '--no_checks', default=True, action="store_false", help="No checks over UI messages.")
    parser.add_argument('-m', '--no_messages', default=True, action="store_false", help="No export of UI messages.")
    parser.add_argument('-o', '--output', default=None, help="Output POT file path.")
    parser.add_argument('-s', '--settings', default=None,
                        help="Override (some) default settings. Either a JSon file name, or a JSon string.")
    args = parser.parse_args(argv)

    settings = settings_i18n.I18nSettings()
    settings.load(args.settings)

    if args.output:
        settings.FILE_NAME_POT = args.output

    dump_messages(do_messages=args.no_messages, do_checks=args.no_checks, settings=settings)


if __name__ == "__main__":
    print("\n\n *** Running {} *** \n".format(__file__))
    main()
