# Apache License, Version 2.0
#
# Generate a HTML page that links to all test reports.

import glob
import os
import pathlib

def _write_html(output_dir):
    combined_reports = ""

    # Gather intermediate data for all tests and combine into one HTML file.
    categories = sorted(glob.glob(os.path.join(output_dir, "report", "*")))

    for category in categories:
        category_name = os.path.basename(category)
        combined_reports += "<h3>" + category_name + "</h3>\n"

        reports = sorted(glob.glob(os.path.join(category, "*.data")))
        for filename in reports:
            filepath = os.path.join(output_dir, filename)
            combined_reports += pathlib.Path(filepath).read_text()

        combined_reports += "<br/>\n";

    html = """
<html>
<head>
    <title>{title}</title>
    <style>
        .ok {{ color: green; }}
        .failed {{ color: red; }}
        .none {{ color: #999; }}
    </style>
    <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0-alpha.6/css/bootstrap.min.css">
</head>
<body>
    <div class="container">
        <br/>
        {combined_reports}
        <br/>
    </div>
</body>
</html>
    """ . format(title="Blender Test Reports",
                 combined_reports=combined_reports)

    filepath = os.path.join(output_dir, "report.html")
    pathlib.Path(filepath).write_text(html)


def add(output_dir, category, name, filepath, failed=None):
    # Write HTML for single test.
    if failed is None:
        status = "none"
    elif failed:
        status = "failed"
    else:
        status = "ok"

    html = """
        <span class="{status}">&#11044;</span>
        <a href="file://{filepath}">{name}</a><br/>
        """ . format(status=status,
                     name=name,
                     filepath=filepath)

    dirpath = os.path.join(output_dir, "report", category);
    os.makedirs(dirpath, exist_ok=True)
    filepath = os.path.join(dirpath, name + ".data")
    pathlib.Path(filepath).write_text(html)

    # Combined into HTML, each time so we can see intermediate results
    # while tests are still running.
    _write_html(output_dir)
