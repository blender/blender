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
from bpy.props import *
import os
import random
from mathutils import *


def create_cutter(context, crack_type, scale, roughness):
    ncuts = 12
    if crack_type == 'FLAT' or crack_type == 'FLAT_ROUGH':
        bpy.ops.mesh.primitive_cube_add(
            view_align=False,
            enter_editmode=False,
            location=(0, 0, 0),
            rotation=(0, 0, 0),
            layers=context.scene.layers)

        for v in context.active_object.data.vertices:
            v.co[0] += 1.0
            v.co *= scale

        bpy.ops.object.editmode_toggle()
        bpy.ops.mesh.faces_shade_smooth()
        bpy.ops.uv.reset()

        if crack_type == 'FLAT_ROUGH':
            bpy.ops.mesh.subdivide(
                number_cuts=ncuts,
                fractal=roughness * 7 * scale,
                smoothness=0)

            bpy.ops.mesh.vertices_smooth(repeat=5)

        bpy.ops.object.editmode_toggle()

    if crack_type == 'SPHERE' or crack_type == 'SPHERE_ROUGH':
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=4,
            size=1,
            view_align=False,
            enter_editmode=False,
            location=(0, 0, 0),
            rotation=(0, 0, 0),
            layers=context.scene.layers)

        bpy.ops.object.editmode_toggle()
        bpy.ops.mesh.faces_shade_smooth()
        bpy.ops.uv.smart_project(angle_limit=66, island_margin=0)

        bpy.ops.object.editmode_toggle()
        for v in context.active_object.data.vertices:
            v.co[0] += 1.0
            v.co *= scale

        if crack_type == 'SPHERE_ROUGH':
            for v in context.scene.objects.active.data.vertices:
                v.co[0] += roughness * scale * 0.2 * (random.random() - 0.5)
                v.co[1] += roughness * scale * 0.1 * (random.random() - 0.5)
                v.co[2] += roughness * scale * 0.1 * (random.random() - 0.5)

    bpy.context.active_object.select = True
#    bpy.context.scene.objects.active.select = True

    '''
    # Adding fracture material
    # @todo Doesn't work at all yet.
    sce = bpy.context.scene
    if bpy.data.materials.get('fracture') is None:
        bpy.ops.material.new()
        bpy.ops.object.material_slot_add()
        sce.objects.active.material_slots[0].material.name = 'fracture'
    else:
        bpy.ops.object.material_slot_add()
        sce.objects.active.material_slots[0].material
            = bpy.data.materials['fracture']
    '''


#UNWRAP
def getsizefrommesh(ob):
    bb = ob.bound_box
    return (
        bb[5][0] - bb[0][0],
        bb[3][1] - bb[0][1],
        bb[1][2] - bb[0][2])


def getIslands(shard):
    sm = shard.data
    vgroups = []
    fgroups = []

    vgi = [-1] * len(sm.vertices)

    gindex = 0
    for i in range(len(vgi)):
        if vgi[i] == -1:
            gproc = [i]
            vgroups.append([i])
            fgroups.append([])

            while len(gproc) > 0:
                # XXX - is popping the first needed? - pop() without args is fastest - campbell
                i = gproc.pop(0)
                for p in sm.polygons:
                    #if i in f.vertices:
                    for v in p.vertices:
                        if v == i:
                            for v1 in p.vertices:
                                if vgi[v1] == -1:
                                    vgi[v1] = gindex
                                    vgroups[gindex].append(v1)
                                    gproc.append(v1)

                            fgroups[gindex].append(p.index)

            gindex += 1

    #print( gindex)

    if gindex == 1:
        shards = [shard]

    else:
        shards = []
        for gi in range(0, gindex):
            bpy.ops.object.select_all(action='DESELECT')
            bpy.context.scene.objects.active = shard
            shard.select = True
            bpy.ops.object.duplicate(linked=False, mode='DUMMY')
            a = bpy.context.scene.objects.active
            sm = a.data
            print (a.name)

            bpy.ops.object.editmode_toggle()
            bpy.ops.mesh.select_all(action='DESELECT')
            bpy.ops.object.editmode_toggle()

            for x in range(len(sm.vertices) - 1, -1, -1):
                if vgi[x] != gi:
                    #print('getIslands: selecting')
                    #print('getIslands: ' + str(x))
                    a.data.vertices[x].select = True

            print(bpy.context.scene.objects.active.name)

            bpy.ops.object.editmode_toggle()
            bpy.ops.mesh.delete()
            bpy.ops.object.editmode_toggle()

            bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY')

            shards.append(a)

        bpy.context.scene.objects.unlink(shard)

    return shards


def boolop(ob, cutter, op):
    sce = bpy.context.scene

    fault = 0
    new_shards = []

    sizex, sizey, sizez = getsizefrommesh(ob)
    gsize = sizex + sizey + sizez

    bpy.ops.object.select_all()
    ob.select = True
    sce.objects.active = ob
    cutter.select = False

    bpy.ops.object.modifier_add(type='BOOLEAN')
    a = sce.objects.active
    a.modifiers['Boolean'].object = cutter
    a.modifiers['Boolean'].operation = op

    nmesh = a.to_mesh(sce, apply_modifiers=True, settings='PREVIEW')

    if len(nmesh.vertices) > 0:
        a.modifiers.remove(a.modifiers['Boolean'])
        bpy.ops.object.duplicate(linked=False, mode='DUMMY')

        new_shard = sce.objects.active
        new_shard.data = nmesh
        #scene.objects.link(new_shard)

        new_shard.location = a.location
        bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY')

        sizex, sizey, sizez = getsizefrommesh(new_shard)
        gsize2 = sizex + sizey + sizez

        if gsize2 > gsize * 1.01:               # Size check
            print (gsize2, gsize, ob.name, cutter.name)
            fault = 1
            #print ('boolop: sizeerror')

         # This checks whether returned shards are non-manifold.
         # Problem is, if org mesh is non-manifold, it will always fail (e.g. with Suzanne).
         # And disabling it does not seem to cause any problem...
#        elif min(mesh_utils.edge_face_count(nmesh)) < 2:    # Manifold check
#            fault = 1

        if not fault:
            new_shards = getIslands(new_shard)

        else:
            sce.objects.unlink(new_shard)

    else:
        fault = 2

    return fault, new_shards


def splitobject(context, ob, crack_type, roughness):
    scene = context.scene

    size = getsizefrommesh(ob)
    shards = []
    scale = max(size) * 1.3

    create_cutter(context, crack_type, scale, roughness)
    cutter = context.active_object
    cutter.location = ob.location

    cutter.location[0] += random.random() * size[0] * 0.1
    cutter.location[1] += random.random() * size[1] * 0.1
    cutter.location[2] += random.random() * size[2] * 0.1
    cutter.rotation_euler = [
        random.random() * 5000.0,
        random.random() * 5000.0,
        random.random() * 5000.0]

    scene.objects.active = ob
    operations = ['INTERSECT', 'DIFFERENCE']

    for op in operations:
        fault, newshards = boolop(ob, cutter, op)

        shards.extend(newshards)
        if fault > 0:
            # Delete all shards in case of fault from previous operation.
            for s in shards:
                scene.objects.unlink(s)

            scene.objects.unlink(cutter)
            #print('splitobject: fault')

            return [ob]

    if shards[0] != ob:
        bpy.context.scene.objects.unlink(ob)

    bpy.context.scene.objects.unlink(cutter)

    return shards


def fracture_basic(context, nshards, crack_type, roughness):
    tobesplit = []
    shards = []

    for ob in context.scene.objects:
        if ob.select:
            tobesplit.append(ob)

    i = 1     # I counts shards, starts with 1 - the original object
    iter = 0  # counts iterations, to prevent eternal loops in case
              # of boolean faults

    maxshards = nshards * len(tobesplit)

    while i < maxshards and len(tobesplit) > 0 and iter < maxshards * 10:
        ob = tobesplit.pop(0)
        newshards = splitobject(context, ob, crack_type, roughness)

        tobesplit.extend(newshards)

        if len(newshards) > 1:
            shards.extend(newshards)
            #shards.remove(ob)

            i += (len(newshards) - 1)

            #print('fracture_basic: ' + str(i))
            #print('fracture_basic: lenobs', len(context.scene.objects))

        iter += 1


def fracture_group(context, group):
    tobesplit = []
    shards = []

    for ob in context.scene.objects:
        if (ob.select
            and (len(ob.users_group) == 0 or ob.users_group[0].name != group)):
            tobesplit.append(ob)

    cutters = bpy.data.groups[group].objects

    # @todo This can be optimized.
    # Avoid booleans on obs where bbox doesn't intersect.
    i = 0
    for ob in tobesplit:
        for cutter in cutters:
            fault, newshards = boolop(ob, cutter, 'INTERSECT')
            shards.extend(newshards)

            if fault == 1:
               # Delete all shards in case of fault from previous operation.
                for s in shards:
                    bpy.context.scene.objects.unlink(s)

                #print('fracture_group: fault')
                #print('fracture_group: ' + str(i))

                return

            i += 1


class FractureSimple(bpy.types.Operator):
    """Split object with boolean operations for simulation, uses an object"""
    bl_idname = "object.fracture_simple"
    bl_label = "Fracture Object"
    bl_options = {'REGISTER', 'UNDO'}

    exe = BoolProperty(name="Execute",
        description="If it shall actually run, for optimal performance",
        default=False)

    hierarchy = BoolProperty(name="Generate hierarchy",
        description="Hierarchy is useful for simulation of objects" \
                    " breaking in motion",
        default=False)

    nshards = IntProperty(name="Number of shards",
        description="Number of shards the object should be split into",
        min=2,
        default=5)

    crack_type = EnumProperty(name='Crack type',
        items=(
            ('FLAT', 'Flat', 'a'),
            ('FLAT_ROUGH', 'Flat rough', 'a'),
            ('SPHERE', 'Spherical', 'a'),
            ('SPHERE_ROUGH', 'Spherical rough', 'a')),
        description='Look of the fracture surface',
        default='FLAT')

    roughness = FloatProperty(name="Roughness",
        description="Roughness of the fracture surface",
        min=0.0,
        max=3.0,
        default=0.5)

    @classmethod
    def poll(clss, context):
        ob = context.active_object
        if context.mode != 'OBJECT' or not ob or ob.type != 'MESH':
            return False
        return True

    def execute(self, context):
        #getIslands(context.object)
        if self.exe:
            fracture_basic(context,
                    self.nshards,
                    self.crack_type,
                    self.roughness)

        return {'FINISHED'}


class FractureGroup(bpy.types.Operator):
    """Split object with boolean operations for simulation, uses a group"""
    bl_idname = "object.fracture_group"
    bl_label = "Fracture Object (Group)"
    bl_options = {'REGISTER', 'UNDO'}

    exe = BoolProperty(name="Execute",
                       description="If it shall actually run, for optimal performance",
                       default=False)

    group = StringProperty(name="Group",
                           description="Specify the group used for fracturing")

#    e = []
#    for i, g in enumerate(bpy.data.groups):
#        e.append((g.name, g.name, ''))
#    group = EnumProperty(name='Group (hit F8 to refresh list)',
#                         items=e,
#                         description='Specify the group used for fracturing')

    @classmethod
    def poll(clss, context):
        ob = context.active_object
        if context.mode != 'OBJECT' or not ob or ob.type != 'MESH':
            return False
        return True

    def execute(self, context):
        #getIslands(context.object)

        if self.exe and self.group:
            fracture_group(context, self.group)

        return {'FINISHED'}

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "exe")
        layout.prop_search(self, "group", bpy.data, "groups")

#####################################################################
# Import Functions

def import_object(obname):
    opath = "//data.blend\\Object\\" + obname
    s = os.sep
    #dpath = bpy.utils.script_paths()[0] + \
    #    '%saddons%sobject_fracture%sdata.blend\\Object\\' % (s, s, s)
    dpath=''
    fpath=''
    for p in bpy.utils.script_paths():

        testfname= p + '%saddons%sobject_fracture%sdata.blend' % (s,s,s)
        print(testfname)
        if os.path.isfile(testfname):
            fname=testfname
            dpath = p + \
            '%saddons%sobject_fracture%sdata.blend\\Object\\' % (s, s, s)
            break
    # DEBUG
    #print('import_object: ' + opath)

    bpy.ops.wm.append(
            filepath=opath,
            filename=obname,
            directory=dpath,
            filemode=1,
            link=False,
            autoselect=True,
            active_layer=True,
            instance_groups=True)

    for ob in bpy.context.selected_objects:
        ob.location = bpy.context.scene.cursor_location


class ImportFractureRecorder(bpy.types.Operator):
    """Imports a rigidbody recorder"""
    bl_idname = "object.import_fracture_recorder"
    bl_label = "Add Rigidbody Recorder (Fracture)"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        import_object("RECORDER")

        return {'FINISHED'}


class ImportFractureBomb(bpy.types.Operator):
    """Import a bomb"""
    bl_idname = "object.import_fracture_bomb"
    bl_label = "Add Bomb (Fracture)"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        import_object("BOMB")

        return {'FINISHED'}


class ImportFractureProjectile(bpy.types.Operator, ):
    """Imports a projectile"""
    bl_idname = "object.import_fracture_projectile"
    bl_label = "Add Projectile (Fracture)"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        import_object("PROJECTILE")

        return {'FINISHED'}
