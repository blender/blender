# This file is a part of the HiRISE DTM Importer for Blender
#
# Copyright (C) 2017 Arizona Board of Regents on behalf of the Planetary Image
# Research Laboratory, Lunar and Planetary Laboratory at the University of
# Arizona.
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program.  If not, see <http://www.gnu.org/licenses/>.

"""Triangulation algorithms"""

import numpy as np


class Triangulate:
    """
    A triangulation algorithm for creating a mesh from a DTM raster.

    I have been re-writing parts of the Blender HiRISE DTM importer in an
    effort to cull its dependencies on external packages. Originally, the
    add-on relied on SciPy's Delaunay triangulation (really a wrapper for
    Qhull's Delaunay triangulation) to triangulate a mesh from a HiRISE DTM.

    This re-write is much better suited to the problem domain. The SciPy
    Delaunay triangulation creates a mesh from any arbitrary point cloud and,
    while robust, doesn't care about the fact that our HiRISE DTMs are
    regularly gridded rasters. This triangulation algorithm is less robust
    but much faster. Credit is due to Tim Spriggs for his work on the previous
    Blender HiRISE DTM importer --- this triangulation algorithm largely
    models the one in his add-on with a few changes (namely interfacing
    with NumPy's API).

    Overview
    ----------
    Suppose we have a DTM:

    .. code::

                -  -  -  -  -  -  -  -  X  X  -  -  -  -  -
                -  -  -  -  -  -  X  X  X  X  X  -  -  -  -
                -  -  -  -  X  X  X  X  X  X  X  X  -  -  -
                -  -  X  X  X  X  X  X  X  X  X  X  X  -  -
                X  X  X  X  X  X  X  X  X  X  X  X  X  X  -
                -  X  X  X  X  X  X  X  X  X  X  X  X  X  X
                -  -  X  X  X  X  X  X  X  X  X  X  X  -  -
                -  -  -  X  X  X  X  X  X  X  X  -  -  -  -
                -  -  -  -  X  X  X  X  X  -  -  -  -  -  -
                -  -  -  -  -  X  X  -  -  -  -  -  -  -  -

    where 'X' represents valid values and '-' represents invalid values.
    Valid values should become vertices in the resulting mesh, invalid
    values should be ignored.

    Our end goal is to supply Blender with:

        1. an (n x 3) list of vertices

        2. an (m x 3) list of faces.

    A vertex is a 3-tuple that we get from the DTM raster array. The
    z-coordinate is whatever elevation value is in the DTM and the xy-
    coordinates are the image indices multiplied by the resolution of the
    DTM (e.g. if the DTM is at 5m/px, the first vertex is at (0m, 0m,
    z_00) and the vertex to the right of it is at (5m, 0m, z_01)).

    A face is a 3-tuple (because we're using triangles) where each element
    is an index of a vertex in the vertices list. Computing the faces is
    tricky because we want to leverage the orthogonal structure of the DTM
    raster for computational efficiency but we also need to reference
    vertex indices in our faces, which don't observe any regular
    structure.

    We take two rows at a time from the DTM raster and track the *raster
    row* indices as well as well as the *vertex* indices. Raster row
    indices are the distance of a pixel in the raster from the left-most
    (valid *or* invalid) pixel of the row. The first vertex is index 0 and
    corresponds to the upperleft-most valid pixel in the DTM raster.
    Vertex indices increase to the right and then down.

    For example, the first two rows:

    .. code::

                -  -  -  -  -  -  -  -  X  X  -  -  -  -  -
                -  -  -  -  -  -  X  X  X  X  X  -  -  -  -

    in vertex indices:

    .. code::

                -  -  -  -  -  -  -  -  0  1  -  -  -  -  -
                -  -  -  -  -  -  2  3  4  5  6  -  -  -  -

    and in raster row indices:

    .. code::

                -  -  -  -  -  -  -  -  9 10  -  -  -  -  -
                -  -  -  -  -  -  7  8  9 10 11  -  -  -  -

    To simplify, we will only add valid square regions to our mesh. So,
    for these first two rows the only region that will be added to our
    mesh is the quadrilateral formed by vertices 0, 1, 4 and 5. We
    further divide this area into 2 triangles and add the vertices to the
    face list in CCW order (i.e. t1: (4, 1, 0), t2: (4, 5, 1)).

    After the triangulation between two rows is completed, the bottom
    row is cached as the top row and the next row in the DTM raster is
    read as the new bottom row. This process continues until the entire
    raster has been triangulated.

    Todo
    ---------
    * It should be pretty trivial to add support for triangular
      regions (i.e. in the example above, also adding the triangles
      formed by (3, 4, 0) and (5, 6, 1)).

    """
    def __init__(self, array):
        self.array = array
        self.faces = self._triangulate()

    def _triangulate(self):
        """Triangulate a mesh from a topography array."""
        # Allocate memory for the triangles array
        max_tris = (self.array.shape[0] - 1) * (self.array.shape[1] - 1) * 2
        tris = np.zeros((max_tris, 3), dtype=int)
        ntri = 0

        # We initialize a vertex counter at 0
        prev_vtx_start = 0
        # We don't care about the values in the array, just whether or not
        # they are valid.
        prev = ~np.isnan(self.array[0])
        # We can sum this boolean array to count the number of valid entries
        prev_num_valid = prev.sum()
        # TODO: Probably a more clear (and faster) function than argmax for
        # getting the first Truth-y value in a 1d array.
        prev_img_start = np.argmax(prev)

        # Start quadrangulation
        for i in range(1, self.array.shape[0]):
            # Fetch this row, get our bearings in image *and* vertex space
            curr = ~np.isnan(self.array[i])
            curr_vtx_start = prev_vtx_start + prev_num_valid
            curr_img_start = np.argmax(curr)
            curr_num_valid = curr.sum()
            # Find the overlap between this row and the previous one
            overlap = np.logical_and(prev, curr)
            num_tris = overlap.sum() - 1
            overlap_start = np.argmax(overlap)
            # Store triangles
            for j in range(num_tris):
                curr_pad = overlap_start - curr_img_start + j
                prev_pad = overlap_start - prev_img_start + j
                tris[ntri + 0] = [
                    curr_vtx_start + curr_pad,
                    prev_vtx_start + prev_pad + 1,
                    prev_vtx_start + prev_pad
                ]
                tris[ntri + 1] = [
                    curr_vtx_start + curr_pad,
                    curr_vtx_start + curr_pad + 1,
                    prev_vtx_start + prev_pad + 1
                ]
                ntri += 2
            # Cache current row as previous row
            prev = curr
            prev_vtx_start = curr_vtx_start
            prev_img_start = curr_img_start
            prev_num_valid = curr_num_valid

        return tris[:ntri]

    def face_list(self):
        return list(self.faces)
