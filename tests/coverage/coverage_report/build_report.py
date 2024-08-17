# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import base64
import gzip
import html
import json
import shutil
import textwrap
import zipfile

from .util import print_updateable_line
from collections import defaultdict
from pathlib import Path
from pprint import pprint

index_template_path = Path(__file__).parent / "index_template.html"
single_file_template_path = Path(__file__).parent / "single_file_template.html"


def report_as_html(analysis_dir, report_dir, *, reference_dir=None):
    analysis_dir = Path(analysis_dir).absolute()
    report_dir = Path(report_dir).absolute()
    if reference_dir is not None:
        reference_dir = Path(reference_dir).absolute()

    if not analysis_dir.exists():
        raise RuntimeError("Missing analysis at: {}".format(analysis_dir))

    try:
        shutil.rmtree(report_dir)
    except:
        pass

    build_summary(analysis_dir, report_dir, reference_dir)
    build_file_pages(analysis_dir, report_dir)
    print("Report written to {}.".format(report_dir / "index.html"))


def build_summary(analysis_dir, report_dir, reference_dir):
    print("Write index...")
    with open(index_template_path) as f:
        template = f.read()

    result = template
    result = result.replace(
        "ANALYSIS_DATA",
        zip_file_to_compressed_base64(analysis_dir / "summary.json.zip"),
    )

    reference_data_str = ""
    if reference_dir is not None:
        reference_summary_path = reference_dir / "summary.json.zip"
        if reference_summary_path.exists():
            reference_data_str = zip_file_to_compressed_base64(reference_summary_path)
    result = result.replace("REFERENCE_DATA", reference_data_str)

    report_summary_path = report_dir / "index.html"
    report_summary_path.parent.mkdir(parents=True, exist_ok=True)
    with open(report_summary_path, "w") as f:
        f.write(result)


def build_file_pages(analysis_dir, report_dir):
    with open(single_file_template_path) as f:
        template = f.read()

    analysis_files_dir = analysis_dir / "files"
    analysis_paths = list(analysis_files_dir.glob("**/*.json.zip"))

    print("Write report pages...")
    for i, analysis_path in enumerate(analysis_paths):
        relative_path = analysis_path.relative_to(analysis_files_dir)
        relative_path = Path(str(relative_path)[: -len(".json.zip")])
        source_path = "/" / relative_path
        report_path = Path(str(report_dir / "files" / relative_path) + ".html")
        index_page_link = "../" * len(relative_path.parents) + "index.html"

        build_report_for_source_file(template, source_path, analysis_path, report_path, index_page_link)

        print_updateable_line("[{}/{}] written: {}".format(i + 1, len(analysis_paths), report_path))
    print()


def build_report_for_source_file(template_str, source_path, analysis_path, report_path, index_page_link):
    result = template_str
    result = result.replace("TITLE", source_path.name)
    result = result.replace("INDEX_PAGE_LINK", index_page_link)
    result = result.replace("SOURCE_FILE_PATH", str(source_path))
    result = result.replace("SOURCE_CODE", file_to_compressed_base64(source_path))
    result = result.replace("ANALYSIS_DATA", zip_file_to_compressed_base64(analysis_path))

    report_path.parent.mkdir(parents=True, exist_ok=True)
    with open(report_path, "w") as f:
        f.write(result)


def file_to_compressed_base64(file_path):
    with open(file_path, "rb") as f:
        text = f.read()
    return bytes_to_compressed_base64(text)


def zip_file_to_compressed_base64(zip_file_path):
    file_name = zip_file_path.with_suffix("").name
    with zipfile.ZipFile(zip_file_path) as zip_file:
        with zip_file.open(file_name) as f:
            data = f.read()
    return bytes_to_compressed_base64(data)


def bytes_to_compressed_base64(data):
    data = gzip.compress(data)
    data = base64.b64encode(data)
    data = data.decode("utf-8")
    return data
