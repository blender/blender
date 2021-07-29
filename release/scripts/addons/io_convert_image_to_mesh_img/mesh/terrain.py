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

"""Objects for creating 3D models in Blender"""

import bpy
import bmesh

import numpy as np

from .triangulate import Triangulate


class BTerrain:
    """
    Functions for creating Blender meshes from DTM objects

    This class contains functions that convert DTM objects to Blender meshes.
    Its main responsiblity is to triangulate a mesh from the elevation data in
    the DTM. Additionally, it attaches some metadata to the object and creates
    a UV map for it so that companion ortho-images drape properly.

    This class provides two public methods: `new()` and `reload()`.

    `new()` creates a new object[1] and attaches a new mesh to it.

    `reload()` replaces the mesh that is attached to an already existing
    object. This allows us to retain the location and orientation of the parent
    object's coordinate system but to reload the terrain at a different
    resolution.

    Notes
    ----------
    [1] If you're unfamiliar with Blender, one thing that will help you in
        reading this code is knowing the difference between 'meshes' and
        'objects'. A mesh is just a collection of vertices, edges and
        faces. An object may have a mesh as a child data object and
        contains additional information, e.g. the location and orientation
        of the coordinate system its child-meshes are reckoned in terms of.

    """

    @staticmethod
    def new(dtm, name='Terrain'):
        """
        Loads a new terrain

        Parameters
        ----------
        dtm : DTM
        name : str, optional
            The name that will be assigned to the new object, defaults
            to 'Terrain' (and, if an object named 'Terrain' already
            exists, Blender will automatically extend the name of the
            new object to something like 'Terrain.001')

        Returns
        ----------
        obj : bpy_types.Object

        """
        bpy.ops.object.add(type="MESH")
        obj = bpy.context.object
        obj.name = name

        # Fill the object data with a Terrain mesh
        obj.data = BTerrain._mesh_from_dtm(dtm)

        # Add some meta-information to the object
        metadata = BTerrain._create_metadata(dtm)
        BTerrain._setobjattrs(obj, **metadata)

        # Center the mesh to its origin and create a UV map for draping
        # ortho images.
        BTerrain._center(obj)

        return obj

    @staticmethod
    def reload(obj, dtm):
        """
        Replaces an exisiting object's terrain mesh

        This replaces an object's mesh with a new mesh, transferring old
        materials over to the new mesh. This is useful for reloading DTMs
        at different resolutions but maintaining textures/location/rotation.

        Parameters
        -----------
        obj : bpy_types.Object
            An already existing Blender object
        dtm : DTM

        Returns
        ----------
        obj : bpy_types.Object

        """
        old_mesh = obj.data
        new_mesh = BTerrain._mesh_from_dtm(dtm)

        # Copy any old materials to the new mesh
        for mat in old_mesh.materials:
            new_mesh.materials.append(mat.copy())

        # Swap out the old mesh for the new one
        obj.data = new_mesh

        # Update out-dated meta-information
        metadata = BTerrain._create_metadata(dtm)
        BTerrain._setobjattrs(obj, **metadata)

        # Center the mesh to its origin and create a UV map for draping
        # ortho images.
        BTerrain._center(obj)

        return obj

    @staticmethod
    def _mesh_from_dtm(dtm, name='Terrain'):
        """
        Creates a Blender *mesh* from a DTM

        Parameters
        ----------
        dtm : DTM
        name : str, optional
            The name that will be assigned to the new mesh, defaults
            to 'Terrain' (and, if an object named 'Terrain' already
            exists, Blender will automatically extend the name of the
            new object to something like 'Terrain.001')

        Returns
        ----------
        mesh : bpy_types.Mesh

        Notes
        ----------
        * We are switching coordinate systems from the NumPy to Blender.

              Numpy:            Blender:
               + ----> (0, j)    ^ (0, y)
               |                 |
               |                 |
               v (i, 0)          + ----> (x, 0)

        """
        # Create an empty mesh
        mesh = bpy.data.meshes.new(name)

        # Get the xy-coordinates from the DTM, see docstring notes
        y, x = np.indices(dtm.data.shape).astype('float64')
        x *= dtm.mesh_scale
        y *= -1 * dtm.mesh_scale

        # Create an array of 3D vertices
        vertices = np.dstack([x, y, dtm.data]).reshape((-1, 3))

        # Drop vertices with NaN values (used in the DTM to represent
        # areas with no data)
        vertices = vertices[~np.isnan(vertices).any(axis=1)]

        # Calculate the faces of the mesh
        triangulation = Triangulate(dtm.data)
        faces = triangulation.face_list()

        # Fill the mesh
        mesh.from_pydata(vertices, [], faces)
        mesh.update()

        # Create a new UV layer
        mesh.uv_textures.new("HiRISE Generated UV Map")
        # We'll use a bmesh to populate the UV map with values
        bm = bmesh.new()
        bm.from_mesh(mesh)
        bm.faces.ensure_lookup_table()
        uv_layer = bm.loops.layers.uv[0]

        # Iterate over each face in the bmesh
        num_faces = len(bm.faces)
        w = dtm.data.shape[1]
        h = dtm.data.shape[0]
        for face_index in range(num_faces):
            # Iterate over each loop in the face
            for loop in bm.faces[face_index].loops:
                # Get this loop's vertex coordinates
                vert_coords = loop.vert.co.xy
                # And calculate it's uv coordinate. We do this by dividing the
                # vertice's x and y coordinates by:
                #
                #     d + 1, dimensions of DTM (in "posts")
                #     mesh_scale, meters/DTM "post"
                #
                # This has the effect of mapping the vertex to its
                # corresponding "post" index in the DTM, and then mapping
                # that value to the range [0, 1).
                u = vert_coords.x / ((w + 1) * dtm.mesh_scale)
                v = 1 + vert_coords.y / ((h + 1) * dtm.mesh_scale)
                loop[uv_layer].uv = (u, v)

        bm.to_mesh(mesh)

        return mesh

    @staticmethod
    def _center(obj):
        """Move object geometry to object origin"""
        bpy.context.scene.objects.active = obj
        bpy.ops.object.origin_set(center='BOUNDS')

    @staticmethod
    def _setobjattrs(obj, **attrs):
        for key, value in attrs.items():
            obj[key] = value

    @staticmethod
    def _create_metadata(dtm):
        """Returns a dict containing meta-information about a DTM"""
        return {
            'PATH': dtm.path,
            'MESH_SCALE': dtm.mesh_scale,
            'DTM_RESOLUTION': dtm.terrain_resolution,
            'BIN_SIZE': dtm.bin_size,
            'MAP_SIZE': dtm.map_size,
            'MAP_SCALE': dtm.map_scale * dtm.unit_scale,
            'UNIT_SCALE': dtm.unit_scale,
            'IS_TERRAIN': True,
            'HAS_UV_MAP': True
        }
