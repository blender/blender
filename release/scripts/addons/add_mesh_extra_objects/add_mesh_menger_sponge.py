# -*- coding: utf-8 -*-
# Copyright (c) 2015 sugiany
# This file is distributed under the MIT License. See the LICENSE.md for more details.

import bpy
from bpy.props import (
        IntProperty,
        BoolProperty,
        BoolVectorProperty,
        FloatVectorProperty,
        FloatProperty,
        )

import mathutils
import copy


class MengerSponge(object):
    FACE_INDICES = [
        [3, 7, 4, 0],
        [5, 6, 2, 1],
        [1, 2, 3, 0],
        [7, 6, 5, 4],
        [4, 5, 1, 0],
        [2, 6, 7, 3],
    ]

    def __init__(self, level):
        self.__level = level
        self.__max_point_number = 3 ** level
        self.__vertices_map = {}
        self.__indices = []
        self.__face_visibility = {}
        self.__faces = []

        for x in range(3):
            for y in range(3):
                for z in range(3):
                    self.__face_visibility[(x, y, z)] = [
                        x == 0 or x == 2 and (y == 1 or z == 1),
                        x == 2 or x == 0 and (y == 1 or z == 1),
                        y == 0 or y == 2 and (x == 1 or z == 1),
                        y == 2 or y == 0 and (x == 1 or z == 1),
                        z == 0 or z == 2 and (y == 1 or x == 1),
                        z == 2 or z == 0 and (y == 1 or x == 1),
                    ]

    def create(self, width, height):
        m = self.__max_point_number
        points = [
            (0, 0, 0),
            (m, 0, 0),
            (m, 0, m),
            (0, 0, m),
            (0, m, 0),
            (m, m, 0),
            (m, m, m),
            (0, m, m),
            ]
        self.__make_sub_sponge(points, None, self.__level)
        vertices = self.__make_vertices(width, height)
        return vertices, self.__faces

    def __get_vindex(self, p):
        if p in self.__vertices_map:
            return self.__vertices_map[p]
        index = len(self.__vertices_map)
        self.__vertices_map[p] = index
        return index

    def __make_vertices(self, width, height):
        vertices = [None] * len(self.__vertices_map)
        w2 = width / 2
        h2 = height / 2
        w_step = width / self.__max_point_number
        h_step = height / self.__max_point_number
        for p, i in sorted(self.__vertices_map.items(), key=lambda x: x[1]):
            vertices[i] = mathutils.Vector([
                    p[0] * w_step - w2,
                    p[1] * w_step - w2,
                    p[2] * h_step - h2,
                    ])
        return vertices

    def __make_sub_sponge(self, cur_points, face_vis, depth):
        if depth <= 0:
            if not face_vis:
                face_vis = [True] * 6
            cur_point_indices = []
            for p in cur_points:
                cur_point_indices.append(self.__get_vindex(p))
            for i, vis in enumerate(face_vis):
                if vis:
                    f = []
                    for vi in self.FACE_INDICES[i]:
                        f.append(cur_point_indices[vi])
                    self.__faces.append(f)
            return

        base = cur_points[0]
        width = (cur_points[1][0] - base[0]) / 3
        local_vert_map = {}
        for z in range(4):
            for y in range(4):
                for x in range(4):
                    local_vert_map[(x, y, z)] = (
                        width * x + base[0],
                        width * y + base[1],
                        width * z + base[2],
                        )

        for x in range(3):
            for y in range(3):
                for z in range(3):
                    if [x, y, z].count(1) > 1:
                        continue
                    next_points = [
                        local_vert_map[(x, y, z)],
                        local_vert_map[(x + 1, y, z)],
                        local_vert_map[(x + 1, y, z + 1)],
                        local_vert_map[(x, y, z + 1)],
                        local_vert_map[(x, y + 1, z)],
                        local_vert_map[(x + 1, y + 1, z)],
                        local_vert_map[(x + 1, y + 1, z + 1)],
                        local_vert_map[(x, y + 1, z + 1)],
                    ]
                    visibility = copy.copy(self.__face_visibility[(x, y, z)])
                    if face_vis:
                        visibility[0] = visibility[0] and (face_vis[0] or x != 0)
                        visibility[1] = visibility[1] and (face_vis[1] or x != 2)
                        visibility[2] = visibility[2] and (face_vis[2] or y != 0)
                        visibility[3] = visibility[3] and (face_vis[3] or y != 2)
                        visibility[4] = visibility[4] and (face_vis[4] or z != 0)
                        visibility[5] = visibility[5] and (face_vis[5] or z != 2)
                    self.__make_sub_sponge(
                        next_points,
                        visibility,
                        depth - 1)


class AddMengerSponge(bpy.types.Operator):
    bl_idname = "mesh.menger_sponge_add"
    bl_label = "Menger Sponge"
    bl_description = "Construct a menger sponge mesh"
    bl_options = {'REGISTER', 'UNDO'}

    level = IntProperty(
            name="Level",
            description="Sponge Level",
            min=0, max=4,
            default=1,
            )
    radius = FloatProperty(
            name="Width",
            description="Sponge Radius",
            min=0.01, max=100.0,
            default=1.0,
            )
    # generic transform props
    view_align = BoolProperty(
            name="Align to View",
            default=False,
            )
    location = FloatVectorProperty(
            name="Location",
            subtype='TRANSLATION',
            )
    rotation = FloatVectorProperty(
            name="Rotation",
            subtype='EULER',
            )
    layers = BoolVectorProperty(
            name="Layers",
            size=20,
            subtype='LAYER',
            options={'HIDDEN', 'SKIP_SAVE'},
            )

    def execute(self, context):
        sponger = MengerSponge(self.level)
        vertices, faces = sponger.create(self.radius * 2, self.radius * 2)
        del sponger

        mesh = bpy.data.meshes.new(name='Sponge')
        mesh.from_pydata(vertices, [], faces)
        uvs = [(0.0, 0.0), (0.0, 1.0), (1.0, 1.0), (1.0, 0.0)]
        mesh.uv_textures.new()
        for i, uvloop in enumerate(mesh.uv_layers.active.data):
            uvloop.uv = uvs[i % 4]

        from bpy_extras import object_utils
        object_utils.object_data_add(context, mesh, operator=self)

        return {'FINISHED'}
