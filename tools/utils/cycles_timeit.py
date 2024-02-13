#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
import re
import shutil
import subprocess
import sys
import time


class COLORS:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'


VERBOSE = False

#########################################
# Generic helper functions.


def logVerbose(*args):
    if VERBOSE:
        print(*args)


def logHeader(*args):
    print(COLORS.HEADER + COLORS.BOLD, end="")
    print(*args, end="")
    print(COLORS.ENDC)


def logWarning(*args):
    print(COLORS.WARNING + COLORS.BOLD, end="")
    print(*args, end="")
    print(COLORS.ENDC)


def logOk(*args):
    print(COLORS.OKGREEN + COLORS.BOLD, end="")
    print(*args, end="")
    print(COLORS.ENDC)


def progress(count, total, prefix="", suffix=""):
    if VERBOSE:
        return

    size = shutil.get_terminal_size((80, 20))

    if prefix != "":
        prefix = prefix + "    "
    if suffix != "":
        suffix = "    " + suffix

    bar_len = size.columns - len(prefix) - len(suffix) - 10
    filled_len = int(round(bar_len * count / float(total)))

    percents = round(100.0 * count / float(total), 1)
    bar = '=' * filled_len + '-' * (bar_len - filled_len)

    sys.stdout.write('%s[%s] %s%%%s\r' % (prefix, bar, percents, suffix))
    sys.stdout.flush()


def progressClear():
    if VERBOSE:
        return

    size = shutil.get_terminal_size((80, 20))
    sys.stdout.write(" " * size.columns + "\r")
    sys.stdout.flush()


def humanReadableTimeDifference(seconds):
    hours = int(seconds) // 60 // 60
    seconds = seconds - hours * 60 * 60
    minutes = int(seconds) // 60
    seconds = seconds - minutes * 60
    if hours == 0:
        return "%02d:%05.2f" % (minutes, seconds)
    else:
        return "%02d:%02d:%05.2f" % (hours, minutes, seconds)


def humanReadableTimeToSeconds(time):
    tokens = time.split(".")
    result = 0
    if len(tokens) == 2:
        result = float("0." + tokens[1])
    mult = 1
    for token in reversed(tokens[0].split(":")):
        result += int(token) * mult
        mult *= 60
    return result

#########################################
# Benchmark specific helper functions.


def configureArgumentParser():
    parser = argparse.ArgumentParser(
        description="Cycles benchmark helper script.")
    parser.add_argument("-b", "--binary",
                        help="Full file path to Blender's binary " +
                             "to use for rendering",
                        default="blender")
    parser.add_argument("-f", "--files", nargs='+')
    parser.add_argument("-v", "--verbose",
                        help="Perform fully verbose communication",
                        action="store_true",
                        default=False)
    return parser


def benchmarkFile(blender, blendfile, stats):
    logHeader("Begin benchmark of file {}" . format(blendfile))
    # Prepare some regex for parsing
    re_path_tracing = re.compile(".*Path Tracing Tile ([0-9]+)/([0-9]+)$")
    re_total_render_time = re.compile(r".*Total render time: ([0-9]+(\.[0-9]+)?)")
    re_render_time_no_sync = re.compile(
        ".*Render time \\(without synchronization\\): ([0-9]+(\\.[0-9]+)?)")
    re_pipeline_time = re.compile(r"Time: ([0-9:\.]+) \(Saving: ([0-9:\.]+)\)")
    # Prepare output folder.
    # TODO(sergey): Use some proper output folder.
    output_folder = "/tmp/"
    # Configure command for the current file.
    command = (blender,
               "--background",
               "--factory-startup",
               blendfile,
               "--engine", "CYCLES",
               "--debug-cycles",
               "--render-output", output_folder,
               "--render-format", "PNG",
               "-f", "1")
    # Run Blender with configured command line.
    logVerbose("About to execute command: {}" . format(command))
    start_time = time.time()
    process = subprocess.Popen(command,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)
    # Keep reading status while Blender is alive.
    total_render_time = "N/A"
    render_time_no_sync = "N/A"
    pipeline_render_time = "N/A"
    while True:
        line = process.stdout.readline()
        if line == b"" and process.poll() is not None:
            break
        line = line.decode().strip()
        if line == "":
            continue
        logVerbose("Line from stdout: {}" . format(line))
        match = re_path_tracing.match(line)
        if match:
            current_tiles = int(match.group(1))
            total_tiles = int(match.group(2))
            elapsed_time = time.time() - start_time
            elapsed_time_str = humanReadableTimeDifference(elapsed_time)
            progress(current_tiles,
                     total_tiles,
                     prefix="Path Tracing Tiles {}" . format(elapsed_time_str))
        match = re_total_render_time.match(line)
        if match:
            total_render_time = float(match.group(1))
        match = re_render_time_no_sync.match(line)
        if match:
            render_time_no_sync = float(match.group(1))
        match = re_pipeline_time.match(line)
        if match:
            pipeline_render_time = humanReadableTimeToSeconds(match.group(1))

    if process.returncode != 0:
        return False

    # Clear line used by progress.
    progressClear()
    print("Total pipeline render time: {} ({} sec)"
          . format(humanReadableTimeDifference(pipeline_render_time),
                   pipeline_render_time))
    print("Total Cycles render time: {} ({} sec)"
          . format(humanReadableTimeDifference(total_render_time),
                   total_render_time))
    print("Pure Cycles render time (without sync): {} ({} sec)"
          . format(humanReadableTimeDifference(render_time_no_sync),
                   render_time_no_sync))
    logOk("Successfully rendered")
    stats[blendfile] = {'PIPELINE_TOTAL': pipeline_render_time,
                        'CYCLES_TOTAL': total_render_time,
                        'CYCLES_NO_SYNC': render_time_no_sync}
    return True


def benchmarkAll(blender, files):
    stats = {}
    for blendfile in files:
        try:
            benchmarkFile(blender, blendfile, stats)
        except KeyboardInterrupt:
            print("")
            logWarning("Rendering aborted!")
            return


def main():
    parser = configureArgumentParser()
    args = parser.parse_args()
    if args.verbose:
        global VERBOSE
        VERBOSE = True
    benchmarkAll(args.binary, args.files)


if __name__ == "__main__":
    main()
