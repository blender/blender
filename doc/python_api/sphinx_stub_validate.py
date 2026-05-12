#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Validate Python type stubs (.pyi) produced by ``sphinx_stub_gen.py``.

Two checks are performed:

- Syntactic: each ``.pyi`` is fed through :func:`compile`.
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
import contextlib
import importlib
import importlib.abc
import importlib.machinery
import importlib.util
import sys
import types

from collections.abc import Iterator, Sequence
from pathlib import Path


def discover_stubs(stubs_dir: Path) -> tuple[list[Path], list[str]]:
    """
    Walk *stubs_dir* once and return ``(stub_paths, module_names)``, both sorted.

    *stub_paths* lists every ``.pyi`` file. *module_names* contains the dotted
    module name for each ``__init__.pyi`` (top-level ``__init__.pyi`` excluded).
    """
    stub_paths: list[Path] = []
    module_names: list[str] = []
    for pyi in sorted(stubs_dir.rglob("*.pyi")):
        stub_paths.append(pyi)
        if pyi.name == "__init__.pyi":
            rel = pyi.parent.relative_to(stubs_dir)
            if rel.parts:
                module_names.append(".".join(rel.parts))
    return stub_paths, module_names


def validate_stubs_syntactically(stub_paths: list[Path]) -> list[str]:
    """Run :func:`compile` on each path; return error messages (empty on success).

    Catches read errors (``OSError``), decoding errors (``UnicodeDecodeError``,
    ``ValueError`` for null bytes), and parse errors (``SyntaxError``) - each
    is recorded rather than propagated.
    """
    errors: list[str] = []
    for pyi_path in stub_paths:
        try:
            compile(pyi_path.read_text(encoding="utf-8"), str(pyi_path), "exec")
        except SyntaxError as ex:
            errors.append("{:s}:{:d}: {:s}".format(str(pyi_path), ex.lineno or 0, str(ex.msg)))
        except (OSError, ValueError) as ex:
            errors.append("{:s}: {:s}: {:s}".format(str(pyi_path), type(ex).__name__, str(ex)))
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
        parts = fullname.split(".")
        pkg_init = self.base.joinpath(*parts, "__init__.pyi")
        if pkg_init.is_file():
            loader = importlib.machinery.SourceFileLoader(fullname, str(pkg_init))
            spec = importlib.util.spec_from_loader(fullname, loader)
            if spec is not None:
                spec.submodule_search_locations = [str(pkg_init.parent)]
            return spec
        return None


@contextlib.contextmanager
def stub_import_context(stubs_dir: Path, module_names: list[str]) -> Iterator[None]:
    """
    Temporarily install a ``.pyi`` finder and clear *module_names* from ``sys.modules``.

    Restores both on exit so that ``sys.meta_path`` and ``sys.modules`` are not
    permanently mutated. Safe to nest and re-enter.
    """
    finder = PyiFinder(stubs_dir.resolve())
    sys.meta_path.insert(0, finder)
    # Drop any pre-existing entries (real Blender modules) so the finder is consulted.
    for name in module_names:
        sys.modules.pop(name, None)
    try:
        yield
    finally:
        try:
            sys.meta_path.remove(finder)
        except ValueError:
            pass  # Already removed by something else; nothing to undo.
        for name in module_names:
            sys.modules.pop(name, None)


def validate_stubs_semantically(
    stubs_dir: Path,
    module_names: list[str],
) -> list[str]:
    """
    Import each stub module from *stubs_dir*; return error messages.

    Uses :func:`stub_import_context` so ``sys.meta_path`` and ``sys.modules``
    are restored after the call. ``typing.TYPE_CHECKING`` is intentionally left
    ``False`` so that cross-stub imports under ``if TYPE_CHECKING:`` blocks do
    not execute - this is what allows cycles to break at runtime. The check
    therefore validates "every stub is importable in isolation", which catches
    base-class and module-level evaluation errors (the bugs runtime-importing
    actually surfaces).
    """
    errors: list[str] = []
    with stub_import_context(stubs_dir, module_names):
        for name in module_names:
            try:
                importlib.import_module(name)
            except Exception as ex:
                errors.append("{:s}: {:s}: {:s}".format(name, type(ex).__name__, str(ex)))
    return errors


def validate_stubs(
    stubs_dir: Path,
    stub_paths: list[Path],
    module_names: list[str],
    *,
    verbose: bool = False,
) -> int:
    """
    Run syntax and semantic validation against the given stubs.

    Steps:

    - Syntax: each path in *stub_paths* is compiled via :func:`compile`.
    - Semantic: each name in *module_names* is imported via a temporary ``.pyi`` finder rooted at *stubs_dir*.

    On any error, details are written to ``stderr`` and the function returns ``1``.
    On success, returns ``0``. The semantic step is skipped if syntax errors are present.
    """
    if verbose:
        print("Checking {:d} stub(s) under {:s}".format(len(stub_paths), str(stubs_dir)))

    syntax_errors = validate_stubs_syntactically(stub_paths)
    if syntax_errors:
        print("\nStub syntax errors:", file=sys.stderr)
        for err in syntax_errors:
            print("  {:s}".format(err), file=sys.stderr)
        return 1

    semantic_errors = validate_stubs_semantically(stubs_dir, module_names)
    if semantic_errors:
        print("\nStub semantic errors:", file=sys.stderr)
        for err in semantic_errors:
            print("  {:s}".format(err), file=sys.stderr)
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
        help="Show per-stub progress.",
    )
    args = parser.parse_args()

    stubs_dir = Path(args.stubs_dir)
    if not stubs_dir.is_dir():
        print("Error: stubs directory not found: {:s}".format(str(stubs_dir)), file=sys.stderr)
        return 1

    stub_paths, module_names = discover_stubs(stubs_dir)
    if not stub_paths:
        print("Error: no .pyi files found under {:s}".format(str(stubs_dir)), file=sys.stderr)
        return 1

    return validate_stubs(stubs_dir, stub_paths, module_names, verbose=args.verbose)


if __name__ == "__main__":
    sys.exit(main())
