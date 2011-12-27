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

# <pep8-80 compliant>

__all__ = (
    "region_2d_to_vector_3d",
    "region_2d_to_location_3d",
    "location_3d_to_region_2d",
    )


def region_2d_to_vector_3d(region, rv3d, coord):
    """
    Return a direction vector from the viewport at the specific 2d region
    coordinate.

    :arg region: region of the 3D viewport, typically bpy.context.region.
    :type region: :class:`bpy.types.Region`
    :arg rv3d: 3D region data, typically bpy.context.space_data.region_3d.
    :type rv3d: :class:`bpy.types.RegionView3D`
    :arg coord: 2d coordinates relative to the region:
       (event.mouse_region_x, event.mouse_region_y) for example.
    :type coord: 2d vector
    :return: normalized 3d vector.
    :rtype: :class:`mathutils.Vector`
    """
    from mathutils import Vector

    viewinv = rv3d.view_matrix.inverted()
    if rv3d.is_perspective:
        persinv = rv3d.perspective_matrix.inverted()

        out = Vector(((2.0 * coord[0] / region.width) - 1.0,
                      (2.0 * coord[1] / region.height) - 1.0,
                      -0.5
                    ))

        w = out.dot(persinv[3].xyz) + persinv[3][3]

        return ((persinv * out) / w) - viewinv.translation
    else:
        return viewinv.col[2].xyz.normalized()


def region_2d_to_location_3d(region, rv3d, coord, depth_location):
    """
    Return a 3d location from the region relative 2d coords, aligned with
    *depth_location*.

    :arg region: region of the 3D viewport, typically bpy.context.region.
    :type region: :class:`bpy.types.Region`
    :arg rv3d: 3D region data, typically bpy.context.space_data.region_3d.
    :type rv3d: :class:`bpy.types.RegionView3D`
    :arg coord: 2d coordinates relative to the region;
       (event.mouse_region_x, event.mouse_region_y) for example.
    :type coord: 2d vector
    :arg depth_location: the returned vectors depth is aligned with this since
       there is no defined depth with a 2d region input.
    :type depth_location: 3d vector
    :return: normalized 3d vector.
    :rtype: :class:`mathutils.Vector`
    """
    from mathutils import Vector
    from mathutils.geometry import intersect_point_line

    persmat = rv3d.perspective_matrix.copy()
    viewinv = rv3d.view_matrix.inverted()
    coord_vec = region_2d_to_vector_3d(region, rv3d, coord)
    depth_location = Vector(depth_location)

    if rv3d.is_perspective:
        from mathutils.geometry import intersect_line_plane

        origin_start = viewinv.translation.copy()
        origin_end = origin_start + coord_vec
        view_vec = viewinv.col[2].copy()
        return intersect_line_plane(origin_start,
                                    origin_end,
                                    depth_location,
                                    view_vec, 1,
                                    )
    else:
        dx = (2.0 * coord[0] / region.width) - 1.0
        dy = (2.0 * coord[1] / region.height) - 1.0
        persinv = persmat.inverted()
        viewinv = rv3d.view_matrix.inverted()
        origin_start = ((persinv.col[0].xyz * dx) +
                        (persinv.col[1].xyz * dy) +
                         viewinv.translation)
        origin_end = origin_start + coord_vec
        return intersect_point_line(depth_location,
                                    origin_start,
                                    origin_end,
                                    )[0]


def location_3d_to_region_2d(region, rv3d, coord):
    """
    Return the *region* relative 2d location of a 3d position.

    :arg region: region of the 3D viewport, typically bpy.context.region.
    :type region: :class:`bpy.types.Region`
    :arg rv3d: 3D region data, typically bpy.context.space_data.region_3d.
    :type rv3d: :class:`bpy.types.RegionView3D`
    :arg coord: 3d worldspace location.
    :type coord: 3d vector
    :return: 2d location
    :rtype: :class:`mathutils.Vector`
    """
    from mathutils import Vector

    prj = rv3d.perspective_matrix * Vector((coord[0], coord[1], coord[2], 1.0))
    if prj.w > 0.0:
        width_half = region.width / 2.0
        height_half = region.height / 2.0

        return Vector((width_half + width_half * (prj.x / prj.w),
                       height_half + height_half * (prj.y / prj.w),
                       ))
    else:
        return None
