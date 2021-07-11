# Apache License, Version 2.0

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
                if entry.status in ('done', 'outdated'):
                    device_name = entry.device_name
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

            # Generate one graph for every device x category combination.
            for category, category_entries in categories.items():
                entries = sorted(category_entries, key=lambda entry: (entry.revision, entry.test))
                chart_type = 'line' if entries[0].benchmark_type == 'time_series' else 'comparison'
                data.append(self.chart(device_name, category, entries, chart_type))

        self.json = json.dumps(data, indent=2)

    def chart(self, device_name: str, category: str, entries: List, chart_type: str) -> Dict:
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

        # Google Charts JSON data layout is like a spreadsheat table, with
        # colums, rows and cells. We create one column for revision labels,
        # and one column for each test.
        cols = []
        if chart_type == 'line':
            cols.append({'id': '', 'label': 'Date', 'type': 'date'})
        else:
            cols.append({'id': '', 'label': 'Revision', 'type': 'string'})
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
            time = entry.output['time']
            rows[revision_index]['c'][test_index + 1] = {'f': None, 'v': time}

        data = {'cols': cols, 'rows': rows}
        return {'device': device_name, 'category': category, 'data': data, 'chart_type': chart_type}

    def write(self, filepath: pathlib.Path) -> None:
        # Write HTML page with JSON graph data embedded.
        template_dir = pathlib.Path(__file__).parent
        with open(template_dir / 'graph.template.html', 'r') as f:
            template = f.read()

        contents = template.replace('%JSON_DATA%', self.json)
        with open(filepath, "w") as f:
            f.write(contents)
