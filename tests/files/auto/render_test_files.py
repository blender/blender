# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Run auto regression files
import os
import select
import shutil
import subprocess
import sys


# Find executables and files
def find_test_directory():
    test_directory = ""

    if "__file__" in globals():
        test_directory = os.path.dirname(__file__)
        sys.path.append(test_directory)

    return test_directory


def find_idiff(idiff_path):
    if not idiff_path:
        if shutil.which("idiff"):
            idiff_path = "idiff"

    return idiff_path


def find_blender(blender_executable):
    if not blender_executable:
        if os.path.split(sys.executable)[1] == "blender":
            blender_executable = sys.executable
            print("")

    return blender_executable


def find_test_files(directory, search_items, extension):
    files = []

    for item in search_items:
        item_path = os.path.join(directory, item)

        if os.path.isdir(item_path):
            # find all .blend files in directory
            for filename in os.listdir(item_path):
                name, ext = os.path.splitext(filename)
                if ext == extension:
                    files += [(name, os.path.join(item, filename))]
        else:
            # single .blend
            name = os.path.splitext(os.path.basename(item))[0]
            files += [(name, item)]

    return files


def render_variations(blend_file):
    if blend_file.find("cycles") == -1:
        return ("",)
    else:
        return ("_svm", "_osl")


# Execute command while printing and logging it
def execute_command(command, logfile):
    logdir = os.path.dirname(logfile)
    if not os.path.exists(logdir):
        os.makedirs(logdir)

    log = open(str(logfile), "w")
    log.write(command + "\n\n")

    p = subprocess.Popen(command.split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    maxlen = 10
    columns = shutil.get_terminal_size().columns

    while True:
        reads = [p.stdout.fileno(), p.stderr.fileno()]
        ret = select.select(reads, [], [])

        for fd in ret[0]:
            for std in (p.stdout, p.stderr):
                if fd == std.fileno():
                    read = std.readline().decode('utf8').rstrip()
                    log.write(read + "\n")

                    # get last part, and no multiline because it breaks \r
                    read = read.split('|')[-1].strip()
                    if len(read) > columns:
                        read = read[:columns]

                    maxlen = max(len(read), maxlen)
                    sys.stdout.write("\r" + read + (maxlen - len(read)) * " " + "\r")

        if p.poll() is not None:
            break

    log.close()
    sys.stdout.write("\r" + maxlen * " \r")

    return p.returncode


def failure_color(message):
    return '\033[91m' + message + '\033[0m'


def ok_color(message):
    return '\033[92m' + message + '\033[0m'


def bold_color(message):
    return '\033[1m' + message + '\033[0m'


# Setup paths
test_directory = find_test_directory()

import test_config

idiff_path = find_idiff(test_config.idiff_path)
blender_executable = find_blender(test_config.blender_executable)
blend_files = find_test_files(test_directory, test_config.files, ".blend")

# Run tests
for blend_name, blend_file in blend_files:
    # Remove previous output
    def clear_file(blend_name, blend_file):
        for variation in render_variations(blend_file):
            extension = variation + ".png"
            diff_log = os.path.join(test_directory, "test_renders", blend_name + ".diff")
            test_image = os.path.join(test_directory, "test_renders", blend_name + extension)

            if os.path.exists(diff_log):
                os.remove(diff_log)
            if os.path.exists(test_image):
                os.remove(test_image)

    # Render
    def render_file(blend_name, blend_file):
        test_script = os.path.join(test_directory, "test_run.py")
        blend_path = os.path.join(test_directory, blend_file)
        blend_log = os.path.join(test_directory, "test_renders", blend_name + ".log")

        command = '%s -b %s -P %s' % (blender_executable, blend_path, test_script)
        print("Rendering " + bold_color(blend_file))
        if execute_command(command, blend_log) != 0:
            print(failure_color("Render Failed"))
            return False

        return True

    # Compare
    def compare_file(blend_name, blend_file):
        for variation in render_variations(blend_file):
            extension = variation + ".png"
            diff_log = os.path.join(test_directory, "test_renders", blend_name + ".diff")
            reference_image = os.path.join(test_directory, "reference_renders", blend_name + extension)
            test_image = os.path.join(test_directory, "test_renders", blend_name + extension)

            if not os.path.isfile(test_image):
                print(failure_color("Output image missing"))
                return False
            if idiff_path and not os.path.isfile(reference_image):
                print(ok_color("Render OK"))
                return True

            if idiff_path:
                command = '%s -fail %f -failpercent %f %s %s' % (idiff_path, 0.02, 2, reference_image, test_image)
                if execute_command(command, diff_log) not in (0, 1):
                    print(failure_color("Image does not match reference"))
                    return False
            else:
                print(ok_color("Render OK"))
                return True

        print(ok_color("Render + Compare OK"))
        return True

    clear_file(blend_name, blend_file)

    if render_file(blend_name, blend_file):
        compare_file(blend_name, blend_file)
