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

import argparse
import sys
import inspect
import shutil
import logging
import warnings

from collections.abc import (
    Callable,
    Iterator,
    Sequence,
)
from typing import (
    Final,
    NamedTuple,
    Protocol,
)

from pathlib import Path
from textwrap import indent

try:
    import bpy  # type: ignore[import-not-found]  # Blender module.
except ImportError:
    print("\nERROR: this script must run from inside Blender")
    print(__doc__)
    sys.exit()

import _rna_info as rna_info  # type: ignore[import-not-found]  # Blender module.


# ----------------------------------------------------------------------------
# Type Info

# Alias for a file's `.write` method, used as `fw(...)`.
WriteFn = Callable[[str], int]


# ----------------------------------------------------------------------------
# Inline stubs for `bpy` and `_rna_info`
#
# These types have no runtime stubs available,
# they could be important but the code depends heavily on `bpy`, so define stubs here.

class stub:
    """Namespace holding inline stubs for `bpy` and `_rna_info` types."""

    class RnaEnumItem(Protocol):
        identifier: str
        name: str
        description: str

        def as_pointer(self) -> int: ...

    # Tuple form: (note, current_version, removal_version)
    RnaDeprecated = tuple[str, tuple[int, int, int], tuple[int, int, int]]

    class InfoPropertyRNA(Protocol):
        identifier: str
        name: str
        description: str
        type: str
        fixed_type: "stub.InfoStructRNA | None"
        srna: "stub.InfoStructRNA | None"
        # Tuples of `(identifier, name, description)`.
        enum_items: Sequence[tuple[str, str, str]]
        enum_pointer: int | None
        deprecated: "stub.RnaDeprecated | None"
        is_required: bool

        def get_arg_default(self, force: bool = ...) -> str: ...

        def get_type_description(
            self,
            *,
            as_arg: bool = ...,
            as_ret: bool = ...,
            class_fmt: str = ...,
            mathutils_fmt: str = ...,
            literal_fmt: str = ...,
            collection_id: str = ...,
            enum_descr_override: str | None = ...,
        ) -> tuple[str, list[str]]: ...

    class InfoFunctionRNA(Protocol):
        identifier: str
        description: str
        args: "Sequence[stub.InfoPropertyRNA]"
        return_values: "Sequence[stub.InfoPropertyRNA]"
        is_classmethod: bool

    class InfoOperatorRNA(Protocol):
        identifier: str
        module_name: str
        func_name: str
        description: str
        args: "Sequence[stub.InfoPropertyRNA]"

        def get_location(self) -> tuple[str | None, int | None]: ...

    class InfoStructRNA(Protocol):
        identifier: str
        module_name: str
        base: "stub.InfoStructRNA | None"
        description: str
        properties: "Sequence[stub.InfoPropertyRNA]"
        functions: "Sequence[stub.InfoFunctionRNA]"
        references: Sequence[str]
        py_class: type

        def get_bases(self) -> "list[stub.InfoStructRNA]": ...

        def get_py_properties(self) -> "list[tuple[str, stub.PyProperty]]": ...

        def get_py_c_properties_getset(self) -> list[tuple[str, object]]: ...

        def get_py_functions(self) -> list[tuple[str, Callable[..., object]]]: ...

        def get_py_c_functions(self) -> list[tuple[str, Callable[..., object]]]: ...

        def is_operator_properties(self) -> bool: ...

    class PyProperty(Protocol):
        fset: Callable[..., object] | None

    # Result of `_rna_info.BuildRNAInfo()`: (structs, funcs, ops, props).
    # Keys are `(parent_id, identifier)` tuples, see `_GetInfoRNA` in `_rna_info.py`.
    RnaInfo = tuple[
        "dict[tuple[str, str], stub.InfoStructRNA]",
        "dict[tuple[str, str], stub.InfoFunctionRNA]",
        "dict[tuple[str, str], stub.InfoOperatorRNA]",
        "dict[tuple[str, str], stub.InfoPropertyRNA]",
    ]


def rna_info_BuildRNAInfo_cache() -> stub.RnaInfo:
    ret: stub.RnaInfo | None = rna_info_BuildRNAInfo_cache.ret  # type: ignore[attr-defined]
    if ret is None:
        ret = rna_info.BuildRNAInfo()
        rna_info_BuildRNAInfo_cache.ret = ret  # type: ignore[attr-defined]
    return ret


rna_info_BuildRNAInfo_cache.ret = None  # type: ignore[attr-defined]
# --- end rna_info cache

SCRIPT_DIR = Path(__file__).resolve().parent

# ----------------------------------------------------------------------------
# Global State


class Global(NamedTuple):
    """Static configuration assembled from command-line arguments and Blender state."""

    # Command line arguments.
    output_dir: Path
    partial: str
    full_rebuild: bool
    bpy: bool
    changelog: bool
    api_dump_index_path: Path | None
    sphinx_build: bool
    sphinx_build_pdf: bool
    pack_reference: bool
    log: bool

    # Derived from `partial` + Blender's available modules.
    exclude_modules: frozenset[str]
    filter_bpy_ops: tuple[str, ...] | None
    filter_bpy_types: tuple[str, ...] | None
    exclude_info_docs: bool

    # Derived from `bpy.app`.
    blender_revision: str
    blender_revision_timestamp: int
    blender_version_string: str
    blender_version_dots: str
    blender_version_path: str

    # Deployable artifact names and paths (used when `pack_reference` is set).
    output_base_name: str
    output_base_path: Path
    output_filename_pdf: str
    output_filename_zip: str

    # Derived from `output_dir`.
    sphinx_in: Path
    sphinx_in_tmp: Path
    sphinx_out: Path
    sphinx_out_pdf: Path | None  # Set only when `sphinx_build_pdf` is True.

    # Sphinx command lines (only populated when the matching `sphinx_build*` flag is set).
    sphinx_build_cmd: tuple[str | Path, ...]
    sphinx_build_pdf_cmd: tuple[str | Path, ...]
    sphinx_make_pdf_cmd: tuple[str | Path, ...]

    # Sphinx log paths (set only when `log` is True alongside the matching `sphinx_build*` flag).
    sphinx_build_log: Path | None
    sphinx_build_pdf_log: Path | None
    sphinx_make_pdf_log: Path | None


# Type-only declaration; bound by `main()`.
GLOBAL: Global


# ----------------------------------------------------------------------------
# Mutable State

class State:
    # Non-fatal issues (e.g. C-API `PyGetSetDef` doc-string problems) increment
    # this counter. `main()` returns non-zero when it ends up greater than zero.
    error_count: int = 0

    # Set of example identifiers referenced during RST generation.
    # Compared against `EXAMPLE_SET` at the end of `main()` to report unused examples.
    example_set_used: set[str] = set()

    @staticmethod
    def reset() -> None:
        State.error_count = 0
        State.example_set_used = set()


# For now, ignore add-ons and internal sub-classes of `bpy.types.PropertyGroup`.
#
# Besides disabling this line, the main change will be to add a
# `toctree` to `write_rst_index` which contains the generated RST files.
# This `toctree` can be generated automatically.
#
# See: D6261 for reference.
USE_ONLY_BUILTIN_RNA_TYPES: Final = True

# Write a page for each static enum defined in:
# `source/blender/makesrna/RNA_enum_items.hh` so the enums can be linked to instead of being expanded everywhere.
USE_SHARED_RNA_ENUM_ITEMS_STATIC: Final = True

# Generate a list of types which support custom properties.
# This isn't listed anywhere, it's just linked to.
USE_RNA_TYPES_WITH_CUSTOM_PROPERTY_INDEX: Final = True

# Write additional RST so `sphinx_stub_gen.py` can produce mypy-compatible stubs
# (dunder methods, valid default reprs).
# Disable to fall back to the pre-stub-gen baseline RST.
USE_STUB_GEN: Final = True

# Other types are assumed to be `bpy.types.*`.
PRIMITIVE_TYPE_NAMES = {"bool", "bytearray", "bytes", "dict", "float", "int", "list", "set", "str", "tuple"}

if USE_SHARED_RNA_ENUM_ITEMS_STATIC:
    from _bpy import rna_enum_items_static  # type: ignore[import-not-found]
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


def handle_args(argv: Sequence[str]) -> argparse.Namespace:
    """
    Parse the arguments passed in ``argv``.

    When invoked from Blender, callers should pass the slice of ``sys.argv``
    after ``"--"`` (Blender ignores everything after ``--`` itself).
    """
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
        type=Path,
        default=None,
        help="Path to the API dump index JSON file "
        "(required when `--api-changelog-generate` is True)",
        required=False,
    )

    parser.add_argument(
        "-o", "--output",
        dest="output_dir",
        type=Path,
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
        "sphinx-build <sphinx-in> <sphinx-out>\n"
        "(default=False; does not depend on -P)",
        required=False,
    )

    parser.add_argument(
        "-P", "--sphinx-build-pdf",
        dest="sphinx_build_pdf",
        default=False,
        action='store_true',
        help="Build the pdf by running:\n"
        "sphinx-build -b latex <sphinx-in> <sphinx-out-pdf>\n"
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

    return parser.parse_args(argv)


# ----------------------------------------------------------------------------
# BPY


BPY_LOGGER = logging.getLogger("bpy")
BPY_LOGGER.setLevel(logging.DEBUG)

# Source files we use, and need to copy to the OUTPUT_DIR
# to have working out-of-source builds.
# Paths are relative to `RST_DIR`; `..` parents are replaced by `__` in the RST destination,
# to avoid having to match Blender's source tree.
EXTRA_SOURCE_FILES = (
    Path("../../../scripts/templates_py/bmesh_simple.py"),
    Path("../../../scripts/templates_py/Gizmo/operator.py"),
    Path("../../../scripts/templates_py/Gizmo/operator_target.py"),
    Path("../../../scripts/templates_py/Gizmo/simple_3d.py"),
    Path("../../../scripts/templates_py/Operator/simple.py"),
    Path("../../../scripts/templates_py/UI/panel_simple.py"),
    Path("../../../scripts/templates_py/UI/previews_custom_icon.py"),
    Path("../examples/bmesh.ops.1.py"),
    Path("../examples/bpy.app.translations.0.py"),
)


# Examples.
EXAMPLES_DIR = (SCRIPT_DIR / "examples").resolve()
EXAMPLE_SET = {p.stem for p in EXAMPLES_DIR.iterdir() if p.suffix == ".py"}

# RST files directory.
RST_DIR = (SCRIPT_DIR / "rst").resolve()

# Extra info, not api reference docs stored in `./rst/info_*`.
# Pairs of (file, description), the title makes from the RST files are displayed before the description.
INFO_DOCS = (
    (Path("info_quickstart.rst"),
     "New to Blender or scripting and want to get your feet wet?"),
    (Path("info_overview.rst"),
     "A more complete explanation of Python integration."),
    (Path("info_api_reference.rst"),
     "Examples of how to use the API reference docs."),
    (Path("info_best_practice.rst"),
     "Conventions to follow for writing good scripts."),
    (Path("info_tips_and_tricks.rst"),
     "Hints to help you while writing scripts for Blender."),
    (Path("info_gotcha.rst"),
     "Some of the problems you may encounter when writing scripts."),
    (Path("info_advanced.rst"),
     "Topics which may not be required for typical usage."),
    (Path("change_log.rst"),
     "List of changes since last Blender release"),
    (Path("info_contributing.rst"),
     "Guide for contributing to Blender's Python API documentation."),
)
# Referenced indirectly.
INFO_DOCS_OTHER = (
    # Included by: `info_advanced.rst`.
    Path("info_advanced_blender_as_bpy.rst"),
    # Included by: `info_gotcha.rst`.
    Path("info_gotchas_crashes.rst"),
    Path("info_gotchas_threading.rst"),
    Path("info_gotchas_internal_data_and_python_objects.rst"),
    Path("info_gotchas_operators.rst"),
    Path("info_gotchas_meshes.rst"),
    Path("info_gotchas_armatures_and_bones.rst"),
    Path("info_gotchas_file_paths_and_encoding.rst"),
)

# Hide the actual TOC, use a separate list that links to the items.
# This is done so a short description can be included with each link.
USE_INFO_DOCS_FANCY_INDEX: Final = True

# Only support for properties at the moment.
RNA_EXCLUDE: dict[str, set[str]] = {
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

# Underscore-prefixed attributes to include in documentation
# (these would otherwise be skipped). Maps module name to the identifiers.
# NOTE: every inclusion must justify itself as this is something we should typically avoid.
PRIVATE_ATTR_INCLUDE = {
    "bpy.props": {
        # Without this type documented, the correct type can't be referenced for `bpy.props` definition.
        "_PropertyDeferred",
    },
}


def is_attr_private(module_name: str, attribute: str) -> bool:
    """Check if an attribute should be skipped as private (underscore-prefixed)."""
    if not attribute.startswith("_"):
        return False
    exceptions = PRIVATE_ATTR_INCLUDE.get(module_name)
    if exceptions is None:
        return True
    if attribute not in exceptions:
        return True
    return False


MODULE_GROUPING: dict[str, tuple[str | tuple[str, str], ...]] = {
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


def global_create(argv: Sequence[str]) -> Global:
    """
    Build a :class:`Global` from CLI arguments and the running Blender, and return it.
    """
    args = handle_args(argv)

    # Build the partial-derived configuration into locals.
    filter_bpy_ops: tuple[str, ...] | None
    filter_bpy_types: tuple[str, ...] | None

    # Switch for quick testing so doc-builds don't take so long.
    if not args.partial:
        # Full build.
        filter_bpy_ops = None
        filter_bpy_types = None
        exclude_info_docs = False
        exclude_modules: list[str] = []
    else:
        # Can manually edit this too:
        # filter_bpy_ops = ("import.scene", )  # allow
        # filter_bpy_types = ("bpy_struct", "Operator", "ID")  # allow
        exclude_info_docs = True
        exclude_modules = [
            "aud",
            "blf",
            "blf.types",
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
            "idprop",
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

        # TODO: support `bpy.ops` and `bpy.types` filtering.
        import fnmatch
        exclude_modules = [m for m in exclude_modules if not fnmatch.fnmatchcase(m, args.partial)]

        # Special support for `bpy.types.*`.
        filter_bpy_ops = tuple([m[8:] for m in args.partial.split(":") if m.startswith("bpy.ops.")])
        if filter_bpy_ops:
            exclude_modules.remove("bpy.ops")

        filter_bpy_types = tuple([m[10:] for m in args.partial.split(":") if m.startswith("bpy.types.")])
        if filter_bpy_types:
            exclude_modules.remove("bpy.types")

        exclude_info_docs = (not fnmatch.fnmatchcase("info", args.partial))

        BPY_LOGGER.debug(
            "Partial Doc Build, Skipping: %s\n",
            "\n                             ".join(sorted(exclude_modules)))

    try:
        __import__("aud")
    except ImportError:
        BPY_LOGGER.debug("Warning: Built without \"aud\" module, docs incomplete...")
        exclude_modules.append("aud")

    try:
        __import__("freestyle")
    except ImportError:
        BPY_LOGGER.debug("Warning: Built without \"freestyle\" module, docs incomplete...")
        exclude_modules.extend([
            "freestyle",
            "freestyle.chainingiterators",
            "freestyle.functions",
            "freestyle.predicates",
            "freestyle.shaders",
            "freestyle.types",
            "freestyle.utils",
        ])

    # Converting bytes to strings, due to #30154.
    blender_revision = str(bpy.app.build_hash, 'utf_8')
    blender_revision_timestamp = bpy.app.build_commit_timestamp

    # '2.83.0 Beta' or '2.83.0' or '2.83.1'
    blender_version_string = bpy.app.version_string
    blender_version_dots = "{:d}.{:d}".format(bpy.app.version[0], bpy.app.version[1])

    # Example: `2_83`.
    blender_version_path = "{:d}_{:d}".format(bpy.app.version[0], bpy.app.version[1])

    output_base_name = "blender_python_reference_{:s}".format(blender_version_path)
    output_base_path = args.output_dir / output_base_name
    output_filename_pdf = "{:s}.pdf".format(output_base_name)
    output_filename_zip = "{:s}.zip".format(output_base_name)

    sphinx_in = args.output_dir / "sphinx-in"
    sphinx_in_tmp = args.output_dir / "sphinx-in-tmp"
    sphinx_out = args.output_dir / "sphinx-out"

    # HTML build.
    sphinx_out_pdf: Path | None = None
    sphinx_build_cmd: tuple[str | Path, ...] = ()
    sphinx_build_pdf_cmd: tuple[str | Path, ...] = ()
    sphinx_make_pdf_cmd: tuple[str | Path, ...] = ()
    sphinx_build_log: Path | None = None
    sphinx_build_pdf_log: Path | None = None
    sphinx_make_pdf_log: Path | None = None

    if args.sphinx_build:
        if args.log:
            sphinx_build_log = args.output_dir / ".sphinx-build.log"
            sphinx_build_cmd = (
                "sphinx-build",
                "-w", sphinx_build_log,
                sphinx_in, sphinx_out,
            )
        else:
            sphinx_build_cmd = ("sphinx-build", sphinx_in, sphinx_out)

    # PDF build.
    if args.sphinx_build_pdf:
        sphinx_out_pdf = args.output_dir / "sphinx-out_pdf"
        sphinx_make_pdf_cmd = ("make", "-C", sphinx_out_pdf)

        if args.log:
            sphinx_build_pdf_log = args.output_dir / ".sphinx-build_pdf.log"
            sphinx_make_pdf_log = args.output_dir / ".latex_make.log"
            sphinx_build_pdf_cmd = (
                "sphinx-build", "-b", "latex",
                "-w", sphinx_build_pdf_log,
                sphinx_in, sphinx_out_pdf,
            )
        else:
            sphinx_build_pdf_cmd = (
                "sphinx-build", "-b", "latex",
                sphinx_in, sphinx_out_pdf,
            )

    return Global(
        **vars(args),
        exclude_modules=frozenset(exclude_modules),
        filter_bpy_ops=filter_bpy_ops,
        filter_bpy_types=filter_bpy_types,
        exclude_info_docs=exclude_info_docs,
        blender_revision=blender_revision,
        blender_revision_timestamp=blender_revision_timestamp,
        blender_version_string=blender_version_string,
        blender_version_dots=blender_version_dots,
        blender_version_path=blender_version_path,
        output_base_name=output_base_name,
        output_base_path=output_base_path,
        output_filename_pdf=output_filename_pdf,
        output_filename_zip=output_filename_zip,
        sphinx_in=sphinx_in,
        sphinx_in_tmp=sphinx_in_tmp,
        sphinx_out=sphinx_out,
        sphinx_out_pdf=sphinx_out_pdf,
        sphinx_build_cmd=sphinx_build_cmd,
        sphinx_build_pdf_cmd=sphinx_build_pdf_cmd,
        sphinx_make_pdf_cmd=sphinx_make_pdf_cmd,
        sphinx_build_log=sphinx_build_log,
        sphinx_build_pdf_log=sphinx_build_pdf_log,
        sphinx_make_pdf_log=sphinx_make_pdf_log,
    )


# ----------------------------------------------------------------------------
# Change Log Generation

def generate_changelog() -> None:
    import importlib.util
    spec = importlib.util.spec_from_file_location(
        "sphinx_changelog_gen",
        SCRIPT_DIR / "sphinx_changelog_gen.py",
    )
    assert spec is not None and spec.loader is not None
    sphinx_changelog_gen = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(sphinx_changelog_gen)
    assert GLOBAL.api_dump_index_path is not None

    API_DUMP_INDEX_FILEPATH = GLOBAL.api_dump_index_path
    API_DUMP_ROOT = API_DUMP_INDEX_FILEPATH.parent
    API_DUMP_FILEPATH = API_DUMP_ROOT / GLOBAL.blender_version_dots / "api_dump.json"
    API_CHANGELOG_FILEPATH = GLOBAL.sphinx_in_tmp / "change_log.rst"

    sphinx_changelog_gen.main((
        "--",
        "--indexpath",
        str(API_DUMP_INDEX_FILEPATH),
        "dump",
        "--filepath-out",
        str(API_DUMP_FILEPATH),
    ))

    sphinx_changelog_gen.main((
        "--",
        "--indexpath",
        str(API_DUMP_INDEX_FILEPATH),
        "changelog",
        "--filepath-out",
        str(API_CHANGELOG_FILEPATH),
    ))


# --------------------------------API DUMP--------------------------------------

# Unfortunately Python doesn't expose direct access to these types.
# Access them indirectly.
ClassMethodDescriptorType = type(dict.__dict__["fromkeys"])
MethodDescriptorType = type(dict.get)
GetSetDescriptorType = type(int.real)
StaticMethodType = type(staticmethod(lambda: None))
from types import (
    BuiltinFunctionType,
    BuiltinMethodType,
    MemberDescriptorType,
    MethodType,
    FunctionType,
)

# These are C-API defined types (accessible via `bpy.types`) that are not known to RNA.
# The RNA wrapping code in `bpy_rna.cc` uses these types when creating `PyObject`s
# for RNA data. They define methods Python developers use, but since they are not RNA
# structs they are not discovered automatically and must be documented explicitly.
#
# There are two separate hierarchies (see `bpy_rna.cc` for the definitive type hierarchy):
#
# - `bpy_struct`: base for all RNA struct instances.
# - `bpy_prop` -> `bpy_prop_collection` -> `bpy_prop_collection_idprop`:
#   used when RNA collection properties are accessed from Python.
#
USE_PYCAPI_TYPES: Final = True

_BPY_STRUCT_PYCAPI = "bpy_struct"
_BPY_PROP_PYCAPI = "bpy_prop"
_BPY_PROP_ARRAY_PYCAPI = "bpy_prop_array"
_BPY_PROP_COLLECTION_PYCAPI = "bpy_prop_collection"
_BPY_PROP_COLLECTION_IDPROP_PYCAPI = "bpy_prop_collection_idprop"
_BPY_FUNC_CAPI = "bpy_func"
_BPY_STRUCT_META_IDPROP_CAPI = "bpy_struct_meta_idprop"

# All core C-API defined types in `bpy.types` that back the RNA wrapping itself.
_BPY_TYPES_CORE_CAPI = frozenset((
    _BPY_STRUCT_PYCAPI,
    _BPY_PROP_PYCAPI,
    _BPY_PROP_ARRAY_PYCAPI,
    _BPY_PROP_COLLECTION_PYCAPI,
    _BPY_PROP_COLLECTION_IDPROP_PYCAPI,
    _BPY_FUNC_CAPI,
    _BPY_STRUCT_META_IDPROP_CAPI,
))

_BPY_PROP_COLLECTION_ID = ":class:`{:s}`".format(_BPY_PROP_COLLECTION_PYCAPI) if USE_PYCAPI_TYPES else "collection"

bpy_struct: type = bpy.types.bpy_struct
bpy_prop: type = bpy.types.bpy_prop
bpy_prop_array: type = bpy.types.bpy_prop_array
bpy_prop_collection: type = bpy.types.bpy_prop_collection
bpy_prop_collection_idprop: type = bpy.types.bpy_prop_collection_idprop


def import_value_from_module(module_name: str, import_name: str) -> object:
    ns: dict[str, object] = {}
    exec_str = "from {:s} import {:s} as value".format(module_name, import_name)
    exec(exec_str, ns, ns)
    return ns["value"]


def execfile(filepath: Path | str) -> None:
    filepath = str(filepath)
    global_namespace = {"__file__": filepath, "__name__": "__main__"}
    with open(filepath, encoding="utf-8") as file_handle:
        exec(compile(file_handle.read(), filepath, 'exec'), global_namespace)


ESCAPE_RST_TRANS = str.maketrans({
    "`": "\\`",
    "|": "\\|",
    "*": "\\*",
    "\\": "\\\\",
})


def escape_rst(text: str) -> str:
    """
    Escape plain text which may contain characters used by RST.
    """
    return text.translate(ESCAPE_RST_TRANS)


def is_struct_seq(value: object) -> bool:
    return isinstance(value, tuple) and type(value) != tuple and hasattr(value, "n_fields")


def undocumented_message(module_name: str, type_name: str | None, identifier: str) -> str:
    BPY_LOGGER.debug(
        "Undocumented: module %s, type: %s, id: %s is not documented",
        module_name, type_name, identifier,
    )

    return "Undocumented, consider `contributing <https://developer.blender.org/>`__."


def example_extract_docstring(filepath: Path | str) -> tuple[str, int, bool]:
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


def title_string(text: str, heading_char: str, double: bool = False) -> str:
    filler = len(text) * heading_char

    if double:
        return "{:s}\n{:s}\n{:s}\n\n".format(filler, text, filler)
    return "{:s}\n{:s}\n\n".format(text, filler)


def write_example_ref_impl(ident: str, fw: WriteFn, example_id: str, ext: str) -> None:
    # Extract the comment.
    # Forward slashes are required for the RST `.. literalinclude::` directive;
    # sphinx resolves it relative to the RST file's location at build time.
    filepath = "../examples/{:s}.{:s}".format(example_id, ext)
    filepath_full = EXAMPLES_DIR / "{:s}.{:s}".format(example_id, ext)

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
    State.example_set_used.add(example_id)


def write_example_ref(ident: str, fw: WriteFn, example_id: str, ext: str = "py") -> None:
    # Support for numbered files `bpy.types.Operator` -> `bpy.types.Operator.0.py`.
    i = 0
    while True:
        example_id_num = "{:s}.{:d}".format(example_id, i)
        if example_id_num in EXAMPLE_SET:
            write_example_ref_impl(ident, fw, example_id_num, ext)
        else:
            # Allow numbers to start at 0 or 1,
            # historically they started at 1, but now 0 is supported too.
            if i > 0:
                break
        i += 1


def write_indented_lines(ident: str, fn: WriteFn, text: str | None, strip: bool = True) -> None:
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


def pyfunc_owner_class(
        py_func: Callable[..., object],
        is_class: bool,
        struct: stub.InfoStructRNA | None,
) -> type | None:
    """
    Return the class ``py_func`` is accessed through, or None when undetermined.

    This is the binding class (``__self__`` for bound methods,
    the RNA struct's Python class for plain functions reached via an RNA struct),
    not necessarily the class that defines ``py_func`` - inherited methods return the subclass.
    """
    if type(py_func) == MethodType and isinstance(py_func.__self__, type):
        return py_func.__self__
    if is_class and struct is not None and type(py_func) == FunctionType:
        return struct.py_class
    return None


def repr_with_function_support(value: object) -> str:
    """
    Like ``repr()``, but return ``__name__`` for callables whose ``repr()``
    is not a valid Python expression (e.g. ``<built-in function print>`` -> ``print``).

    Returns the original ``repr()`` when ``__name__`` itself is not a valid
    identifier (e.g. lambdas have ``__name__ == "<lambda>"``); the caller's
    substitution then falls through as a no-op rather than writing a broken
    identifier into the signature.
    """
    r = repr(value)
    if r.startswith("<"):
        name: str | None = getattr(value, "__name__", None)
        if name is not None and name.isidentifier():
            return name
    return r


def pyfunc_is_inherited_method(py_class: type, py_func: Callable[..., object], identifier: str) -> bool:
    """
    Test if ``py_func`` on py_class is shadowed on ``py_class.__base__``.

    Only the immediate base is checked. Mix-in methods
    (e.g. ``_GenericUI.append`` surfaced as ``Menu.append``)
    survive because RNA bases come first by convention, so the mix-in is never ``__base__``.
    """
    assert isinstance(py_class, type)
    assert type(py_func) in (MethodType, FunctionType)
    base = py_class.__base__
    if base is None or base is object:
        return False
    base_attr = getattr(base, identifier, None)
    if base_attr is None:
        return False
    own_underlying = getattr(py_func, "__func__", py_func)
    base_underlying = getattr(base_attr, "__func__", base_attr)
    if base_underlying is own_underlying:
        return True
    if isinstance(base_attr, bpy.types.bpy_func):
        return True
    return False


def pyfunc2sphinx(
        ident: str,
        fw: WriteFn,
        module_name: str,
        type_name: str | None,
        identifier: str,
        py_func: Callable[..., object],
        *,
        struct: stub.InfoStructRNA | None,
        is_class: bool = True,
) -> None:
    """
    function or class method to sphinx
    """

    if (py_class := pyfunc_owner_class(py_func, is_class, struct)) is not None:
        # Skip RNA-backed methods - docs come from the RNA definition.
        # Including them would list every operator's `poll` example (and similar)
        # in the docs which isn't useful. Excluding all methods would over-reach
        # however, hiding utility methods defined in `_bpy_types.py`.
        bl_rna = getattr(py_class, "bl_rna", None)
        if bl_rna is not None and bl_rna.functions.get(identifier) is not None:
            return
        # Skip inherited methods - docs appear on the defining base.
        if pyfunc_is_inherited_method(py_class, py_func, identifier):
            return

    sig = inspect.signature(py_func)
    arg_str = str(sig)
    if USE_STUB_GEN:
        if "<" in arg_str:
            for p in sig.parameters.values():
                if p.default is not inspect.Parameter.empty:
                    bad_repr = repr(p.default)
                    if bad_repr != (fixed_repr := repr_with_function_support(p.default)):
                        arg_str = arg_str.replace(bad_repr, fixed_repr)

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
        assert type_name is not None
        write_example_ref(ident + "   ", fw, module_name + "." + type_name + "." + identifier)
    else:
        write_example_ref(ident + "   ", fw, module_name + "." + identifier)


def py_descr2sphinx(
        ident: str,
        fw: WriteFn,
        descr: object,
        module_name: str,
        type_name: str,
        identifier: str,
        is_class: bool,
) -> None:
    if identifier.startswith("_"):
        return

    doc = descr.__doc__

    if type(descr) == GetSetDescriptorType:
        # Exclude `aud`, ideally it should confirm to our naming convention, currently it doesn't.
        # TODO: resolve upstream.
        if module_name == "aud" or module_name.startswith("aud."):
            pass
        else:
            if not doc:
                print(
                    "C-API PyGetSetDef {:s}.{:s}.{:s} has empty/missing doc-string".format(
                        module_name, type_name, identifier,
                    ),
                    file=sys.stderr,
                )
                State.error_count += 1
            elif ":type:" not in doc:
                print(
                    "C-API PyGetSetDef {:s}.{:s}.{:s} doc-string is missing ':type:'".format(
                        module_name, type_name, identifier,
                    ),
                    file=sys.stderr,
                )
                State.error_count += 1

    if not doc:
        doc = undocumented_message(module_name, type_name, identifier)

    if type(descr) == GetSetDescriptorType:
        directive = "attribute" if is_class else "data"
        fw(ident + ".. {:s}:: {:s}\n\n".format(directive, identifier))
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


def py_c_func2sphinx(
        ident: str,
        fw: WriteFn,
        module_name: str,
        type_name: str | None,
        identifier: str,
        py_func: Callable[..., object],
        is_class: bool = True,
) -> None:
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
        assert type_name is not None
        write_example_ref(ident + "   ", fw, module_name + "." + type_name + "." + identifier)
    else:
        write_example_ref(ident + "   ", fw, module_name + "." + identifier)

    fw("\n")


def pyprop2sphinx(ident: str, fw: WriteFn, identifier: str, py_prop: stub.PyProperty) -> None:
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


def pymodule2sphinx(
        basepath: Path,
        module_name: str,
        module: object,
        title: str,
        module_all_extra: Sequence[str],
) -> None:
    import types
    attribute_set: set[str] = set()
    filepath = basepath / (module_name + ".rst")

    module_all = getattr(module, "__all__", None)
    module_dir = sorted(dir(module))

    if module_all:
        module_dir = module_all

    # TODO: currently only used for classes.
    # Grouping support.
    module_grouping = MODULE_GROUPING.get(module_name)

    def module_grouping_index(name: str) -> int:
        if module_grouping is not None:
            try:
                return module_grouping.index(name)
            except ValueError:
                pass
        return -1

    def module_grouping_heading(name: str) -> tuple[str, str]:
        if module_grouping is not None:
            i = module_grouping_index(name) - 1
            if i >= 0:
                item = module_grouping[i]
                if type(item) == tuple:
                    return item
        return "", ""

    def module_grouping_sort_key(name: str) -> int:
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
        submod_ls: list[tuple[str, object]] = []
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
            py_descr2sphinx("", fw, descr, module_name_split, type_name, key, is_class=False)
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
        py_descr2sphinx("", fw, descr, module_name, type_name, key, is_class=False)

        attribute_set.add(key)

    del key, descr, descr_sorted

    classes = []
    submodules = []

    # Use this list so we can sort by type.
    module_dir_value_type = []

    for attribute in module_dir:
        if is_attr_private(module_name, attribute):
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
            pyfunc2sphinx("", fw, module_name, None, attribute, value, struct=None, is_class=False)
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
            fw("   :type: {:s}\n\n".format(value_type.__name__))
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


class DunderParam(NamedTuple):
    """A documented parameter on a dunder method: RST type and description."""
    param_type: str
    description: str


class DunderOverload(NamedTuple):
    """A single ``@overload`` entry for a dunder: operand type and return type (RST)."""
    operand_type: str
    return_type: str


class DunderInfo(NamedTuple):
    """
    Everything we know about a dunder (protocol) method.

    Used both as a base entry (in ``DUNDER_METHODS``, all fields populated) and
    as a per-class partial override (in ``CLASS_OVERRIDES``, only set fields are
    applied via ``merge_dunder_info``). All fields are optional so overrides
    can specify just what they change.

    Field semantics:

    - ``signature``: bracketed parameter list literally inserted into the directive
      (e.g. ``"(other)"``, ``"()"``).
    - ``params``: maps each parameter name to its ``DunderParam``.
    - ``return_type``: RST rtype string, ``"self"`` for the enclosing class,
      or ``""`` to omit the ``:rtype:`` field entirely.
    - ``overloads``: when set, write one ``.. method::`` directive per entry
      (subsequent entries use ``:noindex:``); in stubs this becomes ``@overload``.
    """
    signature: str | None = None
    params: dict[str, DunderParam] | None = None
    return_type: str | None = None
    overloads: list[DunderOverload] | None = None


def merge_dunder_info(base: DunderInfo, override: DunderInfo | None) -> DunderInfo:
    """Apply *override* over *base*; ``None`` fields on the override inherit from base."""
    if override is None:
        return base
    return base._replace(**{
        field: value for field, value in override._asdict().items() if value is not None
    })


OTHER_OPERAND = {"other": DunderParam("Self", "The other operand.")}
DUNDER_METHODS: dict[str, DunderInfo] = {
    # Binary arithmetic operators.
    "__add__": DunderInfo("(other)", OTHER_OPERAND, "self"),
    "__radd__": DunderInfo("(other)", OTHER_OPERAND, "self"),
    "__sub__": DunderInfo("(other)", OTHER_OPERAND, "self"),
    "__rsub__": DunderInfo("(other)", OTHER_OPERAND, "self"),
    "__mul__": DunderInfo("(other)", OTHER_OPERAND, "self"),
    "__rmul__": DunderInfo("(other)", OTHER_OPERAND, "self"),
    "__truediv__": DunderInfo("(other)", OTHER_OPERAND, "self"),
    "__rtruediv__": DunderInfo("(other)", OTHER_OPERAND, "self"),
    "__matmul__": DunderInfo("(other)", OTHER_OPERAND, "self"),
    "__rmatmul__": DunderInfo("(other)", OTHER_OPERAND, "self"),
    # Unary operators.
    "__neg__": DunderInfo("()", {}, "self"),
    "__pos__": DunderInfo("()", {}, "self"),
    "__invert__": DunderInfo("()", {}, "self"),
    # In-place arithmetic operators.
    "__iadd__": DunderInfo("(other)", OTHER_OPERAND, "self"),
    "__isub__": DunderInfo("(other)", OTHER_OPERAND, "self"),
    "__imul__": DunderInfo("(other)", OTHER_OPERAND, "self"),
    "__itruediv__": DunderInfo("(other)", OTHER_OPERAND, "self"),
    "__imatmul__": DunderInfo("(other)", OTHER_OPERAND, "self"),
    # Comparison and hashing.
    # `__eq__` / `__ne__` take `object` (not `Self`) per Python convention -
    # `a == b` is valid for any pair of types, even if it just returns False.
    "__eq__": DunderInfo(
        "(other)", {"other": DunderParam("object", "The other operand.")}, "bool"),
    "__ne__": DunderInfo(
        "(other)", {"other": DunderParam("object", "The other operand.")}, "bool"),
    "__lt__": DunderInfo("(other)", OTHER_OPERAND, "bool"),
    "__le__": DunderInfo("(other)", OTHER_OPERAND, "bool"),
    "__gt__": DunderInfo("(other)", OTHER_OPERAND, "bool"),
    "__ge__": DunderInfo("(other)", OTHER_OPERAND, "bool"),
    "__hash__": DunderInfo("()", {}, "int"),
    # Container protocol.
    "__len__": DunderInfo("()", {}, "int"),
    "__getitem__": DunderInfo("(key)", {"key": DunderParam("int", "Index or key.")}, "float"),
    "__setitem__": DunderInfo("(key, value)", {
        "key": DunderParam("int", "Index or key."),
        "value": DunderParam("object", "Value to assign."),
    }, ""),
    "__contains__": DunderInfo(
        "(item)", {"item": DunderParam("object", "Item to test for membership.")}, "bool"),
    # String representation.
    "__repr__": DunderInfo("()", {}, "str"),
    "__str__": DunderInfo("()", {}, "str"),
    # Iteration.
    "__iter__": DunderInfo("()", {}, "self"),
    "__next__": DunderInfo("()", {}, "Any"),
    # Context manager protocol.
    "__enter__": DunderInfo("()", {}, "self"),
    "__exit__": DunderInfo("(exc_type, exc_value, traceback)", {
        "exc_type": DunderParam("type | None", "Exception type, or ``None``."),
        "exc_value": DunderParam("BaseException | None", "Exception instance, or ``None``."),
        "traceback": DunderParam("BaseException | None", "Traceback object, or ``None``."),
    }, "bool"),
}
del OTHER_OPERAND


class ClassOverrides(NamedTuple):
    """
    Per-class customizations applied during RST generation. All fields optional.

    Add new override categories as fields here rather than introducing
    additional purpose-specific dicts.
    """
    # Per-dunder partial overrides merged onto `DUNDER_METHODS` entries.
    # Each value's None fields inherit from the base entry.
    dunders: dict[str, DunderInfo] | None = None
    # Dunder names to skip even when the slot is present on the type.
    # Use for slots that exist in the type's protocol struct but always raise
    # (e.g. `Vector.__imatmul__` exists but unconditionally errors).
    excluded_dunders: set[str] | None = None


CLASS_OVERRIDES: dict[tuple[str, str], ClassOverrides] = {
    ("mathutils", "Matrix"): ClassOverrides(
        dunders={
            "__getitem__": DunderInfo(return_type=":class:`Vector`"),
            # `Matrix_mul` accepts element-wise (Matrix) or scalar (float).
            "__mul__": DunderInfo(overloads=[
                DunderOverload(":class:`Matrix`", ":class:`Matrix`"),
                DunderOverload("float", ":class:`Matrix`"),
            ]),
            # `Matrix_imul` accepts element-wise (Matrix) or scalar (float).
            "__imul__": DunderInfo(overloads=[
                DunderOverload(":class:`Matrix`", ":class:`Matrix`"),
                DunderOverload("float", ":class:`Matrix`"),
            ]),
            # `__rmul__` only fires for `float * mat` (mat * mat goes through `__mul__`).
            "__rmul__": DunderInfo(params={"other": DunderParam("float", "Scalar.")}),
            # Matrix multiplication: see `Matrix_matmul` in `mathutils_Matrix.cc`.
            "__matmul__": DunderInfo(overloads=[
                DunderOverload(":class:`Matrix`", ":class:`Matrix`"),
                DunderOverload(":class:`Vector`", ":class:`Vector`"),
            ]),
        },
        # Ordering ops are NotImplemented; reverse arithmetic ops error on
        # non-Matrix LHS. (`__rmul__` is kept: `Matrix_mul` handles `float * mat`.)
        excluded_dunders={
            "__lt__", "__le__", "__gt__", "__ge__",
            "__radd__", "__rsub__", "__rmatmul__",
        },
    ),
    ("mathutils", "Color"): ClassOverrides(
        dunders={
            # `Color_mul` only accepts a scalar (col * col is unsupported).
            "__mul__": DunderInfo(params={"other": DunderParam("float", "Scalar.")}),
            # `Color_imul` only accepts a scalar.
            "__imul__": DunderInfo(params={"other": DunderParam("float", "Scalar.")}),
            # `__rmul__` only fires for `float * col` (col * col goes through `__mul__`).
            "__rmul__": DunderInfo(params={"other": DunderParam("float", "Scalar.")}),
            # `Color_div` only accepts a scalar divisor.
            "__truediv__": DunderInfo(params={"other": DunderParam("float", "Scalar divisor.")}),
            # `Color_idiv` only accepts a scalar divisor.
            "__itruediv__": DunderInfo(params={"other": DunderParam("float", "Scalar divisor.")}),
        },
        # Ordering ops are NotImplemented; reverse arithmetic ops error on
        # non-Color LHS. (`__rmul__` is kept: `Color_mul` handles `float * col`.)
        excluded_dunders={
            "__lt__", "__le__", "__gt__", "__ge__",
            "__radd__", "__rsub__", "__rtruediv__",
        },
    ),
    ("mathutils", "Euler"): ClassOverrides(
        # `Euler_richcmpr` returns `NotImplemented` for ordering ops; only
        # `__eq__` / `__ne__` are real. Ordering rotations isn't meaningful.
        excluded_dunders={"__lt__", "__le__", "__gt__", "__ge__"},
    ),
    ("mathutils", "Vector"): ClassOverrides(
        dunders={
            # `Vector_mul` accepts element-wise (Vector) or scalar (float).
            "__mul__": DunderInfo(overloads=[
                DunderOverload(":class:`Vector`", ":class:`Vector`"),
                DunderOverload("float", ":class:`Vector`"),
            ]),
            # `Vector_imul` accepts element-wise (Vector) or scalar (float).
            "__imul__": DunderInfo(overloads=[
                DunderOverload(":class:`Vector`", ":class:`Vector`"),
                DunderOverload("float", ":class:`Vector`"),
            ]),
            # `__rmul__` only fires for `float * vec` (vec * vec goes through `__mul__`).
            "__rmul__": DunderInfo(params={"other": DunderParam("float", "Scalar.")}),
            # `Vector_div` / `Vector_idiv` only accept a scalar divisor.
            "__truediv__": DunderInfo(
                params={"other": DunderParam("float", "Scalar divisor.")},
            ),
            "__itruediv__": DunderInfo(
                params={"other": DunderParam("float", "Scalar divisor.")},
            ),
            # See `Vector_matmul` in `mathutils_Vector.cc`. Vector @ Vector is the dot product.
            "__matmul__": DunderInfo(overloads=[
                DunderOverload(":class:`Vector`", "float"),
                DunderOverload(":class:`Matrix`", ":class:`Vector`"),
            ]),
        },
        # Reverse ops that error on non-Vector LHS. (`__rmul__` is kept:
        # `Vector_mul` handles `float * vec`.)
        excluded_dunders={
            "__radd__",
            "__rsub__",
            "__rtruediv__",
            "__rmatmul__",
            # `Vector_imatmul` exists in `nb_inplace_matrix_multiply` but always
            # raises TypeError, so `vec @= other` is unsupported in practice.
            "__imatmul__",
        },
    ),
    ("mathutils", "Quaternion"): ClassOverrides(
        dunders={
            # `Quaternion_mul` accepts element-wise (Quaternion) or scalar (float).
            "__mul__": DunderInfo(overloads=[
                DunderOverload(":class:`Quaternion`", ":class:`Quaternion`"),
                DunderOverload("float", ":class:`Quaternion`"),
            ]),
            # `Quaternion_imul` accepts element-wise (Quaternion) or scalar (float).
            "__imul__": DunderInfo(overloads=[
                DunderOverload(":class:`Quaternion`", ":class:`Quaternion`"),
                DunderOverload("float", ":class:`Quaternion`"),
            ]),
            # `__rmul__` only fires for `float * quat` (quat * quat goes through `__mul__`).
            "__rmul__": DunderInfo(params={"other": DunderParam("float", "Scalar.")}),
            # See `Quaternion_matmul` in `mathutils_Quaternion.cc`.
            "__matmul__": DunderInfo(overloads=[
                DunderOverload(":class:`Quaternion`", ":class:`Quaternion`"),
                DunderOverload(":class:`Vector`", ":class:`Vector`"),
            ]),
        },
        # Ordering ops are NotImplemented; reverse arithmetic ops error on
        # non-Quaternion LHS. (`__rmul__` is kept: `Quaternion_mul` handles
        # `float * quat`.)
        excluded_dunders={
            "__lt__", "__le__", "__gt__", "__ge__",
            "__radd__", "__rsub__", "__rmatmul__",
        },
    ),
    # bpy_prop_collection returns bpy_struct elements, not float.
    ("bpy.types", "bpy_prop_collection"): ClassOverrides(
        dunders={
            "__getitem__": DunderInfo(return_type=":class:`bpy_struct`"),
            "__iter__": DunderInfo(return_type="typing.Iterator[:class:`bpy_struct`]"),
        },
    ),
    # `BPy_IDArray_GetItem` / `BPy_IDArray_SetItem` operate on values matching the
    # array's `subtype` (float / double -> float, int -> int, boolean -> bool).
    # A single array is typecode-homogeneous, so a slice yields a list of one
    # of those types - matching `to_list()`'s rtype.
    ("idprop.types", "IDPropertyArray"): ClassOverrides(
        dunders={
            "__getitem__": DunderInfo(overloads=[
                DunderOverload("int", "float | int | bool"),
                DunderOverload("slice", "list[float] | list[int] | list[bool]"),
            ]),
            "__setitem__": DunderInfo(
                params={
                    "key": DunderParam("int", "Index."),
                    "value": DunderParam("float | int | bool", "Value to assign."),
                },
            ),
        },
    ),
    # `BPy_IDGroup_Map_GetItem` / `BPy_IDGroup_Map_SetItem` require `str` keys
    # and operate on wrapped values of varying types (see `BPy_IDGroup_WrapData`).
    # `BPy_IDGroup_iter` returns the iterator from `ViewKeys`, which yields `str`.
    ("idprop.types", "IDPropertyGroup"): ClassOverrides(
        dunders={
            "__getitem__": DunderInfo(
                params={"key": DunderParam("str", "Property name.")},
                return_type="Any",
            ),
            "__setitem__": DunderInfo(
                params={
                    "key": DunderParam("str", "Property name."),
                    "value": DunderParam("Any", "Value to assign."),
                },
            ),
            "__iter__": DunderInfo(return_type=":class:`IDPropertyGroupIterKeys`"),
        },
    ),
    # The Iter / View types don't set `tp_richcompare`, so they inherit
    # `object`'s slot wrappers; identity-based `__eq__`/`__ne__` work, but
    # ordering ops raise TypeError at runtime. Drop the dead ones.
    ("idprop.types", "IDPropertyGroupIterKeys"): ClassOverrides(
        # `BPy_Group_IterKeys_next` yields the property name as `str`.
        dunders={"__next__": DunderInfo(return_type="str")},
        excluded_dunders={"__lt__", "__le__", "__gt__", "__ge__"},
    ),
    ("idprop.types", "IDPropertyGroupIterValues"): ClassOverrides(
        # `BPy_Group_IterValues_next` yields the wrapped property value.
        dunders={"__next__": DunderInfo(return_type="Any")},
        excluded_dunders={"__lt__", "__le__", "__gt__", "__ge__"},
    ),
    ("idprop.types", "IDPropertyGroupIterItems"): ClassOverrides(
        # `BPy_Group_IterItems_next` yields a `(name, value)` tuple.
        dunders={"__next__": DunderInfo(return_type="tuple[str, Any]")},
        excluded_dunders={"__lt__", "__le__", "__gt__", "__ge__"},
    ),
    ("idprop.types", "IDPropertyGroupViewKeys"): ClassOverrides(
        # `BPy_Group_ViewKeys_iter` returns a fresh `IterKeys`, not the view itself.
        dunders={"__iter__": DunderInfo(return_type=":class:`IDPropertyGroupIterKeys`")},
        excluded_dunders={"__lt__", "__le__", "__gt__", "__ge__"},
    ),
    ("idprop.types", "IDPropertyGroupViewValues"): ClassOverrides(
        # `BPy_Group_ViewValues_iter` returns a fresh `IterValues`.
        dunders={"__iter__": DunderInfo(return_type=":class:`IDPropertyGroupIterValues`")},
        excluded_dunders={"__lt__", "__le__", "__gt__", "__ge__"},
    ),
    ("idprop.types", "IDPropertyGroupViewItems"): ClassOverrides(
        # `BPy_Group_ViewItems_iter` returns a fresh `IterItems`.
        dunders={"__iter__": DunderInfo(return_type=":class:`IDPropertyGroupIterItems`")},
        excluded_dunders={"__lt__", "__le__", "__gt__", "__ge__"},
    ),
    # bmesh element sequence types return their element, not float.
    ("bmesh.types", "BMElemSeq"): ClassOverrides(
        dunders={"__getitem__": DunderInfo(
            return_type=":class:`BMVert` | :class:`BMEdge` | :class:`BMFace`")},
    ),
    ("bmesh.types", "BMVertSeq"): ClassOverrides(
        dunders={"__getitem__": DunderInfo(return_type=":class:`BMVert`")},
    ),
    ("bmesh.types", "BMEdgeSeq"): ClassOverrides(
        dunders={"__getitem__": DunderInfo(return_type=":class:`BMEdge`")},
    ),
    ("bmesh.types", "BMFaceSeq"): ClassOverrides(
        dunders={"__getitem__": DunderInfo(return_type=":class:`BMFace`")},
    ),
    ("bmesh.types", "BMEditSelSeq"): ClassOverrides(
        dunders={"__getitem__": DunderInfo(
            return_type=":class:`BMVert` | :class:`BMEdge` | :class:`BMFace`")},
    ),
    ("bmesh.types", "BMLayerCollection"): ClassOverrides(
        dunders={"__getitem__": DunderInfo(return_type=":class:`BMLayerItem`")},
    ),
}

EMPTY_CLASS_OVERRIDES = ClassOverrides()


def write_dunder_methods(fw: WriteFn, class_value: type, module_name: str, type_name: str) -> None:
    """
    Write known dunder (protocol) methods defined on this type.

    A dunder counts as defined here when it appears directly in
    ``class_value.__dict__`` with a non-``None`` value. Inherited dunders are
    documented on the defining base; a ``None`` entry (e.g. ``dict.__hash__``)
    marks the type as opting out and is skipped.
    """
    overrides = CLASS_OVERRIDES.get((module_name, type_name), EMPTY_CLASS_OVERRIDES)
    excluded = overrides.excluded_dunders or set()
    dunder_keys = {
        key for key in DUNDER_METHODS
        if key not in excluded and class_value.__dict__.get(key) is not None
    }
    if not dunder_keys:
        return

    # Wrap in a `.. details::` block (defined in `conf.py`)
    # so the underscore methods are nested in a `<details>` drop-down.
    # The stub generator handles this so it has no effect on the generated stub.
    fw("   .. details:: Special Methods\n\n")

    class_dunders = overrides.dunders or {}
    for key in sorted(dunder_keys):
        info = merge_dunder_info(DUNDER_METHODS[key], class_dunders.get(key))
        # Base entries always populate signature/params/return_type, so these are non-None here.
        assert info.signature is not None and info.params is not None and info.return_type is not None
        if info.overloads is not None:
            assert len(info.params) == 1, "dunder overloads only supported for single-param dunders"
            (pname, param), = info.params.items()
            for i, overload in enumerate(info.overloads):
                fw("      .. method:: {:s}{:s}\n".format(key, info.signature))
                # `:noindex:` on subsequent overloads keeps Sphinx from raising
                # duplicate-target warnings.
                if i > 0:
                    fw("         :noindex:\n")
                fw("\n")
                fw("         :param {:s}: {:s}\n".format(pname, param.description))
                fw("         :type {:s}: {:s}\n".format(pname, overload.operand_type))
                fw("         :rtype: {:s}\n\n".format(overload.return_type))
            continue
        rtype = info.return_type
        fw("      .. method:: {:s}{:s}\n\n".format(key, info.signature))
        for pname, param in info.params.items():
            fw("         :param {:s}: {:s}\n".format(pname, param.description))
            fw("         :type {:s}: {:s}\n".format(pname, param.param_type))
        if rtype == "self":
            fw("         :rtype: :class:`{:s}`\n\n".format(type_name))
        elif rtype:
            fw("         :rtype: {:s}\n\n".format(rtype))
        else:
            fw("\n")


def pyclass2sphinx(
        fw: WriteFn,
        module_name: str,
        type_name: str,
        value: type,
        write_class_examples: bool,
) -> None:
    # NOTE: for `.. class::` identifiers, the type name alone is enough
    # because the module has already been set via `.. module::`.
    if value.__doc__:
        if value.__doc__.startswith(".. class::"):
            fw(value.__doc__)
        else:
            fw(".. class:: {:s}\n\n".format(type_name))
            write_indented_lines("   ", fw, value.__doc__, True)
    else:
        fw(".. class:: {:s}\n\n".format(type_name))
    fw("\n")

    if write_class_examples:
        write_example_ref("   ", fw, module_name + "." + type_name)

    descr_items = [(key, descr) for key, descr in sorted(value.__dict__.items()) if not key.startswith("_")]

    for key, descr in descr_items:
        if type(descr) == ClassMethodDescriptorType:
            py_descr2sphinx("   ", fw, descr, module_name, type_name, key, is_class=True)

    # Needed for pure Python classes.
    for key, descr in descr_items:
        if type(descr) == FunctionType:
            pyfunc2sphinx("   ", fw, module_name, type_name, key, descr, struct=None, is_class=True)

    for key, descr in descr_items:
        if type(descr) == MethodDescriptorType:
            py_descr2sphinx("   ", fw, descr, module_name, type_name, key, is_class=True)

    for key, descr in descr_items:
        if type(descr) == GetSetDescriptorType:
            py_descr2sphinx("   ", fw, descr, module_name, type_name, key, is_class=True)

    # Needed for pure Python classes.
    for key, descr in descr_items:
        if type(descr) == classmethod:
            descr = getattr(value, key)
            pyfunc2sphinx("   ", fw, module_name, type_name, key, descr, struct=None, is_class=True)

    for key, descr in descr_items:
        if type(descr) == StaticMethodType:
            descr = getattr(value, key)
            if type(descr) in {BuiltinMethodType, BuiltinFunctionType}:
                # CAPI-defined static methods already contain RST directives
                # in their docstrings, write them directly.
                write_indented_lines("   ", fw, descr.__doc__ or "Undocumented", False)
                fw("\n")
            else:
                # Python-defined static methods need signature extraction.
                pyfunc2sphinx("   ", fw, module_name, type_name, key, descr, struct=None, is_class=True)

    if USE_STUB_GEN:
        write_dunder_methods(fw, value, module_name, type_name)

    fw("\n\n")


# Changes In Blender will force errors here.
context_type_map = {
    # Support multiple types for each item, where each list item is a possible type:
    # `context_member: [(RNA type, is_collection), ...]`
    "active_action": [("Action", False)],
    "active_annotation_layer": [("AnnotationLayer", False)],
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
    "fluid": [("FluidModifier", False)],
    "gpencil": [("GreasePencil", False)],
    "grease_pencil": [("GreasePencil", False)],
    "curves": [("Curves", False)],
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
    experimental: dict[str, list[tuple[str, bool]]] = {
        # No experimental members in context currently.
    }
    for key, value in experimental.items():
        assert key not in context_type_map, "Duplicate, the member must be removed from one of the dictionaries"
        context_type_map[key] = value
    del experimental


def format_operator_as_module(op_id: str) -> str:
    # `FOO_OT_bar` -> `foo.bar`.
    mod, _, fn = op_id.partition("_OT_")
    assert fn, "Expected to have an `_OT_` separator."
    return "{:s}.{:s}".format(mod.lower(), fn)


def format_description_and_type_info(description: str, type_info: Sequence[str]) -> str:
    if type_info:
        # Add some information at the end of the description,
        # this doesn't really fit all that well anywhere, but it's often short and not worth
        # the vertical space used by having its own line.
        # However, if the description is already multi-line, then we do need to add a separate line,
        # otherwise this would get mixed up in dot-points or notes.
        if "\n" in description:
            sep = "\n\n"
        elif description:
            sep = " "
        else:
            sep = ""
        description = "{:s}{:s}({:s})".format(description, sep, ", ".join(type_info))
    return description


def pycontext2sphinx(basepath: Path) -> None:
    # Not actually a module, only write this file so we can reference in the TOC.
    filepath = basepath / "bpy.context.rst"
    with open(filepath, "w", encoding="utf-8") as fh:
        fw = fh.write
        fw(title_string("Context Access (bpy.context)", "="))
        fw(".. module:: bpy.context\n")
        fw("\n")
        fw("The context members available depend on the area of Blender which is currently being accessed.\n")
        fw("\n")
        fw("Note that all context values are read-only,\n")
        fw("but may be modified through the data API or by running operators.\n")
        fw("\n")
        fw(".. data:: context\n")
        fw("\n")
        fw("   Access to the current window-manager and data context.\n")
        fw("\n")
        fw("   :type: :class:`bpy.types.Context`\n")


def pycontext_members2sphinx(ident: str, fw: WriteFn, written_props: set[str]) -> None:
    # Write context members into `bpy.types.Context`.

    # Track all unique properties to avoid duplicates.
    unique = set(written_props)

    # Internal API call only intended to be used to extract context members.
    from _bpy import context_members
    context_member_map = context_members()
    del context_members

    # Track all context strings to validate `context_type_map`.
    unique_context_strings = set()
    for ctx_str, ctx_members in sorted(context_member_map.items()):
        subsection = "{:s} Context".format(ctx_str.split("_")[0].title())
        fw("\n{:s}.. rubric:: {:s}\n\n".format(ident, subsection))
        for member in ctx_members:
            unique_all_len = len(unique)
            unique.add(member)
            member_visited = unique_all_len == len(unique)

            unique_context_strings.add(member)

            fw("{:s}.. data:: {:s}\n".format(ident, member))
            # Avoid warnings about the member being included multiple times.
            if member_visited:
                fw("{:s}   :noindex:\n".format(ident))
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
                    class_str = ":class:`{:s}`".format(member_type)
                    if is_seq:
                        type_strs.append("Sequence[{:s}]".format(class_str))
                    else:
                        type_strs.append(class_str)
                else:
                    type_strs.append(member_type)

            fw("{:s}   :type: {:s}\n\n".format(ident, " | ".join(type_strs)))
            write_example_ref(ident + "   ", fw, "bpy.context." + member)

    # A bit of a hack: add a trailing rubric so that the methods which follow
    # aren't visually grouped under the last context heading.
    fw("\n{:s}.. rubric:: Methods\n\n".format(ident))

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


def pyrna_enum2sphinx(prop: stub.InfoPropertyRNA, use_empty_descriptions: bool = False) -> str:
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


def pyrna_deprecated_directive(ident: str, deprecated: stub.RnaDeprecated) -> str:
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


def pyrna2sphinx(basepath: Path) -> None:
    """
    ``bpy.types`` and ``bpy.ops``.
    """
    # `structs, funcs, ops, props = rna_info.BuildRNAInfo()`
    structs, _funcs, ops, _props = rna_info_BuildRNAInfo_cache()

    if USE_ONLY_BUILTIN_RNA_TYPES:
        # Ignore properties that use non `bpy.types` properties.
        structs_exclude = {
            v.identifier for v in structs.values()
            if v.module_name != "bpy.types"
        }
        for v in structs.values():
            for p in v.properties:
                for identifier in (
                        getattr(p.srna, "identifier", None),
                        getattr(p.fixed_type, "identifier", None),
                ):
                    if identifier is not None:
                        if identifier in structs_exclude:
                            RNA_EXCLUDE.setdefault(v.identifier, set()).add(identifier)
        del structs_exclude

        structs = {
            k: v for k, v in structs.items()
            if v.module_name == "bpy.types"
        }

    if GLOBAL.filter_bpy_types is not None:
        structs = {
            k: v for k, v in structs.items()
            if k[1] in GLOBAL.filter_bpy_types
            if v.module_name == "bpy.types"
        }

    if GLOBAL.filter_bpy_ops is not None:
        ops = {k: v for k, v in ops.items() if v.module_name in GLOBAL.filter_bpy_ops}

    # Build set of struct identifiers that are collection wrappers
    # (e.g. BlendDataObjects wraps `BlendData.objects`).  These should
    # inherit from `bpy_prop_collection` rather than `bpy_struct`.
    _collection_wrapper_ids: set[str] = set()
    for _struct in structs.values():
        for _prop in _struct.properties:
            if _prop.type == "collection" and _prop.srna is not None:
                _collection_wrapper_ids.add(_prop.srna.identifier)

    def write_param(ident: str, fw: WriteFn, prop: stub.InfoPropertyRNA, is_return: bool = False) -> None:
        if is_return:
            id_name = "return"
            id_type = "rtype"
            identifier = ""
        else:
            id_name = "param"
            id_type = "type"
            identifier = " {:s}".format(prop.identifier)

        enum_descr_override: str | None = None
        if USE_SHARED_RNA_ENUM_ITEMS_STATIC:
            enum_descr_override = pyrna_enum2sphinx_shared_link(prop)

        type_descr, type_info = prop.get_type_description(
            as_arg=not is_return,
            as_ret=is_return,
            class_fmt=":class:`{:s}`",
            mathutils_fmt=":class:`mathutils.{:s}`",
            literal_fmt="``{!r}``",  # String with quotes.
            collection_id=_BPY_PROP_COLLECTION_ID,
            enum_descr_override=enum_descr_override,
        )

        # Only for `bpy.ops.*` parameters.
        if prop.fixed_type is not None and prop.fixed_type.is_operator_properties():
            # Support operator macros where properties from another operator are passed in.
            # These *would* be an `OperatorProperties` type, however, there isn't a convenient
            # way to construct this data type, so - coercing them from a `dict` is supported.
            # Since this is a special case in the RNA API, we need to override the type.
            # Link to the operator to find the supported arguments.
            type_info.append(
                ":func:`bpy.ops.{:s}` keyword arguments".format(format_operator_as_module(prop.identifier)),
            )
            type_descr = "dict[str, Any]"

        prop_name = prop.name
        prop_description = format_description_and_type_info(prop.description, type_info)

        # If the link has been written, no need to inline the enum items.
        enum_text = "" if enum_descr_override else pyrna_enum2sphinx(prop)
        # Don't accidentally use this again (some value have been manipulated).
        del prop

        if prop_name or prop_description or enum_text:
            fw(ident + ":{:s}{:s}: ".format(id_name, identifier))

            if prop_name or prop_description:
                fw(", ".join(val for val in (prop_name, prop_description.replace("\n", "")) if val) + "\n")

            # Special exception, can't use generic code here for enums.
            if enum_text:
                fw("\n")
                write_indented_lines(ident + "   ", fw, enum_text)
            del enum_text
            # End enum exception.

        fw(ident + ":{:s}{:s}: {:s}\n".format(id_type, identifier, type_descr))

    def write_struct(struct: stub.InfoStructRNA) -> None:
        # if not struct.identifier.startswith("Sc") and not struct.identifier.startswith("I"):
        #     return

        # if not struct.identifier == "Object":
        #     return

        struct_module_name = struct.module_name
        if USE_ONLY_BUILTIN_RNA_TYPES:
            assert struct_module_name == "bpy.types"
        filepath = basepath / "{:s}.{:s}.rst".format(struct_module_name, struct.identifier)
        file = open(filepath, "w", encoding="utf-8")
        fw = file.write

        base_id = getattr(struct.base, "identifier", "")
        struct_id = struct.identifier

        if USE_PYCAPI_TYPES:
            if not base_id:
                # Collection wrapper structs inherit from `bpy_prop_collection`,
                # all other root structs inherit from `bpy_struct`.
                if struct_id in _collection_wrapper_ids:
                    base_id = _BPY_PROP_COLLECTION_PYCAPI
                else:
                    base_id = _BPY_STRUCT_PYCAPI

        if base_id:
            title = "{:s}({:s})".format(struct_id, base_id)
        else:
            title = struct_id

        fw(title_string(title, "="))

        fw(".. currentmodule:: {:s}\n\n".format(struct_module_name))

        # Docs first? OK.
        write_example_ref("", fw, "{:s}.{:s}".format(struct_module_name, struct_id))

        base_ids = [base.identifier for base in struct.get_bases()]

        if USE_PYCAPI_TYPES:
            if not base_ids:
                if struct_id in _collection_wrapper_ids:
                    base_ids.append(_BPY_PROP_COLLECTION_PYCAPI)
                else:
                    base_ids.append(_BPY_STRUCT_PYCAPI)
            else:
                base_ids.append(_BPY_STRUCT_PYCAPI)

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

        if USE_PYCAPI_TYPES:
            if not base_id:
                if struct_id in _collection_wrapper_ids:
                    base_id = _BPY_PROP_COLLECTION_PYCAPI
                else:
                    base_id = _BPY_STRUCT_PYCAPI

        if base_id:
            fw(".. class:: {:s}({:s})\n\n".format(struct_id, base_id))
        else:
            fw(".. class:: {:s}\n\n".format(struct_id))

        write_indented_lines("   ", fw, struct.description, False)
        fw("\n")

        # Properties sorted in alphabetical order.
        sorted_struct_properties = sorted(struct.properties, key=lambda prop: prop.identifier)

        # Support excluding props.
        struct_exclude = RNA_EXCLUDE.get(struct_id, ())

        for prop in sorted_struct_properties:
            identifier = prop.identifier

            # Support excluding props.
            if identifier in struct_exclude:
                continue

            enum_descr_override = None
            if USE_SHARED_RNA_ENUM_ITEMS_STATIC:
                enum_descr_override = pyrna_enum2sphinx_shared_link(prop)

            type_descr, type_info = prop.get_type_description(
                class_fmt=":class:`{:s}`",
                mathutils_fmt=":class:`mathutils.{:s}`",
                literal_fmt="``{!r}``",  # String with quotes.
                collection_id=_BPY_PROP_COLLECTION_ID,
                enum_descr_override=enum_descr_override,
            )
            # Read-only properties use "data" directive, variables properties use "attribute" directive.
            if "readonly" in type_info:
                fw("   .. data:: {:s}\n".format(identifier))
            else:
                fw("   .. attribute:: {:s}\n".format(identifier))
            # Also write `noindex` on request.
            if ("bpy.types", struct_id, identifier) in RST_NOINDEX_ATTR:
                fw("      :noindex:\n")
            fw("\n")

            prop_description = format_description_and_type_info(prop.description, type_info)
            if prop_description:
                write_indented_lines("      ", fw, prop_description, False)
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

        # Screen context members (only for Context struct).
        if struct_id == "Context":
            written_props = {prop.identifier for prop in sorted_struct_properties
                             if prop.identifier not in struct_exclude}
            pycontext_members2sphinx("   ", fw, written_props)

        # Python attributes.
        py_properties = struct.get_py_properties()
        py_prop = None
        for identifier, py_prop in py_properties:
            pyprop2sphinx("   ", fw, identifier, py_prop)
        del py_properties, py_prop

        # C/Python attributes: `GetSetDescriptorType`.
        key = descr = None
        for key, descr in sorted(struct.get_py_c_properties_getset()):
            py_descr2sphinx("   ", fw, descr, "bpy.types", struct_id, key, is_class=True)
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

                    type_descr, _type_info = prop.get_type_description(
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

                fw("      :rtype: tuple[{:s}]\n".format(", ".join(type_descrs)))

            write_example_ref("      ", fw, struct_module_name + "." + struct_id + "." + func.identifier)

            fw("\n")

        # Python methods.
        py_funcs = struct.get_py_functions()
        py_func = None

        for identifier, py_func in py_funcs:
            pyfunc2sphinx("   ", fw, "bpy.types", struct_id, identifier, py_func, struct=struct, is_class=True)
        del py_funcs, py_func

        py_funcs = struct.get_py_c_functions()
        py_func = None

        for identifier, py_func in py_funcs:
            py_c_func2sphinx("   ", fw, "bpy.types", struct_id, identifier, py_func, is_class=True)

        lines: list[str] = []

        if struct.base or USE_PYCAPI_TYPES:
            bases = list(reversed(struct.get_bases()))

            # Properties.
            del lines[:]

            if USE_PYCAPI_TYPES:
                descr_items = [
                    (key, descr) for key, descr in sorted(bpy_struct.__dict__.items())
                    if not key.startswith("__")
                ]

                for key, descr in descr_items:
                    if type(descr) == GetSetDescriptorType:
                        lines.append("   - :class:`{:s}.{:s}`\n".format(_BPY_STRUCT_PYCAPI, key))

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

            if USE_PYCAPI_TYPES:
                for key, descr in descr_items:
                    if type(descr) == MethodDescriptorType:
                        lines.append("   - :class:`{:s}.{:s}`\n".format(_BPY_STRUCT_PYCAPI, key))

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

    if "bpy.types" not in GLOBAL.exclude_modules:
        for struct in structs.values():
            # TODO: rna_info should filter these out!
            if struct.is_operator_properties():
                continue
            write_struct(struct)

        def bpy_pycapi_type(
                class_module_name: str,
                class_value: type,
                class_name: str,
                descr_str: str,
                *,
                use_subclasses: bool,
                base_class: str | None,
        ) -> None:
            filepath = basepath / "{:s}.{:s}.rst".format(class_module_name, class_name)
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

            if base_class is not None:
                fw(".. class:: {:s}({:s})\n\n".format(class_name, base_class))
            else:
                fw(".. class:: {:s}\n\n".format(class_name))
            fw("   {:s}\n\n".format(descr_str))

            descr_items = [
                (key, descr) for key, descr in sorted(class_value.__dict__.items())
                if not key.startswith("__")
            ]

            for key, descr in descr_items:
                if type(descr) == MethodDescriptorType:
                    py_descr2sphinx("   ", fw, descr, class_module_name, class_name, key, is_class=True)

            for key, descr in descr_items:
                if type(descr) == GetSetDescriptorType:
                    py_descr2sphinx("   ", fw, descr, class_module_name, class_name, key, is_class=True)

            if USE_STUB_GEN:
                write_dunder_methods(fw, class_value, class_module_name, class_name)

            file.close()

        # Write Python C-API classes.
        if USE_PYCAPI_TYPES:
            bpy_pycapi_type(
                "bpy.types", bpy_struct, _BPY_STRUCT_PYCAPI,
                "built-in base class for all classes in bpy.types.",
                use_subclasses=True,
                base_class=None,
            )

            bpy_pycapi_type(
                "bpy.types", bpy_prop, _BPY_PROP_PYCAPI,
                "built-in base class for all property classes.",
                use_subclasses=False,
                base_class=None,
            )

            bpy_pycapi_type(
                "bpy.types", bpy_prop_array, _BPY_PROP_ARRAY_PYCAPI,
                "built-in class used for array properties.",
                use_subclasses=False,
                base_class=_BPY_PROP_PYCAPI,
            )

            bpy_pycapi_type(
                "bpy.types", bpy_prop_collection, _BPY_PROP_COLLECTION_PYCAPI,
                "built-in class used for all collections.",
                use_subclasses=False,
                base_class=_BPY_PROP_PYCAPI,
            )

            bpy_pycapi_type(
                "bpy.types", bpy_prop_collection_idprop, _BPY_PROP_COLLECTION_IDPROP_PYCAPI,
                "built-in class used for user defined collections.",
                use_subclasses=False,
                base_class=_BPY_PROP_COLLECTION_PYCAPI,
            )

    # Operators.
    def write_ops() -> None:
        API_BASEURL = "https://projects.blender.org/blender/blender/src/branch/main/scripts"

        op_modules: dict[str, list[stub.InfoOperatorRNA]] = {}
        op: stub.InfoOperatorRNA | None = None
        for op in ops.values():
            op_modules.setdefault(op.module_name, []).append(op)
        del op

        for op_module_name, ops_mod in op_modules.items():
            filepath = basepath / "bpy.ops.{:s}.rst".format(op_module_name)
            file = open(filepath, "w", encoding="utf-8")
            fw = file.write

            title = "{:s} Operators".format(op_module_name.replace("_", " ").title())

            fw(title_string(title, "="))

            fw(".. module:: bpy.ops.{:s}\n\n".format(op_module_name))

            ops_mod.sort(key=lambda op: op.func_name)

            for op in ops_mod:
                args = []
                for prop in op.args:
                    arg_default = prop.get_arg_default(force=True)
                    # NOTE: prop.fixed_type.bl_rna.base.identifier == "OperatorProperties"
                    if prop.fixed_type and prop.fixed_type.is_operator_properties():
                        if arg_default.endswith("=None"):
                            arg_default = arg_default.removesuffix("None") + "{}"
                    args.append(arg_default)
                args_str = ", ".join(args)
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

                fw("   :return: Result of the operator call.\n")
                fw("   :rtype: set[Literal[:ref:`rna_enum_operator_return_items`]]\n")

                loc_file, loc_line = op.get_location()
                if loc_file is not None and loc_line is not None:
                    fw("   :File: `{:s}\\:{:d} <{:s}/{:s}#L{:d}>`__\n\n".format(
                        loc_file, loc_line, API_BASEURL, loc_file, loc_line
                    ))

                if op.args:
                    fw("\n")

            file.close()

    if "bpy.ops" not in GLOBAL.exclude_modules:
        write_ops()


def write_rst_index(basepath: Path) -> None:
    """
    Write the RST file of the main page, needed for sphinx: ``index.html``.
    """
    filepath = basepath / "index.rst"
    file = open(filepath, "w", encoding="utf-8")
    fw = file.write

    fw(title_string("Blender {:s} Python API Documentation".format(GLOBAL.blender_version_dots), "%", double=True))
    fw("\n")
    fw("Welcome to the Python API documentation for `Blender <https://www.blender.org>`__, ")
    fw("the free and open source 3D creation suite.\n")
    fw("\n")

    # fw("`A PDF version of this document is also available <{:s}>`_\n".format(GLOBAL.output_filename_pdf))
    fw("This site can be used offline: `Download the full documentation (zipped HTML files) <{:s}>`__\n".format(
        GLOBAL.output_filename_zip,
    ))
    fw("\n")

    if not GLOBAL.exclude_info_docs:
        fw(".. toctree::\n")
        if USE_INFO_DOCS_FANCY_INDEX:
            fw("   :hidden:\n")
        fw("   :maxdepth: 1\n")
        fw("   :caption: Documentation\n\n")
        for info, info_desc in INFO_DOCS:
            fw("   {:s}\n".format(info.name))
        fw("\n")

        if USE_INFO_DOCS_FANCY_INDEX:
            # Show a fake TOC, allowing for an extra description to be shown as well as the title.
            fw(title_string("Documentation", "="))
            for info, info_desc in INFO_DOCS:
                fw("- :doc:`{:s}`: {:s}\n".format(info.stem, info_desc))
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
        if mod not in GLOBAL.exclude_modules:
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
        "idprop",
        "imbuf",
        "mathutils",
    )

    for mod in standalone_modules:
        if mod not in GLOBAL.exclude_modules:
            fw("   {:s}\n".format(mod))
    fw("\n")

    fw(title_string("Indices", "="))
    fw("- :ref:`genindex`\n")
    fw("- :ref:`modindex`\n\n")

    # Special case, this `bmesh.ops.rst` is extracted from C++ source.
    if "bmesh.ops" not in GLOBAL.exclude_modules:
        execfile(SCRIPT_DIR / "rst_from_bmesh_opdefines.py")

    file.close()


def write_rst_bpy(basepath: Path) -> None:
    """
    Write RST file of ``bpy`` module (disabled by default)
    """
    if not GLOBAL.bpy:
        return

    filepath = basepath / "bpy.rst"
    with open(filepath, "w", encoding="utf-8") as fh:
        fw = fh.write

        fw("\n")

        title = ":mod:`bpy` --- Blender Python Module"

        fw(title_string(title, "="))

        fw(".. module:: bpy.types\n\n")


def write_rst_types_index(basepath: Path) -> None:
    """
    Write the RST file of ``bpy.types`` module (index)
    """
    if "bpy.types" in GLOBAL.exclude_modules:
        return

    filepath = basepath / "bpy.types.rst"
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


def write_rst_ops_index(basepath: Path) -> None:
    """
    Write the RST file of bpy.ops module (index)
    """
    if "bpy.ops" in GLOBAL.exclude_modules:
        return

    filepath = basepath / "bpy.ops.rst"
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


def bpy_types_capi_iter() -> Iterator[str]:
    """
    Yield names of C-API defined ``bpy.types.*`` classes (e.g. ``GeometrySet``).

    These are classes added to ``bpy.types`` from C/C++ that are not RNA-derived
    (i.e. not sub-classes of ``bpy_struct``, so they aren't picked up by :func:`pyrna2sphinx`.
    """
    for name in dir(bpy.types):
        if name.startswith("_"):
            continue

        # Core C-API types in `bpy.types` that back the RNA wrapping itself
        # (not user-facing C-API types).
        if name in _BPY_TYPES_CORE_CAPI:
            continue
        attr = getattr(bpy.types, name)
        if not isinstance(attr, type):
            continue
        # Skip RNA-derived types (sub-classes of `bpy_struct`).
        if issubclass(attr, bpy_struct):
            continue
        yield name


def write_rst_bpy_types_capi(basepath: Path) -> None:
    """
    Write the RST files for C-API defined ``bpy.types.*`` classes.
    """
    for type_name in bpy_types_capi_iter():
        identifier = "bpy.types." + type_name
        if identifier in GLOBAL.exclude_modules:
            continue

        filepath = basepath / (identifier + ".rst")
        with open(filepath, "w", encoding="utf-8") as fh:
            fw = fh.write
            fw(title_string(type_name, "="))
            # Needed for Sphinx cross-referencing.
            fw(".. currentmodule:: bpy.types\n\n")
            write_example_ref("", fw, identifier)
            pyclass2sphinx(fw, "bpy.types", type_name, getattr(bpy.types, type_name), False)

        State.example_set_used.add(identifier)


def write_rst_msgbus(basepath: Path) -> None:
    """
    Write the RST files of ``bpy.msgbus`` module
    """
    if 'bpy.msgbus' in GLOBAL.exclude_modules:
        return

    # Write the index.
    filepath = basepath / "bpy.msgbus.rst"
    with open(filepath, "w", encoding="utf-8") as fh:
        fw = fh.write
        fw(title_string("Message Bus (bpy.msgbus)", "="))
        write_example_ref("", fw, "bpy.msgbus")
        fw(".. toctree::\n")
        fw("   :glob:\n\n")
        fw("   bpy.msgbus.*\n\n")

    # Write the contents.
    pymodule2sphinx(basepath, 'bpy.msgbus', bpy.msgbus, 'Message Bus', ())
    State.example_set_used.add("bpy.msgbus")


def write_rst_data(basepath: Path) -> None:
    """
    Write the RST file of ``bpy.data`` module.
    """
    if "bpy.data" in GLOBAL.exclude_modules:
        return

    # Not actually a module, only write this file so we can reference in the TOC.
    filepath = basepath / "bpy.data.rst"
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
        fw(".. literalinclude:: ../examples/bpy.data.0.py\n")

    State.example_set_used.add("bpy.data")


def pyrna_enum2sphinx_shared_link(prop: stub.InfoPropertyRNA) -> str | None:
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


def write_rst_enum_items(
        basepath: Path,
        key: str,
        key_no_prefix: str,
        enum_items: Sequence[stub.RnaEnumItem],
) -> None:
    """
    Write a single page for a static enum in RST.

    This helps avoiding very large lists being in-lined in many places which is an issue
    especially with icons in ``bpy.types.UILayout``. See #87008.
    """
    filepath = basepath / "{:s}.rst".format(key_no_prefix)
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


def write_rst_enum_items_and_index(basepath: Path) -> None:
    """
    Write shared enum items.
    """
    subdir = "bpy_types_enum_items"
    basepath_bpy_types_rna_enum = basepath / subdir
    basepath_bpy_types_rna_enum.mkdir(parents=True, exist_ok=True)
    with open(basepath_bpy_types_rna_enum / "index.rst", "w", encoding="utf-8") as fh:
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


def write_rst_rna_types_with_custom_property_support(basepath: Path) -> None:
    from bpy.types import bpy_struct_meta_idprop  # type: ignore[import-not-found]

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

    with open(basepath / "bpy_types_custom_properties.rst", "w", encoding="utf-8") as fh:
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


def write_rst_importable_modules(basepath: Path) -> None:
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
        "blf.types": "Font Drawing Types",
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
        "gpu.compute": "GPU Compute Utilities",
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
        "idprop": "ID Properties Module",
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
    importable_modules_parent_map: dict[str, list[str]] = {}
    for mod_name in importable_modules:  # Iterate over keys.
        if mod_name in GLOBAL.exclude_modules:
            continue
        if "." in mod_name:
            mod_name, submod_name = mod_name.rsplit(".", 1)
            importable_modules_parent_map.setdefault(mod_name, []).append(submod_name)

    for mod_name, mod_descr in importable_modules.items():
        if mod_name in GLOBAL.exclude_modules:
            continue
        module_all_extra = importable_modules_parent_map.get(mod_name, ())
        module = __import__(mod_name, fromlist=[mod_name.rsplit(".", 1)[-1]])
        pymodule2sphinx(basepath, mod_name, module, mod_descr, module_all_extra)


def copy_handwritten_rsts(basepath: Path) -> None:

    # Info docs.
    if not GLOBAL.exclude_info_docs:
        for info, _info_desc in INFO_DOCS:
            shutil.copy2(RST_DIR / info, basepath)
        for info in INFO_DOCS_OTHER:
            shutil.copy2(RST_DIR / info, basepath)

    # TODO: put this docs in Blender's code and use import as per modules above.
    handwritten_modules = [
        "bmesh.ops",  # Generated by `rst_from_bmesh_opdefines.py`.

        # Includes.
        "include__bmesh",
    ]

    for mod_name in handwritten_modules:
        if mod_name not in GLOBAL.exclude_modules:
            # Copy2 keeps time/date stamps.
            shutil.copy2(RST_DIR / "{:s}.rst".format(mod_name), basepath)

    # Change-log.
    shutil.copy2(RST_DIR / "change_log.rst", basepath)

    # Copy images, could be smarter but just glob for now.
    for p in RST_DIR.iterdir():
        if p.suffix == ".png":
            shutil.copy2(p, basepath)


def copy_handwritten_extra(basepath: Path) -> None:
    for f_src_rel in EXTRA_SOURCE_FILES:
        f_src = RST_DIR / f_src_rel
        f_dst = basepath.joinpath(*("__" if part == ".." else part for part in f_src_rel.parts))

        f_dst.parent.mkdir(parents=True, exist_ok=True)

        shutil.copy2(f_src, f_dst)


def copy_sphinx_files(basepath: Path) -> None:
    shutil.copytree(
        SCRIPT_DIR / "static",
        basepath / "static",
        copy_function=shutil.copy,
    )
    shutil.copytree(
        SCRIPT_DIR / "templates",
        basepath / "templates",
        copy_function=shutil.copy,
    )

    shutil.copy2(SCRIPT_DIR / "conf.py", basepath)


def format_config(basepath: Path) -> None:
    """
    Updates ``conf.py`` with context information from Blender.
    """
    from string import Template

    # Ensure the string literals can contain any characters by closing the surrounding quotes
    # and declare a separate literal via `repr()`.
    def declare_in_quotes(string: str) -> str:
        return "\" {!r} \"".format(string)

    substitutions = {
        "BLENDER_VERSION_STRING": declare_in_quotes(GLOBAL.blender_version_string),
        "BLENDER_VERSION_DOTS": declare_in_quotes(GLOBAL.blender_version_dots),
        "BLENDER_REVISION_TIMESTAMP": declare_in_quotes(str(GLOBAL.blender_revision_timestamp)),
        "BLENDER_REVISION": declare_in_quotes(GLOBAL.blender_revision),
    }

    filepath = basepath / "conf.py"

    # Read the template string from the template file.
    with open(filepath, 'r', encoding="utf-8") as fh:
        template_file = fh.read()

    with open(filepath, 'w', encoding="utf-8") as fh:
        fh.write(Template(template_file).substitute(substitutions))


def rna2sphinx(basepath: Path) -> None:
    # Main page.
    write_rst_index(basepath)

    # Context.
    if "bpy.context" not in GLOBAL.exclude_modules:
        pycontext2sphinx(basepath)

    # Internal modules.
    write_rst_bpy(basepath)                 # `bpy`, disabled by default
    write_rst_types_index(basepath)         # `bpy.types`.
    write_rst_ops_index(basepath)           # `bpy.ops`.
    write_rst_msgbus(basepath)              # `bpy.msgbus`.
    write_rst_bpy_types_capi(basepath)      # `bpy.types.*` (C-API defined).
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


def align_sphinx_in_to_sphinx_in_tmp(dir_src: Path, dir_dst: Path) -> None:
    """
    Move changed files from GLOBAL.sphinx_in_tmp to GLOBAL.sphinx_in
    """
    import filecmp

    # Possible the dir doesn't exist when running recursively.
    dir_dst.mkdir(parents=True, exist_ok=True)

    sphinx_dst_files = {p.name for p in dir_dst.iterdir()}
    sphinx_src_files = {p.name for p in dir_src.iterdir()}

    # Remove deprecated files that have been removed.
    for f in sorted(sphinx_dst_files):
        if f not in sphinx_src_files:
            BPY_LOGGER.debug("\tdeprecated: %s", f)
            f_dst = dir_dst / f
            if f_dst.is_dir():
                shutil.rmtree(f_dst, True)
            else:
                f_dst.unlink()

    # Freshen with new files.
    for f in sorted(sphinx_src_files):
        f_src = dir_src / f
        f_dst = dir_dst / f

        if f_src.is_dir():
            align_sphinx_in_to_sphinx_in_tmp(f_src, f_dst)
        else:
            do_copy = True
            if f in sphinx_dst_files:
                if filecmp.cmp(f_src, f_dst):
                    do_copy = False

            if do_copy:
                BPY_LOGGER.debug("\tupdating: %s", f)
                shutil.copy(f_src, f_dst)


def refactor_sphinx_log(sphinx_logfile: Path) -> None:
    refactored_log: list[tuple[str, str, str]] = []
    with open(sphinx_logfile, "r", encoding="utf-8") as original_logfile:
        lines = set(original_logfile.readlines())
        for line in lines:
            if 'warning' in line.lower() or 'error' in line.lower():
                parts = line.strip().split(None, 2)
                if len(parts) == 3:
                    location, kind, msg = parts
                    location = str(Path(location).relative_to(GLOBAL.sphinx_in, walk_up=True))
                    refactored_log.append((kind, location, msg))
    with open(sphinx_logfile, "w", encoding="utf-8") as refactored_logfile:
        for log in sorted(refactored_log):
            refactored_logfile.write("%-12s %s\n             %s\n" % log)


def setup_monkey_patch() -> None:
    execfile(SCRIPT_DIR / "sphinx_doc_gen_monkeypatch.py")


# Each entry is `(snapshot, original_list)`; restore by writing the snapshot back.
SetupData = dict[str, list[tuple[list[object], list[object]]]]


# Avoid adding too many changes here.
def setup_blender() -> SetupData:
    # Remove handlers since the functions get included
    # in the doc-string and don't have meaningful names.
    lists_to_restore: list[tuple[list[object], list[object]]] = []
    for var in bpy.app.handlers:
        if isinstance(var, list):
            lists_to_restore.append((var[:], var))
            var.clear()

    return {
        "lists_to_restore": lists_to_restore,
    }


def teardown_blender(setup_data: SetupData) -> None:
    for var_src, var_dst in setup_data["lists_to_restore"]:
        var_dst[:] = var_src


def main(argv: Sequence[str] | None = None) -> int:
    global GLOBAL
    if argv is None:
        argv = []
        if "--" in sys.argv:
            argv = sys.argv[sys.argv.index("--") + 1:]

    GLOBAL = global_create(argv)
    State.reset()

    # First monkey patch to load in fake members.
    setup_monkey_patch()

    # Perform changes to Blender itself.
    setup_data = setup_blender()

    # Eventually, create the directories.
    for dir_path in [GLOBAL.output_dir, GLOBAL.sphinx_in]:
        if not dir_path.exists():
            dir_path.mkdir()

    # Eventually, log in files.
    if GLOBAL.log:
        bpy_logfile = GLOBAL.output_dir / ".bpy.log"
        bpy_logfilehandler = logging.FileHandler(bpy_logfile, mode="w")
        bpy_logfilehandler.setLevel(logging.DEBUG)
        BPY_LOGGER.addHandler(bpy_logfilehandler)

        # Using a `FileHandler` seems to disable the `stdout`, so we add a `StreamHandler`.
        bpy_log_stdout_handler = logging.StreamHandler(stream=sys.stdout)
        bpy_log_stdout_handler.setLevel(logging.DEBUG)
        BPY_LOGGER.addHandler(bpy_log_stdout_handler)

    # In case of out-of-source build, copy the needed directories.
    if GLOBAL.output_dir != SCRIPT_DIR:
        # Examples directory.
        examples_dir_copy = GLOBAL.output_dir / "examples"
        if examples_dir_copy.exists():
            shutil.rmtree(examples_dir_copy, True)
        shutil.copytree(
            EXAMPLES_DIR,
            examples_dir_copy,
            ignore=shutil.ignore_patterns(*(".svn",)),
            copy_function=shutil.copy,
        )

    # Start from a clean directory every time.
    if GLOBAL.sphinx_in_tmp.exists():
        shutil.rmtree(GLOBAL.sphinx_in_tmp, True)

    try:
        GLOBAL.sphinx_in_tmp.mkdir()
    except Exception:
        pass

    # Copy extra files needed for theme.
    copy_sphinx_files(GLOBAL.sphinx_in_tmp)

    # Write information needed for `conf.py`.
    format_config(GLOBAL.sphinx_in_tmp)

    # Dump the API in RST files.
    rna2sphinx(GLOBAL.sphinx_in_tmp)

    if GLOBAL.changelog:
        generate_changelog()

    if GLOBAL.full_rebuild:
        # Only for full updates.
        shutil.rmtree(GLOBAL.sphinx_in, True)
        shutil.copytree(
            GLOBAL.sphinx_in_tmp,
            GLOBAL.sphinx_in,
            copy_function=shutil.copy,
        )
        if GLOBAL.sphinx_build and GLOBAL.sphinx_out.exists():
            shutil.rmtree(GLOBAL.sphinx_out, True)
        if GLOBAL.sphinx_build_pdf:
            assert GLOBAL.sphinx_out_pdf is not None
            if GLOBAL.sphinx_out_pdf.exists():
                shutil.rmtree(GLOBAL.sphinx_out_pdf, True)
    else:
        # Move changed files in `GLOBAL.sphinx_in`.
        align_sphinx_in_to_sphinx_in_tmp(GLOBAL.sphinx_in_tmp, GLOBAL.sphinx_in)

    # Report which example files weren't used.
    example_set_unused = EXAMPLE_SET - State.example_set_used
    if example_set_unused:
        BPY_LOGGER.debug("\nUnused examples found in '%s'...", EXAMPLES_DIR)
        for f in sorted(example_set_unused):
            BPY_LOGGER.debug("    %s.py", f)
        BPY_LOGGER.debug("  %d total\n", len(example_set_unused))
    del example_set_unused

    # Eventually, build the HTML docs.
    if GLOBAL.sphinx_build:
        import subprocess
        subprocess.call(GLOBAL.sphinx_build_cmd)

        # Sphinx-build log cleanup+sort.
        if GLOBAL.log:
            assert GLOBAL.sphinx_build_log is not None
            if GLOBAL.sphinx_build_log.stat().st_size:
                refactor_sphinx_log(GLOBAL.sphinx_build_log)

    # Eventually, build the PDF docs.
    if GLOBAL.sphinx_build_pdf:
        import subprocess
        subprocess.call(GLOBAL.sphinx_build_pdf_cmd)

        assert GLOBAL.sphinx_make_pdf_log is not None  # FIXME: only set when `log=True`.
        with open(GLOBAL.sphinx_make_pdf_log, "w", encoding="utf-8") as fh:
            subprocess.call(GLOBAL.sphinx_make_pdf_cmd, stdout=fh)

        # Sphinx-build log cleanup+sort.
        if GLOBAL.log:
            assert GLOBAL.sphinx_build_pdf_log is not None
            if GLOBAL.sphinx_build_pdf_log.stat().st_size:
                refactor_sphinx_log(GLOBAL.sphinx_build_pdf_log)

    # Eventually, prepare the dir to be deployed online (`GLOBAL.output_base_path`).
    if GLOBAL.pack_reference:

        if GLOBAL.sphinx_build:
            # Delete GLOBAL.output_base_path.
            if GLOBAL.output_base_path.exists():
                shutil.rmtree(GLOBAL.output_base_path, True)

            # Copy GLOBAL.sphinx_out to the GLOBAL.output_base_path.
            ignores = (".doctrees", ".buildinfo")
            shutil.copytree(
                GLOBAL.sphinx_out,
                GLOBAL.output_base_path,
                ignore=shutil.ignore_patterns(*ignores),
            )

            # Zip GLOBAL.output_base_path.
            basename = GLOBAL.output_dir / GLOBAL.output_base_name
            tmp_path = shutil.make_archive(
                str(basename), "zip",
                root_dir=GLOBAL.output_dir,
                base_dir=GLOBAL.output_base_name,
            )
            final_path = GLOBAL.output_base_path / GLOBAL.output_filename_zip
            Path(tmp_path).rename(final_path)

        if GLOBAL.sphinx_build_pdf:
            # Copy the pdf to `GLOBAL.output_base_path`.
            assert GLOBAL.sphinx_out_pdf is not None
            shutil.copy(
                GLOBAL.sphinx_out_pdf / "contents.pdf",
                GLOBAL.output_base_path / GLOBAL.output_filename_pdf,
            )

    teardown_blender(setup_data)

    return 1 if State.error_count else 0


if __name__ == '__main__':
    sys.exit(main())
