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

# This is a quite stupid script which extracts bmesh api docs from
# 'bmesh_opdefines.c' in order to avoid having to add a lot of introspection
# data access into the api.
#
# The script is stupid becase it makes assumptions about formatting...
# that each arg has its own line, that comments above or directly after will be __doc__ etc...
#
# We may want to replace this script with something else one day but for now its good enough.
# if it needs large updates it may be better to rewrite using a real parser or
# add introspection into bmesh.ops.
# - campbell

import os

CURRENT_DIR = os.path.abspath(os.path.dirname(__file__))
SOURCE_DIR = os.path.normpath(os.path.abspath(os.path.normpath(os.path.join(CURRENT_DIR, "..", ".."))))
FILE_OP_DEFINES_C = os.path.join(SOURCE_DIR, "source", "blender", "bmesh", "intern", "bmesh_opdefines.c")
OUT_RST = os.path.join(CURRENT_DIR, "rst", "bmesh.ops.rst")

HEADER = r"""
BMesh Operators (bmesh.ops)
===========================

.. module:: bmesh.ops

This module gives access to low level bmesh operations.

Most operators take input and return output, they can be chained together
to perform useful operations.

.. note::

   This API us new in 2.65 and not yet well tested.


Operator Example
++++++++++++++++
This script shows how operators can be used to model a link of a chain.

.. literalinclude:: ../examples/bmesh.ops.1.py
"""


def main():
    fsrc = open(FILE_OP_DEFINES_C, 'r', encoding="utf-8")

    blocks = []

    is_block = False
    is_comment = False  # /* global comments only */

    comment_ctx = None
    block_ctx = None

    for l in fsrc:
        l = l[:-1]
        # weak but ok
        if ("BMOpDefine" in l and l.split()[1] == "BMOpDefine") and "bmo_opdefines[]" not in l:
            is_block = True
            block_ctx = []
            blocks.append((comment_ctx, block_ctx))
        elif l.strip().startswith("/*"):
            is_comment = True
            comment_ctx = []

        if is_block:
            if l.strip().startswith("//"):
                pass
            else:
                # remove c++ comment if we have one
                cpp_comment = l.find("//")
                if cpp_comment != -1:
                    l = l[:cpp_comment]

                block_ctx.append(l)

            if l.strip() == "};":
                is_block = False
                comment_ctx = None

        if is_comment:
            c_comment_start = l.find("/*")
            if c_comment_start != -1:
                l = l[c_comment_start + 2:]

            c_comment_end = l.find("*/")
            if c_comment_end != -1:
                l = l[:c_comment_end]

                is_comment = False
            comment_ctx.append(l)

    fsrc.close()
    del fsrc

    # namespace hack
    vars = (
        "BMO_OP_SLOT_ELEMENT_BUF",
        "BMO_OP_SLOT_BOOL",
        "BMO_OP_SLOT_FLT",
        "BMO_OP_SLOT_INT",
        "BMO_OP_SLOT_MAT",
        "BMO_OP_SLOT_VEC",
        "BMO_OP_SLOT_PTR",
        "BMO_OP_SLOT_MAPPING",

        "BMO_OP_SLOT_SUBTYPE_MAP_ELEM",
        "BMO_OP_SLOT_SUBTYPE_MAP_BOOL",
        "BMO_OP_SLOT_SUBTYPE_MAP_INT",
        "BMO_OP_SLOT_SUBTYPE_MAP_FLT",
        "BMO_OP_SLOT_SUBTYPE_MAP_EMPTY",
        "BMO_OP_SLOT_SUBTYPE_MAP_INTERNAL",

        "BMO_OP_SLOT_SUBTYPE_PTR_SCENE",
        "BMO_OP_SLOT_SUBTYPE_PTR_OBJECT",
        "BMO_OP_SLOT_SUBTYPE_PTR_MESH",
        "BMO_OP_SLOT_SUBTYPE_PTR_BMESH",

        "BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE",

        "BM_VERT",
        "BM_EDGE",
        "BM_FACE",

        "BMO_OPTYPE_FLAG_NORMALS_CALC",
        "BMO_OPTYPE_FLAG_UNTAN_MULTIRES",
        "BMO_OPTYPE_FLAG_SELECT_FLUSH",
        "BMO_OPTYPE_FLAG_NOP",
    )
    vars_dict = {}
    for i, v in enumerate(vars):
        vars_dict[v] = (1 << i)
    globals().update(vars_dict)
    # reverse lookup
    vars_dict_reverse = {v: k for k, v in vars_dict.items()}
    # end namespace hack

    blocks_py = []
    for comment, b in blocks:
        # magic, translate into python
        b[0] = b[0].replace("static BMOpDefine ", "")

        for i, l in enumerate(b):
            l = l.strip()
            l = l.replace("{", "(")
            l = l.replace("}", ")")

            if l.startswith("/*"):
                l = l.replace("/*", "'''own <")
            else:
                l = l.replace("/*", "'''inline <")
            l = l.replace("*/", ">''',")

            # exec func. eg: bmo_rotate_edges_exec,
            if l.startswith("bmo_") and l.endswith("_exec,"):
                l = "None,"
            b[i] = l

        #for l in b:
        #    print(l)

        text = "\n".join(b)
        global_namespace = {
            "__file__": "generated",
            "__name__": "__main__",
        }

        global_namespace.update(vars_dict)

        text_a, text_b = text.split("=", 1)
        text = "result = " + text_b
        exec(compile(text, "generated", 'exec'), global_namespace)
        # print(global_namespace["result"])
        blocks_py.append((comment, global_namespace["result"]))

    # ---------------------
    # Now convert into rst.
    fout = open(OUT_RST, 'w', encoding="utf-8")
    fw = fout.write
    fw(HEADER)
    for comment, b in blocks_py:
        args_in = None
        args_out = None
        for member in b[1:]:
            if type(member) == tuple:
                if args_in is None:
                    args_in = member
                elif args_out is None:
                    args_out = member
                    break

        args_in_index = []
        args_out_index = []

        if args_in is not None:
            args_in_index[:] = [i for (i, a) in enumerate(args_in) if type(a) == tuple]
        if args_out is not None:
            args_out_index[:] = [i for (i, a) in enumerate(args_out) if type(a) == tuple]

        fw(".. function:: %s(bm, %s)\n\n" % (b[0], ", ".join([args_in[i][0] for i in args_in_index])))

        # -- wash the comment
        comment_washed = []
        for i, l in enumerate(comment):
            assert((l.strip() == "") or
                   (l in {"/*", " *"}) or
                   (l.startswith(("/* ", " * "))))

            l = l[3:]
            if i == 0 and not l.strip():
                continue
            if l.strip():
                l = "   " + l
            comment_washed.append(l)

        fw("\n".join(comment_washed))
        fw("\n")
        # -- done

        # get the args
        def get_args_wash(args, args_index, is_ret):
            args_wash = []
            for i in args_index:
                arg = args[i]
                if len(arg) == 3:
                    name, tp, tp_sub = arg
                elif len(arg) == 2:
                    name, tp = arg
                    tp_sub = None
                else:
                    print(arg)
                    assert(0)

                tp_str = ""

                comment_prev = ""
                comment_next = ""
                if i != 0:
                    comment_prev = args[i + 1]
                    if type(comment_prev) == str and comment_prev.startswith("our <"):
                        comment_prev = comment_next[5:-1]  # strip inline <...>
                    else:
                        comment_prev = ""

                if i + 1 < len(args):
                    comment_next = args[i + 1]
                    if type(comment_next) == str and comment_next.startswith("inline <"):
                        comment_next = comment_next[8:-1]  # strip inline <...>
                    else:
                        comment_next = ""

                comment = ""
                if comment_prev:
                    comment += comment_prev.strip()
                if comment_next:
                    comment += ("\n" if comment_prev else "") + comment_next.strip()

                if tp == BMO_OP_SLOT_FLT:
                    tp_str = "float"
                elif tp == BMO_OP_SLOT_INT:
                    tp_str = "int"
                elif tp == BMO_OP_SLOT_BOOL:
                    tp_str = "bool"
                elif tp == BMO_OP_SLOT_MAT:
                    tp_str = ":class:`mathutils.Matrix`"
                elif tp == BMO_OP_SLOT_VEC:
                    tp_str = ":class:`mathutils.Vector`"
                    if not is_ret:
                        tp_str += " or any sequence of 3 floats"
                elif tp == BMO_OP_SLOT_PTR:
                    tp_str = "dict"
                    assert(tp_sub is not None)
                    if tp_sub == BMO_OP_SLOT_SUBTYPE_PTR_BMESH:
                        tp_str = ":class:`bmesh.types.BMesh`"
                    elif tp_sub == BMO_OP_SLOT_SUBTYPE_PTR_SCENE:
                        tp_str = ":class:`bpy.types.Scene`"
                    elif tp_sub == BMO_OP_SLOT_SUBTYPE_PTR_OBJECT:
                        tp_str = ":class:`bpy.types.Object`"
                    elif tp_sub == BMO_OP_SLOT_SUBTYPE_PTR_MESH:
                        tp_str = ":class:`bpy.types.Mesh`"
                    else:
                        print("Cant find", vars_dict_reverse[tp_sub])
                        assert(0)

                elif tp == BMO_OP_SLOT_ELEMENT_BUF:
                    assert(tp_sub is not None)

                    ls = []
                    if tp_sub & BM_VERT:
                        ls.append(":class:`bmesh.types.BMVert`")
                    if tp_sub & BM_EDGE:
                        ls.append(":class:`bmesh.types.BMEdge`")
                    if tp_sub & BM_FACE:
                        ls.append(":class:`bmesh.types.BMFace`")
                    assert(ls)  # must be at least one

                    if tp_sub & BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE:
                        tp_str = "/".join(ls)
                    else:
                        tp_str = ("list of (%s)" % ", ".join(ls))

                    del ls
                elif tp == BMO_OP_SLOT_MAPPING:
                    if tp_sub & BMO_OP_SLOT_SUBTYPE_MAP_EMPTY:
                        tp_str = "set of vert/edge/face type"
                    else:
                        tp_str = "dict mapping vert/edge/face types to "
                        if tp_sub == BMO_OP_SLOT_SUBTYPE_MAP_BOOL:
                            tp_str += "bool"
                        elif tp_sub == BMO_OP_SLOT_SUBTYPE_MAP_INT:
                            tp_str += "int"
                        elif tp_sub == BMO_OP_SLOT_SUBTYPE_MAP_FLT:
                            tp_str += "float"
                        elif tp_sub == BMO_OP_SLOT_SUBTYPE_MAP_ELEM:
                            tp_str += ":class:`bmesh.types.BMVert`/:class:`bmesh.types.BMEdge`/:class:`bmesh.types.BMFace`"
                        elif tp_sub == BMO_OP_SLOT_SUBTYPE_MAP_INTERNAL:
                            tp_str += "unknown internal data, not compatible with python"
                        else:
                            print("Cant find", vars_dict_reverse[tp_sub])
                            assert(0)
                else:
                    print("Cant find", vars_dict_reverse[tp])
                    assert(0)

                args_wash.append((name, tp_str, comment))
            return args_wash
        # end get_args_wash

        # all ops get this arg
        fw("   :arg bm: The bmesh to operate on.\n")
        fw("   :type bm: :class:`bmesh.types.BMesh`\n")

        args_in_wash = get_args_wash(args_in, args_in_index, False)
        args_out_wash = get_args_wash(args_out, args_out_index, True)

        for (name, tp, comment) in args_in_wash:
            if comment == "":
                comment = "Undocumented."

            fw("   :arg %s: %s\n" % (name, comment))
            fw("   :type %s: %s\n" % (name, tp))

        if args_out_wash:
            fw("   :return:\n\n")

            for (name, tp, comment) in args_out_wash:
                assert(name.endswith(".out"))
                name = name[:-4]
                fw("      - ``%s``: %s\n\n" % (name, comment))
                fw("        **type** %s\n" % tp)

            fw("\n")
            fw("   :rtype: dict with string keys\n")

        fw("\n\n")

    fout.close()
    del fout
    print(OUT_RST)


if __name__ == "__main__":
    main()
