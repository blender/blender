#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Script for checking source code spelling.

   python3 tools/check_source/check_spelling.py some_soure_file.py

- Pass in a path for it to be checked recursively.
- Pass in '--strings' to check strings instead of comments.

Currently only python source is checked.
"""

import os
import argparse
import sys

from typing import (
    Callable,
    Dict,
    Generator,
    List,
    Optional,
    Set,
    Tuple,
)


# Report: word, line, column.
Report = Tuple[str, int, int]
# Cache: {filepath: length, hash, reports}.
CacheData = Dict[str, Tuple[int, bytes, List[Report]]]
# Map word to suggestions.
SuggestMap = Dict[str, str]

ONLY_ONCE = True
USE_COLOR = True

# Ignore: `/*identifier*/` as these are used in C++ for unused arguments or to denote struct members.
# These identifiers can be ignored in most cases.
USE_SKIP_SINGLE_IDENTIFIER_COMMENTS = True

_words_visited = set()
_files_visited = set()

# Lowercase word -> suggestion list.
_suggest_map: SuggestMap = {}

VERBOSE_CACHE = False

if USE_COLOR:
    COLOR_WORD = "\033[92m"
    COLOR_ENDC = "\033[0m"
else:
    COLOR_WORD = ""
    COLOR_ENDC = ""

from check_spelling_config import (
    dict_custom,
    dict_ignore,
    dict_ignore_hyphenated_prefix,
    dict_ignore_hyphenated_suffix,
    files_ignore,
    directories_ignore,
)

SOURCE_EXT = (
    "c",
    "cc",
    "inl",
    "cpp",
    "cxx",
    "hpp",
    "hxx",
    "h",
    "hh",
    "m",
    "mm",
    "metal",
    "msl",
    "glsl",
    "osl",
    "py",
)

BASEDIR = os.path.abspath(os.path.dirname(__file__))
ROOTDIR = os.path.normpath(os.path.join(BASEDIR, "..", ".."))
ROOTDIR_WITH_SLASH = ROOTDIR + os.sep

# Ensure native slashes.
files_ignore = {
    os.path.normpath(os.path.join(ROOTDIR, f.replace("/", os.sep)))
    for f in files_ignore
}

directories_ignore = {
    os.path.normpath(os.path.join(ROOTDIR, f.replace("/", os.sep)))
    for f in directories_ignore
}

# -----------------------------------------------------------------------------
# Dictionary Utilities


def dictionary_create():  # type: ignore
    import enchant  # type: ignore
    dict_spelling = enchant.Dict("en_US")

    # Don't add ignore to the dictionary, since they will be suggested.
    for w in dict_custom:
        # Also, don't use `add(w)`, this will manipulate users personal dictionaries.
        dict_spelling.add_to_session(w)
    return dict_spelling


def dictionary_check(w: str, code_words: Set[str]) -> bool:
    w_lower = w.lower()
    if w_lower in dict_ignore:
        return True

    is_correct: bool = _dict.check(w)
    # Split by hyphenation and check.
    if not is_correct:
        if "-" in w:
            is_correct = True

            # Allow: `un-word`, `re-word`.
            w_split = w.strip("-").split("-")
            if len(w_split) > 1:
                if w_split and w_split[0].lower() in dict_ignore_hyphenated_prefix:
                    del w_split[0]
            # Allow: `word-ish`, `word-ness`.
            if len(w_split) > 1:
                if w_split and w_split[-1].lower() in dict_ignore_hyphenated_suffix:
                    del w_split[-1]

            for w_sub in w_split:
                if w_sub:
                    if w_sub in code_words:
                        continue
                    w_sub_lower = w_sub.lower()
                    if w_sub_lower in dict_ignore:
                        continue
                    if not _dict.check(w_sub):
                        is_correct = False
                        break
    return is_correct


def dictionary_suggest(w: str) -> List[str]:
    return _dict.suggest(w)  # type: ignore


_dict = dictionary_create()  # type: ignore


# -----------------------------------------------------------------------------
# General Utilities

def hash_of_file_and_len(fp: str) -> Tuple[bytes, int]:
    import hashlib
    with open(fp, 'rb') as fh:
        data = fh.read()
        m = hashlib.sha512()
        m.update(data)
        return m.digest(), len(data)


import re
re_vars = re.compile("[A-Za-z]+")

# First remove this from comments, so we don't spell check example code, DOXYGEN commands, etc.
re_ignore = re.compile(
    r'('

    # URL.
    r'\b(https?|ftp)://\S+|'
    # Email address: <me@email.com>
    #                <someone@foo.bar-baz.com>
    r"<\w+@[\w\.\-]+>|"

    # Convention for TODO/FIXME messages: TODO(my name) OR FIXME(name+name) OR XXX(some-name) OR NOTE(name/other-name):
    r"\b(TODO|FIXME|XXX|NOTE|WARNING)\(@?[\w\s\+\-/]+\)|"

    # DOXYGEN style: <pre> ... </pre>
    r"<pre>.+</pre>|"
    # DOXYGEN style: \code ... \endcode
    r"\s+\\code\b.+\s\\endcode\b|"
    # DOXYGEN style #SOME_CODE.
    r'#\S+|'
    # DOXYGEN commands: \param foo
    r"\\(section|subsection|subsubsection|defgroup|ingroup|addtogroup|param|tparam|page|a|see)\s+\S+|"
    # DOXYGEN commands without any arguments after them: \command
    r"\\(retval|todo|name)\b|"
    # DOXYGEN 'param' syntax used rarely: \param foo[in,out]
    r"\\param\[[a-z,]+\]\S*|"

    # Words containing underscores: a_b
    r'\S*\w+_\S+|'
    # Words containing arrows: a->b
    r'\S*\w+\->\S+'
    # Words containing dot notation: a.b  (NOT  ab... since this is used in English).
    r'\w+\.\w+\S*|'

    # Single and back-tick quotes (often used to reference code).
    # Allow white-space or any bracket prefix, e.g:
    # (`expr a+b`)
    r"[\s\(\[\{]\`[^\n`]+\`|"
    r"[\s\(\[\{]'[^\n']+'"

    r')',
    re.MULTILINE | re.DOTALL,
)
# Then extract words.
re_words = re.compile(
    r"\b("
    # Capital words, with optional '-' and "'".
    r"[A-Z]+[\-'A-Z]*[A-Z]|"
    # Lowercase words, with optional '-' and "'".
    r"[A-Za-z][\-'a-z]*[a-z]+"
    r")\b"
)

re_not_newline = re.compile("[^\n]")

if USE_SKIP_SINGLE_IDENTIFIER_COMMENTS:
    re_single_word_c_comments = re.compile(r"\/\*[\s]*[a-zA-Z_]+[a-zA-Z0-9_]*[\s]*\*\/")


def words_from_text(text: str, check_type: str) -> List[Tuple[str, int]]:
    """ Extract words to treat as English for spell checking.
    """
    # Replace non-newlines with white-space, so all alignment is kept.
    def replace_ignore(match: re.Match[str]) -> str:
        start, end = match.span()
        return re_not_newline.sub(" ", match.string[start:end])

    # Handy for checking what we ignore, in case we ignore too much and miss real errors.
    # for match in re_ignore.finditer(text):
    #     print(match.group(0))

    # Strip out URL's, code-blocks, etc.
    text = re_ignore.sub(replace_ignore, text)

    words = []

    if check_type == 'SPELLING':
        for match in re_words.finditer(text):
            words.append((match.group(0), match.start()))

        def word_ok(w: str) -> bool:
            # Ignore all uppercase words.
            if w.isupper():
                return False
            return True
        words[:] = [w for w in words if word_ok(w[0])]

    elif check_type == 'DUPLICATES':
        w_prev = ""
        w_prev_start = 0
        for match in re_words.finditer(text):
            w = match.group(0)
            w_start = match.start()
            w_lower = w.lower()
            if w_lower == w_prev:
                text_ws = text[w_prev_start + len(w_prev): w_start]
                if text_ws == " ":
                    words.append((w_lower, w_start))
            w_prev = w_lower
            w_prev_start = w_start
    else:
        assert False, "unreachable"

    return words


class Comment:
    __slots__ = (
        "file",
        "text",
        "line",
        "type",
    )

    def __init__(self, file: str, text: str, line: int, type: str):
        self.file = file
        self.text = text
        self.line = line
        self.type = type

    def parse(self, check_type: str) -> List[Tuple[str, int]]:
        return words_from_text(self.text, check_type=check_type)

    def line_and_column_from_comment_offset(self, pos: int) -> Tuple[int, int]:
        text = self.text
        slineno = self.line + text.count("\n", 0, pos)
        # Allow for -1 to be not found.
        scol = text.rfind("\n", 0, pos) + 1
        if scol == 0:
            # Not found.
            scol = pos
        else:
            scol = pos - scol
        return slineno, scol


def extract_code_strings(filepath: str) -> Tuple[List[Comment], Set[str]]:
    from pygments import lexers
    from pygments.token import Token

    comments = []
    code_words = set()

    # lex = lexers.find_lexer_class_for_filename(filepath)
    # if lex is None:
    #     return comments, code_words
    if filepath.endswith(".py"):
        lex = lexers.get_lexer_by_name("python")
    else:
        lex = lexers.get_lexer_by_name("c")

    slineno = 0
    with open(filepath, encoding='utf-8') as fh:
        source = fh.read()

    for ty, ttext in lex.get_tokens(source):
        if ty in {Token.Literal.String, Token.Literal.String.Double, Token.Literal.String.Single}:
            comments.append(Comment(filepath, ttext, slineno, 'STRING'))
        else:
            for match in re_vars.finditer(ttext):
                code_words.add(match.group(0))
        # Ugh - not nice or fast.
        slineno += ttext.count("\n")

    return comments, code_words


def extract_py_comments(filepath: str) -> Tuple[List[Comment], Set[str]]:

    import token
    import tokenize

    source = open(filepath, encoding='utf-8')

    comments = []
    code_words = set()

    prev_toktype = token.INDENT

    tokgen = tokenize.generate_tokens(source.readline)
    for toktype, ttext, (slineno, scol), (elineno, ecol), ltext in tokgen:
        if toktype == token.STRING:
            if prev_toktype == token.INDENT:
                comments.append(Comment(filepath, ttext, slineno - 1, 'DOCSTRING'))
        elif toktype == tokenize.COMMENT:
            # non standard hint for commented CODE that we can ignore
            if not ttext.startswith("#~"):
                comments.append(Comment(filepath, ttext, slineno - 1, 'COMMENT'))
        else:
            for match in re_vars.finditer(ttext):
                code_words.add(match.group(0))

        prev_toktype = toktype
    return comments, code_words


def extract_c_comments(filepath: str) -> Tuple[List[Comment], Set[str]]:
    """
    Extracts comments like this:

        /*
         * This is a multi-line comment, notice the '*'s are aligned.
         */
    """
    text = open(filepath, encoding='utf-8').read()

    BEGIN = "/*"
    END = "*/"

    # reverse these to find blocks we won't parse
    PRINT_NON_ALIGNED = False
    PRINT_SPELLING = True

    comment_ranges = []

    if USE_SKIP_SINGLE_IDENTIFIER_COMMENTS:
        comment_ignore_offsets = set()
        for match in re_single_word_c_comments.finditer(text):
            comment_ignore_offsets.add(match.start(0))

    i = 0
    while i != -1:
        i = text.find(BEGIN, i)
        if i != -1:
            i_next = text.find(END, i)
            if i_next != -1:
                do_comment_add = True
                if USE_SKIP_SINGLE_IDENTIFIER_COMMENTS:
                    if i in comment_ignore_offsets:
                        do_comment_add = False

                # Not essential but seek back to find beginning of line.
                while i > 0 and text[i - 1] in {"\t", " "}:
                    i -= 1
                i_next += len(END)
                if do_comment_add:
                    comment_ranges.append((i, i_next))
            i = i_next
        else:
            pass

    if PRINT_NON_ALIGNED:
        for i, i_next in comment_ranges:
            # Seek i back to the line start.
            i_bol = text.rfind("\n", 0, i) + 1
            l_ofs_first = i - i_bol
            star_offsets = set()
            block = text[i_bol:i_next]
            for line_index, l in enumerate(block.split("\n")):
                star_offsets.add(l.find("*", l_ofs_first))
                l_ofs_first = 0
                if len(star_offsets) > 1:
                    print("%s:%d" % (filepath, line_index + text.count("\n", 0, i)))
                    break

    if not PRINT_SPELLING:
        return [], set()

    # Collect variables from code, so we can reference variables from code blocks
    # without this generating noise from the spell checker.

    code_ranges = []
    if not comment_ranges:
        code_ranges.append((0, len(text)))
    else:
        for index in range(len(comment_ranges) + 1):
            if index == 0:
                i_prev = 0
            else:
                i_prev = comment_ranges[index - 1][1]

            if index == len(comment_ranges):
                i_next = len(text)
            else:
                i_next = comment_ranges[index][0]

            code_ranges.append((i_prev, i_next))

    code_words = set()

    for i, i_next in code_ranges:
        for match in re_vars.finditer(text[i:i_next]):
            w = match.group(0)
            code_words.add(w)
            # Allow plurals of these variables too.
            code_words.add(w + "'s")

    comments = []

    slineno = 0
    i_prev = 0
    for i, i_next in comment_ranges:
        block = text[i:i_next]
        # Add white-space in front of the block (for alignment test)
        # allow for -1 being not found, which results as zero.
        j = text.rfind("\n", 0, i) + 1
        block = (" " * (i - j)) + block

        slineno += text.count("\n", i_prev, i)
        comments.append(Comment(filepath, block, slineno, 'COMMENT'))
        i_prev = i

    return comments, code_words


def spell_check_report(filepath: str, check_type: str, report: Report) -> None:
    w, slineno, scol = report

    if check_type == 'SPELLING':
        w_lower = w.lower()

        if ONLY_ONCE:
            if w_lower in _words_visited:
                return
            else:
                _words_visited.add(w_lower)

        suggest = _suggest_map.get(w_lower)
        if suggest is None:
            _suggest_map[w_lower] = suggest = " ".join(dictionary_suggest(w))

        print("%s:%d:%d: %s%s%s, suggest (%s)" % (
            filepath,
            slineno + 1,
            scol + 1,
            COLOR_WORD,
            w,
            COLOR_ENDC,
            suggest,
        ))
    elif check_type == 'DUPLICATES':
        print("%s:%d:%d: %s%s%s, duplicate" % (
            filepath,
            slineno + 1,
            scol + 1,
            COLOR_WORD,
            w,
            COLOR_ENDC,
        ))


def spell_check_file(
        filepath: str,
        check_type: str,
        extract_type: str = 'COMMENTS',
) -> Generator[Report, None, None]:
    if extract_type == 'COMMENTS':
        if filepath.endswith(".py"):
            comment_list, code_words = extract_py_comments(filepath)
        else:
            comment_list, code_words = extract_c_comments(filepath)
    elif extract_type == 'STRINGS':
        comment_list, code_words = extract_code_strings(filepath)
    if check_type == 'SPELLING':
        for comment in comment_list:
            words = comment.parse(check_type='SPELLING')
            for w, pos in words:
                w_lower = w.lower()
                if w_lower in dict_ignore:
                    continue

                is_good_spelling = dictionary_check(w, code_words)
                if not is_good_spelling:
                    # Ignore literals that show up in code,
                    # gets rid of a lot of noise from comments that reference variables.
                    if w in code_words:
                        # print("Skipping", w)
                        continue

                    slineno, scol = comment.line_and_column_from_comment_offset(pos)
                    yield (w, slineno, scol)
    elif check_type == 'DUPLICATES':
        for comment in comment_list:
            words = comment.parse(check_type='DUPLICATES')
            for w, pos in words:
                slineno, scol = comment.line_and_column_from_comment_offset(pos)
                # print(filepath + ":" + str(slineno + 1) + ":" + str(scol), w, "(duplicates)")
                yield (w, slineno, scol)
    else:
        assert False, "unreachable"


def spell_check_file_recursive(
        dirpath: str,
        check_type: str,
        regex_list: List[re.Pattern[str]],
        extract_type: str = 'COMMENTS',
        cache_data: Optional[CacheData] = None,
) -> None:
    import os
    from os.path import join

    def source_list(
            path: str,
            filename_check: Optional[Callable[[str], bool]] = None,
    ) -> Generator[str, None, None]:
        for dirpath, dirnames, filenames in os.walk(path):
            # Only needed so this can be matches with ignore paths.
            dirpath = os.path.abspath(dirpath)
            if dirpath in directories_ignore:
                dirnames.clear()
                continue
            # skip '.git'
            dirnames[:] = [d for d in dirnames if not d.startswith(".")]
            for filename in filenames:
                if filename.startswith("."):
                    continue
                filepath = join(dirpath, filename)
                if not (filename_check is None or filename_check(filepath)):
                    continue
                if filepath in files_ignore:
                    continue
                yield filepath

    def is_source(filename: str) -> bool:
        from os.path import splitext
        filename = filename.removeprefix(ROOTDIR_WITH_SLASH)
        for regex in regex_list:
            if regex.match(filename) is not None:
                filename
                ext = splitext(filename)[1].removeprefix(".")
                if ext not in SOURCE_EXT:
                    raise Exception("Unknown extension \".{:s}\" aborting!".format(ext))
                return True
        return False

    for filepath in source_list(dirpath, is_source):
        for report in spell_check_file_with_cache_support(
                filepath, check_type, extract_type=extract_type, cache_data=cache_data,
        ):
            spell_check_report(filepath, check_type, report)


# -----------------------------------------------------------------------------
# Cache File Support
#
# Cache is formatted as follows:
# (
#     # Store all misspelled words.
#     {filepath: (size, sha512, [reports, ...])},
#
#     # Store suggestions, as these are slow to re-calculate.
#     {lowercase_words: suggestions},
# )
#

def spell_cache_read(cache_filepath: str) -> Tuple[CacheData, SuggestMap]:
    import pickle
    cache_store: Tuple[CacheData, SuggestMap] = {}, {}
    if os.path.exists(cache_filepath):
        with open(cache_filepath, 'rb') as fh:
            cache_store = pickle.load(fh)
    return cache_store


def spell_cache_write(cache_filepath: str, cache_store: Tuple[CacheData, SuggestMap]) -> None:
    import pickle
    with open(cache_filepath, 'wb') as fh:
        pickle.dump(cache_store, fh)


def spell_check_file_with_cache_support(
        filepath: str,
        check_type: str,
        *,
        extract_type: str = 'COMMENTS',
        cache_data: Optional[CacheData] = None,
) -> Generator[Report, None, None]:
    """
    Iterator each item is a report: (word, line_number, column_number)
    """
    _files_visited.add(filepath)

    if cache_data is None:
        yield from spell_check_file(filepath, check_type, extract_type=extract_type)
        return

    cache_data_for_file = cache_data.get(filepath)
    if cache_data_for_file and len(cache_data_for_file) != 3:
        cache_data_for_file = None

    cache_hash_test, cache_len_test = hash_of_file_and_len(filepath)
    if cache_data_for_file is not None:
        cache_len, cache_hash, cache_reports = cache_data_for_file
        if cache_len_test == cache_len:
            if cache_hash_test == cache_hash:
                if VERBOSE_CACHE:
                    print("Using cache for:", filepath)
                yield from cache_reports
                return

    cache_reports = []
    for report in spell_check_file(filepath, check_type, extract_type=extract_type):
        cache_reports.append(report)

    cache_data[filepath] = (cache_len_test, cache_hash_test, cache_reports)

    yield from cache_reports


# -----------------------------------------------------------------------------
# Extract Bad Spelling from a Source File


# -----------------------------------------------------------------------------
# Main & Argument Parsing

def argparse_create() -> argparse.ArgumentParser:

    # When --help or no args are given, print this help
    description = __doc__
    parser = argparse.ArgumentParser(description=description)

    parser.add_argument(
        "--match",
        nargs='+',
        default=(
            r".*\.(" + "|".join(SOURCE_EXT) + ")$",
        ),
        required=False,
        metavar="REGEX",
        help="Match file paths against this expression",
    )

    parser.add_argument(
        '--extract',
        dest='extract',
        choices=('COMMENTS', 'STRINGS'),
        default='COMMENTS',
        required=False,
        metavar='WHAT',
        help=(
            'Text to extract for checking.\n'
            '\n'
            '- ``COMMENTS`` extracts comments from source code.\n'
            '- ``STRINGS`` extracts text.'
        ),
    )

    parser.add_argument(
        '--check',
        dest='check_type',
        choices=('SPELLING', 'DUPLICATES'),
        default='SPELLING',
        required=False,
        metavar='CHECK_TYPE',
        help=(
            'Text to extract for checking.\n'
            '\n'
            '- ``COMMENTS`` extracts comments from source code.\n'
            '- ``STRINGS`` extracts text.'
        ),
    )

    parser.add_argument(
        "--cache-file",
        dest="cache_file",
        help=(
            "Optional cache, for fast re-execution, "
            "avoiding re-extracting spelling when files have not been modified."
        ),
        required=False,
    )

    parser.add_argument(
        "paths",
        nargs='+',
        help="Files or directories to walk recursively.",
    )

    return parser


def main() -> int:
    global _suggest_map

    import os

    args = argparse_create().parse_args()

    regex_list = []
    for expr in args.match:
        try:
            regex_list.append(re.compile(expr))
        except Exception as ex:
            print("Error in expression: {!r}\n  {!r}".format(expr, ex))
            return 1

    extract_type = args.extract
    cache_filepath = args.cache_file
    check_type = args.check_type

    cache_data: Optional[CacheData] = None
    if cache_filepath:
        cache_data, _suggest_map = spell_cache_read(cache_filepath)
        clear_stale_cache = True

    # print(extract_type)
    try:
        for filepath in args.paths:
            if os.path.isdir(filepath):

                # recursive search
                spell_check_file_recursive(
                    filepath,
                    check_type,
                    regex_list=regex_list,
                    extract_type=extract_type,
                    cache_data=cache_data,
                )
            else:
                # single file
                for report in spell_check_file_with_cache_support(
                        filepath,
                        check_type,
                        extract_type=extract_type,
                        cache_data=cache_data,
                ):
                    spell_check_report(filepath, check_type, report)
    except KeyboardInterrupt:
        clear_stale_cache = False

    if cache_filepath:
        assert cache_data is not None
        if VERBOSE_CACHE:
            print("Writing cache:", len(cache_data))

        if clear_stale_cache:
            # Don't keep suggestions for old misspellings.
            _suggest_map = {w_lower: _suggest_map[w_lower] for w_lower in _words_visited}

            for filepath in list(cache_data.keys()):
                if filepath not in _files_visited:
                    del cache_data[filepath]

        spell_cache_write(cache_filepath, (cache_data, _suggest_map))
    return 0


if __name__ == "__main__":
    sys.exit(main())
