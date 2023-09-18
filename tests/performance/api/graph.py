# SPDX-FileCopyrightText: 2021-2022 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

from . import TestQueue

import json
import pathlib
from typing import Dict, List


class TestGraph:
    def __init__(self, json_filepaths: List[pathlib.Path]):
        # Initialize graph from JSON file. Note that this is implemented without
        # accessing any benchmark environment or configuration. This ways benchmarks
        # run on various machines can be aggregated and the graph generated on another
        # machine.

        # Gather entries for each device.
        devices = {}

        for json_filepath in json_filepaths:
            queue = TestQueue(json_filepath)

            for entry in queue.entries:
                if entry.status in {'done', 'outdated'}:
                    device_name = entry.device_name + " (" + entry.device_type + ")"
                    if device_name in devices.keys():
                        devices[device_name].append(entry)
                    else:
                        devices[device_name] = [entry]

        data = []
        for device_name, device_entries in devices.items():

            # Gather used categories.
            categories = {}
            for entry in device_entries:
                category = entry.category
                if category in categories.keys():
                    categories[category].append(entry)
                else:
                    categories[category] = [entry]

            # Generate one graph for every device x category x result key combination.
            for category, category_entries in categories.items():
                entries = sorted(category_entries, key=lambda entry: (entry.date, entry.revision, entry.test))

                outputs = set()
                for entry in entries:
                    for output in entry.output.keys():
                        outputs.add(output)

                chart_type = 'line' if entries[0].benchmark_type == 'time_series' else 'comparison'

                for output in outputs:
                    chart_name = f"{category} ({output})"
                    data.append(self.chart(device_name, chart_name, entries, chart_type, output))

        self.json = json.dumps(data, indent=2)

    def chart(self, device_name: str, chart_name: str, entries: List, chart_type: str, output: str) -> Dict:
        # Gather used tests.
        tests = {}
        for entry in entries:
            test = entry.test
            if test not in tests.keys():
                tests[test] = len(tests)

        # Gather used revisions.
        revisions = {}
        revision_dates = {}
        for entry in entries:
            revision = entry.revision
            if revision not in revisions.keys():
                revisions[revision] = len(revisions)
                revision_dates[revision] = int(entry.date)

        # Google Charts JSON data layout is like a spreadsheet table, with
        # columns, rows, and cells. We create one column for revision labels,
        # and one column for each test.
        cols = []
        if chart_type == 'line':
            cols.append({'id': '', 'label': 'Date', 'type': 'date'})
        else:
            cols.append({'id': '', 'label': ' ', 'type': 'string'})
        for test, test_index in tests.items():
            cols.append({'id': '', 'label': test, 'type': 'number'})

        rows = []
        for revision, revision_index in revisions.items():
            if chart_type == 'line':
                date = revision_dates[revision]
                row = [{'f': None, 'v': 'Date({0})'.format(date * 1000)}]
            else:
                row = [{'f': None, 'v': revision}]
            row += [{}] * len(tests)
            rows.append({'c': row})

        for entry in entries:
            test_index = tests[entry.test]
            revision_index = revisions[entry.revision]
            output_value = entry.output[output] if output in entry.output else -1.0

            if output.find("memory") != -1:
                formatted_value = '%.2f MB' % (output_value / (1024 * 1024))
            else:
                formatted_value = "%.4f" % output_value

            cell = {'f': formatted_value, 'v': output_value}
            rows[revision_index]['c'][test_index + 1] = cell

        data = {'cols': cols, 'rows': rows}
        return {'device': device_name, 'name': chart_name, 'data': data, 'chart_type': chart_type}

    def write(self, filepath: pathlib.Path) -> None:
        # Write HTML page with JSON graph data embedded.
        template_dir = pathlib.Path(__file__).parent
        with open(template_dir / 'graph.template.html', 'r') as f:
            template = f.read()

        contents = template.replace('%JSON_DATA%', self.json)
        with open(filepath, "w") as f:
            f.write(contents)
