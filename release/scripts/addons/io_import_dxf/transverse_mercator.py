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

from math import sin, cos, atan, atanh, radians, tan, sinh, asin, cosh, degrees

# see conversion formulas at
# http://en.wikipedia.org/wiki/Transverse_Mercator_projection
# http://mathworld.wolfram.com/MercatorProjection.html


class TransverseMercator:
    radius = 6378137

    def __init__(self, lat=0, lon=0):
        self.lat = lat  # in degrees
        self.lon = lon  # in degrees
        self.lat_rad = radians(self.lat)
        self.lon_rad = radians(self.lon)

    def fromGeographic(self, lat, lon):
        lat_rad = radians(lat)
        lon_rad = radians(lon)
        B = cos(lat_rad) * sin(lon_rad - self.lon_rad)
        x = self.radius * atanh(B)
        y = self.radius * (atan(tan(lat_rad) / cos(lon_rad - self.lon_rad)) - self.lat_rad)
        return x, y

    def toGeographic(self, x, y):
        x /= self.radius
        y /= self.radius
        D = y + self.lat_rad
        lon = atan(sinh(x) / cos(D))
        lat = asin(sin(D) / cosh(x))

        lon = self.lon + degrees(lon)
        lat = degrees(lat)
        return lat, lon
