# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

class MarkdownColumn:
    def __init__(self, name, width=10, is_visible=True, alignment='LEFT', key=None):
        self.name = name
        self.width = width
        self.is_visible = is_visible
        self.alignment = alignment
        self.key = key
        if len(self.name) > self.width:
            self.width = len(self.name)


class MarkdownTable:
    def __init__(self):
        self.columns = []
        self.show_header = True

    def add_column(self, *args, **kwargs):
        self.columns.append(MarkdownColumn(*args, **kwargs))

    def print_header(self):
        if not self.show_header:
            return

        values = []
        lines = []
        for column in self.columns:
            if not column.is_visible:
                continue
            values.append(f"{column.name:{column.width}}")
            alignment_char = ':' if column.alignment == 'RIGHT' else '-'
            lines.append('-' * (column.width - 1) + alignment_char)

        print('| ' + (' | '.join(values)) + ' |')
        print('| ' + (' | '.join(lines)) + ' |')

    def print_row(self, row_values, end='\n'):
        values = []
        for column, value in zip(self.columns, row_values):
            if not column.is_visible:
                continue
            if len(value) > column.width:
                column.width = len(value)
            if column.alignment == 'LEFT':
                values.append(f"{value:<{column.width}}")
            else:
                values.append(f"{value:>{column.width}}")

        print("| " + (" | ".join(values)) + " |", end=end, flush=True)
