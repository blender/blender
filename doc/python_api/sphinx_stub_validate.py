#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Validate Python type stubs (.pyi) produced by ``sphinx_stub_gen.py``.

The following checks are performed:

- Static: each ``.pyi`` is parsed & compiled, each name it references must be defined, imported or a builtin.
- Semantic: each module is loaded via a custom ``.pyi`` finder, surfacing
  base-class and module-level evaluation errors (e.g. references to undefined
  names from class bases or non-trivial defaults).
"""

from __future__ import annotations
__all__ = (
    "main",
    "validate_stubs",
)

import argparse
import ast
import builtins
import contextlib
import importlib
import importlib.abc
import importlib.machinery
import importlib.util
import sys
import traceback
import types

from collections.abc import (
    Iterable,
    Iterator,
    Sequence,
)
from pathlib import Path


def stub_module_names(stubs_dir: Path, stub_paths: list[Path]) -> list[str]:
    """Return the dotted module name for each ``__init__.pyi`` (top-level ``__init__.pyi`` excluded)."""
    return [
        ".".join(pyi.parent.relative_to(stubs_dir).parts)
        for pyi in stub_paths
        if pyi.name == "__init__.pyi" and pyi.parent != stubs_dir
    ]


# Generated stubs don't use `lambda`, so it's not treated as a scope.
FunctionNode = ast.FunctionDef | ast.AsyncFunctionDef
ScopeNode = ast.ClassDef | FunctionNode


def scope_args(args: ast.arguments) -> list[ast.arg]:
    """Return all arguments of a function."""
    return [
        *args.posonlyargs, *args.args, *args.kwonlyargs,
        *(arg for arg in (args.vararg, args.kwarg) if arg is not None),
    ]


def scope_outer_nodes(node: ScopeNode) -> list[ast.AST]:
    """Return nodes of *node* evaluated in the enclosing scope (decorators, bases, annotations & defaults)."""
    if isinstance(node, ast.ClassDef):
        return [*node.decorator_list, *node.bases, *node.keywords]
    args = node.args
    nodes: list[ast.AST] = [*node.decorator_list, *args.defaults, *(v for v in args.kw_defaults if v is not None)]
    nodes += (arg.annotation for arg in scope_args(args) if arg.annotation is not None)
    if node.returns is not None:
        nodes.append(node.returns)
    return nodes


def iter_scope_nodes(nodes: Iterable[ast.AST]) -> Iterator[ast.AST]:
    """Walk *nodes* and their children, without descending into the bodies of nested scopes."""
    for node in nodes:
        yield node
        if isinstance(node, ScopeNode):
            yield from iter_scope_nodes(scope_outer_nodes(node))
        else:
            yield from iter_scope_nodes(ast.iter_child_nodes(node))


def find_undefined_names(nodes: Iterable[ast.AST], visible: set[str]) -> Iterator[ast.Name]:
    """
    Yield names loaded in the scope containing *nodes* that aren't defined there or in *visible*.
    Names an enclosing scope defines are visible to nested scopes (as with annotations in a class body).
    """
    scope_nodes = list(iter_scope_nodes(nodes))
    defined = set(visible)
    for node in scope_nodes:
        if isinstance(node, ScopeNode):
            defined.add(node.name)
        elif isinstance(node, (ast.Import, ast.ImportFrom)):
            for alias in node.names:
                defined.add((alias.asname or alias.name).partition(".")[0])
        elif isinstance(node, ast.Name) and isinstance(node.ctx, ast.Store):
            defined.add(node.id)
    for node in scope_nodes:
        if isinstance(node, ast.Name):
            if isinstance(node.ctx, ast.Load) and node.id not in defined:
                yield node
        elif isinstance(node, ast.ClassDef):
            yield from find_undefined_names(node.body, defined)
        elif isinstance(node, FunctionNode):
            yield from find_undefined_names(node.body, defined | {arg.arg for arg in scope_args(node.args)})


def validate_stubs_statically(stub_paths: list[Path]) -> list[str]:
    """
    Parse & compile each path once, return error messages (empty on success).

    Catches read errors (``OSError``), decoding errors (``UnicodeDecodeError``, a ``ValueError``)
    and parse errors (``SyntaxError``, including null bytes) - each is recorded rather than propagated.

    Names referenced but not defined in their stub are also reported,
    since the semantic check can't detect these as annotations aren't evaluated at runtime.
    """
    errors: list[str] = []
    builtin_names = set(dir(builtins))
    for pyi_path in stub_paths:
        try:
            tree = ast.parse(pyi_path.read_text(encoding="utf-8"), str(pyi_path))
            # Reports errors `ast.parse` doesn't, e.g. duplicate argument names.
            compile(tree, str(pyi_path), "exec")
        except SyntaxError as ex:
            errors.append("{:s}:{:d}: {:s}".format(str(pyi_path), ex.lineno or 0, str(ex.msg)))
            continue
        except (OSError, ValueError) as ex:
            errors.append("{:s}: {:s}: {:s}".format(str(pyi_path), type(ex).__name__, str(ex)))
            continue
        # Report each name once per line.
        undefined = sorted({(node.lineno, node.id) for node in find_undefined_names(tree.body, builtin_names)})
        for lineno, name in undefined:
            errors.append("{:s}:{:d}: name not defined: {:s}".format(str(pyi_path), lineno, name))
    return errors


class PyiFinder(importlib.abc.MetaPathFinder):
    """``MetaPathFinder`` that resolves ``__init__.pyi`` files under *base*."""

    def __init__(self, base: Path) -> None:
        self.base = base

    def find_spec(
        self,
        fullname: str,
        path: Sequence[str] | None,
        target: types.ModuleType | None = None,
        /,
    ) -> importlib.machinery.ModuleSpec | None:
        del path, target
        pkg_init = self.base.joinpath(*fullname.split("."), "__init__.pyi")
        if not pkg_init.is_file():
            return None
        return importlib.util.spec_from_file_location(
            fullname,
            pkg_init,
            loader=importlib.machinery.SourceFileLoader(fullname, str(pkg_init)),
            submodule_search_locations=[str(pkg_init.parent)],
        )


@contextlib.contextmanager
def sys_meta_path_insert_context(finder: importlib.abc.MetaPathFinder) -> Iterator[None]:
    """Temporarily insert *finder* at the start of ``sys.meta_path``."""
    sys.meta_path.insert(0, finder)
    try:
        yield
    finally:
        with contextlib.suppress(ValueError):
            sys.meta_path.remove(finder)


@contextlib.contextmanager
def sys_modules_remove_context(module_names: list[str]) -> Iterator[None]:
    """
    Temporarily remove *module_names* from ``sys.modules``.

    On exit, modules imported with these names are removed & the original modules restored.
    """
    modules_orig = {name: sys.modules.pop(name) for name in module_names if name in sys.modules}
    try:
        yield
    finally:
        for name in module_names:
            sys.modules.pop(name, None)
        sys.modules.update(modules_orig)


@contextlib.contextmanager
def backup_attrs_and_assign_multi(
        *obj_attrs: tuple[object, dict[str, object]],
) -> Iterator[None]:
    """
    Context manager that saves and assigns attributes on multiple objects,
    restoring all originals on exit.

    Each argument is a ``(obj, {attr: value, ...})`` pair.
    """
    saved: list[tuple[object, dict[str, object]]] = []
    try:
        for obj, attrs in obj_attrs:
            saved.append((obj, {attr: getattr(obj, attr) for attr in attrs}))
            for attr, value in attrs.items():
                setattr(obj, attr, value)
        yield
    finally:
        for obj, attrs in saved:
            for attr, value in attrs.items():
                setattr(obj, attr, value)


@contextlib.contextmanager
def stub_import_context(stubs_dir: Path, module_names: list[str]) -> Iterator[None]:
    """
    Temporarily import *module_names* from the ``.pyi`` stubs under *stubs_dir*.

    ``sys.meta_path``, ``sys.modules`` & ``sys.dont_write_bytecode`` are restored on exit.
    """
    with (
            sys_meta_path_insert_context(PyiFinder(stubs_dir)),
            # Drop any pre-existing entries (real Blender modules) so the finder is consulted.
            sys_modules_remove_context(module_names),
            # Don't write byte-code (`__pycache__`) into the stubs directory.
            backup_attrs_and_assign_multi((sys, {"dont_write_bytecode": True})),
    ):
        yield


def validate_stubs_semantically(
    stubs_dir: Path,
    module_names: list[str],
) -> list[str]:
    """
    Import each stub module from *stubs_dir*; return error messages.

    ``typing.TYPE_CHECKING`` is intentionally left ``False`` so that cross-stub imports
    under ``if TYPE_CHECKING:`` blocks do not execute - this is what allows cycles to break at runtime.
    The check therefore validates "every stub is importable in isolation".
    """
    errors: list[str] = []
    with stub_import_context(stubs_dir, module_names):
        for name in module_names:
            try:
                importlib.import_module(name)
            except Exception as ex:
                # Locate the error in the innermost stub frame, e.g. the class with an invalid base.
                frames = [frame for frame in traceback.extract_tb(ex.__traceback__) if frame.filename.endswith(".pyi")]
                location = "{:s}:{:d}".format(frames[-1].filename, frames[-1].lineno or 0) if frames else name
                errors.append("{:s}: {:s}: {:s}".format(location, type(ex).__name__, str(ex)))
    # Stubs importing a failing stub report the same error.
    return list(dict.fromkeys(errors))


def report_errors(title: str, errors: list[str]) -> bool:
    """Write *errors* under *title* to ``stderr``, return True when there are errors."""
    if not errors:
        return False
    print("\n{:s}:".format(title), file=sys.stderr)
    for err in errors:
        print("  {:s}".format(err), file=sys.stderr)
    return True


def validate_stubs(
    stubs_dir: Path,
    stub_paths: list[Path],
    *,
    verbose: bool = False,
) -> int:
    """
    Run the checks (see module docstring) against *stub_paths* under *stubs_dir*.

    On any error, details are written to ``stderr`` and the function returns ``1``.
    On success, returns ``0``. The semantic step is skipped if static errors are present.
    """
    if verbose:
        print("Checking {:d} stub(s) under {:s}".format(len(stub_paths), str(stubs_dir)))

    if report_errors("Stub static errors", validate_stubs_statically(stub_paths)):
        return 1
    module_names = stub_module_names(stubs_dir, stub_paths)
    if report_errors("Stub semantic errors", validate_stubs_semantically(stubs_dir, module_names)):
        return 1

    if verbose:
        print("\nAll {:d} stub(s) passed validation.".format(len(stub_paths)))
    return 0


def main() -> int:
    """Validate stubs found under a directory passed on the command line."""
    parser = argparse.ArgumentParser(description="Validate .pyi stubs.")
    parser.add_argument(
        "stubs_dir", nargs="?",
        default=str(Path(__file__).parent / "stubs"),
        help="Directory containing .pyi stubs (default: %(default)s)",
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true",
        help="Report the number of stubs checked.",
    )
    args = parser.parse_args()

    stubs_dir = Path(args.stubs_dir)
    if not stubs_dir.is_dir():
        print("Error: stubs directory not found: {:s}".format(str(stubs_dir)), file=sys.stderr)
        return 1

    stub_paths = sorted(stubs_dir.rglob("*.pyi"))
    if not stub_paths:
        print("Error: no .pyi files found under {:s}".format(str(stubs_dir)), file=sys.stderr)
        return 1

    return validate_stubs(stubs_dir, stub_paths, verbose=args.verbose)


if __name__ == "__main__":
    sys.exit(main())
