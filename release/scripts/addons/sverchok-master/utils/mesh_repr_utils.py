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

import bpy
import itertools


def xjoined(structure):
    """ returns a flat list of vertex indices that represent polygons,
    example input: [[0,1,2], [1,2,3], [2,3,4,5]]
    example output: [3, 0, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4, 5]
                     |           |           |
                     polygon length tokens
    """
    if not structure:
        return []
    faces = []
    fex = faces.extend
    fap = faces.append
    len_gen = (len(p) for p in structure)
    [(fap(lp), fex(face)) for lp, face in zip(len_gen, structure)]
    return faces


def flatten(data):
    # returns empty lists if the data[x] input is empty
    return {
        'Vertices': list(itertools.chain.from_iterable(data['Vertices'])),
        'Edges': list(itertools.chain.from_iterable(data['Edges'])),
        'Polygons': xjoined(data['Polygons']),
        'Matrix': list(itertools.chain.from_iterable(data['Matrix']))
    }


def unroll(data, stride=None, constant=True):
    if constant == False:
        # stride is variable, means we are unrolling polygons
        polygons = []
        pap = polygons.append
        index = 0
        while(index < len(data)):
            length = data[index]
            index += 1
            segment = data[index: index+length]
            pap(segment)
            index += length
        return polygons
    if stride and stride in {2, 3, 4}:
        # 2 = edges, 3 = vertex, 4 = matrix
        return [data[i:i+stride] for i in range(0, len(data), stride)]


def unflatten(data):
    return {
        'Vertices': unroll(data['Vertices'], stride=3),
        'Edges': [] or unroll(data['Edges'], stride=2),
        'Polygons': [] or unroll(data['Polygons'], constant=False),
        'Matrix': unroll(data['Matrix'], stride=4)
    }


def generate_object(name, bm):
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()

    # create object and link to scene
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.objects.link(obj)
    return obj 


if __name__ == "__main__":

    # polygons
    somelist = [[0,1,2], [1,2,3], [2,3,4,5]]

    f = xjoined(somelist)
    xf = unroll(f, constant=False)
    print(f)
    print(xf)

    # edges
    some_edges = [3, 0, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4]
    xe = unroll(some_edges, stride=2)
    print(xe)

    data = {
        'Vertices': [0.3, 0.3, 0.2, 0.1, 0.3, 0.6, 0.1, 0.6, 0.2, 0.3, 0.5, 0.6],
        'Edges' : [0, 1, 3, 6, 2, 5, 3, 6],
        'Polygons': [3, 0, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4, 5],
        'Matrix': [0.3, 0.3, 0.2, 0.2, 0.3, 0.3, 0.2, 0.2, 0.3, 0.3, 0.2, 0.2, 0.3, 0.3, 0.2, 0.2]

    }
    undata = unflatten(data)
    print(undata)

    



