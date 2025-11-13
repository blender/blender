# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
API dump in RST files
---------------------
  Run this script from Blender's root path once you have compiled Blender

    blender --background --factory-startup --python doc/python_api/sphinx_doc_gen.py

  This will generate Python files in doc/python_api/sphinx-in/
  providing ./blender is or links to the Blender executable

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
__all__ = (
    "main",
)

import os
import sys
import inspect
import shutil
import logging
import warnings

from textwrap import indent


try:
    import bpy  # Blender module.
except ImportError:
    print("\nERROR: this script must run from inside Blender")
    print(__doc__)
    sys.exit()

import _rna_info as rna_info  # Blender module.


def rna_info_BuildRNAInfo_cache():
    if rna_info_BuildRNAInfo_cache.ret is None:
        rna_info_BuildRNAInfo_cache.ret = rna_info.BuildRNAInfo()
    return rna_info_BuildRNAInfo_cache.ret


rna_info_BuildRNAInfo_cache.ret = None
# --- end rna_info cache

SCRIPT_DIR = os.path.abspath(os.path.dirname(__file__))

# For now, ignore add-ons and internal sub-classes of `bpy.types.PropertyGroup`.
#
# Besides disabling this line, the main change will be to add a
# `toctree` to `write_rst_index` which contains the generated RST files.
# This `toctree` can be generated automatically.
#
# See: D6261 for reference.
USE_ONLY_BUILTIN_RNA_TYPES = True

# Write a page for each static enum defined in:
# `source/blender/makesrna/RNA_enum_items.hh` so the enums can be linked to instead of being expanded everywhere.
USE_SHARED_RNA_ENUM_ITEMS_STATIC = True

# Generate a list of types which support custom properties.
# This isn't listed anywhere, it's just linked to.
USE_RNA_TYPES_WITH_CUSTOM_PROPERTY_INDEX = True

# Other types are assumed to be `bpy.types.*`.
PRIMITIVE_TYPE_NAMES = {"bool", "bytearray", "bytes", "dict", "float", "int", "list", "set", "str", "tuple"}

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

    # Optional arguments.
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
            "- OUTPUT_DIR/.bpy.log\n"
            "- OUTPUT_DIR/.sphinx-build.log\n"
            "- OUTPUT_DIR/.sphinx-build_pdf.log\n"
            "- OUTPUT_DIR/.latex_make.log"
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
    # Full build.
    FILTER_BPY_OPS = None
    FILTER_BPY_TYPES = None
    EXCLUDE_INFO_DOCS = False
    EXCLUDE_MODULES = []

else:
    # Can manually edit this too:
    # FILTER_BPY_OPS = ("import.scene", )  # allow
    # FILTER_BPY_TYPES = ("bpy_struct", "Operator", "ID")  # allow
    EXCLUDE_INFO_DOCS = True
    EXCLUDE_MODULES = [
        "aud",
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
        "bpy.ops",  # Supports filtering.
        "bpy.path",
        "bpy.props",
        "bpy.types",  # Supports filtering.
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

    # Special support for `bpy.types.*`.
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
    # Done filtering
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
    "../../../scripts/templates_py/gizmo_simple_3d.py",
    "../../../scripts/templates_py/operator_simple.py",
    "../../../scripts/templates_py/ui_panel_simple.py",
    "../../../scripts/templates_py/ui_previews_custom_icon.py",
    "../examples/bmesh.ops.1.py",
    "../examples/bpy.app.translations.py",
)


# Examples.
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
    # Included by: `info_gotcha.rst`.
    "info_gotchas_crashes.rst",
    "info_gotchas_threading.rst",
    "info_gotchas_internal_data_and_python_objects.rst",
    "info_gotchas_operators.rst",
    "info_gotchas_meshes.rst",
    "info_gotchas_armatures_and_bones.rst",
    "info_gotchas_file_paths_and_encoding.rst",
)

# Hide the actual TOC, use a separate list that links to the items.
# This is done so a short description can be included with each link.
USE_INFO_DOCS_FANCY_INDEX = True

# Only support for properties at the moment.
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
    # is registered and called from C++ code where the attribute is accessed from the instance.
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
BLENDER_VERSION_DOTS = "{:d}.{:d}".format(bpy.app.version[0], bpy.app.version[1])

# Example: `2_83`.
BLENDER_VERSION_PATH = "{:d}_{:d}".format(bpy.app.version[0], bpy.app.version[1])

# --------------------------DOWNLOADABLE FILES----------------------------------

REFERENCE_NAME = "blender_python_reference_{:s}".format(BLENDER_VERSION_PATH)
REFERENCE_PATH = os.path.join(ARGS.output_dir, REFERENCE_NAME)
BLENDER_PDF_FILENAME = "{:s}.pdf".format(REFERENCE_NAME)
BLENDER_ZIP_FILENAME = "{:s}.zip".format(REFERENCE_NAME)

# -------------------------------SPHINX-----------------------------------------

SPHINX_IN = os.path.join(ARGS.output_dir, "sphinx-in")
SPHINX_IN_TMP = SPHINX_IN + "-tmp"
SPHINX_OUT = os.path.join(ARGS.output_dir, "sphinx-out")

# HTML build.
if ARGS.sphinx_build:
    SPHINX_BUILD = ["sphinx-build", SPHINX_IN, SPHINX_OUT]

    if ARGS.log:
        SPHINX_BUILD_LOG = os.path.join(ARGS.output_dir, ".sphinx-build.log")
        SPHINX_BUILD = [
            "sphinx-build",
            "-w", SPHINX_BUILD_LOG,
            SPHINX_IN, SPHINX_OUT,
        ]

# PDF build.
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

# Unfortunately Python doesn't expose direct access to these types.
# Access them indirectly.
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
_BPY_PROP_COLLECTION_IDPROP_FAKE = "bpy_prop_collection_idprop"

if _BPY_PROP_COLLECTION_FAKE:
    _BPY_PROP_COLLECTION_ID = ":class:`{:s}`".format(_BPY_PROP_COLLECTION_FAKE)
else:
    _BPY_PROP_COLLECTION_ID = "collection"

if _BPY_STRUCT_FAKE:
    bpy_struct = bpy.types.bpy_struct
else:
    bpy_struct = None


def import_value_from_module(module_name, import_name):
    ns = {}
    exec_str = "from {:s} import {:s} as value".format(module_name, import_name)
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


def example_extract_docstring(filepath):
    """
    Return (text, line_no, line_no_has_content) where:
    - ``text`` is the doc-string text.
    - ``line_no`` is the line the doc-string text ends.
    - ``line_no_has_content`` when False, this file only contains a doc-string.
      There is no need to include the remainder.
    """
    with open(filepath, "r", encoding="utf-8") as fh:
        line = fh.readline()
        line_no = 0
        text = []
        if line.startswith('"""'):  # Assume nothing here.
            line_no += 1
        else:
            return "", 0, True

        for line in fh:
            line_no += 1
            if line.startswith('"""'):
                break
            text.append(line.rstrip())

        line_no += 1
        line_no_has_content = False

        # Skip over blank lines so the Python code doesn't have blank lines at the top.
        for line in fh:
            if line.strip():
                line_no_has_content = True
                break
            line_no += 1

        return "\n".join(text).rstrip("\n"), line_no, line_no_has_content


def title_string(text, heading_char, double=False):
    filler = len(text) * heading_char

    if double:
        return "{:s}\n{:s}\n{:s}\n\n".format(filler, text, filler)
    return "{:s}\n{:s}\n\n".format(text, filler)


def write_example_ref(ident, fw, example_id, ext="py"):
    if example_id in EXAMPLE_SET:

        # Extract the comment.
        filepath = os.path.join("..", "examples", "{:s}.{:s}".format(example_id, ext))
        filepath_full = os.path.join(os.path.dirname(fw.__self__.name), filepath)

        text, line_no, line_no_has_content = example_extract_docstring(filepath_full)
        if text:
            # Ensure a blank line, needed since in some cases the indentation doesn't match the previous line.
            # which causes Sphinx not to warn about bad indentation.
            fw("\n")
            for line in text.split("\n"):
                fw("{:s}\n".format((ident + line).rstrip()))

        fw("\n")

        # Some files only contain a doc-string.
        if line_no_has_content:
            fw("{:s}.. literalinclude:: {:s}\n".format(ident, filepath))
            if line_no > 0:
                fw("{:s}   :lines: {:d}-\n".format(ident, line_no))
            fw("\n")
        EXAMPLE_SET_USED.add(example_id)
    else:
        if bpy.app.debug:
            BPY_LOGGER.debug("\tskipping example: %s", example_id)

    # Support for numbered files `bpy.types.Operator` -> `bpy.types.Operator.1.py`.
    i = 1
    while True:
        example_id_num = "{:s}.{:d}".format(example_id, i)
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


def pyfunc_is_inherited_method(py_func, identifier):
    assert type(py_func) == MethodType
    # Exclude Mix-in classes (after the first), because these don't get their own documentation.
    cls = py_func.__self__
    if (py_func_base := getattr(cls.__base__, identifier, None)) is not None:
        if type(py_func_base) == MethodType:
            if py_func.__func__ == py_func_base.__func__:
                return True
        elif type(py_func_base) == bpy.types.bpy_func:
            return True
    return False


def pyfunc2sphinx(ident, fw, module_name, type_name, identifier, py_func, is_class=True):
    """
    function or class method to sphinx
    """

    if type(py_func) == MethodType:
        # Including methods means every operators "poll" function example
        # would be listed in documentation which isn't useful.
        #
        # However, excluding all of them is also incorrect as it means class methods defined
        # in `_bpy_types.py` for example are excluded, making some utility functions entirely hidden.
        if (bl_rna := getattr(py_func.__self__, "bl_rna", None)) is not None:
            if bl_rna.functions.get(identifier) is not None:
                return
        del bl_rna

        # Only inline the method if it's not inherited from another class.
        if pyfunc_is_inherited_method(py_func, identifier):
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
        if type(py_func) == MethodType:
            func_type = "classmethod"
        else:
            func_type = "staticmethod"

    doc = py_func.__doc__
    if (not doc) or (not doc.startswith(".. {:s}:: ".format(func_type))):
        fw(ident + ".. {:s}:: {:s}{:s}\n\n".format(func_type, identifier, arg_str))
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
        fw(ident + ".. attribute:: {:s}\n\n".format(identifier))
        # NOTE: `RST_NOINDEX_ATTR` currently not supported (as it's not used).
        write_indented_lines(ident + "   ", fw, doc, False)
        fw("\n")
    elif type(descr) == MemberDescriptorType:  # Same as above but use "data".
        fw(ident + ".. data:: {:s}\n\n".format(identifier))
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
    C/C++ defined function to Sphinx.
    """

    # Dump the doc-string, assume its formatted correctly.
    if py_func.__doc__:
        write_indented_lines(ident, fw, py_func.__doc__, False)
        fw("\n")
    else:
        fw(ident + ".. function:: {:s}()\n\n".format(identifier))
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
        fw(ident + ".. data:: {:s}\n\n".format(identifier))
    else:
        fw(ident + ".. attribute:: {:s}\n\n".format(identifier))

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

    fw(title_string("{:s} ({:s})".format(title, module_name), "="))

    fw(".. module:: {:s}\n\n".format(module_name))

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
                submod_name_full = "{:s}.{:s}".format(module_name, submod_name)
                fw("   {:s}.rst\n".format(submod_name_full))

                pymodule2sphinx(basepath, submod_name_full, submod, "{:s} submodule".format(module_name), ())
            fw("\n")
        del submod_ls
    # Done writing sub-modules!

    write_example_ref("", fw, module_name)

    # Write members of the module.
    # Only tested with `PyStructs` which are not exactly modules.
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

        # Workaround for `bpy.app` documenting `.index()` and `.count()`.
        if isinstance(module, tuple) and hasattr(tuple, attribute):
            continue

        value = getattr(module, attribute)

        module_dir_value_type.append((attribute, value, type(value)))

    # Sort by `str` of each type this way lists, functions etc are grouped.
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
            fw(".. data:: {:s}\n\n".format(attribute))
            write_indented_lines("   ", fw, "Constant value {!r}".format(value), False)
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
            fw("- :mod:`{:s}.{:s}`\n".format(module_name, attribute))
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

        pyclass2sphinx(fw, module_name, type_name, value, True)

    file.close()


def pyclass2sphinx(fw, module_name, type_name, value, write_class_examples):
    if value.__doc__:
        if value.__doc__.startswith(".. class::"):
            fw(value.__doc__)
        else:
            fw(".. class:: {:s}.{:s}\n\n".format(module_name, type_name))
            write_indented_lines("   ", fw, value.__doc__, True)
    else:
        fw(".. class:: {:s}.{:s}\n\n".format(module_name, type_name))
    fw("\n")

    if write_class_examples:
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


# Changes In Blender will force errors here.
context_type_map = {
    # Support multiple types for each item, where each list item is a possible type:
    # `context_member: [(RNA type, is_collection), ...]`
    "active_action": [("Action", False)],
    "active_annotation_layer": [("GPencilLayer", False)],
    "active_bone": [("EditBone", False), ("Bone", False)],
    "active_file": [("FileSelectEntry", False)],
    "active_node": [("Node", False)],
    "active_object": [("Object", False)],
    "active_operator": [("Operator", False)],
    "active_pose_bone": [("PoseBone", False)],
    "active_strip": [("Strip", False)],
    "active_editable_fcurve": [("FCurve", False)],
    "active_nla_strip": [("NlaStrip", False)],
    "active_nla_track": [("NlaTrack", False)],
    "annotation_data": [("GreasePencil", False)],
    "annotation_data_owner": [("ID", False)],
    "armature": [("Armature", False)],
    "asset": [("AssetRepresentation", False)],
    "asset_library_reference": [("AssetLibraryReference", False)],
    "bone": [("Bone", False)],
    "brush": [("Brush", False)],
    "camera": [("Camera", False)],
    "cloth": [("ClothModifier", False)],
    "collection": [("LayerCollection", False)],
    "collision": [("CollisionModifier", False)],
    "curve": [("Curve", False)],
    "dynamic_paint": [("DynamicPaintModifier", False)],
    "edit_bone": [("EditBone", False)],
    "edit_image": [("Image", False)],
    "edit_mask": [("Mask", False)],
    "edit_movieclip": [("MovieClip", False)],
    "edit_object": [("Object", False)],
    "edit_text": [("Text", False)],
    "editable_bones": [("EditBone", True)],
    "editable_objects": [("Object", True)],
    "editable_fcurves": [("FCurve", True)],
    "fluid": [("FluidSimulationModifier", False)],
    "gpencil": [("GreasePencil", False)],
    "grease_pencil": [("GreasePencil", False)],
    "curves": [("Hair Curves", False)],
    "id": [("ID", False)],
    "image_paint_object": [("Object", False)],
    "lattice": [("Lattice", False)],
    "light": [("Light", False)],
    "lightprobe": [("LightProbe", False)],
    "line_style": [("FreestyleLineStyle", False)],
    "material": [("Material", False)],
    "material_slot": [("MaterialSlot", False)],
    "mesh": [("Mesh", False)],
    "meta_ball": [("MetaBall", False)],
    "object": [("Object", False)],
    "objects_in_mode": [("Object", True)],
    "objects_in_mode_unique_data": [("Object", True)],
    "particle_edit_object": [("Object", False)],
    "particle_settings": [("ParticleSettings", False)],
    "particle_system": [("ParticleSystem", False)],
    "particle_system_editable": [("ParticleSystem", False)],
    "pointcloud": [("PointCloud", False)],
    "pose_bone": [("PoseBone", False)],
    "pose_object": [("Object", False)],
    "property": [("AnyType", False), ("str", False), ("int", False)],
    "scene": [("Scene", False)],
    "sculpt_object": [("Object", False)],
    "selectable_objects": [("Object", True)],
    "selected_assets": [("AssetRepresentation", True)],
    "selected_bones": [("EditBone", True)],
    "selected_editable_actions": [("Action", True)],
    "selected_editable_bones": [("EditBone", True)],
    "selected_editable_fcurves": [("FCurve", True)],
    "selected_editable_keyframes": [("Keyframe", True)],
    "selected_editable_objects": [("Object", True)],
    "selected_editable_strips": [("Strip", True)],
    "selected_files": [("FileSelectEntry", True)],
    "selected_ids": [("ID", True)],
    "selected_nla_strips": [("NlaStrip", True)],
    "selected_movieclip_tracks": [("MovieTrackingTrack", True)],
    "selected_nodes": [("Node", True)],
    "selected_objects": [("Object", True)],
    "selected_pose_bones": [("PoseBone", True)],
    "selected_pose_bones_from_active_object": [("PoseBone", True)],
    "selected_strips": [("Strip", True)],
    "selected_visible_actions": [("Action", True)],
    "selected_visible_fcurves": [("FCurve", True)],
    "sequencer_scene": [("Scene", False)],
    "strips": [("Strip", True)],
    "strip": [("Strip", False)],
    "strip_modifier": [("StripModifier", False)],
    "soft_body": [("SoftBodyModifier", False)],
    "speaker": [("Speaker", False)],
    "texture": [("Texture", False)],
    "texture_node": [("Node", False)],
    "texture_slot": [("TextureSlot", False)],
    "texture_user": [("ID", False)],
    "texture_user_property": [("Property", False)],
    "tool_settings": [("ToolSettings", False)],
    "ui_list": [("UIList", False)],
    "vertex_paint_object": [("Object", False)],
    "view_layer": [("ViewLayer", False)],
    "visible_bones": [("EditBone", True)],
    "visible_objects": [("Object", True)],
    "visible_pose_bones": [("PoseBone", True)],
    "visible_fcurves": [("FCurve", True)],
    "weight_paint_object": [("Object", False)],
    "volume": [("Volume", False)],
    "world": [("World", False)],
}

if bpy.app.build_options.experimental_features:
    for key, value in {
        # No experimental members in context currently.
    }.items():
        assert key not in context_type_map, "Duplicate, the member must be removed from one of the dictionaries"
        context_type_map[key] = value


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
    fw("Note that all context values are read-only,\n")
    fw("but may be modified through the data API or by running operators.\n\n")

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
                class_fmt=":class:`bpy.types.{:s}`",
                mathutils_fmt=":class:`mathutils.{:s}`",
                literal_fmt="``{!r}``",  # String with quotes.
                collection_id=_BPY_PROP_COLLECTION_ID,
                enum_descr_override=enum_descr_override,
            )
            fw(".. data:: {:s}\n\n".format(prop.identifier))
            if prop.description:
                fw("   {:s}\n\n".format(prop.description))
            if (deprecated := prop.deprecated) is not None:
                fw(pyrna_deprecated_directive("   ", deprecated))

            # Special exception, can't use generic code here for enums.
            if prop.type == "enum":
                # If the link has been written, no need to inline the enum items.
                enum_text = "" if enum_descr_override else pyrna_enum2sphinx(prop)
                if enum_text:
                    write_indented_lines("   ", fw, enum_text)
                    fw("\n")
                del enum_text
            # End enum exception.

            fw("   :type: {:s}\n\n".format(type_descr))

    write_contex_cls()
    del write_contex_cls
    # End.

    # Internal API call only intended to be used to extract context members.
    from _bpy import context_members
    context_member_map = context_members()
    del context_members

    # Track unique for `context_strings` to validate `context_type_map`.
    unique_context_strings = set()
    for ctx_str, ctx_members in sorted(context_member_map.items()):
        subsection = "{:s} Context".format(ctx_str.split("_")[0].title())
        fw("\n{:s}\n{:s}\n\n".format(subsection, (len(subsection) * "-")))
        for member in ctx_members:
            unique_all_len = len(unique)
            unique.add(member)
            member_visited = unique_all_len == len(unique)

            unique_context_strings.add(member)

            fw(".. data:: {:s}\n".format(member))
            # Avoid warnings about the member being included multiple times.
            if member_visited:
                fw("   :noindex:\n")
            fw("\n")

            if (member_types := context_type_map.get(member)) is None:
                raise SystemExit(
                    "Error: context key {!r} not found in context_type_map; update {:s}".format(member, __file__)
                ) from None
            if len(member_types) == 0:
                raise SystemExit(
                    "Error: context key {!r} empty in context_type_map; update {:s}".format(member, __file__)
                )

            type_strs = []
            for member_type, is_seq in member_types:
                if member_type.isidentifier():
                    type_strs.append(
                        "{:s}:class:`{:s}{:s}`".format(
                            "sequence of " if is_seq else "",
                            "bpy.types." if member_type not in PRIMITIVE_TYPE_NAMES else "",
                            member_type,
                        )
                    )
                else:
                    type_strs.append(member_type)

            fw("   :type: {:s}\n\n".format(" or ".join(type_strs)))
            write_example_ref("   ", fw, "bpy.context." + member)

    # Generate type-map:
    # for member in sorted(unique_context_strings):
    #     print('        "{:s}": ("", False),'.format(member))
    if len(context_type_map) > len(unique_context_strings):
        warnings.warn(
            "Some types are not used: {:s}".format(
                str([member for member in context_type_map if member not in unique_context_strings]),
            ))
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
            "- ``{:s}``\n"
            "{:s}.\n".format(
                identifier,
                # Account for multi-line enum descriptions, allowing this to be a block of text.
                indent(" -- ".join(escape_rst(val) for val in (name, description) if val) or "Undocumented", "  "),
            )
            for identifier, name, description in prop.enum_items
        ])
    return ""


def pyrna_deprecated_directive(ident, deprecated):
    note, version, removal_version = deprecated

    # Show a short 2 number version where possible to reduce noise.
    version_str = "{:d}.{:d}.{:d}".format(*version).removesuffix(".0")
    removal_version_str = "{:d}.{:d}.{:d}".format(*removal_version).removesuffix(".0")

    return (
        "{:s}.. deprecated:: {:s} removal planned in version {:s}\n"
        "\n"
        "{:s}   {:s}\n"
    ).format(
        ident, version_str, removal_version_str,
        ident, note,
    )


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
            identifier = " {:s}".format(prop.identifier)

        kwargs["class_fmt"] = ":class:`{:s}`"
        kwargs["mathutils_fmt"] = ":class:`mathutils.{:s}`"
        kwargs["literal_fmt"] = "``{!r}``"  # String with quotes.

        kwargs["collection_id"] = _BPY_PROP_COLLECTION_ID

        enum_descr_override = None
        if USE_SHARED_RNA_ENUM_ITEMS_STATIC:
            enum_descr_override = pyrna_enum2sphinx_shared_link(prop)
            kwargs["enum_descr_override"] = enum_descr_override

        type_descr = prop.get_type_description(**kwargs)

        # If the link has been written, no need to inline the enum items.
        enum_text = "" if enum_descr_override else pyrna_enum2sphinx(prop)
        if prop.name or prop.description or enum_text:
            fw(ident + ":{:s}{:s}: ".format(id_name, identifier))

            if prop.name or prop.description:
                fw(", ".join(val for val in (prop.name, prop.description.replace("\n", "")) if val) + "\n")

            # Special exception, can't use generic code here for enums.
            if enum_text:
                fw("\n")
                write_indented_lines(ident + "   ", fw, enum_text)
            del enum_text
            # End enum exception.

        fw(ident + ":{:s}{:s}: {:s}\n".format(id_type, identifier, type_descr))

    def write_struct(struct):
        # if not struct.identifier.startswith("Sc") and not struct.identifier.startswith("I"):
        #     return

        # if not struct.identifier == "Object":
        #     return

        struct_module_name = struct.module_name
        if USE_ONLY_BUILTIN_RNA_TYPES:
            assert struct_module_name == "bpy.types"
        filepath = os.path.join(basepath, "{:s}.{:s}.rst".format(struct_module_name, struct.identifier))
        file = open(filepath, "w", encoding="utf-8")
        fw = file.write

        base_id = getattr(struct.base, "identifier", "")
        struct_id = struct.identifier

        if _BPY_STRUCT_FAKE:
            if not base_id:
                base_id = _BPY_STRUCT_FAKE

        if base_id:
            title = "{:s}({:s})".format(struct_id, base_id)
        else:
            title = struct_id

        fw(title_string(title, "="))

        fw(".. currentmodule:: {:s}\n\n".format(struct_module_name))

        # Docs first? OK.
        write_example_ref("", fw, "{:s}.{:s}".format(struct_module_name, struct_id))

        base_ids = [base.identifier for base in struct.get_bases()]

        if _BPY_STRUCT_FAKE:
            base_ids.append(_BPY_STRUCT_FAKE)

        base_ids.reverse()

        if base_ids:
            if len(base_ids) > 1:
                fw("base classes --- ")
            else:
                fw("base class --- ")

            fw(", ".join((":class:`{:s}`".format(base_id)) for base_id in base_ids))
            fw("\n\n")

        subclass_ids = [
            s.identifier for s in structs.values()
            if s.base is struct
            if not rna_info.rna_id_ignore(s.identifier)
        ]
        subclass_ids.sort()
        if subclass_ids:
            fw("subclasses --- \n" + ", ".join((":class:`{:s}`".format(s)) for s in subclass_ids) + "\n\n")

        base_id = getattr(struct.base, "identifier", "")

        if _BPY_STRUCT_FAKE:
            if not base_id:
                base_id = _BPY_STRUCT_FAKE

        if base_id:
            fw(".. class:: {:s}({:s})\n\n".format(struct_id, base_id))
        else:
            fw(".. class:: {:s}\n\n".format(struct_id))

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
                class_fmt=":class:`{:s}`",
                mathutils_fmt=":class:`mathutils.{:s}`",
                literal_fmt="``{!r}``",  # String with quotes.
                collection_id=_BPY_PROP_COLLECTION_ID,
                enum_descr_override=enum_descr_override,
            )
            # Read-only properties use "data" directive, variables properties use "attribute" directive.
            if "readonly" in type_descr:
                fw("   .. data:: {:s}\n".format(identifier))
            else:
                fw("   .. attribute:: {:s}\n".format(identifier))
            # Also write `noindex` on request.
            if ("bpy.types", struct_id, identifier) in RST_NOINDEX_ATTR:
                fw("      :noindex:\n")
            fw("\n")

            if prop.description:
                write_indented_lines("      ", fw, prop.description, False)
                fw("\n")
            if (deprecated := prop.deprecated) is not None:
                fw(pyrna_deprecated_directive("      ", deprecated))
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

            fw("      :type: {:s}\n\n".format(type_descr))

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
            args_kw_only_index = next((i for i, prop in enumerate(func.args) if not prop.is_required), -1)
            if args_kw_only_index == -1:
                args_str = ", ".join(prop.get_arg_default(force=False) for prop in func.args)
            else:
                args_str = ", ".join([
                    *[prop.get_arg_default(force=False) for prop in func.args[:args_kw_only_index]],
                    # Keyword only.
                    "*",
                    *[prop.get_arg_default(force=False) for prop in func.args[args_kw_only_index:]],

                ])
            del args_kw_only_index

            fw("   .. {:s}:: {:s}({:s})\n\n".format(
                "classmethod" if func.is_classmethod else "method",
                func.identifier,
                args_str,
            ))
            fw("      {:s}\n\n".format(func.description))

            for prop in func.args:
                write_param("      ", fw, prop)

            if len(func.return_values) == 1:
                write_param("      ", fw, func.return_values[0], is_return=True)
            elif func.return_values:  # Multiple return values.
                fw("      :return:\n")
                type_descrs = []
                for prop in func.return_values:
                    # TODO: pyrna_enum2sphinx for multiple return values,
                    # actually don't think we even use this but still!

                    enum_descr_override = None
                    if USE_SHARED_RNA_ENUM_ITEMS_STATIC:
                        enum_descr_override = pyrna_enum2sphinx_shared_link(prop)

                    type_descr = prop.get_type_description(
                        as_ret=True, class_fmt=":class:`{:s}`",
                        mathutils_fmt=":class:`mathutils.{:s}`",
                        literal_fmt="``{!r}``",  # String with quotes.
                        collection_id=_BPY_PROP_COLLECTION_ID,
                        enum_descr_override=enum_descr_override,
                    )
                    type_descrs.append(type_descr)
                    descr = prop.description
                    if not descr:
                        descr = prop.name
                    # In rare cases `descr` may be empty.
                    fw("         ``{:s}``, {:s}\n\n".format(
                        prop.identifier,
                        ", ".join((val for val in (descr, type_descr) if val))
                    ))
                    if (deprecated := prop.deprecated) is not None:
                        fw(pyrna_deprecated_directive("      ", deprecated))
                        fw("\n")

                fw("      :rtype: ({:s})\n".format(", ".join(type_descrs)))

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
                        lines.append("   - :class:`{:s}.{:s}`\n".format(_BPY_STRUCT_FAKE, key))

            for base in bases:
                for prop in base.properties:
                    lines.append("   - :class:`{:s}.{:s}`\n".format(base.identifier, prop.identifier))

                for identifier, py_prop in base.get_py_properties():
                    lines.append("   - :class:`{:s}.{:s}`\n".format(base.identifier, identifier))

            if lines:
                fw(title_string("Inherited Properties", "-"))

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
                        lines.append("   - :class:`{:s}.{:s}`\n".format(_BPY_STRUCT_FAKE, key))

            for base in bases:
                for func in base.functions:
                    lines.append("   - :class:`{:s}.{:s}`\n".format(base.identifier, func.identifier))
                for identifier, py_func in base.get_py_functions():
                    lines.append("   - :class:`{:s}.{:s}`\n".format(base.identifier, identifier))
                for identifier, py_func in base.get_py_c_functions():
                    lines.append("   - :class:`{:s}.{:s}`\n".format(base.identifier, identifier))

            if lines:
                fw(title_string("Inherited Functions", "-"))

                fw(".. hlist::\n")
                fw("   :columns: 2\n\n")

                for line in lines:
                    fw(line)
                fw("\n")

            del lines[:]

        if struct.references:
            fw(title_string("References", "-"))

            fw(".. hlist::\n")
            fw("   :columns: 2\n\n")

            # Context does its own thing.
            # "active_object": [("Object", False)],
            for ref_attr, ref_types in sorted(context_type_map.items()):
                for ref_type, _ in ref_types:
                    if ref_type == struct_id:
                        fw("   - :mod:`bpy.context.{:s}`\n".format(ref_attr))
            del ref_attr, ref_types

            for ref in struct.references:
                ref_split = ref.split(".")
                if len(ref_split) > 2:
                    ref = ref_split[-2] + "." + ref_split[-1]
                fw("   - :class:`{:s}`\n".format(ref))
            fw("\n")

        # Docs last?, disable for now.
        # write_example_ref("", fw, "bpy.types.{:s}".format(struct_id))
        file.close()

    if "bpy.types" not in EXCLUDE_MODULES:
        for struct in structs.values():
            # TODO: rna_info should filter these out!
            if "_OT_" in struct.identifier:
                continue
            write_struct(struct)

        def fake_bpy_type(
                class_module_name,
                class_value,
                class_name,
                descr_str,
                *,
                use_subclasses,  # `bool`
                base_class,  # `str | None`
        ):
            filepath = os.path.join(basepath, "{:s}.{:s}.rst".format(class_module_name, class_name))
            file = open(filepath, "w", encoding="utf-8")
            fw = file.write

            fw(title_string(class_name, "="))

            fw(".. currentmodule:: {:s}\n\n".format(class_module_name))

            if base_class is not None:
                fw("base classes --- :class:`{:s}`\n\n".format(base_class))

            if use_subclasses:
                subclass_ids = [
                    s.identifier for s in structs.values()
                    if s.base is None
                    if not rna_info.rna_id_ignore(s.identifier)
                ]
                if subclass_ids:
                    fw("subclasses --- \n" + ", ".join((":class:`{:s}`".format(s))
                       for s in sorted(subclass_ids)) + "\n\n")

            fw(".. class:: {:s}\n\n".format(class_name))
            fw("   {:s}\n\n".format(descr_str))
            fw("   .. note::\n\n")
            fw("      Note that :class:`{:s}.{:s}` is not actually available from within Blender,\n"
               "      it only exists for the purpose of documentation.\n\n".format(class_module_name, class_name))

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

        # Write fake classes.
        if _BPY_STRUCT_FAKE:
            class_value = bpy_struct
            fake_bpy_type(
                "bpy.types", class_value, _BPY_STRUCT_FAKE,
                "built-in base class for all classes in bpy.types.",
                use_subclasses=True,
                base_class=None,
            )

        if _BPY_PROP_COLLECTION_FAKE:
            class_value = bpy.types.bpy_prop_collection
            fake_bpy_type(
                "bpy.types", class_value, _BPY_PROP_COLLECTION_FAKE,
                "built-in class used for all collections.",
                use_subclasses=False,
                base_class=None,
            )

        if _BPY_PROP_COLLECTION_IDPROP_FAKE:
            class_value = bpy.types.bpy_prop_collection_idprop
            fake_bpy_type(
                "bpy.types", class_value, _BPY_PROP_COLLECTION_IDPROP_FAKE,
                "built-in class used for user defined collections.",
                use_subclasses=False,
                base_class=_BPY_PROP_COLLECTION_FAKE,
            )

    # Operators.
    def write_ops():
        API_BASEURL = "https://projects.blender.org/blender/blender/src/branch/main/scripts"

        op_modules = {}
        op = None
        for op in ops.values():
            op_modules.setdefault(op.module_name, []).append(op)
        del op

        for op_module_name, ops_mod in op_modules.items():
            filepath = os.path.join(basepath, "bpy.ops.{:s}.rst".format(op_module_name))
            file = open(filepath, "w", encoding="utf-8")
            fw = file.write

            title = "{:s} Operators".format(op_module_name.replace("_", " ").title())

            fw(title_string(title, "="))

            fw(".. module:: bpy.ops.{:s}\n\n".format(op_module_name))

            ops_mod.sort(key=lambda op: op.func_name)

            for op in ops_mod:
                args_str = ", ".join(prop.get_arg_default(force=True) for prop in op.args)
                # All operator arguments are keyword only (denoted by the leading `*`).
                fw(".. function:: {:s}({:s}{:s})\n\n".format(op.func_name, "*, " if args_str else "", args_str))

                # If the description isn't valid, we output the standard warning
                # with a link to the wiki so that people can help.
                if not op.description or op.description == "(undocumented operator)":
                    operator_description = undocumented_message("bpy.ops", op.module_name, op.func_name)
                else:
                    operator_description = op.description

                # Set `strip` to false as `operator_description` must never be indented.
                write_indented_lines("   ", fw, operator_description, strip=False)
                fw("\n")
                for prop in op.args:
                    write_param("   ", fw, prop)

                location = op.get_location()
                if location != (None, None):
                    fw("   :File: `{:s}\\:{:d} <{:s}/{:s}#L{:d}>`__\n\n".format(
                        location[0], location[1], API_BASEURL, location[0], location[1]
                    ))

                if op.args:
                    fw("\n")

            file.close()

    if "bpy.ops" not in EXCLUDE_MODULES:
        write_ops()


def write_rst_index(basepath):
    """
    Write the RST file of the main page, needed for sphinx: ``index.html``.
    """
    filepath = os.path.join(basepath, "index.rst")
    file = open(filepath, "w", encoding="utf-8")
    fw = file.write

    fw(title_string("Blender {:s} Python API Documentation".format(BLENDER_VERSION_DOTS), "%", double=True))
    fw("\n")
    fw("Welcome to the Python API documentation for `Blender <https://www.blender.org>`__, ")
    fw("the free and open source 3D creation suite.\n")
    fw("\n")

    # fw("`A PDF version of this document is also available <{:s}>`_\n".format(BLENDER_PDF_FILENAME))
    fw("This site can be used offline: `Download the full documentation (zipped HTML files) <{:s}>`__\n".format(
        BLENDER_ZIP_FILENAME,
    ))
    fw("\n")

    if not EXCLUDE_INFO_DOCS:
        fw(".. toctree::\n")
        if USE_INFO_DOCS_FANCY_INDEX:
            fw("   :hidden:\n")
        fw("   :maxdepth: 1\n")
        fw("   :caption: Documentation\n\n")
        for info, info_desc in INFO_DOCS:
            fw("   {:s}\n".format(info))
        fw("\n")

        if USE_INFO_DOCS_FANCY_INDEX:
            # Show a fake TOC, allowing for an extra description to be shown as well as the title.
            fw(title_string("Documentation", "="))
            for info, info_desc in INFO_DOCS:
                fw("- :doc:`{:s}`: {:s}\n".format(info.removesuffix(".rst"), info_desc))
            fw("\n")

    fw(".. toctree::\n")
    fw("   :maxdepth: 1\n")
    fw("   :caption: Application Modules\n\n")

    app_modules = (
        "bpy.context",  # NOTE: not actually a module.
        "bpy.data",     # NOTE: not actually a module.
        "bpy.msgbus",   # NOTE: not actually a module.
        "bpy.ops",
        "bpy.types",

        # Python modules.
        "bpy.utils",
        "bpy.path",
        "bpy.app",

        # Python C-API modules.
        "bpy.props",
    )

    for mod in app_modules:
        if mod not in EXCLUDE_MODULES:
            fw("   {:s}\n".format(mod))
    fw("\n")

    fw(".. toctree::\n")
    fw("   :maxdepth: 1\n")
    fw("   :caption: Standalone Modules\n\n")

    standalone_modules = (
        # Sub-modules are added in parent page.
        "aud",
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
            fw("   {:s}\n".format(mod))
    fw("\n")

    fw(title_string("Indices", "="))
    fw("- :ref:`genindex`\n")
    fw("- :ref:`modindex`\n\n")

    # Special case, this `bmesh.ops.rst` is extracted from C++ source.
    if "bmesh.ops" not in EXCLUDE_MODULES:
        execfile(os.path.join(SCRIPT_DIR, "rst_from_bmesh_opdefines.py"))

    file.close()


def write_rst_bpy(basepath):
    """
    Write RST file of ``bpy`` module (disabled by default)
    """
    if not ARGS.bpy:
        return

    filepath = os.path.join(basepath, "bpy.rst")
    with open(filepath, "w", encoding="utf-8") as fh:
        fw = fh.write

        fw("\n")

        title = ":mod:`bpy` --- Blender Python Module"

        fw(title_string(title, "="))

        fw(".. module:: bpy.types\n\n")


def write_rst_types_index(basepath):
    """
    Write the RST file of ``bpy.types`` module (index)
    """
    if "bpy.types" in EXCLUDE_MODULES:
        return

    filepath = os.path.join(basepath, "bpy.types.rst")
    with open(filepath, "w", encoding="utf-8") as fh:
        fw = fh.write
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

        # This needs to be included somewhere, while it's hidden, list to avoid warnings.
        if USE_RNA_TYPES_WITH_CUSTOM_PROPERTY_INDEX:
            fw(".. toctree::\n")
            fw("   :hidden:\n")
            fw("   :maxdepth: 1\n\n")
            fw("   Types with Custom Property Support <bpy_types_custom_properties>\n\n")


def write_rst_ops_index(basepath):
    """
    Write the RST file of bpy.ops module (index)
    """
    if "bpy.ops" in EXCLUDE_MODULES:
        return

    filepath = os.path.join(basepath, "bpy.ops.rst")
    with open(filepath, "w", encoding="utf-8") as fh:
        fw = fh.write
        fw(title_string("Operators (bpy.ops)", "="))
        fw(".. module:: bpy.ops\n\n")
        write_example_ref("", fw, "bpy.ops")
        fw(".. toctree::\n")
        fw("   :caption: Submodules\n")
        # Only show top-level entries (avoids unreasonably large pages).
        fw("   :maxdepth: 1\n")
        fw("   :glob:\n\n")
        fw("   bpy.ops.*\n\n")


def write_rst_geometry_set(basepath):
    """
    Write the RST file for ``bpy.types.GeometrySet``.
    """
    if 'bpy.types.GeometrySet' in EXCLUDE_MODULES:
        return

    # Write the index.
    filepath = os.path.join(basepath, "bpy.types.GeometrySet.rst")
    with open(filepath, "w", encoding="utf-8") as fh:
        fw = fh.write
        fw(title_string("GeometrySet", "="))
        write_example_ref("", fw, "bpy.types.GeometrySet")
        pyclass2sphinx(fw, "bpy.types", "GeometrySet", bpy.types.GeometrySet, False)

    EXAMPLE_SET_USED.add("bpy.types.GeometrySet")


def write_rst_inline_shader_nodes(basepath):
    """
    Write the RST files for ``bpy.types.InlineShaderNodes``.
    """
    if 'bpy.types.InlineShaderNodes' in EXCLUDE_MODULES:
        return

    # Write the index.
    filepath = os.path.join(basepath, "bpy.types.InlineShaderNodes.rst")
    with open(filepath, "w", encoding="utf-8") as fh:
        fw = fh.write
        fw(title_string("InlineShaderNodes", "="))
        write_example_ref("", fw, "bpy.types.InlineShaderNodes")
        pyclass2sphinx(fw, "bpy.types", "InlineShaderNodes", bpy.types.InlineShaderNodes, False)

    EXAMPLE_SET_USED.add("bpy.types.InlineShaderNodes")


def write_rst_msgbus(basepath):
    """
    Write the RST files of ``bpy.msgbus`` module
    """
    if 'bpy.msgbus' in EXCLUDE_MODULES:
        return

    # Write the index.
    filepath = os.path.join(basepath, "bpy.msgbus.rst")
    with open(filepath, "w", encoding="utf-8") as fh:
        fw = fh.write
        fw(title_string("Message Bus (bpy.msgbus)", "="))
        write_example_ref("", fw, "bpy.msgbus")
        fw(".. toctree::\n")
        fw("   :glob:\n\n")
        fw("   bpy.msgbus.*\n\n")

    # Write the contents.
    pymodule2sphinx(basepath, 'bpy.msgbus', bpy.msgbus, 'Message Bus', ())
    EXAMPLE_SET_USED.add("bpy.msgbus")


def write_rst_data(basepath):
    """
    Write the RST file of ``bpy.data`` module.
    """
    if "bpy.data" in EXCLUDE_MODULES:
        return

    # Not actually a module, only write this file so we can reference in the TOC.
    filepath = os.path.join(basepath, "bpy.data.rst")
    with open(filepath, "w", encoding="utf-8") as fh:
        fw = fh.write
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
        return ":ref:`{:s}`".format(identifier)
    return None


def write_rst_enum_items(basepath, key, key_no_prefix, enum_items):
    """
    Write a single page for a static enum in RST.

    This helps avoiding very large lists being in-lined in many places which is an issue
    especially with icons in ``bpy.types.UILayout``. See #87008.
    """
    filepath = os.path.join(basepath, "{:s}.rst".format(key_no_prefix))
    with open(filepath, "w", encoding="utf-8") as fh:
        fw = fh.write
        # fw(".. noindex::\n\n")
        fw(".. _{:s}:\n\n".format(key))

        fw(title_string(key_no_prefix.replace("_", " ").title(), "#"))

        for item in enum_items:
            identifier = item.identifier
            name = item.name
            description = item.description
            if identifier:
                fw(":{:s}: {:s}\n".format(item.identifier, (escape_rst(name) + ".") if name else ""))
                if description:
                    fw("\n")
                    write_indented_lines("   ", fw, escape_rst(description) + ".")
                else:
                    fw("\n")
            else:
                if name:
                    fw("\n\n**{:s}**\n\n".format(name))
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
                raise Exception(
                    "Found RNA enum identifier that doesn't use the 'rna_enum_' prefix, found {!r}!".format(
                        key,
                    ))
            key_no_prefix = key.removeprefix("rna_enum_")
            fw("   {:s}\n".format(key_no_prefix))

        for key, enum_items in rna_enum_dict.items():
            key_no_prefix = key.removeprefix("rna_enum_")
            write_rst_enum_items(basepath_bpy_types_rna_enum, key, key_no_prefix, enum_items)
        fw("\n")


def write_rst_rna_types_with_custom_property_support(basepath):
    from bpy.types import bpy_struct_meta_idprop

    types_exclude = {
        "IDPropertyWrapPtr",  # Internal type, exclude form public docs.
    }
    types_found = []

    for ty_id in dir(bpy.types):
        if ty_id.startswith("_"):
            continue
        if ty_id in types_exclude:
            continue

        ty = getattr(bpy.types, ty_id)
        if not isinstance(ty, bpy_struct_meta_idprop):
            continue

        # Don't include every sub-type as it's very noisy and not helpful.
        if any((isinstance(ty_base, bpy_struct_meta_idprop) for ty_base in ty.__bases__)):
            continue

        types_found.append(ty_id)

    types_found.sort()

    with open(os.path.join(basepath, "bpy_types_custom_properties.rst"), "w", encoding="utf-8") as fh:
        fw = fh.write

        fw(".. _bpy_types-custom_properties:\n\n")

        fw(title_string("Types with Custom Property Support", "="))
        fw("\n")
        fw("The following types (and their sub-types) have custom-property access.\n\n")

        fw("For examples on using custom properties see the quick-start section on\n")
        fw(":ref:`info_quickstart-custom_properties`.\n")

        fw("\n")
        for ty_id in types_found:
            fw("- :class:`bpy.types.{:s}`\n".format(ty_id))


def write_rst_importable_modules(basepath):
    """
    Write the RST files of importable modules.
    """
    importable_modules = {
        # Python_modules.
        "bpy.path": "Path Utilities",
        "bpy.utils": "Utilities",
        "bpy_extras": "Extra Utilities",
        "gpu_extras": "GPU Utilities",

        # C_modules.
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
        "bmesh.ops",  # Generated by `rst_from_bmesh_opdefines.py`.

        # Includes.
        "include__bmesh",
    ]

    for mod_name in handwritten_modules:
        if mod_name not in EXCLUDE_MODULES:
            # Copy2 keeps time/date stamps.
            shutil.copy2(os.path.join(RST_DIR, "{:s}.rst".format(mod_name)), basepath)

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


def copy_sphinx_files(basepath):
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

    shutil.copy2(os.path.join(SCRIPT_DIR, "conf.py"), basepath, )


def format_config(basepath):
    """
    Updates ``conf.py`` with context information from Blender.
    """
    from string import Template

    # Ensure the string literals can contain any characters by closing the surrounding quotes
    # and declare a separate literal via `repr()`.
    def declare_in_quotes(string):
        return "\" {!r} \"".format(string)

    substitutions = {
        "BLENDER_VERSION_STRING": declare_in_quotes(BLENDER_VERSION_STRING),
        "BLENDER_VERSION_DOTS": declare_in_quotes(BLENDER_VERSION_DOTS),
        "BLENDER_REVISION_TIMESTAMP": declare_in_quotes(str(BLENDER_REVISION_TIMESTAMP)),
        "BLENDER_REVISION": declare_in_quotes(BLENDER_REVISION),
    }

    filepath = os.path.join(basepath, "conf.py")

    # Read the template string from the template file.
    with open(filepath, 'r', encoding="utf-8") as fh:
        template_file = fh.read()

    with open(filepath, 'w', encoding="utf-8") as fh:
        fh.write(Template(template_file).substitute(substitutions))


def rna2sphinx(basepath):
    # Main page.
    write_rst_index(basepath)

    # Context.
    if "bpy.context" not in EXCLUDE_MODULES:
        pycontext2sphinx(basepath)

    # Internal modules.
    write_rst_bpy(basepath)                 # `bpy`, disabled by default
    write_rst_types_index(basepath)         # `bpy.types`.
    write_rst_ops_index(basepath)           # `bpy.ops`.
    write_rst_msgbus(basepath)              # `bpy.msgbus`.
    write_rst_geometry_set(basepath)        # `bpy.types.GeometrySet`.
    write_rst_inline_shader_nodes(basepath)  # `bpy.types.InlineShaderNodes`.
    pyrna2sphinx(basepath)                  # `bpy.types.*` & `bpy.ops.*`.
    write_rst_data(basepath)                # `bpy.data`.
    write_rst_importable_modules(basepath)

    # `bpy_types_enum_items/*` (referenced from `bpy.types`).
    if USE_SHARED_RNA_ENUM_ITEMS_STATIC:
        write_rst_enum_items_and_index(basepath)

    if USE_RNA_TYPES_WITH_CUSTOM_PROPERTY_INDEX:
        write_rst_rna_types_with_custom_property_support(basepath)

    # Copy the other RST files.
    copy_handwritten_rsts(basepath)

    # Copy source files referenced.
    copy_handwritten_extra(basepath)


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

    # Freshen with new files.
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

        # Using a `FileHandler` seems to disable the `stdout`, so we add a `StreamHandler`.
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

    # Start from a clean directory every time.
    if os.path.exists(SPHINX_IN_TMP):
        shutil.rmtree(SPHINX_IN_TMP, True)

    try:
        os.mkdir(SPHINX_IN_TMP)
    except Exception:
        pass

    # Copy extra files needed for theme.
    copy_sphinx_files(SPHINX_IN_TMP)

    # Write information needed for `conf.py`.
    format_config(SPHINX_IN_TMP)

    # Dump the API in RST files.
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

    # Eventually, build the HTML docs.
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

        with open(sphinx_make_pdf_log, "w", encoding="utf-8") as fh:
            subprocess.call(SPHINX_MAKE_PDF, stdout=fh)

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

    return 0


if __name__ == '__main__':
    sys.exit(main())
