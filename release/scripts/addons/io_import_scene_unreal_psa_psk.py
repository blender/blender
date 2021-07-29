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

bl_info = {
    "name": "Import Unreal Skeleton Mesh (.psk)/Animation Set (psa)",
    "author": "Darknet, flufy3d, camg188",
    "version": (2, 2, 0),
    "blender": (2, 64, 0),
    "location": "File > Import > Skeleton Mesh (.psk)/Animation Set (psa)",
    "description": "Import Skeleleton Mesh/Animation Data",
    "warning": "may produce errors, fix in progress",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.5/Py/"
                "Scripts/Import-Export/Unreal_psk_psa",
    "category": "Import-Export",
}

"""
Version': '2.0' ported by Darknet

Unreal Tournament PSK file to Blender mesh converter V1.0
Author: D.M. Sturgeon (camg188 at the elYsium forum), ported by Darknet
Imports a *psk file to a new mesh

-No UV Texutre
-No Weight
-No Armature Bones
-No Material ID
-Export Text Log From Current Location File (Bool )
"""

import bpy
import mathutils
import math
# XXX Yuck! 'from foo import *' is really bad!
from mathutils import *
from math import *
from bpy.props import *
from string import *
from struct import *
from math import *
from bpy.props import *

bpy.types.Scene.unrealbonesize = FloatProperty(
    name="Bone Length",
    description="Bone Length from head to tail distance",
    default=1, min=0.001, max=1000
)

#output log in to txt file
DEBUGLOG = False

scale = 1.0
bonesize = 1.0
from bpy_extras.io_utils import unpack_list, unpack_face_list

class md5_bone:
    bone_index = 0
    name = ""
    bindpos = []
    bindmat = []
    origmat = []
    head = []
    tail = []
    scale = []
    parent = ""
    parent_index = 0
    blenderbone = None
    roll = 0

    def __init__(self):
        self.bone_index = 0
        self.name = ""
        self.bindpos = [0.0] * 3
        self.scale = [0.0] * 3
        self.head = [0.0] * 3
        self.tail = [0.0] * 3
        self.bindmat = [None] * 3  # is this how you initilize a 2d-array
        for i in range(3):
            self.bindmat[i] = [0.0] * 3
        self.origmat = [None] * 3  #is this how you initilize a 2d-array
        for i in range(3):
            self.origmat[i] = [0.0] * 3
        self.parent = ""
        self.parent_index = 0
        self.blenderbone = None

    def dump(self):
        print ("bone index: ", self.bone_index)
        print ("name: ", self.name)
        print ("bind position: ", self.bindpos)
        print ("bind translation matrix: ", self.bindmat)
        print ("parent: ", self.parent)
        print ("parent index: ", self.parent_index)
        print ("blenderbone: ", self.blenderbone)

def getheadpos(pbone,bones):
    pos_head = [0.0] * 3

    #pos = mathutils.Vector((x,y,z)) * pbone.origmat
    pos = pbone.bindmat.to_translation()

    """
    tmp_bone = pbone
    while tmp_bone.name != tmp_bone.parent.name:
        pos = pos * tmp_bone.parent.bindmat
        tmp_bone = tmp_bone.parent
    """

    pos_head[0] = pos.x
    pos_head[1] = pos.y
    pos_head[2] = pos.z

    return pos_head

def gettailpos(pbone,bones):
    pos_tail = [0.0] * 3
    ischildfound = False
    childbone = None
    childbonelist = []
    for bone in bones:
        if bone.parent.name == pbone.name:
            ischildfound = True
            childbone = bone
            childbonelist.append(bone)

    if ischildfound:
        tmp_head = [0.0] * 3
        for bone in childbonelist:
            tmp_head[0] += bone.head[0]
            tmp_head[1] += bone.head[1]
            tmp_head[2] += bone.head[2]
        tmp_head[0] /= len(childbonelist)
        tmp_head[1] /= len(childbonelist)
        tmp_head[2] /= len(childbonelist)
        return tmp_head
    else:
        tmp_len = 0.0
        tmp_len += (pbone.head[0] - pbone.parent.head[0]) ** 2
        tmp_len += (pbone.head[1] - pbone.parent.head[1]) ** 2
        tmp_len += (pbone.head[2] - pbone.parent.head[2]) ** 2
        tmp_len = tmp_len ** 0.5 * 0.5
        pos_tail[0] = pbone.head[0] + tmp_len * pbone.bindmat[0][0]
        pos_tail[1] = pbone.head[1] + tmp_len * pbone.bindmat[1][0]
        pos_tail[2] = pbone.head[2] + tmp_len * pbone.bindmat[2][0]

    return pos_tail

def pskimport(infile,importmesh,importbone,bDebugLogPSK,importmultiuvtextures):
    global DEBUGLOG
    DEBUGLOG = bDebugLogPSK
    print ("--------------------------------------------------")
    print ("---------SCRIPT EXECUTING PYTHON IMPORTER---------")
    print ("--------------------------------------------------")
    print (" DEBUG Log:",bDebugLogPSK)
    print ("Importing file: ", infile)

    pskfile = open(infile,'rb')
    if (DEBUGLOG):
        logpath = infile.replace(".psk", ".txt")
        print("logpath:",logpath)
        logf = open(logpath,'w')

    def printlog(strdata):
        if (DEBUGLOG):
            logf.write(strdata)

    objName = infile.split('\\')[-1].split('.')[0]

    me_ob = bpy.data.meshes.new(objName)
    print("objName:",objName)
    printlog(("New Mesh = " + me_ob.name + "\n"))
    #read general header
    indata = unpack('20s3i', pskfile.read(32))
    #not using the general header at this time
    #==================================================================================================
    # vertex point
    #==================================================================================================
    #read the PNTS0000 header
    indata = unpack('20s3i', pskfile.read(32))
    recCount = indata[3]
    printlog(("Nbr of PNTS0000 records: " + str(recCount) + "\n"))
    counter = 0
    verts = []
    verts2 = []
    while counter < recCount:
        counter = counter + 1
        indata = unpack('3f', pskfile.read(12))
        #print(indata[0], indata[1], indata[2])
        verts.extend([(indata[0], indata[1], indata[2])])
        verts2.extend([(indata[0], indata[1], indata[2])])
        #print([(indata[0], indata[1], indata[2])])
        printlog(str(indata[0]) + "|" + str(indata[1]) + "|" + str(indata[2]) + "\n")
        #Tmsh.vertices.append(NMesh.Vert(indata[0], indata[1], indata[2]))

    #==================================================================================================
    # UV
    #==================================================================================================
    #read the VTXW0000 header
    indata = unpack('20s3i', pskfile.read(32))
    recCount = indata[3]
    printlog("Nbr of VTXW0000 records: " + str(recCount)+ "\n")
    counter = 0
    UVCoords = []
    #UVCoords record format = [index to PNTS, U coord, v coord]
    printlog("[index to PNTS, U coord, v coord]\n");
    while counter < recCount:
        counter = counter + 1
        indata = unpack('hhffhh', pskfile.read(16))
        UVCoords.append([indata[0], indata[2], indata[3]])
        printlog(str(indata[0]) + "|" + str(indata[2]) + "|" + str(indata[3]) + "\n")
        #print('mat index %i', indata(4))
        #print([indata[0], indata[2], indata[3]])
        #print([indata[1], indata[2], indata[3]])

    #==================================================================================================
    # Face
    #==================================================================================================
    #read the FACE0000 header
    indata = unpack('20s3i', pskfile.read(32))
    recCount = indata[3]
    printlog("Nbr of FACE0000 records: " + str(recCount) + "\n")
    #PSK FACE0000 fields: WdgIdx1|WdgIdx2|WdgIdx3|MatIdx|AuxMatIdx|SmthGrp
    #associate MatIdx to an image, associate SmthGrp to a material
    SGlist = []
    counter = 0
    faces = []
    faceuv = []
    facesmooth = []
    #the psk values are: nWdgIdx1|WdgIdx2|WdgIdx3|MatIdx|AuxMatIdx|SmthGrp
    printlog("nWdgIdx1|WdgIdx2|WdgIdx3|MatIdx|AuxMatIdx|SmthGrp \n")
    while counter < recCount:
        counter = counter + 1
        indata = unpack('hhhbbi', pskfile.read(12))
        printlog(str(indata[0]) + "|" + str(indata[1]) + "|" + str(indata[2]) + "|" + str(indata[3]) + "|" +
                 str(indata[4]) + "|" + str(indata[5]) + "\n")
        #indata[0] = index of UVCoords
        #UVCoords[indata[0]]=[index to PNTS, U coord, v coord]
        #UVCoords[indata[0]][0] = index to PNTS
        PNTSA = UVCoords[indata[2]][0]
        PNTSB = UVCoords[indata[1]][0]
        PNTSC = UVCoords[indata[0]][0]
        #print(PNTSA, PNTSB, PNTSC) #face id vertex
        #faces.extend([0, 1, 2, 0])
        faces.extend([(PNTSA, PNTSB, PNTSC, 0)])
        uv = []
        u0 = UVCoords[indata[2]][1]
        v0 = UVCoords[indata[2]][2]
        uv.append([u0, 1.0 - v0])
        u1 = UVCoords[indata[1]][1]
        v1 = UVCoords[indata[1]][2]
        uv.append([u1, 1.0 - v1])
        u2 = UVCoords[indata[0]][1]
        v2 = UVCoords[indata[0]][2]
        uv.append([u2, 1.0 - v2])
        faceuv.append([uv, indata[3], indata[4], indata[5]])

        #print("material:", indata[3])
        #print("UV: ", u0, v0)
        #update the uv var of the last item in the Tmsh.faces list
        # which is the face just added above
        ##Tmsh.faces[-1].uv = [(u0, v0), (u1, v1), (u2, v2)]
        #print("smooth:",indata[5])
        #collect a list of the smoothing groups
        facesmooth.append(indata[5])
        #print(indata[5])
        if SGlist.count(indata[5]) == 0:
            SGlist.append(indata[5])
            print("smooth:", indata[5])
        #assign a material index to the face
        #Tmsh.faces[-1].materialIndex = SGlist.index(indata[5])
    printlog("Using Materials to represent PSK Smoothing Groups...\n")
    #==========
    # skip something...
    #==========

    #==================================================================================================
    # Material
    #==================================================================================================
    ##
    #read the MATT0000 header
    indata = unpack('20s3i', pskfile.read(32))
    recCount = indata[3]
    printlog("Nbr of MATT0000 records: " +  str(recCount) + "\n" )
    printlog(" - Not importing any material data now. PSKs are texture wrapped! \n")
    counter = 0
    materialcount = 0
    while counter < recCount:
        counter = counter + 1
        indata = unpack('64s6i', pskfile.read(88))
        materialcount += 1
        print("Material", counter)
        print("Mat name %s", indata[0])

    ##
    #==================================================================================================
    # Bones (Armature)
    #==================================================================================================
    #read the REFSKEL0 header
    indata = unpack('20s3i', pskfile.read(32))
    recCount = indata[3]
    printlog( "Nbr of REFSKEL0 records: " + str(recCount) + "\n")
    #REFSKEL0 fields - Name|Flgs|NumChld|PrntIdx|Qw|Qx|Qy|Qz|LocX|LocY|LocZ|Lngth|XSize|YSize|ZSize

    Bns = []
    bone = []

    md5_bones = []
    bni_dict = {}
    #==================================================================================================
    # Bone Data
    #==================================================================================================
    counter = 0
    print ("---PRASE--BONES---")
    printlog("Name|Flgs|NumChld|PrntIdx|Qx|Qy|Qz|Qw|LocX|LocY|LocZ|Lngth|XSize|YSize|ZSize\n")
    while counter < recCount:
        indata = unpack('64s3i11f', pskfile.read(120))
        #print( "DATA",str(indata))

        bone.append(indata)

        createbone = md5_bone()
        #temp_name = indata[0][:30]
        temp_name = indata[0]
        temp_name = bytes.decode(temp_name)
        temp_name = temp_name.lstrip(" ")
        temp_name = temp_name.rstrip(" ")
        temp_name = temp_name.strip()
        temp_name = temp_name.strip( bytes.decode(b'\x00'))
        printlog(temp_name + "|" + str(indata[1]) + "|" + str(indata[2]) + "|" + str(indata[3]) + "|" +
                 str(indata[4]) + "|" + str(indata[5]) + "|" + str(indata[6]) + "|" + str(indata[7]) + "|" +
                 str(indata[8]) + "|" + str(indata[9]) + "|" + str(indata[10]) + "|" + str(indata[11]) + "|" +
                 str(indata[12]) + "|" + str(indata[13]) + "|" + str(indata[14]) + "\n")
        createbone.name = temp_name
        createbone.bone_index = counter
        createbone.parent_index = indata[3]
        createbone.bindpos[0] = indata[8]
        createbone.bindpos[1] = indata[9]
        createbone.bindpos[2] = indata[10]
        createbone.scale[0] = indata[12]
        createbone.scale[1] = indata[13]
        createbone.scale[2] = indata[14]

        bni_dict[createbone.name] = createbone.bone_index

        #w,x,y,z
        if (counter == 0):#main parent
             createbone.bindmat = mathutils.Quaternion((indata[7], -indata[4], -indata[5], -indata[6])).to_matrix()
             createbone.origmat = mathutils.Quaternion((indata[7], -indata[4], -indata[5], -indata[6])).to_matrix()
        else:
             createbone.bindmat = mathutils.Quaternion((indata[7], -indata[4], -indata[5], -indata[6])).to_matrix()
             createbone.origmat = mathutils.Quaternion((indata[7], -indata[4], -indata[5], -indata[6])).to_matrix()

        createbone.bindmat = mathutils.Matrix.Translation(mathutils.Vector((indata[8], indata[9], indata[10]))) * \
                             createbone.bindmat.to_4x4()

        md5_bones.append(createbone)
        counter = counter + 1
        bnstr = (str(indata[0]))
        Bns.append(bnstr)

    for pbone in md5_bones:
        pbone.parent = md5_bones[pbone.parent_index]

    for pbone in md5_bones:
        if pbone.name != pbone.parent.name:
            pbone.bindmat = pbone.parent.bindmat * pbone.bindmat
            #print(pbone.name)
            #print(pbone.bindmat)
            #print("end")
        else:
            pbone.bindmat = pbone.bindmat

    for pbone in md5_bones:
        pbone.head = getheadpos(pbone, md5_bones)

    for pbone in md5_bones:
        pbone.tail = gettailpos(pbone, md5_bones)

    for pbone in md5_bones:
        pbone.parent =  md5_bones[pbone.parent_index].name

    bonecount = 0
    for armbone in bone:
        temp_name = armbone[0][:30]
        #print ("BONE NAME: ", len(temp_name))
        temp_name=str((temp_name))
        #temp_name = temp_name[1]
        #print ("BONE NAME: ", temp_name)
        bonecount += 1
    print ("-------------------------")
    print ("----Creating--Armature---")
    print ("-------------------------")

    #================================================================================================
    #Check armature if exist if so create or update or remove all and addnew bone
    #================================================================================================
    #bpy.ops.object.mode_set(mode='OBJECT')
    meshname ="ArmObject"
    objectname = "armaturedata"
    # arm = None  # UNUSED
    if importbone:
        obj = bpy.data.objects.get(meshname)
        # arm = obj  # UNUSED

        if not obj:
            armdata = bpy.data.armatures.new(objectname)
            ob_new = bpy.data.objects.new(meshname, armdata)
            #ob_new = bpy.data.objects.new(meshname, 'ARMATURE')
            #ob_new.data = armdata
            bpy.context.scene.objects.link(ob_new)
            #bpy.ops.object.mode_set(mode='OBJECT')
            for i in bpy.context.scene.objects:
                i.select = False #deselect all objects
            ob_new.select = True
            #set current armature to edit the bone
            bpy.context.scene.objects.active = ob_new
            #set mode to able to edit the bone
            if bpy.ops.object.mode_set.poll():
                bpy.ops.object.mode_set(mode='EDIT')

            #newbone = ob_new.data.edit_bones.new('test')
            #newbone.tail.y = 1
            print("creating bone(s)")
            bpy.ops.object.mode_set(mode='OBJECT')
            for bone in md5_bones:
                #print(dir(bone))
                bpy.ops.object.mode_set(mode='EDIT')#Go to edit mode for the bones
                newbone = ob_new.data.edit_bones.new(bone.name)
                #parent the bone
                #print("DRI:", dir(newbone))
                parentbone = None
                #note bone location is set in the real space or global not local
                bonesize = bpy.types.Scene.unrealbonesize
                if bone.name != bone.parent:
                    pos_x = bone.bindpos[0]
                    pos_y = bone.bindpos[1]
                    pos_z = bone.bindpos[2]
                    #print("LINKING:" , bone.parent ,"j")
                    parentbone = ob_new.data.edit_bones[bone.parent]
                    newbone.parent = parentbone
                    rotmatrix = bone.bindmat
                    newbone.head.x = bone.head[0]
                    newbone.head.y = bone.head[1]
                    newbone.head.z = bone.head[2]
                    newbone.tail.x = bone.tail[0]
                    newbone.tail.y = bone.tail[1]
                    newbone.tail.z = bone.tail[2]

                    vecp = parentbone.tail - parentbone.head
                    vecc = newbone.tail - newbone.head
                    vecc.normalize()
                    vecp.normalize()
                    if vecp.dot(vecc) > -0.8:
                        newbone.roll = parentbone.roll
                    else:
                        newbone.roll = - parentbone.roll
                else:
                    rotmatrix = bone.bindmat
                    newbone.head.x = bone.head[0]
                    newbone.head.y = bone.head[1]
                    newbone.head.z = bone.head[2]
                    newbone.tail.x = bone.tail[0]
                    newbone.tail.y = bone.tail[1]
                    newbone.tail.z = bone.tail[2]
                    newbone.roll = math.radians(90.0)
                """
                vec = newbone.tail - newbone.head
                if vec.z > 0.0:
                    newbone.roll = math.radians(90.0)
                else:
                    newbone.roll = math.radians(-90.0)
                """
    bpy.context.scene.update()

    #==================================================================================================
    #END BONE DATA BUILD
    #==================================================================================================
    VtxCol = []
    for x in range(len(Bns)):
        #change the overall darkness of each material in a range between 0.1 and 0.9
        tmpVal = ((float(x) + 1.0) / (len(Bns)) * 0.7) + 0.1
        tmpVal = int(tmpVal * 256)
        tmpCol = [tmpVal, tmpVal, tmpVal, 0]
        #Change the color of each material slightly
        if x % 3 == 0:
            if tmpCol[0] < 128:
                tmpCol[0] += 60
            else:
                tmpCol[0] -= 60
        if x % 3 == 1:
            if tmpCol[1] < 128:
                tmpCol[1] += 60
            else:
                tmpCol[1] -= 60
        if x % 3 == 2:
            if tmpCol[2] < 128:
                tmpCol[2] += 60
            else:
                tmpCol[2] -= 60
        #Add the material to the mesh
        VtxCol.append(tmpCol)

    #==================================================================================================
    # Bone Weight
    #==================================================================================================
    #read the RAWW0000 header
    indata = unpack('20s3i', pskfile.read(32))
    recCount = indata[3]
    printlog("Nbr of RAWW0000 records: " + str(recCount) +"\n")
    #RAWW0000 fields: Weight|PntIdx|BoneIdx
    RWghts = []
    counter = 0
    while counter < recCount:
        counter = counter + 1
        indata = unpack('fii', pskfile.read(12))
        RWghts.append([indata[1], indata[2], indata[0]])
        #print("weight:", [indata[1], indata[2], indata[0]])
    #RWghts fields = PntIdx|BoneIdx|Weight
    RWghts.sort()
    printlog("Vertex point and groups count =" + str(len(RWghts)) + "\n")
    printlog("PntIdx|BoneIdx|Weight")
    for vg in RWghts:
        printlog(str(vg[0]) + "|" + str(vg[1]) + "|" + str(vg[2]) + "\n")

    #Tmsh.update_tag()

    #set the Vertex Colors of the faces
    #face.v[n] = RWghts[0]
    #RWghts[1] = index of VtxCol
    """
    for x in range(len(Tmsh.faces)):
        for y in range(len(Tmsh.faces[x].v)):
            #find v in RWghts[n][0]
            findVal = Tmsh.faces[x].v[y].index
            n = 0
            while findVal != RWghts[n][0]:
                n = n + 1
            TmpCol = VtxCol[RWghts[n][1]]
            #check if a vertex has more than one influence
            if n != len(RWghts) - 1:
                if RWghts[n][0] == RWghts[n + 1][0]:
                    #if there is more than one influence, use the one with the greater influence
                    #for simplicity only 2 influences are checked, 2nd and 3rd influences are usually very small
                    if RWghts[n][2] < RWghts[n + 1][2]:
                        TmpCol = VtxCol[RWghts[n + 1][1]]
        Tmsh.faces[x].col.append(NMesh.Col(TmpCol[0], TmpCol[1], TmpCol[2], 0))
    """
    if (DEBUGLOG):
        logf.close()
    #==================================================================================================
    #Building Mesh
    #==================================================================================================
    print("vertex:", len(verts), "faces:", len(faces))
    print("vertex2:", len(verts2))
    me_ob.vertices.add(len(verts2))
    me_ob.tessfaces.add(len(faces))
    me_ob.vertices.foreach_set("co", unpack_list(verts2))
    me_ob.tessfaces.foreach_set("vertices_raw", unpack_list( faces))

    for face in me_ob.tessfaces:
        face.use_smooth = facesmooth[face.index]

    """
    Material setup coding.
    First the mesh has to be create first to get the uv texture setup working.
    -Create material(s) list in the psk pack data from the list.(to do list)
    -Append the material to the from create the mesh object.
    -Create Texture(s)
    -face loop for uv assign and assign material index
    """
    bpy.ops.object.mode_set(mode='OBJECT')
    #===================================================================================================
    #Material Setup
    #===================================================================================================
    print ("-------------------------")
    print ("----Creating--Materials--")
    print ("-------------------------")
    materialname = "pskmat"
    materials = []

    for matcount in range(materialcount):
        #if texturedata is not None:
        matdata = bpy.data.materials.new(materialname + str(matcount))
        #mtex = matdata.texture_slots.new()
        #mtex.texture = texture[matcount].data
        #print(type(texture[matcount].data))
        #print(dir(mtex))
        #print(dir(matdata))
        #for texno in range(len( bpy.data.textures)):
            #print((bpy.data.textures[texno].name))
            #print(dir(bpy.data.textures[texno]))
        #matdata.active_texture = bpy.data.textures[matcount - 1]
        #matdata.texture_coords = 'UV'
        #matdata.active_texture = texturedata
        materials.append(matdata)

    for material in materials:
        #add material to the mesh list of materials
        me_ob.materials.append(material)
    #===================================================================================================
    #UV Setup
    #===================================================================================================
    print ("-------------------------")
    print ("-- Creating UV Texture --")
    print ("-------------------------")
    texture = []
    # texturename = "text1"  # UNUSED
    countm = 0
    for countm in range(materialcount):
        psktexname = "psk" + str(countm)
        me_ob.uv_textures.new(name=psktexname)
        countm += 1
    print("INIT UV TEXTURE...")
    _matcount = 0
    #for mattexcount in materials:
        #print("MATERAIL ID:", _matcount)
    _textcount = 0
    for uv in me_ob.tessface_uv_textures: # uv texture
        print("UV TEXTURE ID:",_textcount)
        print(dir(uv))
        for face in me_ob.tessfaces:# face, uv
            #print(dir(face))
            if faceuv[face.index][1] == _textcount: #if face index and texture index matches assign it
                mfaceuv = faceuv[face.index] #face index
                _uv1 = mfaceuv[0][0] #(0,0)
                uv.data[face.index].uv1 = mathutils.Vector((_uv1[0], _uv1[1])) #set them
                _uv2 = mfaceuv[0][1] #(0,0)
                uv.data[face.index].uv2 = mathutils.Vector((_uv2[0], _uv2[1])) #set them
                _uv3 = mfaceuv[0][2] #(0,0)
                uv.data[face.index].uv3 = mathutils.Vector((_uv3[0], _uv3[1])) #set them
            else: #if not match zero them
                uv.data[face.index].uv1 = mathutils.Vector((0, 0)) #zero them
                uv.data[face.index].uv2 = mathutils.Vector((0, 0)) #zero them
                uv.data[face.index].uv3 = mathutils.Vector((0, 0)) #zero them
        _textcount += 1
        #_matcount += 1
        #print(matcount)
    print("END UV TEXTURE...")

    print("UV TEXTURE LEN:", len(texture))
        #for tex in me_ob.uv_textures:
            #print("mesh tex:", dir(tex))
            #print((tex.name))

    #for face in me_ob.faces:
        #print(dir(face))

    #===================================================================================================
    #
    #===================================================================================================
    obmesh = bpy.data.objects.new(objName,me_ob)
    #===================================================================================================
    #Mesh Vertex Group bone weight
    #===================================================================================================
    print("---- building bone weight mesh ----")
    #print(dir(ob_new.data.bones))
    #create bone vertex group #deal with bone id for index number
    for bone in ob_new.data.bones:
        #print("names:", bone.name, ":", dir(bone))
        #print("names:", bone.name)
        group = obmesh.vertex_groups.new(bone.name)

    for vgroup in obmesh.vertex_groups:
        #print(vgroup.name, ":", vgroup.index)
        for vgp in RWghts:
            #bone index
            if vgp[1] == bni_dict[vgroup.name]:
                #print(vgp)
                #[vertex id],weight
                vgroup.add([vgp[0]], vgp[2], 'ADD')

    #check if there is a material to set to
    if len(materials) > 0:
        obmesh.active_material = materials[0] #material setup tmp
    print("---- adding mesh to the scene ----")

    bpy.ops.object.mode_set(mode='OBJECT')
    #bpy.ops.object.select_pattern(extend=True, pattern=obmesh.name, case_sensitive=True)
    #bpy.ops.object.select_pattern(extend=True, pattern=ob_new.name, case_sensitive=True)

    #bpy.ops.object.select_name(name=str(obmesh.name))
    #bpy.ops.object.select_name(name=str(ob_new.name))
    #bpy.context.scene.objects.active = ob_new
    me_ob.update()
    bpy.context.scene.objects.link(obmesh)
    bpy.context.scene.update()
    obmesh.select = False
    ob_new.select = False
    obmesh.select = True
    ob_new.select = True
    bpy.ops.object.parent_set(type="ARMATURE")

    print ("PSK2Blender completed")
#End of def pskimport#########################

def getInputFilenamepsk(self, filename, importmesh, importbone, bDebugLogPSK, importmultiuvtextures):
    checktype = filename.split('\\')[-1].split('.')[1]
    print ("------------",filename)
    if checktype.lower() != 'psk':
        print ("  Selected file = ", filename)
        raise (IOError, "The selected input file is not a *.psk file")
        #self.report({'INFO'}, ("Selected file:"+ filename))
    else:
        pskimport(filename, importmesh, importbone, bDebugLogPSK, importmultiuvtextures)

def getInputFilenamepsa(self, filename, context):
    checktype = filename.split('\\')[-1].split('.')[1]
    if checktype.lower() != 'psa':
        print ("  Selected file = ", filename)
        raise (IOError, "The selected input file is not a *.psa file")
        #self.report({'INFO'}, ("Selected file:" + filename))
    else:
        psaimport(filename,context)

class IMPORT_OT_psk(bpy.types.Operator):
    '''Load a skeleton mesh psk File'''
    bl_idname = "import_scene.psk"
    bl_label = "Import PSK"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_options = {'UNDO'}

    # List of operator properties, the attributes will be assigned
    # to the class instance from the operator settings before calling.
    filepath = StringProperty(
            subtype='FILE_PATH',
            )
    filter_glob = StringProperty(
            default="*.psk",
            options={'HIDDEN'},
            )
    importmesh = BoolProperty(
            name="Mesh",
            description="Import mesh only. (not yet build.)",
            default=True,
            )
    importbone = BoolProperty(
            name="Bones",
            description="Import bones only. Current not working yet",
            default=True,
            )
    importmultiuvtextures = BoolProperty(
            name="Single UV Texture(s)",
            description="Single or Multi uv textures",
            default=True,
            )
    bDebugLogPSK = BoolProperty(
            name="Debug Log.txt",
            description="Log the output of raw format. It will save in "
                        "current file dir. Note this just for testing",
            default=False,
            )
    unrealbonesize = FloatProperty(
            name="Bone Length",
            description="Bone Length from head to tail distance",
            default=1,
            min=0.001,
            max=1000,
            )

    def execute(self, context):
        bpy.types.Scene.unrealbonesize = self.unrealbonesize
        getInputFilenamepsk(self, self.filepath, self.importmesh, self.importbone, self.bDebugLogPSK,
                            self.importmultiuvtextures)
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}

class psa_bone:
    name=""
    Transform=None
    parent=None
    def __init__(self):
        self.name=""
        self.Transform=None
        self.parent=None

def psaimport(filename,context):
    print ("--------------------------------------------------")
    print ("---------SCRIPT EXECUTING PYTHON IMPORTER---------")
    print ("--------------------------------------------------")
    print ("Importing file: ", filename)
    psafile = open(filename,'rb')
    debug = True
    if (debug):
        logpath = filename.replace(".psa", ".txt")
        print("logpath:", logpath)
        logf = open(logpath, 'w')
    def printlog(strdata):
        if (debug):
            logf.write(strdata)
    def printlogplus(name, data):
        if (debug):
            logf.write(str(name) + '\n')
            if isinstance(data, bytes):
                logf.write(str(bytes.decode(data).strip(bytes.decode(b'\x00'))))
            else:
                logf.write(str(data))
            logf.write('\n')

    printlog('-----------Log File------------\n')
    #General Header
    indata = unpack('20s3i', psafile.read(32))
    printlogplus('ChunkID', indata[0])
    printlogplus('TypeFlag', indata[1])
    printlogplus('DataSize', indata[2])
    printlogplus('DataCount', indata[3])
    #Bones Header
    indata = unpack('20s3i', psafile.read(32))
    printlogplus('ChunkID', indata[0])
    printlogplus('TypeFlag', indata[1])
    printlogplus('DataSize', indata[2])
    printlogplus('DataCount', indata[3])
    #Bones Data
    BoneIndex2NamePairMap = {}
    BoneNotFoundList = []
    printlog("Name|Flgs|NumChld|PrntIdx|Qx|Qy|Qz|Qw|LocX|LocY|LocZ|Length|XSize|YSize|ZSize\n")
    recCount = indata[3]
    counter = 0
    nobonematch = True
    while counter < recCount:
        indata = unpack('64s3i11f', psafile.read(120))
        #printlogplus('bone', indata[0])
        bonename = str(bytes.decode(indata[0]).strip(bytes.decode(b'\x00')))
        if bonename in bpy.data.armatures['armaturedata'].bones.keys():
            BoneIndex2NamePairMap[counter] = bonename
            print('find bone', bonename)
            nobonematch = False
        else:
            print('can not find the bone:', bonename)
            BoneNotFoundList.append(counter)
        counter += 1

    if nobonematch:
        print('no bone was match so skip import!')
        return

    #Animations Header
    indata = unpack('20s3i', psafile.read(32))
    printlogplus('ChunkID', indata[0])
    printlogplus('TypeFlag', indata[1])
    printlogplus('DataSize', indata[2])
    printlogplus('DataCount', indata[3])
    #Animations Data
    recCount = indata[3]
    counter = 0
    Raw_Key_Nums = 0
    Action_List = []
    while counter < recCount:
        indata = unpack('64s64s4i3f3i', psafile.read(64 + 64 + 4 * 4 + 3 * 4 + 3 * 4))
        printlogplus('Name', indata[0])
        printlogplus('Group', indata[1])
        printlogplus('totalbones', indata[2])
        printlogplus('NumRawFrames', indata[-1])
        Name = str(bytes.decode(indata[0]).strip(bytes.decode(b'\x00')))
        Group = str(bytes.decode(indata[1]).strip(bytes.decode(b'\x00')))
        totalbones = indata[2]
        NumRawFrames = indata[-1]

        Raw_Key_Nums += indata[2] * indata[-1]
        Action_List.append((Name,Group,totalbones,NumRawFrames))

        counter += 1

    #Raw keys Header
    Raw_Key_List = []
    indata = unpack('20s3i', psafile.read(32))
    printlogplus('ChunkID', indata[0])
    printlogplus('TypeFlag', indata[1])
    printlogplus('DataSize', indata[2])
    printlogplus('DataCount', indata[3])
    if(Raw_Key_Nums != indata[3]):
        print('error! Raw_Key_Nums Inconsistent')
        return
    #Raw keys Data
    recCount = Raw_Key_Nums
    counter = 0
    while counter < recCount:
        indata = unpack('3f4f1f', psafile.read(3 * 4 + 4 * 4 + 4))
        pos = mathutils.Vector((indata[0], indata[1], indata[2]))
        quat = mathutils.Quaternion((indata[6], indata[3], indata[4], indata[5]))
        time = indata[7]
        Raw_Key_List.append((pos, quat, time))
        counter += 1
    #Scale keys Header,Scale keys Data,Curve keys Header,Curve keys Data
    curFilePos = psafile.tell()
    psafile.seek(0, 2)
    endFilePos = psafile.tell()
    if curFilePos == endFilePos:
        print('no Scale keys,Curve keys')

    #build the animation line
    if bpy.ops.object.mode_set.poll():
        bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

    NeededBoneMatrix = {}
    ARMATURE_OBJ = 'ArmObject'
    ARMATURE_DATA = 'armaturedata'
    if bpy.context.scene.udk_importarmatureselect:
        if len(bpy.context.scene.udkas_list) > 0:
            print("CHECKING ARMATURE...")
            #for bone in bpy.data.objects[ARMATURE_OBJ].pose.bones:
            #for objd in bpy.data.objects:
                #print("NAME:", objd.name, " TYPE:", objd.type)
                #if objd.type == 'ARMARURE':
                    #print(dir(objd))
            armature_list = bpy.context.scene.udkas_list #armature list array
            armature_idx = bpy.context.scene.udkimportarmature_list_idx #armature index selected
            ARMATURE_OBJ = bpy.data.objects[armature_list[armature_idx]].name #object armature
            ARMATURE_DATA = bpy.data.objects[armature_list[armature_idx]].data.name #object data

    for bone in bpy.data.armatures[ARMATURE_DATA].bones:
        name = bone.name
        ori_matrix = bone.matrix
        matrix = bone.matrix_local.to_3x3()
        bone_rest_matrix = Matrix(matrix)
        #bone_rest_matrix = bone.matrix_local.to_3x3()
        #bone_rest_matrix = bone.matrix_local.to_quaternion().conjugated().to_matrix()
        bone_rest_matrix_inv = Matrix(bone_rest_matrix)
        bone_rest_matrix_inv.invert()
        bone_rest_matrix_inv.resize_4x4()
        bone_rest_matrix.resize_4x4()
        NeededBoneMatrix[name] = (bone_rest_matrix,bone_rest_matrix_inv,ori_matrix)

    #build tmp pose bone tree
    psa_bones = {}
    for bone in bpy.data.objects[ARMATURE_OBJ].pose.bones:
        _psa_bone = psa_bone()
        _psa_bone.name = bone.name
        _psa_bone.Transform = bone.matrix
        if bone.parent is not None:
            _psa_bone.parent = psa_bones[bone.parent.name]
        else:
            _psa_bone.parent = None
        psa_bones[bone.name] = _psa_bone

    raw_key_index = 0

    for raw_action in Action_List:
        Name = raw_action[0]
        Group = raw_action[1]
        Totalbones = raw_action[2]
        NumRawFrames = raw_action[3]
        context.scene.update()
        object = bpy.data.objects['ArmObject']
        object.animation_data_create()
        action = bpy.data.actions.new(name=Name)
        object.animation_data.action = action
        for i in range(NumRawFrames):
            context.scene.frame_set(i + 1)
            pose_bones = object.pose.bones
            for j in range(Totalbones):
                if j not in BoneNotFoundList:
                    bName = BoneIndex2NamePairMap[j]
                    pbone = psa_bones[bName]
                    pos = Raw_Key_List[raw_key_index][0]
                    quat = Raw_Key_List[raw_key_index][1]

                    mat = Matrix()
                    if pbone.parent is not None:
                        quat = quat.conjugated()
                        mat = Matrix.Translation(pos) * quat.to_matrix().to_4x4()
                        mat = pose_bones[bName].parent.matrix * mat
                        #mat = pbone.parent.Transform * mat
                    else:
                        mat = pbone.Transform * Matrix.Translation(pos) * quat.to_matrix().to_4x4()

                    pose_bones[bName].matrix = mat
                    pbone.Transform = mat

                raw_key_index += 1

            #bpy.data.meshes[1]
            for bone in pose_bones:
                bone.matrix = psa_bones[bone.name].Transform
                bone.keyframe_insert("rotation_quaternion")
                bone.keyframe_insert("location")

            def whirlSingleBone(pose_bone,quat):
                bpy.context.scene.update()
                #record child's matrix and origin rotate
                hymat = Quaternion((0.707, -0.707, 0, 0)).inverted().to_matrix().to_4x4()
                children_infos = {}
                childrens = pose_bone.children
                for child in childrens:
                    armmat = bpy.data.armatures['armaturedata'].bones[child.name].matrix.copy().to_4x4()
                    cmat = child.matrix.copy() * armmat.inverted() * hymat.inverted()
                    pos = cmat.to_translation()
                    rotmat = cmat.to_3x3()
                    children_infos[child] = (armmat, pos, rotmat)

                #whirl this bone by quat
                pose_bone.matrix *= quat.to_matrix().to_4x4()
                pose_bone.keyframe_insert("location")
                pose_bone.keyframe_insert("rotation_quaternion")
                bpy.context.scene.update()
                #set back children bon to original position
                #reverse whirl child bone by quat.inverse()

                for child in childrens:
                    armmat = children_infos[child][0]
                    pos = children_infos[child][1]
                    rotmat = children_infos[child][2]

                    child.matrix = Matrix.Translation(pos) * rotmat.to_4x4() * hymat * armmat
                    child.keyframe_insert("location")
                    child.keyframe_insert("rotation_quaternion")

            for bone in pose_bones:
                if bone.parent is not None:
                    whirlSingleBone(bone,Quaternion((0.707, 0, 0, -0.707)))
                else:
                    bone.rotation_quaternion *= Quaternion((0.707, -0.707, 0, 0)) * Quaternion((0.707, 0, 0, -0.707))
                    bone.keyframe_insert("rotation_quaternion")

        break

    context.scene.frame_set(0)
    if(debug):
        logf.close()

class IMPORT_OT_psa(bpy.types.Operator):
    '''Load a skeleton anim psa File'''
    bl_idname = "import_scene.psa"
    bl_label = "Import PSA"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"

    filepath = StringProperty(
            subtype='FILE_PATH',
            )
    filter_glob = StringProperty(
            default="*.psa",
            options={'HIDDEN'},
            )

    def execute(self, context):
        getInputFilenamepsa(self,self.filepath,context)
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}

class IMPORT_OT_psa(bpy.types.Operator):
    '''Load a skeleton anim psa File'''
    bl_idname = "import_scene.psa"
    bl_label = "Import PSA"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"

    filepath = StringProperty(
            subtype='FILE_PATH',
            )
    filter_glob = StringProperty(
            default="*.psa",
            options={'HIDDEN'},
            )

    def execute(self, context):
        getInputFilenamepsa(self,self.filepath,context)
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}

bpy.types.Scene.udk_importpsk = StringProperty(
        name = "Import .psk",
        description = "Skeleton mesh file path for psk",
        default = "")
bpy.types.Scene.udk_importpsa = StringProperty(
        name = "Import .psa",
        description = "Animation Data to Action Set(s) file path for psa",
        default = "")
bpy.types.Scene.udk_importarmatureselect = BoolProperty(
        name = "Armature Selected",
        description = "Select Armature to Import psa animation data",
        default = False)

class Panel_UDKImport(bpy.types.Panel):
    bl_label = "UDK Import"
    bl_idname = "OBJECT_PT_udk_import"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_category = "File I/O"
    bl_context = "objectmode"

    filepath = StringProperty(
            subtype='FILE_PATH',
            )

    #@classmethod
    #def poll(cls, context):
    #   return context.active_object

    def draw(self, context):
        layout = self.layout
        layout.operator(OBJECT_OT_PSKPath.bl_idname)

        layout.prop(context.scene, "udk_importarmatureselect")
        if bpy.context.scene.udk_importarmatureselect:
            layout.operator(OBJECT_OT_UDKImportArmature.bl_idname)
            layout.template_list("UI_UL_list", "udkimportarmature_list", context.scene, "udkimportarmature_list",
                                 context.scene, "udkimportarmature_list_idx", rows=5)
        layout.operator(OBJECT_OT_PSAPath.bl_idname)

class OBJECT_OT_PSKPath(bpy.types.Operator):
    """Select .psk file path to import for skeleton mesh"""
    bl_idname = "object.pskpath"
    bl_label = "Import PSK Path"

    filepath = StringProperty(
            subtype='FILE_PATH',
            )
    filter_glob = StringProperty(
            default="*.psk",
            options={'HIDDEN'},
            )
    importmesh = BoolProperty(
            name="Mesh",
            description="Import mesh only. (not yet build.)",
            default=True,
            )
    importbone = BoolProperty(
            name="Bones",
            description="Import bones only. Current not working yet",
            default=True,
            )
    importmultiuvtextures = BoolProperty(
            name="Single UV Texture(s)",
            description="Single or Multi uv textures",
            default=True,
            )
    bDebugLogPSK = BoolProperty(
            name="Debug Log.txt",
            description="Log the output of raw format. It will save in " \
                        "current file dir. Note this just for testing",
            default=False,
            )
    unrealbonesize = FloatProperty(
            name="Bone Length",
            description="Bone Length from head to tail distance",
            default=1,
            min=0.001,
            max=1000,
            )

    def execute(self, context):
        #context.scene.importpskpath = self.properties.filepath
        bpy.types.Scene.unrealbonesize = self.unrealbonesize
        getInputFilenamepsk(self, self.filepath, self.importmesh, self.importbone, self.bDebugLogPSK,
                            self.importmultiuvtextures)
        return {'FINISHED'}

    def invoke(self, context, event):
        #bpy.context.window_manager.fileselect_add(self)
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}

class UDKImportArmaturePG(bpy.types.PropertyGroup):
    #boolean = BoolProperty(default=False)
    string = StringProperty()
    bexport = BoolProperty(default=False, name="Export", options={"HIDDEN"},
                           description = "This will be ignore when exported")
    bselect = BoolProperty(default=False, name="Select", options={"HIDDEN"},
                           description = "This will be ignore when exported")
    otype = StringProperty(name="Type",description = "This will be ignore when exported")

bpy.utils.register_class(UDKImportArmaturePG)
bpy.types.Scene.udkimportarmature_list = CollectionProperty(type=UDKImportArmaturePG)
bpy.types.Scene.udkimportarmature_list_idx = IntProperty()

class OBJECT_OT_PSAPath(bpy.types.Operator):
    """Select .psa file path to import for animation data"""
    bl_idname = "object.psapath"
    bl_label = "Import PSA Path"

    filepath = StringProperty(name="PSA File Path", description="Filepath used for importing the PSA file",
                              maxlen=1024, default="")
    filter_glob = StringProperty(
            default="*.psa",
            options={'HIDDEN'},
            )

    def execute(self, context):
        #context.scene.importpsapath = self.properties.filepath
        getInputFilenamepsa(self,self.filepath,context)
        return {'FINISHED'}

    def invoke(self, context, event):
        bpy.context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

class OBJECT_OT_UDKImportArmature(bpy.types.Operator):
    """This will update the filter of the mesh and armature"""
    bl_idname = "object.udkimportarmature"
    bl_label = "Update Armature"

    def execute(self, context):
        my_objlist = bpy.context.scene.udkimportarmature_list
        objectl = []
        for objarm in bpy.context.scene.objects:#list and filter only mesh and armature
            if objarm.type == 'ARMATURE':
                objectl.append(objarm)
        for _objd in objectl:#check if list has in udk list
            bfound_obj = False
            for _obj in my_objlist:
                if _obj.name == _objd.name and _obj.otype == _objd.type:
                    _obj.bselect = _objd.select
                    bfound_obj = True
                    break
            if bfound_obj == False:
                #print("ADD ARMATURE...")
                my_item = my_objlist.add()
                my_item.name = _objd.name
                my_item.bselect = _objd.select
                my_item.otype = _objd.type
        removeobject = []
        for _udkobj in my_objlist:
            bfound_objv = False
            for _objd in bpy.context.scene.objects: #check if there no existing object from sense to remove it
                if _udkobj.name == _objd.name and _udkobj.otype == _objd.type:
                    bfound_objv = True
                    break
            if bfound_objv == False:
                removeobject.append(_udkobj)
        #print("remove check...")
        for _item in removeobject: #loop remove object from udk list object
            count = 0
            for _obj in my_objlist:
                if _obj.name == _item.name and _obj.otype == _item.otype:
                    my_objlist.remove(count)
                    break
                count += 1
        return{'FINISHED'}

class OBJECT_OT_UDKImportA(bpy.types.Operator):
    """This will update the filter of the mesh and armature"""
    bl_idname = "object.udkimporta"
    bl_label = "Update Armature"

    def execute(self, context):
        for objd in bpy.data.objects:
            print("NAME:",objd.name," TYPE:",objd.type)
            if objd.type == "ARMATURE":
                print(dir(objd))
                print((objd.data.name))
        return{'FINISHED'}

def menu_func(self, context):
    self.layout.operator(IMPORT_OT_psk.bl_idname, text="Skeleton Mesh (.psk)")
    self.layout.operator(IMPORT_OT_psa.bl_idname, text="Skeleton Anim (.psa)")

def register():
    bpy.utils.register_module(__name__)
    bpy.types.INFO_MT_file_import.append(menu_func)

def unregister():
    bpy.utils.unregister_module(__name__)
    bpy.types.INFO_MT_file_import.remove(menu_func)

if __name__ == "__main__":
    register()

#note this only read the data and will not be place in the scene
#getInputFilename('C:\\blenderfiles\\BotA.psk')
#getInputFilename('C:\\blenderfiles\\AA.PSK')
