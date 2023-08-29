# SPDX-FileCopyrightText: 2021-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import fnmatch
import json
import pathlib

from dataclasses import dataclass, field
from typing import Dict, List

from .test import TestCollection


def get_build_hash(args: None) -> str:
    import bpy
    build_hash = bpy.app.build_hash.decode('utf-8')
    return '' if build_hash == 'Unknown' else build_hash


@dataclass
class TestEntry:
    """Test to run, a combination of revision, test and device."""
    test: str = ''
    category: str = ''
    revision: str = ''
    git_hash: str = ''
    environment: Dict = field(default_factory=dict)
    executable: str = ''
    date: int = 0
    device_type: str = 'CPU'
    device_id: str = 'CPU'
    device_name: str = 'Unknown CPU'
    status: str = 'queued'
    error_msg: str = ''
    output: Dict = field(default_factory=dict)
    benchmark_type: str = 'comparison'

    def to_json(self) -> Dict:
        json_dict = {}
        for field in self.__dataclass_fields__:
            json_dict[field] = getattr(self, field)
        return json_dict

    def from_json(self, json_dict):
        for field in self.__dataclass_fields__:
            if field in json_dict:
                setattr(self, field, json_dict[field])


class TestQueue:
    """Queue of tests to be run or inspected. Matches JSON file on disk."""

    def __init__(self, filepath: pathlib.Path):
        self.filepath = filepath
        self.has_multiple_revisions_to_build = False
        self.has_multiple_categories = False
        self.entries = []

        if self.filepath.is_file():
            with open(self.filepath, 'r') as f:
                json_entries = json.load(f)

            for json_entry in json_entries:
                entry = TestEntry()
                entry.from_json(json_entry)
                self.entries.append(entry)

    def rows(self, use_revision_columns: bool) -> List:
        # Generate rows of entries for printing and running.
        entries = sorted(
            self.entries,
            key=lambda entry: (
                entry.revision,
                entry.device_id,
                entry.category,
                entry.test,
            ))

        if not use_revision_columns:
            # One entry per row.
            return [[entry] for entry in entries]
        else:
            # Multiple revisions per row.
            rows = {}

            for entry in entries:
                key = (entry.device_id, entry.category, entry.test)
                if key in rows:
                    rows[key].append(entry)
                else:
                    rows[key] = [entry]

            return [value for _, value in sorted(rows.items())]

    def find(self, revision: str, test: str, category: str, device_id: str) -> Dict:
        for entry in self.entries:
            if entry.revision == revision and \
               entry.test == test and \
               entry.category == category and \
               entry.device_id == device_id:
                return entry

        return None

    def write(self) -> None:
        json_entries = [entry.to_json() for entry in self.entries]
        with open(self.filepath, 'w') as f:
            json.dump(json_entries, f, indent=2)


class TestConfig:
    """Test configuration, containing a subset of revisions, tests and devices."""

    def __init__(self, env, name: str):
        # Init configuration from config.py file.
        self.name = name
        self.base_dir = env.base_dir / name
        self.logs_dir = self.base_dir / 'logs'

        config = TestConfig._read_config_module(self.base_dir)
        self.tests = TestCollection(env,
                                    getattr(config, 'tests', ['*']),
                                    getattr(config, 'categories', ['*']))
        self.revisions = getattr(config, 'revisions', {})
        self.builds = getattr(config, 'builds', {})
        self.queue = TestQueue(self.base_dir / 'results.json')
        self.benchmark_type = getattr(config, 'benchmark_type', 'comparison')

        self.devices = []
        self._update_devices(env, getattr(config, 'devices', ['CPU']))

        self._update_queue(env)

    def revision_names(self) -> List:
        return sorted(list(self.revisions.keys()) + list(self.builds.keys()))

    def device_name(self, device_id: str) -> str:
        for device in self.devices:
            if device.id == device_id:
                return device.name

        return "Unknown"

    @staticmethod
    def write_default_config(env, config_dir: pathlib.Path) -> None:
        config_dir.mkdir(parents=True, exist_ok=True)

        default_config = """devices = ['CPU']\n"""
        default_config += """tests = ['*']\n"""
        default_config += """categories = ['*']\n"""
        default_config += """builds = {\n"""
        default_config += """    'main': '/home/user/blender-git/build/bin/blender',"""
        default_config += """    '2.93': '/home/user/blender-2.93/blender',"""
        default_config += """}\n"""
        default_config += """revisions = {\n"""
        default_config += """}\n"""

        config_file = config_dir / 'config.py'
        with open(config_file, 'w') as f:
            f.write(default_config)

    @staticmethod
    def read_blender_executables(env, name) -> List:
        config = TestConfig._read_config_module(env.base_dir / name)
        builds = getattr(config, 'builds', {})
        executables = []

        for executable in builds.values():
            executable, _ = TestConfig._split_environment_variables(executable)
            executables.append(pathlib.Path(executable))

        return executables

    @staticmethod
    def _read_config_module(base_dir: pathlib.Path) -> None:
        # Import config.py as a module.
        import importlib.util
        spec = importlib.util.spec_from_file_location("testconfig", base_dir / 'config.py')
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod

    def _update_devices(self, env, device_filters: List) -> None:
        # Find devices matching the filters.
        need_gpus = device_filters != ['CPU']
        machine = env.get_machine(need_gpus)

        self.devices = []
        for device in machine.devices:
            for device_filter in device_filters:
                if fnmatch.fnmatch(device.id, device_filter):
                    self.devices.append(device)
                    break

    def _update_queue(self, env) -> None:
        # Update queue to match configuration, adding and removing entries
        # so that there is one entry for each revision, device and test
        # combination.
        entries = []

        # Get entries for specified commits, tags and branches.
        for revision_name, revision_commit in self.revisions.items():
            revision_commit, environment = self._split_environment_variables(revision_commit)
            git_hash = env.resolve_git_hash(revision_commit)
            date = env.git_hash_date(git_hash)
            entries += self._get_entries(revision_name, git_hash, '', environment, date)

        # Optimization to avoid rebuilds.
        revisions_to_build = set()
        for entry in entries:
            if entry.status in {'queued', 'outdated'}:
                revisions_to_build.add(entry.git_hash)
        self.queue.has_multiple_revisions_to_build = len(revisions_to_build) > 1

        # Get entries for revisions based on existing builds.
        for revision_name, executable in self.builds.items():
            executable, environment = self._split_environment_variables(executable)
            executable_path = env._blender_executable_from_path(pathlib.Path(executable))
            if not executable_path:
                import sys
                sys.stderr.write(f'Error: build {executable} not found\n')
                sys.exit(1)

            env.set_blender_executable(executable_path)
            git_hash, _ = env.run_in_blender(get_build_hash, {})
            env.set_default_blender_executable()

            mtime = executable_path.stat().st_mtime
            entries += self._get_entries(revision_name, git_hash, executable, environment, mtime)

        # Detect number of categories for more compact printing.
        categories = set()
        for entry in entries:
            categories.add(entry.category)
        self.queue.has_multiple_categories = len(categories) > 1

        # Replace actual entries.
        self.queue.entries = entries

    def _get_entries(self,
                     revision_name: str,
                     git_hash: str,
                     executable: pathlib.Path,
                     environment: str,
                     date: int) -> None:
        entries = []
        for test in self.tests.tests:
            test_name = test.name()
            test_category = test.category()

            for device in self.devices:
                entry = self.queue.find(revision_name, test_name, test_category, device.id)
                if entry:
                    # Test if revision hash or executable changed.
                    if entry.git_hash != git_hash or \
                       entry.executable != executable or \
                       entry.environment != environment or \
                       entry.benchmark_type != self.benchmark_type or \
                       entry.date != date:
                        # Update existing entry.
                        entry.git_hash = git_hash
                        entry.environment = environment
                        entry.executable = executable
                        entry.benchmark_type = self.benchmark_type
                        entry.date = date
                        if entry.status in {'done', 'failed'}:
                            entry.status = 'outdated'
                else:
                    # Add new entry if it did not exist yet.
                    entry = TestEntry(
                        revision=revision_name,
                        git_hash=git_hash,
                        executable=executable,
                        environment=environment,
                        date=date,
                        test=test_name,
                        category=test_category,
                        device_type=device.type,
                        device_id=device.id,
                        device_name=device.name,
                        benchmark_type=self.benchmark_type)
                entries.append(entry)

        return entries

    @staticmethod
    def _split_environment_variables(revision):
        if isinstance(revision, str):
            return revision, {}
        else:
            return revision[0], revision[1]
