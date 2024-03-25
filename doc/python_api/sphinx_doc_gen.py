# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
API dump in RST files
---------------------
  Run this script from Blender's root path once you have compiled Blender

    blender --background --factory-startup --python doc/python_api/sphinx_doc_gen.py

  This will generate python files in doc/python_api/sphinx-in/
  providing ./blender is or links to the blender executable

  To choose sphinx-in directory:
    blender --background --factory-startup --python doc/python_api/sphinx_doc_gen.py -- --output=../python_api

  For quick builds:
    blender --background --factory-startup --python doc/python_api/sphinx_doc_gen.py -- --partial=bmesh.*


Sphinx: HTML generation
-----------------------
  After you have built doc/python_api/sphinx-in (see above),
  generate html docs by running:

    sphinx-build doc/python_api/sphinx-in doc/python_api/sphinx-out


Sphinx: PDF generation
----------------------
  After you have built doc/python_api/sphinx-in (see above),
  generate the pdf doc by running:

    sphinx-build -b latex doc/python_api/sphinx-in doc/python_api/sphinx-out
    cd doc/python_api/sphinx-out
    make
"""

try:
    import bpy  # Blender module.
except ImportError:
    print("\nERROR: this script must run from inside Blender")
    print(__doc__)
    import sys
    sys.exit()

import rna_info  # Blender module.


def rna_info_BuildRNAInfo_cache():
    if rna_info_BuildRNAInfo_cache.ret is None:
        rna_info_BuildRNAInfo_cache.ret = rna_info.BuildRNAInfo()
    return rna_info_BuildRNAInfo_cache.ret


rna_info_BuildRNAInfo_cache.ret = None
# --- end rna_info cache

import os
import sys
import inspect
import shutil
import time
import logging
import warnings

from textwrap import indent

SCRIPT_DIR = os.path.abspath(os.path.dirname(__file__))

# For now, ignore add-ons and internal sub-classes of `bpy.types.PropertyGroup`.
#
# Besides disabling this line, the main change will be to add a
# 'toctree' to 'write_rst_index' which contains the generated RST files.
# This 'toctree' can be generated automatically.
#
# See: D6261 for reference.
USE_ONLY_BUILTIN_RNA_TYPES = True

# Write a page for each static enum defined in:
# `source/blender/makesrna/RNA_enum_items.hh` so the enums can be linked to instead of being expanded everywhere.
USE_SHARED_RNA_ENUM_ITEMS_STATIC = True

if USE_SHARED_RNA_ENUM_ITEMS_STATIC:
    from _bpy import rna_enum_items_static
    rna_enum_dict = rna_enum_items_static()
    for key in ("rna_enum_dummy_NULL_items", "rna_enum_dummy_DEFAULT_items"):
        del rna_enum_dict[key]
    del key, rna_enum_items_static

    # Build enum `{pointer: identifier}` map, so any enum property pointer can
    # lookup an identifier using `InfoPropertyRNA.enum_pointer` as the key.
    rna_enum_pointer_to_id_map = {
        enum_prop.as_pointer(): key
        for key, enum_items in rna_enum_dict.items()
        # It's possible the first item is a heading (which has no identifier).
        # skip these as the `EnumProperty.enum_items` does not expose them.
        if (enum_prop := next(iter(enum_prop for enum_prop in enum_items if enum_prop.identifier), None))
    }


def handle_args():
    """
    Parse the args passed to Blender after "--", ignored by Blender
    """
    import argparse

    # When --help is given, print the usage text
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawTextHelpFormatter,
        usage=__doc__
    )

    # optional arguments
    parser.add_argument(
        "-p", "--partial",
        dest="partial",
        type=str,
        default="",
        help="Use a wildcard to only build specific module(s)\n"
        "Example: --partial\"=bmesh*\"\n",
        required=False,
    )

    parser.add_argument(
        "-f", "--fullrebuild",
        dest="full_rebuild",
        default=False,
        action='store_true',
        help="Rewrite all RST files in sphinx-in/ "
        "(default=False)",
        required=False,
    )

    parser.add_argument(
        "-b", "--bpy",
        dest="bpy",
        default=False,
        action='store_true',
        help="Write the RST file of the bpy module "
        "(default=False)",
        required=False,
    )

    parser.add_argument(
        "--api-changelog-generate",
        dest="changelog",
        default=False,
        action='store_true',
        help="Generate the API changelog RST file "
        "(default=False, requires `--api-dump-index-path` parameter)",
        required=False,
    )

    parser.add_argument(
        "--api-dump-index-path",
        dest="api_dump_index_path",
        metavar='FILE',
        default=None,
        help="Path to the API dump index JSON file "
        "(required when `--api-changelog-generate` is True)",
        required=False,
    )

    parser.add_argument(
        "-o", "--output",
        dest="output_dir",
        type=str,
        default=SCRIPT_DIR,
        help="Path of the API docs (default=<script dir>)",
        required=False,
    )

    parser.add_argument(
        "-B", "--sphinx-build",
        dest="sphinx_build",
        default=False,
        action='store_true',
        help="Build the html docs by running:\n"
        "sphinx-build SPHINX_IN SPHINX_OUT\n"
        "(default=False; does not depend on -P)",
        required=False,
    )

    parser.add_argument(
        "-P", "--sphinx-build-pdf",
        dest="sphinx_build_pdf",
        default=False,
        action='store_true',
        help="Build the pdf by running:\n"
        "sphinx-build -b latex SPHINX_IN SPHINX_OUT_PDF\n"
        "(default=False; does not depend on -B)",
        required=False,
    )

    parser.add_argument(
        "-R", "--pack-reference",
        dest="pack_reference",
        default=False,
        action='store_true',
        help="Pack all necessary files in the deployed dir.\n"
        "(default=False; use with -B and -P)",
        required=False,
    )

    parser.add_argument(
        "-l", "--log",
        dest="log",
        default=False,
        action='store_true',
        help=(
            "Log the output of the API dump and sphinx|latex "
            "warnings and errors (default=False).\n"
            "If given, save logs in:\n"
            "* OUTPUT_DIR/.bpy.log\n"
            "* OUTPUT_DIR/.sphinx-build.log\n"
            "* OUTPUT_DIR/.sphinx-build_pdf.log\n"
            "* OUTPUT_DIR/.latex_make.log"
        ),
        required=False,
    )

    # Parse only the arguments passed after "--".
    argv = []
    if "--" in sys.argv:
        argv = sys.argv[sys.argv.index("--") + 1:]  # Get all arguments after "--".

    return parser.parse_args(argv)


ARGS = handle_args()

# ----------------------------------BPY-----------------------------------------

BPY_LOGGER = logging.getLogger("bpy")
BPY_LOGGER.setLevel(logging.DEBUG)

"""
# for quick rebuilds
rm -rf /b/doc/python_api/sphinx-* && \
./blender -b --factory-startup -P doc/python_api/sphinx_doc_gen.py && \
sphinx-build doc/python_api/sphinx-in doc/python_api/sphinx-out

or

./blender -b --factory-startup -P doc/python_api/sphinx_doc_gen.py -- -f -B
"""

# Switch for quick testing so doc-builds don't take so long.
if not ARGS.partial:
    # full build
    FILTER_BPY_OPS = None
    FILTER_BPY_TYPES = None
    EXCLUDE_INFO_DOCS = False
    EXCLUDE_MODULES = []

else:
    # can manually edit this too:
    # FILTER_BPY_OPS = ("import.scene", )  # allow
    # FILTER_BPY_TYPES = ("bpy_struct", "Operator", "ID")  # allow
    EXCLUDE_INFO_DOCS = True
    EXCLUDE_MODULES = [
        "aud",
        "bgl",
        "blf",
        "bl_math",
        "imbuf",
        "imbuf.types",
        "bmesh",
        "bmesh.ops",
        "bmesh.types",
        "bmesh.utils",
        "bmesh.geometry",
        "bpy.app",
        "bpy.app.handlers",
        "bpy.app.timers",
        "bpy.app.translations",
        "bpy.context",
        "bpy.data",
        "bpy.ops",  # supports filtering
        "bpy.path",
        "bpy.props",
        "bpy.types",  # supports filtering
        "bpy.utils",
        "bpy.utils.previews",
        "bpy.utils.units",
        "bpy_extras",
        "gpu",
        "gpu.types",
        "gpu.matrix",
        "gpu.select",
        "gpu.shader",
        "gpu.state",
        "gpu.texture",
        "gpu.platform",
        "gpu.capabilities",
        "gpu_extras",
        "idprop.types",
        "mathutils",
        "mathutils.bvhtree",
        "mathutils.geometry",
        "mathutils.interpolate",
        "mathutils.kdtree",
        "mathutils.noise",
        "freestyle",
        "freestyle.chainingiterators",
        "freestyle.functions",
        "freestyle.predicates",
        "freestyle.shaders",
        "freestyle.types",
        "freestyle.utils",
    ]

    # ------
    # Filter
    #
    # TODO: support `bpy.ops` and `bpy.types` filtering.
    import fnmatch
    m = None
    EXCLUDE_MODULES = [m for m in EXCLUDE_MODULES if not fnmatch.fnmatchcase(m, ARGS.partial)]

    # special support for bpy.types.XXX
    FILTER_BPY_OPS = tuple([m[8:] for m in ARGS.partial.split(":") if m.startswith("bpy.ops.")])
    if FILTER_BPY_OPS:
        EXCLUDE_MODULES.remove("bpy.ops")

    FILTER_BPY_TYPES = tuple([m[10:] for m in ARGS.partial.split(":") if m.startswith("bpy.types.")])
    if FILTER_BPY_TYPES:
        EXCLUDE_MODULES.remove("bpy.types")

    # print(FILTER_BPY_TYPES)

    EXCLUDE_INFO_DOCS = (not fnmatch.fnmatchcase("info", ARGS.partial))

    del m
    del fnmatch

    BPY_LOGGER.debug(
        "Partial Doc Build, Skipping: %s\n",
        "\n                             ".join(sorted(EXCLUDE_MODULES)))

    #
    # done filtering
    # --------------

try:
    __import__("aud")
except ImportError:
    BPY_LOGGER.debug("Warning: Built without \"aud\" module, docs incomplete...")
    EXCLUDE_MODULES.append("aud")

try:
    __import__("freestyle")
except ImportError:
    BPY_LOGGER.debug("Warning: Built without \"freestyle\" module, docs incomplete...")
    EXCLUDE_MODULES.extend([
        "freestyle",
        "freestyle.chainingiterators",
        "freestyle.functions",
        "freestyle.predicates",
        "freestyle.shaders",
        "freestyle.types",
        "freestyle.utils",
    ])

# Source files we use, and need to copy to the OUTPUT_DIR
# to have working out-of-source builds.
# Note that ".." is replaced by "__" in the RST files,
# to avoid having to match Blender's source tree.
EXTRA_SOURCE_FILES = (
    "../../../scripts/templates_py/bmesh_simple.py",
    "../../../scripts/templates_py/gizmo_operator.py",
    "../../../scripts/templates_py/gizmo_operator_target.py",
    "../../../scripts/templates_py/gizmo_simple.py",
    "../../../scripts/templates_py/operator_simple.py",
    "../../../scripts/templates_py/ui_panel_simple.py",
    "../../../scripts/templates_py/ui_previews_custom_icon.py",
    "../examples/bmesh.ops.1.py",
    "../examples/bpy.app.translations.py",
)


# examples
EXAMPLES_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "examples"))
EXAMPLE_SET = set(os.path.splitext(f)[0] for f in os.listdir(EXAMPLES_DIR) if f.endswith(".py"))
EXAMPLE_SET_USED = set()

# RST files directory.
RST_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "rst"))

# Extra info, not api reference docs stored in `./rst/info_*`.
# Pairs of (file, description), the title makes from the RST files are displayed before the description.
INFO_DOCS = (
    ("info_quickstart.rst",
     "New to Blender or scripting and want to get your feet wet?"),
    ("info_overview.rst",
     "A more complete explanation of Python integration."),
    ("info_api_reference.rst",
     "Examples of how to use the API reference docs."),
    ("info_best_practice.rst",
     "Conventions to follow for writing good scripts."),
    ("info_tips_and_tricks.rst",
     "Hints to help you while writing scripts for Blender."),
    ("info_gotcha.rst",
     "Some of the problems you may encounter when writing scripts."),
    ("info_advanced.rst",
     "Topics which may not be required for typical usage."),
    ("change_log.rst",
     "List of changes since last Blender release"),
)
# Referenced indirectly.
INFO_DOCS_OTHER = (
    # Included by: `info_advanced.rst`.
    "info_advanced_blender_as_bpy.rst",
)

# Hide the actual TOC, use a separate list that links to the items.
# This is done so a short description can be included with each link.
USE_INFO_DOCS_FANCY_INDEX = True

# only support for properties atm.
RNA_BLACKLIST = {
    # XXX messes up PDF!, really a bug but for now just workaround.
    "PreferencesSystem": {"language", }
}

# Support suppressing errors when attributes collide with methods,
# use `noindex` on the attributes / data declarations.
#
# NOTE: in general this should be avoided but changing it would break the API,
# so explicitly suppress warnings instead.
#
# NOTE: Currently some API generation doesn't support this is it is not used yet,
# see references to `RST_NOINDEX_ATTR` in code comments.
#
# A set of tuple identifiers: `(module, type, attr)`.
RST_NOINDEX_ATTR = {
    # Render is both a method and an attribute, from looking into this
    # having both doesn't cause problems in practice since the `render` method
    # is registered and called from C code where the attribute is accessed from the instance.
    ("bpy.types", "RenderEngine", "render"),
}

MODULE_GROUPING = {
    "bmesh.types": (
        ("Base Mesh Type", "-"),
        "BMesh",
        ("Mesh Elements", "-"),
        "BMVert",
        "BMEdge",
        "BMFace",
        "BMLoop",
        ("Sequence Accessors", "-"),
        "BMElemSeq",
        "BMVertSeq",
        "BMEdgeSeq",
        "BMFaceSeq",
        "BMLoopSeq",
        "BMIter",
        ("Selection History", "-"),
        "BMEditSelSeq",
        "BMEditSelIter",
        ("Custom-Data Layer Access", "-"),
        "BMLayerAccessVert",
        "BMLayerAccessEdge",
        "BMLayerAccessFace",
        "BMLayerAccessLoop",
        "BMLayerCollection",
        "BMLayerItem",
        ("Custom-Data Layer Types", "-"),
        "BMLoopUV",
        "BMDeformVert",
    )
}

# --------------------configure compile time options----------------------------

# -------------------------------BLENDER----------------------------------------

# Converting bytes to strings, due to #30154.
BLENDER_REVISION = str(bpy.app.build_hash, 'utf_8')
BLENDER_REVISION_TIMESTAMP = bpy.app.build_commit_timestamp

# '2.83.0 Beta' or '2.83.0' or '2.83.1'
BLENDER_VERSION_STRING = bpy.app.version_string
BLENDER_VERSION_DOTS = "%d.%d" % (bpy.app.version[0], bpy.app.version[1])

if BLENDER_REVISION != "Unknown":
    # SHA1 Git hash
    BLENDER_VERSION_HASH = BLENDER_REVISION
    BLENDER_VERSION_HASH_HTML_LINK = "<a href=https://projects.blender.org/blender/blender/commit/%s>%s</a>" % (
        BLENDER_VERSION_HASH, BLENDER_VERSION_HASH,
    )
    BLENDER_VERSION_DATE = time.strftime("%d/%m/%Y", time.localtime(BLENDER_REVISION_TIMESTAMP))
else:
    # Fallback: Should not be used
    BLENDER_VERSION_HASH = "Hash Unknown"
    BLENDER_VERSION_HASH_HTML_LINK = BLENDER_VERSION_HASH
    BLENDER_VERSION_DATE = time.strftime("%Y-%m-%d")

# Example: `2_83`.
BLENDER_VERSION_PATH = "%d_%d" % (bpy.app.version[0], bpy.app.version[1])

# --------------------------DOWNLOADABLE FILES----------------------------------

REFERENCE_NAME = "blender_python_reference_%s" % BLENDER_VERSION_PATH
REFERENCE_PATH = os.path.join(ARGS.output_dir, REFERENCE_NAME)
BLENDER_PDF_FILENAME = "%s.pdf" % REFERENCE_NAME
BLENDER_ZIP_FILENAME = "%s.zip" % REFERENCE_NAME

# -------------------------------SPHINX-----------------------------------------

SPHINX_IN = os.path.join(ARGS.output_dir, "sphinx-in")
SPHINX_IN_TMP = SPHINX_IN + "-tmp"
SPHINX_OUT = os.path.join(ARGS.output_dir, "sphinx-out")

# html build
if ARGS.sphinx_build:
    SPHINX_BUILD = ["sphinx-build", SPHINX_IN, SPHINX_OUT]

    if ARGS.log:
        SPHINX_BUILD_LOG = os.path.join(ARGS.output_dir, ".sphinx-build.log")
        SPHINX_BUILD = [
            "sphinx-build",
            "-w", SPHINX_BUILD_LOG,
            SPHINX_IN, SPHINX_OUT,
        ]

# pdf build
if ARGS.sphinx_build_pdf:
    SPHINX_OUT_PDF = os.path.join(ARGS.output_dir, "sphinx-out_pdf")
    SPHINX_BUILD_PDF = [
        "sphinx-build",
        "-b", "latex",
        SPHINX_IN, SPHINX_OUT_PDF,
    ]
    SPHINX_MAKE_PDF = ["make", "-C", SPHINX_OUT_PDF]
    SPHINX_MAKE_PDF_STDOUT = None

    if ARGS.log:
        SPHINX_BUILD_PDF_LOG = os.path.join(ARGS.output_dir, ".sphinx-build_pdf.log")
        SPHINX_BUILD_PDF = [
            "sphinx-build", "-b", "latex",
            "-w", SPHINX_BUILD_PDF_LOG,
            SPHINX_IN, SPHINX_OUT_PDF,
        ]
        sphinx_make_pdf_log = os.path.join(ARGS.output_dir, ".latex_make.log")
        SPHINX_MAKE_PDF_STDOUT = open(sphinx_make_pdf_log, "w", encoding="utf-8")


# --------------------------------CHANGELOG GENERATION--------------------------------------

def generate_changelog():
    import importlib.util
    spec = importlib.util.spec_from_file_location(
        "sphinx_changelog_gen",
        os.path.abspath(os.path.join(SCRIPT_DIR, "sphinx_changelog_gen.py")),
    )
    sphinx_changelog_gen = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(sphinx_changelog_gen)

    API_DUMP_INDEX_FILEPATH = ARGS.api_dump_index_path
    API_DUMP_ROOT = os.path.dirname(API_DUMP_INDEX_FILEPATH)
    API_DUMP_FILEPATH = os.path.abspath(os.path.join(API_DUMP_ROOT, BLENDER_VERSION_DOTS, "api_dump.json"))
    API_CHANGELOG_FILEPATH = os.path.abspath(os.path.join(SPHINX_IN_TMP, "change_log.rst"))

    sphinx_changelog_gen.main((
        "--",
        "--indexpath",
        API_DUMP_INDEX_FILEPATH,
        "dump",
        "--filepath-out",
        API_DUMP_FILEPATH,
    ))

    sphinx_changelog_gen.main((
        "--",
        "--indexpath",
        API_DUMP_INDEX_FILEPATH,
        "changelog",
        "--filepath-out",
        API_CHANGELOG_FILEPATH,
    ))


# --------------------------------API DUMP--------------------------------------

# Lame, python won't give some access.
ClassMethodDescriptorType = type(dict.__dict__["fromkeys"])
MethodDescriptorType = type(dict.get)
GetSetDescriptorType = type(int.real)
StaticMethodType = type(staticmethod(lambda: None))
from types import (
    MemberDescriptorType,
    MethodType,
    FunctionType,
)

_BPY_STRUCT_FAKE = "bpy_struct"
_BPY_PROP_COLLECTION_FAKE = "bpy_prop_collection"

if _BPY_PROP_COLLECTION_FAKE:
    _BPY_PROP_COLLECTION_ID = ":class:`%s`" % _BPY_PROP_COLLECTION_FAKE
else:
    _BPY_PROP_COLLECTION_ID = "collection"

if _BPY_STRUCT_FAKE:
    bpy_struct = bpy.types.bpy_struct
else:
    bpy_struct = None


def import_value_from_module(module_name, import_name):
    ns = {}
    exec_str = "from %s import %s as value" % (module_name, import_name)
    exec(exec_str, ns, ns)
    return ns["value"]


def execfile(filepath):
    global_namespace = {"__file__": filepath, "__name__": "__main__"}
    with open(filepath, encoding="utf-8") as file_handle:
        exec(compile(file_handle.read(), filepath, 'exec'), global_namespace)


def escape_rst(text):
    """
    Escape plain text which may contain characters used by RST.
    """
    return text.translate(escape_rst.trans)


escape_rst.trans = str.maketrans({
    "`": "\\`",
    "|": "\\|",
    "*": "\\*",
    "\\": "\\\\",
})


def is_struct_seq(value):
    return isinstance(value, tuple) and type(value) != tuple and hasattr(value, "n_fields")


def undocumented_message(module_name, type_name, identifier):
    BPY_LOGGER.debug(
        "Undocumented: module %s, type: %s, id: %s is not documented",
        module_name, type_name, identifier,
    )

    return "Undocumented, consider `contributing <https://developer.blender.org/>`__."


def range_str(val):
    """
    Converts values to strings for the range directive.
    (unused function it seems)
    """
    if val < -10000000:
        return "-inf"
    elif val > 10000000:
        return "inf"
    elif type(val) == float:
        return "%g" % val
    else:
        return str(val)


def example_extract_docstring(filepath):
    """
    Return (text, line_no, line_no_has_content) where:
    - ``text`` is the doc-string text.
    - ``line_no`` is the line the doc-string text ends.
    - ``line_no_has_content`` when False, this file only contains a doc-string.
      There is no need to include the remainder.
    """
    file = open(filepath, "r", encoding="utf-8")
    line = file.readline()
    line_no = 0
    text = []
    if line.startswith('"""'):  # assume nothing here
        line_no += 1
    else:
        file.close()
        return "", 0, True

    for line in file:
        line_no += 1
        if line.startswith('"""'):
            break
        text.append(line.rstrip())

    line_no += 1
    line_no_has_content = False

    # Skip over blank lines so the Python code doesn't have blank lines at the top.
    for line in file:
        if line.strip():
            line_no_has_content = True
            break
        line_no += 1

    file.close()
    return "\n".join(text).rstrip("\n"), line_no, line_no_has_content


def title_string(text, heading_char, double=False):
    filler = len(text) * heading_char

    if double:
        return "%s\n%s\n%s\n\n" % (filler, text, filler)
    else:
        return "%s\n%s\n\n" % (text, filler)


def write_example_ref(ident, fw, example_id, ext="py"):
    if example_id in EXAMPLE_SET:

        # Extract the comment.
        filepath = os.path.join("..", "examples", "%s.%s" % (example_id, ext))
        filepath_full = os.path.join(os.path.dirname(fw.__self__.name), filepath)

        text, line_no, line_no_has_content = example_extract_docstring(filepath_full)
        if text:
            # Ensure a blank line, needed since in some cases the indentation doesn't match the previous line.
            # which causes Sphinx not to warn about bad indentation.
            fw("\n")
            for line in text.split("\n"):
                fw("%s\n" % (ident + line).rstrip())

        fw("\n")

        # Some files only contain a doc-string.
        if line_no_has_content:
            fw("%s.. literalinclude:: %s\n" % (ident, filepath))
            if line_no > 0:
                fw("%s   :lines: %d-\n" % (ident, line_no))
            fw("\n")
        EXAMPLE_SET_USED.add(example_id)
    else:
        if bpy.app.debug:
            BPY_LOGGER.debug("\tskipping example: %s", example_id)

    # Support for numbered files `bpy.types.Operator` -> `bpy.types.Operator.1.py`.
    i = 1
    while True:
        example_id_num = "%s.%d" % (example_id, i)
        if example_id_num in EXAMPLE_SET:
            write_example_ref(ident, fw, example_id_num, ext)
            i += 1
        else:
            break


def write_indented_lines(ident, fn, text, strip=True):
    """
    Apply same indentation to all lines in a multi-lines text.
    """
    if text is None:
        return

    lines = text.split("\n")

    # Strip empty lines from the start/end.
    while lines and not lines[0].strip():
        del lines[0]
    while lines and not lines[-1].strip():
        del lines[-1]

    if strip:
        # Set indentation to `<indent>`.
        ident_strip = 1000
        for l in lines:
            if l.strip():
                ident_strip = min(ident_strip, len(l) - len(l.lstrip()))
        for l in lines:
            fn(ident + l[ident_strip:] + "\n")
    else:
        # Add <indent> number of blanks to the current indentation.
        for l in lines:
            fn(ident + l + "\n")


def pyfunc2sphinx(ident, fw, module_name, type_name, identifier, py_func, is_class=True):
    """
    function or class method to sphinx
    """

    if type(py_func) == MethodType:
        return

    arg_str = str(inspect.signature(py_func))

    if not is_class:
        func_type = "function"

        # The rest are class methods.
    elif arg_str.startswith("(self, ") or arg_str == "(self)":
        arg_str = "()" if (arg_str == "(self)") else ("(" + arg_str[7:])
        func_type = "method"
    elif arg_str.startswith("(cls, "):
        arg_str = "()" if (arg_str == "(cls)") else ("(" + arg_str[6:])
        func_type = "classmethod"
    else:
        func_type = "staticmethod"

    doc = py_func.__doc__
    if (not doc) or (not doc.startswith(".. %s:: " % func_type)):
        fw(ident + ".. %s:: %s%s\n\n" % (func_type, identifier, arg_str))
        ident_temp = ident + "   "
    else:
        ident_temp = ident

    if doc:
        write_indented_lines(ident_temp, fw, doc)
        fw("\n")
    del doc, ident_temp

    if is_class:
        write_example_ref(ident + "   ", fw, module_name + "." + type_name + "." + identifier)
    else:
        write_example_ref(ident + "   ", fw, module_name + "." + identifier)


def py_descr2sphinx(ident, fw, descr, module_name, type_name, identifier):
    if identifier.startswith("_"):
        return

    doc = descr.__doc__
    if not doc:
        doc = undocumented_message(module_name, type_name, identifier)

    if type(descr) == GetSetDescriptorType:
        fw(ident + ".. attribute:: %s\n\n" % identifier)
        # NOTE: `RST_NOINDEX_ATTR` currently not supported (as it's not used).
        write_indented_lines(ident + "   ", fw, doc, False)
        fw("\n")
    elif type(descr) == MemberDescriptorType:  # same as above but use "data"
        fw(ident + ".. data:: %s\n\n" % identifier)
        # NOTE: `RST_NOINDEX_ATTR` currently not supported (as it's not used).
        write_indented_lines(ident + "   ", fw, doc, False)
        fw("\n")
    elif type(descr) in {MethodDescriptorType, ClassMethodDescriptorType}:
        write_indented_lines(ident, fw, doc, False)
        fw("\n")
    else:
        raise TypeError("type was not GetSetDescriptorType, MethodDescriptorType or ClassMethodDescriptorType")

    write_example_ref(ident + "   ", fw, module_name + "." + type_name + "." + identifier)
    fw("\n")


def py_c_func2sphinx(ident, fw, module_name, type_name, identifier, py_func, is_class=True):
    """
    C defined function to sphinx.
    """

    # Dump the doc-string, assume its formatted correctly.
    if py_func.__doc__:
        write_indented_lines(ident, fw, py_func.__doc__, False)
        fw("\n")
    else:
        fw(ident + ".. function:: %s()\n\n" % identifier)
        fw(ident + "   " + undocumented_message(module_name, type_name, identifier))

    if is_class:
        write_example_ref(ident + "   ", fw, module_name + "." + type_name + "." + identifier)
    else:
        write_example_ref(ident + "   ", fw, module_name + "." + identifier)

    fw("\n")


def pyprop2sphinx(ident, fw, identifier, py_prop):
    """
    Python property to sphinx
    """
    # Read-only properties use "data" directive, variables use "attribute" directive.
    if py_prop.fset is None:
        fw(ident + ".. data:: %s\n\n" % identifier)
    else:
        fw(ident + ".. attribute:: %s\n\n" % identifier)

    # NOTE: `RST_NOINDEX_ATTR` currently not supported (as it's not used).
    write_indented_lines(ident + "   ", fw, py_prop.__doc__)
    fw("\n")
    if py_prop.fset is None:
        fw(ident + "   (readonly)\n\n")


def pymodule2sphinx(basepath, module_name, module, title, module_all_extra):
    import types
    attribute_set = set()
    filepath = os.path.join(basepath, module_name + ".rst")

    module_all = getattr(module, "__all__", None)
    module_dir = sorted(dir(module))

    if module_all:
        module_dir = module_all

    # TODO: currently only used for classes.
    # Grouping support.
    module_grouping = MODULE_GROUPING.get(module_name)

    def module_grouping_index(name):
        if module_grouping is not None:
            try:
                return module_grouping.index(name)
            except ValueError:
                pass
        return -1

    def module_grouping_heading(name):
        if module_grouping is not None:
            i = module_grouping_index(name) - 1
            if i >= 0 and type(module_grouping[i]) == tuple:
                return module_grouping[i]
        return None, None

    def module_grouping_sort_key(name):
        return module_grouping_index(name)
    # Done grouping support.

    file = open(filepath, "w", encoding="utf-8")

    fw = file.write

    fw(title_string("%s (%s)" % (title, module_name), "="))

    fw(".. module:: %s\n\n" % module_name)

    if module.__doc__:
        # Note, may contain sphinx syntax, don't mangle!
        fw(module.__doc__.strip())
        fw("\n\n")

    # Write sub-modules.
    # We could also scan files but this ensures `__all__` is used correctly.
    if module_all or module_all_extra:
        submod_name = None
        submod = None
        submod_ls = []
        for submod_name in (module_all or ()):
            submod = import_value_from_module(module_name, submod_name)
            if type(submod) == types.ModuleType:
                submod_ls.append((submod_name, submod))

        for submod_name in module_all_extra:
            if submod_name in attribute_set:
                continue
            submod = import_value_from_module(module_name, submod_name)
            # No type checks, since there are non-module types we treat as modules
            # such as `bpy.app.translations` & `bpy.app.handlers`.
            submod_ls.append((submod_name, submod))

        del submod_name
        del submod

        if submod_ls:
            fw(".. toctree::\n")
            fw("   :maxdepth: 1\n")
            fw("   :caption: Submodules\n\n")

            for submod_name, submod in submod_ls:
                submod_name_full = "%s.%s" % (module_name, submod_name)
                fw("   %s.rst\n" % submod_name_full)

                pymodule2sphinx(basepath, submod_name_full, submod, "%s submodule" % module_name, ())
            fw("\n")
        del submod_ls
    # Done writing sub-modules!

    write_example_ref("", fw, module_name)

    # write members of the module
    # only tested with PyStructs which are not exactly modules
    for key, descr in sorted(type(module).__dict__.items()):
        if key.startswith("__"):
            continue
        if key in module_all_extra:
            continue
        # Naughty! We also add `getset` to `PyStruct`, this is not typical Python but also not incorrect.

        # `type_name` is only used for examples and messages:
        # `<class 'bpy.app.handlers'>` -> `bpy.app.handlers`.
        type_name = str(type(module)).strip("<>").split(" ", 1)[-1][1:-1]

        # The type typically contains the module in the case of PyStruct's (defined by Blender).
        # Assign a temporary module name: `module_name_split`.
        if module_name == type_name:
            assert "." in module_name
            module_name_split, type_name = module_name.rpartition(".")[0::2]
        elif type_name.startswith(module_name + "."):
            type_name = type_name.removeprefix(module_name + ".")
        else:
            module_name_split = module_name

        if type(descr) == types.GetSetDescriptorType:
            py_descr2sphinx("", fw, descr, module_name_split, type_name, key)
            attribute_set.add(key)
        del module_name_split
    descr_sorted = []
    for key, descr in sorted(type(module).__dict__.items()):
        if key.startswith("__"):
            continue

        if type(descr) == MemberDescriptorType:
            if descr.__doc__:
                value = getattr(module, key, None)

                value_type = type(value)
                descr_sorted.append((key, descr, value, type(value)))
    # Sort by the value type.
    descr_sorted.sort(key=lambda descr_data: str(descr_data[3]))
    for key, descr, value, value_type in descr_sorted:
        if key in module_all_extra:
            continue

        # Must be documented as a sub-module.
        if is_struct_seq(value):
            continue

        type_name = value_type.__name__
        py_descr2sphinx("", fw, descr, module_name, type_name, key)

        attribute_set.add(key)

    del key, descr, descr_sorted

    classes = []
    submodules = []

    # Use this list so we can sort by type.
    module_dir_value_type = []

    for attribute in module_dir:
        if attribute.startswith("_"):
            continue

        if attribute in attribute_set:
            continue

        if attribute.startswith("n_"):  # Annoying exception, needed for `bpy.app`.
            continue

        # workaround for bpy.app documenting .index() and .count()
        if isinstance(module, tuple) and hasattr(tuple, attribute):
            continue

        value = getattr(module, attribute)

        module_dir_value_type.append((attribute, value, type(value)))

    # sort by str of each type
    # this way lists, functions etc are grouped.
    module_dir_value_type.sort(key=lambda triple: str(triple[2]))

    for attribute, value, value_type in module_dir_value_type:
        if attribute in module_all_extra:
            continue

        if value_type == FunctionType:
            pyfunc2sphinx("", fw, module_name, None, attribute, value, is_class=False)
        # Both the same at the moment but to be future proof.
        elif value_type in {types.BuiltinMethodType, types.BuiltinFunctionType}:
            # NOTE: can't get args from these, so dump the string as is
            # this means any module used like this must have fully formatted doc-strings.
            py_c_func2sphinx("", fw, module_name, None, attribute, value, is_class=False)
        elif value_type == type:
            classes.append((attribute, value))
        elif issubclass(value_type, types.ModuleType):
            submodules.append((attribute, value))
        elif issubclass(value_type, (bool, int, float, str, tuple)):
            # Constant, not much fun we can do here except to list it.
            # TODO: figure out some way to document these!
            fw(".. data:: %s\n\n" % attribute)
            write_indented_lines("   ", fw, "Constant value %s" % repr(value), False)
            fw("\n")
        else:
            BPY_LOGGER.debug("\tnot documenting %s.%s of %r type", module_name, attribute, value_type.__name__)
            continue

        attribute_set.add(attribute)
        # TODO: more types.
    del module_dir_value_type

    # TODO: `bpy_extras` does this already, `mathutils` not.
    """
    if submodules:
        fw("\n"
           "**********\n"
           "Submodules\n"
           "**********\n"
           "\n"
           )
        for attribute, submod in submodules:
            fw("* :mod:`%s.%s`\n" % (module_name, attribute))
        fw("\n")
    """

    if module_grouping is not None:
        classes.sort(key=lambda pair: module_grouping_sort_key(pair[0]))

    # Write collected classes now.
    for (type_name, value) in classes:

        if module_grouping is not None:
            heading, heading_char = module_grouping_heading(type_name)
            if heading:
                fw(title_string(heading, heading_char))

        # May need to be its own function.
        if value.__doc__:
            if value.__doc__.startswith(".. class::"):
                fw(value.__doc__)
            else:
                fw(".. class:: %s\n\n" % type_name)
                write_indented_lines("   ", fw, value.__doc__, True)
        else:
            fw(".. class:: %s\n\n" % type_name)
        fw("\n")

        write_example_ref("   ", fw, module_name + "." + type_name)

        descr_items = [(key, descr) for key, descr in sorted(value.__dict__.items()) if not key.startswith("_")]

        for key, descr in descr_items:
            if type(descr) == ClassMethodDescriptorType:
                py_descr2sphinx("   ", fw, descr, module_name, type_name, key)

        # Needed for pure Python classes.
        for key, descr in descr_items:
            if type(descr) == FunctionType:
                pyfunc2sphinx("   ", fw, module_name, type_name, key, descr, is_class=True)

        for key, descr in descr_items:
            if type(descr) == MethodDescriptorType:
                py_descr2sphinx("   ", fw, descr, module_name, type_name, key)

        for key, descr in descr_items:
            if type(descr) == GetSetDescriptorType:
                py_descr2sphinx("   ", fw, descr, module_name, type_name, key)

        for key, descr in descr_items:
            if type(descr) == StaticMethodType:
                descr = getattr(value, key)
                write_indented_lines("   ", fw, descr.__doc__ or "Undocumented", False)
                fw("\n")

        fw("\n\n")

    file.close()


# Changes In Blender will force errors here.
context_type_map = {
    # context_member: (RNA type, is_collection)
    "active_action": ("Action", False),
    "active_annotation_layer": ("GPencilLayer", False),
    "active_bone": ("EditBone", False),
    "active_file": ("FileSelectEntry", False),
    "active_gpencil_frame": ("GreasePencilLayer", True),
    "active_gpencil_layer": ("GPencilLayer", True),
    "active_node": ("Node", False),
    "active_object": ("Object", False),
    "active_operator": ("Operator", False),
    "active_pose_bone": ("PoseBone", False),
    "active_sequence_strip": ("Sequence", False),
    "active_editable_fcurve": ("FCurve", False),
    "active_nla_strip": ("NlaStrip", False),
    "active_nla_track": ("NlaTrack", False),
    "annotation_data": ("GreasePencil", False),
    "annotation_data_owner": ("ID", False),
    "armature": ("Armature", False),
    "asset_library_reference": ("AssetLibraryReference", False),
    "bone": ("Bone", False),
    "brush": ("Brush", False),
    "camera": ("Camera", False),
    "cloth": ("ClothModifier", False),
    "collection": ("LayerCollection", False),
    "collision": ("CollisionModifier", False),
    "curve": ("Curve", False),
    "dynamic_paint": ("DynamicPaintModifier", False),
    "edit_bone": ("EditBone", False),
    "edit_image": ("Image", False),
    "edit_mask": ("Mask", False),
    "edit_movieclip": ("MovieClip", False),
    "edit_object": ("Object", False),
    "edit_text": ("Text", False),
    "editable_bones": ("EditBone", True),
    "editable_gpencil_layers": ("GPencilLayer", True),
    "editable_gpencil_strokes": ("GPencilStroke", True),
    "editable_objects": ("Object", True),
    "editable_fcurves": ("FCurve", True),
    "fluid": ("FluidSimulationModifier", False),
    "gpencil": ("GreasePencil", False),
    "gpencil_data": ("GreasePencil", False),
    "grease_pencil": ("GreasePencilv3", False),
    "gpencil_data_owner": ("ID", False),
    "curves": ("Hair Curves", False),
    "id": ("ID", False),
    "image_paint_object": ("Object", False),
    "lattice": ("Lattice", False),
    "light": ("Light", False),
    "lightprobe": ("LightProbe", False),
    "line_style": ("FreestyleLineStyle", False),
    "material": ("Material", False),
    "material_slot": ("MaterialSlot", False),
    "mesh": ("Mesh", False),
    "meta_ball": ("MetaBall", False),
    "object": ("Object", False),
    "objects_in_mode": ("Object", True),
    "objects_in_mode_unique_data": ("Object", True),
    "particle_edit_object": ("Object", False),
    "particle_settings": ("ParticleSettings", False),
    "particle_system": ("ParticleSystem", False),
    "particle_system_editable": ("ParticleSystem", False),
    "property": ("(:class:`bpy.types.AnyType`, :class:`string`, :class:`int`)", False),
    "pointcloud": ("PointCloud", False),
    "pose_bone": ("PoseBone", False),
    "pose_object": ("Object", False),
    "scene": ("Scene", False),
    "sculpt_object": ("Object", False),
    "selectable_objects": ("Object", True),
    "selected_assets": ("AssetRepresentation", True),
    "selected_bones": ("EditBone", True),
    "selected_editable_actions": ("Action", True),
    "selected_editable_bones": ("EditBone", True),
    "selected_editable_fcurves": ("FCurve", True),
    "selected_editable_keyframes": ("Keyframe", True),
    "selected_editable_objects": ("Object", True),
    "selected_editable_sequences": ("Sequence", True),
    "selected_files": ("FileSelectEntry", True),
    "selected_ids": ("ID", True),
    "selected_nla_strips": ("NlaStrip", True),
    "selected_movieclip_tracks": ("MovieTrackingTrack", True),
    "selected_nodes": ("Node", True),
    "selected_objects": ("Object", True),
    "selected_pose_bones": ("PoseBone", True),
    "selected_pose_bones_from_active_object": ("PoseBone", True),
    "selected_sequences": ("Sequence", True),
    "selected_visible_actions": ("Action", True),
    "selected_visible_fcurves": ("FCurve", True),
    "sequences": ("Sequence", True),
    "soft_body": ("SoftBodyModifier", False),
    "speaker": ("Speaker", False),
    "texture": ("Texture", False),
    "texture_node": ("Node", False),
    "texture_slot": ("TextureSlot", False),
    "texture_user": ("ID", False),
    "texture_user_property": ("Property", False),
    "ui_list": ("UIList", False),
    "vertex_paint_object": ("Object", False),
    "view_layer": ("ViewLayer", False),
    "visible_bones": ("EditBone", True),
    "visible_gpencil_layers": ("GPencilLayer", True),
    "visible_objects": ("Object", True),
    "visible_pose_bones": ("PoseBone", True),
    "visible_fcurves": ("FCurve", True),
    "weight_paint_object": ("Object", False),
    "volume": ("Volume", False),
    "world": ("World", False),
}


def pycontext2sphinx(basepath):
    # Only use once. very irregular.

    filepath = os.path.join(basepath, "bpy.context.rst")
    file = open(filepath, "w", encoding="utf-8")
    fw = file.write
    fw(title_string("Context Access (bpy.context)", "="))
    fw(".. module:: bpy.context\n")
    fw("\n")
    fw("The context members available depend on the area of Blender which is currently being accessed.\n")
    fw("\n")
    fw("Note that all context values are readonly,\n")
    fw("but may be modified through the data API or by running operators\n\n")

    # Track all unique properties to properly use `noindex`.
    unique = set()

    def write_contex_cls():

        fw(title_string("Global Context", "-"))
        fw("These properties are available in any contexts.\n\n")

        # Very silly. could make these global and only access once:
        # `structs, funcs, ops, props = rna_info.BuildRNAInfo()`.
        structs, funcs, ops, props = rna_info_BuildRNAInfo_cache()
        struct = structs[("", "Context")]
        struct_blacklist = RNA_BLACKLIST.get(struct.identifier, ())
        del structs, funcs, ops, props

        sorted_struct_properties = struct.properties[:]
        sorted_struct_properties.sort(key=lambda prop: prop.identifier)

        # First write RNA.
        for prop in sorted_struct_properties:
            # Support blacklisting props.
            if prop.identifier in struct_blacklist:
                continue
            # No need to check if there are duplicates yet as it's known there wont be.
            unique.add(prop.identifier)

            enum_descr_override = None
            if USE_SHARED_RNA_ENUM_ITEMS_STATIC:
                enum_descr_override = pyrna_enum2sphinx_shared_link(prop)

            type_descr = prop.get_type_description(
                class_fmt=":class:`bpy.types.%s`",
                mathutils_fmt=":class:`mathutils.%s`",
                collection_id=_BPY_PROP_COLLECTION_ID,
                enum_descr_override=enum_descr_override,
            )
            fw(".. data:: %s\n\n" % prop.identifier)
            if prop.description:
                fw("   %s\n\n" % prop.description)

            # Special exception, can't use generic code here for enums.
            if prop.type == "enum":
                # If the link has been written, no need to inline the enum items.
                enum_text = "" if enum_descr_override else pyrna_enum2sphinx(prop)
                if enum_text:
                    write_indented_lines("   ", fw, enum_text)
                    fw("\n")
                del enum_text
            # End enum exception.

            fw("   :type: %s\n\n" % type_descr)

    write_contex_cls()
    del write_contex_cls
    # end

    # Internal API call only intended to be used to extract context members.
    from _bpy import context_members
    context_member_map = context_members()
    del context_members

    # Track unique for `context_strings` to validate `context_type_map`.
    unique_context_strings = set()
    for ctx_str, ctx_members in sorted(context_member_map.items()):
        subsection = "%s Context" % ctx_str.split("_")[0].title()
        fw("\n%s\n%s\n\n" % (subsection, (len(subsection) * "-")))
        for member in ctx_members:
            unique_all_len = len(unique)
            unique.add(member)
            member_visited = unique_all_len == len(unique)

            unique_context_strings.add(member)

            fw(".. data:: %s\n" % member)
            # Avoid warnings about the member being included multiple times.
            if member_visited:
                fw("   :noindex:\n")
            fw("\n")

            try:
                member_type, is_seq = context_type_map[member]
            except KeyError:
                raise SystemExit(
                    "Error: context key %r not found in context_type_map; update %s" %
                    (member, __file__)) from None

            if member_type.isidentifier():
                member_type = ":class:`bpy.types.%s`" % member_type
            fw("   :type: %s %s\n\n" % ("sequence of " if is_seq else "", member_type))
            write_example_ref("   ", fw, "bpy.context." + member)

    # Generate type-map:
    # for member in sorted(unique_context_strings):
    #     print('        "%s": ("", False),' % member)
    if len(context_type_map) > len(unique_context_strings):
        warnings.warn(
            "Some types are not used: %s" %
            str([member for member in context_type_map if member not in unique_context_strings]))
    else:
        pass  # Will have raised an error above.

    file.close()


def pyrna_enum2sphinx(prop, use_empty_descriptions=False):
    """
    Write a bullet point list of enum + descriptions.
    """

    # Write a link to the enum if this is part of `rna_enum_pointer_map`.
    if USE_SHARED_RNA_ENUM_ITEMS_STATIC:
        if (result := pyrna_enum2sphinx_shared_link(prop)) is not None:
            return result

    if use_empty_descriptions:
        ok = True
    else:
        ok = False
        for identifier, name, description in prop.enum_items:
            if description:
                ok = True
                break

    if ok:
        return "".join([
            "* ``%s``\n"
            "%s.\n" % (
                identifier,
                # Account for multi-line enum descriptions, allowing this to be a block of text.
                indent(" -- ".join(escape_rst(val) for val in (name, description) if val) or "Undocumented", "  "),
            )
            for identifier, name, description in prop.enum_items
        ])
    else:
        return ""


def pyrna2sphinx(basepath):
    """
    ``bpy.types`` and ``bpy.ops``.
    """
    # `structs, funcs, ops, props = rna_info.BuildRNAInfo()`
    structs, _funcs, ops, _props = rna_info_BuildRNAInfo_cache()

    if USE_ONLY_BUILTIN_RNA_TYPES:
        # Ignore properties that use non `bpy.types` properties.
        structs_blacklist = {
            v.identifier for v in structs.values()
            if v.module_name != "bpy.types"
        }
        for k, v in structs.items():
            for p in v.properties:
                for identifier in (
                        getattr(p.srna, "identifier", None),
                        getattr(p.fixed_type, "identifier", None),
                ):
                    if identifier is not None:
                        if identifier in structs_blacklist:
                            RNA_BLACKLIST.setdefault(k, set()).add(identifier)
        del structs_blacklist

        structs = {
            k: v for k, v in structs.items()
            if v.module_name == "bpy.types"
        }

    if FILTER_BPY_TYPES is not None:
        structs = {
            k: v for k, v in structs.items()
            if k[1] in FILTER_BPY_TYPES
            if v.module_name == "bpy.types"
        }

    if FILTER_BPY_OPS is not None:
        ops = {k: v for k, v in ops.items() if v.module_name in FILTER_BPY_OPS}

    def write_param(ident, fw, prop, is_return=False):
        if is_return:
            id_name = "return"
            id_type = "rtype"
            kwargs = {"as_ret": True}
            identifier = ""
        else:
            id_name = "arg"
            id_type = "type"
            kwargs = {"as_arg": True}
            identifier = " %s" % prop.identifier

        kwargs["class_fmt"] = ":class:`%s`"
        kwargs["mathutils_fmt"] = ":class:`mathutils.%s`"

        kwargs["collection_id"] = _BPY_PROP_COLLECTION_ID

        enum_descr_override = None
        if USE_SHARED_RNA_ENUM_ITEMS_STATIC:
            enum_descr_override = pyrna_enum2sphinx_shared_link(prop)
            kwargs["enum_descr_override"] = enum_descr_override

        type_descr = prop.get_type_description(**kwargs)

        # If the link has been written, no need to inline the enum items.
        enum_text = "" if enum_descr_override else pyrna_enum2sphinx(prop)
        if prop.name or prop.description or enum_text:
            fw(ident + ":%s%s: " % (id_name, identifier))

            if prop.name or prop.description:
                fw(", ".join(val for val in (prop.name, prop.description.replace("\n", "")) if val) + "\n")

            # Special exception, can't use generic code here for enums.
            if enum_text:
                fw("\n")
                write_indented_lines(ident + "   ", fw, enum_text)
            del enum_text
            # end enum exception

        fw(ident + ":%s%s: %s\n" % (id_type, identifier, type_descr))

    def write_struct(struct):
        # if not struct.identifier.startswith("Sc") and not struct.identifier.startswith("I"):
        #     return

        # if not struct.identifier == "Object":
        #     return

        struct_module_name = struct.module_name
        if USE_ONLY_BUILTIN_RNA_TYPES:
            assert struct_module_name == "bpy.types"
        filepath = os.path.join(basepath, "%s.%s.rst" % (struct_module_name, struct.identifier))
        file = open(filepath, "w", encoding="utf-8")
        fw = file.write

        base_id = getattr(struct.base, "identifier", "")
        struct_id = struct.identifier

        if _BPY_STRUCT_FAKE:
            if not base_id:
                base_id = _BPY_STRUCT_FAKE

        if base_id:
            title = "%s(%s)" % (struct_id, base_id)
        else:
            title = struct_id

        fw(title_string(title, "="))

        fw(".. currentmodule:: %s\n\n" % struct_module_name)

        # docs first?, ok
        write_example_ref("", fw, "%s.%s" % (struct_module_name, struct_id))

        base_ids = [base.identifier for base in struct.get_bases()]

        if _BPY_STRUCT_FAKE:
            base_ids.append(_BPY_STRUCT_FAKE)

        base_ids.reverse()

        if base_ids:
            if len(base_ids) > 1:
                fw("base classes --- ")
            else:
                fw("base class --- ")

            fw(", ".join((":class:`%s`" % base_id) for base_id in base_ids))
            fw("\n\n")

        subclass_ids = [
            s.identifier for s in structs.values()
            if s.base is struct
            if not rna_info.rna_id_ignore(s.identifier)
        ]
        subclass_ids.sort()
        if subclass_ids:
            fw("subclasses --- \n" + ", ".join((":class:`%s`" % s) for s in subclass_ids) + "\n\n")

        base_id = getattr(struct.base, "identifier", "")

        if _BPY_STRUCT_FAKE:
            if not base_id:
                base_id = _BPY_STRUCT_FAKE

        if base_id:
            fw(".. class:: %s(%s)\n\n" % (struct_id, base_id))
        else:
            fw(".. class:: %s\n\n" % struct_id)

        write_indented_lines("   ", fw, struct.description, False)
        fw("\n")

        # Properties sorted in alphabetical order.
        sorted_struct_properties = struct.properties[:]
        sorted_struct_properties.sort(key=lambda prop: prop.identifier)

        # Support blacklisting props.
        struct_blacklist = RNA_BLACKLIST.get(struct_id, ())

        for prop in sorted_struct_properties:
            identifier = prop.identifier

            # Support blacklisting props.
            if identifier in struct_blacklist:
                continue

            enum_descr_override = None
            if USE_SHARED_RNA_ENUM_ITEMS_STATIC:
                enum_descr_override = pyrna_enum2sphinx_shared_link(prop)

            type_descr = prop.get_type_description(
                class_fmt=":class:`%s`",
                mathutils_fmt=":class:`mathutils.%s`",
                collection_id=_BPY_PROP_COLLECTION_ID,
                enum_descr_override=enum_descr_override,
            )
            # Read-only properties use "data" directive, variables properties use "attribute" directive.
            if "readonly" in type_descr:
                fw("   .. data:: %s\n" % identifier)
            else:
                fw("   .. attribute:: %s\n" % identifier)
            # Also write `noindex` on requerst.
            if ("bpy.types", struct_id, identifier) in RST_NOINDEX_ATTR:
                fw("      :noindex:\n")
            fw("\n")

            if prop.description:
                write_indented_lines("      ", fw, prop.description, False)
                fw("\n")

            # Special exception, can't use generic code here for enums.
            if prop.type == "enum":
                # If the link has been written, no need to inline the enum items.
                enum_text = "" if enum_descr_override else pyrna_enum2sphinx(prop)
                if enum_text:
                    write_indented_lines("      ", fw, enum_text)
                    fw("\n")
                del enum_text
            # End enum exception.

            fw("      :type: %s\n\n" % type_descr)

        # Python attributes.
        py_properties = struct.get_py_properties()
        py_prop = None
        for identifier, py_prop in py_properties:
            pyprop2sphinx("   ", fw, identifier, py_prop)
        del py_properties, py_prop

        # C/Python attributes: `GetSetDescriptorType`.
        key = descr = None
        for key, descr in sorted(struct.get_py_c_properties_getset()):
            py_descr2sphinx("   ", fw, descr, "bpy.types", struct_id, key)
        del key, descr

        for func in struct.functions:
            args_str = ", ".join(prop.get_arg_default(force=False) for prop in func.args)

            fw("   .. %s:: %s(%s)\n\n" %
               ("classmethod" if func.is_classmethod else "method", func.identifier, args_str))
            fw("      %s\n\n" % func.description)

            for prop in func.args:
                write_param("      ", fw, prop)

            if len(func.return_values) == 1:
                write_param("      ", fw, func.return_values[0], is_return=True)
            elif func.return_values:  # Multiple return values.
                fw("      :return (%s):\n" % ", ".join(prop.identifier for prop in func.return_values))
                for prop in func.return_values:
                    # TODO: pyrna_enum2sphinx for multiple return values... actually don't
                    # think we even use this but still!

                    enum_descr_override = None
                    if USE_SHARED_RNA_ENUM_ITEMS_STATIC:
                        enum_descr_override = pyrna_enum2sphinx_shared_link(prop)

                    type_descr = prop.get_type_description(
                        as_ret=True, class_fmt=":class:`%s`",
                        mathutils_fmt=":class:`mathutils.%s`",
                        collection_id=_BPY_PROP_COLLECTION_ID,
                        enum_descr_override=enum_descr_override,
                    )
                    descr = prop.description
                    if not descr:
                        descr = prop.name
                    # In rare cases `descr` may be empty.
                    fw("         `%s`, %s\n\n" %
                       (prop.identifier,
                        ", ".join((val for val in (descr, type_descr) if val))))

            write_example_ref("      ", fw, struct_module_name + "." + struct_id + "." + func.identifier)

            fw("\n")

        # Python methods.
        py_funcs = struct.get_py_functions()
        py_func = None

        for identifier, py_func in py_funcs:
            pyfunc2sphinx("   ", fw, "bpy.types", struct_id, identifier, py_func, is_class=True)
        del py_funcs, py_func

        py_funcs = struct.get_py_c_functions()
        py_func = None

        for identifier, py_func in py_funcs:
            py_c_func2sphinx("   ", fw, "bpy.types", struct_id, identifier, py_func, is_class=True)

        lines = []

        if struct.base or _BPY_STRUCT_FAKE:
            bases = list(reversed(struct.get_bases()))

            # Properties.
            del lines[:]

            if _BPY_STRUCT_FAKE:
                descr_items = [
                    (key, descr) for key, descr in sorted(bpy_struct.__dict__.items())
                    if not key.startswith("__")
                ]

            if _BPY_STRUCT_FAKE:
                for key, descr in descr_items:
                    if type(descr) == GetSetDescriptorType:
                        lines.append("   * :class:`%s.%s`\n" % (_BPY_STRUCT_FAKE, key))

            for base in bases:
                for prop in base.properties:
                    lines.append("   * :class:`%s.%s`\n" % (base.identifier, prop.identifier))

                for identifier, py_prop in base.get_py_properties():
                    lines.append("   * :class:`%s.%s`\n" % (base.identifier, identifier))

            if lines:
                fw(".. rubric:: Inherited Properties\n\n")

                fw(".. hlist::\n")
                fw("   :columns: 2\n\n")

                for line in lines:
                    fw(line)
                fw("\n")

            # Functions.
            del lines[:]

            if _BPY_STRUCT_FAKE:
                for key, descr in descr_items:
                    if type(descr) == MethodDescriptorType:
                        lines.append("   * :class:`%s.%s`\n" % (_BPY_STRUCT_FAKE, key))

            for base in bases:
                for func in base.functions:
                    lines.append("   * :class:`%s.%s`\n" % (base.identifier, func.identifier))
                for identifier, py_func in base.get_py_functions():
                    lines.append("   * :class:`%s.%s`\n" % (base.identifier, identifier))
                for identifier, py_func in base.get_py_c_functions():
                    lines.append("   * :class:`%s.%s`\n" % (base.identifier, identifier))

            if lines:
                fw(".. rubric:: Inherited Functions\n\n")

                fw(".. hlist::\n")
                fw("   :columns: 2\n\n")

                for line in lines:
                    fw(line)
                fw("\n")

            del lines[:]

        if struct.references:
            # use this otherwise it gets in the index for a normal heading.
            fw(".. rubric:: References\n\n")

            fw(".. hlist::\n")
            fw("   :columns: 2\n\n")

            # Context does its own thing.
            # "active_object": ("Object", False),
            for ref_attr, (ref_type, ref_is_seq) in sorted(context_type_map.items()):
                if ref_type == struct_id:
                    fw("   * :mod:`bpy.context.%s`\n" % ref_attr)
            del ref_attr, ref_type, ref_is_seq

            for ref in struct.references:
                ref_split = ref.split(".")
                if len(ref_split) > 2:
                    ref = ref_split[-2] + "." + ref_split[-1]
                fw("   * :class:`%s`\n" % ref)
            fw("\n")

        # docs last?, disable for now
        # write_example_ref("", fw, "bpy.types.%s" % struct_id)
        file.close()

    if "bpy.types" not in EXCLUDE_MODULES:
        for struct in structs.values():
            # TODO: rna_info should filter these out!
            if "_OT_" in struct.identifier:
                continue
            write_struct(struct)

        def fake_bpy_type(class_module_name, class_value, class_name, descr_str, use_subclasses=True):
            filepath = os.path.join(basepath, "%s.%s.rst" % (class_module_name, class_name))
            file = open(filepath, "w", encoding="utf-8")
            fw = file.write

            fw(title_string(class_name, "="))

            fw(".. currentmodule:: %s\n\n" % class_module_name)

            if use_subclasses:
                subclass_ids = [
                    s.identifier for s in structs.values()
                    if s.base is None
                    if not rna_info.rna_id_ignore(s.identifier)
                ]
                if subclass_ids:
                    fw("subclasses --- \n" + ", ".join((":class:`%s`" % s) for s in sorted(subclass_ids)) + "\n\n")

            fw(".. class:: %s\n\n" % class_name)
            fw("   %s\n\n" % descr_str)
            fw("   .. note::\n\n")
            fw("      Note that :class:`%s.%s` is not actually available from within Blender,\n"
               "      it only exists for the purpose of documentation.\n\n" % (class_module_name, class_name))

            descr_items = [
                (key, descr) for key, descr in sorted(class_value.__dict__.items())
                if not key.startswith("__")
            ]

            for key, descr in descr_items:
                # `GetSetDescriptorType`, `GetSetDescriptorType` types are not documented yet.
                if type(descr) == MethodDescriptorType:
                    py_descr2sphinx("   ", fw, descr, "bpy.types", class_name, key)

            for key, descr in descr_items:
                if type(descr) == GetSetDescriptorType:
                    py_descr2sphinx("   ", fw, descr, "bpy.types", class_name, key)
            file.close()

        # write fake classes
        if _BPY_STRUCT_FAKE:
            class_value = bpy_struct
            fake_bpy_type(
                "bpy.types", class_value, _BPY_STRUCT_FAKE,
                "built-in base class for all classes in bpy.types.", use_subclasses=True,
            )

        if _BPY_PROP_COLLECTION_FAKE:
            class_value = bpy.data.objects.__class__
            fake_bpy_type(
                "bpy.types", class_value, _BPY_PROP_COLLECTION_FAKE,
                "built-in class used for all collections.", use_subclasses=False,
            )

    # operators
    def write_ops():
        API_BASEURL = "https://projects.blender.org/blender/blender/src/branch/main/scripts"
        API_BASEURL_ADDON = "https://projects.blender.org/blender/blender-addons"
        API_BASEURL_ADDON_CONTRIB = "https://projects.blender.org/blender/blender-addons-contrib"

        op_modules = {}
        op = None
        for op in ops.values():
            op_modules.setdefault(op.module_name, []).append(op)
        del op

        for op_module_name, ops_mod in op_modules.items():
            filepath = os.path.join(basepath, "bpy.ops.%s.rst" % op_module_name)
            file = open(filepath, "w", encoding="utf-8")
            fw = file.write

            title = "%s Operators" % op_module_name.replace("_", " ").title()

            fw(title_string(title, "="))

            fw(".. module:: bpy.ops.%s\n\n" % op_module_name)

            ops_mod.sort(key=lambda op: op.func_name)

            for op in ops_mod:
                args_str = ", ".join(prop.get_arg_default(force=True) for prop in op.args)
                fw(".. function:: %s(%s)\n\n" % (op.func_name, args_str))

                # if the description isn't valid, we output the standard warning
                # with a link to the wiki so that people can help
                if not op.description or op.description == "(undocumented operator)":
                    operator_description = undocumented_message("bpy.ops", op.module_name, op.func_name)
                else:
                    operator_description = op.description

                fw("   %s\n\n" % operator_description)
                for prop in op.args:
                    write_param("   ", fw, prop)

                location = op.get_location()
                if location != (None, None):
                    if location[0].startswith("addons_contrib" + os.sep):
                        url_base = API_BASEURL_ADDON_CONTRIB
                    elif location[0].startswith("addons" + os.sep):
                        url_base = API_BASEURL_ADDON
                    else:
                        url_base = API_BASEURL

                    fw("   :File: `%s\\:%d <%s/%s#L%d>`__\n\n" %
                       (location[0], location[1], url_base, location[0], location[1]))

                if op.args:
                    fw("\n")

            file.close()

    if "bpy.ops" not in EXCLUDE_MODULES:
        write_ops()


def write_sphinx_conf_py(basepath):
    """
    Write sphinx's ``conf.py``.
    """
    filepath = os.path.join(basepath, "conf.py")
    file = open(filepath, "w", encoding="utf-8")
    fw = file.write

    fw("import sys, os\n\n")
    fw("extensions = ['sphinx.ext.intersphinx']\n\n")
    fw("intersphinx_mapping = {'blender_manual': ('https://docs.blender.org/manual/en/dev/', None)}\n\n")
    fw("project = 'Blender %s Python API'\n" % BLENDER_VERSION_STRING)
    fw("root_doc = 'index'\n")
    fw("copyright = 'Blender Authors'\n")
    fw("version = '%s'\n" % BLENDER_VERSION_DOTS)
    fw("release = '%s'\n" % BLENDER_VERSION_DOTS)

    # Set this as the default is a super-set of Python3.
    fw("highlight_language = 'python3'\n")
    # No need to detect encoding.
    fw("highlight_options = {'default': {'encoding': 'utf-8'}}\n\n")

    # Quiet file not in table-of-contents warnings.
    fw("exclude_patterns = [\n")
    fw("    'include__bmesh.rst',\n")
    fw("]\n\n")

    fw("html_title = 'Blender Python API'\n")

    fw("html_theme = 'default'\n")
    # The theme 'sphinx_rtd_theme' is no longer distributed with sphinx by default, only use when available.
    fw(r"""
try:
    import furo
    html_theme = "furo"
    del furo
except ModuleNotFoundError:
    pass
if html_theme == "furo":
    html_theme_options = {
        "light_css_variables": {
            "color-brand-primary": "#265787",
            "color-brand-content": "#265787",
        },
    }

    html_sidebars = {
        "**": [
            "sidebar/brand.html",
            "sidebar/search.html",
            "sidebar/scroll-start.html",
            "sidebar/navigation.html",
            "sidebar/scroll-end.html",
            "sidebar/variant-selector.html",
        ]
    }
""")

    # not helpful since the source is generated, adds to upload size.
    fw("html_copy_source = False\n")
    fw("html_show_sphinx = False\n")
    fw("html_baseurl = 'https://docs.blender.org/api/current/'\n")
    fw("html_use_opensearch = 'https://docs.blender.org/api/current'\n")
    fw("html_show_search_summary = True\n")
    fw("html_split_index = True\n")
    fw("html_static_path = ['static']\n")
    fw("templates_path = ['templates']\n")
    fw("html_context = {'commit': '%s - %s'}\n" % (BLENDER_VERSION_HASH_HTML_LINK, BLENDER_VERSION_DATE))
    fw("html_extra_path = ['static/favicon.ico', 'static/blender_logo.svg']\n")
    fw("html_favicon = 'static/favicon.ico'\n")
    fw("html_logo = 'static/blender_logo.svg'\n")
    # Disable default `last_updated` value, since this is the date of doc generation, not the one of the source commit.
    fw("html_last_updated_fmt = None\n\n")
    fw("if html_theme == 'furo':\n")
    fw("    html_css_files = ['css/version_switch.css']\n")
    fw("    html_js_files = ['js/version_switch.js']\n")

    # needed for latex, pdf gen
    fw("latex_elements = {\n")
    fw("  'papersize': 'a4paper',\n")
    fw("}\n\n")

    fw("latex_documents = [ ('contents', 'contents.tex', 'Blender Index', 'Blender Foundation', 'manual'), ]\n")

    # Workaround for useless links leading to compile errors
    # See https://github.com/sphinx-doc/sphinx/issues/3866
    fw(r"""
from sphinx.domains.python import PythonDomain

class PatchedPythonDomain(PythonDomain):
    def resolve_xref(self, env, fromdocname, builder, typ, target, node, contnode):
        if 'refspecific' in node:
            del node['refspecific']
        return super(PatchedPythonDomain, self).resolve_xref(
            env, fromdocname, builder, typ, target, node, contnode)
""")
    # end workaround

    fw("def setup(app):\n")
    fw("    app.add_css_file('css/theme_overrides.css')\n")
    fw("    app.add_domain(PatchedPythonDomain, override=True)\n\n")

    file.close()


def write_rst_index(basepath):
    """
    Write the RST file of the main page, needed for sphinx: ``index.html``.
    """
    filepath = os.path.join(basepath, "index.rst")
    file = open(filepath, "w", encoding="utf-8")
    fw = file.write

    fw(title_string("Blender %s Python API Documentation" % BLENDER_VERSION_DOTS, "%", double=True))
    fw("\n")
    fw("Welcome to the Python API documentation for `Blender <https://www.blender.org>`__, ")
    fw("the free and open source 3D creation suite.\n")
    fw("\n")

    # fw("`A PDF version of this document is also available <%s>`_\n" % BLENDER_PDF_FILENAME)
    fw("This site can be used offline: `Download the full documentation (zipped HTML files) <%s>`__\n" %
       BLENDER_ZIP_FILENAME)
    fw("\n")

    if not EXCLUDE_INFO_DOCS:
        fw(".. toctree::\n")
        if USE_INFO_DOCS_FANCY_INDEX:
            fw("   :hidden:\n")
        fw("   :maxdepth: 1\n")
        fw("   :caption: Documentation\n\n")
        for info, info_desc in INFO_DOCS:
            fw("   %s\n" % info)
        fw("\n")

        if USE_INFO_DOCS_FANCY_INDEX:
            # Show a fake TOC, allowing for an extra description to be shown as well as the title.
            fw(title_string("Documentation", "="))
            for info, info_desc in INFO_DOCS:
                fw("- :doc:`%s`: %s\n" % (info.removesuffix(".rst"), info_desc))
            fw("\n")

    fw(".. toctree::\n")
    fw("   :maxdepth: 1\n")
    fw("   :caption: Application Modules\n\n")

    app_modules = (
        "bpy.context",  # note: not actually a module
        "bpy.data",     # note: not actually a module
        "bpy.msgbus",   # note: not actually a module
        "bpy.ops",
        "bpy.types",

        # py modules
        "bpy.utils",
        "bpy.path",
        "bpy.app",

        # C modules
        "bpy.props",
    )

    for mod in app_modules:
        if mod not in EXCLUDE_MODULES:
            fw("   %s\n" % mod)
    fw("\n")

    fw(".. toctree::\n")
    fw("   :maxdepth: 1\n")
    fw("   :caption: Standalone Modules\n\n")

    standalone_modules = (
        # submodules are added in parent page
        "aud",
        "bgl",
        "bl_math",
        "blf",
        "bmesh",
        "bpy_extras",
        "freestyle",
        "gpu",
        "gpu_extras",
        "idprop.types",
        "imbuf",
        "mathutils",
    )

    for mod in standalone_modules:
        if mod not in EXCLUDE_MODULES:
            fw("   %s\n" % mod)
    fw("\n")

    fw(title_string("Indices", "="))
    fw("* :ref:`genindex`\n")
    fw("* :ref:`modindex`\n\n")

    # Special case, this `bmesh.ops.rst` is extracted from C source.
    if "bmesh.ops" not in EXCLUDE_MODULES:
        execfile(os.path.join(SCRIPT_DIR, "rst_from_bmesh_opdefines.py"))

    file.close()


def write_rst_bpy(basepath):
    """
    Write RST file of ``bpy`` module (disabled by default)
    """
    if ARGS.bpy:
        filepath = os.path.join(basepath, "bpy.rst")
        file = open(filepath, "w", encoding="utf-8")
        fw = file.write

        fw("\n")

        title = ":mod:`bpy` --- Blender Python Module"

        fw(title_string(title, "="))

        fw(".. module:: bpy.types\n\n")
        file.close()


def write_rst_types_index(basepath):
    """
    Write the RST file of ``bpy.types`` module (index)
    """
    if "bpy.types" not in EXCLUDE_MODULES:
        filepath = os.path.join(basepath, "bpy.types.rst")
        file = open(filepath, "w", encoding="utf-8")
        fw = file.write
        fw(title_string("Types (bpy.types)", "="))
        fw(".. module:: bpy.types\n\n")
        fw(".. toctree::\n")
        # Only show top-level entries (avoids unreasonably large pages).
        fw("   :maxdepth: 1\n")
        fw("   :glob:\n\n")
        fw("   bpy.types.*\n\n")

        # This needs to be included somewhere, while it's hidden, list to avoid warnings.
        if USE_SHARED_RNA_ENUM_ITEMS_STATIC:
            fw(".. toctree::\n")
            fw("   :hidden:\n")
            fw("   :maxdepth: 1\n\n")
            fw("   Shared Enum Types <bpy_types_enum_items/index>\n\n")

        file.close()


def write_rst_ops_index(basepath):
    """
    Write the RST file of bpy.ops module (index)
    """
    if "bpy.ops" not in EXCLUDE_MODULES:
        filepath = os.path.join(basepath, "bpy.ops.rst")
        file = open(filepath, "w", encoding="utf-8")
        fw = file.write
        fw(title_string("Operators (bpy.ops)", "="))
        fw(".. module:: bpy.ops\n\n")
        write_example_ref("", fw, "bpy.ops")
        fw(".. toctree::\n")
        fw("   :caption: Submodules\n")
        # Only show top-level entries (avoids unreasonably large pages).
        fw("   :maxdepth: 1\n")
        fw("   :glob:\n\n")
        fw("   bpy.ops.*\n\n")
        file.close()


def write_rst_msgbus(basepath):
    """
    Write the RST files of ``bpy.msgbus`` module
    """
    if 'bpy.msgbus' in EXCLUDE_MODULES:
        return

    # Write the index.
    filepath = os.path.join(basepath, "bpy.msgbus.rst")
    file = open(filepath, "w", encoding="utf-8")
    fw = file.write
    fw(title_string("Message Bus (bpy.msgbus)", "="))
    write_example_ref("", fw, "bpy.msgbus")
    fw(".. toctree::\n")
    fw("   :glob:\n\n")
    fw("   bpy.msgbus.*\n\n")
    file.close()

    # Write the contents.
    pymodule2sphinx(basepath, 'bpy.msgbus', bpy.msgbus, 'Message Bus', ())
    EXAMPLE_SET_USED.add("bpy.msgbus")


def write_rst_data(basepath):
    """
    Write the RST file of ``bpy.data`` module.
    """
    if "bpy.data" not in EXCLUDE_MODULES:
        # Not actually a module, only write this file so we can reference in the TOC.
        filepath = os.path.join(basepath, "bpy.data.rst")
        file = open(filepath, "w", encoding="utf-8")
        fw = file.write
        fw(title_string("Data Access (bpy.data)", "="))
        fw(".. module:: bpy.data\n")
        fw("\n")
        fw("This module is used for all Blender/Python access.\n")
        fw("\n")
        fw(".. data:: data\n")
        fw("\n")
        fw("   Access to Blender's internal data\n")
        fw("\n")
        fw("   :type: :class:`bpy.types.BlendData`\n")
        fw("\n")
        fw(".. literalinclude:: ../examples/bpy.data.py\n")
        file.close()

        EXAMPLE_SET_USED.add("bpy.data")


def pyrna_enum2sphinx_shared_link(prop):
    """
    Return a reference to the enum used by ``prop`` or None when not found.
    """
    if (
            (prop.type == "enum") and
            (pointer := prop.enum_pointer) and
            (identifier := rna_enum_pointer_to_id_map.get(pointer))
    ):
        return ":ref:`%s`" % identifier
    return None


def write_rst_enum_items(basepath, key, key_no_prefix, enum_items):
    """
    Write a single page for a static enum in RST.

    This helps avoiding very large lists being in-lined in many places which is an issue
    especially with icons in ``bpy.types.UILayout``. See #87008.
    """
    filepath = os.path.join(basepath, "%s.rst" % key_no_prefix)
    with open(filepath, "w", encoding="utf-8") as fh:
        fw = fh.write
        # fw(".. noindex::\n\n")
        fw(".. _%s:\n\n" % key)

        fw(title_string(key_no_prefix.replace("_", " ").title(), "#"))
        # fw(".. rubric:: %s\n\n" % key_no_prefix.replace("_", " ").title())

        for item in enum_items:
            identifier = item.identifier
            name = item.name
            description = item.description
            if identifier:
                fw(":%s: %s\n" % (item.identifier, (escape_rst(name) + ".") if name else ""))
                if description:
                    fw("\n")
                    write_indented_lines("   ", fw, escape_rst(description) + ".")
                else:
                    fw("\n")
            else:
                if name:
                    fw("\n\n**%s**\n\n" % name)
                else:
                    fw("\n\n----\n\n")

                if description:
                    fw(escape_rst(description) + ".")
                    fw("\n\n")


def write_rst_enum_items_and_index(basepath):
    """
    Write shared enum items.
    """
    subdir = "bpy_types_enum_items"
    basepath_bpy_types_rna_enum = os.path.join(basepath, subdir)
    os.makedirs(basepath_bpy_types_rna_enum, exist_ok=True)
    with open(os.path.join(basepath_bpy_types_rna_enum, "index.rst"), "w", encoding="utf-8") as fh:
        fw = fh.write
        fw(title_string("Shared Enum Items", "#"))
        fw(".. toctree::\n")
        fw("\n")
        for key, enum_items in rna_enum_dict.items():
            if not key.startswith("rna_enum_"):
                raise Exception("Found RNA enum identifier that doesn't use the 'rna_enum_' prefix, found %r!" % key)
            key_no_prefix = key.removeprefix("rna_enum_")
            fw("   %s\n" % key_no_prefix)

        for key, enum_items in rna_enum_dict.items():
            key_no_prefix = key.removeprefix("rna_enum_")
            write_rst_enum_items(basepath_bpy_types_rna_enum, key, key_no_prefix, enum_items)
        fw("\n")


def write_rst_importable_modules(basepath):
    """
    Write the RST files of importable modules.
    """
    importable_modules = {
        # Python_modules
        "bpy.path": "Path Utilities",
        "bpy.utils": "Utilities",
        "bpy_extras": "Extra Utilities",
        "gpu_extras": "GPU Utilities",

        # C_modules
        "aud": "Audio System",
        "blf": "Font Drawing",
        "imbuf": "Image Buffer",
        "imbuf.types": "Image Buffer Types",
        "gpu": "GPU Module",
        "gpu.types": "GPU Types",
        "gpu.matrix": "GPU Matrix Utilities",
        "gpu.select": "GPU Select Utilities",
        "gpu.shader": "GPU Shader Utilities",
        "gpu.state": "GPU State Utilities",
        "gpu.texture": "GPU Texture Utilities",
        "gpu.platform": "GPU Platform Utilities",
        "gpu.capabilities": "GPU Capabilities Utilities",
        "bmesh": "BMesh Module",
        "bmesh.ops": "BMesh Operators",
        "bmesh.types": "BMesh Types",
        "bmesh.utils": "BMesh Utilities",
        "bmesh.geometry": "BMesh Geometry Utilities",
        "bpy.app": "Application Data",
        "bpy.app.handlers": "Application Handlers",
        "bpy.app.translations": "Application Translations",
        "bpy.app.icons": "Application Icons",
        "bpy.app.timers": "Application Timers",
        "bpy.props": "Property Definitions",
        "idprop.types": "ID Property Access",
        "mathutils": "Math Types & Utilities",
        "mathutils.geometry": "Geometry Utilities",
        "mathutils.bvhtree": "BVHTree Utilities",
        "mathutils.kdtree": "KDTree Utilities",
        "mathutils.interpolate": "Interpolation Utilities",
        "mathutils.noise": "Noise Utilities",
        "bl_math": "Additional Math Functions",
        "freestyle": "Freestyle Module",
        "freestyle.types": "Freestyle Types",
        "freestyle.predicates": "Freestyle Predicates",
        "freestyle.functions": "Freestyle Functions",
        "freestyle.chainingiterators": "Freestyle Chaining Iterators",
        "freestyle.shaders": "Freestyle Shaders",
        "freestyle.utils": "Freestyle Utilities",
    }

    # This is needed since some of the sub-modules listed above are not actual modules.
    # Examples include `bpy.app.translations` & `bpy.app.handlers`.
    #
    # Most of these are `PyStructSequence` internally,
    # however we don't want to document all of these as modules since some only contain
    # a few values (version number for e.g).
    #
    # If we remove this logic and document all `PyStructSequence` as sub-modules it means
    # `bpy.app.timers` for example would be presented on the same level as library information
    # access such as `bpy.app.sdl` which doesn't seem useful since it hides more useful
    # module-like objects among library data access.
    importable_modules_parent_map = {}
    for mod_name in importable_modules:  # Iterate over keys.
        if mod_name in EXCLUDE_MODULES:
            continue
        if "." in mod_name:
            mod_name, submod_name = mod_name.rsplit(".", 1)
            importable_modules_parent_map.setdefault(mod_name, []).append(submod_name)

    for mod_name, mod_descr in importable_modules.items():
        if mod_name in EXCLUDE_MODULES:
            continue
        module_all_extra = importable_modules_parent_map.get(mod_name, ())
        module = __import__(mod_name, fromlist=[mod_name.rsplit(".", 1)[-1]])
        pymodule2sphinx(basepath, mod_name, module, mod_descr, module_all_extra)


def copy_handwritten_rsts(basepath):

    # Info docs.
    if not EXCLUDE_INFO_DOCS:
        for info, _info_desc in INFO_DOCS:
            shutil.copy2(os.path.join(RST_DIR, info), basepath)
        for info in INFO_DOCS_OTHER:
            shutil.copy2(os.path.join(RST_DIR, info), basepath)

    # TODO: put this docs in Blender's code and use import as per modules above.
    handwritten_modules = [
        "bgl",  # "Blender OpenGl wrapper"
        "bmesh.ops",  # generated by rst_from_bmesh_opdefines.py

        # includes...
        "include__bmesh",
    ]

    for mod_name in handwritten_modules:
        if mod_name not in EXCLUDE_MODULES:
            # Copy2 keeps time/date stamps.
            shutil.copy2(os.path.join(RST_DIR, "%s.rst" % mod_name), basepath)

    # Change-log.
    shutil.copy2(os.path.join(RST_DIR, "change_log.rst"), basepath)

    # Copy images, could be smarter but just glob for now.
    for f in os.listdir(RST_DIR):
        if f.endswith(".png"):
            shutil.copy2(os.path.join(RST_DIR, f), basepath)


def copy_handwritten_extra(basepath):
    for f_src in EXTRA_SOURCE_FILES:
        if os.sep != "/":
            f_src = os.sep.join(f_src.split("/"))

        f_dst = f_src.replace("..", "__")

        f_src = os.path.join(RST_DIR, f_src)
        f_dst = os.path.join(basepath, f_dst)

        os.makedirs(os.path.dirname(f_dst), exist_ok=True)

        shutil.copy2(f_src, f_dst)


def copy_theme_assets(basepath):
    shutil.copytree(
        os.path.join(SCRIPT_DIR, "static"),
        os.path.join(basepath, "static"),
        copy_function=shutil.copy,
    )
    shutil.copytree(
        os.path.join(SCRIPT_DIR, "templates"),
        os.path.join(basepath, "templates"),
        copy_function=shutil.copy,
    )


def rna2sphinx(basepath):

    try:
        os.mkdir(basepath)
    except:
        pass

    # sphinx setup
    write_sphinx_conf_py(basepath)

    # main page
    write_rst_index(basepath)

    # context
    if "bpy.context" not in EXCLUDE_MODULES:
        pycontext2sphinx(basepath)

    # internal modules
    write_rst_bpy(basepath)                 # bpy, disabled by default
    write_rst_types_index(basepath)         # bpy.types
    write_rst_ops_index(basepath)           # bpy.ops
    write_rst_msgbus(basepath)              # bpy.msgbus
    pyrna2sphinx(basepath)                  # bpy.types.* and bpy.ops.*
    write_rst_data(basepath)                # bpy.data
    write_rst_importable_modules(basepath)

    # `bpy_types_enum_items/*` (referenced from `bpy.types`).
    if USE_SHARED_RNA_ENUM_ITEMS_STATIC:
        write_rst_enum_items_and_index(basepath)

    # copy the other rsts
    copy_handwritten_rsts(basepath)

    # copy source files referenced
    copy_handwritten_extra(basepath)

    # copy extra files needed for theme
    copy_theme_assets(basepath)


def align_sphinx_in_to_sphinx_in_tmp(dir_src, dir_dst):
    """
    Move changed files from SPHINX_IN_TMP to SPHINX_IN
    """
    import filecmp

    # Possible the dir doesn't exist when running recursively.
    os.makedirs(dir_dst, exist_ok=True)

    sphinx_dst_files = set(os.listdir(dir_dst))
    sphinx_src_files = set(os.listdir(dir_src))

    # Remove deprecated files that have been removed.
    for f in sorted(sphinx_dst_files):
        if f not in sphinx_src_files:
            BPY_LOGGER.debug("\tdeprecated: %s", f)
            f_dst = os.path.join(dir_dst, f)
            if os.path.isdir(f_dst):
                shutil.rmtree(f_dst, True)
            else:
                os.remove(f_dst)

    # freshen with new files.
    for f in sorted(sphinx_src_files):
        f_src = os.path.join(dir_src, f)
        f_dst = os.path.join(dir_dst, f)

        if os.path.isdir(f_src):
            align_sphinx_in_to_sphinx_in_tmp(f_src, f_dst)
        else:
            do_copy = True
            if f in sphinx_dst_files:
                if filecmp.cmp(f_src, f_dst):
                    do_copy = False

            if do_copy:
                BPY_LOGGER.debug("\tupdating: %s", f)
                shutil.copy(f_src, f_dst)


def refactor_sphinx_log(sphinx_logfile):
    refactored_log = []
    with open(sphinx_logfile, "r", encoding="utf-8") as original_logfile:
        lines = set(original_logfile.readlines())
        for line in lines:
            if 'warning' in line.lower() or 'error' in line.lower():
                line = line.strip().split(None, 2)
                if len(line) == 3:
                    location, kind, msg = line
                    location = os.path.relpath(location, start=SPHINX_IN)
                    refactored_log.append((kind, location, msg))
    with open(sphinx_logfile, "w", encoding="utf-8") as refactored_logfile:
        for log in sorted(refactored_log):
            refactored_logfile.write("%-12s %s\n             %s\n" % log)


def setup_monkey_patch():
    filepath = os.path.join(SCRIPT_DIR, "sphinx_doc_gen_monkeypatch.py")
    execfile(filepath)


# Avoid adding too many changes here.
def setup_blender():
    # Remove handlers since the functions get included
    # in the doc-string and don't have meaningful names.
    lists_to_restore = []
    for var in bpy.app.handlers:
        if isinstance(var, list):
            lists_to_restore.append((var[:], var))
            var.clear()

    return {
        "lists_to_restore": lists_to_restore,
    }


def teardown_blender(setup_data):
    for var_src, var_dst in setup_data["lists_to_restore"]:
        var_dst[:] = var_src


def main():

    # First monkey patch to load in fake members.
    setup_monkey_patch()

    # Perform changes to Blender itself.
    setup_data = setup_blender()

    # Eventually, create the directories.
    for dir_path in [ARGS.output_dir, SPHINX_IN]:
        if not os.path.exists(dir_path):
            os.mkdir(dir_path)

    # Eventually, log in files.
    if ARGS.log:
        bpy_logfile = os.path.join(ARGS.output_dir, ".bpy.log")
        bpy_logfilehandler = logging.FileHandler(bpy_logfile, mode="w")
        bpy_logfilehandler.setLevel(logging.DEBUG)
        BPY_LOGGER.addHandler(bpy_logfilehandler)

        # using a `FileHandler` seems to disable the `stdout`, so we add a `StreamHandler`.
        bpy_log_stdout_handler = logging.StreamHandler(stream=sys.stdout)
        bpy_log_stdout_handler.setLevel(logging.DEBUG)
        BPY_LOGGER.addHandler(bpy_log_stdout_handler)

    # In case of out-of-source build, copy the needed directories.
    if ARGS.output_dir != SCRIPT_DIR:
        # Examples directory.
        examples_dir_copy = os.path.join(ARGS.output_dir, "examples")
        if os.path.exists(examples_dir_copy):
            shutil.rmtree(examples_dir_copy, True)
        shutil.copytree(
            EXAMPLES_DIR,
            examples_dir_copy,
            ignore=shutil.ignore_patterns(*(".svn",)),
            copy_function=shutil.copy,
        )

    # Dump the API in RST files.
    if os.path.exists(SPHINX_IN_TMP):
        shutil.rmtree(SPHINX_IN_TMP, True)

    rna2sphinx(SPHINX_IN_TMP)

    if ARGS.changelog:
        generate_changelog()

    if ARGS.full_rebuild:
        # Only for full updates.
        shutil.rmtree(SPHINX_IN, True)
        shutil.copytree(
            SPHINX_IN_TMP,
            SPHINX_IN,
            copy_function=shutil.copy,
        )
        if ARGS.sphinx_build and os.path.exists(SPHINX_OUT):
            shutil.rmtree(SPHINX_OUT, True)
        if ARGS.sphinx_build_pdf and os.path.exists(SPHINX_OUT_PDF):
            shutil.rmtree(SPHINX_OUT_PDF, True)
    else:
        # Move changed files in `SPHINX_IN`.
        align_sphinx_in_to_sphinx_in_tmp(SPHINX_IN_TMP, SPHINX_IN)

    # Report which example files weren't used.
    EXAMPLE_SET_UNUSED = EXAMPLE_SET - EXAMPLE_SET_USED
    if EXAMPLE_SET_UNUSED:
        BPY_LOGGER.debug("\nUnused examples found in '%s'...", EXAMPLES_DIR)
        for f in sorted(EXAMPLE_SET_UNUSED):
            BPY_LOGGER.debug("    %s.py", f)
        BPY_LOGGER.debug("  %d total\n", len(EXAMPLE_SET_UNUSED))

    # Eventually, build the html docs.
    if ARGS.sphinx_build:
        import subprocess
        subprocess.call(SPHINX_BUILD)

        # Sphinx-build log cleanup+sort.
        if ARGS.log:
            if os.stat(SPHINX_BUILD_LOG).st_size:
                refactor_sphinx_log(SPHINX_BUILD_LOG)

    # Eventually, build the PDF docs.
    if ARGS.sphinx_build_pdf:
        import subprocess
        subprocess.call(SPHINX_BUILD_PDF)
        subprocess.call(SPHINX_MAKE_PDF, stdout=SPHINX_MAKE_PDF_STDOUT)

        # Sphinx-build log cleanup+sort.
        if ARGS.log:
            if os.stat(SPHINX_BUILD_PDF_LOG).st_size:
                refactor_sphinx_log(SPHINX_BUILD_PDF_LOG)

    # Eventually, prepare the dir to be deployed online (REFERENCE_PATH).
    if ARGS.pack_reference:

        if ARGS.sphinx_build:
            # Delete REFERENCE_PATH.
            if os.path.exists(REFERENCE_PATH):
                shutil.rmtree(REFERENCE_PATH, True)

            # Copy SPHINX_OUT to the REFERENCE_PATH.
            ignores = (".doctrees", ".buildinfo")
            shutil.copytree(
                SPHINX_OUT,
                REFERENCE_PATH,
                ignore=shutil.ignore_patterns(*ignores),
            )

            # Zip REFERENCE_PATH.
            basename = os.path.join(ARGS.output_dir, REFERENCE_NAME)
            tmp_path = shutil.make_archive(
                basename, "zip",
                root_dir=ARGS.output_dir,
                base_dir=REFERENCE_NAME,
            )
            final_path = os.path.join(REFERENCE_PATH, BLENDER_ZIP_FILENAME)
            os.rename(tmp_path, final_path)

        if ARGS.sphinx_build_pdf:
            # Copy the pdf to REFERENCE_PATH.
            shutil.copy(
                os.path.join(SPHINX_OUT_PDF, "contents.pdf"),
                os.path.join(REFERENCE_PATH, BLENDER_PDF_FILENAME),
            )

    teardown_blender(setup_data)

    sys.exit()


if __name__ == '__main__':
    main()
