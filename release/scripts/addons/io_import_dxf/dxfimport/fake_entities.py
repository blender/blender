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


class ArcEntity:
    """
    Used in convert.bulge_to_cubic() since bulges define how much a straight polyline segment should be transformed to
    an arc. ArcEntity is just used to call do.arc() without having a real Arc-Entity from dxfgrabber.
    """
    def __init__(self, start, end, center, radius, angdir):
        self.start_angle = start
        self.end_angle = end
        self.center = center
        self.radius = radius
        self.angdir = angdir

    def __str__(self):
        return "startangle: %s, endangle: %s, center: %s, radius: %s, angdir: %s" % \
               (str(self.start_angle), str(self.end_angle), str(self.center), str(self.radius), str(self.angdir))


class LineEntity:
    """
    Used in do._gen_meshface()
    """
    def __init__(self, start, end):
        self.start = start
        self.end = end
