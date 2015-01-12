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
    "region_2d_to_origin_3d",
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

        view_vector = ((persinv * out) / w) - viewinv.translation
    else:
        view_vector = -viewinv.col[2].xyz

    view_vector.normalize()

    return view_vector


def region_2d_to_origin_3d(region, rv3d, coord, clamp=None):
    """
    Return the 3d view origin from the region relative 2d coords.

    .. note::

       Orthographic views have a less obvious origin, the far clip is used to define the viewport near/far extents.
       Since far clip can be a very large value, the result may give with numeric precision issues.

       To avoid this problem, you can optionally clamp the far clip to a smaller value
       based on the data you're operating on.

    :arg region: region of the 3D viewport, typically bpy.context.region.
    :type region: :class:`bpy.types.Region`
    :arg rv3d: 3D region data, typically bpy.context.space_data.region_3d.
    :type rv3d: :class:`bpy.types.RegionView3D`
    :arg coord: 2d coordinates relative to the region;
       (event.mouse_region_x, event.mouse_region_y) for example.
    :type coord: 2d vector
    :arg clamp: Clamp the maximum far-clip value used.
       (negative value will move the offset away from the view_location)
    :type clamp: float or None
    :return: The origin of the viewpoint in 3d space.
    :rtype: :class:`mathutils.Vector`
    """
    viewinv = rv3d.view_matrix.inverted()

    if rv3d.is_perspective:
        origin_start = viewinv.translation.copy()
    else:
        persmat = rv3d.perspective_matrix.copy()
        dx = (2.0 * coord[0] / region.width) - 1.0
        dy = (2.0 * coord[1] / region.height) - 1.0
        persinv = persmat.inverted()
        origin_start = ((persinv.col[0].xyz * dx) +
                        (persinv.col[1].xyz * dy) +
                        viewinv.translation)

        if clamp != 0.0:
            if rv3d.view_perspective != 'CAMERA':
                # this value is scaled to the far clip already
                origin_offset = persinv.col[2].xyz
                if clamp is not None:
                    if clamp < 0.0:
                        origin_offset.negate()
                        clamp = -clamp
                    if origin_offset.length > clamp:
                        origin_offset.length = clamp

                origin_start -= origin_offset

    return origin_start


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

    coord_vec = region_2d_to_vector_3d(region, rv3d, coord)
    depth_location = Vector(depth_location)

    origin_start = region_2d_to_origin_3d(region, rv3d, coord)
    origin_end = origin_start + coord_vec

    if rv3d.is_perspective:
        from mathutils.geometry import intersect_line_plane
        viewinv = rv3d.view_matrix.inverted()
        view_vec = viewinv.col[2].copy()
        return intersect_line_plane(origin_start,
                                    origin_end,
                                    depth_location,
                                    view_vec, 1,
                                    )
    else:
        from mathutils.geometry import intersect_point_line
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
