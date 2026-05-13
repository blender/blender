#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Generate Python type stubs (.pyi) from RST API documentation.

Reads RST files and produces .pyi stub files for use with mypy.
"""

from __future__ import annotations

__all__ = (
    "main",
)

import argparse
import ast
import fnmatch
import re
import sys
from collections.abc import Callable
from dataclasses import dataclass, field
from pathlib import Path
from typing import NamedTuple

from docutils import nodes  # type: ignore[import-untyped]
from docutils.parsers.rst import Parser as RstParser, Directive, directives, roles  # type: ignore[import-untyped]
from docutils.utils import new_document  # type: ignore[import-untyped]
from docutils.frontend import get_default_settings  # type: ignore[import-untyped]


# ----------------------------------------------------------------------------
# Global State

class Global(NamedTuple):
    verbose: bool = False


# Module-level singleton; rebound on update via `_replace`.
GLOBAL = Global()
del Global


# ----------------------------------------------------------------------------
# Mutable State

class State:
    # While not expected, failure to parse a single signature should not
    # cause all stub generation to fail. These should be treated as bugs and fixed.

    parse_errors: int = 0
    structure_errors: int = 0
    undocumented_errors: int = 0


# ----------------------------------------------------------------------------
# Data Model

@dataclass
class MethodInfo:
    """A method or staticmethod extracted from RST."""

    name: str
    params: str  # "(self, arg: Type = default, ...)"
    return_type: str | None = None
    is_static: bool = False
    is_classmethod: bool = False
    is_overload: bool = False
    body_param_types: dict[str, str] = field(default_factory=dict)


@dataclass
class AttrInfo:
    """A class attribute or data descriptor extracted from RST."""
    name: str
    type_str: str | None = None


@dataclass
class ClassInfo:
    """A class definition extracted from RST."""
    name: str
    init_params: str | None = None  # Constructor params "(x: int, y: int)"
    bases: list[str] = field(default_factory=list)  # Base class names
    init_param_types: dict[str, str] = field(default_factory=dict)
    methods: list[MethodInfo] = field(default_factory=list)
    attributes: list[AttrInfo] = field(default_factory=list)


@dataclass
class FunctionInfo:
    """A module-level function extracted from RST."""
    name: str
    params: str
    return_type: str | None = None
    body_param_types: dict[str, str] = field(default_factory=dict)


@dataclass
class DataInfo:
    """A module-level data/constant extracted from RST."""
    name: str
    type_str: str | None = None


@dataclass
class ModuleInfo:
    """A module's collected classes, functions, and data."""
    name: str = ""
    classes: list[ClassInfo] = field(default_factory=list)
    functions: list[FunctionInfo] = field(default_factory=list)
    data: list[DataInfo] = field(default_factory=list)


# ----------------------------------------------------------------------------
# RST Parsing (docutils)

class ApiNode(nodes.Element):  # type: ignore[misc]
    """Custom node representing an API directive (class, method, etc.)."""


class ApiDirective(Directive):  # type: ignore[misc]
    """Captures API directives as an ApiNode with nested-parsed content."""
    required_arguments = 1
    optional_arguments = 0
    final_argument_whitespace = True
    has_content = True
    # Without this, `:noindex:` is folded into the argument by
    # `final_argument_whitespace`, corrupting the directive name.
    option_spec = {"noindex": directives.flag}

    def run(self) -> list[nodes.Node]:
        # `:noindex:` on a callable directive marks an `@overload` signature; on
        # any other directive it just suppresses a duplicate cross-reference and
        # the directive is dropped.
        is_callable = self.name in {"method", "staticmethod", "classmethod"}
        if "noindex" in self.options and not is_callable:
            return []
        node = ApiNode()
        node["directive_type"] = self.name
        node["argument"] = self.arguments[0]
        node["noindex"] = "noindex" in self.options
        if self.content:
            self.state.nested_parse(self.content, self.content_offset, node)
        return [node]


class ModuleDirective(Directive):  # type: ignore[misc]
    """Captures ``.. module::`` directives (no content body)."""
    required_arguments = 1
    optional_arguments = 0
    has_content = False

    def run(self) -> list[nodes.Node]:
        node = ApiNode()
        node["directive_type"] = "module"
        node["argument"] = self.arguments[0]
        return [node]


class TransparentDirective(Directive):  # type: ignore[misc]
    """
    Parses content and inserts its children directly into the parent node.

    Used for visual directives like ``.. details::`` that Sphinx renders
    as HTML wrappers but don't affect stub generation; nested content is
    exposed as if the directive weren't there.
    """
    required_arguments = 0
    optional_arguments = 1
    final_argument_whitespace = True
    has_content = True

    def run(self) -> list[nodes.Node]:
        if not self.content:
            return []
        temp = nodes.Element()
        self.state.nested_parse(self.content, self.content_offset, temp)
        # Detach children from temp so `docutils` re-parents them cleanly into the parent of this directive.
        children = list(temp.children)
        temp.children = []
        for child in children:
            child.parent = None
        return children


def setup_docutils_once() -> None:
    """Register handlers for Sphinx Python domain directives and roles."""
    for name in (
        "class",
        "function",
        "data",
        "attribute",
        "method",
        "staticmethod",
        "classmethod",
    ):
        directives.register_directive(name, ApiDirective)
    directives.register_directive("module", ModuleDirective)
    directives.register_directive("currentmodule", ModuleDirective)
    directives.register_directive("details", TransparentDirective)

    # Capture Sphinx roles as inline nodes with role metadata.
    def _preserve_role(
        name: str, rawtext: str, text: str, lineno: int,
        inliner: object, options: dict[str, str] | None = None,
        content: list[str] | None = None,
    ) -> tuple[list[nodes.Node], list[nodes.Node]]:
        del lineno, inliner, options, content
        node = nodes.inline(rawtext, text)
        node["role"] = name
        return [node], []

    for role_name in (
        "class",
        "attr",
        "meth",
        "func",
        "data",
        "mod",
        "obj",
        "ref",
    ):
        roles.register_local_role(role_name, _preserve_role)


# ----------------------------------------------------------------------------
# Docutils tree helpers

def parse_document(path: Path) -> nodes.document:
    """Parse an RST file into a docutils document tree."""
    parser = RstParser()
    settings = get_default_settings(RstParser)
    settings.report_level = 5  # suppress warnings
    settings.halt_level = 5
    settings.line_length_limit = 100_000  # bpy_struct subclass listing exceeds default 10k
    doc = new_document(str(path), settings)
    parser.parse(path.read_text(encoding="utf-8"), doc)
    return doc


def inline_node_text(node: nodes.Node) -> str:
    """
    Convert an inline node to its text representation for type strings.

    ``:class:`` roles are preserved as ``:class:`ShortName``` so downstream consumers
    (``convert_rst_type``, ``collect_class_refs``) can identify class references structurally
    rather than relying on naming heuristics.
    """
    # `astext()` is documented to return `str`, but docutils is untyped so
    # mypy infers `Any`; bind to a typed local to satisfy `--strict`.
    text: str = node.astext()
    if isinstance(node, nodes.inline):
        role = node.get("role")
        if role == "ref":
            return text
        if role == "class":
            short = text.split(".")[-1]
            return ":class:`{}`".format(short)
        return text
    if isinstance(node, nodes.literal):
        return text
    if isinstance(node, nodes.substitution_reference):
        ref_name = node.get("refname", text)
        doc = node.document
        if doc is not None and ref_name in doc.substitution_defs:
            sub_text: str = doc.substitution_defs[ref_name].astext()
            return sub_text
        return text
    return text


def paragraph_to_type_str(para: nodes.paragraph) -> str:
    """
    Build a type string from a paragraph node by walking its children.

    Detects ``Literal[alias_ref]`` patterns in adjacent siblings and strips the
    redundant ``Literal[]`` wrapper since the alias is already a ``Literal`` type alias.
    """
    children = list(para.children)
    parts: list[str] = []
    i = 0
    while i < len(children):
        child = children[i]
        # Detect :ref: alias that may be wrapped in Literal[].
        if (
            isinstance(child, nodes.inline) and
            child.get("role") == "ref" and
            (alias_name := child.astext()) in enum_alias_map
        ):
            # Check if wrapped in Literal[...]: previous text ends with
            # "Literal[" and next sibling text starts with "]".
            if (parts and parts[-1].endswith("Literal[") and i + 1 < len(children)):
                next_text = inline_node_text(children[i + 1])
                if next_text.startswith("]"):
                    parts[-1] = parts[-1][:-len("Literal[")]
                    parts.append(alias_name)
                    remainder = next_text[1:]
                    if remainder:
                        parts.append(remainder)
                    i += 2
                    continue
            parts.append(alias_name)
        else:
            parts.append(inline_node_text(child))
        i += 1
    return "".join(parts).strip()


def field_raw_value(field_node: nodes.field) -> str:
    """Extract type string from a field body using the docutils node tree."""
    body = field_node.children[1]  # field_body
    for child in body.children:
        if isinstance(child, nodes.paragraph):
            return paragraph_to_type_str(child)
    text: str = body.astext()
    return text.strip()


def node_fields(node: nodes.Element) -> dict[str, str]:
    """
    Extract field-list entries from direct children of *node*.

    Returns ``{"type position": "tuple[int, int]", "rtype": "str", ...}``.
    """
    result: dict[str, str] = {}
    for child in node.children:
        if isinstance(child, nodes.field_list):
            for field_node in child.children:
                if isinstance(field_node, nodes.field):
                    name = field_node.children[0].astext()
                    result[name] = field_raw_value(field_node)
    return result


# ----------------------------------------------------------------------------
# Document -> data-model extraction

def parse_rst_file(path: Path) -> ModuleInfo:
    """Parse an RST file into a ModuleInfo using docutils."""
    doc = parse_document(path)
    module = ModuleInfo()

    # Walk the tree collecting only *top-level* API nodes (those whose
    # parent is NOT another ApiNode).  Titles create nested sections, so
    # we cannot simply iterate `doc.children`.
    for node in doc.findall(ApiNode):
        if isinstance(node.parent, ApiNode):
            continue  # handled by extract_class / extract_method
        dtype = node["directive_type"]
        if dtype == "module":
            module.name = node["argument"]
        elif dtype == "class":
            module.classes.append(extract_class(node, path))
        elif dtype == "function":
            module.functions.append(extract_function(node, path))
        elif dtype in {"method", "staticmethod", "classmethod"}:
            print(
                "Warning: top-level '{:s}' directive in {:s}: {:s}".format(
                    dtype, str(path), node["argument"],
                ),
                file=sys.stderr,
            )
            State.structure_errors += 1
        elif dtype == "data":
            module.data.append(extract_data(node))

    return module


def extract_class(node: ApiNode, source_path: Path) -> ClassInfo:
    """Extract ClassInfo from a ``.. class::`` node."""
    arg = node["argument"]
    name, rest = split_leading_ident(arg)
    if not name:
        raise ValueError("Expected identifier in class directive: {:s}".format(arg))
    rest = rest.strip()

    # Distinguish base classes from constructor params by trying to parse
    # as a class definition. Constructor syntax (defaults, `/`, etc.)
    # causes a `SyntaxError`, identifying it as constructor params.
    # e.g. `.. class:: Object(ID)` -> bases=["ID"]
    # e.g. `.. class:: Color(rgb=(0.0, 0.0, 0.0), /)` -> init_params
    init_params = None
    init_args: ast.arguments | None = None
    bases: list[str] = []
    if rest.startswith("("):
        try:
            tree = ast.parse("class _{:s}: pass".format(rest))
            cls_def = tree.body[0]
            assert isinstance(cls_def, ast.ClassDef)
            bases = [b.id for b in cls_def.bases if isinstance(b, ast.Name)]
        except SyntaxError:
            # Constructor-style syntax (defaults, `/`, etc.) is not valid
            # in a class definition. Verify it parses as a function signature.
            init_params = rest
            try:
                func_def = ast.parse("def _{:s}: pass".format(rest)).body[0]
                assert isinstance(func_def, ast.FunctionDef)
                init_args = func_def.args
            except SyntaxError:
                print("Warning: could not parse class arguments: {:s}{:s}".format(
                    name, rest), file=sys.stderr)
                State.parse_errors += 1

    cls = ClassInfo(name=name, init_params=init_params, bases=bases)

    # Class-level `:type param:` fields describe constructor param types.
    fields = node_fields(node)
    cls.init_param_types = {
        k[5:]: v for k, v in fields.items() if k.startswith("type ")
    }

    # Reclassify: if a single-identifier "base" has a matching :type: field,
    # it is actually a constructor parameter (e.g. `.. class:: KDTree(size)`).
    # No extra parse for `init_args` here: the sole param is already in
    # `init_param_types`, so the doc check would pass anyway.
    if bases and not init_params and len(bases) == 1 and bases[0] in cls.init_param_types:
        cls.init_params = rest
        cls.bases = []
    elif bases and not init_params and any(b in cls.init_param_types for b in bases):
        print(
            "Warning: multi-arg implicit constructor not supported: {:s}({:s})".format(
                name, ", ".join(bases),
            ),
            file=sys.stderr,
        )
        State.structure_errors += 1

    check_params_consistent(name, init_args, fields, source_path)

    for child in node.children:
        if not isinstance(child, ApiNode):
            continue
        dtype = child["directive_type"]
        if dtype in {"method", "staticmethod", "classmethod"}:
            cls.methods.append(extract_method(child, source_path, class_name=name))
        elif dtype in {"attribute", "data"}:
            cls.attributes.append(extract_attribute(child))

    # Mark methods as overloads when their name appears more than once in the
    # class. Overload signatures are written as `.. method::` directives sharing
    # a name, with `:noindex:` on every entry past the first.
    method_name_counts: dict[str, int] = {}
    for method in cls.methods:
        method_name_counts[method.name] = method_name_counts.get(method.name, 0) + 1
    for method in cls.methods:
        if method_name_counts[method.name] > 1:
            method.is_overload = True

    return cls


def check_params_consistent(
    qual_name: str,
    args: ast.arguments | None,
    fields: dict[str, str],
    source_path: Path,
) -> None:
    """
    Warn for parameters lacking ``:param X:`` / ``:type X:`` documentation,
    and for ``:param X:`` / ``:type X:`` fields that do not match any argument.
    """
    if args is None:
        return
    # `self`/`cls` are omitted from RST signatures, so no skip needed; `*args`/`**kwargs`
    # excluded as they're conventionally documented as a group.
    arg_names: set[str] = set()
    for arg in args.posonlyargs + args.args + args.kwonlyargs:
        name = arg.arg
        arg_names.add(name)
        has_param = ("param " + name) in fields
        has_type = ("type " + name) in fields
        issue: str | None = None
        if not has_param and not has_type:
            issue = "param '{:s}' is undocumented".format(name)
        elif not has_param:
            issue = "param '{:s}' has no :param:".format(name)
        elif not has_type:
            issue = "param '{:s}' has no :type:".format(name)

        if issue is not None:
            print(
                "Warning: {:s}: {:s}: {:s}".format(source_path.name, qual_name, issue),
                file=sys.stderr,
            )
            State.undocumented_errors += 1

    # Inverse: warn for `:param X:` / `:type X:` whose `X` is not an argument.
    # Skip when the signature is variadic - `*args` / `**kwargs` can accept any caller-supplied name.
    if args.vararg is None and args.kwarg is None:
        for key in fields:
            for prefix in ("param ", "type "):
                if key.startswith(prefix):
                    pname = key[len(prefix):]
                    if pname not in arg_names:
                        print(
                            "Warning: {:s}: {:s}: orphan ':{:s}{:s}:' (no matching argument)".format(
                                source_path.name, qual_name, prefix, pname,
                            ),
                            file=sys.stderr,
                        )
                        State.undocumented_errors += 1
                    break


def extract_callable(
    node: ApiNode,
    source_path: Path,
    class_name: str | None = None,
) -> tuple[str, str, str | None, dict[str, str]]:
    """Extract name, params, return type, and body param types from a callable node."""
    arg = node["argument"]
    name, sig_rest = split_leading_ident(arg)
    if not name:
        raise ValueError("Expected identifier in callable directive: {:s}".format(arg))
    sig_rest = sig_rest.strip()

    params, return_type, parsed_args = split_signature(sig_rest)

    fields = node_fields(node)
    body_param_types = {
        k[5:]: v for k, v in fields.items() if k.startswith("type ")
    }
    body_rtype = fields.get("rtype")

    if return_type is None and body_rtype:
        return_type = body_rtype

    qual_name = "{:s}.{:s}".format(class_name, name) if class_name else name
    check_params_consistent(qual_name, parsed_args, fields, source_path)

    return name, params, return_type, body_param_types


def extract_method(node: ApiNode, source_path: Path, class_name: str) -> MethodInfo:
    """Extract MethodInfo from a ``.. method::`` / ``.. staticmethod::`` node."""
    name, params, return_type, body_param_types = extract_callable(node, source_path, class_name=class_name)
    return MethodInfo(
        name=name,
        params=params,
        return_type=return_type,
        is_static=node["directive_type"] == "staticmethod",
        is_classmethod=node["directive_type"] == "classmethod",
        body_param_types=body_param_types,
    )


def extract_attribute(node: ApiNode) -> AttrInfo:
    """Extract AttrInfo from a ``.. attribute::`` / ``.. data::`` node."""
    name = node["argument"].strip()
    fields = node_fields(node)

    type_str = fields.get("type")
    return AttrInfo(name=name, type_str=type_str)


def extract_function(node: ApiNode, source_path: Path) -> FunctionInfo:
    """Extract FunctionInfo from a ``.. function::`` node."""
    name, params, return_type, body_param_types = extract_callable(node, source_path)
    return FunctionInfo(
        name=name,
        params=params,
        return_type=return_type,
        body_param_types=body_param_types,
    )


def extract_data(node: ApiNode) -> DataInfo:
    """Extract DataInfo from a ``.. data::`` node."""
    name = node["argument"].strip()
    fields = node_fields(node)
    type_str = fields.get("type")
    return DataInfo(name=name, type_str=type_str)


def extract_default_root_names(params: str) -> set[str]:
    """
    Return root identifiers referenced in default values of *params*.

    For ``(a, b=IntegrationType.MEAN, c=sys.float_info.max, d=print)`` this
    returns ``{"IntegrationType", "sys", "print"}``. Used to promote cross-stub
    imports needed at runtime (default evaluation) out of ``TYPE_CHECKING``.
    """
    # The signature has already been validated by `split_signature`.
    tree = ast.parse("def _{:s}: pass".format(params))
    func_def = tree.body[0]
    assert isinstance(func_def, ast.FunctionDef)
    args = func_def.args
    names: set[str] = set()
    for d in list(args.defaults) + list(args.kw_defaults):
        if d is None:
            continue
        for sub in ast.walk(d):
            if isinstance(sub, ast.Name):
                names.add(sub.id)
    return names


def split_signature(sig_str: str) -> tuple[str, str | None, ast.arguments | None]:
    """Split ``(params) -> RetType`` into (params_str, return_type, parsed_args)."""
    if not sig_str:
        return "()", None, None

    # Parse as a function definition and inspect the AST.
    snippet = "def _{:s}: pass".format(sig_str)
    try:
        tree = ast.parse(snippet)
    except SyntaxError:
        print("Warning: could not parse signature: {:s}".format(sig_str), file=sys.stderr)
        State.parse_errors += 1
        return sig_str.strip(), None, None
    func_def = tree.body[0]
    assert isinstance(func_def, ast.FunctionDef)
    params = "({:s})".format(ast.unparse(func_def.args))
    return_type = ast.unparse(func_def.returns) if func_def.returns else None
    return params, return_type, func_def.args


# ----------------------------------------------------------------------------
# Shared Enum Aliases

# Write a page for each static enum defined in:
# `source/blender/makesrna/RNA_enum_items.hh` so the enums can be linked to instead of being expanded everywhere.
USE_SHARED_RNA_ENUM_ITEMS_STATIC = True

# Module name where shared enum type aliases are written.
ENUM_ALIASES_MODULE = "_rna_enums"

# Populated by `parse_enum_items()`. Maps label -> list of enum identifiers.
# E.g. `{"rna_enum_attribute_domain_items": ["POINT", "EDGE", ...], ...}`.
enum_alias_map: dict[str, list[str]] = {}

# Populated by `build_collection_element_map()`.
# Maps collection wrapper class name -> element class name.
# E.g. `{"Windows": "Window", "BlendDataObjects": "Object", ...}`.
collection_element_map: dict[str, str] = {}


def parse_enum_items(enum_dir: Path) -> None:
    """Parse shared enum RST files into ``enum_alias_map``."""
    if not enum_dir.is_dir():
        return
    for rst_path in sorted(enum_dir.glob("*.rst")):
        doc = parse_document(rst_path)
        # The label anchor is `.. _rna_enum_xxx:` on the first line.
        label: str | None = None
        for target in doc.findall(nodes.target):
            for name in target.get("names", ()):
                if name.startswith("rna_enum_"):
                    label = name
                    break
            if label:
                break
        if label is None:
            continue

        items: list[str] = []
        for field_list in doc.findall(nodes.field_list):
            for field_node in field_list.children:
                if isinstance(field_node, nodes.field):
                    name = field_node.children[0].astext()
                    items.append(name)
        if items:
            enum_alias_map[label] = items


def write_enum_aliases_module(output_dir: Path) -> Path | None:
    """Write a ``_rna_enums`` stub module containing all shared enum aliases."""
    if not enum_alias_map:
        return None
    lines: list[str] = [
        "# Auto-generated by sphinx_stub_gen.py - do not edit",
        "from __future__ import annotations",
        "",
        "from typing import Literal, TypeAlias",
        "",
    ]
    for name in sorted(enum_alias_map):
        items_str = ", ".join("'{:s}'".format(v) for v in enum_alias_map[name])
        lines.append("{:s}: TypeAlias = Literal[{:s}]".format(name, items_str))

    stub_dir = output_dir / ENUM_ALIASES_MODULE
    stub_dir.mkdir(parents=True, exist_ok=True)
    stub_path = stub_dir / "__init__.pyi"
    stub_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return stub_path


# ----------------------------------------------------------------------------
# Type Conversion

def resolve_class_ref(
    fqn: str,
    imports: set[tuple[str, str]],
    current_module: str,
    class_locations: dict[str, str],
) -> str:
    """Resolve a fully-qualified class name to its short form, tracking imports."""
    parts = fqn.split(".")
    short = parts[-1]
    actual_module = class_locations.get(short, ".".join(parts[:-1]))
    if actual_module and actual_module != current_module:
        imports.add((actual_module, short))
    return short


def track_imports(
    text: str,
    imports: set[tuple[str, str]],
    current_module: str,
    class_locations: dict[str, str],
) -> None:
    """Scan *text* for type names and track the necessary imports."""
    for typing_name in (
        "Any",
        "BinaryIO",
        "Literal",
        "Self",
    ):
        if contains_word(text, typing_name):
            imports.add(("typing", typing_name))
    for abc_name in (
        "Callable",
        "Generator",
        "Iterable",
        "Iterator",
        "Sequence",
    ):
        if contains_word(text, abc_name):
            imports.add(("collections.abc", abc_name))
    if contains_word(text, "ModuleType"):
        imports.add(("types", "ModuleType"))

    # Track imports for enum aliases (prefix-based search + set lookup).
    if "rna_enum_" in text:
        for word in find_rna_enum_words(text):
            if word in enum_alias_map:
                imports.add((ENUM_ALIASES_MODULE, word))

    # Track imports for lowercase class names (bpy_prop_collection, bpy_prop_array, bpy_struct).
    for name in ("bpy_prop_collection", "bpy_prop_array", "bpy_struct"):
        if name in text and name in class_locations and class_locations[name] != current_module:
            imports.add((class_locations[name], name))

    # Track module-qualified references (e.g. `datetime.timedelta`, `mathutils.Vector()`).
    for mod_name in ("contextlib", "datetime", "mathutils", "sys", "bpy"):
        if mod_name + "." in text:
            imports.add(("__bare__", mod_name))


def convert_rst_type(
    type_str: str,
    imports: set[tuple[str, str]],
    current_module: str,
    class_locations: dict[str, str],
) -> str:
    """
    Convert RST type string to Python type annotation, tracking imports.

    Tree-walked type strings preserve ``:class:`` markup so class references
    can be identified structurally. The resolver replaces ``:class:`Name```
    with the short name and tracks the import.
    """
    if not type_str:
        imports.add(("typing", "Any"))
        return "Any"

    result = type_str

    # Fallback: resolve any `:class:` references still present.
    if ":class:" in result:
        result = replace_class_roles(
            result,
            lambda ref: resolve_class_ref(ref, imports, current_module, class_locations),
        )
    result = result.replace("``", "")

    # Resolve dotted class references (e.g. `bpy.types.Scene` -> `Scene`).
    result = resolve_dotted_refs(result, class_locations, current_module, imports)

    track_imports(result, imports, current_module, class_locations)

    result = result.strip()

    # Strip element type from collection wrapper references
    # (e.g. `Windows[Window]` -> `Windows`).  The wrapper classes
    # aren't generic; the element type is conveyed via typed overloads.
    result = strip_collection_wrappers(result)

    return result


def strip_outer_parens(params: str) -> str:
    """
    Remove only the outermost parentheses from a parameter string.

    Parameter strings are stored wrapped in ``(...)`` from RST parsing,
    but need to be unwrapped before embedding in ``def name(...) -> ...: ...``
    templates (which supply their own parentheses).

    Given the input ``(*, location: Vector = (0.0, 0.0))`` it's important
    to only remove the outer-most parentheses.
    """
    if params.startswith("(") and params.endswith(")"):
        return params[1:-1]
    raise ValueError("Input is expected to be wrapped by parentheses: {:s}".format(params))


def is_word_char(c: str) -> bool:
    """Return True if *c* is alphanumeric or underscore (equivalent to ``\\w``)."""
    return c.isalnum() or c == "_"


def params_startswith(params: str, name: str) -> bool:
    """Check if *params* starts with *name* as a complete identifier."""
    if not params.startswith(name):
        return False
    return len(params) == len(name) or not is_word_char(params[len(name)])


def contains_word(text: str, word: str) -> bool:
    """Check if *word* appears in *text* as a whole word (not a substring of a larger identifier)."""
    pos = 0
    word_len = len(word)
    while True:
        idx = text.find(word, pos)
        if idx == -1:
            return False
        if (
                (idx == 0 or not is_word_char(text[idx - 1])) and
                (idx + word_len >= len(text) or not is_word_char(text[idx + word_len]))
        ):
            return True
        pos = idx + 1


def split_leading_ident(s: str) -> tuple[str, str]:
    """Split *s* into a leading identifier and remainder."""
    i = 0
    while i < len(s) and is_word_char(s[i]):
        i += 1
    return s[:i], s[i:]


def replace_class_roles(text: str, fn: Callable[[str], str]) -> str:
    """Replace ``:class:`X``` patterns in *text* by calling *fn(X)*."""
    marker = ":class:`"
    parts: list[str] = []
    pos = 0
    while True:
        idx = text.find(marker, pos)
        if idx == -1:
            parts.append(text[pos:])
            break
        parts.append(text[pos:idx])
        start = idx + len(marker)
        end = text.find("`", start)
        if end == -1:
            parts.append(text[idx:])
            break
        parts.append(fn(text[start:end]))
        pos = end + 1
    return "".join(parts)


def iter_class_roles(text: str) -> list[str]:
    """Return content strings from all ``:class:`X``` patterns in *text*."""
    marker = ":class:`"
    result: list[str] = []
    pos = 0
    while True:
        idx = text.find(marker, pos)
        if idx == -1:
            break
        start = idx + len(marker)
        end = text.find("`", start)
        if end == -1:
            break
        result.append(text[start:end])
        pos = end + 1
    return result


def resolve_dotted_refs(
    text: str,
    class_locations: dict[str, str],
    current_module: str,
    imports: set[tuple[str, str]],
) -> str:
    """Resolve dotted FQN references like ``bpy.types.Scene`` to ``Scene``."""
    parts: list[str] = []
    pos = 0
    span_start = 0  # Start of current non-matching span.
    length = len(text)
    while pos < length:
        # Look for a lowercase letter at a word boundary.
        if text[pos].islower() and (pos == 0 or not is_word_char(text[pos - 1])):
            # Read dot-separated identifiers.
            start = pos
            while True:
                ident_start = pos
                while pos < length and is_word_char(text[pos]):
                    pos += 1
                ident = text[ident_start:pos]
                if not ident:
                    break
                if pos < length and text[pos] == ".":
                    pos += 1  # skip dot, continue reading
                else:
                    break
            fqn = text[start:pos]
            # Check if the final component starts with uppercase and length >= 2.
            dot_idx = fqn.rfind(".")
            if dot_idx != -1:
                final = fqn[dot_idx + 1:]
                if len(final) >= 2 and final[0].isupper() and final in class_locations:
                    module = class_locations[final]
                    if module != current_module:
                        imports.add((module, final))
                    parts.append(text[span_start:start])
                    parts.append(final)
                    span_start = pos
                    continue
            # No resolution, the span continues through this identifier.
        else:
            pos += 1
    parts.append(text[span_start:pos])
    return "".join(parts)


def strip_collection_wrappers(text: str) -> str:
    """Strip ``[element]`` from collection wrapper references like ``Windows[Window]``."""
    parts: list[str] = []
    pos = 0
    span_start = 0  # Start of current non-matching span.
    length = len(text)
    while pos < length:
        # Look for an identifier at a word boundary.
        if is_word_char(text[pos]) and (pos == 0 or not is_word_char(text[pos - 1])):
            ident_start = pos
            while pos < length and is_word_char(text[pos]):
                pos += 1
            ident = text[ident_start:pos]
            # Check for [word] suffix.
            if pos < length and text[pos] == "[" and ident in collection_element_map:
                bracket_start = pos
                pos += 1  # skip [
                elem_start = pos
                while pos < length and is_word_char(text[pos]):
                    pos += 1
                if pos < length and text[pos] == "]" and pos > elem_start:
                    pos += 1  # skip ]
                    # Append span up to and including the identifier, skip [element].
                    parts.append(text[span_start:bracket_start])
                    span_start = pos
                else:
                    pos = bracket_start  # rewind, not a valid [word] pattern
        else:
            pos += 1
    parts.append(text[span_start:pos])
    return "".join(parts)


def find_rna_enum_words(text: str) -> list[str]:
    """Find all ``rna_enum_...`` words in *text* at word boundaries."""
    marker = "rna_enum_"
    result: list[str] = []
    pos = 0
    length = len(text)
    while True:
        idx = text.find(marker, pos)
        if idx == -1:
            break
        # Check word boundary before match.
        if idx > 0 and is_word_char(text[idx - 1]):
            pos = idx + 1
            continue
        # Read to end of word.
        end = idx + len(marker)
        while end < length and is_word_char(text[end]):
            end += 1
        result.append(text[idx:end])
        pos = end
    return result


def parse_bracket_pair(text: str) -> tuple[str, str] | None:
    """Parse ``Name[Element]`` into ``(Name, Element)`` or return None."""
    bracket = text.find("[")
    if bracket == -1 or not text.endswith("]"):
        return None
    name = text[:bracket]
    element = text[bracket + 1:-1]
    if not name or not element:
        return None
    if not all(is_word_char(c) for c in name):
        return None
    if not all(is_word_char(c) for c in element):
        return None
    return name, element


def augment_params(
    params: str,
    body_types: dict[str, str],
    imports: set[tuple[str, str]],
    current_module: str,
    class_locations: dict[str, str],
) -> str:
    """Add type annotations to params from :type: directives using ``ast``."""
    if not body_types:
        return params

    # Parse as a function definition to inspect parameters.
    snippet = "def _{:s}: pass".format(params)
    try:
        tree = ast.parse(snippet)
    except SyntaxError:
        return params
    func_def = tree.body[0]
    assert isinstance(func_def, ast.FunctionDef)
    args = func_def.args

    # Skip if any parameter already has an annotation.
    all_args = args.posonlyargs + args.args + args.kwonlyargs
    if args.vararg:
        all_args.append(args.vararg)
    if args.kwarg:
        all_args.append(args.kwarg)
    if any(a.annotation is not None for a in all_args):
        return params

    # Inject annotations from :type: directives.
    changed = False
    for a in all_args:
        if a.arg in body_types:
            type_ann = convert_rst_type(body_types[a.arg], imports, current_module, class_locations)
            try:
                a.annotation = ast.parse(type_ann, mode="eval").body
            except SyntaxError:
                # Type string isn't a valid Python expression, inject as a string annotation.
                print("Warning: could not parse type annotation: {:s}".format(type_ann), file=sys.stderr)
                State.parse_errors += 1
                a.annotation = ast.Constant(value=type_ann)
            changed = True

    if not changed:
        return params

    return "({:s})".format(ast.unparse(args))


# ----------------------------------------------------------------------------
# Stub Output

# Base classes to declare on specific types for static-analysis compatibility.
# C extension types implement protocols (Sequence, etc.) via slots without
# inheriting the ABC; the stubs need the inheritance for `mypy`.
# Each value is a list of `(base_str, import_module)` pairs.
CLASS_BASES: dict[tuple[str, str], list[tuple[str, str]]] = {
    ("mathutils", "Color"): [("Sequence[float]", "collections.abc")],
    ("mathutils", "Euler"): [("Sequence[float]", "collections.abc")],
    ("mathutils", "Quaternion"): [("Sequence[float]", "collections.abc")],
    ("mathutils", "Vector"): [("Sequence[float]", "collections.abc")],
    ("mathutils", "Matrix"): [("Sequence[Sequence[float]]", "collections.abc")],
}

# `__getitem__` element types for C-extension classes implementing the sequence
# protocol whose doc tree alone cannot drive the overloads (mathutils types are
# the canonical case - they aren't RNA-backed). Each value is the element type
# returned by `__getitem__(int)`; the slice overload returns `tuple[X, ...]`.
SEQUENCE_ELEM_TYPES: dict[tuple[str, str], str] = {
    ("mathutils", "Color"): "float",
    ("mathutils", "Euler"): "float",
    ("mathutils", "Quaternion"): "float",
    ("mathutils", "Vector"): "float",
    ("mathutils", "Matrix"): "Vector",
}

# Hand-written stub fragments to append to specific classes for declarations that
# can't be expressed via RST. Lines are written verbatim into the class body
# (indented one level) after the auto-generated members. Type names referenced in
# the fragment participate in the normal import-tracking mechanism.
CLASS_EXTRA_MEMBERS: dict[tuple[str, str], str] = {}


def sort_classes_by_hierarchy(classes: list[ClassInfo]) -> list[ClassInfo]:
    """
    Reorder classes so each appears after its locally-defined bases.

    Base classes are evaluated at class-definition time; without this, a stub
    file like ``class AOV(bpy_struct): ...`` fails at import when ``bpy_struct``
    is defined later in the same module. Original order is preserved as a
    tiebreaker so the output stays stable.
    """
    by_name = {cls.name: cls for cls in classes}
    result: list[ClassInfo] = []
    written: set[str] = set()

    def visit(cls: ClassInfo) -> None:
        if cls.name in written:
            return
        written.add(cls.name)
        for base in cls.bases:
            base_cls = by_name.get(base)
            if base_cls is not None:
                visit(base_cls)
        result.append(cls)

    for cls in classes:
        visit(cls)
    return result


def write_stub(
    module: ModuleInfo,
    output_dir: Path,
    class_locations: dict[str, str],
    all_modules: dict[str, ModuleInfo],
    missing_types: frozenset[str] = frozenset(),
) -> Path:
    """Generate a .pyi stub file for a module."""
    imports: set[tuple[str, str]] = set()
    body_chunks: list[str] = []
    fw = body_chunks.append
    current_module = module.name

    # Defined class names in this module (for self-reference).
    local_classes = {cls.name for cls in module.classes}

    # Write classes (hierarchy-sorted so locally-defined bases come first).
    for cls in sort_classes_by_hierarchy(module.classes):
        write_class(fw, cls, imports, current_module, class_locations)
        fw("\n")

    for func in module.functions:
        write_function(fw, func, imports, current_module, class_locations)

    for data in module.data:
        write_data(fw, data, imports, current_module, class_locations)

    # Write submodule re-exports for package `__init__`.
    submodules = get_submodules(current_module, all_modules)
    if submodules:
        for sub in submodules:
            fw("from {0:s} import {1:s} as {1:s}\n".format(current_module, sub))

    # Build import header.
    header_chunks: list[str] = []
    fw = header_chunks.append
    fw("# Auto-generated by sphinx_stub_gen.py - do not edit\n")
    fw("from __future__ import annotations\n")
    fw("\n")

    # Check which missing types are actually used in the body, so we can
    # write `X = Any` aliases and ensure `Any` is imported.
    used_missing: list[str] = []
    if missing_types:
        body_text = "".join(body_chunks)
        used_missing = sorted(name for name in missing_types if contains_word(body_text, name))
        if used_missing:
            imports.add(("typing", "Any"))

    # Cross-stub imports go under `if TYPE_CHECKING:` to break import cycles.
    # With `from __future__ import annotations` the names only appear in annotation strings,
    # never executed at runtime - so deferring is safe.
    #
    # Exception: names referenced in default values (e.g. `mode=Type.VALUE`) *are* evaluated at runtime,
    # so those imports are promoted to the top-level group regardless of which stub they live in.
    # If that introduces a real import cycle the default should be re-expressed as a literal.
    stub_module_names = set(all_modules) | {ENUM_ALIASES_MODULE}

    runtime_default_names: set[str] = set()
    for cls in module.classes:
        if cls.init_params:
            runtime_default_names |= extract_default_root_names(cls.init_params)
        for method in cls.methods:
            runtime_default_names |= extract_default_root_names(method.params)
    for func in module.functions:
        runtime_default_names |= extract_default_root_names(func.params)

    # Register imports for cross-stub classes named in defaults:
    # these are bare identifiers like `IntegrationType` in `=IntegrationType.MEAN`,
    # which the type-scanning pass doesn't pick up since they aren't `:class:` references.
    for name in runtime_default_names:
        loc = class_locations.get(name)
        if loc and loc != current_module:
            imports.add((loc, name))

    bare_stdlib_imports = sorted(
        name for mod, name in imports
        if mod == "__bare__" and (
            name not in stub_module_names or
            name in runtime_default_names
        )
    )
    bare_stub_imports = sorted(
        name for mod, name in imports
        if mod == "__bare__" and (
            name in stub_module_names and
            name not in runtime_default_names
        )
    )

    stdlib_groups: dict[str, set[str]] = {}
    stub_groups: dict[str, set[str]] = {}
    for mod, name in imports:
        if mod in {"typing", current_module, "__bare__"}:
            continue
        if name in local_classes:
            continue
        is_stub = mod in stub_module_names and name not in runtime_default_names
        target = stub_groups if is_stub else stdlib_groups
        target.setdefault(mod, set()).add(name)

    typing_names = sorted({name for mod, name in imports if mod == "typing"})
    needs_type_checking = bool(bare_stub_imports or stub_groups)
    if needs_type_checking:
        typing_names = sorted(set(typing_names) | {"TYPE_CHECKING"})

    has_imports = bool(
        typing_names or bare_stdlib_imports or stdlib_groups or needs_type_checking
    )

    if typing_names:
        fw("from typing import {:s}\n".format(", ".join(typing_names)))

    for mod_name in bare_stdlib_imports:
        fw("import {:s}\n".format(mod_name))
    for mod in sorted(stdlib_groups):
        names = ", ".join(sorted(stdlib_groups[mod]))
        fw("from {:s} import {:s}\n".format(mod, names))

    if needs_type_checking:
        fw("\n")
        fw("if TYPE_CHECKING:\n")
        for mod_name in bare_stub_imports:
            fw("    import {:s}\n".format(mod_name))
        for mod in sorted(stub_groups):
            names = ", ".join(sorted(stub_groups[mod]))
            fw("    from {:s} import {:s}\n".format(mod, names))

    if has_imports:
        fw("\n")

    # Write TypeVar declarations needed by generic classes.
    if ("typing", "TypeVar") in imports:
        fw("_T = TypeVar('_T')\n")
        fw("\n")

    # Write `Any` aliases for types referenced but not defined in any module.
    if used_missing:
        for name in used_missing:
            fw("{:s} = Any\n".format(name))
        fw("\n")

    parts = current_module.split(".")
    stub_dir = output_dir
    for part in parts:
        stub_dir = stub_dir / part
    stub_dir.mkdir(parents=True, exist_ok=True)
    stub_path = stub_dir / "__init__.pyi"

    with open(stub_path, "w", encoding="utf-8") as fh:
        fh.write("".join(header_chunks))
        fh.write("".join(body_chunks))
    return stub_path


def write_class(
    fw: Callable[[str], None],
    cls: ClassInfo,
    imports: set[tuple[str, str]],
    current_module: str,
    class_locations: dict[str, str],
) -> None:
    """Write a class definition."""
    # Merge explicit CLASS_BASES overrides with RST-parsed base classes.
    # Use both lists (overrides first) so that e.g. `mathutils.Vector` gets
    # Sequence[float] from the override without discarding any RST-parsed base.
    override = CLASS_BASES.get((current_module, cls.name))
    if override is not None:
        seen: set[str] = set()
        bases: list[str] = []
        for base_str, import_mod in override:
            if base_str not in seen:
                seen.add(base_str)
                bases.append(base_str)
                imports.add((import_mod, base_str.split("[")[0]))
        for b in cls.bases:
            if b not in seen:
                seen.add(b)
                bases.append(b)
    else:
        bases = list(cls.bases)
    resolved: list[str] = []
    for b in bases:
        # Resolve base class to its import location.
        loc = class_locations.get(b)
        if loc and loc != current_module:
            imports.add((loc, b))
        resolved.append(b)

    # `bpy_prop_array` and `bpy_prop_collection` are generic over their element type.
    if cls.name in {"bpy_prop_array", "bpy_prop_collection"}:
        imports.add(("typing", "Generic"))
        imports.add(("typing", "TypeVar"))
        resolved.append("Generic[_T]")

    if resolved:
        fw("class {:s}({:s}):\n".format(cls.name, ", ".join(resolved)))
    else:
        fw("class {:s}:\n".format(cls.name))

    has_members = False

    init_params = cls.init_params
    if init_params:
        params = init_params
        # Apply constructor param types from :type param: fields.
        if cls.init_param_types:
            params = augment_params(
                params, cls.init_param_types, imports,
                current_module, class_locations,
            )
        # Convert types in params if they contain RST references.
        params = convert_params_types(params, imports, current_module, class_locations)
        fw("    def __init__(self, {:s}) -> None: ...\n".format(
            strip_outer_parens(params)
        ))
        has_members = True

    # Collect method names that will be replaced by specialized overloads below.
    skip_methods: set[str] = set()
    sequence_elem_type = SEQUENCE_ELEM_TYPES.get((current_module, cls.name))
    is_generic_collection = cls.name in {"bpy_prop_array", "bpy_prop_collection"}
    is_collection_wrapper = cls.name in collection_element_map
    assert sum((is_generic_collection, is_collection_wrapper, sequence_elem_type is not None)) <= 1
    if is_generic_collection:
        imports.add(("collections.abc", "Iterator"))
        skip_methods.update(("__iter__", "__getitem__"))
    elif is_collection_wrapper:
        skip_methods.update(("__iter__", "__getitem__"))
    elif sequence_elem_type is not None:
        # __iter__ rtype defaults to "self" in DUNDER_METHODS, which is wrong
        # for sequence-protocol classes - the iterator yields the element type,
        # not the container type.
        skip_methods.update(("__iter__", "__getitem__"))

    # Heuristic: a class with an `items()` method exposes a dict-like keyed
    # interface, so its `__getitem__` likely accepts `str` keys in addition
    # to `int`/`slice`.
    has_items_method = any(m.name == "items" for m in cls.methods)

    for method in cls.methods:
        if method.name in skip_methods:
            continue
        write_method(
            fw, method, imports, current_module, class_locations,
            has_items_method=has_items_method,
        )
        has_members = True

    if cls.name == "bpy_prop_array":
        imports.add(("typing", "overload"))
        fw("    @overload\n")
        fw("    def __getitem__(self, key: int) -> _T: ...\n")
        fw("    @overload\n")
        fw("    def __getitem__(self, key: slice) -> tuple[_T, ...]: ...\n")
        fw("    def __iter__(self) -> Iterator[_T]: ...\n")
        has_members = True

    if cls.name == "bpy_prop_collection":
        imports.add(("typing", "overload"))
        fw("    @overload\n")
        fw("    def __getitem__(self, key: int) -> _T: ...\n")
        fw("    @overload\n")
        fw("    def __getitem__(self, key: str) -> _T: ...\n")
        fw("    @overload\n")
        fw("    def __getitem__(self, key: slice) -> list[_T]: ...\n")
        fw("    def __iter__(self) -> Iterator[_T]: ...\n")
        has_members = True

    # Collection element overloads: if this class is a collection wrapper
    # (e.g. Windows wraps Window), write typed __getitem__ / __iter__.
    elem_type = collection_element_map.get(cls.name)
    if elem_type:
        # Track import for the element type.
        loc = class_locations.get(elem_type)
        if loc and loc != current_module:
            imports.add((loc, elem_type))
        imports.add(("typing", "overload"))
        imports.add(("collections.abc", "Iterator"))
        fw("    @overload\n")
        fw("    def __getitem__(self, key: int) -> {:s}: ...\n".format(elem_type))
        fw("    @overload\n")
        fw("    def __getitem__(self, key: str) -> {:s}: ...\n".format(elem_type))
        fw("    @overload\n")
        fw("    def __getitem__(self, key: slice) -> list[{:s}]: ...\n".format(elem_type))
        fw("    def __iter__(self) -> Iterator[{:s}]: ...\n".format(elem_type))
        has_members = True

    # Explicit overloads for sequence-protocol classes (see SEQUENCE_ELEM_TYPES).
    # Replaces both __getitem__ (with int/slice overloads) and __iter__ so the
    # iterator yields the element type rather than the container type.
    if sequence_elem_type is not None:
        # Track import for the element type if it names a known class.
        loc = class_locations.get(sequence_elem_type)
        if loc and loc != current_module:
            imports.add((loc, sequence_elem_type))
        imports.add(("typing", "overload"))
        imports.add(("collections.abc", "Iterator"))
        fw("    @overload\n")
        fw("    def __getitem__(self, key: int) -> {:s}: ...\n".format(sequence_elem_type))
        fw("    @overload\n")
        fw("    def __getitem__(self, key: slice) -> tuple[{:s}, ...]: ...\n".format(sequence_elem_type))
        fw("    def __iter__(self) -> Iterator[{:s}]: ...\n".format(sequence_elem_type))
        has_members = True

    for attr in cls.attributes:
        if write_attribute(fw, attr, imports, current_module, class_locations):
            has_members = True

    extra_members = CLASS_EXTRA_MEMBERS.get((current_module, cls.name))
    if extra_members is not None:
        # Track imports for any type names referenced in the fragment, then
        # write each non-empty line indented one level into the class body.
        track_imports(extra_members, imports, current_module, class_locations)
        if "@overload" in extra_members:
            imports.add(("typing", "overload"))
        for line in extra_members.splitlines():
            if line:
                fw("    {:s}\n".format(line))
            else:
                fw("\n")
        has_members = True

    if not has_members:
        fw("    ...\n")


def write_method(
    fw: Callable[[str], None],
    method: MethodInfo,
    imports: set[tuple[str, str]],
    current_module: str,
    class_locations: dict[str, str],
    *,
    has_items_method: bool = False,
) -> None:
    """Write a method definition."""
    name = method.name

    params = method.params
    if method.body_param_types:
        params = augment_params(params, method.body_param_types, imports, current_module, class_locations)

    # Convert any RST type references in the params.
    params = convert_params_types(params, imports, current_module, class_locations)

    ret_type = "None"
    if method.return_type:
        ret_type = convert_rst_type(method.return_type, imports, current_module, class_locations)

    # For __getitem__ with a non-primitive return type, write overloads so that
    # int keys return the element and slice keys return a list. Skip when
    # the method is already part of an explicit overload set - in that case
    # each entry should be written as-is by the @overload path below.
    if (
            name == "__getitem__" and
            not method.is_overload and
            ret_type not in {"float", "int", "bool", "str", "None", "Any"}
    ):
        imports.add(("typing", "overload"))
        fw("    @overload\n")
        fw("    def __getitem__(self, key: int) -> {:s}: ...\n".format(ret_type))
        if has_items_method:
            fw("    @overload\n")
            fw("    def __getitem__(self, key: str) -> {:s}: ...\n".format(ret_type))
        fw("    @overload\n")
        fw("    def __getitem__(self, key: slice) -> list[{:s}]: ...\n".format(ret_type))
        return

    inner = strip_outer_parens(params)
    if method.is_overload:
        imports.add(("typing", "overload"))
        fw("    @overload\n")
    if method.is_static:
        fw("    @staticmethod\n")
    elif method.is_classmethod:
        fw("    @classmethod\n")
        if not params_startswith(inner, "cls"):
            inner = "cls, " + inner if inner else "cls"
    else:
        if not params_startswith(inner, "self"):
            inner = "self, " + inner if inner else "self"
    fw("    def {:s}({:s}) -> {:s}: ...\n".format(name, inner, ret_type))


def write_attribute(
    fw: Callable[[str], None],
    attr: AttrInfo,
    imports: set[tuple[str, str]],
    current_module: str,
    class_locations: dict[str, str],
) -> bool:
    """Write an attribute declaration. Returns True if written."""
    name = attr.name

    # Skip attributes whose names aren't valid Python identifiers.
    if not name.isidentifier():
        return False

    if attr.type_str:
        type_ann = convert_rst_type(attr.type_str, imports, current_module, class_locations)
    else:
        imports.add(("typing", "Any"))
        type_ann = "Any"
    # Initialize with `= ...` so the name is a real class attribute. Without it,
    # `from __future__ import annotations` turns `name: type` into a string-only entry in `__annotations__`,
    # so `Class.name` raises `AttributeError` at runtime - which breaks stubs that reference it as a default value.
    fw("    {:s}: {:s} = ...\n".format(name, type_ann))
    return True


def write_function(
    fw: Callable[[str], None],
    func: FunctionInfo,
    imports: set[tuple[str, str]],
    current_module: str,
    class_locations: dict[str, str],
) -> None:
    """Write a module-level function."""
    params = func.params
    if func.body_param_types:
        params = augment_params(params, func.body_param_types, imports, current_module, class_locations)

    params = convert_params_types(params, imports, current_module, class_locations)

    ret_type = "None"
    if func.return_type:
        ret_type = convert_rst_type(func.return_type, imports, current_module, class_locations)

    inner = strip_outer_parens(params)
    fw("def {:s}({:s}) -> {:s}: ...\n".format(func.name, inner, ret_type))


def write_data(
    fw: Callable[[str], None],
    data: DataInfo,
    imports: set[tuple[str, str]],
    current_module: str,
    class_locations: dict[str, str],
) -> None:
    """Write a module-level data declaration."""
    if data.type_str:
        type_ann = convert_rst_type(data.type_str, imports, current_module, class_locations)
    else:
        imports.add(("typing", "Any"))
        type_ann = "Any"
    fw("{:s}: {:s}\n".format(data.name, type_ann))


def convert_params_types(
    params: str,
    imports: set[tuple[str, str]],
    current_module: str,
    class_locations: dict[str, str],
) -> str:
    """Convert ``:class:`` markup in a params string to plain names and track imports."""
    result = params

    if ":class:" in result:
        result = replace_class_roles(
            result,
            lambda ref: resolve_class_ref(ref, imports, current_module, class_locations),
        )

    track_imports(result, imports, current_module, class_locations)

    return result


def get_submodules(module_name: str, all_modules: dict[str, ModuleInfo]) -> list[str]:
    """Get submodule names for a package module."""
    prefix = module_name + "."
    subs = []
    for name in sorted(all_modules):
        if name.startswith(prefix) and "." not in name[len(prefix):]:
            subs.append(name[len(prefix):])
    return subs


# ----------------------------------------------------------------------------
# Module Exclusions

# Module name patterns to skip during stub generation.
# An exact name matches that module only; a pattern ending in `.*` matches
# all submodules under that prefix (the prefix itself must be listed separately
# to also exclude the parent).
EXCLUDED_MODULES: tuple[str, ...] = (
    "aud",
    "aud.*",
)
EXCLUDED_MODULES_RE = re.compile("|".join(fnmatch.translate(p) for p in EXCLUDED_MODULES))


def is_module_excluded(name: str) -> bool:
    """Return True if *name* matches any pattern in ``EXCLUDED_MODULES``."""
    return EXCLUDED_MODULES_RE.match(name) is not None


# ----------------------------------------------------------------------------
# Main

def build_class_locations(modules: dict[str, ModuleInfo]) -> dict[str, str]:
    """Build a mapping from class name to the module where it's defined."""
    locations: dict[str, str] = {}
    for mod_name, mod in modules.items():
        for cls in mod.classes:
            locations.setdefault(cls.name, mod_name)
    return locations


def build_collection_element_map(modules: dict[str, ModuleInfo]) -> None:
    """
    Scan attribute types for ``Wrapper[Element]`` patterns to populate
    ``collection_element_map``.

    Collection properties in the RST are typed as e.g.
    ``:class:`Windows`\\ [:class:`Window`]``, which after parsing becomes
    ``Windows[Window]``.  This builds a mapping from wrapper class name
    to element class name so the stub generator can write typed
    ``__getitem__`` / ``__iter__`` overloads on the wrapper.
    """
    # Only consider wrapper names that are actual classes defined in the
    # modules - not generic types like `Sequence` which also appear as
    # `Sequence[Element]` in attribute types.
    defined_classes: set[str] = set()
    for mod in modules.values():
        for cls in mod.classes:
            defined_classes.add(cls.name)
    # Exclude generic base classes that appear with many different element
    # types - they get their own Generic[_T] overloads instead.
    defined_classes.discard("bpy_prop_collection")
    defined_classes.discard("bpy_prop_array")

    for mod in modules.values():
        for cls in mod.classes:
            for attr in cls.attributes:
                if attr.type_str:
                    # Strip :class: markup so parse_bracket_pair sees
                    # bare identifiers (e.g. "Windows[Window]" not
                    # ":class:`Windows`[:class:`Window`]").
                    clean = replace_class_roles(attr.type_str, lambda ref: ref.split(".")[-1])
                    pair = parse_bracket_pair(clean)
                    if pair and pair[0] in defined_classes:
                        collection_element_map.setdefault(pair[0], pair[1])


def collect_class_refs(type_str: str | None, out: set[str]) -> None:
    """
    Add short class names referenced in *type_str* to *out*.

    Class references are identified via ``:class:`` RST roles which are
    preserved through the pipeline by ``inline_node_text``.
    """
    if type_str:
        for ref in iter_class_roles(type_str):
            out.add(ref.split(".")[-1])


def find_missing_types(
    modules: dict[str, ModuleInfo],
    class_locations: dict[str, str],
) -> frozenset[str]:
    """Find types referenced in annotations but not defined in any module."""
    referenced: set[str] = set()

    for mod in modules.values():
        for cls in mod.classes:
            for attr in cls.attributes:
                collect_class_refs(attr.type_str, referenced)
            for method in cls.methods:
                collect_class_refs(method.return_type, referenced)
                for v in method.body_param_types.values():
                    collect_class_refs(v, referenced)
            for v in cls.init_param_types.values():
                collect_class_refs(v, referenced)
        for func in mod.functions:
            collect_class_refs(func.return_type, referenced)
            for v in func.body_param_types.values():
                collect_class_refs(v, referenced)
        for data in mod.data:
            collect_class_refs(data.type_str, referenced)

    defined = set(class_locations.keys())
    # Exclude builtins, typing names, stdlib types, and common non-class identifiers.
    builtin_and_typing = {
        "AbstractContextManager",
        "Any",
        "BaseException",
        "BinaryIO",
        "Callable",
        "False",
        "Generator",
        "Generic",
        "Iterable",
        "Iterator",
        "Literal",
        "ModuleType",
        "MutableSequence",
        "None",
        "Self",
        "Sequence",
        "True",
        "TypeVar",
        "bool",
        "bytes",
        "dict",
        "float",
        "int",
        "list",
        "set",
        "str",
        "tuple",
        "type",
    }
    return frozenset(referenced - defined - builtin_and_typing)


def main() -> int:
    """Generate ``*.pyi`` stubs from RST documentation."""
    setup_docutils_once()

    parser = argparse.ArgumentParser(description="Generate .pyi stubs from RST API documentation.")
    parser.add_argument(
        "build_dir", nargs="?",
        default=str(Path(__file__).parent / "sphinx-in"),
        help="Directory containing RST files (default: %(default)s)",
    )
    parser.add_argument(
        "-o", "--output", dest="output_dir",
        default=str(Path(__file__).parent / "stubs"),
        help="Output directory for .pyi stubs (default: %(default)s)",
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true",
        help="Show per-module progress and informational warnings.",
    )
    parser.add_argument(
        "--strict-docs", action="store_true",
        help="Treat undocumented parameters as errors (non-zero exit).",
    )
    args = parser.parse_args()

    global GLOBAL
    GLOBAL = GLOBAL._replace(verbose=args.verbose)
    build_dir = Path(args.build_dir)
    output_dir = Path(args.output_dir)

    if not build_dir.exists():
        print("Error: {:s} directory not found.".format(str(build_dir)), file=sys.stderr)
        return 1

    # Refuse to overwrite a non-empty output directory; checked before parsing.
    if output_dir.exists() and any(output_dir.iterdir()):
        print(
            "Error: output directory is not empty: {:s}".format(str(output_dir)),
            file=sys.stderr,
        )
        return 1

    # Parse shared enum items for type aliases.
    if USE_SHARED_RNA_ENUM_ITEMS_STATIC:
        parse_enum_items(build_dir / "bpy_types_enum_items")

    rst_files = sorted(build_dir.glob("*.rst"))
    modules: dict[str, ModuleInfo] = {}

    for rst_path in rst_files:
        if rst_path.name == "index.rst":
            continue
        # Skip excluded modules before parsing so no warnings are emitted.
        # Filename stems match `.. module::` names in this build tree.
        if is_module_excluded(rst_path.stem):
            if GLOBAL.verbose:
                print("Skipped (excluded): {:s}".format(rst_path.stem))
            continue
        module = parse_rst_file(rst_path)
        if not module.name:
            continue
        existing = modules.get(module.name)
        if existing:
            existing.classes.extend(module.classes)
            existing.functions.extend(module.functions)
            existing.data.extend(module.data)
        else:
            modules[module.name] = module
            if GLOBAL.verbose:
                print("Parsed: {:s}".format(module.name))

    # Replace flat `bpy.context` and `bpy.data` modules with typed attributes
    # on the `bpy` package - they are instances, not real modules.
    modules.pop("bpy.context", None)
    modules.pop("bpy.data", None)

    # Create implied parent packages so every module is part of a proper
    # package hierarchy.  For example, `bpy.types` implies a `bpy`
    # package even though there is no `bpy.rst`.
    all_names = set(modules.keys())
    for name in sorted(all_names):
        parts = name.split(".")
        for i in range(1, len(parts)):
            parent = ".".join(parts[:i])
            if parent not in modules:
                modules[parent] = ModuleInfo(name=parent)

    # Type bpy.context and bpy.data as class instances rather than flat modules.
    if "bpy" in modules:
        modules["bpy"].data.append(DataInfo(name="context", type_str=":class:`bpy.types.Context`"))
        modules["bpy"].data.append(DataInfo(name="data", type_str=":class:`bpy.types.BlendData`"))

    build_collection_element_map(modules)
    class_locations = build_class_locations(modules)

    # Undefined referenced types are written as `Foo = Any` aliases by `write_stub`
    # so stubs remain importable; the warning flags ones that should be defined.
    missing = find_missing_types(modules, class_locations)
    if missing and GLOBAL.verbose:
        for name in sorted(missing):
            print("Warning: referenced type not defined: {:s}".format(name), file=sys.stderr)

    stub_paths: list[Path] = []
    if USE_SHARED_RNA_ENUM_ITEMS_STATIC:
        enum_stub = write_enum_aliases_module(output_dir)
        if enum_stub is not None:
            stub_paths.append(enum_stub)

    for _, module in sorted(modules.items()):
        stub_path = write_stub(module, output_dir, class_locations, modules, missing)
        stub_paths.append(stub_path)
        if GLOBAL.verbose:
            print("Generated: {:s}".format(str(stub_path)))

    print("\nStubs written to: {:s}".format(str(output_dir)))
    print("Add to mypy config: mypy_path = doc/python_api/stubs")

    # In-process validation via the standalone validator module.
    from sphinx_stub_validate import validate_stubs
    semantic_module_names = sorted(modules.keys())
    if enum_alias_map:
        semantic_module_names.append(ENUM_ALIASES_MODULE)
    if validate_stubs(
            output_dir, stub_paths, semantic_module_names,
            verbose=GLOBAL.verbose,
    ) != 0:
        return 1

    if State.parse_errors:
        print("\n{:d} parse error(s) encountered".format(State.parse_errors), file=sys.stderr)
        return 1

    if State.structure_errors:
        print("\n{:d} structure error(s) encountered".format(State.structure_errors), file=sys.stderr)
        return 1

    if State.undocumented_errors:
        print(
            "\n{:d} parameter documentation issue(s) encountered".format(State.undocumented_errors),
            file=sys.stderr,
        )
        if args.strict_docs:
            return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
