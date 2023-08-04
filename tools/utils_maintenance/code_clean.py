#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Example:
  ./tools/utils/code_clean.py /src/cmake_debug --match ".*/editmesh_.*" --fix=use_const_vars

Note: currently this is limited to paths in "source/" and "intern/",
we could change this if it's needed.
"""

import argparse
import re
import subprocess
import sys
import os
import string

from typing import (
    Any,
    Dict,
    Generator,
    List,
    Optional,
    Sequence,
    Set,
    Tuple,
    Type,
)

# List of (source_file, all_arguments)
ProcessedCommands = List[Tuple[str, str]]

BASE_DIR = os.path.abspath(os.path.dirname(__file__))

SOURCE_DIR = os.path.normpath(os.path.join(BASE_DIR, "..", ".."))

# (id: doc-string) pairs.
VERBOSE_INFO = [
    (
        "compile", (
            "Print the compiler output (noisy).\n"
            "Try setting '--jobs=1' for usable output.\n"
        ),
    ),
    (
        "edit_actions", (
            "Print the result of each attempted edit, useful for troubleshooting:\n"
            "- Causes code not to compile.\n"
            "- Compiles but changes the resulting behavior.\n"
            "- Succeeds.\n"
        ),
    )
]


# -----------------------------------------------------------------------------
# Generic Constants

# Sorted numeric types.
# Intentionally missing are "unsigned".
BUILT_IN_NUMERIC_TYPES = (
    "bool",
    "char",
    "char32_t",
    "double",
    "float",
    "int",
    "int16_t",
    "int32_t",
    "int64_t",
    "int8_t",
    "intptr_t",
    "long",
    "off_t",
    "ptrdiff_t",
    "short",
    "size_t",
    "ssize_t",
    "uchar",
    "uint",
    "uint16_t",
    "uint32_t",
    "uint64_t",
    "uint8_t",
    "uintptr_t",
    "ulong",
    "ushort",
)

IDENTIFIER_CHARS = set(string.ascii_letters + "_" + string.digits)


# -----------------------------------------------------------------------------
# General Utilities

# Note that we could use a hash, however there is no advantage, compare its contents.
def file_as_bytes(filename: str) -> bytes:
    with open(filename, 'rb') as fh:
        return fh.read()


def line_from_span(text: str, start: int, end: int) -> str:
    while start > 0 and text[start - 1] != '\n':
        start -= 1
    while end < len(text) and text[end] != '\n':
        end += 1
    return text[start:end]


def files_recursive_with_ext(path: str, ext: Tuple[str, ...]) -> Generator[str, None, None]:
    for dirpath, dirnames, filenames in os.walk(path):
        # skip '.git' and other dot-files.
        dirnames[:] = [d for d in dirnames if not d.startswith(".")]
        for filename in filenames:
            if filename.endswith(ext):
                yield os.path.join(dirpath, filename)


def text_matching_bracket_forward(
        data: str,
        pos_beg: int,
        pos_limit: int,
        beg_bracket: str,
        end_bracket: str,
) -> int:
    """
    Return the matching bracket or -1.

    .. note:: This is not sophisticated, brackets in strings will confuse the function.
    """
    level = 1

    # The next bracket.
    pos = pos_beg + 1

    # Clamp the limit.
    limit = min(pos_beg + pos_limit, len(data))

    while pos < limit:
        c = data[pos]
        if c == beg_bracket:
            level += 1
        elif c == end_bracket:
            level -= 1
            if level == 0:
                return pos
        pos += 1
    return -1


def text_matching_bracket_backward(
        data: str,
        pos_end: int,
        pos_limit: int,
        beg_bracket: str,
        end_bracket: str,
) -> int:
    """
    Return the matching bracket or -1.

    .. note:: This is not sophisticated, brackets in strings will confuse the function.
    """
    level = 1

    # The next bracket.
    pos = pos_end - 1

    # Clamp the limit.
    limit = max(0, pos_limit)

    while pos >= limit:
        c = data[pos]
        if c == end_bracket:
            level += 1
        elif c == beg_bracket:
            level -= 1
            if level == 0:
                return pos
        pos -= 1
    return -1


def text_prev_bol(data: str, pos: int, limit: int) -> int:
    if pos == 0:
        return pos
    # Already at the bounds.
    if data[pos - 1] == "\n":
        return pos
    pos_next = data.rfind("\n", limit, pos)
    if pos_next == -1:
        return limit
    # We don't want to include the newline.
    return pos_next + 1


def text_next_eol(data: str, pos: int, limit: int, step_over: bool) -> int:
    """
    Extend ``pos`` to just before the next EOL, otherwise EOF.
    As this is intended for use as a range, ``data[pos]``
    will either be ``\n`` or equal to out of range (equal to ``len(data)``).
    """
    if pos + 1 >= len(data):
        return pos
    # Already at the bounds.
    if data[pos] == "\n":
        return pos + (1 if step_over else 0)
    pos_next = data.find("\n", pos, limit)
    if pos_next == -1:
        return limit
    return pos_next + (1 if step_over else 0)


def text_prev_eol_nonblank(data: str, pos: int, limit: int) -> int:
    """
    Return the character immediately before the previous lines new-line,
    stepping backwards over any trailing tab or space characters.
    """
    if pos == 0:
        return pos
    # Already at the bounds.
    pos_next = data.rfind("\n", limit, pos)
    if pos_next == -1:
        return limit
    # Step over the newline.
    pos_next -= 1
    if pos_next <= limit:
        return pos_next
    while pos_next > limit and data[pos_next] in " \t":
        pos_next -= 1
    return pos_next


# -----------------------------------------------------------------------------
# General C/C++ Source Code Checks

RE_DEFINE = re.compile(r"\s*#\s*define\b")


def text_cxx_in_macro_definition(data: str, pos: int) -> bool:
    """
    Return true when ``pos`` is inside a macro (including multi-line macros).
    """
    pos_bol = text_prev_bol(data, pos, 0)
    pos_eol = text_next_eol(data, pos, len(data), False)
    if RE_DEFINE.match(data[pos_bol:pos_eol]):
        return True
    while (pos_eol_prev := text_prev_eol_nonblank(data, pos_bol, 0)) != pos_bol:
        if data[pos_eol_prev] != "\\":
            break
        pos_bol = text_prev_bol(data, pos_eol_prev + 1, 0)
        # Otherwise keep checking if this is part of a macro.
        if RE_DEFINE.match(data[pos_bol:pos_eol_prev]):
            return True

    return False


# -----------------------------------------------------------------------------
# Execution Wrappers

def run(
        args: Sequence[str],
        *,
        cwd: Optional[str],
        quiet: bool,
        verbose_compile: bool,
) -> int:
    if verbose_compile and not quiet:
        out = sys.stdout.fileno()
    else:
        out = subprocess.DEVNULL

    with subprocess.Popen(
            args,
            stdout=out,
            stderr=out,
            cwd=cwd,
    ) as proc:
        proc.wait()
        return proc.returncode


# -----------------------------------------------------------------------------
# Build System Access

def cmake_cache_var(cmake_dir: str, var: str) -> Optional[str]:
    with open(os.path.join(cmake_dir, "CMakeCache.txt"), encoding='utf-8') as cache_file:
        lines = [
            l_strip for l in cache_file
            if (l_strip := l.strip())
            if not l_strip.startswith(("//", "#"))
        ]

    for l in lines:
        if l.split(":")[0] == var:
            return l.split("=", 1)[-1]
    return None


def cmake_cache_var_is_true(cmake_var: Optional[str]) -> bool:
    if cmake_var is None:
        return False

    cmake_var = cmake_var.upper()
    if cmake_var in {"ON", "YES", "TRUE", "Y"}:
        return True
    if cmake_var.isdigit() and cmake_var != "0":
        return True

    return False


RE_CFILE_SEARCH = re.compile(r"\s\-c\s([\S]+)")


def process_commands(cmake_dir: str, data: Sequence[str]) -> Optional[ProcessedCommands]:
    compiler_c = cmake_cache_var(cmake_dir, "CMAKE_C_COMPILER")
    compiler_cxx = cmake_cache_var(cmake_dir, "CMAKE_CXX_COMPILER")
    if compiler_c is None:
        sys.stderr.write("Can't find C compiler in %r\n" % cmake_dir)
        return None
    if compiler_cxx is None:
        sys.stderr.write("Can't find C++ compiler in %r\n" % cmake_dir)
        return None

    # Check for unsupported configurations.
    for arg in ("WITH_UNITY_BUILD", "WITH_COMPILER_CCACHE"):
        if cmake_cache_var_is_true(cmake_cache_var(cmake_dir, arg)):
            sys.stderr.write("The option '%s' must be disabled for proper functionality\n" % arg)
            return None

    file_args = []

    for l in data:
        if (
                (compiler_c in l) or
                (compiler_cxx in l)
        ):
            # Extract:
            #   -c SOME_FILE
            c_file_search = re.search(RE_CFILE_SEARCH, l)
            if c_file_search is not None:
                c_file = c_file_search.group(1)
                file_args.append((c_file, l))
            else:
                # could print, NO C FILE FOUND?
                pass

    file_args.sort()

    return file_args


def find_build_args_ninja(build_dir: str) -> Optional[ProcessedCommands]:
    import time
    cmake_dir = build_dir
    make_exe = "ninja"
    with subprocess.Popen(
        [make_exe, "-t", "commands"],
        stdout=subprocess.PIPE,
        cwd=build_dir,
    ) as proc:
        while proc.poll():
            time.sleep(1)
        assert proc.stdout is not None

        out = proc.stdout.read()
        proc.stdout.close()
        # print("done!", len(out), "bytes")
        data = out.decode("utf-8", errors="ignore").split("\n")
    return process_commands(cmake_dir, data)


def find_build_args_make(build_dir: str) -> Optional[ProcessedCommands]:
    import time
    make_exe = "make"
    with subprocess.Popen(
        [make_exe, "--always-make", "--dry-run", "--keep-going", "VERBOSE=1"],
        stdout=subprocess.PIPE,
        cwd=build_dir,
    ) as proc:
        while proc.poll():
            time.sleep(1)
        assert proc.stdout is not None

        out = proc.stdout.read()
        proc.stdout.close()

        # print("done!", len(out), "bytes")
        data = out.decode("utf-8", errors="ignore").split("\n")
    return process_commands(build_dir, data)


# -----------------------------------------------------------------------------
# Create Edit Lists

# Create an edit list from a file, in the format:
#
#    [((start_index, end_index), text_to_replace), ...]
#
# Note that edits should not overlap, in the _very_ rare case overlapping edits are needed,
# this could be run multiple times on the same code-base.
#
# Although this seems like it's not a common use-case.

from collections import namedtuple
Edit = namedtuple(
    "Edit", (
        # Keep first, for sorting.
        "span",

        "content",
        "content_fail",

        # Optional.
        "extra_build_args",
    ),

    defaults=(
        # `extra_build_args`.
        None,
    )
)
del namedtuple


class EditGenerator:
    __slots__ = ()

    # Each subclass must also a default boolean: `is_default`.
    # When false, a detailed explanation must be included for why.
    #
    # Declare here to quiet `mypy` warning, `__init_subclass__` ensures this value is never used.
    # This is done so the creator of edit is forced to make a decision on the reliability of the edit
    # and document why it might need manual checking.
    is_default = False

    @classmethod
    def __init_subclass__(cls) -> None:
        # Ensure the sub-class declares this.
        if (not isinstance(getattr(cls, "is_default", None), bool)) or ("is_default" not in cls.__dict__):
            raise Exception("Class %r missing \"is_default\" boolean!" % cls)
        if getattr(cls, "edit_list_from_file") is EditGenerator.edit_list_from_file:
            raise Exception("Class %r missing \"edit_list_from_file\" callback!" % cls)

    def __new__(cls, *args: Tuple[Any], **kwargs: Dict[str, Any]) -> Any:
        raise RuntimeError("%s should not be instantiated" % cls)

    @staticmethod
    def edit_list_from_file(_source: str, _data: str, _shared_edit_data: Any) -> List[Edit]:
        # The `__init_subclass__` function ensures this is always overridden.
        raise RuntimeError("This function must be overridden by it's subclass!")
        return []

    @staticmethod
    def setup() -> Any:
        return None

    @staticmethod
    def teardown(_shared_edit_data: Any) -> None:
        pass


class edit_generators:
    # fake module.

    class sizeof_fixed_array(EditGenerator):
        """
        Use fixed size array syntax with `sizeof`:

        Replace:
          sizeof(float) * 4 * 4
        With:
          sizeof(float[4][4])
        """

        # Not default because there are times when the literal sizes don't represent extra dimensions on an array,
        # where making this edit would be misleading as it would indicate a matrix (for e.g.) when a vector is intended.
        is_default = False

        @staticmethod
        def edit_list_from_file(_source: str, data: str, _shared_edit_data: Any) -> List[Edit]:
            edits = []

            for match in re.finditer(r"sizeof\(([a-zA-Z_]+)\) \* (\d+) \* (\d+)", data):
                edits.append(Edit(
                    span=match.span(),
                    content='sizeof(%s[%s][%s])' % (match.group(1), match.group(2), match.group(3)),
                    content_fail='__ALWAYS_FAIL__',
                ))

            for match in re.finditer(r"sizeof\(([a-zA-Z_]+)\) \* (\d+)", data):
                edits.append(Edit(
                    span=match.span(),
                    content='sizeof(%s[%s])' % (match.group(1), match.group(2)),
                    content_fail='__ALWAYS_FAIL__',
                ))

            for match in re.finditer(r"\b(\d+) \* sizeof\(([a-zA-Z_]+)\)", data):
                edits.append(Edit(
                    span=match.span(),
                    content='sizeof(%s[%s])' % (match.group(2), match.group(1)),
                    content_fail='__ALWAYS_FAIL__',
                ))
            return edits

    class use_const(EditGenerator):
        """
        Use const variables:

        Replace:
          float abc[3] = {0, 1, 2};
        With:
          const float abc[3] = {0, 1, 2};

        Replace:
          float abc[3]
        With:
          const float abc[3]

        As well as casts.

        Replace:
          (float *)
        With:
          (const float *)

        Replace:
          (float (*))
        With:
          (const float (*))
        """

        # Non-default because pre-processor defines can cause `const` variables on some platforms
        # to be non `const` on others.
        is_default = False

        @staticmethod
        def edit_list_from_file(_source: str, data: str, _shared_edit_data: Any) -> List[Edit]:
            edits = []

            # `float abc[3] = {0, 1, 2};` -> `const float abc[3] = {0, 1, 2};`
            for match in re.finditer(r"(\(|, |  )([a-zA-Z_0-9]+ [a-zA-Z_0-9]+\[)\b([^\n]+ = )", data):
                edits.append(Edit(
                    span=match.span(),
                    content='%s const %s%s' % (match.group(1), match.group(2), match.group(3)),
                    content_fail='__ALWAYS_FAIL__',
                ))

            # `float abc[3]` -> `const float abc[3]`
            for match in re.finditer(r"(\(|, )([a-zA-Z_0-9]+ [a-zA-Z_0-9]+\[)", data):
                edits.append(Edit(
                    span=match.span(),
                    content='%s const %s' % (match.group(1), match.group(2)),
                    content_fail='__ALWAYS_FAIL__',
                ))

            # `(float *)`      -> `(const float *)`
            # `(float (*))`    -> `(const float (*))`
            # `(float (*)[4])` -> `(const float (*)[4])`
            for match in re.finditer(
                    r"(\()"
                    r"([a-zA-Z_0-9]+\s*)"
                    r"(\*+\)|\(\*+\))"
                    r"(|\[[a-zA-Z_0-9]+\])",
                    data,
            ):
                edits.append(Edit(
                    span=match.span(),
                    content='%sconst %s%s%s' % (match.group(1), match.group(2), match.group(3), match.group(4)),
                    content_fail='__ALWAYS_FAIL__',
                ))

            return edits

    class use_zero_before_float_suffix(EditGenerator):
        """
        Use zero before the float suffix.

        Replace:
          1.f
        With:
          1.0f

        Replace:
          1.0F
        With:
          1.0f
        """
        is_default = True

        @staticmethod
        def edit_list_from_file(_source: str, data: str, _shared_edit_data: Any) -> List[Edit]:
            edits = []

            # `1.f` -> `1.0f`
            for match in re.finditer(r"\b(\d+)\.([fF])\b", data):
                edits.append(Edit(
                    span=match.span(),
                    content='%s.0%s' % (match.group(1), match.group(2)),
                    content_fail='__ALWAYS_FAIL__',
                ))

            # `1.0F` -> `1.0f`
            for match in re.finditer(r"\b(\d+\.\d+)F\b", data):
                edits.append(Edit(
                    span=match.span(),
                    content='%sf' % (match.group(1),),
                    content_fail='__ALWAYS_FAIL__',
                ))

            return edits

    class use_brief_types(EditGenerator):
        """
        Use less verbose unsigned types.

        Replace:
          unsigned int
        With:
          uint
        """
        is_default = True

        @staticmethod
        def edit_list_from_file(_source: str, data: str, _shared_edit_data: Any) -> List[Edit]:
            edits = []

            # Keep `typedef` unsigned as some files have local types, e.g.
            #    `typedef unsigned int uint;`
            # Should not be changed to:
            #    `typedef uint uint;`
            # ... even if it happens to compile - because it may cause problems on other platforms
            # that don't have `uint` defined.
            span_skip = set()
            for match in re.finditer(r"\btypedef\s+(unsigned)\b", data):
                span_skip.add(match.span(1))

            # `unsigned char` -> `uchar`.
            for match in re.finditer(r"(unsigned)\s+([a-z]+)", data):
                if match.span(1) in span_skip:
                    continue

                edits.append(Edit(
                    span=match.span(),
                    content='u%s' % match.group(2),
                    content_fail='__ALWAYS_FAIL__',
                ))

            # There may be some remaining uses of `unsigned` without any integer type afterwards.
            # `unsigned` -> `uint`.
            for match in re.finditer(r"\b(unsigned)\b", data):
                if match.span(1) in span_skip:
                    continue

                edits.append(Edit(
                    span=match.span(),
                    content='uint',
                    content_fail='__ALWAYS_FAIL__',
                ))

            return edits

    class use_nullptr(EditGenerator):
        """
        Use ``nullptr`` instead of ``NULL`` for C++ code.

        Replace:
          NULL
        With:
          nullptr
        """
        is_default = True

        @staticmethod
        def edit_list_from_file(source: str, data: str, _shared_edit_data: Any) -> List[Edit]:
            edits: List[Edit] = []

            # The user might include C & C++, if they forget, it is better not to operate on C.
            if source.lower().endswith((".h", ".c")):
                return edits

            # `NULL` -> `nullptr`.
            for match in re.finditer(r"\bNULL\b", data):
                edits.append(Edit(
                    span=match.span(),
                    content='nullptr',
                    content_fail='__ALWAYS_FAIL__',
                ))

            # There may be some remaining uses of `unsigned` without any integer type afterwards.
            # `unsigned` -> `uint`.
            for match in re.finditer(r"\bunsigned\b", data):
                edits.append(Edit(
                    span=match.span(),
                    content='uint',
                    content_fail='__ALWAYS_FAIL__',
                ))

            return edits

    class use_empty_void_arg(EditGenerator):
        """
        Use ``()`` instead of ``(void)`` for C++ code.

        Replace:
          function(void) {}
        With:
          function() {}
        """
        is_default = True

        @staticmethod
        def edit_list_from_file(source: str, data: str, _shared_edit_data: Any) -> List[Edit]:
            edits: List[Edit] = []

            # The user might include C & C++, if they forget, it is better not to operate on C.
            if source.lower().endswith((".h", ".c")):
                return edits

            # `(void)` -> `()`.
            for match in re.finditer(r"(\(void\))(\s*{)", data, flags=re.MULTILINE):
                edits.append(Edit(
                    span=match.span(),
                    content="()" + match.group(2),
                    content_fail="(__ALWAYS_FAIL__) {",
                ))
            return edits

    class unused_arg_as_comment(EditGenerator):
        """
        Replace `UNUSED(argument)` in C++ code.

        Replace:
          void function(int UNUSED(arg)) {...}
        With:
          void function(int /*arg*/) {...}
        """
        is_default = True

        @staticmethod
        def edit_list_from_file(source: str, data: str, _shared_edit_data: Any) -> List[Edit]:
            edits: List[Edit] = []

            # The user might include C & C++, if they forget, it is better not to operate on C.
            if source.lower().endswith((".h", ".c")):
                return edits

            # `UNUSED(arg)` -> `/*arg*/`.
            for match in re.finditer(
                    r"\b(UNUSED)"
                    # # Opening parenthesis.
                    r"\("
                    # Capture the identifier as group 1.
                    r"([" + "".join(list(IDENTIFIER_CHARS)) + "]+)"
                    # # Capture any non-identifier characters as group 2.
                    # (e.g. `[3]`) which need to be added outside the comment.
                    r"([^\)]*)"
                    # Closing parenthesis of `UNUSED(..)`.
                    r"\)",
                    data,
            ):
                edits.append(Edit(
                    span=match.span(),
                    content='/*%s*/%s' % (match.group(2), match.group(3)),
                    content_fail='__ALWAYS_FAIL__(%s%s)' % (match.group(2), match.group(3)),
                ))

            return edits

    class use_elem_macro(EditGenerator):
        """
        Use the `ELEM` macro for more abbreviated expressions.

        Replace:
          (a == b || a == c)
          (a != b && a != c)
        With:
          (ELEM(a, b, c))
          (!ELEM(a, b, c))
        """
        is_default = True

        @staticmethod
        def edit_list_from_file(_source: str, data: str, _shared_edit_data: Any) -> List[Edit]:
            edits = []

            for use_brackets in (True, False):

                test_equal = (
                    r'([^\|\(\)]+)'  # group 1 (no (|))
                    r'\s+==\s+'
                    r'([^\|\(\)]+)'  # group 2 (no (|))
                )

                test_not_equal = (
                    r'([^\|\(\)]+)'  # group 1 (no (|))
                    r'\s+!=\s+'
                    r'([^\|\(\)]+)'  # group 2 (no (|))
                )

                if use_brackets:
                    test_equal = r'\(' + test_equal + r'\)'
                    test_not_equal = r'\(' + test_not_equal + r'\)'

                for is_equal in (True, False):
                    for n in reversed(range(2, 64)):
                        if is_equal:
                            re_str = r'\(' + r'\s+\|\|\s+'.join([test_equal] * n) + r'\)'
                        else:
                            re_str = r'\(' + r'\s+\&\&\s+'.join([test_not_equal] * n) + r'\)'

                        for match in re.finditer(re_str, data):
                            var = match.group(1)
                            var_rest = []
                            groups = match.groups()
                            groups_paired = [(groups[i * 2], groups[i * 2 + 1]) for i in range(len(groups) // 2)]
                            found = True
                            for a, b in groups_paired:
                                # Unlikely but possible the checks are swapped.
                                if b == var and a != var:
                                    a, b = b, a

                                if a != var:
                                    found = False
                                    break
                                var_rest.append(b)

                            if found:
                                edits.append(Edit(
                                    span=match.span(),
                                    content='(%sELEM(%s, %s))' % (
                                        ('' if is_equal else '!'),
                                        var,
                                        ', '.join(var_rest),
                                    ),
                                    # Use same expression otherwise this can change values
                                    # inside assert when it shouldn't.
                                    content_fail='(%s__ALWAYS_FAIL__(%s, %s))' % (
                                        ('' if is_equal else '!'),
                                        var,
                                        ', '.join(var_rest),
                                    ),
                                ))

            return edits

    class use_str_elem_macro(EditGenerator):
        """
        Use `STR_ELEM` macro:

        Replace:
          (STREQ(a, b) || STREQ(a, c))
        With:
          (STR_ELEM(a, b, c))
        """
        is_default = True

        @staticmethod
        def edit_list_from_file(_source: str, data: str, _shared_edit_data: Any) -> List[Edit]:
            edits = []

            for use_brackets in (True, False):

                test_equal = (
                    r'STREQ'
                    r'\('
                    r'([^\|\(\),]+)'  # group 1 (no (|,))
                    r',\s+'
                    r'([^\|\(\),]+)'  # group 2 (no (|,))
                    r'\)'
                )

                test_not_equal = (
                    '!'  # Only difference.
                    r'STREQ'
                    r'\('
                    r'([^\|\(\),]+)'  # group 1 (no (|,))
                    r',\s+'
                    r'([^\|\(\),]+)'  # group 2 (no (|,))
                    r'\)'
                )

                if use_brackets:
                    test_equal = r'\(' + test_equal + r'\)'
                    test_not_equal = r'\(' + test_not_equal + r'\)'

                for is_equal in (True, False):
                    for n in reversed(range(2, 64)):
                        if is_equal:
                            re_str = r'\(' + r'\s+\|\|\s+'.join([test_equal] * n) + r'\)'
                        else:
                            re_str = r'\(' + r'\s+\&\&\s+'.join([test_not_equal] * n) + r'\)'

                        for match in re.finditer(re_str, data):
                            var = match.group(1)
                            var_rest = []
                            groups = match.groups()
                            groups_paired = [(groups[i * 2], groups[i * 2 + 1]) for i in range(len(groups) // 2)]
                            found = True
                            for a, b in groups_paired:
                                # Unlikely but possible the checks are swapped.
                                if b == var and a != var:
                                    a, b = b, a

                                if a != var:
                                    found = False
                                    break
                                var_rest.append(b)

                            if found:
                                edits.append(Edit(
                                    span=match.span(),
                                    content='(%sSTR_ELEM(%s, %s))' % (
                                        ('' if is_equal else '!'),
                                        var,
                                        ', '.join(var_rest),
                                    ),
                                    # Use same expression otherwise this can change values
                                    # inside assert when it shouldn't.
                                    content_fail='(%s__ALWAYS_FAIL__(%s, %s))' % (
                                        ('' if is_equal else '!'),
                                        var,
                                        ', '.join(var_rest),
                                    ),
                                ))

            return edits

    class use_const_vars(EditGenerator):
        """
        Use `const` where possible:

        Replace:
          float abc[3] = {0, 1, 2};
        With:
          const float abc[3] = {0, 1, 2};
        """

        # Non-default because pre-processor defines can cause `const` variables on some platforms
        # to be non `const` on others.
        is_default = False

        @staticmethod
        def edit_list_from_file(_source: str, data: str, _shared_edit_data: Any) -> List[Edit]:
            edits = []

            # for match in re.finditer(r"(  [a-zA-Z0-9_]+ [a-zA-Z0-9_]+ = [A-Z][A-Z_0-9_]*;)", data):
            #     edits.append(Edit(
            #         span=match.span(),
            #         content='const %s' % (match.group(1).lstrip()),
            #         content_fail='__ALWAYS_FAIL__',
            #     ))

            for match in re.finditer(r"(  [a-zA-Z0-9_]+ [a-zA-Z0-9_]+ = .*;)", data):
                edits.append(Edit(
                    span=match.span(),
                    content='const %s' % (match.group(1).lstrip()),
                    content_fail='__ALWAYS_FAIL__',
                ))

            return edits

    class remove_struct_qualifier(EditGenerator):
        """
        Remove redundant struct qualifiers:

        Replace:
          struct Foo
        With:
          Foo
        """
        is_default = True

        @staticmethod
        def edit_list_from_file(_source: str, data: str, _shared_edit_data: Any) -> List[Edit]:
            edits = []

            # Keep:
            # - `strucrt Foo;` (forward declaration).
            # - `struct Foo {` (declaration).
            # - `struct {` (declaration).
            # In these cases removing will cause a build error (which is technically "safe")
            # it just causes a lot of unnecessary code edits which always fail and slow down operation.
            span_skip = set()
            for match in re.finditer(r"\b(struct)\s+([a-zA-Z0-9_]+)?\s*({|;)", data):
                span_skip.add(match.span(1))

            # Remove `struct`
            for match in re.finditer(r"\b(struct)\s+[a-zA-Z0-9_]+", data):
                span = match.span(1)
                if span in span_skip:
                    continue

                edits.append(Edit(
                    span=span,
                    content=' ',
                    content_fail=' __ALWAYS_FAIL__ ',
                ))
            return edits

    class remove_return_parens(EditGenerator):
        """
        Remove redundant parenthesis around return arguments:

        Replace:
          return (value);
        With:
          return value;
        """
        is_default = True

        @staticmethod
        def edit_list_from_file(_source: str, data: str, _shared_edit_data: Any) -> List[Edit]:
            edits = []

            # Remove `return (NULL);`
            for match in re.finditer(r"return \(([a-zA-Z_0-9]+)\);", data):
                edits.append(Edit(
                    span=match.span(),
                    content='return %s;' % (match.group(1)),
                    content_fail='return __ALWAYS_FAIL__;',
                ))
            return edits

    class use_streq_macro(EditGenerator):
        """
        Use `STREQ` macro:

        Replace:
          strcmp(a, b) == 0
        With:
          STREQ(a, b)

        Replace:
          strcmp(a, b) != 0
        With:
          !STREQ(a, b)
        """
        is_default = True

        @staticmethod
        def edit_list_from_file(_source: str, data: str, _shared_edit_data: Any) -> List[Edit]:
            edits = []

            # `strcmp(a, b) == 0` -> `STREQ(a, b)`
            for match in re.finditer(r"\bstrcmp\((.*)\) == 0", data):
                edits.append(Edit(
                    span=match.span(),
                    content='STREQ(%s)' % (match.group(1)),
                    content_fail='__ALWAYS_FAIL__',
                ))
            for match in re.finditer(r"!strcmp\((.*)\)", data):
                edits.append(Edit(
                    span=match.span(),
                    content='STREQ(%s)' % (match.group(1)),
                    content_fail='__ALWAYS_FAIL__',
                ))

            # `strcmp(a, b) != 0` -> `!STREQ(a, b)`
            for match in re.finditer(r"\bstrcmp\((.*)\) != 0", data):
                edits.append(Edit(
                    span=match.span(),
                    content='!STREQ(%s)' % (match.group(1)),
                    content_fail='__ALWAYS_FAIL__',
                ))
            for match in re.finditer(r"\bstrcmp\((.*)\)", data):
                edits.append(Edit(
                    span=match.span(),
                    content='!STREQ(%s)' % (match.group(1)),
                    content_fail='__ALWAYS_FAIL__',
                ))

            return edits

    class use_str_sizeof_macros(EditGenerator):
        """
        Use `STRNCPY` & `SNPRINTF` macros:

        Replace:
          BLI_strncpy(a, b, sizeof(a))
        With:
          STRNCPY(a, b)

        Replace:
          BLI_snprintf(a, sizeof(a), "format %s", b)
        With:
          SNPRINTF(a, "format %s", b)
        """
        is_default = True

        @staticmethod
        def edit_list_from_file(_source: str, data: str, _shared_edit_data: Any) -> List[Edit]:
            edits = []

            # `BLI_strncpy(a, b, sizeof(a))` -> `STRNCPY(a, b)`
            # `BLI_strncpy(a, b, SOME_ID)` -> `STRNCPY(a, b)`
            for src, dst in (
                    ("BLI_strncpy", "STRNCPY"),
                    ("BLI_strncpy_rlen", "STRNCPY_RLEN"),
                    ("BLI_strncpy_utf8", "STRNCPY_UTF8"),
                    ("BLI_strncpy_utf8_rlen", "STRNCPY_UTF8_RLEN"),
            ):
                for match in re.finditer(
                        (r"\b" + src + (
                            r"\(([^,]+,\s+[^,]+),\s+" r"("
                            r"sizeof\([^\(\)]+\)"  # Trailing `sizeof(..)`.
                            r"|"
                            r"[a-zA-Z0-9_]+"  # Trailing identifier (typically a define).
                            r")" r"\)"
                        )),
                        data,
                        flags=re.MULTILINE,
                ):
                    edits.append(Edit(
                        span=match.span(),
                        content='%s(%s)' % (dst, match.group(1)),
                        content_fail='__ALWAYS_FAIL__',
                    ))

            # `BLI_snprintf(a, SOME_SIZE, ...` -> `SNPRINTF(a, ...`
            for src, dst in (
                    ("BLI_snprintf", "SNPRINTF"),
                    ("BLI_snprintf_rlen", "SNPRINTF_RLEN"),
                    ("BLI_vsnprintf", "VSNPRINTF"),
                    ("BLI_vsnprintf_rlen", "VSNPRINTF_RLEN"),
            ):
                for match in re.finditer(
                        r"\b" + src + r"\(([^,]+),\s+([^,]+),",
                        data,
                        flags=re.MULTILINE,
                ):
                    edits.append(Edit(
                        span=match.span(),
                        content='%s(%s,' % (dst, match.group(1)),
                        content_fail='__ALWAYS_FAIL__',
                    ))

            return edits

    class use_array_size_macro(EditGenerator):
        """
        Use macro for an error checked array size:

        Replace:
          sizeof(foo) / sizeof(*foo)
        With:
          ARRAY_SIZE(foo)
        """
        is_default = True

        @staticmethod
        def edit_list_from_file(_source: str, data: str, _shared_edit_data: Any) -> List[Edit]:
            edits = []
            # Note that this replacement is only valid in some cases,
            # so only apply with validation that binary output matches.
            for match in re.finditer(r"\bsizeof\((.*)\) / sizeof\([^\)]+\)", data):
                edits.append(Edit(
                    span=match.span(),
                    content='ARRAY_SIZE(%s)' % match.group(1),
                    content_fail='__ALWAYS_FAIL__',
                ))

            return edits

    class parenthesis_cleanup(EditGenerator):
        """
        Use macro for an error checked array size:

        Replace:
          ((a + b))
        With:
          (a + b)

        Replace:
          (func(a + b))
        With:
          func(a + b)

        Note that the `CFLAGS` should be set so missing parentheses that contain assignments - error instead of warn:
        With GCC: `-Werror=parentheses`

        Note that this does not make any edits inside macros because it can be important to keep parenthesis
        around macro arguments.
        """

        # Non-default because this edit can be applied to macros in situations where removing the parentheses
        # could result in macro expansion to have different results (depending on the arguments parsed in).
        # TODO: make this check skip macro text and it could be enabled by default.
        is_default = False

        @staticmethod
        def edit_list_from_file(_source: str, data: str, _shared_edit_data: Any) -> List[Edit]:
            edits = []

            # Give up after searching for a bracket this many characters and finding none.
            bracket_seek_limit = 4000

            # Don't match double brackets because this will not match multiple overlapping matches
            # Where 3 brackets should be checked as two separate pairs.
            for match in re.finditer(r"(\()", data):
                outer_beg = match.span()[0]
                inner_beg = outer_beg + 1
                if data[inner_beg] != "(":
                    continue

                inner_end = text_matching_bracket_forward(data, inner_beg, inner_beg + bracket_seek_limit, "(", ")")
                if inner_end == -1:
                    continue
                outer_beg = inner_beg - 1
                outer_end = text_matching_bracket_forward(data, outer_beg, inner_end + 1, "(", ")")
                if outer_end != inner_end + 1:
                    continue

                if text_cxx_in_macro_definition(data, outer_beg):
                    continue

                text = data[inner_beg:inner_end + 1]
                edits.append(Edit(
                    span=(outer_beg, outer_end + 1),
                    content=text,
                    content_fail='(__ALWAYS_FAIL__)',
                ))

            # Handle `(func(a + b))` -> `func(a + b)`
            for match in re.finditer(r"(\))", data):
                inner_end = match.span()[0]
                outer_end = inner_end + 1
                if data[outer_end] != ")":
                    continue

                inner_beg = text_matching_bracket_backward(data, inner_end, inner_end - bracket_seek_limit, "(", ")")
                if inner_beg == -1:
                    continue
                outer_beg = text_matching_bracket_backward(data, outer_end, outer_end - bracket_seek_limit, "(", ")")
                if outer_beg == -1:
                    continue

                # The text between the first two opening brackets:
                # `(function_name(a + b))` -> `function_name`.
                text = data[outer_beg + 1:inner_beg]

                # Handled in the first loop looking for forward brackets.
                if text == "":
                    continue

                # Don't convert `prefix(func(a + b))` -> `prefixfunc(a + b)`
                if data[outer_beg - 1] in IDENTIFIER_CHARS:
                    continue

                # Don't convert `static_cast<float>(foo(bar))` -> `static_cast<float>foo(bar)`
                # While this will always fail to compile it slows down tests.
                if data[outer_beg - 1] == ">":
                    continue

                # Exact rule here is arbitrary, in general though spaces mean there are operations
                # that can use the brackets.
                if " " in text:
                    continue

                # Search back an arbitrary number of chars 8 should be enough
                # but manual formatting can add additional white-space, so increase
                # the size to account for that.
                prefix = data[max(outer_beg - 20, 0):outer_beg].strip()
                if prefix:
                    # Avoid `if (SOME_MACRO(..)) {..}` -> `if SOME_MACRO(..) {..}`
                    # While correct it relies on parenthesis within the macro which isn't ideal.
                    if prefix.split()[-1] in {"if", "while", "switch"}:
                        continue
                    # Avoid `*(--foo)` -> `*--foo`.
                    # While correct it reads badly.
                    if data[outer_beg - 1] == "*":
                        continue

                if text_cxx_in_macro_definition(data, outer_beg):
                    continue

                text_no_parens = data[outer_beg + 1: outer_end]

                edits.append(Edit(
                    span=(outer_beg, outer_end + 1),
                    content=text_no_parens,
                    content_fail='__ALWAYS_FAIL__',
                ))

            return edits

    class header_clean(EditGenerator):
        """
        Clean headers, ensuring that the headers removed are not used directly or indirectly.

        Note that the `CFLAGS` should be set so missing prototypes error instead of warn:
        With GCC: `-Werror=missing-prototypes`
        """

        # Non-default because changes to headers may cause breakage on other platforms.
        # Before committing these changes all supported platforms should be tested to compile without problems.
        is_default = False

        @staticmethod
        def _header_guard_from_filename(f: str) -> str:
            return '__%s__' % os.path.basename(f).replace('.', '_').upper()

        @classmethod
        def setup(cls) -> Any:
            # For each file replace `pragma once` with old-style header guard.
            # This is needed so we can remove the header with the knowledge the source file didn't use it indirectly.
            files: List[Tuple[str, str, str, str]] = []
            shared_edit_data = {
                'files': files,
            }
            for f in files_recursive_with_ext(
                    os.path.join(SOURCE_DIR, 'source'),
                    ('.h', '.hh', '.inl', '.hpp', '.hxx'),
            ):
                with open(f, 'r', encoding='utf-8') as fh:
                    data = fh.read()

                for match in re.finditer(r'^[ \t]*#\s*(pragma\s+once)\b', data, flags=re.MULTILINE):
                    header_guard = cls._header_guard_from_filename(f)
                    start, end = match.span()
                    src = data[start:end]
                    dst = (
                        '#ifndef %s\n#define %s' % (header_guard, header_guard)
                    )
                    dst_footer = '\n#endif /* %s */\n' % header_guard
                    files.append((f, src, dst, dst_footer))
                    data = data[:start] + dst + data[end:] + dst_footer
                    with open(f, 'w', encoding='utf-8') as fh:
                        fh.write(data)
                    break
            return shared_edit_data

        @staticmethod
        def teardown(shared_edit_data: Any) -> None:
            files = shared_edit_data['files']
            for f, src, dst, dst_footer in files:
                with open(f, 'r', encoding='utf-8') as fh:
                    data = fh.read()

                data = data.replace(
                    dst, src,
                ).replace(
                    dst_footer, '',
                )
                with open(f, 'w', encoding='utf-8') as fh:
                    fh.write(data)

        @classmethod
        def edit_list_from_file(cls, _source: str, data: str, _shared_edit_data: Any) -> List[Edit]:
            edits = []

            # Remove include.
            for match in re.finditer(r"^(([ \t]*#\s*include\s+\")([^\"]+)(\"[^\n]*\n))", data, flags=re.MULTILINE):
                header_name = match.group(3)
                header_guard = cls._header_guard_from_filename(header_name)
                edits.append(Edit(
                    span=match.span(),
                    content='',  # Remove the header.
                    content_fail='%s__ALWAYS_FAIL__%s' % (match.group(2), match.group(4)),
                    extra_build_args=('-D' + header_guard, ),
                ))

            return edits

    class use_function_style_cast(EditGenerator):
        """
        Use function call style casts (C++ only).

        Replace:
          (float)(a + b)
        With:
          float(a + b)

        Also support more complex cases involving right hand bracket insertion.

        Replace:
          (float)foo(a + b)
        With:
          float(foo(a + b))
        """
        is_default = True

        @staticmethod
        def edit_list_from_file(source: str, data: str, _shared_edit_data: Any) -> List[Edit]:

            edits: List[Edit] = []

            # The user might include C & C++, if they forget, it is better not to operate on C.
            if source.lower().endswith((".h", ".c")):
                return edits

            any_number_re = "(" + "|".join(BUILT_IN_NUMERIC_TYPES) + ")"

            # Handle both:
            # - Simple case:  `(float)(a + b)` -> `float(a + b)`.
            # - Complex Case: `(float)foo(a + b) + c` -> `float(foo(a + b)) + c`
            for match in re.finditer(
                    "(\\()" +  # 1st group.
                    any_number_re +  # 2nd group.
                    "(\\))",  # 3rd group.
                    data,
            ):
                beg, end = match.span()
                # This could be ignored, but `sizeof` accounts for such a large number
                # of cases that should be left as-is, that it's best to explicitly ignore them.
                if (
                    (beg > 6) and
                    (data[beg - 6: beg] == 'sizeof') and
                    (not data[beg - 7].isalpha())
                ):
                    continue

                char_after = data[end]
                if char_after == "(":
                    # Simple case.
                    edits.append(Edit(
                        span=(beg, end),
                        content=match.group(2),
                        content_fail='__ALWAYS_FAIL__',
                    ))
                else:
                    # The complex case is involved as brackets need to be added.
                    # Currently this is not handled in a clever way, just try add in brackets
                    # and rely on matching build output to know if they were added in the right place.
                    text = match.group(2)
                    # `span = (beg, end)`.
                    for offset_end in range(end + 1, len(data)):
                        # Not technically correct, but it's rare that this will span lines.
                        if "\n" == data[offset_end]:
                            break

                        if (
                                (data[offset_end - 1] in IDENTIFIER_CHARS) and
                                (data[offset_end] in IDENTIFIER_CHARS)
                        ):
                            continue

                        # Include `text_tail` in fail content in case it contains comments.
                        text_tail = "(" + data[end:offset_end] + ")"
                        edits.append(Edit(
                            span=(beg, offset_end),
                            content=text + text_tail,
                            content_fail='(__ALWAYS_FAIL__)' + text_tail,
                        ))

            # Simple case: `static_cast<float>(a + b)` => `float(a + b)`.
            for match in re.finditer(
                    r"\b(static_cast<)" +  # 1st group.
                    any_number_re +  # 2nd group.
                    "(>)",  # 3rd group.
                    data,
            ):
                edits.append(Edit(
                    span=match.span(),
                    content='%s' % match.group(2),
                    content_fail='__ALWAYS_FAIL__',
                ))

            return edits


def test_edit(
        source: str,
        output: str,
        output_bytes: Optional[bytes],
        build_args: Sequence[str],
        build_cwd: Optional[str],
        data: str,
        data_test: str,
        *,
        keep_edits: bool,
        expect_failure: bool,
        verbose_compile: bool,
        verbose_edit_actions: bool,
) -> bool:
    """
    Return true if `data_test` has the same object output as `data`.
    """
    if os.path.exists(output):
        os.remove(output)

    with open(source, 'w', encoding='utf-8') as fh:
        fh.write(data_test)

    ret = run(build_args, cwd=build_cwd, quiet=expect_failure, verbose_compile=verbose_compile)
    if ret == 0:
        output_bytes_test = file_as_bytes(output)
        if (output_bytes is None) or (file_as_bytes(output) == output_bytes):
            if not keep_edits:
                with open(source, 'w', encoding='utf-8') as fh:
                    fh.write(data)
            return True
        else:
            if verbose_edit_actions:
                print("Changed code, skip...", hex(hash(output_bytes)), hex(hash(output_bytes_test)))
    else:
        if not expect_failure:
            if verbose_edit_actions:
                print("Failed to compile, skip...")

    with open(source, 'w', encoding='utf-8') as fh:
        fh.write(data)
    return False


# -----------------------------------------------------------------------------
# List Fix Functions

def edit_function_get_all(*, is_default: Optional[bool] = None) -> List[str]:
    fixes = []
    for name in dir(edit_generators):
        value = getattr(edit_generators, name)
        if type(value) is type and issubclass(value, EditGenerator):
            if is_default is not None:
                if is_default != value.is_default:
                    continue
            fixes.append(name)
    fixes.sort()
    return fixes


def edit_class_from_id(name: str) -> Type[EditGenerator]:
    result = getattr(edit_generators, name)
    assert issubclass(result, EditGenerator)
    # MYPY 0.812 doesn't recognize the assert above.
    return result  # type: ignore


def edit_docstring_from_id(name: str) -> str:
    from textwrap import dedent
    result = getattr(edit_generators, name).__doc__
    return dedent(result or '').strip('\n') + '\n'


def edit_group_compatible(edits: Sequence[str]) -> Sequence[Sequence[str]]:
    """
    Group compatible edits, so it's possible for a single process to iterate on many edits for a single file.
    """
    edits_grouped = []

    edit_generator_class_prev = None
    for edit in edits:
        edit_generator_class = edit_class_from_id(edit)
        if edit_generator_class_prev is None or (
                edit_generator_class.setup != edit_generator_class_prev.setup and
                edit_generator_class.teardown != edit_generator_class_prev.teardown
        ):
            # Create a new group.
            edits_grouped.append([edit])
        else:
            edits_grouped[-1].append(edit)
        edit_generator_class_prev = edit_generator_class
    return edits_grouped


# -----------------------------------------------------------------------------
# Accept / Reject Edits

def apply_edit(source_relative: str, data: str, text_to_replace: str, start: int, end: int, *, verbose: bool) -> str:
    if verbose:
        line_before = line_from_span(data, start, end)

    data = data[:start] + text_to_replace + data[end:]

    if verbose:
        end += len(text_to_replace) - (end - start)
        line_after = line_from_span(data, start, end)

        print("")
        print("Testing edit:", source_relative)
        print(line_before)
        print(line_after)

    return data


def wash_source_with_edit(
        source: str,
        output: str,
        build_args: Sequence[str],
        build_cwd: Optional[str],
        skip_test: bool,
        verbose_compile: bool,
        verbose_edit_actions: bool,
        shared_edit_data: Any,
        edit_to_apply: str,
) -> None:
    # For less verbose printing, strip the prefix.
    source_relative = os.path.relpath(source, SOURCE_DIR)

    # build_args = build_args + " -Werror=duplicate-decl-specifier"
    with open(source, 'r', encoding='utf-8') as fh:
        data = fh.read()
    edit_generator_class = edit_class_from_id(edit_to_apply)

    # After performing all edits, store the result in this set.
    #
    # This is a heavy solution that guarantees edits never oscillate between
    # multiple states, so re-visiting a previously visited state will always exit.
    data_states: Set[str] = set()

    # When overlapping edits are found, keep attempting edits.
    edit_again = True
    while edit_again:
        edit_again = False

        edits = edit_generator_class.edit_list_from_file(source, data, shared_edit_data)
        # Sort by span, in a way that tries shorter spans first
        # This is more efficient when testing multiple overlapping edits,
        # since when a smaller edit succeeds, it's less likely to have to try as many edits that span wider ranges.
        # (This applies to `use_function_style_cast`).
        edits.sort(reverse=True, key=lambda edit: (edit.span[0], -edit.span[1]))
        if not edits:
            return

        if skip_test:
            # Just apply all edits.
            for (start, end), text, _text_always_fail, _extra_build_args in edits:
                data = apply_edit(source_relative, data, text, start, end, verbose=verbose_edit_actions)
            with open(source, 'w', encoding='utf-8') as fh:
                fh.write(data)
            return

        test_edit(
            source, output, None, build_args, build_cwd, data, data,
            keep_edits=False,
            expect_failure=False,
            verbose_compile=verbose_compile,
            verbose_edit_actions=verbose_edit_actions,
        )
        if not os.path.exists(output):
            # raise Exception("Failed to produce output file: " + output)

            # NOTE(@ideasman42): This fails very occasionally and needs to be investigated why.
            # For now skip, as it's disruptive to force-quit in the middle of all other changes.
            print("Failed to produce output file, skipping:", repr(output))
            return

        output_bytes = file_as_bytes(output)
        # Dummy value that won't cause problems.
        edit_prev_start = len(data) + 1

        for (start, end), text, text_always_fail, extra_build_args in edits:
            if end >= edit_prev_start:
                # Run the edits again, in case this would have succeeded,
                # but was skipped due to edit-overlap.
                edit_again = True
                continue
            build_args_for_edit = build_args
            if extra_build_args:
                # Add directly after the compile command.
                build_args_for_edit = build_args[:1] + extra_build_args + build_args[1:]

            data_test = apply_edit(source_relative, data, text, start, end, verbose=verbose_edit_actions)
            if test_edit(
                    source, output, output_bytes, build_args_for_edit, build_cwd, data, data_test,
                    keep_edits=False,
                    expect_failure=False,
                    verbose_compile=verbose_compile,
                    verbose_edit_actions=verbose_edit_actions,
            ):
                # This worked, check if the change would fail if replaced with 'text_always_fail'.
                data_test_always_fail = apply_edit(source_relative, data, text_always_fail, start, end, verbose=False)
                if test_edit(
                        source, output, output_bytes, build_args_for_edit, build_cwd, data, data_test_always_fail,
                        expect_failure=True,
                        keep_edits=False,
                        verbose_compile=verbose_compile,
                        verbose_edit_actions=verbose_edit_actions,
                ):
                    if verbose_edit_actions:
                        print("Edit at", (start, end), "doesn't fail, assumed to be ifdef'd out, continuing")
                    continue

                # Apply the edit.
                data = data_test
                with open(source, 'w', encoding='utf-8') as fh:
                    fh.write(data)

                # Update the last successful edit, the end of the next edit must not overlap this one.
                edit_prev_start = start

        # Finished applying `edits`, check if further edits should be applied.
        if edit_again:
            data_states_len = len(data_states)
            data_states.add(data)
            if data_states_len == len(data_states):
                # Avoid the *extremely* unlikely case that edits re-visit previously visited states.
                edit_again = False
            else:
                # It is interesting to know how many passes run when debugging.
                # print("Passes for: ", source, len(data_states))
                pass


def wash_source_with_edit_list(
        source: str,
        output: str,
        build_args: Sequence[str],
        build_cwd: Optional[str],
        skip_test: bool,
        verbose_compile: bool,
        verbose_edit_actions: bool,
        shared_edit_data: Any,
        edit_list: Sequence[str],
) -> None:
    for edit_to_apply in edit_list:
        wash_source_with_edit(
            source,
            output,
            build_args,
            build_cwd,
            skip_test,
            verbose_compile,
            verbose_edit_actions,
            shared_edit_data,
            edit_to_apply,
        )


# -----------------------------------------------------------------------------
# Edit Source Code From Args

def run_edits_on_directory(
        *,
        build_dir: str,
        regex_list: List[re.Pattern[str]],
        edits_to_apply: Sequence[str],
        skip_test: bool,
        jobs: int,
        verbose_compile: bool,
        verbose_edit_actions: bool,
) -> int:
    import multiprocessing

    # currently only supports ninja or makefiles
    build_file_ninja = os.path.join(build_dir, "build.ninja")
    build_file_make = os.path.join(build_dir, "Makefile")
    if os.path.exists(build_file_ninja):
        print("Using Ninja")
        args = find_build_args_ninja(build_dir)
    elif os.path.exists(build_file_make):
        print("Using Make")
        args = find_build_args_make(build_dir)
    else:
        sys.stderr.write(
            "Can't find Ninja or Makefile (%r or %r), aborting" %
            (build_file_ninja, build_file_make)
        )
        return 1

    if jobs <= 0:
        jobs = multiprocessing.cpu_count() * 2

    if args is None:
        # Error will have been reported.
        return 1

    # needed for when arguments are referenced relatively
    os.chdir(build_dir)

    # Weak, but we probably don't want to handle extern.
    # this limit could be removed.
    source_paths = (
        os.path.join("intern", "ghost"),
        os.path.join("intern", "guardedalloc"),
        os.path.join("source"),
    )

    def split_build_args_with_cwd(build_args_str: str) -> Tuple[Sequence[str], Optional[str]]:
        import shlex
        build_args = shlex.split(build_args_str)

        cwd = None
        if len(build_args) > 3:
            if build_args[0] == "cd" and build_args[2] == "&&":
                cwd = build_args[1]
                del build_args[0:3]
        return build_args, cwd

    def output_from_build_args(build_args: Sequence[str], cwd: Optional[str]) -> str:
        i = build_args.index("-o")
        # Assume the output is a relative path is a CWD was set.
        if cwd:
            return os.path.join(cwd, build_args[i + 1])
        return build_args[i + 1]

    def test_path(c: str) -> bool:
        # Skip any generated source files (files in the build directory).
        if os.path.abspath(c).startswith(build_dir):
            return False
        # Raise an exception since this should never happen,
        # we want to know about it early if it does, as it will cause failure
        # when attempting to compile the missing file.
        if not os.path.exists(c):
            raise Exception("Missing source file: " + c)

        for source_path in source_paths:
            index = c.rfind(source_path)
            # print(c)
            if index != -1:
                # Remove first part of the path, we don't want to match
                # against paths in Blender's repo.
                # print(source_path)
                c_strip = c[index:]
                for regex in regex_list:
                    if regex.match(c_strip) is not None:
                        return True
        return False

    # Filter out build args.
    args_orig_len = len(args)
    args_with_cwd = [
        (c, *split_build_args_with_cwd(build_args_str))
        for (c, build_args_str) in args
        if test_path(c)
    ]
    del args
    print("Operating on %d of %d files..." % (len(args_with_cwd), args_orig_len))
    for (c, build_args, build_cwd) in args_with_cwd:
        print(" ", c)
    del args_orig_len

    if jobs > 1:
        # Group edits to avoid one file holding up the queue before other edits can be worked on.
        # Custom setup/tear-down functions still block though.
        edits_to_apply_grouped = edit_group_compatible(edits_to_apply)
    else:
        # No significant advantage in grouping, split each into a group of one for simpler debugging/execution.
        edits_to_apply_grouped = [[edit] for edit in edits_to_apply]

    for i, edits_group in enumerate(edits_to_apply_grouped):
        print("Applying edit:", edits_group, "(%d of %d)" % (i + 1, len(edits_to_apply_grouped)))
        edit_generator_class = edit_class_from_id(edits_group[0])

        shared_edit_data = edit_generator_class.setup()

        try:
            if jobs > 1:
                args_expanded = [(
                    c,
                    output_from_build_args(build_args, build_cwd),
                    build_args,
                    build_cwd,
                    skip_test,
                    verbose_compile,
                    verbose_edit_actions,
                    shared_edit_data,
                    edits_group,
                ) for (c, build_args, build_cwd) in args_with_cwd]
                pool = multiprocessing.Pool(processes=jobs)
                pool.starmap(wash_source_with_edit_list, args_expanded)
                del args_expanded
            else:
                # now we have commands
                for c, build_args, build_cwd in args_with_cwd:
                    wash_source_with_edit_list(
                        c,
                        output_from_build_args(build_args, build_cwd),
                        build_args,
                        build_cwd,
                        skip_test,
                        verbose_compile,
                        verbose_edit_actions,
                        shared_edit_data,
                        edits_group,
                    )
        except Exception as ex:
            raise ex
        finally:
            edit_generator_class.teardown(shared_edit_data)

    print("\n" "Exit without errors")
    return 0


def create_parser(edits_all: Sequence[str], edits_all_default: Sequence[str]) -> argparse.ArgumentParser:
    from textwrap import indent

    # Create doc-string for edits.
    edits_all_docs = []
    for edit in edits_all:
        # `%` -> `%%` is needed for `--help` not to interpret these as formatting arguments.
        edits_all_docs.append(
            "  %s\n%s" % (
                edit,
                indent(edit_docstring_from_id(edit).replace("%", "%%"), '    '),
            )
        )

    # Create doc-string for verbose.
    verbose_all_docs = []
    for verbose_id, verbose_doc in VERBOSE_INFO:
        # `%` -> `%%` is needed for `--help` not to interpret these as formatting arguments.
        verbose_all_docs.append(
            "  %s\n%s" % (
                verbose_id,
                indent(verbose_doc.replace("%", "%%"), "    "),
            )
        )

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        "build_dir",
        help="list of files or directories to check",
    )
    parser.add_argument(
        "--match",
        nargs='+',
        default=(
            r".*\.(c|cc|cpp)$",
        ),
        required=False,
        metavar="REGEX",
        help="Match file paths against this expression",
    )
    parser.add_argument(
        "--edits",
        dest="edits",
        default=",".join(edits_all_default),
        help=(
            "Specify the edit preset to run.\n"
            "\n" +
            "\n".join(edits_all_docs) + "\n"
            "Multiple edits may be passed at once (comma separated, no spaces).\n"
            "\n"
            "The default value for this argument includes edits which are unlikely\n"
            "to cause problems on other platforms and are generally considered safe to apply.\n"
            "Non-default edits should be manually reviewed in more derail before committing."
        ),
        required=False,
    )
    parser.add_argument(
        "--verbose",
        dest="verbose",
        default="",
        help=(
            "Specify verbose actions.\n\n" +
            "\n".join(verbose_all_docs) + "\n"
            "Multiple verbose types may be passed at once (comma separated, no spaces)."),
        required=False,
    )
    parser.add_argument(
        "--skip-test",
        dest="skip_test",
        default=False,
        action='store_true',
        help=(
            "Perform all edits without testing if they perform functional changes. "
            "Use to quickly preview edits, or to perform edits which are manually checked (default=False)"
        ),
        required=False,
    )
    parser.add_argument(
        "--jobs",
        dest="jobs",
        type=int,
        default=0,
        help=(
            "The number of processes to use. "
            "Defaults to zero which detects the available cores, 1 is single threaded (useful for debugging)."
        ),
        required=False,
    )

    return parser


def main() -> int:
    edits_all = edit_function_get_all()
    edits_all_default = edit_function_get_all(is_default=True)
    parser = create_parser(edits_all, edits_all_default)
    args = parser.parse_args()

    build_dir = os.path.normpath(os.path.abspath(args.build_dir))
    regex_list = []

    for expr in args.match:
        try:
            regex_list.append(re.compile(expr))
        except Exception as ex:
            print(f"Error in expression: {expr}\n  {ex}")
            return 1

    edits_all_from_args = args.edits.split(",")
    if not edits_all_from_args:
        print("Error, no '--edits' arguments given!")
        return 1

    for edit in edits_all_from_args:
        if edit not in edits_all:
            print("Error, unrecognized '--edits' argument '%s', expected a value in {%s}" % (
                edit,
                ", ".join(edits_all),
            ))
            return 1

    verbose_all = [verbose_id for verbose_id, _ in VERBOSE_INFO]
    verbose_compile = False
    verbose_edit_actions = False
    verbose_all_from_args = args.verbose.split(",") if args.verbose else []
    while verbose_all_from_args:
        match (verbose_id := verbose_all_from_args.pop()):
            case "compile":
                verbose_compile = True
            case "edit_actions":
                verbose_edit_actions = True
            case _:
                print("Error, unrecognized '--verbose' argument '%s', expected a value in {%s}" % (
                    verbose_id,
                    ", ".join(verbose_all),
                ))
                return 1

    if len(edits_all_from_args) > 1:
        for edit in edits_all:
            if edit not in edits_all_from_args:
                print("Skipping edit: %s, default=%d" % (edit, getattr(edit_generators, edit).is_default))

    return run_edits_on_directory(
        build_dir=build_dir,
        regex_list=regex_list,
        edits_to_apply=edits_all_from_args,
        skip_test=args.skip_test,
        jobs=args.jobs,
        verbose_compile=verbose_compile,
        verbose_edit_actions=verbose_edit_actions,
    )


if __name__ == "__main__":
    sys.exit(main())
