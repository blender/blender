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

# Script copyright (C) Campbell Barton

"""
This script is an exporter to the FBX file format.

http://wiki.blender.org/index.php/Scripts/Manual/Export/autodesk_fbx
"""

import os
import time
import math # math.pi
import shutil # for file copying

import bpy
from mathutils import Vector, Euler, Matrix

# XXX not used anymore, images are copied one at a time
def copy_images(dest_dir, textures):
    import shutil
    
    if not dest_dir.endswith(os.sep):
        dest_dir += os.sep

    image_paths = set()
    for tex in textures:
        image_paths.add(bpy.path.abspath(tex.filepath))

    # Now copy images
    copyCount = 0
    for image_path in image_paths:
        if Blender.sys.exists(image_path):
            # Make a name for the target path.
            dest_image_path = dest_dir + image_path.split('\\')[-1].split('/')[-1]
            if not Blender.sys.exists(dest_image_path): # Image isnt already there
                print("\tCopying %r > %r" % (image_path, dest_image_path))
                try:
                    shutil.copy(image_path, dest_image_path)
                    copyCount+=1
                except:
                    print("\t\tWarning, file failed to copy, skipping.")

    print('\tCopied %d images' % copyCount)

# I guess FBX uses degrees instead of radians (Arystan).
# Call this function just before writing to FBX.
def eulerRadToDeg(eul):
    ret = Euler()

    ret.x = 180 / math.pi * eul[0]
    ret.y = 180 / math.pi * eul[1]
    ret.z = 180 / math.pi * eul[2]

    return ret


# def strip_path(p):
# 	return p.split('\\')[-1].split('/')[-1]

# Used to add the scene name into the filepath without using odd chars
sane_name_mapping_ob = {}
sane_name_mapping_mat = {}
sane_name_mapping_tex = {}
sane_name_mapping_take = {}
sane_name_mapping_group = {}

# Make sure reserved names are not used
sane_name_mapping_ob['Scene'] = 'Scene_'
sane_name_mapping_ob['blend_root'] = 'blend_root_'

def increment_string(t):
    name = t
    num = ''
    while name and name[-1].isdigit():
        num = name[-1] + num
        name = name[:-1]
    if num:	return '%s%d' % (name, int(num)+1)
    else:	return name + '_0'



# todo - Disallow the name 'Scene' and 'blend_root' - it will bugger things up.
def sane_name(data, dct):
    #if not data: return None

    if type(data)==tuple: # materials are paired up with images
        data, other = data
        use_other = True
    else:
        other = None
        use_other = False

    if data:	name = data.name
    else:		name = None
    orig_name = name

    if other:
        orig_name_other = other.name
        name = '%s #%s' % (name, orig_name_other)
    else:
        orig_name_other = None

    # dont cache, only ever call once for each data type now,
    # so as to avoid namespace collision between types - like with objects <-> bones
    #try:		return dct[name]
    #except:		pass

    if not name:
        name = 'unnamed' # blank string, ASKING FOR TROUBLE!
    else:

        name = bpy.path.clean_name(name) # use our own

    while name in iter(dct.values()):	name = increment_string(name)

    if use_other: # even if other is None - orig_name_other will be a string or None
        dct[orig_name, orig_name_other] = name
    else:
        dct[orig_name] = name

    return name

def sane_obname(data):		return sane_name(data, sane_name_mapping_ob)
def sane_matname(data):		return sane_name(data, sane_name_mapping_mat)
def sane_texname(data):		return sane_name(data, sane_name_mapping_tex)
def sane_takename(data):	return sane_name(data, sane_name_mapping_take)
def sane_groupname(data):	return sane_name(data, sane_name_mapping_group)

# def derived_paths(fname_orig, basepath, FORCE_CWD=False):
# 	'''
# 	fname_orig - blender path, can be relative
# 	basepath - fname_rel will be relative to this
# 	FORCE_CWD - dont use the basepath, just add a ./ to the filepath.
# 		use when we know the file will be in the basepath.
# 	'''
# 	fname = bpy.path.abspath(fname_orig)
# # 	fname = Blender.sys.expandpath(fname_orig)
# 	fname_strip = os.path.basename(fname)
# # 	fname_strip = strip_path(fname)
# 	if FORCE_CWD:
# 		fname_rel = '.' + os.sep + fname_strip
# 	else:
# 		fname_rel = bpy.path.relpath(fname, basepath)
# # 		fname_rel = Blender.sys.relpath(fname, basepath)
# 	if fname_rel.startswith('//'): fname_rel = '.' + os.sep + fname_rel[2:]
# 	return fname, fname_strip, fname_rel


def mat4x4str(mat):
    return '%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f,%.15f' % tuple([ f for v in mat for f in v ])

# XXX not used
# duplicated in OBJ exporter
def getVertsFromGroup(me, group_index):
    ret = []

    for i, v in enumerate(me.vertices):
        for g in v.groups:
            if g.group == group_index:
                ret.append((i, g.weight))

        return ret

# ob must be OB_MESH
def BPyMesh_meshWeight2List(ob, me):
    ''' Takes a mesh and return its group names and a list of lists, one list per vertex.
    aligning the each vert list with the group names, each list contains float value for the weight.
    These 2 lists can be modified and then used with list2MeshWeight to apply the changes.
    '''

    # Clear the vert group.
    groupNames= [g.name for g in ob.vertex_groups]
    len_groupNames= len(groupNames)

    if not len_groupNames:
        # no verts? return a vert aligned empty list
        return [[] for i in range(len(me.vertices))], []
    else:
        vWeightList= [[0.0]*len_groupNames for i in range(len(me.vertices))]

    for i, v in enumerate(me.vertices):
        for g in v.groups:
            vWeightList[i][g.group] = g.weight

    return groupNames, vWeightList

def meshNormalizedWeights(ob, me):
    try: # account for old bad BPyMesh
        groupNames, vWeightList = BPyMesh_meshWeight2List(ob, me)
# 		groupNames, vWeightList = BPyMesh.meshWeight2List(me)
    except:
        return [],[]

    if not groupNames:
        return [],[]

    for i, vWeights in enumerate(vWeightList):
        tot = 0.0
        for w in vWeights:
            tot+=w

        if tot:
            for j, w in enumerate(vWeights):
                vWeights[j] = w/tot

    return groupNames, vWeightList

header_comment = \
'''; FBX 6.1.0 project file
; Created by Blender FBX Exporter
; for support mail: ideasman42@gmail.com
; ----------------------------------------------------

'''

# This func can be called with just the filepath
def save(operator, context, filepath="",
        GLOBAL_MATRIX =				None,
        EXP_OBS_SELECTED =			True,
        EXP_MESH =					True,
        EXP_MESH_APPLY_MOD =		True,
        EXP_ARMATURE =				True,
        EXP_LAMP =					True,
        EXP_CAMERA =				True,
        EXP_EMPTY =					True,
        EXP_IMAGE_COPY =			False,
        ANIM_ENABLE =				True,
        ANIM_OPTIMIZE =				True,
        ANIM_OPTIMIZE_PRECISSION =	6,
        ANIM_ACTION_ALL =			False,
        BATCH_ENABLE =				False,
        BATCH_GROUP =				True,
        BATCH_FILE_PREFIX =			'',
        BATCH_OWN_DIR =				False
    ):

    #XXX, missing arg 
    batch_objects = None

    # testing
    mtx_x90		= Matrix.Rotation( math.pi/2.0, 3, 'X') # used
    mtx4_z90	= Matrix.Rotation( math.pi/2.0, 4, 'Z')

    if GLOBAL_MATRIX is None:
        GLOBAL_MATRIX = Matrix()

    if bpy.ops.object.mode_set.poll():
        bpy.ops.object.mode_set(mode='OBJECT')

    # ----------------- Batch support!
    if BATCH_ENABLE:
        fbxpath = filepath

        # get the path component of filepath
        tmp_exists = bpy.utils.exists(fbxpath)
# 		tmp_exists = Blender.sys.exists(fbxpath)

        if tmp_exists != 2: # a file, we want a path
            fbxpath = os.path.dirname(fbxpath)
# 			while fbxpath and fbxpath[-1] not in ('/', '\\'):
# 				fbxpath = fbxpath[:-1]
            if not fbxpath:
# 			if not filepath:
                # XXX
                print('Error%t|Directory does not exist!')
# 				Draw.PupMenu('Error%t|Directory does not exist!')
                return

            tmp_exists = bpy.utils.exists(fbxpath)
# 			tmp_exists = Blender.sys.exists(fbxpath)

        if tmp_exists != 2:
            # XXX
            print('Error%t|Directory does not exist!')
# 			Draw.PupMenu('Error%t|Directory does not exist!')
            return

        if not fbxpath.endswith(os.sep):
            fbxpath += os.sep
        del tmp_exists


        if BATCH_GROUP:
            data_seq = bpy.data.groups
        else:
            data_seq = bpy.data.scenes

        # call this function within a loop with BATCH_ENABLE == False
        orig_sce = context.scene
# 		orig_sce = bpy.data.scenes.active

        new_fbxpath = fbxpath # own dir option modifies, we need to keep an original
        for data in data_seq: # scene or group
            newname = BATCH_FILE_PREFIX + bpy.path.clean_name(data.name)


            if BATCH_OWN_DIR:
                new_fbxpath = fbxpath + newname + os.sep
                # path may already exist
                # TODO - might exist but be a file. unlikely but should probably account for it.

                if bpy.utils.exists(new_fbxpath) == 0:
# 				if Blender.sys.exists(new_fbxpath) == 0:
                    os.mkdir(new_fbxpath)


            filepath = new_fbxpath + newname + '.fbx'

            print('\nBatch exporting %s as...\n\t%r' % (data, filepath))

            # XXX don't know what to do with this, probably do the same? (Arystan)
            if BATCH_GROUP: #group
                # group, so objects update properly, add a dummy scene.
                scene = bpy.data.scenes.new()
                scene.Layers = (1<<20) -1
                bpy.data.scenes.active = scene
                for ob_base in data.objects:
                    scene.objects.link(ob_base)

                scene.update(1)

                # TODO - BUMMER! Armatures not in the group wont animate the mesh

            else:# scene


                data_seq.active = data


            # Call self with modified args
            # Dont pass batch options since we already usedt them
            write(filepath, data.objects,
                context,
                False,
                EXP_MESH,
                EXP_MESH_APPLY_MOD,
                EXP_ARMATURE,
                EXP_LAMP,
                EXP_CAMERA,
                EXP_EMPTY,
                EXP_IMAGE_COPY,
                GLOBAL_MATRIX,
                ANIM_ENABLE,
                ANIM_OPTIMIZE,
                ANIM_OPTIMIZE_PRECISSION,
                ANIM_ACTION_ALL
            )

            if BATCH_GROUP:
                # remove temp group scene
                bpy.data.scenes.unlink(scene)

        bpy.data.scenes.active = orig_sce

        return # so the script wont run after we have batch exported.

    # end batch support

    # Use this for working out paths relative to the export location
    basepath = os.path.dirname(filepath) or '.'
    basepath += os.sep
# 	basepath = Blender.sys.dirname(filepath)

    # ----------------------------------------------
    # storage classes
    class my_bone_class(object):
        __slots__ =("blenName",
                    "blenBone",
                    "blenMeshes",
                    "restMatrix",
                    "parent",
                    "blenName",
                    "fbxName",
                    "fbxArm",
                    "__pose_bone",
                    "__anim_poselist")

        def __init__(self, blenBone, fbxArm):

            # This is so 2 armatures dont have naming conflicts since FBX bones use object namespace
            self.fbxName = sane_obname(blenBone)

            self.blenName =			blenBone.name
            self.blenBone =			blenBone
            self.blenMeshes =		{}					# fbxMeshObName : mesh
            self.fbxArm =			fbxArm
            self.restMatrix =		blenBone.matrix_local
# 			self.restMatrix =		blenBone.matrix['ARMATURESPACE']

            # not used yet
            # self.restMatrixInv =	self.restMatrix.copy().invert()
            # self.restMatrixLocal =	None # set later, need parent matrix

            self.parent =			None

            # not public
            pose = fbxArm.blenObject.pose
# 			pose = fbxArm.blenObject.getPose()
            self.__pose_bone =		pose.bones[self.blenName]

            # store a list if matricies here, (poseMatrix, head, tail)
            # {frame:posematrix, frame:posematrix, ...}
            self.__anim_poselist = {}

        '''
        def calcRestMatrixLocal(self):
            if self.parent:
                self.restMatrixLocal = self.restMatrix * self.parent.restMatrix.copy().invert()
            else:
                self.restMatrixLocal = self.restMatrix.copy()
        '''
        def setPoseFrame(self, f):
            # cache pose info here, frame must be set beforehand

            # Didnt end up needing head or tail, if we do - here it is.
            '''
            self.__anim_poselist[f] = (\
                self.__pose_bone.poseMatrix.copy(),\
                self.__pose_bone.head.copy(),\
                self.__pose_bone.tail.copy() )
            '''

            self.__anim_poselist[f] = self.__pose_bone.matrix.copy()
# 			self.__anim_poselist[f] = self.__pose_bone.poseMatrix.copy()

        # get pose from frame.
        def getPoseMatrix(self, f):# ----------------------------------------------
            return self.__anim_poselist[f]
        '''
        def getPoseHead(self, f):
            #return self.__pose_bone.head.copy()
            return self.__anim_poselist[f][1].copy()
        def getPoseTail(self, f):
            #return self.__pose_bone.tail.copy()
            return self.__anim_poselist[f][2].copy()
        '''
        # end

        def getAnimParRelMatrix(self, frame):
            #arm_mat = self.fbxArm.matrixWorld
            #arm_mat = self.fbxArm.parRelMatrix()
            if not self.parent:
                #return mtx4_z90 * (self.getPoseMatrix(frame) * arm_mat) # dont apply arm matrix anymore
                return self.getPoseMatrix(frame) * mtx4_z90
            else:
                #return (mtx4_z90 * ((self.getPoseMatrix(frame) * arm_mat)))  *  (mtx4_z90 * (self.parent.getPoseMatrix(frame) * arm_mat)).invert()
                return (self.parent.getPoseMatrix(frame) * mtx4_z90).invert() * ((self.getPoseMatrix(frame)) * mtx4_z90)

        # we need thes because cameras and lights modified rotations
        def getAnimParRelMatrixRot(self, frame):
            return self.getAnimParRelMatrix(frame)

        def flushAnimData(self):
            self.__anim_poselist.clear()


    class my_object_generic(object):
        __slots__ =("fbxName",
                    "blenObject",
                    "blenData",
                    "origData",
                    "blenTextures",
                    "blenMaterials",
                    "blenMaterialList",
                    "blenAction",
                    "blenActionList",
                    "fbxGroupNames",
                    "fbxParent",
                    "fbxBoneParent",
                    "fbxBones",
                    "fbxArm",
                    "matrixWorld",
                    "__anim_poselist",
                    )

        # Other settings can be applied for each type - mesh, armature etc.
        def __init__(self, ob, matrixWorld = None):
            self.fbxName = sane_obname(ob)
            self.blenObject = ob
            self.fbxGroupNames = []
            self.fbxParent = None # set later on IF the parent is in the selection.
            if matrixWorld:		self.matrixWorld = GLOBAL_MATRIX * matrixWorld
            else:				self.matrixWorld = GLOBAL_MATRIX * ob.matrix_world
# 			else:				self.matrixWorld = ob.matrixWorld * GLOBAL_MATRIX
            self.__anim_poselist = {} # we should only access this

        def parRelMatrix(self):
            if self.fbxParent:
                return self.fbxParent.matrixWorld.copy().invert() * self.matrixWorld
            else:
                return self.matrixWorld

        def setPoseFrame(self, f):
            self.__anim_poselist[f] =  self.blenObject.matrix_world.copy()

        def getAnimParRelMatrix(self, frame):
            if self.fbxParent:
                #return (self.__anim_poselist[frame] * self.fbxParent.__anim_poselist[frame].copy().invert() ) * GLOBAL_MATRIX
                return (GLOBAL_MATRIX * self.fbxParent.__anim_poselist[frame]).invert() * (GLOBAL_MATRIX * self.__anim_poselist[frame])
            else:
                return GLOBAL_MATRIX * self.__anim_poselist[frame]

        def getAnimParRelMatrixRot(self, frame):
            obj_type = self.blenObject.type
            if self.fbxParent:
                matrix_rot = ((GLOBAL_MATRIX * self.fbxParent.__anim_poselist[frame]).invert() * (GLOBAL_MATRIX * self.__anim_poselist[frame])).rotation_part()
            else:
                matrix_rot = (GLOBAL_MATRIX * self.__anim_poselist[frame]).rotation_part()

            # Lamps need to be rotated
            if obj_type =='LAMP':
                matrix_rot = matrix_rot * mtx_x90
            elif obj_type =='CAMERA':
                y = matrix_rot * Vector((0.0, 1.0, 0.0))
                matrix_rot = Matrix.Rotation(math.pi/2, 3, y) * matrix_rot

            return matrix_rot

    # ----------------------------------------------





    print('\nFBX export starting... %r' % filepath)
    start_time = time.clock()
    try:
        file = open(filepath, 'w')
    except:
        return False

    scene = context.scene
    world = scene.world


    # ---------------------------- Write the header first
    file.write(header_comment)
    if time:
        curtime = time.localtime()[0:6]
    else:
        curtime = (0,0,0,0,0,0)
    #
    file.write(\
'''FBXHeaderExtension:  {
    FBXHeaderVersion: 1003
    FBXVersion: 6100
    CreationTimeStamp:  {
        Version: 1000
        Year: %.4i
        Month: %.2i
        Day: %.2i
        Hour: %.2i
        Minute: %.2i
        Second: %.2i
        Millisecond: 0
    }
    Creator: "FBX SDK/FBX Plugins build 20070228"
    OtherFlags:  {
        FlagPLE: 0
    }
}''' % (curtime))

    file.write('\nCreationTime: "%.4i-%.2i-%.2i %.2i:%.2i:%.2i:000"' % curtime)
    file.write('\nCreator: "Blender version %s"' % bpy.app.version_string)


    pose_items = [] # list of (fbxName, matrix) to write pose data for, easier to collect allong the way

    # --------------- funcs for exporting
    def object_tx(ob, loc, matrix, matrix_mod = None):
        '''
        Matrix mod is so armature objects can modify their bone matricies
        '''
        if isinstance(ob, bpy.types.Bone):
# 		if isinstance(ob, Blender.Types.BoneType):

            # we know we have a matrix
            # matrix = mtx4_z90 * (ob.matrix['ARMATURESPACE'] * matrix_mod)
            matrix = ob.matrix_local * mtx4_z90 # dont apply armature matrix anymore
# 			matrix = mtx4_z90 * ob.matrix['ARMATURESPACE'] # dont apply armature matrix anymore

            parent = ob.parent
            if parent:
                #par_matrix = mtx4_z90 * (parent.matrix['ARMATURESPACE'] * matrix_mod)
                par_matrix = parent.matrix_local * mtx4_z90 # dont apply armature matrix anymore
# 				par_matrix = mtx4_z90 * parent.matrix['ARMATURESPACE'] # dont apply armature matrix anymore
                matrix = par_matrix.copy().invert() * matrix

            matrix_rot =	matrix.rotation_part()

            loc =			tuple(matrix.translation_part())
            scale =			tuple(matrix.scale_part())
            rot =			tuple(matrix_rot.to_euler())

        else:
            # This is bad because we need the parent relative matrix from the fbx parent (if we have one), dont use anymore
            #if ob and not matrix: matrix = ob.matrix_world * GLOBAL_MATRIX
            if ob and not matrix: raise Exception("error: this should never happen!")

            matrix_rot = matrix
            #if matrix:
            #	matrix = matrix_scale * matrix

            if matrix:
                loc = tuple(matrix.translation_part())
                scale = tuple(matrix.scale_part())

                matrix_rot = matrix.rotation_part()
                # Lamps need to be rotated
                if ob and ob.type =='LAMP':
                    matrix_rot = matrix_rot * mtx_x90
                    rot = tuple(matrix_rot.to_euler())
                elif ob and ob.type =='CAMERA':
                    y = matrix_rot * Vector((0.0, 1.0, 0.0))
                    matrix_rot = Matrix.Rotation(math.pi/2, 3, y) * matrix_rot
                    rot = tuple(matrix_rot.to_euler())
                else:
                    rot = tuple(matrix_rot.to_euler())
            else:
                if not loc:
                    loc = 0,0,0
                scale = 1,1,1
                rot = 0,0,0

        return loc, rot, scale, matrix, matrix_rot

    def write_object_tx(ob, loc, matrix, matrix_mod= None):
        '''
        We have loc to set the location if non blender objects that have a location

        matrix_mod is only used for bones at the moment
        '''
        loc, rot, scale, matrix, matrix_rot = object_tx(ob, loc, matrix, matrix_mod)

        file.write('\n\t\t\tProperty: "Lcl Translation", "Lcl Translation", "A+",%.15f,%.15f,%.15f' % loc)
        file.write('\n\t\t\tProperty: "Lcl Rotation", "Lcl Rotation", "A+",%.15f,%.15f,%.15f' % tuple(eulerRadToDeg(rot)))
# 		file.write('\n\t\t\tProperty: "Lcl Rotation", "Lcl Rotation", "A+",%.15f,%.15f,%.15f' % rot)
        file.write('\n\t\t\tProperty: "Lcl Scaling", "Lcl Scaling", "A+",%.15f,%.15f,%.15f' % scale)
        return loc, rot, scale, matrix, matrix_rot

    def write_object_props(ob=None, loc=None, matrix=None, matrix_mod=None):
        # if the type is 0 its an empty otherwise its a mesh
        # only difference at the moment is one has a color
        file.write('''
        Properties60:  {
            Property: "QuaternionInterpolate", "bool", "",0
            Property: "Visibility", "Visibility", "A+",1''')

        loc, rot, scale, matrix, matrix_rot = write_object_tx(ob, loc, matrix, matrix_mod)

        # Rotation order, note, for FBX files Iv loaded normal order is 1
        # setting to zero.
        # eEULER_XYZ = 0
        # eEULER_XZY
        # eEULER_YZX
        # eEULER_YXZ
        # eEULER_ZXY
        # eEULER_ZYX

        file.write('''
            Property: "RotationOffset", "Vector3D", "",0,0,0
            Property: "RotationPivot", "Vector3D", "",0,0,0
            Property: "ScalingOffset", "Vector3D", "",0,0,0
            Property: "ScalingPivot", "Vector3D", "",0,0,0
            Property: "TranslationActive", "bool", "",0
            Property: "TranslationMin", "Vector3D", "",0,0,0
            Property: "TranslationMax", "Vector3D", "",0,0,0
            Property: "TranslationMinX", "bool", "",0
            Property: "TranslationMinY", "bool", "",0
            Property: "TranslationMinZ", "bool", "",0
            Property: "TranslationMaxX", "bool", "",0
            Property: "TranslationMaxY", "bool", "",0
            Property: "TranslationMaxZ", "bool", "",0
            Property: "RotationOrder", "enum", "",0
            Property: "RotationSpaceForLimitOnly", "bool", "",0
            Property: "AxisLen", "double", "",10
            Property: "PreRotation", "Vector3D", "",0,0,0
            Property: "PostRotation", "Vector3D", "",0,0,0
            Property: "RotationActive", "bool", "",0
            Property: "RotationMin", "Vector3D", "",0,0,0
            Property: "RotationMax", "Vector3D", "",0,0,0
            Property: "RotationMinX", "bool", "",0
            Property: "RotationMinY", "bool", "",0
            Property: "RotationMinZ", "bool", "",0
            Property: "RotationMaxX", "bool", "",0
            Property: "RotationMaxY", "bool", "",0
            Property: "RotationMaxZ", "bool", "",0
            Property: "RotationStiffnessX", "double", "",0
            Property: "RotationStiffnessY", "double", "",0
            Property: "RotationStiffnessZ", "double", "",0
            Property: "MinDampRangeX", "double", "",0
            Property: "MinDampRangeY", "double", "",0
            Property: "MinDampRangeZ", "double", "",0
            Property: "MaxDampRangeX", "double", "",0
            Property: "MaxDampRangeY", "double", "",0
            Property: "MaxDampRangeZ", "double", "",0
            Property: "MinDampStrengthX", "double", "",0
            Property: "MinDampStrengthY", "double", "",0
            Property: "MinDampStrengthZ", "double", "",0
            Property: "MaxDampStrengthX", "double", "",0
            Property: "MaxDampStrengthY", "double", "",0
            Property: "MaxDampStrengthZ", "double", "",0
            Property: "PreferedAngleX", "double", "",0
            Property: "PreferedAngleY", "double", "",0
            Property: "PreferedAngleZ", "double", "",0
            Property: "InheritType", "enum", "",0
            Property: "ScalingActive", "bool", "",0
            Property: "ScalingMin", "Vector3D", "",1,1,1
            Property: "ScalingMax", "Vector3D", "",1,1,1
            Property: "ScalingMinX", "bool", "",0
            Property: "ScalingMinY", "bool", "",0
            Property: "ScalingMinZ", "bool", "",0
            Property: "ScalingMaxX", "bool", "",0
            Property: "ScalingMaxY", "bool", "",0
            Property: "ScalingMaxZ", "bool", "",0
            Property: "GeometricTranslation", "Vector3D", "",0,0,0
            Property: "GeometricRotation", "Vector3D", "",0,0,0
            Property: "GeometricScaling", "Vector3D", "",1,1,1
            Property: "LookAtProperty", "object", ""
            Property: "UpVectorProperty", "object", ""
            Property: "Show", "bool", "",1
            Property: "NegativePercentShapeSupport", "bool", "",1
            Property: "DefaultAttributeIndex", "int", "",0''')
        if ob and not isinstance(ob, bpy.types.Bone):
            # Only mesh objects have color
            file.write('\n\t\t\tProperty: "Color", "Color", "A",0.8,0.8,0.8')
            file.write('\n\t\t\tProperty: "Size", "double", "",100')
            file.write('\n\t\t\tProperty: "Look", "enum", "",1')

        return loc, rot, scale, matrix, matrix_rot


    # -------------------------------------------- Armatures
    #def write_bone(bone, name, matrix_mod):
    def write_bone(my_bone):
        file.write('\n\tModel: "Model::%s", "Limb" {' % my_bone.fbxName)
        file.write('\n\t\tVersion: 232')

        #poseMatrix = write_object_props(my_bone.blenBone, None, None, my_bone.fbxArm.parRelMatrix())[3]
        poseMatrix = write_object_props(my_bone.blenBone)[3] # dont apply bone matricies anymore
        pose_items.append( (my_bone.fbxName, poseMatrix) )


        # file.write('\n\t\t\tProperty: "Size", "double", "",%.6f' % ((my_bone.blenData.head['ARMATURESPACE'] - my_bone.blenData.tail['ARMATURESPACE']) * my_bone.fbxArm.parRelMatrix()).length)
        file.write('\n\t\t\tProperty: "Size", "double", "",1')

        #((my_bone.blenData.head['ARMATURESPACE'] * my_bone.fbxArm.matrixWorld) - (my_bone.blenData.tail['ARMATURESPACE'] * my_bone.fbxArm.parRelMatrix())).length)

        """
        file.write('\n\t\t\tProperty: "LimbLength", "double", "",%.6f' %\
            ((my_bone.blenBone.head['ARMATURESPACE'] - my_bone.blenBone.tail['ARMATURESPACE']) * my_bone.fbxArm.parRelMatrix()).length)
        """

        file.write('\n\t\t\tProperty: "LimbLength", "double", "",%.6f' %
                   (my_bone.blenBone.head_local - my_bone.blenBone.tail_local).length)
# 			(my_bone.blenBone.head['ARMATURESPACE'] - my_bone.blenBone.tail['ARMATURESPACE']).length)

        #file.write('\n\t\t\tProperty: "LimbLength", "double", "",1')
        file.write('\n\t\t\tProperty: "Color", "ColorRGB", "",0.8,0.8,0.8')
        file.write('\n\t\t\tProperty: "Color", "Color", "A",0.8,0.8,0.8')
        file.write('\n\t\t}')
        file.write('\n\t\tMultiLayer: 0')
        file.write('\n\t\tMultiTake: 1')
        file.write('\n\t\tShading: Y')
        file.write('\n\t\tCulling: "CullingOff"')
        file.write('\n\t\tTypeFlags: "Skeleton"')
        file.write('\n\t}')

    def write_camera_switch():
        file.write('''
    Model: "Model::Camera Switcher", "CameraSwitcher" {
        Version: 232''')

        write_object_props()
        file.write('''
            Property: "Color", "Color", "A",0.8,0.8,0.8
            Property: "Camera Index", "Integer", "A+",100
        }
        MultiLayer: 0
        MultiTake: 1
        Hidden: "True"
        Shading: W
        Culling: "CullingOff"
        Version: 101
        Name: "Model::Camera Switcher"
        CameraId: 0
        CameraName: 100
        CameraIndexName:
    }''')

    def write_camera_dummy(name, loc, near, far, proj_type, up):
        file.write('\n\tModel: "Model::%s", "Camera" {' % name )
        file.write('\n\t\tVersion: 232')
        write_object_props(None, loc)

        file.write('\n\t\t\tProperty: "Color", "Color", "A",0.8,0.8,0.8')
        file.write('\n\t\t\tProperty: "Roll", "Roll", "A+",0')
        file.write('\n\t\t\tProperty: "FieldOfView", "FieldOfView", "A+",40')
        file.write('\n\t\t\tProperty: "FieldOfViewX", "FieldOfView", "A+",1')
        file.write('\n\t\t\tProperty: "FieldOfViewY", "FieldOfView", "A+",1')
        file.write('\n\t\t\tProperty: "OpticalCenterX", "Real", "A+",0')
        file.write('\n\t\t\tProperty: "OpticalCenterY", "Real", "A+",0')
        file.write('\n\t\t\tProperty: "BackgroundColor", "Color", "A+",0.63,0.63,0.63')
        file.write('\n\t\t\tProperty: "TurnTable", "Real", "A+",0')
        file.write('\n\t\t\tProperty: "DisplayTurnTableIcon", "bool", "",1')
        file.write('\n\t\t\tProperty: "Motion Blur Intensity", "Real", "A+",1')
        file.write('\n\t\t\tProperty: "UseMotionBlur", "bool", "",0')
        file.write('\n\t\t\tProperty: "UseRealTimeMotionBlur", "bool", "",1')
        file.write('\n\t\t\tProperty: "ResolutionMode", "enum", "",0')
        file.write('\n\t\t\tProperty: "ApertureMode", "enum", "",2')
        file.write('\n\t\t\tProperty: "GateFit", "enum", "",0')
        file.write('\n\t\t\tProperty: "FocalLength", "Real", "A+",21.3544940948486')
        file.write('\n\t\t\tProperty: "CameraFormat", "enum", "",0')
        file.write('\n\t\t\tProperty: "AspectW", "double", "",320')
        file.write('\n\t\t\tProperty: "AspectH", "double", "",200')
        file.write('\n\t\t\tProperty: "PixelAspectRatio", "double", "",1')
        file.write('\n\t\t\tProperty: "UseFrameColor", "bool", "",0')
        file.write('\n\t\t\tProperty: "FrameColor", "ColorRGB", "",0.3,0.3,0.3')
        file.write('\n\t\t\tProperty: "ShowName", "bool", "",1')
        file.write('\n\t\t\tProperty: "ShowGrid", "bool", "",1')
        file.write('\n\t\t\tProperty: "ShowOpticalCenter", "bool", "",0')
        file.write('\n\t\t\tProperty: "ShowAzimut", "bool", "",1')
        file.write('\n\t\t\tProperty: "ShowTimeCode", "bool", "",0')
        file.write('\n\t\t\tProperty: "NearPlane", "double", "",%.6f' % near)
        file.write('\n\t\t\tProperty: "FarPlane", "double", "",%.6f' % far)
        file.write('\n\t\t\tProperty: "FilmWidth", "double", "",0.816')
        file.write('\n\t\t\tProperty: "FilmHeight", "double", "",0.612')
        file.write('\n\t\t\tProperty: "FilmAspectRatio", "double", "",1.33333333333333')
        file.write('\n\t\t\tProperty: "FilmSqueezeRatio", "double", "",1')
        file.write('\n\t\t\tProperty: "FilmFormatIndex", "enum", "",4')
        file.write('\n\t\t\tProperty: "ViewFrustum", "bool", "",1')
        file.write('\n\t\t\tProperty: "ViewFrustumNearFarPlane", "bool", "",0')
        file.write('\n\t\t\tProperty: "ViewFrustumBackPlaneMode", "enum", "",2')
        file.write('\n\t\t\tProperty: "BackPlaneDistance", "double", "",100')
        file.write('\n\t\t\tProperty: "BackPlaneDistanceMode", "enum", "",0')
        file.write('\n\t\t\tProperty: "ViewCameraToLookAt", "bool", "",1')
        file.write('\n\t\t\tProperty: "LockMode", "bool", "",0')
        file.write('\n\t\t\tProperty: "LockInterestNavigation", "bool", "",0')
        file.write('\n\t\t\tProperty: "FitImage", "bool", "",0')
        file.write('\n\t\t\tProperty: "Crop", "bool", "",0')
        file.write('\n\t\t\tProperty: "Center", "bool", "",1')
        file.write('\n\t\t\tProperty: "KeepRatio", "bool", "",1')
        file.write('\n\t\t\tProperty: "BackgroundMode", "enum", "",0')
        file.write('\n\t\t\tProperty: "BackgroundAlphaTreshold", "double", "",0.5')
        file.write('\n\t\t\tProperty: "ForegroundTransparent", "bool", "",1')
        file.write('\n\t\t\tProperty: "DisplaySafeArea", "bool", "",0')
        file.write('\n\t\t\tProperty: "SafeAreaDisplayStyle", "enum", "",1')
        file.write('\n\t\t\tProperty: "SafeAreaAspectRatio", "double", "",1.33333333333333')
        file.write('\n\t\t\tProperty: "Use2DMagnifierZoom", "bool", "",0')
        file.write('\n\t\t\tProperty: "2D Magnifier Zoom", "Real", "A+",100')
        file.write('\n\t\t\tProperty: "2D Magnifier X", "Real", "A+",50')
        file.write('\n\t\t\tProperty: "2D Magnifier Y", "Real", "A+",50')
        file.write('\n\t\t\tProperty: "CameraProjectionType", "enum", "",%i' % proj_type)
        file.write('\n\t\t\tProperty: "UseRealTimeDOFAndAA", "bool", "",0')
        file.write('\n\t\t\tProperty: "UseDepthOfField", "bool", "",0')
        file.write('\n\t\t\tProperty: "FocusSource", "enum", "",0')
        file.write('\n\t\t\tProperty: "FocusAngle", "double", "",3.5')
        file.write('\n\t\t\tProperty: "FocusDistance", "double", "",200')
        file.write('\n\t\t\tProperty: "UseAntialiasing", "bool", "",0')
        file.write('\n\t\t\tProperty: "AntialiasingIntensity", "double", "",0.77777')
        file.write('\n\t\t\tProperty: "UseAccumulationBuffer", "bool", "",0')
        file.write('\n\t\t\tProperty: "FrameSamplingCount", "int", "",7')
        file.write('\n\t\t}')
        file.write('\n\t\tMultiLayer: 0')
        file.write('\n\t\tMultiTake: 0')
        file.write('\n\t\tHidden: "True"')
        file.write('\n\t\tShading: Y')
        file.write('\n\t\tCulling: "CullingOff"')
        file.write('\n\t\tTypeFlags: "Camera"')
        file.write('\n\t\tGeometryVersion: 124')
        file.write('\n\t\tPosition: %.6f,%.6f,%.6f' % loc)
        file.write('\n\t\tUp: %i,%i,%i' % up)
        file.write('\n\t\tLookAt: 0,0,0')
        file.write('\n\t\tShowInfoOnMoving: 1')
        file.write('\n\t\tShowAudio: 0')
        file.write('\n\t\tAudioColor: 0,1,0')
        file.write('\n\t\tCameraOrthoZoom: 1')
        file.write('\n\t}')

    def write_camera_default():
        # This sucks but to match FBX converter its easier to
        # write the cameras though they are not needed.
        write_camera_dummy('Producer Perspective',	(0,71.3,287.5), 10, 4000, 0, (0,1,0))
        write_camera_dummy('Producer Top',			(0,4000,0), 1, 30000, 1, (0,0,-1))
        write_camera_dummy('Producer Bottom',			(0,-4000,0), 1, 30000, 1, (0,0,-1))
        write_camera_dummy('Producer Front',			(0,0,4000), 1, 30000, 1, (0,1,0))
        write_camera_dummy('Producer Back',			(0,0,-4000), 1, 30000, 1, (0,1,0))
        write_camera_dummy('Producer Right',			(4000,0,0), 1, 30000, 1, (0,1,0))
        write_camera_dummy('Producer Left',			(-4000,0,0), 1, 30000, 1, (0,1,0))

    def write_camera(my_cam):
        '''
        Write a blender camera
        '''
        render = scene.render
        width	= render.resolution_x
        height	= render.resolution_y
        aspect	= width / height

        data = my_cam.blenObject.data

        file.write('\n\tModel: "Model::%s", "Camera" {' % my_cam.fbxName )
        file.write('\n\t\tVersion: 232')
        loc, rot, scale, matrix, matrix_rot = write_object_props(my_cam.blenObject, None, my_cam.parRelMatrix())

        file.write('\n\t\t\tProperty: "Roll", "Roll", "A+",0')
        file.write('\n\t\t\tProperty: "FieldOfView", "FieldOfView", "A+",%.6f' % math.degrees(data.angle))
        file.write('\n\t\t\tProperty: "FieldOfViewX", "FieldOfView", "A+",1')
        file.write('\n\t\t\tProperty: "FieldOfViewY", "FieldOfView", "A+",1')
        # file.write('\n\t\t\tProperty: "FocalLength", "Real", "A+",14.0323972702026')
        file.write('\n\t\t\tProperty: "OpticalCenterX", "Real", "A+",%.6f' % data.shift_x) # not sure if this is in the correct units?
        file.write('\n\t\t\tProperty: "OpticalCenterY", "Real", "A+",%.6f' % data.shift_y) # ditto
        file.write('\n\t\t\tProperty: "BackgroundColor", "Color", "A+",0,0,0')
        file.write('\n\t\t\tProperty: "TurnTable", "Real", "A+",0')
        file.write('\n\t\t\tProperty: "DisplayTurnTableIcon", "bool", "",1')
        file.write('\n\t\t\tProperty: "Motion Blur Intensity", "Real", "A+",1')
        file.write('\n\t\t\tProperty: "UseMotionBlur", "bool", "",0')
        file.write('\n\t\t\tProperty: "UseRealTimeMotionBlur", "bool", "",1')
        file.write('\n\t\t\tProperty: "ResolutionMode", "enum", "",0')
        file.write('\n\t\t\tProperty: "ApertureMode", "enum", "",2')
        file.write('\n\t\t\tProperty: "GateFit", "enum", "",2')
        file.write('\n\t\t\tProperty: "CameraFormat", "enum", "",0')
        file.write('\n\t\t\tProperty: "AspectW", "double", "",%i' % width)
        file.write('\n\t\t\tProperty: "AspectH", "double", "",%i' % height)

        '''Camera aspect ratio modes.
            0 If the ratio mode is eWINDOW_SIZE, both width and height values aren't relevant.
            1 If the ratio mode is eFIXED_RATIO, the height value is set to 1.0 and the width value is relative to the height value.
            2 If the ratio mode is eFIXED_RESOLUTION, both width and height values are in pixels.
            3 If the ratio mode is eFIXED_WIDTH, the width value is in pixels and the height value is relative to the width value.
            4 If the ratio mode is eFIXED_HEIGHT, the height value is in pixels and the width value is relative to the height value.

        Definition at line 234 of file kfbxcamera.h. '''

        file.write('\n\t\t\tProperty: "PixelAspectRatio", "double", "",2')

        file.write('\n\t\t\tProperty: "UseFrameColor", "bool", "",0')
        file.write('\n\t\t\tProperty: "FrameColor", "ColorRGB", "",0.3,0.3,0.3')
        file.write('\n\t\t\tProperty: "ShowName", "bool", "",1')
        file.write('\n\t\t\tProperty: "ShowGrid", "bool", "",1')
        file.write('\n\t\t\tProperty: "ShowOpticalCenter", "bool", "",0')
        file.write('\n\t\t\tProperty: "ShowAzimut", "bool", "",1')
        file.write('\n\t\t\tProperty: "ShowTimeCode", "bool", "",0')
        file.write('\n\t\t\tProperty: "NearPlane", "double", "",%.6f' % data.clip_start)
        file.write('\n\t\t\tProperty: "FarPlane", "double", "",%.6f' % data.clip_end)
        file.write('\n\t\t\tProperty: "FilmWidth", "double", "",1.0')
        file.write('\n\t\t\tProperty: "FilmHeight", "double", "",1.0')
        file.write('\n\t\t\tProperty: "FilmAspectRatio", "double", "",%.6f' % aspect)
        file.write('\n\t\t\tProperty: "FilmSqueezeRatio", "double", "",1')
        file.write('\n\t\t\tProperty: "FilmFormatIndex", "enum", "",0')
        file.write('\n\t\t\tProperty: "ViewFrustum", "bool", "",1')
        file.write('\n\t\t\tProperty: "ViewFrustumNearFarPlane", "bool", "",0')
        file.write('\n\t\t\tProperty: "ViewFrustumBackPlaneMode", "enum", "",2')
        file.write('\n\t\t\tProperty: "BackPlaneDistance", "double", "",100')
        file.write('\n\t\t\tProperty: "BackPlaneDistanceMode", "enum", "",0')
        file.write('\n\t\t\tProperty: "ViewCameraToLookAt", "bool", "",1')
        file.write('\n\t\t\tProperty: "LockMode", "bool", "",0')
        file.write('\n\t\t\tProperty: "LockInterestNavigation", "bool", "",0')
        file.write('\n\t\t\tProperty: "FitImage", "bool", "",0')
        file.write('\n\t\t\tProperty: "Crop", "bool", "",0')
        file.write('\n\t\t\tProperty: "Center", "bool", "",1')
        file.write('\n\t\t\tProperty: "KeepRatio", "bool", "",1')
        file.write('\n\t\t\tProperty: "BackgroundMode", "enum", "",0')
        file.write('\n\t\t\tProperty: "BackgroundAlphaTreshold", "double", "",0.5')
        file.write('\n\t\t\tProperty: "ForegroundTransparent", "bool", "",1')
        file.write('\n\t\t\tProperty: "DisplaySafeArea", "bool", "",0')
        file.write('\n\t\t\tProperty: "SafeAreaDisplayStyle", "enum", "",1')
        file.write('\n\t\t\tProperty: "SafeAreaAspectRatio", "double", "",%.6f' % aspect)
        file.write('\n\t\t\tProperty: "Use2DMagnifierZoom", "bool", "",0')
        file.write('\n\t\t\tProperty: "2D Magnifier Zoom", "Real", "A+",100')
        file.write('\n\t\t\tProperty: "2D Magnifier X", "Real", "A+",50')
        file.write('\n\t\t\tProperty: "2D Magnifier Y", "Real", "A+",50')
        file.write('\n\t\t\tProperty: "CameraProjectionType", "enum", "",0')
        file.write('\n\t\t\tProperty: "UseRealTimeDOFAndAA", "bool", "",0')
        file.write('\n\t\t\tProperty: "UseDepthOfField", "bool", "",0')
        file.write('\n\t\t\tProperty: "FocusSource", "enum", "",0')
        file.write('\n\t\t\tProperty: "FocusAngle", "double", "",3.5')
        file.write('\n\t\t\tProperty: "FocusDistance", "double", "",200')
        file.write('\n\t\t\tProperty: "UseAntialiasing", "bool", "",0')
        file.write('\n\t\t\tProperty: "AntialiasingIntensity", "double", "",0.77777')
        file.write('\n\t\t\tProperty: "UseAccumulationBuffer", "bool", "",0')
        file.write('\n\t\t\tProperty: "FrameSamplingCount", "int", "",7')

        file.write('\n\t\t}')
        file.write('\n\t\tMultiLayer: 0')
        file.write('\n\t\tMultiTake: 0')
        file.write('\n\t\tShading: Y')
        file.write('\n\t\tCulling: "CullingOff"')
        file.write('\n\t\tTypeFlags: "Camera"')
        file.write('\n\t\tGeometryVersion: 124')
        file.write('\n\t\tPosition: %.6f,%.6f,%.6f' % loc)
        file.write('\n\t\tUp: %.6f,%.6f,%.6f' % tuple(matrix_rot * Vector((0.0, 1.0, 0.0))))
        file.write('\n\t\tLookAt: %.6f,%.6f,%.6f' % tuple(matrix_rot * Vector((0.0, 0.0, -1.0))))

        #file.write('\n\t\tUp: 0,0,0' )
        #file.write('\n\t\tLookAt: 0,0,0' )

        file.write('\n\t\tShowInfoOnMoving: 1')
        file.write('\n\t\tShowAudio: 0')
        file.write('\n\t\tAudioColor: 0,1,0')
        file.write('\n\t\tCameraOrthoZoom: 1')
        file.write('\n\t}')

    def write_light(my_light):
        light = my_light.blenObject.data
        file.write('\n\tModel: "Model::%s", "Light" {' % my_light.fbxName)
        file.write('\n\t\tVersion: 232')

        write_object_props(my_light.blenObject, None, my_light.parRelMatrix())

        # Why are these values here twice?????? - oh well, follow the holy sdk's output

        # Blender light types match FBX's, funny coincidence, we just need to
        # be sure that all unsupported types are made into a point light
        #ePOINT,
        #eDIRECTIONAL
        #eSPOT
        light_type_items = {'POINT': 0, 'SUN': 1, 'SPOT': 2, 'HEMI': 3, 'AREA': 4}
        light_type = light_type_items[light.type]

        if light_type > 2: light_type = 1 # hemi and area lights become directional

# 		mode = light.mode
        if light.shadow_method == 'RAY_SHADOW' or light.shadow_method == 'BUFFER_SHADOW':
# 		if mode & Blender.Lamp.Modes.RayShadow or mode & Blender.Lamp.Modes.Shadows:
            do_shadow = 1
        else:
            do_shadow = 0

        if light.use_only_shadow or (not light.use_diffuse and not light.use_specular):
# 		if mode & Blender.Lamp.Modes.OnlyShadow or (mode & Blender.Lamp.Modes.NoDiffuse and mode & Blender.Lamp.Modes.NoSpecular):
            do_light = 0
        else:
            do_light = 1

        scale = abs(GLOBAL_MATRIX.scale_part()[0]) # scale is always uniform in this case

        file.write('\n\t\t\tProperty: "LightType", "enum", "",%i' % light_type)
        file.write('\n\t\t\tProperty: "CastLightOnObject", "bool", "",1')
        file.write('\n\t\t\tProperty: "DrawVolumetricLight", "bool", "",1')
        file.write('\n\t\t\tProperty: "DrawGroundProjection", "bool", "",1')
        file.write('\n\t\t\tProperty: "DrawFrontFacingVolumetricLight", "bool", "",0')
        file.write('\n\t\t\tProperty: "GoboProperty", "object", ""')
        file.write('\n\t\t\tProperty: "Color", "Color", "A+",1,1,1')
        file.write('\n\t\t\tProperty: "Intensity", "Intensity", "A+",%.2f' % (min(light.energy*100, 200))) # clamp below 200
        if light.type == 'SPOT':
            file.write('\n\t\t\tProperty: "Cone angle", "Cone angle", "A+",%.2f' % math.degrees(light.spot_size))
        file.write('\n\t\t\tProperty: "Fog", "Fog", "A+",50')
        file.write('\n\t\t\tProperty: "Color", "Color", "A",%.2f,%.2f,%.2f' % tuple(light.color))

        file.write('\n\t\t\tProperty: "Intensity", "Intensity", "A+",%.2f' % (min(light.energy*100, 200))) # clamp below 200

        file.write('\n\t\t\tProperty: "Fog", "Fog", "A+",50')
        file.write('\n\t\t\tProperty: "LightType", "enum", "",%i' % light_type)
        file.write('\n\t\t\tProperty: "CastLightOnObject", "bool", "",%i' % do_light)
        file.write('\n\t\t\tProperty: "DrawGroundProjection", "bool", "",1')
        file.write('\n\t\t\tProperty: "DrawFrontFacingVolumetricLight", "bool", "",0')
        file.write('\n\t\t\tProperty: "DrawVolumetricLight", "bool", "",1')
        file.write('\n\t\t\tProperty: "GoboProperty", "object", ""')
        file.write('\n\t\t\tProperty: "DecayType", "enum", "",0')
        file.write('\n\t\t\tProperty: "DecayStart", "double", "",%.2f' % light.distance)
        file.write('\n\t\t\tProperty: "EnableNearAttenuation", "bool", "",0')
        file.write('\n\t\t\tProperty: "NearAttenuationStart", "double", "",0')
        file.write('\n\t\t\tProperty: "NearAttenuationEnd", "double", "",0')
        file.write('\n\t\t\tProperty: "EnableFarAttenuation", "bool", "",0')
        file.write('\n\t\t\tProperty: "FarAttenuationStart", "double", "",0')
        file.write('\n\t\t\tProperty: "FarAttenuationEnd", "double", "",0')
        file.write('\n\t\t\tProperty: "CastShadows", "bool", "",%i' % do_shadow)
        file.write('\n\t\t\tProperty: "ShadowColor", "ColorRGBA", "",0,0,0,1')
        file.write('\n\t\t}')
        file.write('\n\t\tMultiLayer: 0')
        file.write('\n\t\tMultiTake: 0')
        file.write('\n\t\tShading: Y')
        file.write('\n\t\tCulling: "CullingOff"')
        file.write('\n\t\tTypeFlags: "Light"')
        file.write('\n\t\tGeometryVersion: 124')
        file.write('\n\t}')

    # matrixOnly is not used at the moment
    def write_null(my_null = None, fbxName = None, matrixOnly = None):
        # ob can be null
        if not fbxName: fbxName = my_null.fbxName

        file.write('\n\tModel: "Model::%s", "Null" {' % fbxName)
        file.write('\n\t\tVersion: 232')

        # only use this for the root matrix at the moment
        if matrixOnly:
            poseMatrix = write_object_props(None, None, matrixOnly)[3]

        else: # all other Null's
            if my_null:		poseMatrix = write_object_props(my_null.blenObject, None, my_null.parRelMatrix())[3]
            else:			poseMatrix = write_object_props()[3]

        pose_items.append((fbxName, poseMatrix))

        file.write('''
        }
        MultiLayer: 0
        MultiTake: 1
        Shading: Y
        Culling: "CullingOff"
        TypeFlags: "Null"
    }''')

    # Material Settings
    if world:	world_amb = tuple(world.ambient_color)
# 	if world:	world_amb = world.getAmb()
    else:		world_amb = (0,0,0) # Default value

    def write_material(matname, mat):
        file.write('\n\tMaterial: "Material::%s", "" {' % matname)

        # Todo, add more material Properties.
        if mat:
            mat_cold = tuple(mat.diffuse_color)
            mat_cols = tuple(mat.specular_color)
            #mat_colm = tuple(mat.mirCol) # we wont use the mirror color
            mat_colamb = world_amb

            mat_dif = mat.diffuse_intensity
            mat_amb = mat.ambient
            mat_hard = (float(mat.specular_hardness)-1)/5.10
            mat_spec = mat.specular_intensity/2.0
            mat_alpha = mat.alpha
            mat_emit = mat.emit
            mat_shadeless = mat.use_shadeless
            if mat_shadeless:
                mat_shader = 'Lambert'
            else:
                if mat.diffuse_shader == 'LAMBERT':
                    mat_shader = 'Lambert'
                else:
                    mat_shader = 'Phong'
        else:
            mat_cols = mat_cold = 0.8, 0.8, 0.8
            mat_colamb = 0.0,0.0,0.0
            # mat_colm
            mat_dif = 1.0
            mat_amb = 0.5
            mat_hard = 20.0
            mat_spec = 0.2
            mat_alpha = 1.0
            mat_emit = 0.0
            mat_shadeless = False
            mat_shader = 'Phong'

        file.write('\n\t\tVersion: 102')
        file.write('\n\t\tShadingModel: "%s"' % mat_shader.lower())
        file.write('\n\t\tMultiLayer: 0')

        file.write('\n\t\tProperties60:  {')
        file.write('\n\t\t\tProperty: "ShadingModel", "KString", "", "%s"' % mat_shader)
        file.write('\n\t\t\tProperty: "MultiLayer", "bool", "",0')
        file.write('\n\t\t\tProperty: "EmissiveColor", "ColorRGB", "",%.4f,%.4f,%.4f' % mat_cold) # emit and diffuse color are he same in blender
        file.write('\n\t\t\tProperty: "EmissiveFactor", "double", "",%.4f' % mat_emit)

        file.write('\n\t\t\tProperty: "AmbientColor", "ColorRGB", "",%.4f,%.4f,%.4f' % mat_colamb)
        file.write('\n\t\t\tProperty: "AmbientFactor", "double", "",%.4f' % mat_amb)
        file.write('\n\t\t\tProperty: "DiffuseColor", "ColorRGB", "",%.4f,%.4f,%.4f' % mat_cold)
        file.write('\n\t\t\tProperty: "DiffuseFactor", "double", "",%.4f' % mat_dif)
        file.write('\n\t\t\tProperty: "Bump", "Vector3D", "",0,0,0')
        file.write('\n\t\t\tProperty: "TransparentColor", "ColorRGB", "",1,1,1')
        file.write('\n\t\t\tProperty: "TransparencyFactor", "double", "",%.4f' % (1.0 - mat_alpha))
        if not mat_shadeless:
            file.write('\n\t\t\tProperty: "SpecularColor", "ColorRGB", "",%.4f,%.4f,%.4f' % mat_cols)
            file.write('\n\t\t\tProperty: "SpecularFactor", "double", "",%.4f' % mat_spec)
            file.write('\n\t\t\tProperty: "ShininessExponent", "double", "",80.0')
            file.write('\n\t\t\tProperty: "ReflectionColor", "ColorRGB", "",0,0,0')
            file.write('\n\t\t\tProperty: "ReflectionFactor", "double", "",1')
        file.write('\n\t\t\tProperty: "Emissive", "ColorRGB", "",0,0,0')
        file.write('\n\t\t\tProperty: "Ambient", "ColorRGB", "",%.1f,%.1f,%.1f' % mat_colamb)
        file.write('\n\t\t\tProperty: "Diffuse", "ColorRGB", "",%.1f,%.1f,%.1f' % mat_cold)
        if not mat_shadeless:
            file.write('\n\t\t\tProperty: "Specular", "ColorRGB", "",%.1f,%.1f,%.1f' % mat_cols)
            file.write('\n\t\t\tProperty: "Shininess", "double", "",%.1f' % mat_hard)
        file.write('\n\t\t\tProperty: "Opacity", "double", "",%.1f' % mat_alpha)
        if not mat_shadeless:
            file.write('\n\t\t\tProperty: "Reflectivity", "double", "",0')

        file.write('\n\t\t}')
        file.write('\n\t}')

    def copy_image(image):
        fn = bpy.path.abspath(image.filepath)
        fn_strip = os.path.basename(fn)

        if EXP_IMAGE_COPY:
            rel = fn_strip
            fn_abs_dest = os.path.join(basepath, fn_strip)
            if not os.path.exists(fn_abs_dest):
                shutil.copy(fn, fn_abs_dest)
        else:
            rel = os.path.relpath(fn, basepath)

        return (rel, fn_strip)

    # tex is an Image (Arystan)
    def write_video(texname, tex):
        # Same as texture really!
        file.write('\n\tVideo: "Video::%s", "Clip" {' % texname)

        file.write('''
        Type: "Clip"
        Properties60:  {
            Property: "FrameRate", "double", "",0
            Property: "LastFrame", "int", "",0
            Property: "Width", "int", "",0
            Property: "Height", "int", "",0''')
        if tex:
            fname_rel, fname_strip = copy_image(tex)
# 			fname, fname_strip, fname_rel = derived_paths(tex.filepath, basepath, EXP_IMAGE_COPY)
        else:
            fname = fname_strip = fname_rel = ''

        file.write('\n\t\t\tProperty: "Path", "charptr", "", "%s"' % fname_strip)


        file.write('''
            Property: "StartFrame", "int", "",0
            Property: "StopFrame", "int", "",0
            Property: "PlaySpeed", "double", "",1
            Property: "Offset", "KTime", "",0
            Property: "InterlaceMode", "enum", "",0
            Property: "FreeRunning", "bool", "",0
            Property: "Loop", "bool", "",0
            Property: "AccessMode", "enum", "",0
        }
        UseMipMap: 0''')

        file.write('\n\t\tFilename: "%s"' % fname_strip)
        if fname_strip: fname_strip = '/' + fname_strip
        file.write('\n\t\tRelativeFilename: "%s"' % fname_rel) # make relative
        file.write('\n\t}')


    def write_texture(texname, tex, num):
        # if tex is None then this is a dummy tex
        file.write('\n\tTexture: "Texture::%s", "TextureVideoClip" {' % texname)
        file.write('\n\t\tType: "TextureVideoClip"')
        file.write('\n\t\tVersion: 202')
        # TODO, rare case _empty_ exists as a name.
        file.write('\n\t\tTextureName: "Texture::%s"' % texname)

        file.write('''
        Properties60:  {
            Property: "Translation", "Vector", "A+",0,0,0
            Property: "Rotation", "Vector", "A+",0,0,0
            Property: "Scaling", "Vector", "A+",1,1,1''')
        file.write('\n\t\t\tProperty: "Texture alpha", "Number", "A+",%i' % num)


        # WrapModeU/V 0==rep, 1==clamp, TODO add support
        file.write('''
            Property: "TextureTypeUse", "enum", "",0
            Property: "CurrentTextureBlendMode", "enum", "",1
            Property: "UseMaterial", "bool", "",0
            Property: "UseMipMap", "bool", "",0
            Property: "CurrentMappingType", "enum", "",0
            Property: "UVSwap", "bool", "",0''')

        file.write('\n\t\t\tProperty: "WrapModeU", "enum", "",%i' % tex.use_clamp_x)
        file.write('\n\t\t\tProperty: "WrapModeV", "enum", "",%i' % tex.use_clamp_y)

        file.write('''
            Property: "TextureRotationPivot", "Vector3D", "",0,0,0
            Property: "TextureScalingPivot", "Vector3D", "",0,0,0
            Property: "VideoProperty", "object", ""
        }''')

        file.write('\n\t\tMedia: "Video::%s"' % texname)

        if tex:
            fname_rel, fname_strip = copy_image(tex)
# 			fname, fname_strip, fname_rel = derived_paths(tex.filepath, basepath, EXP_IMAGE_COPY)
        else:
            fname = fname_strip = fname_rel = ''

        file.write('\n\t\tFileName: "%s"' % fname_strip)
        file.write('\n\t\tRelativeFilename: "%s"' % fname_rel) # need some make relative command

        file.write('''
        ModelUVTranslation: 0,0
        ModelUVScaling: 1,1
        Texture_Alpha_Source: "None"
        Cropping: 0,0,0,0
    }''')

    def write_deformer_skin(obname):
        '''
        Each mesh has its own deformer
        '''
        file.write('\n\tDeformer: "Deformer::Skin %s", "Skin" {' % obname)
        file.write('''
        Version: 100
        MultiLayer: 0
        Type: "Skin"
        Properties60:  {
        }
        Link_DeformAcuracy: 50
    }''')

    # in the example was 'Bip01 L Thigh_2'
    def write_sub_deformer_skin(my_mesh, my_bone, weights):

        '''
        Each subdeformer is spesific to a mesh, but the bone it links to can be used by many sub-deformers
        So the SubDeformer needs the mesh-object name as a prefix to make it unique

        Its possible that there is no matching vgroup in this mesh, in that case no verts are in the subdeformer,
        a but silly but dosnt really matter
        '''
        file.write('\n\tDeformer: "SubDeformer::Cluster %s %s", "Cluster" {' % (my_mesh.fbxName, my_bone.fbxName))

        file.write('''
        Version: 100
        MultiLayer: 0
        Type: "Cluster"
        Properties60:  {
            Property: "SrcModel", "object", ""
            Property: "SrcModelReference", "object", ""
        }
        UserData: "", ""''')

        # Support for bone parents
        if my_mesh.fbxBoneParent:
            if my_mesh.fbxBoneParent == my_bone:
                # TODO - this is a bit lazy, we could have a simple write loop
                # for this case because all weights are 1.0 but for now this is ok
                # Parent Bones arent used all that much anyway.
                vgroup_data = [(j, 1.0) for j in range(len(my_mesh.blenData.vertices))]
            else:
                # This bone is not a parent of this mesh object, no weights
                vgroup_data = []

        else:
            # Normal weight painted mesh
            if my_bone.blenName in weights[0]:
                # Before we used normalized wright list
                #vgroup_data = me.getVertsFromGroup(bone.name, 1)
                group_index = weights[0].index(my_bone.blenName)
                vgroup_data = [(j, weight[group_index]) for j, weight in enumerate(weights[1]) if weight[group_index]]
            else:
                vgroup_data = []

        file.write('\n\t\tIndexes: ')

        i = -1
        for vg in vgroup_data:
            if i == -1:
                file.write('%i'  % vg[0])
                i=0
            else:
                if i==23:
                    file.write('\n\t\t')
                    i=0
                file.write(',%i' % vg[0])
            i+=1

        file.write('\n\t\tWeights: ')
        i = -1
        for vg in vgroup_data:
            if i == -1:
                file.write('%.8f'  % vg[1])
                i=0
            else:
                if i==38:
                    file.write('\n\t\t')
                    i=0
                file.write(',%.8f' % vg[1])
            i+=1

        if my_mesh.fbxParent:
            # TODO FIXME, this case is broken in some cases. skinned meshes just shouldnt have parents where possible!
            m = (my_mesh.matrixWorld.copy().invert() * my_bone.fbxArm.matrixWorld.copy() * my_bone.restMatrix) * mtx4_z90
        else:
            # Yes! this is it...  - but dosnt work when the mesh is a.
            m = (my_mesh.matrixWorld.copy().invert() * my_bone.fbxArm.matrixWorld.copy() * my_bone.restMatrix) * mtx4_z90

        #m = mtx4_z90 * my_bone.restMatrix
        matstr = mat4x4str(m)
        matstr_i = mat4x4str(m.invert())

        file.write('\n\t\tTransform: %s' % matstr_i) # THIS IS __NOT__ THE GLOBAL MATRIX AS DOCUMENTED :/
        file.write('\n\t\tTransformLink: %s' % matstr)
        file.write('\n\t}')

    def write_mesh(my_mesh):

        me = my_mesh.blenData

        # if there are non NULL materials on this mesh
        if my_mesh.blenMaterials:	do_materials = True
        else:						do_materials = False

        if my_mesh.blenTextures:	do_textures = True
        else:						do_textures = False

        do_uvs = bool(me.uv_textures)


        file.write('\n\tModel: "Model::%s", "Mesh" {' % my_mesh.fbxName)
        file.write('\n\t\tVersion: 232') # newline is added in write_object_props

        poseMatrix = write_object_props(my_mesh.blenObject, None, my_mesh.parRelMatrix())[3]
        pose_items.append((my_mesh.fbxName, poseMatrix))

        file.write('\n\t\t}')
        file.write('\n\t\tMultiLayer: 0')
        file.write('\n\t\tMultiTake: 1')
        file.write('\n\t\tShading: Y')
        file.write('\n\t\tCulling: "CullingOff"')


        # Write the Real Mesh data here
        file.write('\n\t\tVertices: ')
        i=-1

        for v in me.vertices:
            if i==-1:
                file.write('%.6f,%.6f,%.6f' % tuple(v.co));	i=0
            else:
                if i==7:
                    file.write('\n\t\t');	i=0
                file.write(',%.6f,%.6f,%.6f'% tuple(v.co))
            i+=1

        file.write('\n\t\tPolygonVertexIndex: ')
        i=-1
        for f in me.faces:
            fi = f.vertices[:]

            # last index XORd w. -1 indicates end of face
            fi[-1] = fi[-1] ^ -1
            fi = tuple(fi)

            if i==-1:
                if len(fi) == 3:	file.write('%i,%i,%i' % fi )
                else:				file.write('%i,%i,%i,%i' % fi )
                i=0
            else:
                if i==13:
                    file.write('\n\t\t')
                    i=0
                if len(fi) == 3:	file.write(',%i,%i,%i' % fi )
                else:				file.write(',%i,%i,%i,%i' % fi )
            i+=1

        # write loose edges as faces.
        for ed in me.edges:
            if ed.is_loose:
                ed_val = ed.vertices[:]
                ed_val = ed_val[0], ed_val[-1] ^ -1

                if i==-1:
                    file.write('%i,%i' % ed_val)
                    i=0
                else:
                    if i==13:
                        file.write('\n\t\t')
                        i=0
                    file.write(',%i,%i' % ed_val)
            i+=1


        file.write('\n\t\tEdges: ')
        i=-1
        for ed in me.edges:
                if i==-1:
                    file.write('%i,%i' % (ed.vertices[0], ed.vertices[1]))
                    i=0
                else:
                    if i==13:
                        file.write('\n\t\t')
                        i=0
                    file.write(',%i,%i' % (ed.vertices[0], ed.vertices[1]))
                i+=1

        file.write('\n\t\tGeometryVersion: 124')

        file.write('''
        LayerElementNormal: 0 {
            Version: 101
            Name: ""
            MappingInformationType: "ByVertice"
            ReferenceInformationType: "Direct"
            Normals: ''')

        i=-1
        for v in me.vertices:
            if i==-1:
                file.write('%.15f,%.15f,%.15f' % tuple(v.normal));	i=0
            else:
                if i==2:
                    file.write('\n			 ');	i=0
                file.write(',%.15f,%.15f,%.15f' % tuple(v.normal))
            i+=1
        file.write('\n\t\t}')

        # Write Face Smoothing
        file.write('''
        LayerElementSmoothing: 0 {
            Version: 102
            Name: ""
            MappingInformationType: "ByPolygon"
            ReferenceInformationType: "Direct"
            Smoothing: ''')

        i=-1
        for f in me.faces:
            if i==-1:
                file.write('%i' % f.use_smooth);	i=0
            else:
                if i==54:
                    file.write('\n			 ');	i=0
                file.write(',%i' % f.use_smooth)
            i+=1

        file.write('\n\t\t}')

        # Write Edge Smoothing
        file.write('''
        LayerElementSmoothing: 0 {
            Version: 101
            Name: ""
            MappingInformationType: "ByEdge"
            ReferenceInformationType: "Direct"
            Smoothing: ''')

        i=-1
        for ed in me.edges:
            if i==-1:
                file.write('%i' % (ed.use_edge_sharp));	i=0
            else:
                if i==54:
                    file.write('\n			 ');	i=0
                file.write(',%i' % (ed.use_edge_sharp))
            i+=1

        file.write('\n\t\t}')

        # small utility function
        # returns a slice of data depending on number of face verts
        # data is either a MeshTextureFace or MeshColor
        def face_data(data, face):
            totvert = len(f.vertices)

            return data[:totvert]


        # Write VertexColor Layers
        # note, no programs seem to use this info :/
        collayers = []
        if len(me.vertex_colors):
            collayers = me.vertex_colors
            for colindex, collayer in enumerate(collayers):
                file.write('\n\t\tLayerElementColor: %i {' % colindex)
                file.write('\n\t\t\tVersion: 101')
                file.write('\n\t\t\tName: "%s"' % collayer.name)

                file.write('''
            MappingInformationType: "ByPolygonVertex"
            ReferenceInformationType: "IndexToDirect"
            Colors: ''')

                i = -1
                ii = 0 # Count how many Colors we write

                for f, cf in zip(me.faces, collayer.data):
                    colors = [cf.color1, cf.color2, cf.color3, cf.color4]

                    # determine number of verts
                    colors = face_data(colors, f)

                    for col in colors:
                        if i==-1:
                            file.write('%.4f,%.4f,%.4f,1' % tuple(col))
                            i=0
                        else:
                            if i==7:
                                file.write('\n\t\t\t\t')
                                i=0
                            file.write(',%.4f,%.4f,%.4f,1' % tuple(col))
                        i+=1
                        ii+=1 # One more Color

                file.write('\n\t\t\tColorIndex: ')
                i = -1
                for j in range(ii):
                    if i == -1:
                        file.write('%i' % j)
                        i=0
                    else:
                        if i==55:
                            file.write('\n\t\t\t\t')
                            i=0
                        file.write(',%i' % j)
                    i+=1

                file.write('\n\t\t}')



        # Write UV and texture layers.
        uvlayers = []
        if do_uvs:
            uvlayers = me.uv_textures
            uvlayer_orig = me.uv_textures.active
            for uvindex, uvlayer in enumerate(me.uv_textures):
                file.write('\n\t\tLayerElementUV: %i {' % uvindex)
                file.write('\n\t\t\tVersion: 101')
                file.write('\n\t\t\tName: "%s"' % uvlayer.name)

                file.write('''
            MappingInformationType: "ByPolygonVertex"
            ReferenceInformationType: "IndexToDirect"
            UV: ''')

                i = -1
                ii = 0 # Count how many UVs we write

                for uf in uvlayer.data:
                    # workaround, since uf.uv iteration is wrong atm
                    for uv in uf.uv:
                        if i==-1:
                            file.write('%.6f,%.6f' % tuple(uv))
                            i=0
                        else:
                            if i==7:
                                file.write('\n			 ')
                                i=0
                            file.write(',%.6f,%.6f' % tuple(uv))
                        i+=1
                        ii+=1 # One more UV

                file.write('\n\t\t\tUVIndex: ')
                i = -1
                for j in range(ii):
                    if i == -1:
                        file.write('%i'  % j)
                        i=0
                    else:
                        if i==55:
                            file.write('\n\t\t\t\t')
                            i=0
                        file.write(',%i' % j)
                    i+=1

                file.write('\n\t\t}')

                if do_textures:
                    file.write('\n\t\tLayerElementTexture: %i {' % uvindex)
                    file.write('\n\t\t\tVersion: 101')
                    file.write('\n\t\t\tName: "%s"' % uvlayer.name)

                    if len(my_mesh.blenTextures) == 1:
                        file.write('\n\t\t\tMappingInformationType: "AllSame"')
                    else:
                        file.write('\n\t\t\tMappingInformationType: "ByPolygon"')

                    file.write('\n\t\t\tReferenceInformationType: "IndexToDirect"')
                    file.write('\n\t\t\tBlendMode: "Translucent"')
                    file.write('\n\t\t\tTextureAlpha: 1')
                    file.write('\n\t\t\tTextureId: ')

                    if len(my_mesh.blenTextures) == 1:
                        file.write('0')
                    else:
                        texture_mapping_local = {None:-1}

                        i = 0 # 1 for dummy
                        for tex in my_mesh.blenTextures:
                            if tex: # None is set above
                                texture_mapping_local[tex] = i
                                i+=1

                        i=-1
                        for f in uvlayer.data:
                            img_key = f.image

                            if i==-1:
                                i=0
                                file.write( '%s' % texture_mapping_local[img_key])
                            else:
                                if i==55:
                                    file.write('\n			 ')
                                    i=0

                                file.write(',%s' % texture_mapping_local[img_key])
                            i+=1

                else:
                    file.write('''
        LayerElementTexture: 0 {
            Version: 101
            Name: ""
            MappingInformationType: "NoMappingInformation"
            ReferenceInformationType: "IndexToDirect"
            BlendMode: "Translucent"
            TextureAlpha: 1
            TextureId: ''')
                file.write('\n\t\t}')


        # Done with UV/textures.

        if do_materials:
            file.write('\n\t\tLayerElementMaterial: 0 {')
            file.write('\n\t\t\tVersion: 101')
            file.write('\n\t\t\tName: ""')

            if len(my_mesh.blenMaterials) == 1:
                file.write('\n\t\t\tMappingInformationType: "AllSame"')
            else:
                file.write('\n\t\t\tMappingInformationType: "ByPolygon"')

            file.write('\n\t\t\tReferenceInformationType: "IndexToDirect"')
            file.write('\n\t\t\tMaterials: ')

            if len(my_mesh.blenMaterials) == 1:
                file.write('0')
            else:
                # Build a material mapping for this
                material_mapping_local = {} # local-mat & tex : global index.

                for j, mat_tex_pair in enumerate(my_mesh.blenMaterials):
                    material_mapping_local[mat_tex_pair] = j

                len_material_mapping_local = len(material_mapping_local)

                mats = my_mesh.blenMaterialList

                if me.uv_textures.active:
                    uv_faces = me.uv_textures.active.data
                else:
                    uv_faces = [None] * len(me.faces)

                i=-1
                for f, uf in zip(me.faces, uv_faces):
# 				for f in me.faces:
                    try:	mat = mats[f.material_index]
                    except:mat = None

                    if do_uvs: tex = uf.image # WARNING - MULTI UV LAYER IMAGES NOT SUPPORTED :/
                    else: tex = None

                    if i==-1:
                        i=0
                        file.write( '%s' % (material_mapping_local[mat, tex])) # None for mat or tex is ok
                    else:
                        if i==55:
                            file.write('\n\t\t\t\t')
                            i=0

                        file.write(',%s' % (material_mapping_local[mat, tex]))
                    i+=1

            file.write('\n\t\t}')

        file.write('''
        Layer: 0 {
            Version: 100
            LayerElement:  {
                Type: "LayerElementNormal"
                TypedIndex: 0
            }''')

        if do_materials:
            file.write('''
            LayerElement:  {
                Type: "LayerElementMaterial"
                TypedIndex: 0
            }''')

        # Always write this
        if do_textures:
            file.write('''
            LayerElement:  {
                Type: "LayerElementTexture"
                TypedIndex: 0
            }''')

        if me.vertex_colors:
            file.write('''
            LayerElement:  {
                Type: "LayerElementColor"
                TypedIndex: 0
            }''')

        if do_uvs: # same as me.faceUV
            file.write('''
            LayerElement:  {
                Type: "LayerElementUV"
                TypedIndex: 0
            }''')


        file.write('\n\t\t}')

        if len(uvlayers) > 1:
            for i in range(1, len(uvlayers)):

                file.write('\n\t\tLayer: %i {' % i)
                file.write('\n\t\t\tVersion: 100')

                file.write('''
            LayerElement:  {
                Type: "LayerElementUV"''')

                file.write('\n\t\t\t\tTypedIndex: %i' % i)
                file.write('\n\t\t\t}')

                if do_textures:

                    file.write('''
            LayerElement:  {
                Type: "LayerElementTexture"''')

                    file.write('\n\t\t\t\tTypedIndex: %i' % i)
                    file.write('\n\t\t\t}')

                file.write('\n\t\t}')

        if len(collayers) > 1:
            # Take into account any UV layers
            layer_offset = 0
            if uvlayers: layer_offset = len(uvlayers)-1

            for i in range(layer_offset, len(collayers)+layer_offset):
                file.write('\n\t\tLayer: %i {' % i)
                file.write('\n\t\t\tVersion: 100')

                file.write('''
            LayerElement:  {
                Type: "LayerElementColor"''')

                file.write('\n\t\t\t\tTypedIndex: %i' % i)
                file.write('\n\t\t\t}')
                file.write('\n\t\t}')
        file.write('\n\t}')

    def write_group(name):
        file.write('\n\tGroupSelection: "GroupSelection::%s", "Default" {' % name)

        file.write('''
        Properties60:  {
            Property: "MultiLayer", "bool", "",0
            Property: "Pickable", "bool", "",1
            Property: "Transformable", "bool", "",1
            Property: "Show", "bool", "",1
        }
        MultiLayer: 0
    }''')


    # add meshes here to clear because they are not used anywhere.
    meshes_to_clear = []

    ob_meshes = []
    ob_lights = []
    ob_cameras = []
    # in fbx we export bones as children of the mesh
    # armatures not a part of a mesh, will be added to ob_arms
    ob_bones = []
    ob_arms = []
    ob_null = [] # emptys

    # List of types that have blender objects (not bones)
    ob_all_typegroups = [ob_meshes, ob_lights, ob_cameras, ob_arms, ob_null]

    groups = [] # blender groups, only add ones that have objects in the selections
    materials = {} # (mat, image) keys, should be a set()
    textures = {} # should be a set()

    tmp_ob_type = ob_type = None # incase no objects are exported, so as not to raise an error

    # if EXP_OBS_SELECTED is false, use sceens objects
    if not batch_objects:
        if EXP_OBS_SELECTED:	tmp_objects = context.selected_objects
        else:					tmp_objects = scene.objects
    else:
        tmp_objects = batch_objects

    if EXP_ARMATURE:
        # This is needed so applying modifiers dosnt apply the armature deformation, its also needed
        # ...so mesh objects return their rest worldspace matrix when bone-parents are exported as weighted meshes.
        # set every armature to its rest, backup the original values so we done mess up the scene
        ob_arms_orig_rest = [arm.pose_position for arm in bpy.data.armatures]

        for arm in bpy.data.armatures:
            arm.pose_position = 'REST'

        if ob_arms_orig_rest:
            for ob_base in bpy.data.objects:
                if ob_base.type == 'ARMATURE':
                    ob_base.update(scene)

            # This causes the makeDisplayList command to effect the mesh
            scene.frame_set(scene.frame_current)


    for ob_base in tmp_objects:

        # ignore dupli children
        if ob_base.parent and ob_base.parent.dupli_type != 'NONE':
            continue

        obs = [(ob_base, ob_base.matrix_world)]
        if ob_base.dupli_type != 'NONE':
            ob_base.create_dupli_list(scene)
            obs = [(dob.object, dob.matrix) for dob in ob_base.dupli_list]

        for ob, mtx in obs:
# 		for ob, mtx in BPyObject.getDerivedObjects(ob_base):
            tmp_ob_type = ob.type
            if tmp_ob_type == 'CAMERA':
                if EXP_CAMERA:
                    ob_cameras.append(my_object_generic(ob, mtx))
            elif tmp_ob_type == 'LAMP':
                if EXP_LAMP:
                    ob_lights.append(my_object_generic(ob, mtx))
            elif tmp_ob_type == 'ARMATURE':
                if EXP_ARMATURE:
                    # TODO - armatures dont work in dupligroups!
                    if ob not in ob_arms: ob_arms.append(ob)
                    # ob_arms.append(ob) # replace later. was "ob_arms.append(sane_obname(ob), ob)"
            elif tmp_ob_type == 'EMPTY':
                if EXP_EMPTY:
                    ob_null.append(my_object_generic(ob, mtx))
            elif EXP_MESH:
                origData = True
                if tmp_ob_type != 'MESH':
# 					me = bpy.data.meshes.new()
                    try:	me = ob.create_mesh(scene, True, 'PREVIEW')
# 					try:	me.getFromObject(ob)
                    except:	me = None
                    if me:
                        meshes_to_clear.append( me )
                        mats = me.materials
                        origData = False
                else:
                    # Mesh Type!
                    if EXP_MESH_APPLY_MOD:
# 						me = bpy.data.meshes.new()
                        me = ob.create_mesh(scene, True, 'PREVIEW')
# 						me.getFromObject(ob)

                        # print ob, me, me.getVertGroupNames()
                        meshes_to_clear.append( me )
                        origData = False
                        mats = me.materials
                    else:
                        me = ob.data
# 						me = ob.getData(mesh=1)
                        mats = me.materials

# 						# Support object colors
# 						tmp_colbits = ob.colbits
# 						if tmp_colbits:
# 							tmp_ob_mats = ob.getMaterials(1) # 1 so we get None's too.
# 							for i in xrange(16):
# 								if tmp_colbits & (1<<i):
# 									mats[i] = tmp_ob_mats[i]
# 							del tmp_ob_mats
# 						del tmp_colbits


                if me:
# 					# This WILL modify meshes in blender if EXP_MESH_APPLY_MOD is disabled.
# 					# so strictly this is bad. but only in rare cases would it have negative results
# 					# say with dupliverts the objects would rotate a bit differently
# 					if EXP_MESH_HQ_NORMALS:
# 						BPyMesh.meshCalcNormals(me) # high quality normals nice for realtime engines.

                    texture_mapping_local = {}
                    material_mapping_local = {}
                    if me.uv_textures:
                        for uvlayer in me.uv_textures:
                            for f, uf in zip(me.faces, uvlayer.data):
                                tex = uf.image
                                textures[tex] = texture_mapping_local[tex] = None

                                try: mat = mats[f.material_index]
                                except: mat = None

                                materials[mat, tex] = material_mapping_local[mat, tex] = None # should use sets, wait for blender 2.5

                    else:
                        for mat in mats:
                            # 2.44 use mat.lib too for uniqueness
                            materials[mat, None] = material_mapping_local[mat, None] = None
                        else:
                            materials[None, None] = None

                    if EXP_ARMATURE:
                        armob = ob.find_armature()
                        blenParentBoneName = None

                        # parent bone - special case
                        if (not armob) and ob.parent and ob.parent.type == 'ARMATURE' and \
                                ob.parent_type == 'BONE':
                            armob = ob.parent
                            blenParentBoneName = ob.parent_bone


                        if armob and armob not in ob_arms:
                            ob_arms.append(armob)

                    else:
                        blenParentBoneName = armob = None

                    my_mesh = my_object_generic(ob, mtx)
                    my_mesh.blenData =		me
                    my_mesh.origData = 		origData
                    my_mesh.blenMaterials =	list(material_mapping_local.keys())
                    my_mesh.blenMaterialList = mats
                    my_mesh.blenTextures =	list(texture_mapping_local.keys())

                    # if only 1 null texture then empty the list
                    if len(my_mesh.blenTextures) == 1 and my_mesh.blenTextures[0] is None:
                        my_mesh.blenTextures = []

                    my_mesh.fbxArm =	armob					# replace with my_object_generic armature instance later
                    my_mesh.fbxBoneParent = blenParentBoneName	# replace with my_bone instance later

                    ob_meshes.append( my_mesh )

        # not forgetting to free dupli_list
        if ob_base.dupli_list: ob_base.free_dupli_list()


    if EXP_ARMATURE:
        # now we have the meshes, restore the rest arm position
        for i, arm in enumerate(bpy.data.armatures):
            arm.pose_position = ob_arms_orig_rest[i]

        if ob_arms_orig_rest:
            for ob_base in bpy.data.objects:
                if ob_base.type == 'ARMATURE':
                    ob_base.update(scene)
            # This causes the makeDisplayList command to effect the mesh
            scene.frame_set(scene.frame_current)

    del tmp_ob_type, tmp_objects

    # now we have collected all armatures, add bones
    for i, ob in enumerate(ob_arms):

        ob_arms[i] = my_arm = my_object_generic(ob)

        my_arm.fbxBones =		[]
        my_arm.blenData =		ob.data
        if ob.animation_data:
            my_arm.blenAction =	ob.animation_data.action
        else:
            my_arm.blenAction = None
# 		my_arm.blenAction =		ob.action
        my_arm.blenActionList =	[]

        # fbxName, blenderObject, my_bones, blenderActions
        #ob_arms[i] = fbxArmObName, ob, arm_my_bones, (ob.action, [])

        for bone in my_arm.blenData.bones:
            my_bone = my_bone_class(bone, my_arm)
            my_arm.fbxBones.append( my_bone )
            ob_bones.append( my_bone )

    # add the meshes to the bones and replace the meshes armature with own armature class
    #for obname, ob, mtx, me, mats, arm, armname in ob_meshes:
    for my_mesh in ob_meshes:
        # Replace
        # ...this could be sped up with dictionary mapping but its unlikely for
        # it ever to be a bottleneck - (would need 100+ meshes using armatures)
        if my_mesh.fbxArm:
            for my_arm in ob_arms:
                if my_arm.blenObject == my_mesh.fbxArm:
                    my_mesh.fbxArm = my_arm
                    break

        for my_bone in ob_bones:

            # The mesh uses this bones armature!
            if my_bone.fbxArm == my_mesh.fbxArm:
                my_bone.blenMeshes[my_mesh.fbxName] = me


                # parent bone: replace bone names with our class instances
                # my_mesh.fbxBoneParent is None or a blender bone name initialy, replacing if the names match.
                if my_mesh.fbxBoneParent == my_bone.blenName:
                    my_mesh.fbxBoneParent = my_bone

    bone_deformer_count = 0 # count how many bones deform a mesh
    my_bone_blenParent = None
    for my_bone in ob_bones:
        my_bone_blenParent = my_bone.blenBone.parent
        if my_bone_blenParent:
            for my_bone_parent in ob_bones:
                # Note 2.45rc2 you can compare bones normally
                if my_bone_blenParent.name == my_bone_parent.blenName and my_bone.fbxArm == my_bone_parent.fbxArm:
                    my_bone.parent = my_bone_parent
                    break

        # Not used at the moment
        # my_bone.calcRestMatrixLocal()
        bone_deformer_count += len(my_bone.blenMeshes)

    del my_bone_blenParent


    # Build blenObject -> fbxObject mapping
    # this is needed for groups as well as fbxParenting
    for ob in bpy.data.objects:	ob.tag = False

    # using a list of object names for tagging (Arystan)

    tmp_obmapping = {}
    for ob_generic in ob_all_typegroups:
        for ob_base in ob_generic:
            ob_base.blenObject.tag = True
            tmp_obmapping[ob_base.blenObject] = ob_base

    # Build Groups from objects we export
    for blenGroup in bpy.data.groups:
        fbxGroupName = None
        for ob in blenGroup.objects:
            if ob.tag:
                if fbxGroupName is None:
                    fbxGroupName = sane_groupname(blenGroup)
                    groups.append((fbxGroupName, blenGroup))

                tmp_obmapping[ob].fbxGroupNames.append(fbxGroupName) # also adds to the objects fbxGroupNames

    groups.sort() # not really needed

    # Assign parents using this mapping
    for ob_generic in ob_all_typegroups:
        for my_ob in ob_generic:
            parent = my_ob.blenObject.parent
            if parent and parent.tag: # does it exist and is it in the mapping
                my_ob.fbxParent = tmp_obmapping[parent]


    del tmp_obmapping
    # Finished finding groups we use


    materials =	[(sane_matname(mat_tex_pair), mat_tex_pair) for mat_tex_pair in materials.keys()]
    textures =	[(sane_texname(tex), tex) for tex in textures.keys()  if tex]
    materials.sort() # sort by name
    textures.sort()

    camera_count = 8
    file.write('''

; Object definitions
;------------------------------------------------------------------

Definitions:  {
    Version: 100
    Count: %i''' % (\
        1+1+camera_count+\
        len(ob_meshes)+\
        len(ob_lights)+\
        len(ob_cameras)+\
        len(ob_arms)+\
        len(ob_null)+\
        len(ob_bones)+\
        bone_deformer_count+\
        len(materials)+\
        (len(textures)*2))) # add 1 for the root model 1 for global settings

    del bone_deformer_count

    file.write('''
    ObjectType: "Model" {
        Count: %i
    }''' % (\
        1+camera_count+\
        len(ob_meshes)+\
        len(ob_lights)+\
        len(ob_cameras)+\
        len(ob_arms)+\
        len(ob_null)+\
        len(ob_bones))) # add 1 for the root model

    file.write('''
    ObjectType: "Geometry" {
        Count: %i
    }''' % len(ob_meshes))

    if materials:
        file.write('''
    ObjectType: "Material" {
        Count: %i
    }''' % len(materials))

    if textures:
        file.write('''
    ObjectType: "Texture" {
        Count: %i
    }''' % len(textures)) # add 1 for an empty tex
        file.write('''
    ObjectType: "Video" {
        Count: %i
    }''' % len(textures)) # add 1 for an empty tex

    tmp = 0
    # Add deformer nodes
    for my_mesh in ob_meshes:
        if my_mesh.fbxArm:
            tmp+=1

    # Add subdeformers
    for my_bone in ob_bones:
        tmp += len(my_bone.blenMeshes)

    if tmp:
        file.write('''
    ObjectType: "Deformer" {
        Count: %i
    }''' % tmp)
    del tmp

    # we could avoid writing this possibly but for now just write it

    file.write('''
    ObjectType: "Pose" {
        Count: 1
    }''')

    if groups:
        file.write('''
    ObjectType: "GroupSelection" {
        Count: %i
    }''' % len(groups))

    file.write('''
    ObjectType: "GlobalSettings" {
        Count: 1
    }
}''')

    file.write('''

; Object properties
;------------------------------------------------------------------

Objects:  {''')

    # To comply with other FBX FILES
    write_camera_switch()

    # Write the null object
    write_null(None, 'blend_root')# , GLOBAL_MATRIX)

    for my_null in ob_null:
        write_null(my_null)

    for my_arm in ob_arms:
        write_null(my_arm)

    for my_cam in ob_cameras:
        write_camera(my_cam)

    for my_light in ob_lights:
        write_light(my_light)

    for my_mesh in ob_meshes:
        write_mesh(my_mesh)

    #for bonename, bone, obname, me, armob in ob_bones:
    for my_bone in ob_bones:
        write_bone(my_bone)

    write_camera_default()

    for matname, (mat, tex) in materials:
        write_material(matname, mat) # We only need to have a material per image pair, but no need to write any image info into the material (dumb fbx standard)

    # each texture uses a video, odd
    for texname, tex in textures:
        write_video(texname, tex)
    i = 0
    for texname, tex in textures:
        write_texture(texname, tex, i)
        i+=1

    for groupname, group in groups:
        write_group(groupname)

    # NOTE - c4d and motionbuilder dont need normalized weights, but deep-exploration 5 does and (max?) do.

    # Write armature modifiers
    # TODO - add another MODEL? - because of this skin definition.
    for my_mesh in ob_meshes:
        if my_mesh.fbxArm:
            write_deformer_skin(my_mesh.fbxName)

            # Get normalized weights for temorary use
            if my_mesh.fbxBoneParent:
                weights = None
            else:
                weights = meshNormalizedWeights(my_mesh.blenObject, my_mesh.blenData)

            #for bonename, bone, obname, bone_mesh, armob in ob_bones:
            for my_bone in ob_bones:
                if me in iter(my_bone.blenMeshes.values()):
                    write_sub_deformer_skin(my_mesh, my_bone, weights)

    # Write pose's really weired, only needed when an armature and mesh are used together
    # each by themselves dont need pose data. for now only pose meshes and bones

    file.write('''
    Pose: "Pose::BIND_POSES", "BindPose" {
        Type: "BindPose"
        Version: 100
        Properties60:  {
        }
        NbPoseNodes: ''')
    file.write(str(len(pose_items)))


    for fbxName, matrix in pose_items:
        file.write('\n\t\tPoseNode:  {')
        file.write('\n\t\t\tNode: "Model::%s"' % fbxName )
        file.write('\n\t\t\tMatrix: %s' % mat4x4str(matrix if matrix else Matrix()))
        file.write('\n\t\t}')

    file.write('\n\t}')


    # Finish Writing Objects
    # Write global settings
    file.write('''
    GlobalSettings:  {
        Version: 1000
        Properties60:  {
            Property: "UpAxis", "int", "",1
            Property: "UpAxisSign", "int", "",1
            Property: "FrontAxis", "int", "",2
            Property: "FrontAxisSign", "int", "",1
            Property: "CoordAxis", "int", "",0
            Property: "CoordAxisSign", "int", "",1
            Property: "UnitScaleFactor", "double", "",100
        }
    }
''')
    file.write('}')

    file.write('''

; Object relations
;------------------------------------------------------------------

Relations:  {''')

    file.write('\n\tModel: "Model::blend_root", "Null" {\n\t}')

    for my_null in ob_null:
        file.write('\n\tModel: "Model::%s", "Null" {\n\t}' % my_null.fbxName)

    for my_arm in ob_arms:
        file.write('\n\tModel: "Model::%s", "Null" {\n\t}' % my_arm.fbxName)

    for my_mesh in ob_meshes:
        file.write('\n\tModel: "Model::%s", "Mesh" {\n\t}' % my_mesh.fbxName)

    # TODO - limbs can have the same name for multiple armatures, should prefix.
    #for bonename, bone, obname, me, armob in ob_bones:
    for my_bone in ob_bones:
        file.write('\n\tModel: "Model::%s", "Limb" {\n\t}' % my_bone.fbxName)

    for my_cam in ob_cameras:
        file.write('\n\tModel: "Model::%s", "Camera" {\n\t}' % my_cam.fbxName)

    for my_light in ob_lights:
        file.write('\n\tModel: "Model::%s", "Light" {\n\t}' % my_light.fbxName)

    file.write('''
    Model: "Model::Producer Perspective", "Camera" {
    }
    Model: "Model::Producer Top", "Camera" {
    }
    Model: "Model::Producer Bottom", "Camera" {
    }
    Model: "Model::Producer Front", "Camera" {
    }
    Model: "Model::Producer Back", "Camera" {
    }
    Model: "Model::Producer Right", "Camera" {
    }
    Model: "Model::Producer Left", "Camera" {
    }
    Model: "Model::Camera Switcher", "CameraSwitcher" {
    }''')

    for matname, (mat, tex) in materials:
        file.write('\n\tMaterial: "Material::%s", "" {\n\t}' % matname)

    if textures:
        for texname, tex in textures:
            file.write('\n\tTexture: "Texture::%s", "TextureVideoClip" {\n\t}' % texname)
        for texname, tex in textures:
            file.write('\n\tVideo: "Video::%s", "Clip" {\n\t}' % texname)

    # deformers - modifiers
    for my_mesh in ob_meshes:
        if my_mesh.fbxArm:
            file.write('\n\tDeformer: "Deformer::Skin %s", "Skin" {\n\t}' % my_mesh.fbxName)

    #for bonename, bone, obname, me, armob in ob_bones:
    for my_bone in ob_bones:
        for fbxMeshObName in my_bone.blenMeshes: # .keys() - fbxMeshObName
            # is this bone effecting a mesh?
            file.write('\n\tDeformer: "SubDeformer::Cluster %s %s", "Cluster" {\n\t}' % (fbxMeshObName, my_bone.fbxName))

    # This should be at the end
    # file.write('\n\tPose: "Pose::BIND_POSES", "BindPose" {\n\t}')

    for groupname, group in groups:
        file.write('\n\tGroupSelection: "GroupSelection::%s", "Default" {\n\t}' % groupname)

    file.write('\n}')
    file.write('''

; Object connections
;------------------------------------------------------------------

Connections:  {''')

    # NOTE - The FBX SDK dosnt care about the order but some importers DO!
    # for instance, defining the material->mesh connection
    # before the mesh->blend_root crashes cinema4d


    # write the fake root node
    file.write('\n\tConnect: "OO", "Model::blend_root", "Model::Scene"')

    for ob_generic in ob_all_typegroups: # all blender 'Object's we support
        for my_ob in ob_generic:
            if my_ob.fbxParent:
                file.write('\n\tConnect: "OO", "Model::%s", "Model::%s"' % (my_ob.fbxName, my_ob.fbxParent.fbxName))
            else:
                file.write('\n\tConnect: "OO", "Model::%s", "Model::blend_root"' % my_ob.fbxName)

    if materials:
        for my_mesh in ob_meshes:
            # Connect all materials to all objects, not good form but ok for now.
            for mat, tex in my_mesh.blenMaterials:
                if mat:	mat_name = mat.name
                else:	mat_name = None

                if tex:	tex_name = tex.name
                else:	tex_name = None

                file.write('\n\tConnect: "OO", "Material::%s", "Model::%s"' % (sane_name_mapping_mat[mat_name, tex_name], my_mesh.fbxName))

    if textures:
        for my_mesh in ob_meshes:
            if my_mesh.blenTextures:
                # file.write('\n\tConnect: "OO", "Texture::_empty_", "Model::%s"' % my_mesh.fbxName)
                for tex in my_mesh.blenTextures:
                    if tex:
                        file.write('\n\tConnect: "OO", "Texture::%s", "Model::%s"' % (sane_name_mapping_tex[tex.name], my_mesh.fbxName))

        for texname, tex in textures:
            file.write('\n\tConnect: "OO", "Video::%s", "Texture::%s"' % (texname, texname))

    for my_mesh in ob_meshes:
        if my_mesh.fbxArm:
            file.write('\n\tConnect: "OO", "Deformer::Skin %s", "Model::%s"' % (my_mesh.fbxName, my_mesh.fbxName))

    #for bonename, bone, obname, me, armob in ob_bones:
    for my_bone in ob_bones:
        for fbxMeshObName in my_bone.blenMeshes: # .keys()
            file.write('\n\tConnect: "OO", "SubDeformer::Cluster %s %s", "Deformer::Skin %s"' % (fbxMeshObName, my_bone.fbxName, fbxMeshObName))

    # limbs -> deformers
    # for bonename, bone, obname, me, armob in ob_bones:
    for my_bone in ob_bones:
        for fbxMeshObName in my_bone.blenMeshes: # .keys()
            file.write('\n\tConnect: "OO", "Model::%s", "SubDeformer::Cluster %s %s"' % (my_bone.fbxName, fbxMeshObName, my_bone.fbxName))


    #for bonename, bone, obname, me, armob in ob_bones:
    for my_bone in ob_bones:
        # Always parent to armature now
        if my_bone.parent:
            file.write('\n\tConnect: "OO", "Model::%s", "Model::%s"' % (my_bone.fbxName, my_bone.parent.fbxName) )
        else:
            # the armature object is written as an empty and all root level bones connect to it
            file.write('\n\tConnect: "OO", "Model::%s", "Model::%s"' % (my_bone.fbxName, my_bone.fbxArm.fbxName) )

    # groups
    if groups:
        for ob_generic in ob_all_typegroups:
            for ob_base in ob_generic:
                for fbxGroupName in ob_base.fbxGroupNames:
                    file.write('\n\tConnect: "OO", "Model::%s", "GroupSelection::%s"' % (ob_base.fbxName, fbxGroupName))

    for my_arm in ob_arms:
        file.write('\n\tConnect: "OO", "Model::%s", "Model::blend_root"' % my_arm.fbxName)

    file.write('\n}')


    # Needed for scene footer as well as animation
    render = scene.render

    # from the FBX sdk
    #define KTIME_ONE_SECOND        KTime (K_LONGLONG(46186158000))
    def fbx_time(t):
        # 0.5 + val is the same as rounding.
        return int(0.5 + ((t/fps) * 46186158000))

    fps = float(render.fps)
    start =	scene.frame_start
    end =	scene.frame_end
    if end < start: start, end = end, st

    # comment the following line, otherwise we dont get the pose
    # if start==end: ANIM_ENABLE = False

    # animations for these object types
    ob_anim_lists = ob_bones, ob_meshes, ob_null, ob_cameras, ob_lights, ob_arms

    if ANIM_ENABLE and [tmp for tmp in ob_anim_lists if tmp]:

        frame_orig = scene.frame_current

        if ANIM_OPTIMIZE:
            ANIM_OPTIMIZE_PRECISSION_FLOAT = 0.1 ** ANIM_OPTIMIZE_PRECISSION

        # default action, when no actions are avaioable
        tmp_actions = []
        blenActionDefault = None
        action_lastcompat = None

        # instead of tagging
        tagged_actions = []

        if ANIM_ACTION_ALL:
# 			bpy.data.actions.tag = False
            tmp_actions = bpy.data.actions[:]


            # find which actions are compatible with the armatures
            # blenActions is not yet initialized so do it now.
            tmp_act_count = 0
            for my_arm in ob_arms:

                # get the default name
                if not blenActionDefault:
                    blenActionDefault = my_arm.blenAction

                arm_bone_names = set([my_bone.blenName for my_bone in my_arm.fbxBones])

                for action in tmp_actions:

                    action_chan_names = arm_bone_names.intersection( set([g.name for g in action.groups]) )
# 					action_chan_names = arm_bone_names.intersection( set(action.getChannelNames()) )

                    if action_chan_names: # at least one channel matches.
                        my_arm.blenActionList.append(action)
                        tagged_actions.append(action.name)
# 						action.tag = True
                        tmp_act_count += 1

                        # incase there is no actions applied to armatures
                        action_lastcompat = action

            if tmp_act_count:
                # unlikely to ever happen but if no actions applied to armatures, just use the last compatible armature.
                if not blenActionDefault:
                    blenActionDefault = action_lastcompat

        del action_lastcompat

        tmp_actions.insert(0, None) # None is the default action

        file.write('''
;Takes and animation section
;----------------------------------------------------

Takes:  {''')

        if blenActionDefault:
            file.write('\n\tCurrent: "%s"' % sane_takename(blenActionDefault))
        else:
            file.write('\n\tCurrent: "Default Take"')

        for blenAction in tmp_actions:
            # we have tagged all actious that are used be selected armatures
            if blenAction:
                if blenAction.name in tagged_actions:
# 				if blenAction.tag:
                    print('\taction: "%s" exporting...' % blenAction.name)
                else:
                    print('\taction: "%s" has no armature using it, skipping' % blenAction.name)
                    continue

            if blenAction is None:
                # Warning, this only accounts for tmp_actions being [None]
                file.write('\n\tTake: "Default Take" {')
                act_start =	start
                act_end =	end
            else:
                # use existing name
                if blenAction == blenActionDefault: # have we already got the name
                    file.write('\n\tTake: "%s" {' % sane_name_mapping_take[blenAction.name])
                else:
                    file.write('\n\tTake: "%s" {' % sane_takename(blenAction))

                act_start, act_end = blenAction.frame_range
                act_start = int(act_start)
                act_end = int(act_end)

                # Set the action active
                for my_bone in ob_arms:
                    if ob.animation_data and blenAction in my_bone.blenActionList:
                        ob.animation_data.action = blenAction
                        # print '\t\tSetting Action!', blenAction
                # scene.update(1)

            file.write('\n\t\tFileName: "Default_Take.tak"') # ??? - not sure why this is needed
            file.write('\n\t\tLocalTime: %i,%i' % (fbx_time(act_start-1), fbx_time(act_end-1))) # ??? - not sure why this is needed
            file.write('\n\t\tReferenceTime: %i,%i' % (fbx_time(act_start-1), fbx_time(act_end-1))) # ??? - not sure why this is needed

            file.write('''

        ;Models animation
        ;----------------------------------------------------''')


            # set pose data for all bones
            # do this here incase the action changes
            '''
            for my_bone in ob_bones:
                my_bone.flushAnimData()
            '''
            i = act_start
            while i <= act_end:
                scene.frame_set(i)
                for ob_generic in ob_anim_lists:
                    for my_ob in ob_generic:
                        #Blender.Window.RedrawAll()
                        if ob_generic == ob_meshes and my_ob.fbxArm:
                            # We cant animate armature meshes!
                            pass
                        else:
                            my_ob.setPoseFrame(i)

                i+=1


            #for bonename, bone, obname, me, armob in ob_bones:
            for ob_generic in (ob_bones, ob_meshes, ob_null, ob_cameras, ob_lights, ob_arms):

                for my_ob in ob_generic:

                    if ob_generic == ob_meshes and my_ob.fbxArm:
                        # do nothing,
                        pass
                    else:

                        file.write('\n\t\tModel: "Model::%s" {' % my_ob.fbxName) # ??? - not sure why this is needed
                        file.write('\n\t\t\tVersion: 1.1')
                        file.write('\n\t\t\tChannel: "Transform" {')

                        context_bone_anim_mats = [ (my_ob.getAnimParRelMatrix(frame), my_ob.getAnimParRelMatrixRot(frame)) for frame in range(act_start, act_end+1) ]

                        # ----------------
                        # ----------------
                        for TX_LAYER, TX_CHAN in enumerate('TRS'): # transform, rotate, scale

                            if		TX_CHAN=='T':	context_bone_anim_vecs = [mtx[0].translation_part()	for mtx in context_bone_anim_mats]
                            elif	TX_CHAN=='S':	context_bone_anim_vecs = [mtx[0].scale_part()		for mtx in context_bone_anim_mats]
                            elif	TX_CHAN=='R':
                                # Was....
                                # elif 	TX_CHAN=='R':	context_bone_anim_vecs = [mtx[1].to_euler()			for mtx in context_bone_anim_mats]
                                #
                                # ...but we need to use the previous euler for compatible conversion.
                                context_bone_anim_vecs = []
                                prev_eul = None
                                for mtx in context_bone_anim_mats:
                                    if prev_eul:	prev_eul = mtx[1].to_euler('XYZ', prev_eul)
                                    else:			prev_eul = mtx[1].to_euler()
                                    context_bone_anim_vecs.append(eulerRadToDeg(prev_eul))

                            file.write('\n\t\t\t\tChannel: "%s" {' % TX_CHAN) # translation

                            for i in range(3):
                                # Loop on each axis of the bone
                                file.write('\n\t\t\t\t\tChannel: "%s" {'% ('XYZ'[i])) # translation
                                file.write('\n\t\t\t\t\t\tDefault: %.15f' % context_bone_anim_vecs[0][i] )
                                file.write('\n\t\t\t\t\t\tKeyVer: 4005')

                                if not ANIM_OPTIMIZE:
                                    # Just write all frames, simple but in-eficient
                                    file.write('\n\t\t\t\t\t\tKeyCount: %i' % (1 + act_end - act_start))
                                    file.write('\n\t\t\t\t\t\tKey: ')
                                    frame = act_start
                                    while frame <= act_end:
                                        if frame!=act_start:
                                            file.write(',')

                                        # Curve types are 'C,n' for constant, 'L' for linear
                                        # C,n is for bezier? - linear is best for now so we can do simple keyframe removal
                                        file.write('\n\t\t\t\t\t\t\t%i,%.15f,L'  % (fbx_time(frame-1), context_bone_anim_vecs[frame-act_start][i] ))
                                        frame+=1
                                else:
                                    # remove unneeded keys, j is the frame, needed when some frames are removed.
                                    context_bone_anim_keys = [ (vec[i], j) for j, vec in enumerate(context_bone_anim_vecs) ]

                                    # last frame to fisrt frame, missing 1 frame on either side.
                                    # removeing in a backwards loop is faster
                                    #for j in xrange( (act_end-act_start)-1, 0, -1 ):
                                    # j = (act_end-act_start)-1
                                    j = len(context_bone_anim_keys)-2
                                    while j > 0 and len(context_bone_anim_keys) > 2:
                                        # print j, len(context_bone_anim_keys)
                                        # Is this key the same as the ones next to it?

                                        # co-linear horizontal...
                                        if		abs(context_bone_anim_keys[j][0] - context_bone_anim_keys[j-1][0]) < ANIM_OPTIMIZE_PRECISSION_FLOAT and\
                                                abs(context_bone_anim_keys[j][0] - context_bone_anim_keys[j+1][0]) < ANIM_OPTIMIZE_PRECISSION_FLOAT:

                                            del context_bone_anim_keys[j]

                                        else:
                                            frame_range = float(context_bone_anim_keys[j+1][1] - context_bone_anim_keys[j-1][1])
                                            frame_range_fac1 = (context_bone_anim_keys[j+1][1] - context_bone_anim_keys[j][1]) / frame_range
                                            frame_range_fac2 = 1.0 - frame_range_fac1

                                            if abs(((context_bone_anim_keys[j-1][0]*frame_range_fac1 + context_bone_anim_keys[j+1][0]*frame_range_fac2)) - context_bone_anim_keys[j][0]) < ANIM_OPTIMIZE_PRECISSION_FLOAT:
                                                del context_bone_anim_keys[j]
                                            else:
                                                j-=1

                                        # keep the index below the list length
                                        if j > len(context_bone_anim_keys)-2:
                                            j = len(context_bone_anim_keys)-2

                                    if len(context_bone_anim_keys) == 2 and context_bone_anim_keys[0][0] == context_bone_anim_keys[1][0]:

                                        # This axis has no moton, its okay to skip KeyCount and Keys in this case
                                        # pass

                                        # better write one, otherwise we loose poses with no animation
                                        file.write('\n\t\t\t\t\t\tKeyCount: 1')
                                        file.write('\n\t\t\t\t\t\tKey: ')
                                        file.write('\n\t\t\t\t\t\t\t%i,%.15f,L'  % (fbx_time(start), context_bone_anim_keys[0][0]))
                                    else:
                                        # We only need to write these if there is at least one
                                        file.write('\n\t\t\t\t\t\tKeyCount: %i' % len(context_bone_anim_keys))
                                        file.write('\n\t\t\t\t\t\tKey: ')
                                        for val, frame in context_bone_anim_keys:
                                            if frame != context_bone_anim_keys[0][1]: # not the first
                                                file.write(',')
                                            # frame is already one less then blenders frame
                                            file.write('\n\t\t\t\t\t\t\t%i,%.15f,L'  % (fbx_time(frame), val ))

                                if		i==0:	file.write('\n\t\t\t\t\t\tColor: 1,0,0')
                                elif	i==1:	file.write('\n\t\t\t\t\t\tColor: 0,1,0')
                                elif	i==2:	file.write('\n\t\t\t\t\t\tColor: 0,0,1')

                                file.write('\n\t\t\t\t\t}')
                            file.write('\n\t\t\t\t\tLayerType: %i' % (TX_LAYER+1) )
                            file.write('\n\t\t\t\t}')

                        # ---------------

                        file.write('\n\t\t\t}')
                        file.write('\n\t\t}')

            # end the take
            file.write('\n\t}')

            # end action loop. set original actions
            # do this after every loop incase actions effect eachother.
            for my_bone in ob_arms:
                if my_bone.blenObject.animation_data:
                    my_bone.blenObject.animation_data.action = my_bone.blenAction

        file.write('\n}')

        scene.frame_set(frame_orig)

    else:
        # no animation
        file.write('\n;Takes and animation section')
        file.write('\n;----------------------------------------------------')
        file.write('\n')
        file.write('\nTakes:  {')
        file.write('\n\tCurrent: ""')
        file.write('\n}')


    # write meshes animation
    #for obname, ob, mtx, me, mats, arm, armname in ob_meshes:


    # Clear mesh data Only when writing with modifiers applied
    for me in meshes_to_clear:
        bpy.data.meshes.remove(me)

    # --------------------------- Footer
    if world:
        m = world.mist_settings
        has_mist = m.use_mist
        mist_intense = m.intensity
        mist_start = m.start
        mist_end = m.depth
        mist_height = m.height
        world_hor = world.horizon_color
    else:
        has_mist = mist_intense = mist_start = mist_end = mist_height = 0
        world_hor = 0, 0, 0

    file.write('\n;Version 5 settings')
    file.write('\n;------------------------------------------------------------------')
    file.write('\n')
    file.write('\nVersion5:  {')
    file.write('\n\tAmbientRenderSettings:  {')
    file.write('\n\t\tVersion: 101')
    file.write('\n\t\tAmbientLightColor: %.1f,%.1f,%.1f,0' % tuple(world_amb))
    file.write('\n\t}')
    file.write('\n\tFogOptions:  {')
    file.write('\n\t\tFlogEnable: %i' % has_mist)
    file.write('\n\t\tFogMode: 0')
    file.write('\n\t\tFogDensity: %.3f' % mist_intense)
    file.write('\n\t\tFogStart: %.3f' % mist_start)
    file.write('\n\t\tFogEnd: %.3f' % mist_end)
    file.write('\n\t\tFogColor: %.1f,%.1f,%.1f,1' % tuple(world_hor))
    file.write('\n\t}')
    file.write('\n\tSettings:  {')
    file.write('\n\t\tFrameRate: "%i"' % int(fps))
    file.write('\n\t\tTimeFormat: 1')
    file.write('\n\t\tSnapOnFrames: 0')
    file.write('\n\t\tReferenceTimeIndex: -1')
    file.write('\n\t\tTimeLineStartTime: %i' % fbx_time(start-1))
    file.write('\n\t\tTimeLineStopTime: %i' % fbx_time(end-1))
    file.write('\n\t}')
    file.write('\n\tRendererSetting:  {')
    file.write('\n\t\tDefaultCamera: "Producer Perspective"')
    file.write('\n\t\tDefaultViewingMode: 0')
    file.write('\n\t}')
    file.write('\n}')
    file.write('\n')

    # Incase sombody imports this, clean up by clearing global dicts
    sane_name_mapping_ob.clear()
    sane_name_mapping_mat.clear()
    sane_name_mapping_tex.clear()

    ob_arms[:] =	[]
    ob_bones[:] =	[]
    ob_cameras[:] =	[]
    ob_lights[:] =	[]
    ob_meshes[:] =	[]
    ob_null[:] =	[]


    # copy images if enabled
# 	if EXP_IMAGE_COPY:
# # 		copy_images( basepath,  [ tex[1] for tex in textures if tex[1] != None ])
# 		bpy.util.copy_images( [ tex[1] for tex in textures if tex[1] != None ], basepath)

    print('export finished in %.4f sec.' % (time.clock() - start_time))
    return {'FINISHED'}


# NOTES (all line numbers correspond to original export_fbx.py (under release/scripts)
# - Draw.PupMenu alternative in 2.5?, temporarily replaced PupMenu with print
# - get rid of bpy.path.clean_name somehow
# + fixed: isinstance(inst, bpy.types.*) doesn't work on RNA objects: line 565
# + get rid of BPyObject_getObjectArmature, move it in RNA?
# - BATCH_ENABLE and BATCH_GROUP options: line 327
# - implement all BPyMesh_* used here with RNA
# - getDerivedObjects is not fully replicated with .dupli* funcs
# - talk to Campbell, this code won't work? lines 1867-1875
# - don't know what those colbits are, do we need them? they're said to be deprecated in DNA_object_types.h: 1886-1893
# - no hq normals: 1900-1901

# TODO

# - bpy.data.remove_scene: line 366
# - bpy.sys.time move to bpy.sys.util?
# - new scene creation, activation: lines 327-342, 368
# - uses bpy.path.abspath, *.relpath - replace at least relpath
