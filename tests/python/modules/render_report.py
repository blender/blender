# SPDX-FileCopyrightText: 2018-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

"""
Compare renders or screenshots against reference versions and generate
a HTML report showing the differences, for regression testing.
"""

import fnmatch
import os
import sys
import shutil
import subprocess
import time
import multiprocessing
import traceback
import re
import json

from collections.abc import (
    Callable,
    Iterator,
)
from pathlib import Path

from . import global_report
from .colored_print import (print_message, use_message_colors)

ArgumentsCallback = Callable[..., list[str | Path]]


def blend_list(dirpath: Path, blocklist: list[str], filter: str) -> Iterator[Path]:
    import re

    positive_patterns = []
    negative_patterns = []

    if filter:
        if "-" in filter:
            positive_filter, negative_filter = filter.split('-', maxsplit=1)
        else:
            positive_filter = filter
            negative_filter = ""

        positive_patterns = positive_filter.lower().split(":") if positive_filter else []
        negative_patterns = negative_filter.lower().split(":") if negative_filter else []

    for root, _dirs, files in dirpath.walk():
        for filename in files:
            filepath = root / filename
            if filepath.suffix.lower() != ".blend":
                continue

            skip = False
            for blocklist_entry in blocklist:
                if re.match(blocklist_entry, filename):
                    skip = True
                    break

            if skip:
                continue

            name_for_filter = filepath.stem.lower()

            if positive_patterns:
                skip = True
                for positive_pattern in positive_patterns:
                    if fnmatch.fnmatch(name_for_filter, positive_pattern):
                        skip = False
                        break

            if negative_patterns:
                for negative_pattern in negative_patterns:
                    if fnmatch.fnmatch(name_for_filter, negative_pattern):
                        skip = True
                        break

            if not skip:
                yield filepath


def test_get_name(filepath: Path) -> str:
    return filepath.stem


def test_get_images(
    output_dir: Path,
    filepath: Path,
    testname: str,
    reference_dir: str,
    reference_override_dir: str | None,
) -> tuple[Path, Path, Path, Path, Path]:
    dirpath = filepath.parent

    old_img = dirpath / reference_dir / (testname + ".png")
    if reference_override_dir:
        override_img = dirpath / reference_override_dir / (testname + ".png")
        if override_img.exists():
            old_img = override_img

    ref_dirpath = output_dir / dirpath.name / "ref"
    ref_img = ref_dirpath / (testname + ".png")
    ref_dirpath.mkdir(parents=True, exist_ok=True)
    if old_img.exists():
        shutil.copy(old_img, ref_img)

    new_dirpath = output_dir / dirpath.name
    new_dirpath.mkdir(parents=True, exist_ok=True)
    new_img = new_dirpath / (testname + ".png")

    diff_dirpath = output_dir / dirpath.name / "diff"
    diff_dirpath.mkdir(parents=True, exist_ok=True)
    diff_color_img = diff_dirpath / (testname + ".diff_color.png")
    diff_alpha_img = diff_dirpath / (testname + ".diff_alpha.png")

    return old_img, ref_img, new_img, diff_color_img, diff_alpha_img


class TestResult:
    def __init__(self, report: "Report", filepath: Path, name: str) -> None:
        self.filepath = filepath
        self.name = name
        self.error: str | None = None
        self.stats: str | None = None
        self.tmp_out_img_base = report.output_dir / ("tmp_" + name)
        self.tmp_out_img = report.output_dir / ("tmp_" + name + "0001.png")
        self.old_img, self.ref_img, self.new_img, self.diff_color_img, self.diff_alpha_img = test_get_images(
            report.output_dir, filepath, name, report.reference_dir, report.reference_override_dir)


def diff_output(
    test: TestResult,
    oiiotool: Path,
    fail_threshold: float,
    fail_percent: float,
    verbose: bool,
    update: bool,
) -> TestResult:
    # Create reference render directory.
    test.old_img.parent.mkdir(parents=True, exist_ok=True)

    # Copy temporary to new image.
    test.new_img.unlink(missing_ok=True)
    if test.tmp_out_img.exists():
        shutil.copy(test.tmp_out_img, test.new_img)

    if test.ref_img.exists():
        # Diff images test with threshold.
        command = (
            oiiotool,
            test.ref_img,
            test.tmp_out_img,
            "--fail", str(fail_threshold),
            "--failpercent", str(fail_percent),
            "--diff",
        )
        try:
            output = subprocess.check_output(command)
            failed = False
        except subprocess.CalledProcessError as e:
            output = e.output
            if verbose:
                print_message(output.decode("utf-8", 'ignore'))
            failed = e.returncode != 0

        try:
            output = output.decode("utf-8", 'ignore')
            # Only print max error and number of pixels over threshold.
            # Max error is not present if the images are a perfect match.
            max_error = re.search(r"Max error *= *(\S+)", output)
            over_threshold = re.findall(r"\S+ pixels .* over \S+", output)
            if max_error or over_threshold:
                test.stats = ""
                if max_error:
                    test.stats += "Max error = {:.3f}\n".format(float(max_error.group(1)))
                if over_threshold:
                    test.stats += over_threshold[-1]
        except Exception:
            print("Error parsing oiiotool output: \n", output, "\n", traceback.format_exc())
            test.error = "STATS ERROR"
            return test
    else:
        if not update:
            test.error = "VERIFY"
            return test

        failed = True

    if failed and update:
        # Update reference image if requested.
        shutil.copy(test.new_img, test.ref_img)
        shutil.copy(test.new_img, test.old_img)
        failed = False

    # Generate color diff image.
    command = (
        oiiotool,
        test.ref_img,
        "--ch", "R,G,B",
        test.tmp_out_img,
        "--ch", "R,G,B",
        "--sub",
        "--abs",
        "--mulc", "16",
        "-o", test.diff_color_img,
    )
    try:
        subprocess.check_output(command, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        if verbose:
            print_message(e.output.decode("utf-8", 'ignore'))

    # Generate alpha diff image.
    command = (
        oiiotool,
        test.ref_img,
        "--ch", "A",
        test.tmp_out_img,
        "--ch", "A",
        "--sub",
        "--abs",
        "--mulc", "16",
        "-o", test.diff_alpha_img,
    )
    try:
        subprocess.check_output(command, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        if verbose:
            msg = e.output.decode("utf-8", 'ignore')
            for line in msg.splitlines():
                # Ignore warnings for images without alpha channel.
                if "--ch: Unknown channel name" not in line:
                    print_message(line)

    if failed:
        test.error = "VERIFY"
    else:
        test.error = None

    return test


def get_gpu_device_info(blender: Path, gpu_backend: str) -> dict[str, str]:
    command = [
        blender,
        "--background",
        "--factory-startup",
        "--gpu-backend",
        gpu_backend,
        "--python",
        str(Path(__file__).parent / "gpu_info.py")
    ]

    completed_process = subprocess.run(command, stdout=subprocess.PIPE, universal_newlines=True)
    info = completed_process.stdout.split("<GPU_INFO>")[1].split("</GPU_INFO>")[0]
    return json.loads(info)


def get_gpu_device_vendor(blender: Path, gpu_backend: str) -> str:
    return get_gpu_device_info(blender, gpu_backend)["DEVICE_TYPE"]


def get_gpu_device_ray_queries_support(blender: Path, gpu_backend: str) -> bool:
    command = [
        blender,
        "--background",
        "--factory-startup",
        "--gpu-backend",
        gpu_backend,
        "--python-expr",
        'import gpu; gpu.init(); print("GPU_RAY_QUERIES_SUPPORT:", gpu.capabilities.ray_query_support_get())'
    ]
    completed_process = subprocess.run(command, stdout=subprocess.PIPE, universal_newlines=True)
    return "GPU_RAY_QUERIES_SUPPORT: True" in completed_process.stdout


class Report:
    __slots__ = (
        'title',
        'engine_name',
        'output_dir',
        'global_dir',
        'reference_dir',
        'reference_override_dir',
        "test_name_suffix",
        'oiiotool',
        'pixelated',
        'fail_threshold',
        'fail_percent',
        'verbose',
        'update',
        'filter',
        'failed_tests',
        'passed_tests',
        'compare_tests',
        'compare_engine',
        'blocklist',
    )

    def __init__(
        self,
        title: str,
        output_dir: Path,
        oiiotool: Path,
        variation: str | None = None,
        blocklist: list[str] = [],
    ) -> None:
        self.title = title

        self.output_dir = output_dir.resolve()
        self.global_dir = self.output_dir.parent

        self.reference_dir = 'reference_renders'
        self.reference_override_dir: str | None = None
        self.test_name_suffix = ""
        self.oiiotool = oiiotool
        self.compare_engine: tuple[str, str | None] | None = None
        self.fail_threshold = 0.016
        self.fail_percent: float = 1
        self.engine_name = self.title.lower().replace(" ", "_")
        self.blocklist = [] if os.getenv('BLENDER_TEST_IGNORE_BLOCKLIST') is not None else blocklist

        if variation:
            self.title = self._engine_title(title, variation)
            self.output_dir = self._engine_path(self.output_dir, variation.lower())

        self.pixelated = False
        self.verbose = os.environ.get("BLENDER_VERBOSE") is not None
        self.update = os.getenv('BLENDER_TEST_UPDATE') is not None
        self.filter = os.getenv('BLENDER_TEST_FILTER') or ""

        if os.environ.get("BLENDER_TEST_COLOR") is not None:
            use_message_colors()

        self.failed_tests = ""
        self.passed_tests = ""
        self.compare_tests = ""

        output_dir.mkdir(parents=True, exist_ok=True)

    def set_pixelated(self, pixelated: bool) -> None:
        self.pixelated = pixelated

    def set_fail_threshold(self, threshold: float) -> None:
        self.fail_threshold = threshold

    def set_fail_percent(self, percent: float) -> None:
        self.fail_percent = percent

    def set_reference_dir(self, reference_dir: str) -> None:
        self.reference_dir = reference_dir

    def set_reference_override_dir(self, reference_override_dir: str) -> None:
        self.reference_override_dir = reference_override_dir

    def set_compare_engine(self, other_engine: str, other_variation: str | None = None) -> None:
        self.compare_engine = (other_engine, other_variation)

    def set_engine_name(self, engine_name: str) -> None:
        self.engine_name = engine_name

    def set_test_name_suffix(self, suffix: str) -> None:
        self.test_name_suffix = suffix

    def run(
        self,
        dirpath: Path,
        blender: Path,
        arguments_cb: ArgumentsCallback,
        batch: bool = False,
        fail_silently: bool = False,
    ) -> bool:
        # Run tests and output report.
        dirname = dirpath.name
        ok = self._run_all_tests(dirname, dirpath, blender, arguments_cb, batch, fail_silently)
        self._write_data(dirname)
        self._write_html()
        if self.compare_engine:
            self._write_html(comparison=True)
        return ok

    def _write_data(self, dirname: str) -> None:
        # Write intermediate data for single test.
        outdir = self.output_dir / dirname
        outdir.mkdir(parents=True, exist_ok=True)

        (outdir / "failed.data").write_text(self.failed_tests)
        (outdir / "passed.data").write_text(self.passed_tests)
        if self.compare_engine:
            (outdir / "compare.data").write_text(self.compare_tests)

    def _navigation_item(self, title: str, href: str, active: bool) -> str:
        if active:
            return """<li class="breadcrumb-item active" aria-current="page">%s</li>""" % title
        else:
            return """<li class="breadcrumb-item"><a href="%s">%s</a></li>""" % (href, title)

    def _engine_title(self, engine: str, variation: str | None) -> str:
        if variation:
            return engine.title() + ' ' + variation
        else:
            return engine.title()

    def _engine_path(self, path: Path | str, variation: str | None) -> Path:
        if variation:
            variation = variation.replace(' ', '_')
            return Path(path) / variation.lower()
        else:
            return Path(path)

    def _navigation_html(self, comparison: bool) -> str:
        html = """<nav aria-label="breadcrumb"><ol class="breadcrumb">"""
        global_report_url = self._relative_url(self.global_dir / "report.html")
        html += self._navigation_item("Test Reports", global_report_url, False)
        html += self._navigation_item(self.title, "report.html", not comparison)
        if self.compare_engine:
            compare_title = "Compare with %s" % self._engine_title(*self.compare_engine)
            html += self._navigation_item(compare_title, "compare.html", comparison)
        html += """</ol></nav>"""

        return html

    def _write_html(self, comparison: bool = False) -> None:
        # Gather intermediate data for all tests.
        if comparison:
            failed_data = []
            passed_data = sorted(self.output_dir.glob("*/compare.data"))
        else:
            failed_data = sorted(self.output_dir.glob("*/failed.data"))
            passed_data = sorted(self.output_dir.glob("*/passed.data"))

        failed_tests = "".join(filepath.read_text() for filepath in failed_data)
        passed_tests = "".join(filepath.read_text() for filepath in passed_data)

        tests_html = failed_tests + passed_tests

        # Navigation
        menu = self._navigation_html(comparison)

        failed = len(failed_tests) > 0

        if comparison:
            assert self.compare_engine is not None
            title = self.title + " Test Compare"
            engine_self = self.title
            engine_other = self._engine_title(*self.compare_engine)
            columns_html = "<tr><th>Name</th><th>%s</th><th>%s</th>" % (engine_self, engine_other)
        else:
            title = self.title + " Test Report"
            columns_html = "<tr><th>Name</th><th>New</th><th>Reference</th><th>Diff Color</th><th>Diff Alpha</th>"

        # Fill in HTML template.
        template_filepath = Path(__file__).parent / "render_report.template.html"
        replacements = {
            "%TITLE%": title,
            "%IMAGE_RENDERING%": "pixelated" if self.pixelated else "auto",
            "%MENU%": menu,
            "%MESSAGE_HIDDEN%": "" if failed else "hidden",
            "%ENGINE_NAME%": self.engine_name,
            "%COLUMNS%": columns_html,
            "%TESTS%": tests_html,
        }
        html = template_filepath.read_text()
        for key, value in replacements.items():
            html = html.replace(key, value)

        filename = "report.html" if not comparison else "compare.html"
        filepath = self.output_dir / filename
        filepath.write_text(html)

        print_message("Report saved to: " + filepath.as_uri())

        # Update global report
        if not comparison:
            global_failed = failed if not comparison else None
            global_report.add(self.global_dir, "Render", self.title, filepath, global_failed)

    def _relative_url(self, filepath: Path) -> str:
        return filepath.relative_to(self.output_dir, walk_up=True).as_posix()

    def _write_test_html(self, test_category: str, test_result: TestResult) -> None:
        name = test_result.name + self.test_name_suffix
        result_attr = test_result.error or ""

        status = "<strong>" + test_result.error + "</strong><br>" if test_result.error else ""
        if test_result.stats:
            status += "<i>" + "<br>".join(test_result.stats.splitlines()) + "</i>"
        tr_style = """ class="table-danger" """ if test_result.error else ""

        new_url = self._relative_url(test_result.new_img)
        ref_url = self._relative_url(test_result.ref_img)
        diff_color_url = self._relative_url(test_result.diff_color_img)
        diff_alpha_url = self._relative_url(test_result.diff_alpha_img)

        test_html = f"""
            <tr{tr_style} data-category="{test_category}" data-name="{name}" data-result="{result_attr}">
                <td><b>{name}</b><br/>{test_category}<br/>{status}</td>
                <td><img src="{new_url}" onmouseover="this.src='{ref_url}';" onmouseout="this.src='{new_url}';" class="render"></td>
                <td><img src="{ref_url}" onmouseover="this.src='{new_url}';" onmouseout="this.src='{ref_url}';" class="render"></td>
                <td><img src="{diff_color_url}"></td>
                <td><img src="{diff_alpha_url}"></td>
            </tr>"""

        if test_result.error:
            self.failed_tests += test_html
        else:
            self.passed_tests += test_html

        if self.compare_engine:
            compare_dir = self.global_dir / self._engine_path(*self.compare_engine)
            ref_url = self._relative_url(compare_dir / new_url)

            test_html = """
                <tr{tr_style} data-category="{test_category}" data-name="{name}" data-result="{result_attr}">
                    <td><b>{name}</b><br/>{testname}<br/>{status}</td>
                    <td><img src="{new_url}" onmouseover="this.src='{ref_url}';" onmouseout="this.src='{new_url}';" class="render"></td>
                    <td><img src="{ref_url}" onmouseover="this.src='{new_url}';" onmouseout="this.src='{ref_url}';" class="render"></td>
                </tr>""" . format(tr_style=tr_style,
                                  test_category=test_category,
                                  result_attr=result_attr,
                                  name=name,
                                  testname=test_result.name,
                                  status=status,
                                  new_url=new_url,
                                  ref_url=ref_url)

            self.compare_tests += test_html

    def _get_render_arguments(
        self,
        arguments_cb: ArgumentsCallback,
        filepath: Path,
        base_output_filepath: Path,
    ) -> list[str | Path]:
        # Each render test can override this method to provide extra functionality.
        # See Cycles render tests for an example.
        # Do not delete.
        return arguments_cb(filepath, base_output_filepath)

    def _get_arguments_suffix(self) -> list[str]:
        # Get command line arguments that need to be provided after all file-specific ones.
        # For example the Cycles render device argument needs to be added at the end of
        # the argument list, otherwise tests can't be batched together.
        #
        # Each render test is supposed to override this method.
        return []

    def _get_filepath_tests(self, filepath: Path) -> list[TestResult]:
        list_filepath = filepath.with_name(filepath.stem + "_permutations.txt")
        if list_filepath.exists():
            with open(list_filepath, 'r') as file:
                return [TestResult(self, filepath, testname.rstrip('\n')) for testname in file]
        else:
            testname = test_get_name(filepath)
            return [TestResult(self, filepath, testname)]

    def _run_tests(
        self,
        filepaths: list[Path],
        blender: Path,
        arguments_cb: ArgumentsCallback,
        batch: bool,
    ) -> list[TestResult]:
        # Run multiple tests in a single Blender process since startup can be
        # a significant factor. In case of crashes, re-run the remaining tests.
        verbose = os.environ.get("BLENDER_VERBOSE") is not None

        remaining_filepaths = filepaths[:]
        test_results = []
        arguments_suffix = self._get_arguments_suffix()

        while len(remaining_filepaths) > 0:
            command: list[str | Path] = [blender]
            running_tests = []

            # On Windows, there is a maximum length of 32,767 characters (including the terminating null character)
            # for process command line commands, see:
            # https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa
            command_line_length = len(str(blender))
            for suffix in arguments_suffix:
                # Add 3 for taking into account spaces and quotation marks potentially added by Python.
                command_line_length += len(str(suffix)) + 3

            # Construct output filepaths and command to run
            for filepath in remaining_filepaths:
                testname = test_get_name(filepath)

                base_output_filepath = self.output_dir / ("tmp_" + testname)
                command_filepath = self._get_render_arguments(arguments_cb, filepath, base_output_filepath)

                # Check if we have surpassed the command line limit.
                for cmd in command_filepath:
                    command_line_length += len(str(cmd)) + 3
                if sys.platform == 'win32' and command_line_length > 32766 and len(running_tests) > 0:
                    break

                print_message(testname, 'SUCCESS', 'RUN')
                running_tests.append(filepath)
                command.extend(command_filepath)

                output_filepath = self.output_dir / ("tmp_" + testname + "0001.png")
                output_filepath.unlink(missing_ok=True)

                # Only chain multiple commands for batch
                if not batch:
                    break

            command.extend(arguments_suffix)

            # Run process
            crash = False
            output = None
            try:
                completed_process = subprocess.run(command, stdout=subprocess.PIPE)
                if completed_process.returncode != 0:
                    crash = True
                output = completed_process.stdout
            except Exception:
                crash = True

            if verbose:
                def quote_expr_args(cmd: list[str | Path]) -> list[str]:
                    quoted = []
                    quote_next = False
                    for arg in cmd:
                        if quote_next:
                            quoted.append('"{}"'.format(arg))  # wrap the expression in quotes
                            quote_next = False
                        else:
                            quoted.append(str(arg))
                            if arg == "--python-expr":
                                quote_next = True
                    return quoted
                print(' '.join(quote_expr_args(command)))

            if (verbose or crash) and output:
                print(output.decode("utf-8", 'ignore'))

            tests_to_check = []

            # Detect missing filepaths and consider those errors
            for filepath in running_tests:
                remaining_filepaths.pop(0)
                file_crashed = False
                for test in self._get_filepath_tests(filepath):
                    self.postprocess_test(blender, test)
                    if not test.tmp_out_img.exists() or test.tmp_out_img.stat().st_size == 0:
                        if crash:
                            # In case of crash, stop after missing files and re-render remaining
                            test.error = "CRASH"
                            test_results.append(test)
                            file_crashed = True
                            break
                        else:
                            test.error = "NO OUTPUT"
                            test_results.append(test)
                    else:
                        tests_to_check.append(test)
                if file_crashed:
                    break

            pool = multiprocessing.Pool(multiprocessing.cpu_count())
            test_results.extend(pool.starmap(diff_output,
                                             [(test, self.oiiotool, self.fail_threshold, self.fail_percent, self.verbose, self.update)
                                              for test in tests_to_check]))
            pool.close()

        for test in test_results:
            if test.error == "CRASH":
                print_message("Crash running Blender")
                print_message(test.name, 'FAILURE', 'FAILED')
            elif test.error == "NO OUTPUT":
                print_message("No render result file found")
                print_message(str(test.tmp_out_img), 'FAILURE', 'FAILED')
            elif test.error == "VERIFY":
                print_message("Render result is different from reference image")
                print_message(test.name, 'FAILURE', 'FAILED')
            else:
                print_message(test.name, 'SUCCESS', 'OK')

            test.tmp_out_img.unlink(missing_ok=True)

        return test_results

    def postprocess_test(self, blender: Path, test: TestResult) -> None:
        """
        Post-process test result after the Blender has run.
        For example, this function is where conversion from video to a still image suitable for image diffing.
        """

        pass

    def _run_all_tests(
        self,
        dirname: str,
        dirpath: Path,
        blender: Path,
        arguments_cb: ArgumentsCallback,
        batch: bool,
        fail_silently: bool,
    ) -> bool:
        if self.filter:
            print_message(f"Note: Blender Test filter = {self.filter}", type='WARNING', status="RAW")

        passed_tests = []
        failed_tests = []
        silently_failed_tests = []
        all_files = list(blend_list(dirpath, self.blocklist, self.filter))
        all_files.sort()
        if not list(blend_list(dirpath, [], "")):
            print_message("No .blend files found in '{}'!".format(dirpath), 'FAILURE', 'FAILED')
            return False

        print_message("Running {} tests from 1 test case." .
                      format(len(all_files)),
                      'SUCCESS', "==========")
        time_start = time.time()
        test_results = self._run_tests(all_files, blender, arguments_cb, batch)
        for test in test_results:
            if test.error:
                if test.error == "NO_ENGINE":
                    return False
                elif test.error == "NO_START":
                    return False

                if fail_silently and test.error != 'CRASH':
                    silently_failed_tests.append(test.name)
                else:
                    failed_tests.append(test.name)
            else:
                passed_tests.append(test.name)
            self._write_test_html(dirname, test)
        time_end = time.time()
        elapsed_ms = int((time_end - time_start) * 1000)
        print_message("")
        print_message("{} tests from 1 test case ran. ({} ms total)" .
                      format(len(all_files), elapsed_ms),
                      'SUCCESS', "==========")
        print_message("{} tests." .
                      format(len(passed_tests)),
                      'SUCCESS', 'PASSED')
        all_failed_tests = silently_failed_tests + failed_tests
        if all_failed_tests:
            print_message("{} tests, listed below:" .
                          format(len(all_failed_tests)),
                          'FAILURE', 'FAILED')
            all_failed_tests.sort()
            for test in all_failed_tests:
                print_message("{}" . format(test), 'FAILURE', "FAILED")

        return not bool(failed_tests)
