# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This test calls into the path pattern matching logic of ``blender_ext`` directly.
"""
import unittest
import os

from typing import (
    Any,
    Dict,
    List,
    Sequence,
    Tuple,
    Union,
)


CURRENT_DIR = os.path.abspath(os.path.dirname(__file__))
BASE_DIR = os.path.normpath(os.path.join(CURRENT_DIR, ".."))


# Don't import as module, instead load the class.
def execfile(filepath: str, *, name: str = "__main__") -> Dict[str, Any]:
    global_namespace = {"__file__": filepath, "__name__": name}
    with open(filepath, encoding="utf-8") as file_handle:
        exec(compile(file_handle.read(), filepath, 'exec'), global_namespace)
    return global_namespace


PathPatternMatch = execfile(os.path.join(BASE_DIR, "cli", "blender_ext.py"), name="blender_ext")["PathPatternMatch"]
assert isinstance(PathPatternMatch, type)


class TestPathMatch_MixIn:

    def match_paths(
            self,
            expected_paths: List[Tuple[bool, str]],
            path_pattern: Union[Sequence[str], PathPatternMatch],  # type: ignore
    ) -> List[Tuple[bool, str]]:
        result = []
        if not isinstance(path_pattern, PathPatternMatch):
            path_pattern = PathPatternMatch(path_pattern)
        assert hasattr(path_pattern, "test_path")
        for success_expected, path in expected_paths:
            success_actual = path_pattern.test_path(path)
            if False:
                self.assertEqual(success_actual, success_expected)
            result.append((success_actual, path))
        return result

    def match_paths_for_cmp(
            self,
            expected_paths: List[Tuple[bool, str]],
            path_pattern: Union[Sequence[str], PathPatternMatch],  # type: ignore
    ) -> Tuple[
        List[Tuple[bool, str]],
        List[Tuple[bool, str]],
    ]:
        return self.match_paths(expected_paths, path_pattern), expected_paths


class TestSingle(unittest.TestCase, TestPathMatch_MixIn):

    def test_directory(self) -> None:
        pattern = PathPatternMatch(["__pycache__/"])
        self.assertEqual(*self.match_paths_for_cmp([(True, "__pycache__/")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(True, "a/__pycache__/")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(True, "a/__pycache__/any")], pattern))

        # Not a directory.
        self.assertEqual(*self.match_paths_for_cmp([(False, "__pycache__")], pattern))

        # Not exact matches.
        self.assertEqual(*self.match_paths_for_cmp([(False, "__pycache_/")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(False, "__pycache___/")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(False, "___pycache__/")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(False, "_pycache__/")], pattern))

        # Not exact matches (sub-directory).
        self.assertEqual(*self.match_paths_for_cmp([(False, "a/__pycache_/")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(False, "a/__pycache___/")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(False, "a/___pycache__/")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(False, "a/_pycache__/")], pattern))

    def test_file_extension(self) -> None:
        pattern = PathPatternMatch(["*.pyc"])
        self.assertEqual(*self.match_paths_for_cmp([(True, ".pyc")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(True, "a.pyc")], pattern))

        self.assertEqual(*self.match_paths_for_cmp([(True, "a/.pyc")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(True, "a/a.pyc")], pattern))

        # Not exact matches.
        self.assertEqual(*self.match_paths_for_cmp([(False, ".pycx")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(False, "a.pycx")], pattern))

        self.assertEqual(*self.match_paths_for_cmp([(False, ".pyc/x")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(False, "a.pyc/x")], pattern))

        # Not exact matches (sub-directory).
        self.assertEqual(*self.match_paths_for_cmp([(False, "a/.pycx")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(False, "a/a.pycx")], pattern))

        self.assertEqual(*self.match_paths_for_cmp([(False, "a/.pyc/x")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(False, "a/a.pyc/x")], pattern))

    def test_double_star(self) -> None:
        pattern = PathPatternMatch(["a/**/b"])
        self.assertEqual(*self.match_paths_for_cmp([(True, "a/x/b")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(True, "a/x/y/z/b")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(True, "a/b")], pattern))

        self.assertEqual(*self.match_paths_for_cmp([(False, "a_/x/b")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(False, "a_/x/y/z/b")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(False, "a_/b")], pattern))

        self.assertEqual(*self.match_paths_for_cmp([(False, "a/x/_b")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(False, "a/x/y/z/_b")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(False, "a/_b")], pattern))

    def test_double_star_directory(self) -> None:
        pattern = PathPatternMatch(["a/**/b/"])
        self.assertEqual(*self.match_paths_for_cmp([(True, "a/x/b/")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(True, "a/x/y/z/b/")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(True, "a/b/")], pattern))

    def test_root_directory(self) -> None:
        pattern = PathPatternMatch(["/build"])
        self.assertEqual(*self.match_paths_for_cmp([(True, "build/")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(True, "build")], pattern))

        # Not absolute.
        self.assertEqual(*self.match_paths_for_cmp([(False, "a/build/")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(False, "a/build")], pattern))

        self.assertEqual(*self.match_paths_for_cmp([(False, "build_/")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(False, "build_")], pattern))

    def test_root_directory_file(self) -> None:
        pattern = PathPatternMatch(["/a/build.txt"])
        self.assertEqual(*self.match_paths_for_cmp([(True, "a/build.txt")], pattern))

        # Not root relative.
        self.assertEqual(*self.match_paths_for_cmp([(False, "b/a/build.txt")], pattern))

    def test_root_directory_file_star(self) -> None:
        pattern = PathPatternMatch(["/test/*.txt"])
        self.assertEqual(*self.match_paths_for_cmp([(True, "test/a.txt")], pattern))

        self.assertEqual(*self.match_paths_for_cmp([(False, "test/b/c.txt")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(False, "b/test/c.txt")], pattern))


class TestMulti(unittest.TestCase, TestPathMatch_MixIn):

    def test_negate(self) -> None:
        pattern = PathPatternMatch([
            "data.*",
            "!data.csv",
        ])
        self.assertEqual(*self.match_paths_for_cmp([(True, "test/data.txt")], pattern))

        self.assertEqual(*self.match_paths_for_cmp([(False, "test/data.csv")], pattern))

    def test_negate_spesific(self) -> None:
        pattern = PathPatternMatch([
            "data.*",
            "!/test/data.csv",
        ])

        self.assertEqual(*self.match_paths_for_cmp([(True, "other/data.csv")], pattern))
        self.assertEqual(*self.match_paths_for_cmp([(True, "other/data.txt")], pattern))

        self.assertEqual(*self.match_paths_for_cmp([(False, "test/data.csv")], pattern))


if __name__ == "__main__":
    unittest.main()
