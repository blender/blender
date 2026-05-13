# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# ./blender.bin -b -X --python tests/python/bl_pyapi_doc_gen.py -- --verbose

"""
This test involves the following files:
- ``./doc/python_api/sphinx_doc_gen.py`` (generate ``*.rst``).
- ``./doc/python_api/sphinx_stub_gen.py`` (generate ``*.pyi`` stubs).
- ``./doc/python_api/sphinx_stub_validate.py`` (additional validation on the stubs).

Note that issues may be caused by doc-strings which can be declared in both C++ & Python API's.

To troubleshoot any issues this script can be run directly,
pointing to an output directory there the generated content wont be removed
as it would otherwise be when run from CTest.
"""

__all__ = (
    "main",
)

import argparse
import contextlib
import glob
import os
import subprocess
import sys
import tempfile
import unittest
from collections.abc import Iterator
from pathlib import Path
from typing import NamedTuple


# This file lives at: `./tests/python/bl_pyapi_doc_gen.py`.
SOURCE_ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SPHINX_DOC_GEN_DIR = os.path.join(SOURCE_ROOT, "doc", "python_api")

PYTHON_BIN = os.environ.get("PYTHON_BIN") or sys.executable


# ----------------------------------------------------------------------------
# Global State

class Global(NamedTuple):
    # When set (via `--output`) the test writes its output here instead of a
    # temporary directory, and the contents are NOT removed afterwards. Useful
    # for troubleshooting invalid output.
    user_output_dir: Path | None
    # When True (via `--skip-stubs`) the `sphinx_stub_gen` sub-test is skipped.
    skip_stubs: bool


# Module-level singleton; assigned exactly once in `main()`.
GLOBAL: Global


# ----------------------------------------------------------------------------
# Utilities

@contextlib.contextmanager
def output_dir_context() -> Iterator[Path]:
    if GLOBAL.user_output_dir is None:
        with tempfile.TemporaryDirectory() as tmpdir:
            yield Path(tmpdir)
    else:
        GLOBAL.user_output_dir.mkdir(parents=True, exist_ok=True)
        yield GLOBAL.user_output_dir


# ----------------------------------------------------------------------------
# Tests

class DocGenTest(unittest.TestCase):

    def test_doc_gen_and_stubs(self) -> None:
        with output_dir_context() as output_dir:
            rst_dir = os.path.join(output_dir, "sphinx-in")
            stubs_dir = os.path.join(output_dir, "stubs")

            doc_gen_ok = False
            with self.subTest("sphinx_doc_gen"):
                sys.path.insert(0, SPHINX_DOC_GEN_DIR)
                try:
                    import sphinx_doc_gen  # type: ignore[import-not-found]
                    rc = sphinx_doc_gen.main(argv=["--output", str(output_dir)])
                finally:
                    sys.path.remove(SPHINX_DOC_GEN_DIR)

                self.assertEqual(0, rc, "sphinx_doc_gen.main() returned non-zero")
                self.assertTrue(os.path.isdir(rst_dir), "missing directory: {:s}".format(rst_dir))
                rst_files = glob.glob(os.path.join(rst_dir, "*.rst"))
                self.assertTrue(rst_files, "no RST files generated in {:s}".format(rst_dir))
                doc_gen_ok = True

            if not doc_gen_ok:
                return

            if GLOBAL.skip_stubs:
                self.skipTest("--skip-stubs given, skipping sphinx_stub_gen sub-test")

            with self.subTest("sphinx_stub_gen"):
                stub_gen_script = os.path.join(SPHINX_DOC_GEN_DIR, "sphinx_stub_gen.py")
                result = subprocess.run(
                    [PYTHON_BIN, stub_gen_script, rst_dir, "-o", stubs_dir, "--strict-docs"],
                    check=False,
                )
                self.assertEqual(
                    0, result.returncode,
                    "sphinx_stub_gen.py failed (returncode={:d})".format(result.returncode),
                )
                self.assertTrue(os.path.isdir(stubs_dir), "missing directory: {:s}".format(stubs_dir))
                pyi_files = glob.glob(os.path.join(stubs_dir, "**", "*.pyi"), recursive=True)
                self.assertTrue(pyi_files, "no .pyi files generated in {:s}".format(stubs_dir))


# ----------------------------------------------------------------------------
# Main Function

def main() -> None:
    extra_argv = []
    if "--" in sys.argv:
        extra_argv = sys.argv[sys.argv.index("--") + 1:]

    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help=(
            "Output directory for generated files. "
            "When set, the directory is created if missing and its contents are NOT removed "
            "after the test runs - useful for troubleshooting invalid output."
        ),
    )
    parser.add_argument(
        "--skip-stubs",
        action="store_true",
        help="Skip the sphinx_stub_gen sub-test (only generate RST).",
    )
    args, unittest_argv = parser.parse_known_args(extra_argv)

    global GLOBAL
    GLOBAL = Global(
        user_output_dir=args.output,
        skip_stubs=args.skip_stubs,
    )

    unittest.main(argv=[sys.argv[0]] + unittest_argv)


if __name__ == "__main__":
    main()
