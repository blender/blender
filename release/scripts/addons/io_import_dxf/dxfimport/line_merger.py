# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>


def line_merger(lines, precision=6):
    merger = _LineMerger(lines, precision)
    return merger.polylines


def _round_point(point, precision):
    return tuple(round(c, precision) for c in point)


class _LineMerger:
    def __init__(self, lines, precision):
        self.segments = set()  # single lines as tuples: ((sx, sy[, sz]), (ex, ey[, ez]))
        self.used_segments = set()
        self.points = dict()  # key: point -> value: list of segments with this point as start or end point
        self.precision = precision
        self.setup(lines)
        self.polylines = self.merge_lines()  # result of merging process

    def setup(self, lines):
        for line in lines:
            s = _round_point(line.start, self.precision)
            e = _round_point(line.end, self.precision)
            self.add_segment(s, e)

    def add_segment(self, start, end):
        if start == end:
            return  # this is not a segment
        if end < start:  # order start and end points to detect all doubles
            segment = (end, start)
        else:
            segment = (start, end)
        if segment in self.segments:
            return  # this segment already exist
        self.segments.add(segment)
        self.add_point(start, segment)
        self.add_point(end, segment)

    def add_point(self, point, segment):
        segments = self.points.get(point)
        if segments is None:
            segments = list()
            self.points[point] = segments
        segments.append(segment)

    def get_segment_with_point(self, point):
        segments = self.points.get(point)
        if segments is None:
            return None

        # Very important: do not return already used segments
        for segment in segments:
            if segment not in self.used_segments:
                return segment
        return None

    def mark_as_used_segment(self, segment):
        self.used_segments.add(segment)
        self.segments.discard(segment)

    def merge_lines(self):
        def get_extension_point(point):
            extension = self.get_segment_with_point(point)
            if extension is not None:
                self.mark_as_used_segment(extension)
                if extension[0] == point:
                    return extension[1]
                else:
                    return extension[0]
            return None

        polylines = []
        while len(self.segments):
            segment = self.segments.pop()  # take an arbitrary segment
            self.mark_as_used_segment(segment)
            polyline = list(segment)  # start a new polyline
            extend_start = True
            extend_end = True
            while extend_start or extend_end:
                if extend_start:
                    extension_point = get_extension_point(polyline[0])  # extend start of polyline
                    if extension_point is not None:
                        polyline.insert(0, extension_point)
                    else:
                        extend_start = False
                if extend_end:
                    extension_point = get_extension_point(polyline[-1])  # extend end of polyline
                    if extension_point is not None:
                        polyline.append(extension_point)
                    else:
                        extend_end = False
            polylines.append(polyline)
        return polylines
