# SPDX-FileCopyrightText: 2021-2022 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import abc
import fnmatch


class Test:
    @abc.abstractmethod
    def name(self) -> str:
        """
        Name of the test.
        """

    @abc.abstractmethod
    def category(self) -> str:
        """
        Category of the test.
        """

    def use_device(self) -> bool:
        """
        Test uses a specific CPU or GPU device.
        """
        return False

    def use_background(self) -> bool:
        """
        Test runs in background mode and requires no display.
        """
        return True

    @abc.abstractmethod
    def run(self, env, device_id: str) -> dict:
        """
        Execute the test and report results.
        """


class TestCollection:
    def __init__(self, env, names_filter: list = ['*'], categories_filter: list = ['*'], background: bool = False):
        import importlib
        import pkgutil
        import tests

        self.tests = []

        # Find and import all Python files in the tests folder, and generate
        # the list of tests for each.
        for _, modname, _ in pkgutil.iter_modules(tests.__path__, 'tests.'):
            module = importlib.import_module(modname)
            tests = module.generate(env)

            for test in tests:
                if background and not test.use_background():
                    continue

                test_category = test.category()
                found = False
                for category_filter in categories_filter:
                    if fnmatch.fnmatch(test_category, category_filter):
                        found = True
                if not found:
                    continue

                test_name = test.name()

                included = False
                excluded = False

                for name_filter in names_filter:
                    is_exclusion = name_filter.startswith('!')
                    pattern = name_filter[1:] if is_exclusion else name_filter

                    if fnmatch.fnmatch(test_name, pattern):
                        if is_exclusion:
                            excluded = True
                            break
                        else:
                            included = True

                if not included or excluded:
                    continue

                self.tests.append(test)

    def find(self, test_name: str, test_category: str):
        # Find a test based on name and category.
        for test in self.tests:
            if test.name() == test_name and test.category() == test_category:
                return test

        return None
