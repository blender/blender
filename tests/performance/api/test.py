# SPDX-License-Identifier: Apache-2.0

import abc
import fnmatch
from typing import Dict, List


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

    @abc.abstractmethod
    def run(self, env, device_id: str) -> Dict:
        """
        Execute the test and report results.
        """


class TestCollection:
    def __init__(self, env, names_filter: List = None, categories_filter: List = None):
        if names_filter is None:
            names_filter = ["*"]
        if categories_filter is None:
            categories_filter = ["*"]
        import importlib
        import pkgutil
        import tests

        self.tests = []

        # Find and import all Python files in the tests folder, and generate
        # the list of tests for each.
        for _, modname, _ in pkgutil.iter_modules(tests.__path__, "tests."):
            module = importlib.import_module(modname)
            tests = module.generate(env)

            for test in tests:
                test_category = test.category()
                found = False
                for category_filter in categories_filter:
                    if fnmatch.fnmatch(test_category, category_filter):
                        found = True
                if not found:
                    continue

                test_name = test.name()
                found = False
                for name_filter in names_filter:
                    if fnmatch.fnmatch(test_name, name_filter):
                        found = True
                if not found:
                    continue

                self.tests.append(test)

    def find(self, test_name: str, test_category: str):
        # Find a test based on name and category.
        for test in self.tests:
            if test.name() == test_name and test.category() == test_category:
                return test

        return None
