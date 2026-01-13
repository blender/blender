# SPDX-FileCopyrightText: 2021-2022 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

from . import TestQueue

import json
import pathlib


class TestGraph:
    def __init__(self, json_filepaths: list[pathlib.Path]):
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
        # Sort devices alphabetically.
        sorted_devices = sorted(devices.items(), key=lambda item: item[0])
        for device_name, device_entries in sorted_devices:

            # Gather used categories.
            categories = {}
            for entry in device_entries:
                category = entry.category
                if category in categories.keys():
                    categories[category].append(entry)
                else:
                    categories[category] = [entry]

            # Sort categories alphabetically.
            sorted_categories = sorted(categories.items(), key=lambda item: item[0])
            # Generate one graph for every device x category x result key combination.
            for category, category_entries in sorted_categories:
                entries = sorted(category_entries, key=lambda entry: (entry.date, entry.revision, entry.test))

                outputs = set()
                for entry in entries:
                    for output in entry.output.keys():
                        outputs.add(output)

                chart_type = 'line' if entries[0].benchmark_type == 'time_series' else 'comparison'
                if chart_type == 'comparison':
                    entries = sorted(entries, key=lambda entry: (entry.revision, entry.test))

                for output in sorted(outputs, reverse=True):
                    chart_name = f"{category} ({output})"
                    data.append(self.chart(device_name, chart_name, entries, chart_type, output))

        self.json = json.dumps(data, indent=2)

    def chart(self, device_name: str, chart_name: str, entries: list, chart_type: str, output: str) -> dict:
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

        # Convert to chart.js data layout.
        if chart_type == 'comparison':
            # For comparison, tests on the X axis and revisions as datasets.

            # Sort tests by index to ensure stable order for labels.
            sorted_tests = sorted(tests.items(), key=lambda item: item[1])
            labels = [test for test, _ in sorted_tests]

            datasets = []
            # Sort revisions by index.
            sorted_revisions = sorted(revisions.items(), key=lambda item: item[1])
            for revision, index in sorted_revisions:
                datasets.append({
                    'label': revision,
                    'data': [None] * len(tests),
                })

            for entry in entries:
                test_index = tests[entry.test]
                revision_index = revisions[entry.revision]
                output_value = entry.output[output] if output in entry.output else None
                datasets[revision_index]['data'][test_index] = output_value

        else:
            # For time series, dates on the X axis and tests as datasets.
            labels = [None] * len(revisions)
            for revision, index in revisions.items():
                labels[index] = revision_dates[revision] * 1000

            datasets = []
            # Sort tests by index to ensure stable order.
            sorted_tests = sorted(tests.items(), key=lambda item: item[1])
            for test, index in sorted_tests:
                datasets.append({
                    'label': test,
                    'data': [None] * len(revisions),
                    'tension': 0.1,
                })

            for entry in entries:
                test_index = tests[entry.test]
                revision_index = revisions[entry.revision]
                output_value = entry.output[output] if output in entry.output else None

                datasets[test_index]['data'][revision_index] = output_value

        data = {'labels': labels, 'datasets': datasets}
        return {'device': device_name, 'name': chart_name, 'data': data, 'chart_type': chart_type}

    def write(self, filepath: pathlib.Path) -> None:
        # Write HTML page with JSON graph data embedded.
        template_dir = pathlib.Path(__file__).parent
        with open(template_dir / 'graph.template.html', 'r') as f:
            template = f.read()

        contents = template.replace('%JSON_DATA%', self.json)
        with open(filepath, "w") as f:
            f.write(contents)
