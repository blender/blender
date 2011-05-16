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


def region_2d_to_vector_3d(region, rv3d, coord):
    """
    Return a direction vector from the viewport at the spesific 2d region
    coordinate.

    :arg region: region of the 3D viewport, typically bpy.context.region.
    :type region: :class:`Region`
    :arg rv3d: 3D region data, typically bpy.context.space_data.region_3d.
    :type rv3d: :class:`RegionView3D`
    :arg coord: 2d coordinates relative to the region;
       (event.mouse_region_x, event.mouse_region_y) for example.
    :type coord: 2d vector
    :return: normalized 3d vector.
    :rtype: :class:`Vector`
    """
    from mathutils import Vector

    dx = (2.0 * coord[0] / region.width) - 1.0
    dy = (2.0 * coord[1] / region.height) - 1.0

    viewvec = rv3d.view_matrix.inverted()[2].to_3d().normalized()
    perspinv_x, perspinv_y = rv3d.perspective_matrix.inverted().to_3x3()[0:2]
    return ((perspinv_x * dx + perspinv_y * dy) - viewvec).normalized()


def region_2d_to_location_3d(region, rv3d, coord, depth_location):
    """
    Return a 3d location from the region relative 2d coords, aligned with
    *depth_location*.

    :arg region: region of the 3D viewport, typically bpy.context.region.
    :type region: :class:`Region`
    :arg rv3d: 3D region data, typically bpy.context.space_data.region_3d.
    :type rv3d: :class:`RegionView3D`
    :arg coord: 2d coordinates relative to the region;
       (event.mouse_region_x, event.mouse_region_y) for example.
    :type coord: 2d vector
    :arg depth_location: the returned vectors depth is aligned with this since
       there is no defined depth with a 2d region input.
    :type depth_location: 3d vector
    :return: normalized 3d vector.
    :rtype: :class:`Vector`
    """
    from mathutils.geometry import intersect_point_line
    origin_start = rv3d.view_matrix.inverted()[3].to_3d()
    origin_end = origin_start + region_2d_to_vector_3d(region, rv3d, coord)
    return intersect_point_line(depth_location, origin_start, origin_end)[0]


def location_3d_to_region_2d(region, rv3d, coord):
    """
    Return the *region* relative 2d location of a 3d position.

    :arg region: region of the 3D viewport, typically bpy.context.region.
    :type region: :class:`Region`
    :arg rv3d: 3D region data, typically bpy.context.space_data.region_3d.
    :type rv3d: :class:`RegionView3D`
    :arg coord: 3d worldspace location.
    :type coord: 3d vector
    :return: 2d location
    :rtype: :class:`Vector`
    """
    prj = Vector((coord[0], coord[1], coord[2], 1.0)) * rv3d.perspective_matrix
    if prj.w > 0.0:
        width_half = region.width / 2.0
        height_half = region.height / 2.0

        return Vector((width_half + width_half * (prj.x / prj.w),
                       height_half + height_half * (prj.y / prj.w),
                       ))
    else:
        return None
