#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
A command line utility to check Blender's source code with CLANG's Python module.

To call this directly:

export CLANG_LIB_DIR=/usr/lib64
cd {BUILD_DIR}
python ../blender/build_files/cmake/cmake_static_check_clang.py --match=".*" --checks=struct_comments

"""

import argparse
import os
import re
import sys

from typing import (
    Any,
    Dict,
    List,
    Type,
    Sequence,
    Tuple,
)


import project_source_info

# pylint: disable-next=import-outside-toplevel
import clang  # type: ignore
# pylint: disable-next=import-outside-toplevel
import clang.cindex  # type: ignore
from clang.cindex import (
    CursorKind,
)

# Only for readability.
ClangNode = Any
ClangTranslationUnit = Any
ClangSourceLocation = Any


USE_VERBOSE = os.environ.get("VERBOSE", None) is not None

CLANG_BIND_DIR = os.environ.get("CLANG_BIND_DIR")
CLANG_LIB_DIR = os.environ.get("CLANG_LIB_DIR")

if CLANG_BIND_DIR is None:
    print("$CLANG_BIND_DIR python binding dir not set")
if CLANG_LIB_DIR is None:
    print("$CLANG_LIB_DIR clang lib dir not set")

if CLANG_LIB_DIR:
    clang.cindex.Config.set_library_path(CLANG_LIB_DIR)
if CLANG_BIND_DIR:
    sys.path.append(CLANG_BIND_DIR)


CHECKER_IGNORE_PREFIX = [
    "extern",
]

CHECKER_EXCLUDE_SOURCE_FILES = set(os.path.join(*f.split("/")) for f in (
    # Skip parsing these large (mostly data files).
    "source/blender/editors/space_text/text_format_pov.cc",
    "source/blender/editors/space_text/text_format_pov_ini.cc",
))


# -----------------------------------------------------------------------------
# Utility Functions

def clang_source_location_as_str(source_location: ClangSourceLocation) -> str:
    return "{:s}:{:d}:{:d}:".format(str(source_location.file), source_location.line, source_location.column)


# -----------------------------------------------------------------------------
# Checkers

class ClangChecker:
    """
    Base class for checkers.

    Notes:

    - The function ``check_source`` takes file_data as bytes instead of a string
      because the offsets provided by CLANG are byte offsets.
      While the offsets could be converted into UNICODE offset's,
      there doesn't seem to be an efficient & convenient way to do that.
    """
    __slots__ = ()

    def __new__(cls, *args: Tuple[Any], **kwargs: Dict[str, Any]) -> Any:
        raise RuntimeError("%s should not be instantiated" % cls)

    @staticmethod
    def check_source(
            _filepath: str,
            _file_data: bytes,
            _tu: ClangTranslationUnit,
            _shared_check_data: Any,
    ) -> List[str]:
        raise RuntimeError("This function must be overridden by it's subclass!")
        return []

    @staticmethod
    def setup() -> Any:
        return None

    @staticmethod
    def teardown(_shared_check_data: Any) -> None:
        pass


class clang_checkers:
    # fake module.

    class struct_comments(ClangChecker):
        """
        Ensure comments in struct declarations match the members of the struct, e.g:

           SomeStruct var = {
               /*name*/ "Text",
               /*children*/ nullptr,
               /*flag*/ 0,
           };

        Will generate a warning if any of the names in the prefix comments don't match the struct member names.
        """

        _struct_comments_ignore = {
            # `PyTypeObject` uses compile time members that vary (see: #PyVarObject_HEAD_INIT macro)
            # While some clever comment syntax could be supported to signify multiple/optional members
            # this is such a specific case that it's simpler to skip this warning.
            "PyTypeObject": {"ob_base": {"ob_size"}},
        }

        @staticmethod
        def _struct_check_comments_recursive(
                # Static (unchanged for each recursion).
                filepath: str,
                file_data: bytes,
                # Different for each recursion.
                node: ClangNode,
                node_parent: ClangNode,
                level: int,
                # Used to build data.
                struct_decl_map: Dict[str, ClangNode],
                struct_type_map: Dict[str, str],
                output: List[str],
        ) -> None:

            # Needed to read back the node.
            if USE_VERBOSE:
                print("TRY:", node.kind, node.spelling, len(list(node.get_tokens())), level, node.location)

            # if node.kind == CursorKind.VAR_DECL and node.spelling == "Vector_NumMethods":
            #     import IPython
            #     IPython.embed()

            if node.kind == CursorKind.STRUCT_DECL:
                # Ignore forward declarations.
                if next(node.get_children(), None) is not None:
                    struct_type = node.spelling.strip()
                    if not struct_type:
                        # The parent may be a `typedef [..] TypeID` where `[..]` is `struct { a; b; c; }`.
                        # Inspect the parent.
                        if node_parent is not None and (node_parent.kind == CursorKind.TYPEDEF_DECL):
                            tokens = list(node_parent.get_tokens())
                            if tokens[0].spelling == "typedef":
                                struct_type = tokens[-1].spelling

                    struct_decl_map[struct_type] = node

            # Ignore declarations for anything defined outside this file.
            if str(node.location.file) == filepath:
                if node.kind == CursorKind.INIT_LIST_EXPR:
                    if USE_VERBOSE:
                        print(node.spelling, node.location)
                    # Split to avoid `const struct` .. and similar.
                    # NOTE: there may be an array size suffix, e.g. `[4]`.
                    # This could be supported.
                    struct_type = node.type.spelling.split()[-1]
                    struct = struct_decl_map.get(struct_type)
                    if struct is None:
                        if USE_VERBOSE:
                            print("NOT FOUND:", struct_type)
                        struct_type = struct_type_map.get(struct_type)
                        if struct_type is not None:
                            struct = struct_decl_map.get(struct_type)

                    if USE_VERBOSE:
                        print("INSPECTING STRUCT:", struct_type)
                    if struct is not None:
                        member_names = [
                            node_child.spelling for node_child in struct.get_children()
                            if node_child.kind == CursorKind.FIELD_DECL
                        ]
                        # if struct_type == "PyMappingMethods":
                        #     import IPython
                        #     IPython.embed()

                        children = list(node.get_children())
                        comment_names = []

                        # Set to true when there is a comment directly before a value,
                        # this is needed because:
                        # - Comments on the previous line are rarely intended to be identifiers of the struct member.
                        # - Comments which _are_ intended to be identifiers can be wrapped onto new-lines
                        #   so they should not be ignored.
                        #
                        # While it's possible every member is wrapped onto a new-line,
                        # this is highly unlikely.
                        comment_names_prefix_any = False

                        for node_child in children:
                            # Extract the content before the child
                            # (typically a C-style comment containing the struct member).
                            end = min(node_child.location.offset, len(file_data))

                            # It's possible this ID has a preceding "name::space::etc"
                            # which should be skipped.
                            while end > 0 and ((ch := bytes((file_data[end - 1],))).isalpha() or ch == b":"):
                                end -= 1

                            has_newline = False
                            while end > 0:
                                ch = bytes((file_data[end - 1],))
                                if ch in {b"\t", b" "}:
                                    end -= 1
                                elif ch == b"\n":
                                    end -= 1
                                    has_newline = True
                                else:
                                    break

                            beg = end - 1
                            while beg != 0 and bytes((file_data[beg],)) not in {
                                    b"\n",
                                    # Needed so declarations on a single line don't detect a comment
                                    # from an outer comment, e.g.
                                    #    SomeStruct x = {
                                    #      /*list*/ {nullptr, nullptr},
                                    #    };
                                    # Would start inside the first `nullptr` and walk backwards to find `/*list*/`.
                                    b"{"
                            }:
                                beg -= 1

                            # Seek back until the comment end (in some cases this includes code).
                            # This occurs when the body of the declaration includes code, e.g.
                            #    rcti x = {
                            #      /*xmin*/ foo->bar.baz,
                            #      ... snip ...
                            #    };
                            # Where `"xmin*/ foo->bar."` would be extracted were it not for this check.
                            # There might be a more elegant way to handle this, for how snipping off the last
                            # comment characters is sufficient.
                            end_test = file_data.rfind(b"*/", end + 1, beg)
                            if end_test != -1:
                                end = end_test

                            text = file_data[beg:end]
                            if text.lstrip().startswith(b"/*"):
                                if not has_newline:
                                    comment_names_prefix_any = True
                            else:
                                text = b""
                            comment_names.append(text.decode('utf-8'))

                        if USE_VERBOSE:
                            print(member_names)
                            print(comment_names)

                        total = min(len(member_names), len(comment_names))

                        if total != 0 and comment_names_prefix_any:
                            result = [""] * total
                            count_found = 0
                            count_invalid = 0
                            for i in range(total):
                                comment = comment_names[i]
                                if "/*" in comment and "*/" in comment:
                                    comment = comment.strip().strip("/").strip("*")
                                    if comment == member_names[i]:
                                        count_found += 1
                                    else:
                                        suppress_warning = False
                                        if (
                                                skip_members_table :=
                                                clang_checkers.struct_comments._struct_comments_ignore.get(
                                                    node_parent.type.spelling,
                                                )
                                        ) is not None:
                                            if (skip_members := skip_members_table.get(comment)) is not None:
                                                if member_names[i] in skip_members:
                                                    suppress_warning = True

                                        if not suppress_warning:
                                            result[i] = "Incorrect! found \"{:s}\" expected \"{:s}\"".format(
                                                comment, member_names[i])
                                            count_invalid += 1
                                else:
                                    result[i] = "No comment for \"{:s}\"".format(member_names[i])
                            if count_found == 0 and count_invalid == 0:
                                # No comments used, skip this as not all declaration use this comment style.
                                output.append(
                                    "NONE: {:s} {:s}".format(
                                        clang_source_location_as_str(node.location),
                                        node.type.spelling,
                                    )
                                )
                            elif count_found != total:
                                for i in range(total):
                                    if result[i]:
                                        output.append(
                                            "FAIL: {:s} {:s}".format(
                                                clang_source_location_as_str(children[i].location),
                                                result[i],
                                            )
                                        )
                            else:
                                output.append(
                                    "OK: {:s} {:s}".format(
                                        clang_source_location_as_str(node.location),
                                        node.type.spelling,
                                    )
                                )

            for node_child in node.get_children():
                clang_checkers.struct_comments._struct_check_comments_recursive(
                    filepath, file_data,
                    node_child, node, level + 1,
                    struct_decl_map, struct_type_map, output,
                )

        @staticmethod
        def check_source(
                filepath: str,
                file_data: bytes,
                tu: ClangTranslationUnit,
                _shared_check_data: Any) -> List[str]:
            output: List[str] = []

            struct_decl_map: Dict[str, Any] = {}
            struct_type_map: Dict[str, str] = {}
            clang_checkers.struct_comments._struct_check_comments_recursive(
                filepath, file_data,
                tu.cursor, None, 0,
                struct_decl_map, struct_type_map, output,
            )

            return output


# -----------------------------------------------------------------------------
# Checker Class Access

def check_function_get_all() -> List[str]:
    checkers = []
    for name in dir(clang_checkers):
        value = getattr(clang_checkers, name)
        if isinstance(value, type) and issubclass(value, ClangChecker):
            checkers.append(name)
    checkers.sort()
    return checkers


def check_class_from_id(name: str) -> Type[ClangChecker]:
    result = getattr(clang_checkers, name)
    assert issubclass(result, ClangChecker)
    # MYPY 0.812 doesn't recognize the assert above.
    return result  # type: ignore


def check_docstring_from_id(name: str) -> str:
    from textwrap import dedent
    result = getattr(clang_checkers, name).__doc__
    return dedent(result or '').strip('\n') + '\n'


# -----------------------------------------------------------------------------
# Generic Clang Checker

def check_source_file(
        filepath: str,
        args: Sequence[str],
        check_ids: Sequence[str],
        shared_check_data_foreach_check: Sequence[Any],
) -> str:
    index = clang.cindex.Index.create()
    try:
        tu = index.parse(filepath, args)
    except clang.cindex.TranslationUnitLoadError as ex:
        return "PARSE_ERROR: {:s} {!r}".format(filepath, ex)

    with open(filepath, "rb") as fh:
        file_data = fh.read()

    output: List[str] = []

    # we don't really care what we are looking at, just scan entire file for
    # function calls.
    for check, shared_check_data in zip(check_ids, shared_check_data_foreach_check):
        cls = check_class_from_id(check)
        output.extend(cls.check_source(filepath, file_data, tu, shared_check_data))

    if not output:
        return ""
    return "\n".join(output)


def check_source_file_for_imap(args: Tuple[str, Sequence[str], Sequence[str], Sequence[Any]]) -> str:
    return check_source_file(*args)


def source_info_filter(
        source_info: List[Tuple[str, List[str], List[str]]],
        regex_list: Sequence[re.Pattern[str]],
) -> List[Tuple[str, List[str], List[str]]]:
    source_dir = project_source_info.SOURCE_DIR
    if not source_dir.endswith(os.sep):
        source_dir += os.sep
    source_info_result = []
    for item in source_info:
        filepath_source = item[0]
        if filepath_source.startswith(source_dir):
            filepath_source_relative = filepath_source[len(source_dir):]
            if filepath_source_relative in CHECKER_EXCLUDE_SOURCE_FILES:
                CHECKER_EXCLUDE_SOURCE_FILES.remove(filepath_source_relative)
                continue
            if filepath_source_relative.startswith("intern" + os.sep + "ghost"):
                pass
            elif filepath_source_relative.startswith("source" + os.sep):
                pass
            else:
                continue

            has_match = False
            for regex in regex_list:
                if regex.match(filepath_source_relative) is not None:
                    has_match = True
            if not has_match:
                continue
        else:
            # Skip files not in source (generated files from the build directory),
            # these could be check but it's not all that useful (preview blend ... etc).
            continue

        source_info_result.append(item)

    if CHECKER_EXCLUDE_SOURCE_FILES:
        sys.stderr.write(
            "Error: exclude file(s) are missing: {!r}\n".format((list(sorted(CHECKER_EXCLUDE_SOURCE_FILES))))
        )
        sys.exit(1)

    return source_info_result


def run_checks_on_project(
        check_ids: Sequence[str],
        regex_list: Sequence[re.Pattern[str]],
        jobs: int,
) -> None:
    source_info = project_source_info.build_info(ignore_prefix_list=CHECKER_IGNORE_PREFIX)
    source_defines = project_source_info.build_defines_as_args()

    # Apply exclusion.
    source_info = source_info_filter(source_info, regex_list)

    shared_check_data_foreach_check = [
        check_class_from_id(check).setup() for check in check_ids
    ]

    all_args = []
    index = 0
    for filepath_source, inc_dirs, defs in source_info[index:]:
        args = (
            [("-I" + i) for i in inc_dirs] +
            [("-D" + d) for d in defs] +
            source_defines
        )

        all_args.append((filepath_source, args, check_ids, shared_check_data_foreach_check))

    import multiprocessing

    if jobs <= 0:
        jobs = multiprocessing.cpu_count() * 2

    if jobs > 1:
        with multiprocessing.Pool(processes=jobs) as pool:
            # No `istarmap`, use an intermediate function.
            for result in pool.imap(check_source_file_for_imap, all_args):
                if result:
                    print(result)
    else:
        for (filepath_source, args, _check_ids, shared_check_data_foreach_check) in all_args:
            result = check_source_file(filepath_source, args, check_ids, shared_check_data_foreach_check)
            if result:
                print(result)

    for (check, shared_check_data) in zip(check_ids, shared_check_data_foreach_check):
        check_class_from_id(check).teardown(shared_check_data)


def create_parser(checkers_all: Sequence[str]) -> argparse.ArgumentParser:
    from textwrap import indent

    # Create doc-string for checks.
    checks_all_docs = []
    for checker in checkers_all:
        # `%` -> `%%` is needed for `--help` not to interpret these as formatting arguments.
        checks_all_docs.append(
            "  %s\n%s" % (
                checker,
                indent(check_docstring_from_id(checker).replace("%", "%%"), '    '),
            )
        )

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        "--match",
        nargs='+',
        required=True,
        metavar="REGEX",
        help="Match file paths against this expression",
    )
    parser.add_argument(
        "--checks",
        dest="checks",
        help=(
            "Specify the check presets to run.\n\n" +
            "\n".join(checks_all_docs) + "\n"
            "Multiple checkers may be passed at once (comma separated, no spaces)."),
        required=True,
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


# -----------------------------------------------------------------------------
# Main Function

def main() -> int:
    checkers_all = check_function_get_all()
    parser = create_parser(checkers_all)
    args = parser.parse_args()

    regex_list = []

    for expr in args.match:
        try:
            regex_list.append(re.compile(expr))
        except Exception as ex:
            print("Error in expression: \"{:s}\"\n  {!r}".format(expr, ex))
            return 1

    run_checks_on_project(
        args.checks.split(','),
        regex_list,
        args.jobs,
    )

    return 0


if __name__ == "__main__":
    sys.exit(main())
