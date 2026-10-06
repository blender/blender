#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0
"""
The main entry point to running benchmark tests.

See https://developer.blender.org/docs/handbook/testing/performance/
for a general introduction to the topic.
"""

import api
import argparse
import fnmatch
import glob
import logging
import pathlib
import shutil
import sys


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


# Fields that can be printed as result columns.
STATUS_FIELDS = ('revision', 'device')

# All fields that can determine row identity.
ROW_FIELDS = ('revision', 'category', 'device')

# Header name of the key column for each field.
STATUS_FIELD_NAMES = {
    'revision': 'Revision',
    'category': 'Category',
    'device': 'Device',
}


def parse_status_columns(columns_arg: str) -> list:
    # Parse and validate the comma-separated --columns argument.
    columns = [item.strip().lower() for item in (columns_arg or '').split(',') if item.strip()]

    invalid = [field for field in sorted(set(columns)) if field not in STATUS_FIELDS]
    if invalid:
        sys.stderr.write(
            f'Unknown field: {", ".join(invalid)} '
            f'(valid fields are: {", ".join(STATUS_FIELDS)})\n')
        sys.exit(1)

    duplicated = [field for field in sorted(set(columns)) if columns.count(field) > 1]
    if duplicated:
        sys.stderr.write(f'Field listed multiple times: {", ".join(duplicated)}\n')
        sys.exit(1)
    return columns


def field_value(entry: api.TestEntry, field: str) -> str:
    # Value of a configurable field for a test entry.
    if field == 'revision':
        return entry.revision
    if field == 'category':
        return entry.category
    return api.normalize_device_id(entry.device_id)


def field_values(config: api.TestConfig, entries: list, field: str) -> list:
    # Distinct values of a configurable field.
    if field == 'revision':
        return config.revision_names()
    return sorted({field_value(entry, field) for entry in entries})


def resolve_status_layout(config: api.TestConfig, columns: list) -> dict:
    # Reorientate the data for printing:
    # - fields in --columns become result columns, one per distinct value;
    # - fields not listed automatically become rows.
    if not columns:
        if use_revision_columns(config):
            dims = ['revision']
            left = []
        else:
            dims = []
            left = ['revision']
        if config.queue.has_multiple_categories:
            left.append('category')
        if config.queue.has_multiple_devices:
            left.append('device')
    else:
        dims = columns
        # Fields not listed automatically become rows; show them as key
        # columns when they have more than one value.
        left = []
        if len({field_value(entry, 'revision') for entry in config.queue.entries}) > 1:
            left.append('revision')
        if config.queue.has_multiple_categories:
            left.append('category')
        if config.queue.has_multiple_devices:
            left.append('device')
        left = [field for field in left if field not in columns]

    # One result column per combination of the values of the --columns fields.
    combos = [()]
    for field in dims:
        combos = [combo + (value,) for combo in combos
                  for value in field_values(config, config.queue.entries, field)]

    return {
        'default': not columns,
        'left': left,
        'dims': dims,
        'combos': combos,
    }


def status_rows(config: api.TestConfig, layout: dict) -> list:
    # Rows of entries for the resolved layout: one row per combination of
    # the fields that are not result columns.
    if layout['default']:
        return config.queue.rows(use_revision_columns(config))

    keys = [field for field in ROW_FIELDS if field not in layout['dims']]
    groups = {}
    for entry in config.queue.entries:
        key = tuple(field_value(entry, field) for field in keys) + (entry.test,)
        if key in groups:
            groups[key].append(entry)
        else:
            groups[key] = [entry]

    return [groups[key] for key in sorted(groups)]


def init_table(config: api.TestConfig, layout: dict = None) -> api.MarkdownTable:
    if layout is None:
        layout = resolve_status_layout(config, [])

    table = api.MarkdownTable()
    for field in layout['left']:
        table.add_column(STATUS_FIELD_NAMES[field])
    table.add_column("Test", width=40)
    if layout['dims']:
        for combo in layout['combos']:
            table.add_column(' / '.join(combo), width=20, alignment='RIGHT',
                             key=list(zip(layout['dims'], combo)))
    else:
        table.add_column("Result", width=20, alignment='RIGHT')
    return table


def entry_result(entry: api.TestEntry) -> str:
    # Format the result of a single test entry for printing.
    status = entry.status
    output = entry.output
    if status in {'done', 'outdated'} and output:
        if 'time' in output:
            result = '%7.4f s' % output['time']
        elif 'fps' in output:
            result = '%8.3f fps' % output['fps']
        else:
            result = ''
        if status == 'outdated':
            result += " (outdated)"
    elif status == 'failed':
        result = "failed: " + entry.error_msg
    else:
        result = status
    return result


def print_row(table: api.MarkdownTable, entries: list, end='\n') -> None:
    # Print one or more test entries on a row.
    row = []
    for column in table.columns:
        if column.key is None:
            # Key column: Revision, Category, Device, Test or Result.
            if column.name == 'Revision':
                row.append(entries[0].revision)
            elif column.name == 'Category':
                row.append(entries[0].category)
            elif column.name == 'Device':
                row.append(api.normalize_device_id(entries[0].device_id))
            elif column.name == 'Test':
                row.append(entries[0].test)
            else:
                row.append(entry_result(entries[0]))
        else:
            # Result column: the entry matching all (field, value) pairs of its key.
            entry = next((e for e in entries
                          if all(field_value(e, field) == value for field, value in column.key)), None)
            row.append(entry_result(entry) if entry else '')
    table.print_row(row, end=end)


def print_entry(table: api.MarkdownTable, entry: api.TestEntry) -> None:
    # Print a single test entry, potentially on multiple lines, with more details than in `print_row`.
    # NOTE: Currently only used to print detailed error info.

    print_row(table, [entry])

    if entry.status != 'failed':
        return
    if not entry.exception_msg:
        return
    print(entry.exception_msg, flush=True)


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
              table: api.MarkdownTable,
              row: list,
              entry: api.TestEntry,
              update_only: bool,
              count: int,
              update_submodules: bool = True):
    updated = False
    failed = False

    # Check if entry needs to be run.
    if update_only and entry.status not in {'queued', 'outdated'}:
        print_row(table, row, end='\r')
        return updated, failed

    # Run test entry.
    revision = entry.revision
    git_hash = entry.git_hash
    environment = entry.environment
    testname = entry.test
    testcategory = entry.category
    device_type = entry.device_type
    device_id = entry.device_id

    gpu_backend = {
        'VULKAN': 'vulkan',
        'METAL': 'metal',
        'OPENGL': 'opengl'
    }.get(device_type, 'default')

    test = config.tests.find(testname, testcategory)
    if not test:
        return updated, failed

    updated = True

    # Log all output to dedicated log file.
    logname = testcategory + '_' + testname + '_' + device_id + '_' + revision
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
        print_row(table, row, end='\r')

        if config.benchmark_type == "comparison":
            install_dir = config.builds_dir / revision
        else:
            install_dir = env.install_dir
        executable_ok = env.build(git_hash, install_dir, update_submodules)

        if not executable_ok:
            entry.status = 'failed'
            entry.error_msg = 'Failed to build'
            failed = True
        else:
            env.set_blender_executable(install_dir, environment)

    # Run test and update output and status.
    if executable_ok:
        run_outputs = []
        for run in range(count):
            entry.status = 'running' if count == 1 else f'run [{run + 1}/{count}]'
            print_row(table, row, end='\r')

            try:
                output = test.run(env, device_id, gpu_backend)
                if not output:
                    raise Exception("Test produced no output")
                run_outputs.append(output)
                entry.status = 'done'
            except KeyboardInterrupt as e:
                raise e
            except Exception as e:
                failed = True
                entry.status = 'failed'
                entry.error_msg = 'Failed to run'
                entry.exception_msg = str(e)
                break

        if entry.status == 'done' and run_outputs:
            # Combine results from runs

            keys = set()
            for run_output in run_outputs:
                keys |= run_output.keys()

            output = {}
            output_all_runs = {}
            for key in keys:
                values = []
                for run_output in run_outputs:
                    if key not in run_output:
                        continue
                    values.append(run_output[key])
                output[key] = sum(values) / len(values)
                output_all_runs[key] = values
            entry.output = output
            entry.output_all_runs = output_all_runs

    print_row(table, row, end='\r')

    # Update device name in case the device changed since the entry was created.
    entry.device_name = config.device_name(device_id)

    # Restore default logging and Blender executable.
    env.unset_log_file()
    env.set_default_blender_executable()

    return updated, failed


def cmd_init(env: api.TestEnvironment, argv: list):
    # Initialize benchmarks folder.
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', default=False, action='store_true')
    parser.add_argument('--blender')
    args = parser.parse_args(argv)
    env.set_log_file(env.base_dir / 'setup.log', clear=False)
    env.init(args.build, args.blender)
    env.unset_log_file()


def cmd_list(env: api.TestEnvironment, argv: list) -> None:
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


def cmd_status(env: api.TestEnvironment, argv: list):
    # Print status of tests in configurations.
    parser = argparse.ArgumentParser()
    parser.add_argument('config', nargs='?', default=None)
    parser.add_argument('test', nargs='?', default='*')
    parser.add_argument(
        '--columns',
        default=None,
        help="Fields to reorientate into result columns, comma-separated (valid fields: device, revision).")
    args = parser.parse_args(argv)

    columns = parse_status_columns(args.columns)

    configs = env.get_configs(args.config)
    first = True
    for config in configs:
        layout = resolve_status_layout(config, columns)
        if not args.config:
            if first:
                first = False
            else:
                print("")
            print(config.name.upper())

        table = init_table(config, layout)
        table.print_header()
        for row in status_rows(config, layout):
            if match_entry(row[0], args):
                print_row(table, row)


def cmd_reset(env: api.TestEnvironment, argv: list):
    # Reset tests to re-run them.
    parser = argparse.ArgumentParser()
    parser.add_argument('config', nargs='?', default=None)
    parser.add_argument('test', nargs='?', default='*')
    args = parser.parse_args(argv)

    configs = env.get_configs(args.config)
    for config in configs:
        table = init_table(config)
        table.print_header()
        for row in config.queue.rows(use_revision_columns(config)):
            if match_entry(row[0], args):
                for entry in row:
                    entry.status = 'queued'
                    entry.result = {}
                print_row(table, row)

        config.queue.write()

        if args.test == '*':
            shutil.rmtree(config.logs_dir)


def cmd_build(env: api.TestEnvironment, argv: list):
    # Build all revisions, skipping the ones that are already up to date.
    parser = argparse.ArgumentParser()
    parser.add_argument('config', nargs='?', default=None)
    parser.add_argument(
        '--no-submodules',
        action='store_true',
        help="Skip updating submodules when checking out revisions. Useful when testing performance regressions for library changes.")
    args = parser.parse_args(argv)

    update_submodules = not args.no_submodules

    configs = env.get_configs(args.config)
    for config in configs:
        if config.benchmark_type != "comparison":
            print(f"{config.name}: build not supported, this command can only be used with comparison benchmarks")
            continue

        # Collect revisions that need building: entries without a pre-built executable.
        revisions = {}
        for entry in config.queue.entries:
            if len(entry.executable) == 0 and entry.revision not in revisions:
                revisions[entry.revision] = entry.git_hash

        if not revisions:
            continue

        print(config.name.upper())
        for revision in sorted(revisions.keys()):
            git_hash = revisions[revision]
            install_dir = config.builds_dir / revision

            logname = revision + '_build'
            env.set_log_file(config.logs_dir / (logname + '.log'), clear=True)
            print(f"{revision} building")
            env.echo_output = True
            try:
                ok = env.build(git_hash, install_dir, update_submodules)
            except SystemExit:
                # e.g. build directory not initialized, treat as a failed build.
                ok = False
            finally:
                env.echo_output = False
                env.unset_log_file()

            if ok:
                print(f"{revision} done")
            else:
                print(f"{revision} failed")
                sys.exit(1)


def cmd_run(env: api.TestEnvironment, argv: list, update_only: bool):
    # Run tests.
    parser = argparse.ArgumentParser()
    parser.add_argument('config', nargs='?', default=None)
    parser.add_argument('test', nargs='?', default='*')
    parser.add_argument('--count', default=1, type=int, help="Number of runs to perform (default=1)")
    parser.add_argument(
        '--no-submodules',
        action='store_true',
        help="Skip updating submodules when checking out revisions. Useful when testing performance regressions for library changes.")
    args = parser.parse_args(argv)

    exit_code = 0

    configs = env.get_configs(args.config)
    for config in configs:
        updated = False
        cancel = False
        table = init_table(config)
        table.print_header()
        for row in config.queue.rows(use_revision_columns(config)):
            if match_entry(row[0], args):
                for entry in row:
                    try:
                        test_updated, test_failed = run_entry(
                            env, config, table, row, entry, update_only, args.count, not args.no_submodules)
                        if test_updated:
                            updated = True
                            # Write queue every time in case running gets interrupted,
                            # so it can be resumed.
                            config.queue.write()
                        if test_failed:
                            exit_code = 1
                            print_entry(table, entry)
                    except KeyboardInterrupt as e:
                        cancel = True
                        break

                print_row(table, row)

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


def cmd_bisect(env: api.TestEnvironment, argv: list):
    import datetime
    SECONDS_PER_DAY = 86400

    parser = argparse.ArgumentParser(prog='benchmark.py bisect')
    parser.add_argument('--device', required=True,
                        help='Device type or ID to run tests on')
    parser.add_argument('--category', required=True,
                        help='Test category (e.g. eevee, cycles)')
    parser.add_argument('--test', required=True,
                        help='Test name (supports glob patterns)')
    parser.add_argument('--attribute', required=True,
                        help='Performance attribute to compare (e.g. fps, time)')
    parser.add_argument('--threshold', required=True, type=float,
                        help='Threshold value for pass/fail decision')
    parser.add_argument('--success', required=True, choices=['greater_than', 'less_than'],
                        help='Whether higher or lower values are considered a success')
    parser.add_argument('--range', required=True,
                        help='Date range in YYYYMMDD-YYYYMMDD format')
    parser.add_argument('--count', default=1, type=int,
                        help='Number of benchmark runs per commit (default=1)')
    args = parser.parse_args(argv)

    if not env.build_dir.exists() or not env.blender_dir.exists():
        sys.stderr.write('Error: benchmark build not initialized. Run "benchmark.py init --build" first.\n')
        sys.exit(1)

    try:
        start_str, end_str = args.range.split('-')
        start_dt = datetime.datetime.strptime(start_str, '%Y%m%d').replace(tzinfo=datetime.timezone.utc)
        end_dt = datetime.datetime.strptime(end_str, '%Y%m%d').replace(tzinfo=datetime.timezone.utc)
    except:
        sys.stderr.write('Error: invalid date range format. Use YYYYMMDD-YYYYMMDD\n')
        sys.exit(1)
    if start_dt >= end_dt:
        sys.stderr.write(f'Error: invalid date range {start_str} must be before {end_str}\n')
        sys.exit(1)

    collection = api.TestCollection(env, [args.test], [args.category])
    test = collection.find(args.test, args.category)
    if not test:
        sys.stderr.write(f'Error: test not found: {args.category}/{args.test}\n')
        sys.exit(1)

    device_id, gpu_backend = env.resolve_device(args.device)

    print(f"Device: {args.device}")
    print(f"Category: {args.category}")
    print(f"Test: {args.test}")
    print()

    table = api.MarkdownTable()
    table.add_column("Remaining", width=5, alignment='RIGHT')
    table.add_column("Commit", width=14)
    table.add_column("Date (UTC)", width=22)
    table.add_column("Title", width=72)
    table.add_column(args.attribute, width=14, alignment='RIGHT')
    table.add_column("Status", width=8)
    table.print_header()

    tested = set()

    def print_status(row_values, end='\n'):
        table.print_row([str(progress.remaining)] + row_values, end=end)

    def run_commit_wrapper(commit_hash, commit_ts):
        return api.Bisect.run_commit(
            env, test, device_id, gpu_backend, args.count, args.attribute,
            args.success, args.threshold, tested,
            print_status, commit_hash, commit_ts)

    # Phase 1: Daily scan
    start_ts = int(start_dt.timestamp())
    end_ts = int(end_dt.timestamp()) + SECONDS_PER_DAY

    progress = api.bisect.BisectProgress()
    env.set_log_file(env.base_dir / 'bisect.log', clear=True)
    bisect = api.bisect.Bisect(env, run_commit_wrapper, start_ts, end_ts)
    bisect.run(progress=progress)
    env.unset_log_file()

    if bisect.first_bad is None:
        print('\nNo regression found in the given date range.')
        return

    title = env.commit_title(bisect.first_bad).replace('`', '\'')
    print(f'\nRegression introduced by commit `{bisect.first_bad}`: `{title}`')


def cmd_graph(argv: list):
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
    logging.basicConfig()
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
             '  build [<config>]                     Build revisions (skips the up to date ones)\n'
             '  reset [<config>] [<test>]            Clear tests results in configuration\n'
             '  status [<config>] [<test>]           List configurations and their tests\n'
             '  \n'
             '  graph a.json b.json... -o out.html   Create graph from results in JSON files\n'
             '  \n'
             '  bisect                                Find commit that introduced a regression'
             ' between dates\n')

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
        sys.stderr.write(
            'Error: benchmark directory not initialized. '
            'Run the \"init\" command to create the directory and a default configuration.\n')
        sys.exit(1)

    if args.command == 'list':
        cmd_list(env, argv)
    elif args.command == 'run':
        cmd_run(env, argv, update_only=False)
    elif args.command == 'update':
        cmd_run(env, argv, update_only=True)
    elif args.command == 'build':
        cmd_build(env, argv)
    elif args.command == 'reset':
        cmd_reset(env, argv)
    elif args.command == 'bisect':
        cmd_bisect(env, argv)
    elif args.command == 'status':
        cmd_status(env, argv)
    elif args.command == 'help':
        parser.print_usage()
    else:
        sys.stderr.write(f'Unknown command: {args.command}\n')


if __name__ == '__main__':
    main()
