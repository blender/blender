# SPDX-FileCopyrightText: 2019-2022 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# Generate a HTML page that links to all test reports.

from pathlib import Path


def _write_html(output_dir: Path) -> None:
    combined_reports = ""

    # Gather intermediate data for all tests and combine into one HTML file.
    categories = sorted((output_dir / "report").glob("*"))

    for category in categories:
        combined_reports += "<h3>" + category.name + "</h3>\n"

        for filepath in sorted(category.glob("*.data")):
            combined_reports += filepath.read_text()

        combined_reports += "<br/>\n"

    # Fill in HTML template.
    template_filepath = Path(__file__).parent / "global_report.template.html"
    html = template_filepath.read_text().replace("%REPORTS%", combined_reports)

    (output_dir / "report.html").write_text(html)


def add(
    output_dir: Path | str,
    category: str,
    name: str,
    filepath: Path | str,
    failed: bool | None = None,
) -> None:
    # Write HTML for single test.
    if failed is None:
        status = "none"
    elif failed:
        status = "failed"
    else:
        status = "ok"

    output_dir = Path(output_dir).resolve()
    filepath = Path(filepath).resolve()
    relpath = filepath.relative_to(output_dir, walk_up=True)

    html = """
        <span class="{status}">&#11044;</span>
        <a href="{relpath}">{name}</a><br/>
        """ . format(status=status,
                     name=name,
                     relpath=relpath.as_posix())

    dirpath = output_dir / "report" / category
    dirpath.mkdir(parents=True, exist_ok=True)
    (dirpath / (name + ".data")).write_text(html)

    # Combined into HTML, each time so we can see intermediate results
    # while tests are still running.
    _write_html(output_dir)
