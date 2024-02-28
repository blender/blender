#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import api
import argparse
import fnmatch
import glob
import pathlib
import shutil
import sys
from typing import List


def find_blender_git_dir() -> pathlib.Path:
    # Find .git directory of the repository we are in.
    cwd = pathlib.Path.cwd()

    for path in [cwd] + list(cwd.parents):
        if (path / '.git').exists():
            return path

    return None


def get_tests_base_dir(blender_git_dir: pathlib.Path) -> pathlib.Path:
    # Benchmarks dir is next to the Blender source folder.
    return blender_git_dir.parent / 'benchmark'


def use_revision_columns(config: api.TestConfig) -> bool:
    return (
        config.benchmark_type == "comparison" and
        len(config.queue.entries) > 0
    )


def print_header(config: api.TestConfig) -> None:
    # Print header with revision columns headers.
    if use_revision_columns(config):
        header = ""
        if config.queue.has_multiple_categories:
            header += f"{'': <15} "
        header += f"{'': <40} "

        for revision_name in config.revision_names():
            header += f"{revision_name: <20} "
        print(header)


def print_row(config: api.TestConfig, entries: List, end='\n') -> None:
    # Print one or more test entries on a row.
    row = ""

    # For time series, print revision first.
    if not use_revision_columns(config):
        revision = entries[0].revision
        git_hash = entries[0].git_hash

        row += f"{revision: <15} "

    if config.queue.has_multiple_categories:
        category_name = entries[0].category
        if entries[0].device_type != "CPU":
            category_name += " " + entries[0].device_type
        row += f"{category_name: <15} "
    row += f"{entries[0].test: <40} "

    for entry in entries:
        # Show time or status.
        status = entry.status
        output = entry.output
        result = ''
        if status in {'done', 'outdated'} and output:
            result = '%.4fs' % output['time']

            if status == 'outdated':
                result += " (outdated)"
        elif status == 'failed':
            result = "failed: " + entry.error_msg
        else:
            result = status

        row += f"{result: <20} "

    print(row, end=end, flush=True)


def match_entry(entry: api.TestEntry, args: argparse.Namespace):
    # Filter tests by name and category.
    return (
        fnmatch.fnmatch(entry.test, args.test) or
        fnmatch.fnmatch(entry.category, args.test) or
        entry.test.find(args.test) != -1 or
        entry.category.find(args.test) != -1
    )


def run_entry(env: api.TestEnvironment,
              config: api.TestConfig,
              row: List,
              entry: api.TestEntry,
              update_only: bool):
    updated = False
    failed = False

    # Check if entry needs to be run.
    if update_only and entry.status not in {'queued', 'outdated'}:
        print_row(config, row, end='\r')
        return updated, failed

    # Run test entry.
    revision = entry.revision
    git_hash = entry.git_hash
    environment = entry.environment
    testname = entry.test
    testcategory = entry.category
    device_type = entry.device_type
    device_id = entry.device_id

    test = config.tests.find(testname, testcategory)
    if not test:
        return updated, failed

    updated = True

    # Log all output to dedicated log file.
    logname = testcategory + '_' + testname + '_' + revision
    if device_id != 'CPU':
        logname += '_' + device_id
    env.set_log_file(config.logs_dir / (logname + '.log'), clear=True)

    # Clear output
    entry.output = None
    entry.error_msg = ''

    # Build revision, or just set path to existing executable.
    executable_ok = True
    if len(entry.executable):
        env.set_blender_executable(pathlib.Path(entry.executable), environment)
    else:
        entry.status = 'building'
        print_row(config, row, end='\r')

        if config.benchmark_type == "comparison":
            install_dir = config.builds_dir / revision
        else:
            install_dir = env.install_dir
        executable_ok = env.build(git_hash, install_dir)

        if not executable_ok:
            entry.status = 'failed'
            entry.error_msg = 'Failed to build'
            failed = True
        else:
            env.set_blender_executable(install_dir, environment)

    # Run test and update output and status.
    if executable_ok:
        entry.status = 'running'
        print_row(config, row, end='\r')

        try:
            entry.output = test.run(env, device_id)
            if not entry.output:
                raise Exception("Test produced no output")
            entry.status = 'done'
        except KeyboardInterrupt as e:
            raise e
        except Exception as e:
            failed = True
            entry.status = 'failed'
            entry.error_msg = str(e)

    print_row(config, row, end='\r')

    # Update device name in case the device changed since the entry was created.
    entry.device_name = config.device_name(device_id)

    # Restore default logging and Blender executable.
    env.unset_log_file()
    env.set_default_blender_executable()

    return updated, failed


def cmd_init(env: api.TestEnvironment, argv: List):
    # Initialize benchmarks folder.
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', default=False, action='store_true')
    args = parser.parse_args(argv)
    env.set_log_file(env.base_dir / 'setup.log', clear=False)
    env.init(args.build)
    env.unset_log_file()


def cmd_list(env: api.TestEnvironment, argv: List) -> None:
    # List devices, tests and configurations.
    print('DEVICES')
    machine = env.get_machine()
    for device in machine.devices:
        name = f"{device.name} ({device.operating_system})"
        print(f"{device.id: <15} {name}")
    print('')

    print('TESTS')
    collection = api.TestCollection(env)
    for test in collection.tests:
        print(f"{test.category(): <15} {test.name(): <50}")
    print('')

    print('CONFIGS')
    configs = env.get_config_names()
    for config_name in configs:
        print(config_name)


def cmd_status(env: api.TestEnvironment, argv: List):
    # Print status of tests in configurations.
    parser = argparse.ArgumentParser()
    parser.add_argument('config', nargs='?', default=None)
    parser.add_argument('test', nargs='?', default='*')
    args = parser.parse_args(argv)

    configs = env.get_configs(args.config)
    first = True
    for config in configs:
        if not args.config:
            if first:
                first = False
            else:
                print("")
            print(config.name.upper())

        print_header(config)
        for row in config.queue.rows(use_revision_columns(config)):
            if match_entry(row[0], args):
                print_row(config, row)


def cmd_reset(env: api.TestEnvironment, argv: List):
    # Reset tests to re-run them.
    parser = argparse.ArgumentParser()
    parser.add_argument('config', nargs='?', default=None)
    parser.add_argument('test', nargs='?', default='*')
    args = parser.parse_args(argv)

    configs = env.get_configs(args.config)
    for config in configs:
        print_header(config)
        for row in config.queue.rows(use_revision_columns(config)):
            if match_entry(row[0], args):
                for entry in row:
                    entry.status = 'queued'
                    entry.result = {}
                print_row(config, row)

        config.queue.write()

        if args.test == '*':
            shutil.rmtree(config.logs_dir)


def cmd_run(env: api.TestEnvironment, argv: List, update_only: bool):
    # Run tests.
    parser = argparse.ArgumentParser()
    parser.add_argument('config', nargs='?', default=None)
    parser.add_argument('test', nargs='?', default='*')
    args = parser.parse_args(argv)

    exit_code = 0

    configs = env.get_configs(args.config)
    for config in configs:
        updated = False
        cancel = False
        print_header(config)
        for row in config.queue.rows(use_revision_columns(config)):
            if match_entry(row[0], args):
                for entry in row:
                    try:
                        test_updated, test_failed = run_entry(env, config, row, entry, update_only)
                        if test_updated:
                            updated = True
                            # Write queue every time in case running gets interrupted,
                            # so it can be resumed.
                            config.queue.write()
                        if test_failed:
                            exit_code = 1
                    except KeyboardInterrupt as e:
                        cancel = True
                        break

                print_row(config, row)

            if cancel:
                break

        if updated:
            # Generate graph if test were run.
            json_filepath = config.base_dir / "results.json"
            html_filepath = config.base_dir / "results.html"
            graph = api.TestGraph([json_filepath])
            graph.write(html_filepath)

            print("\nfile://" + str(html_filepath))

    sys.exit(exit_code)


def cmd_graph(argv: List):
    # Create graph from a given JSON results file.
    parser = argparse.ArgumentParser()
    parser.add_argument('json_file', nargs='+')
    parser.add_argument('-o', '--output', type=str, required=True)
    args = parser.parse_args(argv)

    # For directories, use all json files in the directory.
    json_files = []
    for path in args.json_file:
        path = pathlib.Path(path)
        if path.is_dir():
            for filepath in glob.iglob(str(path / '*.json')):
                json_files.append(pathlib.Path(filepath))
        else:
            json_files.append(path)

    graph = api.TestGraph(json_files)
    graph.write(pathlib.Path(args.output))


def main():
    usage = ('benchmark <command> [<args>]\n'
             '\n'
             'Commands:\n'
             '  init [--build]                       Init benchmarks directory and default config\n'
             '                                       Optionally with automated revision building setup\n'
             '  \n'
             '  list                                 List available tests, devices and configurations\n'
             '  \n'
             '  run [<config>] [<test>]              Execute all tests in configuration\n'
             '  update [<config>] [<test>]           Execute only queued and outdated tests\n'
             '  reset [<config>] [<test>]            Clear tests results in configuration\n'
             '  status [<config>] [<test>]           List configurations and their tests\n'
             '  \n'
             '  graph a.json b.json... -o out.html   Create graph from results in JSON files\n')

    parser = argparse.ArgumentParser(
        description='Blender performance testing',
        usage=usage)

    parser.add_argument('command', nargs='?', default='help')
    args = parser.parse_args(sys.argv[1:2])

    argv = sys.argv[2:]
    blender_git_dir = find_blender_git_dir()
    if blender_git_dir is None:
        sys.stderr.write('Error: no blender git repository found from current working directory\n')
        sys.exit(1)

    if args.command == 'graph':
        cmd_graph(argv)
        sys.exit(0)

    base_dir = get_tests_base_dir(blender_git_dir)
    env = api.TestEnvironment(blender_git_dir, base_dir)
    if args.command == 'init':
        cmd_init(env, argv)
        sys.exit(0)

    if not env.base_dir.exists():
        sys.stderr.write('Error: benchmark directory not initialized\n')
        sys.exit(1)

    if args.command == 'list':
        cmd_list(env, argv)
    elif args.command == 'run':
        cmd_run(env, argv, update_only=False)
    elif args.command == 'update':
        cmd_run(env, argv, update_only=True)
    elif args.command == 'reset':
        cmd_reset(env, argv)
    elif args.command == 'status':
        cmd_status(env, argv)
    elif args.command == 'help':
        parser.print_usage()
    else:
        sys.stderr.write(f'Unknown command: {args.command}\n')


if __name__ == '__main__':
    main()
