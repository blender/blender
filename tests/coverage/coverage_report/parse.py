# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import concurrent.futures
import json
import multiprocessing
import os
import random
import shutil
import subprocess
import sys
import textwrap
import time
import zipfile

from collections import defaultdict
from pathlib import Path
from pprint import pprint

from .util import print_updateable_line


def parse(build_dir, analysis_dir, gcov_binary="gcov"):
    """
    Parses coverage data generated in the given directory, merges it, and stores
    result in the analysis directory.
    """

    build_dir = Path(build_dir).absolute()
    analysis_dir = Path(analysis_dir).absolute()
    gcov_path = get_gcov_path(gcov_binary)

    if gcov_path is None or not gcov_path.exists():
        raise RuntimeError("Gcov not found.")

    gcda_paths = gather_gcda_files(build_dir)
    if len(gcda_paths) == 0:
        raise RuntimeError(
            textwrap.dedent(
                """\
            No .gcda files found. Make sure to run the tests in a debug build that has
            been compiled with GCC with --coverage.
            """
            )
        )

    # Invoke gcov many times in parallel to get the data in json format.
    gcov_outputs = parse_gcda_files_with_gcov(gcda_paths, gcov_path)

    gcov_by_source_file = collect_data_per_file(gcov_outputs)
    if len(gcov_by_source_file) == 0:
        raise RuntimeError("No coverage data found.")

    # Sort files to make the progress report more useful.
    source_file_order = list(sorted(list(gcov_by_source_file.keys())))

    # Many object files may have collected data from the same source files.
    data_by_source_file = merge_coverage_data(gcov_by_source_file, source_file_order)

    # Generate summary for each file.
    summary = compute_summary(data_by_source_file, source_file_order)

    clear_old_analysis_on_disk(analysis_dir)
    write_analysis_to_disk(analysis_dir, summary, data_by_source_file, source_file_order)


def get_gcov_path(gcov_binary):
    if not Path(gcov_binary).is_file():
        if gcov_path := shutil.which(gcov_binary):
            return Path(gcov_path)
        return None
    return Path(gcov_binary).absolute()


def gather_gcda_files(build_dir):
    print("Gather .gcda files...")
    gcda_paths = []
    for gcda_path in build_dir.glob("**/*.gcda"):
        gcda_paths.append(gcda_path)
        print_updateable_line("[{}]: {}".format(len(gcda_paths), gcda_path))
    print()
    return gcda_paths


def parse_gcda_files_with_gcov(gcda_paths, gcov_path):
    # Shuffle to make chunks more similar in size.
    random.shuffle(gcda_paths)

    # Gcov can process multiple files in a single invocation. So split all the tasks into chunks
    # to reduce the total number of required gcov invocations. The chunks should not be too large
    # because then multi-threading is less useful.
    chunk_size = 10
    gcda_path_chunks = [gcda_paths[i: i + chunk_size] for i in range(0, len(gcda_paths), chunk_size)]

    def parse_with_gcov(file_paths):
        return subprocess.check_output([gcov_path, "--stdout", "--json-format", *file_paths])

    print("Parse files...")
    print_updateable_line("[0/{}] parsed.".format(len(gcda_paths)))
    gcov_outputs = []

    # Use multi-threading instead of multi-processing here because the actual work is actually done
    # in separate gcov processes which run in parallel. Every gcov process is managed by a separate
    # thread though. This does not seem strictly necessary, but was good enough and the easy.
    with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() * 2) as executor:
        futures = {executor.submit(parse_with_gcov, file_paths): file_paths for file_paths in gcda_path_chunks}

        done_count = 0
        for future in concurrent.futures.as_completed(futures):
            file_paths = futures[future]
            done_count += len(file_paths)
            try:
                # Gcov outputs a line for each file that it processed.
                for line in future.result().splitlines():
                    gcov_outputs.append(json.loads(line))
            except Exception as e:
                print("Error:", e)
            print_updateable_line("[{}/{}] parsed.".format(done_count, len(gcda_paths)))
    print()

    return gcov_outputs


def collect_data_per_file(gcov_outputs):
    gcov_by_source_file = defaultdict(list)
    for data in gcov_outputs:
        for file_data in data["files"]:
            gcov_by_source_file[file_data["file"]].append(file_data)
    return gcov_by_source_file


def merge_coverage_data(gcov_by_source_file, source_file_order):
    print("Merge coverage data...")

    data_by_source_file = {}
    for i, file_path in enumerate(source_file_order):
        print_updateable_line("[{}/{}] merged: {}".format(i + 1, len(gcov_by_source_file), file_path))

        # For templated code, many functions may be generated for the same function in the source code.
        # Often we want to merge data from these individual instantiations together though. It's hard
        # to find the functions that belong together based on the name. However, we can use the source
        # code location as a value that's common for all instantiations of the same function.
        function_by_location = {}

        # Sometimes lines don't have function information. Not sure what the exact rules are here.
        # I found that this is sometimes the case for inline functions.
        loose_lines = defaultdict(int)

        # Maps a name of a specific function instantiation to it's source code location.
        location_key_by_mangled_name = {}

        # See the `--json-format` documentation to understand the input data:
        # https://gcc.gnu.org/onlinedocs/gcc/Invoking-Gcov.html
        for gcov_data in gcov_by_source_file[file_path]:
            for gcov_function in gcov_data["functions"]:
                start_line = gcov_line_number_to_index(gcov_function["start_line"])
                end_line = gcov_line_number_to_index(gcov_function["end_line"])
                start_column = gcov_function["start_column"]
                end_column = gcov_function["end_column"]

                # Build an identifier for the function that is common among all template instantiations.
                location_key = "{}:{}-{}:{}".format(start_line, start_column, end_line, end_column)

                if location_key not in function_by_location:
                    function_by_location[location_key] = {
                        "start_line": start_line,
                        "end_line": end_line,
                        "start_column": start_column,
                        "end_column": end_column,
                        "execution_count": 0,
                        "instantiations": {},
                        "lines": defaultdict(int),
                    }

                mangled_name = gcov_function["name"]
                demangled_name = gcov_function["demangled_name"]
                execution_count = gcov_function["execution_count"]

                location_key_by_mangled_name[mangled_name] = location_key

                function = function_by_location[location_key]
                function["execution_count"] += execution_count
                if mangled_name not in function["instantiations"]:
                    function["instantiations"][mangled_name] = {
                        "demangled": demangled_name,
                        "execution_count": 0,
                        "lines": defaultdict(int),
                    }
                function["instantiations"][mangled_name]["execution_count"] += execution_count

            for gcov_line in gcov_data["lines"]:
                line_index = gcov_line_number_to_index(gcov_line["line_number"])
                count = gcov_line["count"]
                mangled_name = gcov_line.get("function_name")
                if mangled_name is None:
                    loose_lines[line_index] += gcov_line["count"]
                else:
                    location_key = location_key_by_mangled_name[mangled_name]
                    function = function_by_location[location_key]
                    function["lines"][line_index] += count
                    instantiation = function["instantiations"][mangled_name]
                    instantiation["lines"][line_index] += count

        data_by_source_file[file_path] = {
            "file": file_path,
            "functions": function_by_location,
            "loose_lines": loose_lines,
        }
    print()

    return data_by_source_file


def compute_summary(data_by_source_file, source_file_order):
    print("Compute summaries...")
    summary_by_source_file = {}
    for i, file_path in enumerate(source_file_order):
        data = data_by_source_file[file_path]
        print_updateable_line("[{}/{}] written: {}".format(i + 1, len(data_by_source_file), file_path))

        num_instantiated_lines = 0
        num_instantiated_lines_run = 0
        num_instantiated_functions = 0
        num_instantiated_functions_run = 0

        all_lines = set()
        run_lines = set()
        all_function_keys = set()
        run_function_keys = set()

        for function_key, fdata in data["functions"].items():
            all_function_keys.add(function_key)
            if fdata["execution_count"] > 0:
                run_function_keys.add(function_key)

            for line_index, execution_count in fdata["lines"].items():
                all_lines.add(line_index)
                if execution_count > 0:
                    run_lines.add(line_index)

            for _function_name, instantiation_fdata in fdata["instantiations"].items():
                num_instantiated_functions += 1
                if instantiation_fdata["execution_count"] > 0:
                    num_instantiated_functions_run += 1
                for line_index, execution_count in instantiation_fdata["lines"].items():
                    num_instantiated_lines += 1
                    if execution_count > 0:
                        num_instantiated_lines_run += 1

        for line_index, execution_count in data["loose_lines"].items():
            num_instantiated_lines += 1
            all_lines.add(line_index)
            if execution_count > 0:
                num_instantiated_lines_run += 1
                run_lines.add(line_index)

        summary_by_source_file[file_path] = {
            "num_instantiated_lines": num_instantiated_lines,
            "num_instantiated_lines_run": num_instantiated_lines_run,
            "num_instantiated_functions": num_instantiated_functions,
            "num_instantiated_functions_run": num_instantiated_functions_run,
            "num_lines": len(all_lines),
            "num_lines_run": len(run_lines),
            "num_functions": len(all_function_keys),
            "num_functions_run": len(run_function_keys),
        }

    print()

    summary = {
        "files": summary_by_source_file,
    }

    return summary


def clear_old_analysis_on_disk(analysis_dir):
    print("Clear old analysis...")
    try:
        shutil.rmtree(analysis_dir)
    except:
        pass


def write_analysis_to_disk(analysis_dir, summary, data_by_source_file, source_file_order):
    print("Write summary...")
    write_dict_to_zip_file(analysis_dir / "summary.json.zip", summary)

    print("Write per file analysis...")
    for i, file_path in enumerate(source_file_order):
        analysis_file_path = analysis_dir / "files" / Path(file_path).relative_to("/")
        analysis_file_path = str(analysis_file_path) + ".json.zip"

        data = data_by_source_file[file_path]
        print_updateable_line("[{}/{}] written: {}".format(i + 1, len(data_by_source_file), analysis_file_path))
        write_dict_to_zip_file(analysis_file_path, data)
    print()
    print("Parsed data written to {}.".format(analysis_dir))


def gcov_line_number_to_index(line_number):
    # Gcov starts counting lines at 1.
    return line_number - 1


def write_dict_to_zip_file(zip_file_path, data):
    zip_file_path = Path(zip_file_path)
    zip_file_path.parent.mkdir(parents=True, exist_ok=True)
    # Was way faster to serialize first before writing to the file instead of using json.dump.
    data_str = json.dumps(data)

    name = zip_file_path.with_suffix("").name
    with zipfile.ZipFile(zip_file_path, "w", compression=zipfile.ZIP_DEFLATED) as f:
        f.writestr(name, data_str)
