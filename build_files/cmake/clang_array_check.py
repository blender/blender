# ---
# * Licensed under the Apache License, Version 2.0 (the "License");
# * you may not use this file except in compliance with the License.
# * You may obtain a copy of the License at
# *
# * http://www.apache.org/licenses/LICENSE-2.0
# ---
# by Campbell Barton

"""
Invocation:

   export CLANG_BIND_DIR="/dsk/src/llvm/tools/clang/bindings/python"
   export CLANG_LIB_DIR="/opt/llvm/lib"

   python2 clang_array_check.py somefile.c -DSOME_DEFINE -I/some/include

... defines and includes are optional

"""

# delay parsing functions until we need them
USE_LAZY_INIT = True
USE_EXACT_COMPARE = False

# -----------------------------------------------------------------------------
# predefined function/arg sizes, handy sometimes, but not complete...

defs_precalc = {
    "glColor3bv": {0: 3},
    "glColor4bv": {0: 4},

    "glColor3ubv": {0: 3},
    "glColor4ubv": {0: 4},

    "glColor4usv": {0: 3},
    "glColor4usv": {0: 4},

    "glColor3fv": {0: 3},
    "glColor4fv": {0: 4},

    "glColor3dv": {0: 3},
    "glColor4dv": {0: 4},

    "glVertex2fv": {0: 2},
    "glVertex3fv": {0: 3},
    "glVertex4fv": {0: 4},

    "glEvalCoord1fv": {0: 1},
    "glEvalCoord1dv": {0: 1},
    "glEvalCoord2fv": {0: 2},
    "glEvalCoord2dv": {0: 2},

    "glRasterPos2dv": {0: 2},
    "glRasterPos3dv": {0: 3},
    "glRasterPos4dv": {0: 4},

    "glRasterPos2fv": {0: 2},
    "glRasterPos3fv": {0: 3},
    "glRasterPos4fv": {0: 4},

    "glRasterPos2sv": {0: 2},
    "glRasterPos3sv": {0: 3},
    "glRasterPos4sv": {0: 4},

    "glTexCoord2fv": {0: 2},
    "glTexCoord3fv": {0: 3},
    "glTexCoord4fv": {0: 4},

    "glTexCoord2dv": {0: 2},
    "glTexCoord3dv": {0: 3},
    "glTexCoord4dv": {0: 4},

    "glNormal3fv": {0: 3},
    "glNormal3dv": {0: 3},
    "glNormal3bv": {0: 3},
    "glNormal3iv": {0: 3},
    "glNormal3sv": {0: 3},
}

# -----------------------------------------------------------------------------

import sys

if 0:
    # Examples with LLVM as the root dir: '/dsk/src/llvm'

    # path containing 'clang/__init__.py'
    CLANG_BIND_DIR = "/dsk/src/llvm/tools/clang/bindings/python"

    # path containing libclang.so
    CLANG_LIB_DIR = "/opt/llvm/lib"
else:
    import os
    CLANG_BIND_DIR = os.environ.get("CLANG_BIND_DIR")
    CLANG_LIB_DIR = os.environ.get("CLANG_LIB_DIR")

    if CLANG_BIND_DIR is None:
        print("$CLANG_BIND_DIR python binding dir not set")
    if CLANG_LIB_DIR is None:
        print("$CLANG_LIB_DIR clang lib dir not set")

sys.path.append(CLANG_BIND_DIR)

import clang
import clang.cindex
from clang.cindex import (CursorKind,
                          TypeKind,
                          TokenKind)

clang.cindex.Config.set_library_path(CLANG_LIB_DIR)

index = clang.cindex.Index.create()

args = sys.argv[2:]
# print(args)

tu = index.parse(sys.argv[1], args)
# print('Translation unit: %s' % tu.spelling)
filepath = tu.spelling

# -----------------------------------------------------------------------------


def function_parm_wash_tokens(parm):
    # print(parm.kind)
    assert parm.kind in (CursorKind.PARM_DECL,
                         CursorKind.VAR_DECL,  # XXX, double check this
                         CursorKind.FIELD_DECL,
                         )

    """
    Return tolens without trailing commands and 'const'
    """

    tokens = [t for t in parm.get_tokens()]
    if not tokens:
        return tokens

    # if tokens[-1].kind == To
    # remove trailing char
    if tokens[-1].kind == TokenKind.PUNCTUATION:
        if tokens[-1].spelling in (",", ")", ";"):
            tokens.pop()
        # else:
        #     print(tokens[-1].spelling)

    t_new = []
    for t in tokens:
        t_kind = t.kind
        t_spelling = t.spelling
        ok = True
        if t_kind == TokenKind.KEYWORD:
            if t_spelling in ("const", "restrict", "volatile"):
                ok = False
            elif t_spelling.startswith("__"):
                ok = False  # __restrict
        elif t_kind in (TokenKind.COMMENT, ):
            ok = False

            # Use these
        elif t_kind in (TokenKind.LITERAL,
                        TokenKind.PUNCTUATION,
                        TokenKind.IDENTIFIER):
            # use but ignore
            pass

        else:
            print("Unknown!", t_kind, t_spelling)

        # if its OK we will add
        if ok:
            t_new.append(t)
    return t_new


def parm_size(node_child):
    tokens = function_parm_wash_tokens(node_child)

    # print(" ".join([t.spelling for t in tokens]))

    # NOT PERFECT CODE, EXTRACT SIZE FROM TOKENS
    if len(tokens) >= 3:  # foo [ 1 ]
        if      ((tokens[-3].kind == TokenKind.PUNCTUATION and tokens[-3].spelling == "[") and
                 (tokens[-2].kind == TokenKind.LITERAL and tokens[-2].spelling.isdigit()) and
                 (tokens[-1].kind == TokenKind.PUNCTUATION and tokens[-1].spelling == "]")):
            # ---
            return int(tokens[-2].spelling)
    return -1


def function_get_arg_sizes(node):
    # Return a dict if (index: size) items
    # {arg_indx: arg_array_size, ... ]
    arg_sizes = {}

    if 1:  # node.spelling == "BM_vert_create", for debugging
        node_parms = [node_child for node_child in node.get_children()
                      if node_child.kind == CursorKind.PARM_DECL]

        for i, node_child in enumerate(node_parms):

            # print(node_child.kind, node_child.spelling)
            # print(node_child.type.kind, node_child.spelling)
            if node_child.type.kind == TypeKind.CONSTANTARRAY:
                pointee = node_child.type.get_pointee()
                size = parm_size(node_child)
                if size != -1:
                    arg_sizes[i] = size

    return arg_sizes


# -----------------------------------------------------------------------------
_defs = {}


def lookup_function_size_def(func_id):
    if USE_LAZY_INIT:
        result = _defs.get(func_id, {})
        if type(result) != dict:
            result = _defs[func_id] = function_get_arg_sizes(result)
        return result
    else:
        return _defs.get(func_id, {})

# -----------------------------------------------------------------------------


def file_check_arg_sizes(tu):

    # main checking function
    def validate_arg_size(node):
        """
        Loop over args and validate sizes for args we KNOW the size of.
        """
        assert node.kind == CursorKind.CALL_EXPR

        if 0:
            print("---",
                  " <~> ".join(
                      [" ".join([t.spelling for t in C.get_tokens()])
                       for C in node.get_children()]
                  ))
        # print(node.location)

        # first child is the function call, skip that.
        children = list(node.get_children())

        if not children:
            return  # XXX, look into this, happens on C++

        func = children[0]

        # get the func declaration!
        # works but we can better scan for functions ahead of time.
        if 0:
            func_dec = func.get_definition()
            if func_dec:
                print("FD", " ".join([t.spelling for t in func_dec.get_tokens()]))
            else:
                # HRMP'f - why does this fail?
                print("AA", " ".join([t.spelling for t in node.get_tokens()]))
        else:
            args_size_definition = ()  # dummy

            # get the key
            tok = list(func.get_tokens())
            if tok:
                func_id = tok[0].spelling
                args_size_definition = lookup_function_size_def(func_id)

        if not args_size_definition:
            return

        children = children[1:]
        for i, node_child in enumerate(children):
            children = list(node_child.get_children())

            # skip if we dont have an index...
            size_def = args_size_definition.get(i, -1)

            if size_def == -1:
                continue

            # print([c.kind for c in children])
            # print(" ".join([t.spelling for t in node_child.get_tokens()]))

            if len(children) == 1:
                arg = children[0]
                if arg.kind in (CursorKind.DECL_REF_EXPR,
                                CursorKind.UNEXPOSED_EXPR):

                    if arg.type.kind == TypeKind.CONSTANTARRAY:
                        dec = arg.get_definition()
                        if dec:
                            size = parm_size(dec)

                            # size == 0 is for 'float *a'
                            if size != -1 and size != 0:

                                # nice print!
                                if 0:
                                    print("".join([t.spelling for t in func.get_tokens()]),
                                          i,
                                          " ".join([t.spelling for t in dec.get_tokens()]))

                                # testing
                                # size_def = 100
                                if size != 1:
                                    if USE_EXACT_COMPARE:
                                        # is_err = (size != size_def) and (size != 4 and size_def != 3)
                                        is_err = (size != size_def)
                                    else:
                                        is_err = (size < size_def)

                                    if is_err:
                                        location = node.location
                                        # if "math_color_inline.c" not in str(location.file):
                                        if 1:
                                            print("%s:%d:%d: argument %d is size %d, should be %d (from %s)" %
                                                  (location.file,
                                                   location.line,
                                                   location.column,
                                                   i + 1, size, size_def,
                                                   filepath  # always the same but useful when running threaded
                                                   ))

    # we dont really care what we are looking at, just scan entire file for
    # function calls.

    def recursive_func_call_check(node):
        if node.kind == CursorKind.CALL_EXPR:
            validate_arg_size(node)

        for c in node.get_children():
            recursive_func_call_check(c)

    recursive_func_call_check(tu.cursor)


# -- first pass, cache function definitions sizes

# PRINT FUNC DEFINES
def recursive_arg_sizes(node, ):
    # print(node.kind, node.spelling)
    if node.kind == CursorKind.FUNCTION_DECL:
        if USE_LAZY_INIT:
            args_sizes = node
        else:
            args_sizes = function_get_arg_sizes(node)
        # if args_sizes:
        #     print(node.spelling, args_sizes)
        _defs[node.spelling] = args_sizes
        # print("adding", node.spelling)
    for c in node.get_children():
        recursive_arg_sizes(c)
# cache function sizes
recursive_arg_sizes(tu.cursor)
_defs.update(defs_precalc)

# --- second pass, check against def's
file_check_arg_sizes(tu)
