import re
import random

import bpy
from bpy.props import IntProperty
import mathutils
from mathutils import Vector, Matrix


bpy.types.Scene.SvGreekAlphabet_index = IntProperty(default=0, min=0, max=23)
greek_alphabet = [
    'Alpha', 'Beta', 'Gamma', 'Delta',
    'Epsilon', 'Zeta', 'Eta', 'Theta',
    'Iota', 'Kappa', 'Lamda', 'Mu',
    'Nu', 'Xi', 'Omicron', 'Pi',
    'Rho', 'Sigma', 'Tau', 'Upsilon',
    'Phi', 'Chi', 'Psi', 'Omega']


def matrix_sanitizer(matrix):
    #  reduces all values below threshold (+ or -) to 0.0, to avoid meaningless
    #  wandering floats.
    # print(matrix)
    coord_strip = lambda c: 0.0 if (-1.6e-5 <= c <= 1.6e-5) else c
    san = lambda v: Vector((coord_strip(c) for c in v[:]))
    return Matrix([san(v) for v in matrix])


def natural_plus_one(object_names):

    ''' sorts ['Alpha', 'Alpha1', 'Alpha11', 'Alpha2', 'Alpha23']
        into ['Alpha', 'Alpha1', 'Alpha2', 'Alpha11', 'Alpha23']
        and returns (23+1)
    '''

    def extended_sort(a):
        ''' finds the digit trailing, or 0 if no digits '''
        k = re.split('(\d*)', a)
        return 0 if len(k) == 1 else int(k[1])

    natural_sort = sorted(object_names, key=extended_sort)
    last = natural_sort[-1]
    num = extended_sort(last)
    return num+1


def get_random_init():
    objects = bpy.data.objects

    with_underscore = lambda obj: '_' in obj.name and obj.type == 'MESH'
    names_with_underscores = list(filter(with_underscore, objects))
    print(names_with_underscores)
    set_of_names_pre_underscores = set([n.name.split('_')[0] for n in names_with_underscores])
    if '' in set_of_names_pre_underscores:
        set_of_names_pre_underscores.remove('')

    n = random.choice(greek_alphabet)

    # not picked yet.
    if not n in set_of_names_pre_underscores:
        return n

    # at this point the name was already picked, we don't want to overwrite
    # existing obj/meshes and instead append digits onto the greek letter
    # if Alpha is present already a new one will be Alpha2, Alpha3 etc..
    # (not Alpha002, or Alpha.002)
    similar_names = [name for name in set_of_names_pre_underscores if n in name]
    plus_one = natural_plus_one(similar_names)
    return n + str(plus_one)


def get_children(node, kind='MESH'):
    # critera, basename must be in object.keys and the value must be self.basemesh_name
    objects = bpy.data.objects
    objs = [obj for obj in objects if obj.type == kind]
    return [o for o in objs if o.get('basename') == node.basemesh_name]


def remove_non_updated_objects(node, obj_index, kind='MESH'):
    objs = get_children(node, kind)
    objs = [obj.name for obj in objs if obj['idx'] > obj_index]
    if not objs:
        return

    if kind == 'MESH':
        kinds = bpy.data.meshes
    elif kind == 'CURVE':
        kinds = bpy.data.curves

    objects = bpy.data.objects
    scene = bpy.context.scene

    # remove excess objects
    for object_name in objs:
        obj = objects[object_name]
        obj.hide_select = False
        scene.objects.unlink(obj)
        objects.remove(obj, do_unlink=True)

    # delete associated meshes
    for object_name in objs:
        kinds.remove(kinds[object_name])        
