#!BPY
# coding: utf-8
""" Registration info for Blender menus:
Name: 'M3G (.m3g, .java)...'
Blender: 244
Group: 'Export'
Tooltip: 'Export to M3G'
"""
#------------------------------------------------------------------------
# M3G exporter for blender 2.37 or above
#
# Source: http://www.nelson-games.de/bl2m3g/source
#
# $Id: m3g_export.py,v 0.1 2005/04/19 12:25 gerhardv Exp gerhardv $
#
# Author: Gerhard Völkl
#
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2005: gerhard völkl gkvoelkl@yahoo.de
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

# ***** END GPL LICENCE BLOCK *****
#
# To use script:
# 1.) load this file in the text window.
#     (press SHIFT+F11, Open New via Datablock button)
# 2.) make sure your mouse is over the text edit window and
#     run this script. (press ALT+P)
# Or:
#   copy to the scripts directory and it will appear in the
#   export list. (Needs 2.32 or higher)
#
# Based on informations from:
#   wrl2export.py from Rick Kimball and others
# --------------------------------------------------------------------------#
# History 0.2
# * maximal Precision in VertexArray (with algorithms from Kalle Raita)
# * IPO Animation with mesh: Rotation, Translation and Size
# History 0.3
# * to find a 3d object in your java programm you can assign a userID
#   your blender object has name 'cube#01' your 3d object will have ID 01
#   the number after '#' is taken
# * more than one material per mesh can be used
# * uv texture support (implemented by Aki Koskinen and Juha Laitinen)
#   The image which is bound to the faces will be exportet within m3g-file
#   Limitations by M3G-API:
#   The width and height of the image must be non-negative powers of two, 
#   but they need not to be equal. Maximum value is 256.
#   *.java export: Only PNG images can be used.
# History 0.4
# * check limitation of texture images (credit to MASTER_ZION for Brasil)
# * Better light: The light modeles of Blender and M3G are naturally 
#   different. So the export script trys to translate as much as possible
#
#   M3G Light type          Blender Light type
#   --------------------------------------------------------------
#   AMIENT Light            Not available as light type in Blender
#   DIRECTIONAL Light       SUN
#   OMNIdirectional light   LAMP
#   SPOT light              SPOT
#   not translated          HEMI
#   not translated          AREA
#
#   Attributs of M3G Lights:
#
#   Attenuation (OMNI,SPOT):
#     Intensity of light changes with distance
#     The attenuation factor is 1 / (c + l d + q d2)
#     where d is the distance between the light and the vertex being lighted
#     and c, l, q are the constant, linear, and quadratic coefficients.
#     In Blender exists much complex posibilies. To simplify exporter uses
#     only button Dist: distance at which the light intensity is half 
#     the Energy 
#   Color (ALL)
#     Color of light 
#   Intensity (ALL)
#     The RGB color of this Light is multiplied component-wise with the 
#     intensity. In Blender : energy
#   SpotAngle (SPOT)
#     the spot cone angle for this Light
#     In Blender: spotSize
#   SpotExponent (SPOT)
#     The spot exponent controls the distribution of the intensity of 
#     this Light within the spot cone, such that larger values yield 
#     a more concentrated cone. In Blender: SpotBl 
#
# * Some GUI for options
#   First prototype of GUI was created using RipSting's Blender-Python 
#   GUI designer. Download at Http://oregonstate.edu/~dennisa/Blender/BPG/
#
# * Ambiente light
#   Information is taken by world ambiente attribute
#
# * Parenting Part 1
#   In Blender the Empty object is used to group objects. All objects
#   which have the same empty as parent are the member of the same group.
#   
#   empty <-- Parent of -- element 1
#        <-- Parent of -- element 2
#
#       is translated in M3G
#
#   group-Node -- Member --> element 1
#              -- Member --> element 2
#
#   In Blender every object can be the parent of every other object
#   In M3G that is not possible. Only a group object can be parent.
#   (Or the world object which is derived from group).
#   That will come later as Parenting Part 2
#
# * Backface Culling
#   you can use backface culling, if option "use backface culloing" is on.
#   Culling will be set in PolygonMode object of every mesh. The correct
#   winding is controlled.
# History 0.5
#* Bone Animation - Armature (Part 1)
#
#  Armature is the skeleton for skinned meshes. It stores the bones in 
#  rest position (more information http://www.blender.org/cms/How_Armatures_work.634.0.html)
#  You can work in Blender with bones and meshes in different ways. In
#  this first attempt only the use of vertex groups is assisted.
#
#  Blender-Objekts       translated into      M3G-Objects
#
#      MESH                                  SkinnedMesh
#        |                                       |
#        v                                       v
#     ARMATURE                                 Group
#        |                                       |
#        v                                       v
#      BONE_1                                  Group
#                                              Group_second
#        |                                       |
#        V                                       v
#      BONE_2                                  Group
#                                              Group_secound
#
#  Every bone is translated into two groups at the moment, because
#  the second bone is needed to do the animation in an easy way.
#
#  The animations in Blender for meshes are stored in action objects.
# 
#  Blender Objects      translated into      M3G-Objects
#
#   ARMATURE
#       | activ
#       v
#    ACTION                               ANIMATIONCONTROLLER
#       | 1..n                                    ^
#       v                                 ANIMATIONTRACK  --> Group_second
#     IPOs                                        |
#                                                 v
#                                            KEYSEQUENZE
#
#  One action is translated into one animationcontroller. One IPO is 
#  translated in one KEYSEQUENZE and one ANIMATIONTRACK.
#
#  At the moment only the active action of the armature object is translated.
#
#* Print Info, if type of light is used that is not supported
#
# History 0.5
#
#* New Option exportAllAction (default value: false)
#  If that option is true, all actions will be exported - not only the active
#  action. 
#  At the moment you can only assign one action to one armature.
#  To know which action is used with which armature the action
#  needs a special name : 
#        <Action Name>#A<M3G ID of Armature>E<End Frame>#<ID of Action>

#  Example: Name of action : walk#A10E250#02
#           Name of armature : man#10
#           End Frame: 250
# 
# History 0.6
# Include the same image only one time into the m3g-file
#
# All the following changes of this version was made by Claus Hoefele
#
#* Until now all vertices of the faces was been written.
#  Now the vertices will be used again if possible: 
#     normal and texture coordinates of to vertices have to be the same
#
#* Smooth/solid shading can now be defined for every single material:
#     in Editing panel (F9)>Link and Materials
#
#* This script uses now correctly the TexFace and Shadless Buttons in
#  Shading panel (F5)>Material buttons>Material box. 
#  TexFace switches on/off the Export of texture coordinates.
#  Shadeless does the some with the normal coordinates
#
#* The GUI was redesigned in a PupBlock
#  
#* Options:
#
#** Texturing Enabled: Switches on/off export of textures and texture 
#           coordinates. Attention: the TextFace button switches only
#           for one mesh
#** Texturing External: the textures will be included it mg3-file or
#           exported in seperate file
#** Lighting Enabled: turns on/off export of lights and normal completly
#           Attention: Shadeless only for one mesh
#** Persp. Correction: turns on/off perspective correction in PolygonMode.
#** Smooth Shading: turns on/off smooth shading in PolygonMode.
#
#* Textures in external references are used again (with ImageFactory)
#
#* Blender function: Double Sided button in Editing Context
#                    (F9)>Mesh panel)
#  turn on/off PolygonMode.CULL_BACK anzuschalten.
#
#* Script ingnores meshes that have no faces
#
# History 0.7
#
# * Exporter can work with texture coordinates greater 1 and smaller 0
#
# * Adler32 did not work always correct. New implementation made.
#
# * Modul shutil is not needed any longer. Exporter has its own copy_file.
#   (realized and inspired by ideasman_42 and Martin Neumann)
#
# History 0.8
#
# * Blender works with SpotAngles 1..180 but M3G works only with 0..90
#   M3G use the 'half angle' (cut off angle) (Thanks to Martin Storsjö)
#
# * Error fixed: Texture coordinates was not calculated correct.
#   (Thanks to Milan Piskla, Vlad, Max Gilead, Regis Cosnier ...)
#
# * New options in GUI:
#      M3G Version 2.0 : Will export M3G files Vers. 2.0 in future
#      Game Physics: Adds Game Physics infos for NOPE API
#   
# --------------------------------------------------------------------------#
# TODO: Export only selected mesh
# TODO: Optimize Bones <--> Vertex Group mapping
# TODO: Compressed File
# TODO: MTex - Support
# TODO: By Rotating use SQUAD instead of Beziere. It's smoother
import Blender
from Blender import Types,Lamp,Material,Texture,Window,Registry,Draw
from Blender.BGL import *
from Blender.Object import *
from Blender.Camera import *
from Blender.Mesh import *
from array import array
import sys, struct, zlib
from inspect import *
from types import *
from Blender.Mathutils import *
from os.path import *
#import rpdb2

# ---- Helper Functions -------------------------------------------------------#
def copy_file(source, dest):
	file = open(source, 'rb')
	data = file.read()
	file.close()
	
	file = open(dest, 'wb')
	file.write(data)
	file.close()

def tracer(frame, event, arg):
    '''Global trace function'''
    if event=='call':
        tmp = getargvalues(frame)
        print event, frame.f_code.co_name, frame.f_lineno, \
                     formatargvalues(tmp[0],tmp[1],tmp[2],tmp[3])
    elif event=='line':
        print event, frame.f_code.co_name, frame.f_lineno
        #print event, frame.f_code.co_name, frame.f_lineno, \
        #             getsourcelines(frame.f_code)[frame.f_lineno]
    elif event=='return':
        print event, frame.f_code.co_name, frame.f_lineno, "->", arg
    return tracer

def doSearchDeep(inList,outList):
    '''Does deepsearch for all elements in inList'''
    for element in inList:
        if element != None : outList = element.searchDeep(outList)
    return outList


def getId(aObject):
    ''' returns 0 if Object is None: M3G value for null'''
    if aObject == None: return 0
    return aObject.id
 
def toJavaBoolean(aValue):
    ''' returns java equivalent to boolean'''
    if aValue:
        return 'true'
    else :
        return 'false'

def sign(a):
    if a<0 : return -1
    elif a>0 : return 1
    else : return 0
     
def isOrderClockWise(v,normal):
    ''' returns true, if order of vertices is clockwise. Important for 
        culling '''
    # (v2-v0)x(v2-v1)=surface_normal
    #
    if type(v[0]) is Types.MVertType:
        mNormal = TriangleNormal(Vector(v[0].co),Vector(v[1].co),Vector(v[2].co))
    else:
        mNormal = TriangleNormal(Vector(v[0]),Vectot(v[1]),Vector(v[2]))
    #print "normal ",mNormal.normalize()
    #print "BNormal ",normal.normalize()
    
    # Do not use any longer. Blender does it correct
    
    result = (sign(normal.x)==sign(mNormal.x) and
              sign(normal.y)==sign(mNormal.y) and
              sign(normal.z)==sign(mNormal.z))
    #print "Result ",result
    
    return True
    
    
# ---- M3G Types --------------------------------------------------------------#
class M3GVertexList:
    def __init__(self, wrapList):
        self.mlist = wrapList

    def __getitem__(self, key):
        item = self.mlist[key]
        if type(item) is Types.MVertType:
            result =(item.co[0],item.co[1],item.co[2])
        else:
            result = item
        return result

class M3GBoneReference:
    def __init__(self,first,count):
        self.firstVertex=first #UInt32 
        self.vertexCount=count #UInt32 
        
        
class M3GBone:
    def __init__(self):
        self.verts=[] #List of influenced verts
        self.transformNode=None #ObjectIndex
        self.references = [] #References to Verts that are needed
        self.weight=0      #Int32

    
    def setVerts(self,aVerts):
        self.verts = aVerts
        self.createReferences()
        
    def createReferences(self):
        #print "createReference::len(verts) ",len(self.verts)
        if len(self.verts)==0: return #No Verts available
        self.verts.sort()
        ref = []
        list = []
        last = self.verts[0]-1
        count = 0
        for vert in self.verts:
            #print "vert ",vert
            if vert==last+1:
                list.append(vert)
            else:
                ref.append(M3GBoneReference(list[0],len(list)))
                #print list[0],len(list)
                list=[vert]
            last=vert
            #print "list ",list
        if len(list)>0:
            ref.append(M3GBoneReference(list[0],len(list)))
        self.references = ref
        
    
class M3GVector3D:
    def __init__(self,ax=0.0,ay=0.0,az=0.0):
        self.x = ax #Float32
        self.y = ay #Float32
        self.z = az #Float32
    
    def writeJava(self):
        return str(self.x)+"f, "+str(self.y)+"f, "+str(self.z)+"f"
    
    def getData(self):
        return struct.pack("<3f",self.x,self.y,self.z)
    
    def getDataLength(self):
        return struct.calcsize("<3f")

class M3GMatrix:
    """ A 4x4 generalized matrix. The 16 elements of the
        matrix are output in the same order as they are
        retrieved using the API Transform.get method. In
        other words, in this order:
        0 1 2 3
        4 5 6 7
        8 9 10 11
        12 13 14 15 """
    def __init__(self):
        self.elements=16 * [0.0] #Float32
        
    def identity(self):
        self.elements[ 0] = 1.0
        self.elements[ 5] = 1.0
        self.elements[10] = 1.0
        self.elements[15] = 1.0
    
    def getData(self):
        return struct.pack('<16f',self.elements[0],self.elements[1],
        self.elements[2],self.elements[3],
        self.elements[4],self.elements[5],
        self.elements[6],self.elements[7],
        self.elements[8],self.elements[9],
        self.elements[10],self.elements[11],
        self.elements[12],self.elements[13],
        self.elements[14],self.elements[15])

    def getDataLength(self):
        return struct.calcsize('<16f')
        
        
class M3GColorRGB:
    """ A color, with no alpha information. Each compo-
        nent is scaled so that 0x00 is 0.0, and 0xFF is 1.0.
        """
    def __init__(self,ared=0,agreen=0,ablue=0):
        self.red = ared #Byte
        self.green = agreen #Byte
        self.blue = ablue #Byte
        
    def writeJava(self):
        return "0x"+("%02X%02X%02X%02X" % (0.0, self.red, self.green, self.blue))
    
    def getData(self):
        return struct.pack('3B',self.red,self.green,self.blue)
    
    def getDataLength(self):
        return struct.calcsize('3B')


class M3GColorRGBA:
    """ A color, with alpha information. Each component
        is scaled so that 0x00 is 0.0, and 0xFF is 1.0. The
        alpha value is scaled so that 0x00 is completely
        transparent, and 0xFF is completely opaque. 
        """
    def __init__(self,ared=0,agreen=0,ablue=0,aalpha=0):
        self.red = ared #Byte 
        self.green = agreen #Byte 
        self.blue = ablue #Byte 
        self.alpha = aalpha #Byte

    def writeJava(self):
        return "0x"+("%02X%02X%02X%02X" % (self.alpha, self.red, self.green, self.blue))
        
    def getData(self):
        return struct.pack('4B',self.red,self.green,self.blue,self.alpha)
    
    def getDataLength(self):
        return struct.calcsize('4B')
    
    
#ObjectIndex
#The index of a previously encountered object in
#the file. Although this is serialized as a single
#unsigned integer, it is included in the compound
#type list because of the additional semantic infor-
#mation embodied in its type. A value of 0 is
#reserved to indicate a null reference; actual object indices start from 1. Object indices must refer
#only to null or to an object which has already been
#created during the input deserialization of a file -
#they must be less than or equal to the index of the
#object in which they appear. Other values are dis-
#allowed and must be treated as errors.
#UInt32
#index;

# ---- M3G Proxy --------------------------------------------------------------- #
class M3GProxy:
    def __init__(self):
        self.name = ""
        self.id=0
        self.ObjectType=0
        self.binaryFormat=''
        
    def __repr__(self):
        return "<"+str(self.__class__)[9:] + ":" + str(self.name) + ":" + str(self.id) + ">"

        
class M3GHeaderObject(M3GProxy):
    def __init__(self):
        M3GProxy.__init__(self)
        self.M3GHeaderObject_binaryFormat = '<BBBII'
        self.ObjectType=0
        self.id = 1   #Special Object: always 1
        self.VersionNumber=[1,0] #Byte[2] 
        self.hasExternalReferences=False #Boolean External Files needed? eg. Textures
        self.TotalFileSize=0 #UInt32 
        self.ApproximateContentSize=0 #UInt32 Only a hint! External sources included
        self.AuthoringField='Blender M3G Export' #String 
        
    def getData(self):
        data = struct.pack(self.M3GHeaderObject_binaryFormat,
                           self.VersionNumber[0],
                           self.VersionNumber[1],
                           self.hasExternalReferences,
                           self.TotalFileSize,
                           self.ApproximateContentSize)
        data += struct.pack(str(len(self.AuthoringField)+1)+'s',self.AuthoringField)
        return data
    
    def getDataLength(self):
        value = struct.calcsize(self.M3GHeaderObject_binaryFormat)
        return value + struct.calcsize(str(len(self.AuthoringField)+1)+'s')
        
class M3GExternalReference(M3GProxy):
    def __init__(self):         
        M3GProxy.__init__(self)
        self.ObjectType=0xFF
        self.URI=''             #reference URI
        
    def getData(self):
        data = struct.pack(str(len(self.URI)+1) + 's', self.URI)
        return data
        
    def getDataLength(self):
        return struct.calcsize(str(len(self.URI)+1)+'s')
        
    def searchDeep(self,alist):
        if not(self in alist): alist.append(self)
        return alist
        
    def __repr__(self):
        return M3GProxy.__repr__(self) + " (" + self.URI + ")"
       
        
class M3GObject3D(M3GProxy):
    def __init__(self):
        M3GProxy.__init__(self)
        self.userID=0 #UInt32 - field may be any value
        self.animationTracks=[] #ObjectIndex[] 
        self.userParameterCount=0 #UInt32  - No user parameter used 
        
    def searchDeep(self,alist):
        alist = doSearchDeep(self.animationTracks,alist)
        if not(self in alist): alist.append(self)
        return alist
        
    def getData(self):
        data = struct.pack('<I',self.userID)
        print "write userID",self.userID,self.name,str(self), self.getDataLength()
        data += struct.pack('<I',len(self.animationTracks))
        for element in self.animationTracks:
            data += struct.pack('<I',getId(element))
        data += struct.pack('<I',self.userParameterCount)
        return data

    def getDataLength(self):
        value = struct.calcsize('<3I')
        if len(self.animationTracks) > 0: 
            value += struct.calcsize('<'+str(len(self.animationTracks))+'I')
        return value

    def writeJava(self,aWriter,aCreate):
        if aCreate : pass #Abstract! Could not be created
        if len(self.animationTracks) > 0 :
            aWriter.write(2)
            for iTrack in self.animationTracks:
                aWriter.write(2,"BL%i.addAnimationTrack(BL%i);" % (self.id,iTrack.id))
            
            
class M3GTransformable(M3GObject3D):
    def __init__(self):
        M3GObject3D.__init__(self)
        self.hasComponentTransform=False #Boolean 
        #IF hasComponentTransform==TRUE, THEN
        self.translation=M3GVector3D(0,0,0) #Vector3D 
        self.scale=M3GVector3D(1,1,1) #Vector3D 
        self.orientationAngle=0 #Float32 
        self.orientationAxis=M3GVector3D(0,0,0) #Vector3D undefined
        #END
        self.hasGeneralTransform=False #Boolean 
        #IF hasGeneralTransform==TRUE, THEN
        self.transform = M3GMatrix() #Matrix identity
        self.transform.identity()
        #END
        #If either hasComponentTransform or hasGeneralTransform is false, the omitted fields will be
        #initialized to their default values (equivalent to an identity transform in both cases).

    def writeJava(self,aWriter,aCreate):
        if aCreate: pass #Abstract Base Class! Cant't be created
        M3GObject3D.writeJava(self,aWriter,False)
        if self.hasGeneralTransform :
            aWriter.write(2,"float[] BL%i_matrix = {" % (self.id))
            aWriter.writeList(self.transform.elements,4,"f")
            aWriter.write(2,"};")
            aWriter.write(2)
            aWriter.write(2,"Transform BL%i_transform = new Transform();" % (self.id))
            aWriter.write(2,"BL%i_transform.set(BL%i_matrix);" % (self.id,self.id))
            aWriter.write(2,"BL%i.setTransform(BL%i_transform);" % (self.id,self.id))
            aWriter.write(2)
        if self.hasComponentTransform:
            aWriter.write(2,("BL%i.setTranslation("+self.translation.writeJava()+");")
                             %(self.id))
    
    def getData(self):
        data = M3GObject3D.getData(self)
        data += struct.pack("<B",self.hasComponentTransform) 
        if self.hasComponentTransform==True:
            data += self.translation.getData()
            data += self.scale.getData() 
            data += struct.pack('<f',self.orientationAngle) 
            data += self.orientationAxis.getData()
        data += struct.pack("<B",self.hasGeneralTransform) 
        if self.hasGeneralTransform==True:
            data += self.transform.getData()
        return data
        
    def getDataLength(self):
        value = M3GObject3D.getDataLength(self)
        value += struct.calcsize("<B")
        if self.hasComponentTransform==True:
            value += self.translation.getDataLength() 
            value += self.scale.getDataLength() 
            value += struct.calcsize('<f') 
            value += self.orientationAxis.getDataLength()
        value += struct.calcsize("<B") 
        if self.hasGeneralTransform==True:
            value += self.transform.getDataLength()
        return value
        
        
class M3GNode(M3GTransformable):
    def __init__(self):
        M3GTransformable.__init__(self)
        self.blenderObj = None #Pointer to corrsponding BlenderObj 
        self.parentBlenderObj = None #Pointer to Parent in Blender
        self.blenderMatrixWorld = None #BlenderObj matrixWorld
        self.M3GNode_binaryFormat = '<BBBIB'
        self.enableRendering=True #Boolean 
        self.enablePicking=True #Boolean 
        self.alphaFactor=255 #Byte 0x00 is equivalent to 0.0 (fully transparent), and 255 is equivalent to 1.0 (fully opaque);
        self.scope=4294967295 #-1 #UInt32 
        self.hasAlignment = False #Boolean 
        #IF hasAlignment==TRUE, THEN
        self.M3GNode_binaryFormat_2 = '<BBII'
        self.zTarget=0 #Byte   The zTarget and yTarget fields must each hold a valid enumerated value, 
        self.yTarget=0 #Byte   as specified in the class definition. Other values must be treated as errors.
        self.zReference=None #ObjectIndex 
        self.yReference=None #ObjectIndex 
        #END
        #If the hasAlignment field is false, the omitted fields are initialized to their default values.


    def getData(self):
        data = M3GTransformable.getData(self)
        #print "Binary ",self.binaryFormat
        data += struct.pack(self.M3GNode_binaryFormat, 
                            self.enableRendering, 
                            self.enablePicking,  
                            self.alphaFactor, 
                            self.scope,  
                            self.hasAlignment)
                            
        if self.hasAlignment:
            data += pack(self.M3GNode_binaryFormat_2, 
                         self.zTarget,  
                         self.yTarget, 
                         getId(self.zReference),  
                         getId(self.yReference)) 
        return data
        
    def getDataLength(self):
        value = M3GTransformable.getDataLength(self) + \
                     struct.calcsize(self.M3GNode_binaryFormat)
        if self.hasAlignment:
            value += struct.calcsize(self.M3GNode_binaryFormat_2)
        return value
        
    def writeJava(self,aWriter,aCreate):
        if aCreate: pass #Abstract Base Class! Cant't be created
        M3GTransformable.writeJava(self,aWriter,False)    
        
class M3GGroup(M3GNode):
    def __init__(self):
        M3GNode.__init__(self)
        self.ObjectType=9
        self.children = [] #ObjectIndex[] 
        
    def searchDeep(self,alist):
        for element in self.children:
            alist = element.searchDeep(alist)
        return M3GNode.searchDeep(self,alist)

    def writeJava(self,aWriter,aCreate):
        if aCreate:
            aWriter.write(2,"//Group:"+self.name )
            aWriter.write(2,"Group BL"+str(self.id)+" = new Group();")
        M3GNode.writeJava(self,aWriter,False)
        for element in self.children:
            aWriter.write(2,"BL%i.addChild(BL%i);" % (self.id,element.id))
   
    def getData(self):
        data = M3GNode.getData(self)
        data = data + struct.pack("<I",len(self.children))
        for element in self.children:
            data += struct.pack("<I",getId(element))
        return data
    
    def getDataLength(self):
        return M3GNode.getDataLength(self)+ \
                 struct.calcsize("<"+str(len(self.children)+1)+"I")

        
class M3GWorld(M3GGroup):
    def __init__(self):
        M3GGroup.__init__(self)
        self.ObjectType=22
        self.activeCamera=None #ObjectIndex 
        self.background=None #ObjectIndex UInt32 0=None
        self.M3GWorld_binaryFormat='<II'
        
    def searchDeep(self,alist):
        alist = doSearchDeep([self.activeCamera, self.background],alist)
        return M3GGroup.searchDeep(self,alist)

        
    def writeJava(self,aWriter,aCreate):
        if aCreate:
            aWriter.write(2,"//World:"+self.name )
            aWriter.write(2,"World BL"+str(self.id)+" = new World();")
        M3GGroup.writeJava(self,aWriter,False)
        if self.background != None:
            aWriter.write(2,"BL"+str(self.id)+".setBackground(BL"+str(self.background.id)+");")
        if self.activeCamera != None:
            aWriter.write(2,"BL%i.setActiveCamera(BL%i);" %
                            (self.id,self.activeCamera.id))
        aWriter.write(2)

        
    def getData(self):
        data = M3GGroup.getData(self)
        return data + \
                struct.pack(self.M3GWorld_binaryFormat,getId(self.activeCamera),getId(self.background))


    def getDataLength(self):
        return M3GGroup.getDataLength(self) + struct.calcsize(self.M3GWorld_binaryFormat)
 
            
class M3GBackground(M3GObject3D):
    def __init__(self):
        M3GObject3D.__init__(self)
        self.ObjectType=4
        self.M3GBackground_binaryFormat = '<BBiiiiBB'
        self.backgroundColor=M3GColorRGBA(0,0,0,0) #ColorRGBA 0x00000000 (black, transparent)
        self.backgroundImage=None #ObjectIndex null (use the background color only)
        self.backgroundImageModeX=32; #Byte BORDER=32 REPEAT=33
        self.backgroundImageModeY=32; #Byte BORDER
        self.cropX = 0; #Int32 
        self.cropY = 0 #Int32 ;
        self.cropWidth = 0 #Int32 ;
        self.cropHeight = 0;#Int32 
        self.depthClearEnabled = True #Boolean 
        self.colorClearEnabled = True #Boolean 

    def writeJava(self,aWriter,aCreate):
        if aCreate:
            aWriter.write(2,"//Background:"+self.name )
            aWriter.write(2,"Background BL"+str(self.id)+" = new Background();")
        M3GObject3D.writeJava(self,aWriter,False)
        aWriter.write(2,"BL"+str(self.id)+".setColor("+self.backgroundColor.writeJava()+");")
        aWriter.write(2,"")

    def getData(self):
        data = M3GObject3D.getData(self)
        data += self.backgroundColor.getData()
        data += struct.pack('<I',getId(self.backgroundImage))
        data += struct.pack(self.M3GBackground_binaryFormat, self.backgroundImageModeX, 
                            self.backgroundImageModeY,
                            self.cropX, 
                            self.cropY, 
                            self.cropWidth, 
                            self.cropHeight, 
                            self.depthClearEnabled,  
                            self.colorClearEnabled)
        return data
    
    def getDataLength(self):
        value=M3GObject3D.getDataLength(self)
        value += self.backgroundColor.getDataLength()
        value += struct.calcsize('<I')
        value += struct.calcsize(self.M3GBackground_binaryFormat)
        return value
        
        
class M3GCamera(M3GNode):
    GENERIC=48      #Projection-Types
    PARALLEL=49
    PERSPECTIVE=50
    
    def __init__(self):
        M3GNode.__init__(self)
        self.ObjectType=5
        self.projectionType=M3GCamera.PARALLEL #Byte 
        #IF projectionType==GENERIC, THEN
        self.projectionMatrix=M3GMatrix() #Matrix •view volume : opposite corners at (-1 -1 -1) and (1 1 1)
        # TODO: Set right matrix    
        #ELSE
        self.fovy=0.0 #Float32 
        self.AspectRatio=0.0#Float32 
        self.near=0.0#Float32 
        self.far=0.0#Float32 
        #END
    
    def writeJava(self,aWriter,aCreate):
        if aCreate:
            aWriter.write(2,"//Camera " + self.name)
            aWriter.write(2,"Camera BL%i = new Camera();" % (self.id))
        M3GNode.writeJava(self,aWriter,False)
        aWriter.write(2,"BL%i.setPerspective(%ff,  //Field of View" % \
                          (self.id,self.fovy))
        aWriter.write(4,"(float)aCanvas.getWidth()/(float)aCanvas.getHeight(),")
        aWriter.write(4,str(self.near)+"f, //Near Clipping Plane")
        aWriter.write(4,str(self.far)+"f); //Far Clipping Plane")              
        
    def getData(self):
        data = M3GNode.getData(self)
        data += struct.pack("B",self.projectionType)
        if self.projectionType == self.GENERIC:
            data += self.projectionMatrix.getData()
        else:
            data += struct.pack("<4f",self.fovy,self.AspectRatio,self.near,self.far)
        return data
    
    def getDataLength(self):
        value = M3GNode.getDataLength(self)
        value += struct.calcsize("B")
        if self.projectionType == self.GENERIC:
            value += self.projectionMatrix.getDataLength()
        else:
            value += struct.calcsize("<4f")
        return value
        

class M3GMesh(M3GNode):
    def __init__(self,aVertexBuffer=None, aIndexBuffer=[], aAppearance=[]):
        M3GNode.__init__(self)
        self.ObjectType=14
        self.vertexBuffer = aVertexBuffer #ObjectIndex 
        self.submeshCount=len(aIndexBuffer) #UInt32 
        #FOR each submesh...
        self.indexBuffer=aIndexBuffer #ObjectIndex 
        self.appearance=aAppearance #;ObjectIndex 
        #END

    def getData(self):
        data = M3GNode.getData(self)
        data += struct.pack('<2I', getId(self.vertexBuffer), 
                                  self.submeshCount)
        for i in range(len(self.indexBuffer)):
            data += struct.pack('<2I',getId(self.indexBuffer[i]),
                                      getId(self.appearance[i]))
        return data
        
    def getDataLength(self):
        value = M3GNode.getDataLength(self)
        value += struct.calcsize('<2I')
        for i in range(len(self.indexBuffer)):
            value += struct.calcsize('<2I')
        return value
            
    def searchDeep(self,alist):
        alist = doSearchDeep([self.vertexBuffer] +self.indexBuffer
                              + self.appearance ,alist)
        return M3GNode.searchDeep(self,alist)
            
    def writeJava(self,aWriter,aCreate):
        self.writeBaseJava(aWriter,aCreate,"Mesh","")
        
    def writeBaseJava(self,aWriter,aCreate,aClassName,aExtension):
        if aCreate:
            aWriter.writeClass(aClassName,self)
            if self.submeshCount > 1:
                aWriter.write(2,"IndexBuffer[] BL%i_indexArray = {" % (self.id))
                aWriter.write(4,",".join(["BL%i" %(i.id) for i in self.indexBuffer ]))
                aWriter.write(2,"                                };")
                aWriter.write(2)
                aWriter.write(2,"Appearance[] BL%i_appearanceArray = {" % (self.id))
                aWriter.write(4,",".join(["BL%i" %(i.id) for i in self.appearance ]))
                aWriter.write(2,"                                };")
                aWriter.write(2)
                aWriter.write(2,"%s BL%i = new %s(BL%i,BL%i_indexArray,BL%i_appearanceArray%s);" % \
                                (aClassName,self.id,aClassName,self.vertexBuffer.id, self.id,self.id,aExtension))
            else:
                #print "indexBuffer", len(self.indexBuffer)
                #print "appearance", len(self.appearance)
                aWriter.write(2,"%s BL%i = new %s(BL%i,BL%i,BL%i%s);" % \
                                (aClassName, 
                                 self.id,
                                 aClassName,
                                 self.vertexBuffer.id,
                                 self.indexBuffer[0].id,
                                 self.appearance[0].id,
                                 aExtension))
        M3GNode.writeJava(self,aWriter,False)
        aWriter.write(2)  


class M3GSkinnedMesh(M3GMesh):
    def __init__(self,aVertexBuffer=None, aIndexBuffer=[], aAppearance=[]):
        M3GMesh.__init__(self,aVertexBuffer, aIndexBuffer, aAppearance)
        self.ObjectType=16
        self.skeleton=None #ObjectIndex
        self.bones={}
        #print"M3GSkinnedMesh.__init__::self.vertexBuffer:",self.vertexBuffer
        ##ObjectIndex skeleton;
        ##UInt32 transformReferenceCount;
        ##FOR each bone reference... 
        ##    ObjectIndex transformNode; 
        ##    UInt32 firstVertex; 
        ##    UInt32 vertexCount; 
        ##    Int32 weight;
        ##END

    def searchDeep(self,alist):
        alist = doSearchDeep([self.skeleton],alist)
        return M3GMesh.searchDeep(self,alist)

    def addSecondBone(self):
        secondBones = {}
        for bone in self.bones.values():
            bone2 = M3GBone()
            bone2.verts=bone.verts
            bone.verts=[]
            mGroup = M3GGroup()
            mGroup.name=bone.transformNode.name+"_second"
            bone2.transformNode=mGroup
            bone2.references = bone.references
            bone.references = [] 
            bone2.weight = bone.weight
            bone.weight=0
            mGroup.children = bone.transformNode.children
            bone.transformNode.children = [mGroup]
            mGroup.animationTracks=bone.transformNode.animationTracks
            bone.transformNode.animationTracks = []
            secondBones[bone.transformNode.name+"_second"]=bone2
        for bone in secondBones.values():
            self.bones[bone.transformNode.name] = bone
            
    def getBlenderIndexes(self):
        #print "M3GSkinnedMesh.vertexBuffer:",self.vertexBuffer
        return self.vertexBuffer.positions.blenderIndexes
    
    def writeJava(self,aWriter,aCreate):
        self.writeBaseJava(aWriter,aCreate,"SkinnedMesh",
                           (",BL%i" % (self.skeleton.id)))
        aWriter.write(2,"//Transforms")
        for bone in self.bones.values():
            #print "bone: ", bone
            #print "bone.references: ", bone.references
            for ref in bone.references:
                aWriter.write(2,"BL%i.addTransform(BL%i,%i,%i,%i);" % 
                                 (self.id,
                                  bone.transformNode.id,bone.weight,
                                  ref.firstVertex, ref.vertexCount))
        aWriter.write(2)
        
    def getDataLength(self):
        value = M3GMesh.getDataLength(self)
        value += struct.calcsize('<I')  #skeleton
        value += struct.calcsize('<I')  #transformReferenceCount
        for bone in self.bones.values():
            for ref in bone.references:
                value += struct.calcsize('<3Ii')
        return value
 
    def getData(self):
        data = M3GMesh.getData(self)
        data += struct.pack('<I', getId(self.skeleton))
        count = 0
        for bone in self.bones.values(): count+=len(bone.references)
        data += struct.pack('<I',count)
        for bone in self.bones.values():
            for ref in bone.references:
                data += struct.pack('<I',getId(bone.transformNode))
                data += struct.pack('<2I',ref.firstVertex,ref.vertexCount)
                data += struct.pack('<i',bone.weight)
        return data

class M3GLight(M3GNode):
    def __init__(self):
        M3GNode.__init__(self)
        self.ObjectType=12
        self.modes = {'AMBIENT':128,
                      'DIRECTIONAL':129,
                      'OMNI':130,
                      'SPOT':131}
        self.attenuationConstant = 1.0 #Float32
        self.attenuationLinear = 0.0 #Float32 
        self.attenuationQuadratic = 0.0 #Float32 
        self.color = M3GColorRGB(1.0, 1.0, 1.0) #ColorRGB 
        self.mode = self.modes['DIRECTIONAL'] #Byte Enumurator mode: DIRECTIONAL
        self.intensity = 1.0 #Float32 
        self.spotAngle = 45 #Float32 
        self.spotExponent = 0.0 #Float32
    
    def writeJava(self,aWriter,aCreate):
        if aCreate:
            aWriter.write(2,"//Light: " + self.name)
            aWriter.write(2,"Light BL%i = new Light();" % (self.id))
        aWriter.write(2,"BL%i.setMode(%i);" % (self.id,self.mode)) #Light.OMNI
        if self.mode in [self.modes['OMNI'],self.modes['SPOT']]:#Attenuation
            aWriter.write(2,"BL%i.setAttenuation(%ff, %ff,%ff);" 
                             % (self.id,
                                self.attenuationConstant,
                                self.attenuationLinear, 
                                self.attenuationQuadratic))
        aWriter.write(2,("BL%i.setColor("+self.color.writeJava()+");") 
                             % (self.id))
        aWriter.write(2,"BL%i.setIntensity(%ff);" 
                             % (self.id,self.intensity))
        if self.mode == self.modes['SPOT']:
            aWriter.write(2,"BL%i.setSpotAngle(%ff);" 
                             % (self.id,self.spotAngle))
            aWriter.write(2,"BL%i.setSpotExponent(%ff);" 
                             % (self.id,self.spotExponent))
        M3GNode.writeJava(self,aWriter,False)
        aWriter.write(2)
        
        
    def getData(self):
        data = M3GNode.getData(self)
        data += struct.pack("<fff", self.attenuationConstant,
                                    self.attenuationLinear, 
                                    self.attenuationQuadratic) 
        data += self.color.getData() 
        data += struct.pack("<Bfff", self.mode,
                                     self.intensity, 
                                     self.spotAngle, 
                                     self.spotExponent)
        return data

    def getDataLength(self):
        value = M3GNode.getDataLength(self)
        value += self.color.getDataLength()
        value += struct.calcsize('<B6f')
        return value
        
class  M3GMaterial(M3GObject3D):
    def __init__(self):
        M3GObject3D.__init__(self)
        self.ObjectType=13
        self.ambientColor=M3GColorRGB(0.2, 0.2, 0.2) #ColorRGB 
        self.diffuseColor=M3GColorRGBA(0.8, 0.8, 0.8, 1.0) #ColorRGBA 
        self.emissiveColor=M3GColorRGB(0.0, 0.0, 0.0) #ColorRGB 
        self.specularColor=M3GColorRGB(0.0, 0.0, 0.0) #ColorRGB 
        self.shininess=0.0 #Float32 
        self.vertexColorTrackingEnabled=False #Boolean

    def writeJava(self,aWriter,aCreate):
        if aCreate :
            aWriter.write(2,"//Material: "+self.name )
            aWriter.write(2,"Material BL%i = new Material();" % (self.id))
        aWriter.write(2,("BL%i.setColor(Material.AMBIENT," + 
                                 self.ambientColor.writeJava()+");") % (self.id) )
        aWriter.write(2,("BL%i.setColor(Material.SPECULAR," + 
                                 self.specularColor.writeJava()+");") % (self.id) )
        aWriter.write(2,("BL%i.setColor(Material.DIFFUSE," +
                                 self.diffuseColor.writeJava()+");") % (self.id))
        aWriter.write(2,("BL%i.setColor(Material.EMISSIVE," +
                                 self.emissiveColor.writeJava()+");") % (self.id))
        aWriter.write(2,("BL%i.setShininess(%ff);") % (self.id,self.shininess))
        aWriter.write(2,("BL%i.setVertexColorTrackingEnable(" + 
                               toJavaBoolean(self.vertexColorTrackingEnabled) + ");") % 
                                (self.id))
        M3GObject3D.writeJava(self,aWriter,False)
        
    def getData(self):
        data = M3GObject3D.getData(self)
        data += self.ambientColor.getData()
        data += self.diffuseColor.getData()
        data += self.emissiveColor.getData()
        data += self.specularColor.getData()
        data += struct.pack('<fB',self.shininess,
                                  self.vertexColorTrackingEnabled)
        return data


    def getDataLength(self):
        value = M3GObject3D.getDataLength(self)
        value += self.ambientColor.getDataLength()
        value += self.diffuseColor.getDataLength()
        value += self.emissiveColor.getDataLength()
        value += self.specularColor.getDataLength()
        value += struct.calcsize('<fB')
        return value


class M3GVertexArray(M3GObject3D):
    def __init__(self,aNumComponents,aComponentSize,aAutoScaling=False,aUVMapping=False):
        M3GObject3D.__init__(self)
        self.ObjectType=20
        self.blenderIndexes={} #Translation-Table from Blender index to m3g index
        self.autoscaling = aAutoScaling #bias and scale should be computed internal
        self.uvmapping=aUVMapping #Change coordinates from blender uv to uv-m3g
        self.bias = [0.0,0.0,0.0] 
        self.scale = 1.0 
        self.componentSize=aComponentSize #Byte number of bytes per component; must be [1, 2]
        self.componentCount=aNumComponents #Byte number of components per vertex; must be [2, 4]
        self.encoding=0 #Byte 0="raw" as bytes or 16 bit integers.
        self.vertexCount=0 #UInt16 number of vertices in this VertexArray; must be [1, 65535]
        if self.autoscaling==True:
            self.components = array('f')
        else:
            self.components = self.createComponentArray()
        #FOR each vertex...
        # IF componentSize==1, THEN
        #  IF encoding==0, THEN
        #   Byte[componentCount] 
        #  ELSE IF encoding==1, THEN
        #   Byte[componentCount] 
        #  END
        # ELSE
        #  IF encoding==0, THEN
        #   Int16[componentCount] 
        #  ELSE IF encoding==1, THEN
        #   Int16[componentCount] 
        #  END
        # END
        #END
    
    def createComponentArray(self):
        if self.componentSize == 1:
            return array('b') #Byte-Array
        else:
            return array('h') #Short-Array
            
    def useMaxPrecision(self,aBoundingBox):
        """With Bias and Scale you can maximize the precision of the array"""
        #print "useMaxPrecision"
        vertexList = M3GVertexList(aBoundingBox)
        first = vertexList[0]
        minimum =[first[0],first[1],first[2]]
        maximum = [first[0],first[1],first[2]] #Search maximal Dimension
        for element in vertexList:
            for i in range(3):
                if minimum[i] > element[i] : minimum[i] = element[i]
                if maximum[i] < element[i] : maximum[i] = element[i]
                #print i, maximum[i],element[i]
        lrange=[0,0,0]
        maxRange=0.0
        maxDimension=-1
        for i in range(3):  #set bias
            lrange[i] = maximum[i]-minimum[i]
            self.bias[i] = minimum[i]*0.5+maximum[i]*0.5
            #print "Bias",i,self.bias[i],"min-max",minimum[i],maximum[i],"lrang",lrange[i]
            if lrange[i] > maxRange:
                maxRange = lrange[i]
                maxDimension=i
        self.scale = maxRange/65533.0
        #print "MaxRange ",maxRange
        #print "scale",self.scale


    def internalAutoScaling(self):
        print "internalAutoScaling"
        #Already done?
        print self.components.typecode
        if not self.autoscaling or self.components.typecode!="f":return
        #Find bais and scale
        minimum=[]
        maximum=[]
        for i in range(self.componentCount):
            minimum.append(self.components[i])
            maximum.append(self.components[i])         
        for i in range(0,len(self.components),self.componentCount):
            for j in range(self.componentCount):
                if minimum[j] > self.components[i+j] : minimum[j] = self.components[i+j]
                if maximum[j] < self.components[i+j] : maximum[j] = self.components[i+j]
                #print "i+j=",i+j,"min=",minimum[j],"max=",maximum[j],"elem=",self.components[i+j]
        #print "min=", minimum
        #print "max=", maximum
        lrange=[0] * self.componentCount
        maxRange=0.0
        maxDimension=-1
        for i in range(self.componentCount):  #set bias
            lrange[i] = maximum[i]-minimum[i]
            self.bias[i] = minimum[i]*0.5+maximum[i]*0.5
            #print "Bias",i,self.bias[i],"min-max",minimum[i],maximum[i],"lrang",lrange[i]
            if lrange[i] > maxRange:
                maxRange = lrange[i]
                maxDimension=i
        maxValue=(2**(8*self.componentSize)*1.0)-2.0
        #print "MaxValue=",maxValue
        self.scale = maxRange/maxValue
        #print "MaxRange ",maxRange
        #print "scale",self.scale
        #Copy Components
        oldArray=self.components
        self.components=self.createComponentArray()
        for i in range(0,len(oldArray),self.componentCount):
            for j in range(self.componentCount):
                element=int((oldArray[i+j]-self.bias[j])/self.scale)
                #print "element",element
                self.components.append(element)
        # Reverse t coordinate because M3G uses a different 2D coordinate system than Blender.
        if self.uvmapping:
            for i in range(0,len(self.components),2):
                self.components[i+1]= int(self.components[i+1]*(-1)) #Error in Version 0.7
        for i in range(len(self.components)):
            if abs(self.components[i])>maxValue:raise Exception( i+". element too great/small!")
                
    def writeJava(self,aWriter,aCreate):
        self.internalAutoScaling()
        if aCreate:
            aWriter.write(2,"// VertexArray " + self.name)
            if self.componentSize == 1:
                aWriter.write(2,"byte[] BL%i_array = {" % (self.id))
            else:
                aWriter.write(2,"short[] BL%i_array = {" % (self.id))
            aWriter.writeList(self.components)
            aWriter.write(2,"};")
            aWriter.write(2)
            aWriter.write(2,"VertexArray BL%i = new VertexArray(BL%i_array.length/%i,%i,%i);" %
                        (self.id,self.id,
                            self.componentCount,self.componentCount,self.componentSize))
            aWriter.write(2,"BL%i.set(0,BL%i_array.length/%i,BL%i_array);" % 
                    (self.id,self.id,self.componentCount,self.id))
        M3GObject3D.writeJava(self,aWriter,False)
        aWriter.write(2)
     
    
    def getData(self):
        self.internalAutoScaling()
        self.vertexCount = len(self.components)/self.componentCount
        data = M3GObject3D.getData(self)
        data += struct.pack('<3BH',self.componentSize,
                                 self.componentCount,
                                 self.encoding,
                                 self.vertexCount)
        componentType = ""
        if self.componentSize == 1:
            componentType = "b"
        else:
            componentType = "h"
        for element in self.components:
            data += struct.pack('<'+componentType,element)
        return data
        
    def getDataLength(self):
        self.internalAutoScaling()
        value = M3GObject3D.getDataLength(self)
        value += struct.calcsize('<3BH')
        componentType = ""
        if self.componentSize == 1:
            componentType = "b"
        else:
            componentType = "h"
        value += struct.calcsize('<'+str(len(self.components))+componentType)
        return value
        
    def append(self,element,index=None):
        #print "type(element):",type(element)
        if type(element) is Types.vectorType :
            for i in range(3):
                value = int((element[i]-self.bias[i])/self.scale)  
                #print "append:",i,element[i],(element[i]-self.bias[i]),value                  
                self.components.append(value)
        elif type(element) is Types.MVertType:
            for i in range(3):
                value = int((element.co[i]-self.bias[i])/self.scale)  
                #print "append:",i,element[i],(element[i]-self.bias[i]),value                  
                self.components.append(value)
            if index!=None:
                key=str(len(self.blenderIndexes))
                #print"key,index:",key,index
                self.blenderIndexes[key]=index
                #print"blenderIndexes",self.blenderIndexes
        else:
            print "VertexArray.append: element=",element
            self.components.append(element)
            
class M3GVertexBuffer(M3GObject3D):
    def __init__(self):
        M3GObject3D.__init__(self)
        self.ObjectType=21
        self.defaultColor=M3GColorRGBA(255,255,255) #ColorRGBA 0xFFFFFFFF (opaque white).
        self.positions = None #ObjectIndex 
        self.positionBias = [0.0,0.0,0.0] #Float32[3] 
        self.positionScale = 1.0 #Float32 
        self.normals = None #ObjectIndex 
        self.colors = None #ObjectIndex
        self.texCoordArrays = [] 
        self.texcoordArrayCount = 0 #UInt32 
##        #FOR each texture coordinate array...
##        self.texCoords = [] #ObjectIndex 
##        self.texCoordBias=[] #Float32[3] 
##        self.texCoordScale=[] #;Float32 
##        #END
##        #If a texture coordinate array has only two components, the corresponding texCoordBias[2] element
##        #must be 0.0.
##        #Null texture coordinate arrays are never serialized, regardless of their position. A single texture
##        #coordinate array will therefore always be serialized as belonging to texturing unit 0, regardless of
##        #its original unit it was assigned to.
##        #There are as many references in the texture coordinates array as there are active texture units for
##        #this geometry. The texture coordinate references are loaded sequentially from texture unit 0. If the
##        #implementation supports more texture units than are specified, these are left in their default, inactive
##        #state, with a null texture coordinate reference and an undefined bias and scale.
##        #If more texture coordinate references are specified than are supported by the implementation, then
##        #this must be treated as an error, as it would be in the API. The application can then decide on an
##        #appropriate course of action to handle this case.

    def searchDeep(self,alist):
        if self.positions!=None: alist = self.positions.searchDeep(alist)
        if self.normals != None: alist = self.normals.searchDeep(alist)
        if self.colors!= None: alist = self.colors.searchDeep(alist)
        alist = doSearchDeep(self.texCoordArrays, alist)
        return M3GObject3D.searchDeep(self,alist)
    
    def setPositions(self,aVertexArray):
        self.positions = aVertexArray
        self.positionBias = aVertexArray.bias
        self.positionScale = aVertexArray.scale
    
    def writeJava(self,aWriter,aCreate):
        if aCreate:
            aWriter.write(2,"//VertexBuffer"+self.name )
            aWriter.write(2,"VertexBuffer BL%i = new VertexBuffer();" % (self.id))
        aWriter.write(2,"float BL%i_Bias[] = { %ff, %ff, %ff};" %
                            (self.id,self.positionBias[0],
                             self.positionBias[1],self.positionBias[2]))
        aWriter.write(2,"BL%i.setPositions(BL%i,%ff,BL%i_Bias);" % 
                               (self.id, self.positions.id,
                                self.positionScale,self.id))
        aWriter.write(2,"BL%i.setNormals(BL%i);" % (self.id,self.normals.id))
        #if self.colors != None: aWriter.write(2,"BL%i.setTexCoords(0,BL%i,1.0f,null);" % 
        #                                           (self.id,self.colors.id))
        lIndex = 0
        for iTexCoord in self.texCoordArrays:
            aWriter.write(2,"float BL%i_%i_TexBias[] = { %ff, %ff, %ff};" %
                            (self.id,lIndex, iTexCoord.bias[0],
                             iTexCoord.bias[1],iTexCoord.bias[2]))
           #int index, javax.microedition.m3g.VertexArray194 texCoords, float scale, float[] bias
            aWriter.write(2,"BL%i.setTexCoords(%i,BL%i,%ff,BL%i_%i_TexBias);" % 
                               (self.id, lIndex, iTexCoord.id, iTexCoord.scale,self.id,lIndex))
            lIndex += 1
   
        M3GObject3D.writeJava(self,aWriter,False)
    
    
    def getData(self):
        self.texcoordArrayCount = len(self.texCoordArrays)
        data = M3GObject3D.getData(self)
        data += self.defaultColor.getData()
        data += struct.pack('<I4f3I',getId(self.positions),
                                     self.positionBias[0],
                                     self.positionBias[1],
                                     self.positionBias[2],
                                     self.positionScale,
                                     getId(self.normals),
                                     getId(self.colors),
                                     self.texcoordArrayCount)
        for iTexCoord in self.texCoordArrays:
            data += struct.pack('<I', getId(iTexCoord))
            data += struct.pack('<ffff', iTexCoord.bias[0],
                                    iTexCoord.bias[1],
                                    iTexCoord.bias[2],
                                    iTexCoord.scale)
        return data

    
    def getDataLength(self):
        value = M3GObject3D.getDataLength(self)
        value += self.defaultColor.getDataLength()
        value += struct.calcsize('<I4f3I')
        value += struct.calcsize('<Iffff') * len(self.texCoordArrays)
        return value


class M3GPolygonMode(M3GObject3D):
    CULL_BACK=160
    CULL_NONE=162
    SHADE_FLAT=164
    SHADE_SMOOTH=165
    WINDING_CCW=168
    WINDING_CW=169
    
    def __init__(self):
        M3GObject3D.__init__(self)
        self.ObjectType=8
        self.culling=M3GPolygonMode.CULL_BACK #Byte
        self.shading=M3GPolygonMode.SHADE_SMOOTH #Byte
        self.winding=M3GPolygonMode.WINDING_CCW #Byte
        self.twoSidedLightingEnabled = False #Boolean 
        self.localCameraLightingEnabled = False #Boolean 
        self.perspectiveCorrectionEnabled = False #Boolean
        
    def writeJava(self,aWriter,aCreate):
        if aCreate:
            aWriter.write(2,"PolygonMode BL%i = new PolygonMode();" % (self.id))
        aWriter.write(2,"BL%i.setCulling(%i);" % (self.id,self.culling))
        aWriter.write(2,"BL%i.setShading(%i);" % (self.id,self.shading))
        aWriter.write(2,"BL%i.setWinding(%i);" % (self.id,self.winding))
        aWriter.write(2,("BL%i.setTwoSidedLightingEnable(" + 
                toJavaBoolean(self.twoSidedLightingEnabled) + ");") % 
                       (self.id))
        aWriter.write(2,("BL%i.setLocalCameraLightingEnable(" + 
                toJavaBoolean(self.localCameraLightingEnabled) + ");") % 
                       (self.id))
        aWriter.write(2,("BL%i.setPerspectiveCorrectionEnable(" + 
                toJavaBoolean(self.perspectiveCorrectionEnabled) + ");") % 
                       (self.id))
        aWriter.write(2)
        M3GObject3D.writeJava(self,aWriter,False)
    
    def getData(self):
        data = M3GObject3D.getData(self)
        data += struct.pack('6B',self.culling,
                                 self.shading,
                                 self.winding,
                                 self.twoSidedLightingEnabled, 
                                 self.localCameraLightingEnabled, 
                                 self.perspectiveCorrectionEnabled)
        return data

    def getDataLength(self):
        value = M3GObject3D.getDataLength(self)
        value += struct.calcsize('6B')
        return value

class M3GIndexBuffer(M3GObject3D):
    def __init__(self):
        M3GObject3D.__init__(self)

    def getData(self):
        return M3GObject3D.getData(self)
        
    def getDataLength(self):
        return M3GObject3D.getDataLength(self)
    
    def writeJava(self,aWriter,aCreate):
        M3GObject3D.writeJava(self,aWriter,False)
        
        
class M3GTriangleStripArray(M3GIndexBuffer):
    def __init__(self):
        M3GIndexBuffer.__init__(self)
        self.ObjectType=11 
        self.encoding=128 #Byte Bit 7: 1 = explicit property on index buffer true
                      #Bit 1 .. 6: 0 = "raw" integer values 1= a single byte will suffice
                                  #2 = a 16 bit integer is suffi to hold all the given index values
        #IF encoding == 0, THEN
        #self.startIndex = 0  #;UInt32 
        #ELSE IF encoding == 1, THEN
        #Byte startIndex;
        #ELSE IF encoding == 2, THEN
        #UInt16 startIndex;
        #ELSE IF encoding == 128, THEN
        self.indices = [] #;UInt32[]
        #ELSE IF encoding == 129, THEN
        #Byte[] indices;
        #ELSE IF encoding == 130, THEN
        #UInt16[] indices;
        #END
        self.stripLengths = [] #;UInt32[]

    def writeJava(self,aWriter,aCreate):
        if aCreate:
            aWriter.write(2,"//length of TriangleStrips")
            aWriter.write(2,"int[] BL"+str(self.id)+"_stripLength ={"+
                ",".join([str(element) for element in self.stripLengths])+"};")
            aWriter.write(2)
            aWriter.write(2,"//IndexBuffer")
            aWriter.write(2,"int[] BL%i_Indices = {" % (self.id))
            aWriter.write(2,",".join([str(element) for element in self.indices])+"};")
            aWriter.write(2)
            aWriter.write(2,"IndexBuffer BL%i=new TriangleStripArray(BL%i_Indices,BL%i_stripLength);" %
                            (self.id, self.id, self.id))
        M3GIndexBuffer.writeJava(self,aWriter,False)
        aWriter.write(2)
     
    
    def getData(self):
        data = M3GIndexBuffer.getData(self)
        data += struct.pack('<BI',self.encoding,
                                  len(self.indices))
        for element in self.indices:
            data += struct.pack('<I',element)
        data += struct.pack('<I',len(self.stripLengths))
        for element in self.stripLengths:
            data += struct.pack('<I',element)
        return data
    
    def getDataLength(self):
        value = M3GIndexBuffer.getDataLength(self)
        value += struct.calcsize('<BI')
        if len(self.indices) > 0 :
            value += struct.calcsize('<' + str(len(self.indices)) + 'I')
        value += struct.calcsize('<I')
        if len(self.stripLengths) > 0:
            value+= struct.calcsize('<'+str(len(self.stripLengths))+'I')
        return value
        
        
class M3GAppearance(M3GObject3D):
    def __init__(self):
        M3GObject3D.__init__(self)
        self.ObjectType=3
        self.layer=0 #Byte 
        self.compositingMode=None #ObjectIndex
        self.fog=None #ObjectIndex 
        self.polygonMode=None #ObjectIndex 
        self.material=None #ObjectIndex 
        self.textures=[] #;ObjectIndex[]
        
    def searchDeep(self,alist):
        alist = doSearchDeep([self.compositingMode,self.fog,
                              self.polygonMode,self.material]
                             + self.textures,alist)
        return M3GObject3D.searchDeep(self,alist)

    def getData(self):
        data = M3GObject3D.getData(self)
        data += struct.pack("<B5I", self.layer,
                                    getId(self.compositingMode),
                                    getId(self.fog), 
                                    getId(self.polygonMode), 
                                    getId(self.material), 
                                    len(self.textures))
        for element in self.textures:
            data += struct.pack("<I",getId(element))
        return data
    
    def getDataLength(self):
        value = M3GObject3D.getDataLength(self)
        value += struct.calcsize("<B5I")
        if len(self.textures) > 0 : 
            value += struct.calcsize("<"+str(len(self.textures))+'I')
        return value
        
        
    def writeJava(self,aWriter,aCreate):
        if aCreate:
            aWriter.write(2,"//Appearance")
            aWriter.write(2,"Appearance BL%i = new Appearance();" % (self.id))
        if self.compositingMode!= None:
            aWriter.write(2,"BL%i.setPolygonMode(BL%i);" % 
                             (self.id,self.compositingMode.id))
        if self.fog!=None:
            aWriter.write(2,"BL%i.setFog(BL%i);" %
                              (self.id,self.fog.id))
        if self.polygonMode!=None:
            aWriter.write(2,"BL%i.setPolygonMode(BL%i);" %
                              (self.id,self.polygonMode.id))
        if self.material!=None: 
            aWriter.write(2,"BL%i.setMaterial(BL%i);" %
                              (self.id,self.material.id))
        i=0
        for itexture in self.textures:
            aWriter.write(2,"BL%i.setTexture(%i,BL%i);" %           
                             (self.id,i,itexture.id))
            i =+ 1
        M3GObject3D.writeJava(self,aWriter,False)
        aWriter.write(2)  

class M3GTexture2D(M3GTransformable):
    #M3G imposes the following restrictions when assigning textures to a model: 
    #The dimensions must be powers of two (4, 8, 16, 32, 64, 128...).

    WRAP_REPEAT = 241
    WRAP_CLAMP = 240
    FILTER_BASE_LEVEL=208
    FILTER_LINEAR=209
    FILTER_NEAREST=210
    FUNC_ADD=224
    FUNC_BLEND=225
    FUNC_DECAL=226
    FUNC_MODULATE=227
    FUNC_REPLACE=228

    def __init__(self,aImage):
        M3GTransformable.__init__(self)
        self.ObjectType=17
        self.Image = aImage #ObjectIndex
        self.blendColor=M3GColorRGB(0,0,0)
        self.blending=M3GTexture2D.FUNC_MODULATE #Byte
        self.wrappingS=M3GTexture2D.WRAP_REPEAT #Byte 
        self.wrappingT=M3GTexture2D.WRAP_REPEAT #Byte 
        self.levelFilter=M3GTexture2D.FILTER_BASE_LEVEL #Byte 
        self.imageFilter=M3GTexture2D.FILTER_NEAREST #Byte

    def searchDeep(self,alist):
        alist = doSearchDeep([self.Image],alist)
        return M3GTransformable.searchDeep(self,alist)

    def getData(self):
        data = M3GTransformable.getData(self)
        data += struct.pack('<I', getId(self.Image))
        data += self.blendColor.getData()
        data += struct.pack('5B',self.blending,
                                 self.wrappingS, 
                                 self.wrappingT, 
                                 self.levelFilter, 
                                 self.imageFilter)
        return data
    
    def getDataLength(self):
        value = M3GTransformable.getDataLength(self)
        value += struct.calcsize('<I')
        value += self.blendColor.getDataLength()
        value += struct.calcsize('5B')
        return value
            
    def writeJava(self,aWriter,aCreate):
        if aCreate:
            aWriter.write(2,"//Texture")
            aWriter.write(2,"Texture2D BL%i = new Texture2D(BL%i);" % (self.id,
                                                                      self.Image.id))
        aWriter.write(2,"BL%i.setFiltering(%i,%i);" % (self.id,
                                   self.levelFilter, 
                                   self.imageFilter))
        aWriter.write(2,"BL%i.setWrapping(%i,%i);" % (self.id,
                                    self.wrappingS,
                                    self.wrappingT))
        aWriter.write(2,"BL%i.setBlending(%i);" % (self.id,self.blending))
        aWriter.write(2)
        M3GTransformable.writeJava(self,aWriter,False)

class ImageFactory:
    images={}
    def getImage(self, image, externalReference):
        # It's important to use getFilename() because getName() returns a 
        # truncated string depending on the length of the file name.
        filename = Blender.sys.expandpath(image.getFilename())

        if self.images.has_key(filename):
            image = self.images[filename]
        elif externalReference:
            # Check for file ending (only relevant for external images). The M3G specification 
            # mandates only PNG support, but some devices might also support additional image types.
            [path,ext] = splitext(filename)
            if ext != ".png":
                print "Warning: image file ends with " + ext + ". M3G specification only mandates PNG support."

            image = M3GExternalReference()
            image.URI = Blender.sys.basename(filename)
            self.images[filename] = image
        else:
            image = M3GImage2D(image)
            self.images[filename] = image
        return image
            
    getImage = classmethod(getImage)

        
class M3GImage2D(M3GObject3D):
    ALPHA=96             #a single byte per pixel, representing pixel opacity
    LUMINANCE=97         #a single byte per pixel, representing pixel luminance.
    LUMINANCE_ALPHA=98   #two bytes per pixel. The first: luminance, the second: alpha.
    RGB=99               #three bytes per pixel, representing red, green and blue
    RGBA=100             #four bytes per pixel, representing red, green, blue and alpha

    def __init__(self, aImage, aFormat=RGBA):
        M3GObject3D.__init__(self)
        self.ObjectType=10
        self.image=aImage #Blender Image
        self.format=aFormat #Byte 
        self.isMutable=False #Boolean changable or not
        [self.width, self.height] = aImage.getSize()

        #IF isMutable==false, THEN
        self.palette=0 #Byte[] 
        self.pixels = array('B') #Byte[] 
        self.extractPixelsFromImage()
        #END
#For a palettised format, the pixels array contains a single palette 
#index per pixel, and the palette array will contain up to 256 entries, 
#each consisting of a pixel specifier appropriate to the format chosen.

#For a non-palettised format, the palette array will be empty, 
#and the pixels array contains a pixel specifier appropriate to the format 
#chosen.
#In a pixel specifier, each byte is scaled such that 0 represents the 
#value 0.0 and 255 represents the value 1.0. The different formats 
#require different data to be serialized, as follows:

#The width and height of the image must be non-negative powers of two, but they need not be equal.

    def getData(self):
        data = M3GObject3D.getData(self)
        data += struct.pack('2B', self.format, self.isMutable)
        data += struct.pack('<2I', self.width, self.height)
        if self.isMutable == False:
            # TODO: support palettised formats also
            # export palette data
            data += struct.pack('<I', 0)
            
            # export pixel data
            if self.format == M3GImage2D.RGBA:
                #print "len pixels",len(self.pixels)
                data += struct.pack('<I', len(self.pixels))
                for pixel in self.pixels:
                    data += struct.pack('B', pixel)
            #elif...
        return data

    def getDataLength(self):
        value = M3GObject3D.getDataLength(self)
        value += struct.calcsize('2B')
        value += struct.calcsize('<2I')
        if self.isMutable == False:
            # TODO: support palettised formats also
            value+= struct.calcsize('<I')
            
            # pixel data size
            if self.format == M3GImage2D.RGBA:
                value += struct.calcsize('<I')
                value += struct.calcsize(str(len(self.pixels))+'B')
        return value
    
    def writeJava(self,aWriter,aCreate):
        if aCreate:
            lFileName = self.image.filename
            if not Blender.sys.exists(lFileName) :
                lFileName = Blender.sys.join(dirname(Blender.Get('filename')),
                                             basename(self.image.filename))
            elif not Blender.sys.exists(lFileName):
                raise FileError, 'Image file not found!'
            lTargetFile = Blender.sys.join(Blender.sys.dirname(aWriter.filename),
                                             Blender.sys.basename(self.image.filename))   
            copy_file(lFileName,lTargetFile)
            #shutil.copyfile(lFileName,lTargetFile)
            aWriter.write(2,"//Image2D")
            aWriter.write(2,"Image BL%i_Image = null;" % (self.id))
            aWriter.write(2,"try {")
            aWriter.write(3,"BL%i_Image = Image.createImage(\"/%s\");" %
                                    (self.id,basename(self.image.filename)))
            aWriter.write(2,"} catch (IOException e) {")
            aWriter.write(3,"e.printStackTrace();")
            aWriter.write(2,"}")
            aWriter.write(2,"Image2D BL%i = new Image2D(Image2D.RGBA,BL%i_Image);" % 
                                    (self.id,self.id))   
        aWriter.write(2)
        M3GObject3D.writeJava(self,aWriter,False)
        aWriter.write(2)  
        
    def extractPixelsFromImage(self):
        # Reverse t coordiante because M3G uses a different 2D coordinate system than OpenGL.
        for y in range(self.height):
            for x in range(self.width):
                [r, g, b, a] = self.image.getPixelI(x, self.height-1-y)
                self.pixels.append(r)
                self.pixels.append(g)
                self.pixels.append(b)
                self.pixels.append(a)
                
class M3GAnimationController(M3GObject3D):
    def __init__(self):
        M3GObject3D.__init__(self)
        self.ObjectType=1
        self.speed = 1.0 #Float32
        self.weight = 1.0 #Float32
        self.activeIntervalStart = 0 #Int32 - (always active)
        self.activeIntervalEnd = 0 #Int32 
        self.referenceSequenceTime = 0.0 #Float32 
        self.referenceWorldTime = 0 #Int32 

    def writeJava(self,aWriter,aCreate):
        if aCreate:
            aWriter.writeClass("AnimationController",self)
            aWriter.write(2,"AnimationController BL%i = new AnimationController();" %
                                   (self.id))
        aWriter.write(2,"BL%i.setActiveInterval(%i, %i);" %
                            (self.id,self.activeIntervalStart,self.activeIntervalEnd))
        #lightAnim.setPosition(0, 2000);(2) Applying the animation during rendering
        M3GObject3D.writeJava(self,aWriter,False)
            
    def getData(self):
        data = M3GObject3D.getData(self)
        data += struct.pack("<ffiifi", self.speed,
                                    self.weight,
                                    self.activeIntervalStart,
                                    self.activeIntervalEnd, 
                                    self.referenceSequenceTime, 
                                    self.referenceWorldTime)
        return data
        
    def getDataLength(self):
        value = M3GObject3D.getDataLength(self)
        return value + struct.calcsize("<ffiifi")
                
class M3GAnimationTrack(M3GObject3D):
    ALPHA=256
    AMBIENT_COLOR=257
    COLOR=258
    CROP=259
    DENSITY=260
    DIFFUSE_COLOR=261
    EMISSIVE_COLOR=262
    FAR_DISTANCE=263
    FIELD_OF_VIEW=264
    INTENSITY=265
    MORPH_WEIGHTS=266
    NEAR_DISTANCE=267
    ORIENTATION=268
    PICKABILITY=269
    SCALE=270
    SHININESS=271
    SPECULAR_COLOR=272
    SPOT_ANGLE=273
    SPOT_EXPONENT=274
    TRANSLATION=275
    VISIBILITY=276

    def __init__(self,aSequence,aProperty):
        M3GObject3D.__init__(self)
        self.ObjectType = 2
        self.keyframeSequence = aSequence #ObjectIndex 
        self.animationController = None #ObjectIndex
        self.propertyID = aProperty #UInt32 
    
    def getData(self):
        data = M3GObject3D.getData(self)
        data += struct.pack("<3I", getId(self.keyframeSequence),
                                   getId(self.animationController),
                                   self.propertyID)
        return data
        
    def getDataLength(self):
        value = M3GObject3D.getDataLength(self)
        return value + struct.calcsize("<3I")
            
    def writeJava(self,aWriter,aCreate):
        if aCreate:
                aWriter.writeClass("AnimationTrack",self)
                #print "self.id,self.keyframeSequence,self.propertyID",self.id,self.keyframeSequence,self.propertyID
                aWriter.write(2,"AnimationTrack BL%i = new AnimationTrack(BL%i,%i);" %
                          (self.id,self.keyframeSequence.id,self.propertyID))
        aWriter.write(2,"BL%i.setController(BL%i);" %
                          (self.id,self.animationController.id))
        M3GObject3D.writeJava(self,aWriter,False)
        
    def searchDeep(self,alist):
        alist = doSearchDeep([self.keyframeSequence, self.animationController],alist)
        return M3GObject3D.searchDeep(self,alist)
       
class M3GKeyframeSequence(M3GObject3D):
    CONSTANT=192
    LINEAR=176
    LOOP=193
    SLERP=177
    SPLINE=178
    SQUAD=179
    STEP=180
        
    def __init__(self,aNumKeyframes, aNumComponents,aBlenderInterpolation,
                      aM3GInterpolation=None):
        M3GObject3D.__init__(self)
        self.ObjectType = 19
        if aM3GInterpolation!=None:
            self.interpolation = aM3GInterpolation
        else:
            if aBlenderInterpolation == "Constant":
                self.interpolation = self.STEP #Byte 
            elif aBlenderInterpolation == "Bezier":
                self.interpolation = self.SPLINE #Byte 
            elif aBlenderInterpolation == "Linear":
                self.interpolation = self.LINEAR #Byte 
            else:
                pass # TODO : Throw Error
        self.repeatMode =  self.CONSTANT #Byte CONSTANT or LOOP
        self.encoding = 0 #Byte 0=raw 
        # TODO: Other encodings
        self.duration = 0 #UInt32 
        self.validRangeFirst = 0 #UInt32 
        self.validRangeLast = 0 #UInt32 
        self.componentCount = aNumComponents #UInt32
        self.keyframeCount = aNumKeyframes #UInt32  
        #IF encoding == 0
        #FOR each key frame...
        self.time = [] #Int32
        self.vectorValue = [] # Float32[componentCount]
        #END
        #ELSE IF encoding == 1
        #Float32[componentCount] vectorBias;
        #Float32[componentCount] vectorScale;
        #FOR each key frame...
        #Int32 time;
        #Byte[componentCount] vectorValue;
        #END
        #ELSE IF encoding == 2
        #Float32[componentCount] vectorBias;
        #Float32[componentCount] vectorScale;
        #FOR each key frame...
        #Int32 time;
        #UInt16[componentCount] vectorValue;
        #END
        #END
        
#All of the vectorValue arrays are the same size, so a separate count is stored outside the individual
#keyframe's data rather than with each array. The encoding field indicates the encoding scheme to be used for the keyframe data. Only the
#nominated values above are allowed. Other values must be treated as errors.

#•Encoding 0 indicates that the values are stored "raw" as floats.
#•Encodings 1 and 2 indicate that the values are quantized to 1 or 2 bytes. For each component,
#a bias and scale are calculated from the sequence of values for that component. The bias is the
#mimimum value, the scale is the maximum value minus the minimum value. The raw values
#are then converted to a value 0..1 by subtracting the bias and dividing by the scale. These raw
#values are then quantized into the range of a Byte or UInt16 by multiplying by 255 or 65535
#respectively. The converse operation restores the original value from the quantized values.

    def beforeExport(self):
        #M3G can not work with negative zero
        #print"beforeExport ID= ",self.id
        for i in range(self.keyframeCount):
            for j in range(self.componentCount):
                x = struct.pack("<f",self.vectorValue[i][j])
                y = struct.unpack("<f",x)
                #print "beforeExport i,j ",i,j,self.vectorValue[i][j],y
                if abs(self.vectorValue[i][j]) < 0.000001 :
                    #print "Negative Zero found!",self.vectorValue[i][j]
                    self.vectorValue[i][j]=0.0
                    #print "zero ",self.vectorValue[i][j]
    
    def getData(self):
        self.beforeExport()
        data = M3GObject3D.getData(self)
        data += struct.pack("<3B5I",self.interpolation, 
                                    self.repeatMode,
                                    self.encoding, 
                                    self.duration, 
                                    self.validRangeFirst, 
                                    self.validRangeLast, 
                                    self.componentCount,
                                    self.keyframeCount) 
        #FOR each key frame...
        for i in range(self.keyframeCount):
            data += struct.pack("<i",self.time[i]) #Int32
            for j in range(self.componentCount):
                data += struct.pack("<f",self.vectorValue[i][j]) # Float32[componentCount]
        return data

    def getDataLength(self):
        value = M3GObject3D.getDataLength(self)
        value += struct.calcsize("<3B5I")
        value += struct.calcsize("<i") * self.keyframeCount
        value += struct.calcsize("<f") * self.keyframeCount * self.componentCount
        return value
        
    def setRepeatMode(self,aBlenderMode):
        if aBlenderMode == "Constant" :
            self.repeatMode = self.CONSTANT
        elif aBlenderMode == "Cyclic":
            self.repeatMode = self.LOOP
        else:
            print "In IPO: Mode " + aBlenderMode + " is not assisted!" 

    def setKeyframe(self, aIndex, aTime, aVector):
        self.time.append(aTime)
        self.vectorValue.append(aVector)
            
    def writeJava(self,aWriter,aCreate):
        self.beforeExport()
        if aCreate:
            aWriter.writeClass("KeyframeSequence",self)
            aWriter.write(2,"KeyframeSequence BL%i = new KeyframeSequence(%i, %i, %i);" %
                            (self.id,self.keyframeCount,self.componentCount,self.interpolation))
            for i in range(len(self.time)):
                lLine = "BL%i.setKeyframe(%i,%i, new float[] { %ff, %ff, %ff" % \
                                        (self.id,i,self.time[i],self.vectorValue[i][0], \
                                          self.vectorValue[i][1],self.vectorValue[i][2])
                if self.componentCount == 4:
                    lLine += ", %ff" % (self.vectorValue[i][3])
                lLine += "});"
                aWriter.write(2,lLine)
                    # TODO : Works only with componentCount = 3
        aWriter.write(2,"BL%i.setDuration(%i);" % (self.id, self.duration))
        aWriter.write(2,"BL%i.setRepeatMode(%i);" % (self.id,self.repeatMode))
        M3GObject3D.writeJava(self,aWriter,False)
            
# ---- Translator -------------------------------------------------------------- #

class M3GTranslator:
    "Trys to translate a blender scene into a mg3 World"
   
    def __init__(self):
        self.world = None
        self.scene = None
        self.nodes = []
    
    def start(self):
        print "Translate started ..."
        
        self.scene = Blender.Scene.GetCurrent()
        self.world = self.translateWorld(self.scene)
        
        for obj in self.scene.objects :
            if obj.getType()=='Camera': #  older Version: isinstance(obj.getData(),Types.CameraType)
                self.translateCamera(obj)
            elif obj.getType()=='Mesh':
                self.translateMesh(obj)
            elif obj.getType()=='Lamp' and mOptions.lightingEnabled:  # older Version: isinstance(obj.getData(),Types.LampType)
                self.translateLamp(obj)
            elif obj.getType()=='Empty':
                self.translateEmpty(obj)
            else:
                print "Warning: could not translate" + str(obj) + ". Try to convert object to mesh using Alt-C"
                
        self.translateParenting()
            
        print "Translate finished."
        return self.world
        
    def translateParenting(self):
        for iNode in self.nodes:
            if iNode.parentBlenderObj == None:
                self.world.children.append(iNode)
            else:
                for jNode in self.nodes:
                    if iNode.parentBlenderObj == jNode.blenderObj:
                        #TODO : Every object can be parent
                        jNode.children.append(iNode)
                        #lMatrix = Matrix(iNode.blenderMatrixWorld) * Matrix(jNode.blenderMatrixWorld).invert()
                        lMatrix = self.calculateChildMatrix(iNode.blenderMatrixWorld,jNode.blenderMatrixWorld)
                        iNode.transform = self.translateMatrix(lMatrix)
                        iNode.hasGeneralTransform=True
                        break
                    
    def calculateChildMatrix(self,child,parent):
        return Matrix(child) * Matrix(parent).invert()
    
    def translateArmature(self,obj,meshObj,aSkinnedMesh):
        print "translate Armature ..."
        #print "translateArmature::aSkinnedMesh.vertexBuffer:",aSkinnedMesh.vertexBuffer
        armature = obj.getData()
        
        #Pose
        #pose = obj.getPose()
        #print "pose ",pose
        #for bone in pose.bones.values():
        #    print "bone local",bone.localMatrix
        #    print "bone pose",bone.poseMatrix
        
        #Skeleton
        mGroup = M3GGroup()
        self.translateCore(obj,mGroup)
        aSkinnedMesh.skeleton = mGroup
        mGroup.transform = self.translateMatrix(
                               self.calculateChildMatrix(obj.matrixWorld,
                                                         meshObj.matrixWorld))
        
        #Bones
        #print "armature:",armature.bones
        for bone in armature.bones.values(): #Copy Bones
            mBone = M3GBone()
            mBone.transformNode = M3GGroup()
            self.translateCore(bone, mBone.transformNode)
            #mBone.transformNode.transform = self.translateMatrix(pose.bones[bone.name].poseMatrix)#Test!!!!
            #print "node transform", mBone.transformNode.transform
            #mBone.transformNode.transform=self.translateMatrix(self.calculateChildMatrix(bone.matrix['ARMATURESPACE'],meshObj.matrixWorld))
            if bone.hasParent():
                mBone.transformNode.transform = self.translateMatrix(
                                                    self.calculateChildMatrix(bone.matrix['ARMATURESPACE'],
                                                                              bone.parent.matrix['ARMATURESPACE']))
            mBone.weight = bone.weight
            aSkinnedMesh.bones[bone.name]=mBone
            
        rootBone = [] #Copy Child-Parent-Structure
        for bone in armature.bones.values():
            mBone = aSkinnedMesh.bones[bone.name]
            if not bone.hasParent(): 
                rootBone.append(mBone)
            if bone.hasChildren():
                for childBone in bone.children:
                    mChildBone = aSkinnedMesh.bones[childBone.name]
                    mBone.transformNode.children.append(mChildBone.transformNode)
        for rbone in rootBone:
            aSkinnedMesh.skeleton.children.append(rbone.transformNode)
        
        #VertexGroups - Skinning
        if armature.vertexGroups:
            for boneName in aSkinnedMesh.bones.keys():
                aSkinnedMesh.bones[boneName].setVerts(self.translateVertsGroup(meshObj.getData(False,True).getVertsFromGroup(boneName),
                                                                              aSkinnedMesh))
        #Envelope - Skinning
        if armature.envelopes:
            pass #TODO
        
        #Action
        self.translateAction(obj,aSkinnedMesh)
        aSkinnedMesh.addSecondBone()    
        
        
    def translateVertsGroup(self,group,aSkinnedMesh):
        #print "group: ",group
        #print "items: ",aSkinnedMesh.getBlenderIndexes().items()
        ergebnis = [int(k) for k,v in aSkinnedMesh.getBlenderIndexes().items() if v in group]
        #print "ergebnis: ",ergebnis
        return ergebnis
    
    def translateAction(self,armatureObj,aSkinnedMesh):
        action = armatureObj.getAction()
        if action==None: return
        
        print "tranlating Action ..."
        if mOptions.exportAllActions:
            lArmatureID = self.translateUserID(armatureObj.getData().name)
            print "armatureID ", lArmatureID, armatureObj
            for a in Blender.Armature.NLA.GetActions().values():
                (lArmatureActionID,lEndFrame,lActionID) = self.translateActionName(a.name)
                #print "action", a
                #print "lArmatureID", lArmatureActionID
                #print "lEndFrame", lEndFrame
                #print "lActionID", lActionID
                if lArmatureActionID == lArmatureID:
                    #print "Action found"
                    mController = self.translateActionIPOs(a,aSkinnedMesh,lEndFrame)
                    mController.userID = lActionID
                    #print "mController.userID ",mController.userID
                    
        #print "getActionScripts() ", Blender.Armature.NLA.getActionStrips()  
        else:
            self.translateActionIPOs(action,aSkinnedMesh)
            
        
    def translateActionIPOs(self,aAction,aSkinnedMesh,aEndFrame=0):
        ipos = aAction.getAllChannelIpos()
        mController=None
        for boneName in aSkinnedMesh.bones.keys():
            if ipos.has_key(boneName):
                ipo = ipos[boneName]
                if mController==None: mController = M3GAnimationController()
                self.translateIpo(ipo,aSkinnedMesh.bones[boneName].transformNode,mController,aEndFrame)
        return mController
    
    def translateActionName(self,name):
        # <Action Name>#A<M3G ID of Armature>E<End Frame>#<ID of Action>
        lError = "Armature name " + name + " is not ok. Perhaps you should set option 'ExportAllAction' to false."
        #print "name ", name
        lLetter = name.find("#")
        if lLetter == -1 :raise Exception(lError)
        if name[lLetter+1]!='A': raise Exception(lError)
        lName = name[lLetter+2:]
        #print "lName ", lName
        lLetter = lName.find("E")
        #print "lLetter ", lLetter
        if lLetter == -1 :raise Exception(lError)
        #print "lName[:]", lName[:0]
        lArmatureID = int(lName[:lLetter])
        lName = lName[lLetter+1:]
        lLetter = lName.find("#")
        if lLetter == -1:raise Exception(lError)
        lEndFrame = int(lName[:lLetter])
        lActionID = int(lName[lLetter+1:])
        return (lArmatureID,lEndFrame,lActionID)

    
    def translateWorld(self,scene):
        "creates world object"
        world = M3GWorld()

        #Background
        world.background = M3GBackground()
        blWorld= scene.world
        #AllWorlds = Blender.World.Get()  # Set Color
        #if len(AllWorlds)>=1:            # world object available
        if blWorld != None:
            world.background.backgroundColor=self.translateRGBA(blWorld.getHor(),0)  # horizon color of the first world
            if mOptions.createAmbientLight & mOptions.lightingEnabled:
                lLight = M3GLight()
                lLight.mode = lLight.modes['AMBIENT']
                lLight.color = self.translateRGB(blWorld.getAmb())
                self.nodes.append(lLight)

        #TODO: Set background picture from world
            
        return world
    
    def translateEmpty(self,obj):
        print "translate empty ..."
        mGroup = M3GGroup()
        self.translateToNode(obj,mGroup)
            
    def translateCamera(self,obj):
        print "translate camera ..."
        camera = obj.getData()
        if camera.getType()!=0:
            print "Only perscpectiv cameras will work korrekt"
            return #Type=0 'perspectiv' Camera will be translated
        mCamera = M3GCamera()
        mCamera.projectionType=mCamera.PERSPECTIVE
        mCamera.fovy=60.0 # TODO: Calculate fovy from Blender.lens 
        mCamera.AspectRatio=4.0/3.0 # TODO: different in every device 
        mCamera.near=camera.getClipStart()
        mCamera.far=camera.getClipEnd()
        self.translateToNode(obj,mCamera)
        self.world.activeCamera = mCamera # Last one is always the active one
    
    
    def translateMaterials(self, aMaterial, aMesh, aMatIndex, createNormals, createUvs):
        print "translate materials ..."
        
        mAppearance = M3GAppearance()
        
        if createNormals:
            mMaterial = M3GMaterial()
            mMaterial.name = aMaterial.name
            mMaterial.diffuseColor = self.translateRGBA(aMaterial.rgbCol,1.0) #ColorRGBA 
            #material.specularColor= self.translateRGB(mat.specCol) #ColorRGB
            mAppearance.material = mMaterial

        if createUvs:
            # Search file name in mesh face.
            lImage = None
            for iface in aMesh.faces:
                if iface.mat==aMatIndex:
                    if iface.image != None:
                        lImage = iface.image
                        break
            if lImage==None:
                raise Exception("Mesh " + aMesh.name + ": No image found for uv-texture! Perhaps no uv-coordinates ?")

            # M3G requires textures to have power-of-two dimensions.
            [width, height] = lImage.getSize()
            powerWidth = 1
            while (powerWidth < width):
                powerWidth *= 2
            powerHeight = 1
            while (powerHeight < height):
                powerHeight *= 2
            if powerWidth != width or powerHeight != height:
                raise Exception("Image " + lImage.filename + ": width and height must be power-of-two!")
                
            # ImageFactory reuses existing images.
            mImage = ImageFactory.getImage(lImage, mOptions.textureExternal)
            mTexture = M3GTexture2D(mImage)
            mAppearance.textures.append(mTexture)

        mPolygonMode=M3GPolygonMode()
        mPolygonMode.perspectiveCorrectionEnabled = mOptions.perspectiveCorrection
        if not aMesh.mode & Modes.TWOSIDED:
            mPolygonMode.culling=M3GPolygonMode.CULL_BACK
        else:
            mPolygonMode.culling=M3GPolygonMode.CULL_NONE 
        if mOptions.smoothShading:
            mPolygonMode.shading=M3GPolygonMode.SHADE_SMOOTH
        else:
            mPolygonMode.shading=M3GPolygonMode.SHADE_FLAT
        
        mAppearance.polygonMode = mPolygonMode
        
        return mAppearance


    def translateMesh(self,obj):
        print "translate mesh ..." + str(obj)

        # Mesh data.
        mesh = obj.getData(False, True)          # get Mesh not NMesh object
        if len(mesh.faces) <= 0:                 # no need to process empty meshes
            print "Empty mesh " + str(obj) + " not processed."
            return
            
        vertexBuffer = M3GVertexBuffer()
        positions = M3GVertexArray(3, 2)         # 3 coordinates - 2 bytes
        if mOptions.autoscaling: positions.useMaxPrecision(mesh.verts)
        indexBuffers = []
        appearances = []
        print str(len(mesh.materials)) + " material(s) found."
        
        # Texture coordinates.
        createUvs = False
        if mOptions.textureEnabled & mesh.faceUV:
            for material in mesh.materials:
                if material.getMode() & Material.Modes.TEXFACE: createUvs = True;
            
        if createUvs:
            if mOptions.autoscaling:
                uvCoordinates = M3GVertexArray(2,2,True,True) #2 coordinates - 2 bytes - autoscaling
            else:
                uvCoordinates = M3GVertexArray(2, 2) #2 coordinates - 2 bytes
                uvCoordinates.bias[0] = 0.5
                uvCoordinates.bias[1] = 0.5
                uvCoordinates.bias[2] = 0.5
                uvCoordinates.scale = 1.0/65535.0
        else:
            uvCoordinates = None

        # Normals.            
        createNormals = False    
        if mOptions.lightingEnabled:
            for material in mesh.materials:
                if not (material.getMode() & Material.Modes.SHADELESS): createNormals = True;

        if createNormals:
            normals = M3GVertexArray(3, 1)       # 3 coordinates - 1 byte
        else:
            normals = None
        
        # Create a submesh for each material. 
        for materialIndex, material in enumerate(mesh.materials):
            faces = [face for face in mesh.faces if face.mat == materialIndex]
            if len(faces) >= 0:
                indexBuffers.append(self.translateFaces(faces, positions, normals, uvCoordinates, createNormals, createUvs))
                appearances.append(self.translateMaterials(material, mesh, materialIndex, createNormals, createUvs))
                
        # If the above didn't result in any IndexBuffer (e.g. there's no material), write a single IndexBuffer 
        # with all faces and a default Appearance.
        if len(indexBuffers) == 0: 
            indexBuffers.append(self.translateFaces(mesh.faces, positions, normals, uvCoordinates, createNormals, createUvs))
            appearances.append(M3GAppearance())

        vertexBuffer.setPositions(positions)
        if createNormals: vertexBuffer.normals = normals
        if createUvs: vertexBuffer.texCoordArrays.append(uvCoordinates)

        parent = obj.getParent()
        if  parent!=None and parent.getType()=='Armature': #Armatures ?
            mMesh = M3GSkinnedMesh(vertexBuffer,indexBuffers,appearances)
            #print"vertexBuffer.positions:",vertexBuffer.positions
            print"mMesh.vertexBuffer:",mMesh.vertexBuffer
            self.translateArmature(parent,obj,mMesh)
        else:
            mMesh = M3GMesh(vertexBuffer,indexBuffers,appearances)
            
        self.translateToNode(obj,mMesh)
        
        #Do Animation
        self.translateObjectIpo(obj,mMesh)  
        
    def translateFaces(self, faces, positions, normals, uvCoordinates, createNormals, createUvs):
        """Translates a list of faces into vertex data and triangle strips."""
        
        # Create vertices and triangle strips.
        indices = [0, 0, 0, 0]
        triangleStrips = M3GTriangleStripArray()
        
        for face in faces:
            for vertexIndex, vertex in enumerate(face.verts):
                # Find candidates for sharing (vertices with same Blender ID).
                vertexCandidateIds = [int(k) for k, v in positions.blenderIndexes.items() if v == vertex.index]

                # Check normal.
                if createNormals and not face.smooth:
                    # For solid faces, a vertex can only be shared if the the face normal is 
                    # the same as the normal of the shared vertex.
                    for candidateId in vertexCandidateIds[:]:
                        for j in range(3):
                            if face.no[j]*127 != normals.components[candidateId*3 + j]:
                                vertexCandidateIds.remove(candidateId)
                                break

                # Check texture coordinates.
                if createUvs:
                    # If texture coordinates are required, a vertex can only be shared if the 
                    # texture coordinates match.
                    for candidateId in vertexCandidateIds[:]:
                        s = int((face.uv[vertexIndex][0]-0.5)*65535)
                        t = int((0.5-face.uv[vertexIndex][1])*65535)
                        if (s != uvCoordinates.components[candidateId*2 + 0]) or (t != uvCoordinates.components[candidateId*2 + 1]):
                            vertexCandidateIds.remove(candidateId)

                if len(vertexCandidateIds) > 0:
                    # Share the vertex.
                    indices[vertexIndex] = vertexCandidateIds[0]
                else:
                    # Create new vertex.
                    positions.append(vertex, vertex.index)
                    indices[vertexIndex] = len(positions.components)/3 - 1

                    # Normal.
                    if createNormals:
                        for j in range(3):
                            if face.smooth:
                                normals.append(int(vertex.no[j]*127))    # vertex normal
                            else:
                                normals.append(int(face.no[j]*127))      # face normal

                    # Texture coordinates.
                    if createUvs:
                        lUvCoordinatesFound = True
                        print "face.uv[vertexIndex][0]:",face.uv[vertexIndex][0]
                        print "face.uv[vertexIndex][1]:",face.uv[vertexIndex][1]
                        if mOptions.autoscaling:
                            uvCoordinates.append(face.uv[vertexIndex][0])
                            uvCoordinates.append(face.uv[vertexIndex][1])
                        else:
                            uvCoordinates.append(int((face.uv[vertexIndex][0]-0.5)*65535))
                            # Reverse t coordinate because M3G uses a different 2D coordinate system than Blender.
                            uvCoordinates.append(int((0.5-face.uv[vertexIndex][1])*65535))

            # IndexBuffer.
            triangleStrips.stripLengths.append(len(face.verts)) 
            if len(face.verts) > 3 :
                triangleStrips.indices += [indices[1], indices[2], indices[0], indices[3]]     # quad
            else :
                triangleStrips.indices += [indices[0], indices[1], indices[2]]                 # tri
                
        return triangleStrips
        
        
    def translateObjectIpo(self,obj,aM3GObject):
        if obj.getIpo() == None : return #No Ipo available
        print "translate Ipo ..."
        
        lIpo = obj.getIpo()
        self.translateIpo(lIpo,aM3GObject)
        
        
    def translateIpo(self,aIpo,aM3GObject,aM3GAnimContr=None,aEndFrame=0):    
        #Print info about curves   
        #for iCurve in lIpo.getCurves():
        #    print "Extrapolation",iCurve.getExtrapolation() #Constant, Extrapolation, Cyclic or Cyclic_extrapolation
        #    print "Interpolation",iCurve.getInterpolation() #Constant, Bezier, or Linear
        #    print "Name",iCurve.getName()
        #    for iPoint in iCurve.getPoints():
        #        print "Knode points",iPoint.getPoints()
        types = ['Loc','Rot','Size','Quat']
        
        for type in types:
            if aIpo.getCurve(type+'X'):
                self.translateIpoCurve(aIpo,aM3GObject,type,aM3GAnimContr,aEndFrame)


    def translateIpoCurve(self,aIpo,aM3GObject,aCurveType,aM3GAnimContr,aEndFrame=0):
        
        lContext = self.scene.getRenderingContext()
        if aEndFrame==0: 
            lEndFrame = lContext.endFrame()
        else:
            lEndFrame = aEndFrame
            
        lTimePerFrame = 1.0 / lContext.framesPerSec() * 1000 
        
        lCurveX = aIpo.getCurve(aCurveType+'X')
        lCurveY = aIpo.getCurve(aCurveType+'Y')
        lCurveZ = aIpo.getCurve(aCurveType+'Z')
        if aCurveType=='Quat': lCurveW = aIpo.getCurve(aCurveType+'W')
        
        lInterpolation = None
        if aCurveType == 'Rot' or aCurveType == 'Quat':
            lTrackType = M3GAnimationTrack.ORIENTATION
            lNumComponents=4
            lCurveFactor= 10 #45 Degrees = 4,5
        if aCurveType == 'Quat':
            lTrackType = M3GAnimationTrack.ORIENTATION
            lNumComponents=4
            lCurveFactor= 1
            lInterpolation = M3GKeyframeSequence.SLERP
            #lInterpolation = M3GKeyframeSequence.SQUAD
        elif aCurveType == 'Size':
            lTrackType = M3GAnimationTrack.SCALE
            lNumComponents=3
            lCurveFactor=1
        else:
            lTrackType = M3GAnimationTrack.TRANSLATION
            lNumComponents=3
            lCurveFactor=1
            
        mSequence = M3GKeyframeSequence(len(lCurveX.getPoints()),
                                        lNumComponents,
                                        lCurveX.getInterpolation(),
                                        lInterpolation)

        #print 'ComponentCount',mSequence.componentCount
        
        mSequence.duration = lEndFrame * lTimePerFrame
        mSequence.setRepeatMode(lCurveX.getExtrapolation())
        
        lIndex = 0
        for iPoint in lCurveX.getPoints():
            lPoint = iPoint.getPoints()
             
            lPointList = [(lPoint[1]*lCurveFactor),
                          (lCurveY.evaluate(lPoint[0])*lCurveFactor),
                          (lCurveZ.evaluate(lPoint[0])*lCurveFactor)]
                        
            #print "aCurveType ", aCurveType
            
            if aCurveType == 'Loc':
                #print "PointList ", lPointList
                #lorgTransVector = aM3GObject.blenderTransformMatrix.translationPart()
                #ltrans = TranslationMatrix(Vector(lPointList))
                #ltrans2 = self.calculateChildMatrix(ltrans,aM3GObject.blenderTransformMatrix)
                #lVector = ltrans2.translationPart() + lorgTransVector
                #lPointList = [lVector.x, lVector.y,lVector.z] 
                #print "PointList ", lPointList
                pass
                           
            if aCurveType == 'Quat':
                lPointList.append(lCurveW.evaluate(lPoint[0])*lCurveFactor)
                #lQuat = Quaternion([lPointList[3],lPointList[0],lPointList[1],lPointList[2]])
                #print "Quat ", lQuat
                #print "Quat.angel ", lQuat.angle
                #print "Quat.axis ", lQuat.axis
                #print "PointList ", lPointList
                
            #print "PointList",lPointList
                                
            if aCurveType =='Rot':
                lQuat = Euler(lPointList).toQuat()
                #lPointList = [lQuat.w,lQuat.x,lQuat.y,lQuat.z]
                lPointList = [lQuat.x,lQuat.y,lQuat.z,lQuat.w]
                #print " Quat=", lPointList
        
            mSequence.setKeyframe(lIndex,
                                  lPoint[0]*lTimePerFrame, 
                                  lPointList)
            lIndex += 1
        mSequence.validRangeFirst = 0 
        mSequence.validRangeLast = lIndex - 1  
        
        mTrack = M3GAnimationTrack(mSequence,lTrackType)
        aM3GObject.animationTracks.append(mTrack)
        if aM3GAnimContr==None:  aM3GAnimContr = M3GAnimationController()
        mTrack.animationController = aM3GAnimContr
        
        
    def translateLamp(self,obj):
        print "translate lamp ..."
        lamp = obj.getData()
        
        #Type
        lampType=lamp.getType()
        if not lampType in [Lamp.Types.Lamp,Lamp.Types.Spot,Lamp.Types.Sun]:
            print "INFO: Type of light is not supported. See documentation"
            return #create not light; type not supported
        mLight = M3GLight()
        if lampType == Lamp.Types.Lamp:
            mLight.mode = mLight.modes['OMNI']
        elif lampType == Lamp.Types.Spot:
            mLight.mode = mLight.modes['SPOT']
        elif lampType == Lamp.Types.Sun:
            mLight.mode = mLight.modes['DIRECTIONAL']
        #Attenuation (OMNI,SPOT):
        if lampType in [Lamp.Types.Lamp,Lamp.Types.Spot]:
            mLight.attenuationConstant = 0.0
            mLight.attenuationLinear = 2.0/lamp.dist 
            mLight.attenuationQuadratic = 0.0 
        #Color
        mLight.color = self.translateRGB(lamp.col)        
        #Energy  
        mLight.intensity = lamp.energy
        #SpotAngle, SpotExponent (SPOT)
        if lampType == Lamp.Types.Spot:
            mLight.spotAngle = lamp.spotSize/2 
            mLight.spotExponent = lamp.spotBlend 
        self.translateToNode(obj,mLight)


    def translateCore(self,obj,node):
        #Name
        node.name = obj.name
        node.userID = self.translateUserID(obj.name)
        #Location
        #node.translation=self.translateLoc(obj.LocX,obj.LocY,obj.LocZ
        #node.hasComponentTransform=True
        #Transform
        #node.transform = self.translateMatrix(obj.getMatrix('localspace'))
        if type(obj) is Types.BoneType:
            #print "BoneMatrix ",obj.matrix['BONESPACE']
            node.transform = self.translateMatrix(obj.matrix['ARMATURESPACE'])
            #'ARMATURESPACE' - this matrix of the bone in relation to the armature 
            #'BONESPACE' - the matrix of the bone in relation to itself
        else:
            node.transform = self.translateMatrix(obj.matrixWorld)
        node.hasGeneralTransform=True
        
        
    def translateToNode(self,obj,node):
        self.translateCore(obj,node)
        #Nodes
        self.nodes.append(node)
        #Link to Blender Object
        node.blenderObj = obj
        node.blenderMatrixWorld = obj.matrixWorld
        lparent = None
        if obj.getParent()!=None:
            if obj.getParent().getType()!='Armature':
                lparent = obj.getParent()
            else:
                if obj.getParent().getParent()!=None and obj.getParent().getParent().getType()!='Armature':
                    lparent = obj.getParent().getParent()
        node.parentBlenderObj = lparent
        
        
    def translateUserID(self, name):
        id = 0
        start = name.find('#')
        
        # Use digits that follow the # sign for id.
        if start != -1:
            start += 1
            end = start
            for char in name[start:]:
                if char.isdigit():
                    end += 1
                else:
                    break
                    
            if end > start:
                id = int(name[start:end])
        
        return id        
        
    def translateLoc(self,aLocX,aLocY,aLocZ):
        return M3GVector3D(aLocX,aLocY,aLocZ)
        
    def translateRGB(self,color):
        return M3GColorRGB(int(color[0]*255),
                            int(color[1]*255), 
                            int(color[2]*255))
    
    def translateRGBA(self,color,alpha):
        return M3GColorRGBA(int(color[0]*255),
                            int(color[1]*255), 
                            int(color[2]*255),
                            int(alpha*255))
    
    def translateMatrix(self,aPyMatrix):
        """
         0   1   2   3 
         4   5   6   7 
         8   9  10  11
         12  13  14  15 """
        #print "Matrix:", aPyMatrix
        lMatrix = M3GMatrix()
        lMatrix.elements[0] = aPyMatrix[0][0]
        lMatrix.elements[1] = aPyMatrix[1][0]
        lMatrix.elements[2] = aPyMatrix[2][0]
        lMatrix.elements[3] = aPyMatrix[3][0]
        lMatrix.elements[4] = aPyMatrix[0][1]
        lMatrix.elements[5] = aPyMatrix[1][1]
        lMatrix.elements[6] = aPyMatrix[2][1]
        lMatrix.elements[7] = aPyMatrix[3][1]
        lMatrix.elements[8] = aPyMatrix[0][2]
        lMatrix.elements[9] = aPyMatrix[1][2]
        lMatrix.elements[10] = aPyMatrix[2][2]
        lMatrix.elements[11] = aPyMatrix[3][2]
        lMatrix.elements[12] = aPyMatrix[0][3]
        lMatrix.elements[13] = aPyMatrix[1][3]
        lMatrix.elements[14] = aPyMatrix[2][3]
        lMatrix.elements[15] = aPyMatrix[3][3]
        return lMatrix
        
    
# ---- Exporter ---------------------------------------------------------------- #

class M3GExporter:
    "Exports Blender-Scene to M3D"
    def __init__(self, aWriter): 
        self.writer = aWriter

        
    def start(self):
        print "Info: starting export ..."
        #rpdb2.start_embedded_debugger("t",True)
        Translator = M3GTranslator()
        world = Translator.start()
        
        #sys.settrace(tracer)
        exportList = self.createDeepSearchList(world)
        externalReferences = [element for element in exportList if element.__class__ is M3GExternalReference]
        exportList = [element for element in exportList if element.__class__ is  not M3GExternalReference]
        #sys.settrace(None)
        
        # 1 is reservated for HeaderObject.
        i=1 
        
        # Next are the external references.
        for element in externalReferences:
            i += 1
            element.id = i
            print "object ",element.id, element
            
        # And the standard scene objects.
        for element in exportList:
            i += 1
            element.id = i
            print "object ",element.id, element
            
        self.writer.writeFile(world, exportList, externalReferences)
        
        print("Ready!")


    def createDeepSearchList(self,aWorld):
        "creates the right order for saving m3g : leafs first"
        return aWorld.searchDeep([])
         
   
       
# ---- Writer ---------------------------------------------------------------- #   
class JavaWriter:
    "writes a java class which creates m3g-Scene in a j2me programm"
    def __init__(self,aFilename):
        self.filename = aFilename
        self.classname = Blender.sys.basename(aFilename)
        self.classname = self.classname[:-5] #without extention ".java"
        self.outFile = file(aFilename,"w")
        
    def write(self, tab, zeile=""):
        "writes to file"
        #print "\t" * tab + zeile
        print >>self.outFile, "\t" * tab + zeile

    def writeFile(self,aWorld,aExportList,externalReferences):
        self.world = aWorld
        self.writeHeader()
        for element in aExportList:
            element.writeJava(self,True)
        self.writeFooter()
        self.outFile.close()
        
    def writeHeader(self):
        "writes class header"
        self.write(0,"import javax.microedition.lcdui.Image;")
        self.write(0,"import javax.microedition.m3g.*;")
        self.write(0,"public final class "+self.classname+" {")
        self.write(1,"public static World getRoot(Canvas3D aCanvas) {")
          
    def writeFooter(self):
        self.write(1)
        self.write(1,"return BL"+str(self.world.id)+";")
        self.write(0,"}}")
        
    def writeList(self,alist,numberOfElementsPerLine=12,aType=""):
        '''Writes numberOfElementsPerLine'''
        line=""
        lastLine=""
        counter=0
        for element in alist:
            if counter!=0:
                line = line + "," + str(element) + aType
            else:
                line = str(element) + aType
            counter = counter + 1
            if counter == numberOfElementsPerLine:
                if len(lastLine)>0:
                    self.write(3,lastLine+",")
                lastLine=line
                line=""
                counter = 0
        if len(lastLine)>0:
            if len(line)>0:
                self.write(3,lastLine+",")
            else:
                self.write(3,lastLine)
        if len(line) > 0: self.write(3,line)
    
    def writeClass(self,aName,aM3GObject):
        self.write(2)
        self.write(2,"//"+aName+":"+aM3GObject.name)
      

class M3GSectionObject:
    def __init__(self,aObject):
        """Object Structure
           Each object in the file represents one object in the 
           scene graph tree, and is stored in a chunk. The
           structure of an object chunk is as follows:
               Byte ObjectType
               UInt32 Length
               Byte[] Data"""
        #ObjectType
        #This field describes what type of object has been serialized.
        #The values 0 and 0xFF are special: 0 represents the header object, 
        #and 0xFF represents an external reference.
        #Example: Byte ObjectType = 14
        self.ObjectType = aObject.ObjectType
        self.data = aObject.getData()
        self.length = aObject.getDataLength()
    
    def getData(self):
        data = struct.pack('<BI',self.ObjectType,self.length)
        data += self.data
        return data
    
    def getDataLength(self):
        return struct.calcsize('<BI') + self.length
        
class M3GSection:
    '''Part of a M3G binary file'
       Section Structur
           Byte CompressionScheme
           UInt32 TotalSectionLength
           UInt32 UncompressedLength
           Byte[] Objects
           UInt32 Checksum '''
    def __init__(self,aObjectList,compressed=False):
        self.CompressionScheme=0
        self.TotalSectionLength=0 #including the CompressionScheme, 
                                  #TotalSectionLength, UncompressedLength,
                                  #Objects and Checksum fields,
        self.UncompressedLength=0 #Length of Objects uncompresses
        self.Objects = ''
        for element in aObjectList:
            lObject = M3GSectionObject(element)
            #print "Obj:", lObject, lObject.getDataLength(), len(lObject.getData())
            self.Objects += lObject.getData()
            self.UncompressedLength += lObject.getDataLength()
        self.TotalSectionLength=struct.calcsize('<BIII')+self.UncompressedLength
    
    def getData(self):
        data = struct.pack('<BII',self.CompressionScheme,
                                self.TotalSectionLength,
                                self.UncompressedLength)
        data += self.Objects
        #self.Checksum = zlib.adler32(data)
        self.Checksum = self.ownAdler32(data)
        print "Checksum",self.Checksum
        #print "Own Checksum",self.ownAdler32(data)
        return data + struct.pack('<I',self.Checksum)
    
    def ownAdler32(self,data):
        s1 = int(1) #uint32_t
        s2 = int(0) #uint32_t
        for n in data:
            s1 = (s1 + int(struct.unpack("B",n)[0])) % 65521
            s2 = (s2 + s1) % 65521
        return (s2 << 16) + s1
    
    def getLength(self):
        return self.TotalSectionLength
        
    def write(self,aFile):
        print "Write Section.."
        print "TotalSectionLength:", str(self.TotalSectionLength)
        aFile.write(self.getData())
            
        #CompressionScheme
        #Example Byte CompressionScheme = 1;
        #0 Uncompressed, Adler32 Checksum
        #1 ZLib compression, 32 k buffer size, Adler32 Checksum
        #2...255 Reserved
        #TotalSectionLength: total length of the section in bytes
        #Example: UInt32 TotalSectionLength = 2056
        #UncompressedLength 
        #contains the length of the Objects field after decompression. 
        #If no compression is specified for this section,
        #this equals the actual number of bytes serialized in the Objects array.
        #CompressionScheme
        #Currently, only the Adler32 checksum is mandatory. 
        #The checksum is calculated using all preceding bytes in
        #the section, i.e. the CompressionScheme,TotalSectionLength, 
        #UncompressedLength, and the actual serialized data in the 
        #Objects field (i.e. in its compressed form if compression is speci-
        #fied).
        #Example: UInt32 Checksum = 0xffe806a3
    
    
    
    
class M3GFileIdentifier:
    def __init__(self):
        '''Byte[12] FileIdentifier = { 0xAB, 0x4A, 0x53, 0x52, 0x31, 0x38, 0x34,
                        0xBB, 0x0D, 0x0A, 0x1A, 0x0A }
           This can also be expressed using C-style character definitions as:
           Byte[12] FileIdentifier = { '«', 'J', 'S', 'R', '1', '8', '4', '»', '\r',
                        '\n', '\x1A', '\n' }'''
        self.data = [ 0xAB, 0x4A, 0x53, 0x52, 0x31, 0x38, 0x34,
                        0xBB, 0x0D, 0x0A, 0x1A, 0x0A ]
    
    def write(self,aFile):
        for element in self.data:
            aFile.write(struct.pack('B',element))
        
    def getLength(self):
        return len(self.data)
        
        
class M3GWriter:
    """writes a m3g binary file
      File Structur
        File Identifier
        Section 0 File Header Object
        Section 1 External Reference Objects
        Section 2 Scene Objects
        Section 3 Scene Objects
                ... ...
        Section n Scene Objects"""
        
    def __init__(self,aFilename):
        self.FileName = aFilename
    
    
    def writeFile(self,aWorld,aExportList,externalReferences):
        '''Called after translation
           first atempt: all objects in one section'''
        print "M3G file writing .."
        
        fileIdentifier = M3GFileIdentifier()
        
        fileHeaderObject = M3GHeaderObject()
        section0 = M3GSection([fileHeaderObject])
        sectionN = M3GSection(aExportList)
        
        length = fileIdentifier.getLength()
        length += section0.getLength()
        length += sectionN.getLength()
        
        if len(externalReferences) != 0:
            section1 = M3GSection(externalReferences)
            length += section1.getLength()
            fileHeaderObject.hasExternalReferences = True
        
        fileHeaderObject.TotalFileSize=length 
        fileHeaderObject.ApproximateContentSize=length
        section0 = M3GSection([fileHeaderObject])
       
        output = open(self.FileName, mode='wb')

        fileIdentifier.write(output)
        section0.write(output)
        if len(externalReferences) != 0:
            section1.write(output)
        sectionN.write(output)
        
        output.close()

        print "M3G file written."
   
 
class OptionMgr:
    """Reads and saves options """

    def __init__(self):
        self.setDefault()
        rdict = Registry.GetKey('M3GExport', True) # True to check on disk as well
        if rdict: # if found, get the values saved there
            try:
                self.textureEnabled        = rdict['textureEnabled']
                self.textureExternal       = rdict['textureExternal']
                self.lightingEnabled       = rdict['lightingEnabled']
                self.createAmbientLight    = rdict['createAmbientLight']
                self.exportAllActions      = rdict['exportAllActions']
                self.autoscaling           = rdict['autoscaling']
                self.perspectiveCorrection = rdict['perspectiveCorrection']
                self.smoothShading         = rdict['smoothShading']
                self.exportAsJava          = rdict['exportAsJava']
                self.exportVersion2        = rdict['exportVersion2']
                self.exportGamePhysics     = rdict['exportGamePhysics']
                
            except: self.save()     # if data isn't valid, rewrite it

    def setDefault(self):
        self.textureEnabled         = True
        self.textureExternal        = False
        self.lightingEnabled        = True
        self.createAmbientLight     = False
        self.exportAllActions       = False
        self.autoscaling            = True
        self.perspectiveCorrection  = False
        self.smoothShading          = True
        self.exportAsJava           = False
        self.exportVersion2         = False
        self.exportGamePhysics      = False
    
    def save(self):
        d = {}
        d['textureEnabled']        = self.textureEnabled
        d['textureExternal']       = self.textureExternal
        d['lightingEnabled']       = self.lightingEnabled
        d['createAmbientLight']    = self.createAmbientLight
        d['exportAllActions']      = self.exportAllActions
        d['autoscaling']           = self.autoscaling
        d['perspectiveCorrection'] = self.perspectiveCorrection
        d['smoothShading']         = self.smoothShading
        d['exportAsJava']          = self.exportAsJava
        d['exportVersion2']        = self.exportVersion2
        d['exportGamePhysics']     = self.exportGamePhysics
        
        Blender.Registry.SetKey('M3GExport', d, True)


# ---- User Interface -------------------------------------------------------- #
mOptions = OptionMgr()

def gui():              
    """Draws the options menu."""
    # Flush events.
    for s in Window.GetScreenInfo():
        Window.QHandle(s['id'])
        
    # Display options.
    textureEnabled         = Draw.Create(mOptions.textureEnabled)
    textureExternal        = Draw.Create(mOptions.textureExternal)
    lightingEnabled        = Draw.Create(mOptions.lightingEnabled)
    createAmbientLight     = Draw.Create(mOptions.createAmbientLight)
    exportAllActions       = Draw.Create(mOptions.exportAllActions)
    autoscaling            = Draw.Create(mOptions.autoscaling)
    perspectiveCorrection  = Draw.Create(mOptions.perspectiveCorrection)
    smoothShading          = Draw.Create(mOptions.smoothShading)
    exportAsJava           = Draw.Create(mOptions.exportAsJava)
    exportVersion2         = Draw.Create(mOptions.exportVersion2)
    exportGamePhysics      = Draw.Create(mOptions.exportGamePhysics)
    
    pupBlock = [\
        ('Texturing'),\
        ('Enabled',              textureEnabled,         'Enables texture export'),\
        ('External',             textureExternal,        'References external files for textures'),\
        ('Lighting'),\
        ('Enabled',              lightingEnabled,        'Enables light export'),\
        ('Ambient Light',        createAmbientLight,     'Inserts an extra light object for ambient light'),\
        ('Mesh Options'),\
        ('Autoscaling',          autoscaling,            'Uses maximum precision for vertex positions'),\
        ('Persp. Correction',    perspectiveCorrection,  'Sets perspective correction flag'),\
        ('Smooth Shading',       smoothShading,          'Sets smooth shading flag'),\
        ('Posing'),\
        ('All Armature Actions', exportAllActions,       'Exports all actions for armatures'),\
        ('Export'),\
        ('As Java Source',       exportAsJava,           'Exports scene as Java source code'),\
        ('M3G Version 2.0',      exportVersion2,         'Exports M3G Version 2.0 File'),\
        ('Game Physics',         exportGamePhysics,      'Includes Game Physics infos for NOPE in export')
    ]
    
    # Only execute if use didn't quit (ESC).
    if Draw.PupBlock('M3G Export', pupBlock):
        mOptions.textureEnabled         = textureEnabled.val
        mOptions.textureExternal        = textureExternal.val
        mOptions.lightingEnabled        = lightingEnabled.val
        mOptions.createAmbientLight     = createAmbientLight.val
        mOptions.exportAllActions       = exportAllActions.val
        mOptions.autoscaling            = autoscaling.val
        mOptions.perspectiveCorrection  = perspectiveCorrection.val
        mOptions.smoothShading          = smoothShading.val
        mOptions.exportAsJava           = exportAsJava.val
        mOptions.exportVersion2         = exportVersion2.val
        mOptions.exportGamePhysics      = exportGamePhysics.val
        mOptions.save()

        if mOptions.exportAsJava:
            Window.FileSelector(file_callback_java, 'Export M3G as Java', Blender.sys.makename(ext='.java'))
        else:
            Window.FileSelector(file_callback_m3g,  'Export M3G Binary',  Blender.sys.makename(ext='.m3g'))
        
def file_callback_java(filename):
    Window.WaitCursor(1)    # Blender will automatically remove wait cursor in case of an exception
    exporter=M3GExporter(JavaWriter(filename))
    exporter.start()
    Window.WaitCursor(0)
    Window.RedrawAll()

def file_callback_m3g(filename):
    Window.WaitCursor(1)
    exporter=M3GExporter(M3GWriter(filename))
    exporter.start()
    Window.WaitCursor(0)
    Window.RedrawAll()
    
if __name__ == '__main__':
    gui()

