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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

__author__ = ("Bart", "Campbell Barton")
__email__ = ["Bart, bart:neeneenee*de"]
__url__ = ["Author's (Bart) homepage, http://www.neeneenee.de/vrml"]
__version__ = "2006/01/17"
__bpydoc__ = """\
This script exports to X3D format.

Usage:

Run this script from "File->Export" menu.  A pop-up will ask whether you
want to export only selected or all relevant objects.

Known issues:<br>
    Doesn't handle multiple materials (don't use material indices);<br>
    Doesn't handle multiple UV textures on a single mesh (create a mesh for each texture);<br>
    Can't get the texture array associated with material * not the UV ones;
"""


# $Id$
#
#------------------------------------------------------------------------
# X3D exporter for blender 2.36 or above
#
# ***** BEGIN GPL LICENSE BLOCK *****
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
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
#

####################################
# Library dependancies
####################################

import math
import os

import bpy
import Mathutils

from export_3ds import create_derived_objects, free_derived_objects

# import Blender
# from Blender import Object, Lamp, Draw, Image, Text, sys, Mesh
# from Blender.Scene import Render
# import BPyObject
# import BPyMesh

#
DEG2RAD=0.017453292519943295
MATWORLD= Mathutils.RotationMatrix(-90, 4, 'x')

####################################
# Global Variables
####################################

filename = ""
# filename = Blender.Get('filename')
_safeOverwrite = True

extension = ''

##########################################################
# Functions for writing output file
##########################################################

class x3d_class:

    def __init__(self, filename):
        #--- public you can change these ---
        self.writingcolor = 0
        self.writingtexture = 0
        self.writingcoords = 0
        self.proto = 1
        self.matonly = 0
        self.share = 0
        self.billnode = 0
        self.halonode = 0
        self.collnode = 0
        self.tilenode = 0
        self.verbose=2	 # level of verbosity in console 0-none, 1-some, 2-most
        self.cp=3		  # decimals for material color values	 0.000 - 1.000
        self.vp=3		  # decimals for vertex coordinate values  0.000 - n.000
        self.tp=3		  # decimals for texture coordinate values 0.000 - 1.000
        self.it=3

        #--- class private don't touch ---
        self.texNames={}   # dictionary of textureNames
        self.matNames={}   # dictionary of materiaNames
        self.meshNames={}   # dictionary of meshNames
        self.indentLevel=0 # keeps track of current indenting
        self.filename=filename
        self.file = None
        if filename.lower().endswith('.x3dz'):
            try:
                import gzip
                self.file = gzip.open(filename, "w")
            except:
                print("failed to import compression modules, exporting uncompressed")
                self.filename = filename[:-1] # remove trailing z

        if self.file == None:
            self.file = open(self.filename, "w")

        self.bNav=0
        self.nodeID=0
        self.namesReserved=[ "Anchor","Appearance","Arc2D","ArcClose2D","AudioClip","Background","Billboard",
                             "BooleanFilter","BooleanSequencer","BooleanToggle","BooleanTrigger","Box","Circle2D",
                             "Collision","Color","ColorInterpolator","ColorRGBA","component","Cone","connect",
                             "Contour2D","ContourPolyline2D","Coordinate","CoordinateDouble","CoordinateInterpolator",
                             "CoordinateInterpolator2D","Cylinder","CylinderSensor","DirectionalLight","Disk2D",
                             "ElevationGrid","EspduTransform","EXPORT","ExternProtoDeclare","Extrusion","field",
                             "fieldValue","FillProperties","Fog","FontStyle","GeoCoordinate","GeoElevationGrid",
                             "GeoLocationLocation","GeoLOD","GeoMetadata","GeoOrigin","GeoPositionInterpolator",
                             "GeoTouchSensor","GeoViewpoint","Group","HAnimDisplacer","HAnimHumanoid","HAnimJoint",
                             "HAnimSegment","HAnimSite","head","ImageTexture","IMPORT","IndexedFaceSet",
                             "IndexedLineSet","IndexedTriangleFanSet","IndexedTriangleSet","IndexedTriangleStripSet",
                             "Inline","IntegerSequencer","IntegerTrigger","IS","KeySensor","LineProperties","LineSet",
                             "LoadSensor","LOD","Material","meta","MetadataDouble","MetadataFloat","MetadataInteger",
                             "MetadataSet","MetadataString","MovieTexture","MultiTexture","MultiTextureCoordinate",
                             "MultiTextureTransform","NavigationInfo","Normal","NormalInterpolator","NurbsCurve",
                             "NurbsCurve2D","NurbsOrientationInterpolator","NurbsPatchSurface",
                             "NurbsPositionInterpolator","NurbsSet","NurbsSurfaceInterpolator","NurbsSweptSurface",
                             "NurbsSwungSurface","NurbsTextureCoordinate","NurbsTrimmedSurface","OrientationInterpolator",
                             "PixelTexture","PlaneSensor","PointLight","PointSet","Polyline2D","Polypoint2D",
                             "PositionInterpolator","PositionInterpolator2D","ProtoBody","ProtoDeclare","ProtoInstance",
                             "ProtoInterface","ProximitySensor","ReceiverPdu","Rectangle2D","ROUTE","ScalarInterpolator",
                             "Scene","Script","Shape","SignalPdu","Sound","Sphere","SphereSensor","SpotLight","StaticGroup",
                             "StringSensor","Switch","Text","TextureBackground","TextureCoordinate","TextureCoordinateGenerator",
                             "TextureTransform","TimeSensor","TimeTrigger","TouchSensor","Transform","TransmitterPdu",
                             "TriangleFanSet","TriangleSet","TriangleSet2D","TriangleStripSet","Viewpoint","VisibilitySensor",
                             "WorldInfo","X3D","XvlShell","VertexShader","FragmentShader","MultiShaderAppearance","ShaderAppearance" ]
        self.namesStandard=[ "Empty","Empty.000","Empty.001","Empty.002","Empty.003","Empty.004","Empty.005",
                             "Empty.006","Empty.007","Empty.008","Empty.009","Empty.010","Empty.011","Empty.012",
                             "Scene.001","Scene.002","Scene.003","Scene.004","Scene.005","Scene.06","Scene.013",
                             "Scene.006","Scene.007","Scene.008","Scene.009","Scene.010","Scene.011","Scene.012",
                             "World","World.000","World.001","World.002","World.003","World.004","World.005" ]
        self.namesFog=[ "","LINEAR","EXPONENTIAL","" ]

##########################################################
# Writing nodes routines
##########################################################

    def writeHeader(self):
        #bfile = sys.expandpath( Blender.Get('filename') ).replace('<', '&lt').replace('>', '&gt')
        bfile = self.filename.replace('<', '&lt').replace('>', '&gt') # use outfile name
        self.file.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")
        self.file.write("<!DOCTYPE X3D PUBLIC \"ISO//Web3D//DTD X3D 3.0//EN\" \"http://www.web3d.org/specifications/x3d-3.0.dtd\">\n")
        self.file.write("<X3D version=\"3.0\" profile=\"Immersive\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema-instance\" xsd:noNamespaceSchemaLocation=\"http://www.web3d.org/specifications/x3d-3.0.xsd\">\n")
        self.file.write("<head>\n")
        self.file.write("\t<meta name=\"filename\" content=\"%s\" />\n" % os.path.basename(bfile))
        # self.file.write("\t<meta name=\"filename\" content=\"%s\" />\n" % sys.basename(bfile))
        self.file.write("\t<meta name=\"generator\" content=\"Blender %s\" />\n" % '2.5')
        # self.file.write("\t<meta name=\"generator\" content=\"Blender %s\" />\n" % Blender.Get('version'))
        self.file.write("\t<meta name=\"translator\" content=\"X3D exporter v1.55 (2006/01/17)\" />\n")
        self.file.write("</head>\n")
        self.file.write("<Scene>\n")

    # This functionality is poorly defined, disabling for now - campbell
    '''
    def writeInline(self):
        inlines = Blender.Scene.Get()
        allinlines = len(inlines)
        if scene != inlines[0]:
            return
        else:
            for i in xrange(allinlines):
                nameinline=inlines[i].name
                if (nameinline not in self.namesStandard) and (i > 0):
                    self.file.write("<Inline DEF=\"%s\" " % (self.cleanStr(nameinline)))
                    nameinline = nameinline+".x3d"
                    self.file.write("url=\"%s\" />" % nameinline)
                    self.file.write("\n\n")


    def writeScript(self):
        textEditor = Blender.Text.Get()
        alltext = len(textEditor)
        for i in xrange(alltext):
            nametext = textEditor[i].name
            nlines = textEditor[i].getNLines()
            if (self.proto == 1):
                if (nametext == "proto" or nametext == "proto.js" or nametext == "proto.txt") and (nlines != None):
                    nalllines = len(textEditor[i].asLines())
                    alllines = textEditor[i].asLines()
                    for j in xrange(nalllines):
                        self.writeIndented(alllines[j] + "\n")
            elif (self.proto == 0):
                if (nametext == "route" or nametext == "route.js" or nametext == "route.txt") and (nlines != None):
                    nalllines = len(textEditor[i].asLines())
                    alllines = textEditor[i].asLines()
                    for j in xrange(nalllines):
                        self.writeIndented(alllines[j] + "\n")
        self.writeIndented("\n")
    '''

    def writeViewpoint(self, ob, mat, scene):
        context = scene.render_data
        # context = scene.render
        ratio = float(context.resolution_x)/float(context.resolution_y)
        # ratio = float(context.imageSizeY())/float(context.imageSizeX())
        lens = (360* (math.atan(ratio *16 / ob.data.lens) / math.pi))*(math.pi/180)
        # lens = (360* (math.atan(ratio *16 / ob.data.getLens()) / math.pi))*(math.pi/180)
        lens = min(lens, math.pi)

        # get the camera location, subtract 90 degress from X to orient like X3D does
        # mat = ob.matrixWorld - mat is now passed!

        loc = self.rotatePointForVRML(mat.translationPart())
        rot = mat.toEuler()
        rot = (((rot[0]-90)), rot[1], rot[2])
        # rot = (((rot[0]-90)*DEG2RAD), rot[1]*DEG2RAD, rot[2]*DEG2RAD)
        nRot = self.rotatePointForVRML( rot )
        # convert to Quaternion and to Angle Axis
        Q  = self.eulerToQuaternions(nRot[0], nRot[1], nRot[2])
        Q1 = self.multiplyQuaternions(Q[0], Q[1])
        Qf = self.multiplyQuaternions(Q1, Q[2])
        angleAxis = self.quaternionToAngleAxis(Qf)
        self.file.write("<Viewpoint DEF=\"%s\" " % (self.cleanStr(ob.name)))
        self.file.write("description=\"%s\" " % (ob.name))
        self.file.write("centerOfRotation=\"0 0 0\" ")
        self.file.write("position=\"%3.2f %3.2f %3.2f\" " % (loc[0], loc[1], loc[2]))
        self.file.write("orientation=\"%3.2f %3.2f %3.2f %3.2f\" " % (angleAxis[0], angleAxis[1], -angleAxis[2], angleAxis[3]))
        self.file.write("fieldOfView=\"%.3f\" />\n\n" % (lens))

    def writeFog(self, world):
        if world:
            mtype = world.mist.falloff
            # mtype = world.getMistype()
            mparam = world.mist
            # mparam = world.getMist()
            grd = world.horizon_color
            # grd = world.getHor()
            grd0, grd1, grd2 = grd[0], grd[1], grd[2]
        else:
            return
        if (mtype == 'LINEAR' or mtype == 'INVERSE_QUADRATIC'):
            mtype = 1 if mtype == 'LINEAR' else 2
        # if (mtype == 1 or mtype == 2):
            self.file.write("<Fog fogType=\"%s\" " % self.namesFog[mtype])
            self.file.write("color=\"%s %s %s\" " % (round(grd0,self.cp), round(grd1,self.cp), round(grd2,self.cp)))
            self.file.write("visibilityRange=\"%s\" />\n\n" % round(mparam[2],self.cp))
        else:
            return

    def writeNavigationInfo(self, scene):
        self.file.write('<NavigationInfo headlight="FALSE" visibilityLimit="0.0" type=\'"EXAMINE","ANY"\' avatarSize="0.25, 1.75, 0.75" />\n')

    def writeSpotLight(self, ob, mtx, lamp, world):
        safeName = self.cleanStr(ob.name)
        if world:
            ambi = world.ambient_color
            # ambi = world.amb
            ambientIntensity = ((float(ambi[0] + ambi[1] + ambi[2]))/3)/2.5
        else:
            ambi = 0
            ambientIntensity = 0

        # compute cutoff and beamwidth
        intensity=min(lamp.energy/1.75,1.0)
        beamWidth=((lamp.spot_size*math.pi)/180.0)*.37;
        # beamWidth=((lamp.spotSize*math.pi)/180.0)*.37;
        cutOffAngle=beamWidth*1.3

        dx,dy,dz=self.computeDirection(mtx)
        # note -dx seems to equal om[3][0]
        # note -dz seems to equal om[3][1]
        # note  dy seems to equal om[3][2]

        #location=(ob.matrixWorld*MATWORLD).translationPart() # now passed
        location=(mtx*MATWORLD).translationPart()

        radius = lamp.distance*math.cos(beamWidth)
        # radius = lamp.dist*math.cos(beamWidth)
        self.file.write("<SpotLight DEF=\"%s\" " % safeName)
        self.file.write("radius=\"%s\" " % (round(radius,self.cp)))
        self.file.write("ambientIntensity=\"%s\" " % (round(ambientIntensity,self.cp)))
        self.file.write("intensity=\"%s\" " % (round(intensity,self.cp)))
        self.file.write("color=\"%s %s %s\" " % (round(lamp.color[0],self.cp), round(lamp.color[1],self.cp), round(lamp.color[2],self.cp)))
        # self.file.write("color=\"%s %s %s\" " % (round(lamp.col[0],self.cp), round(lamp.col[1],self.cp), round(lamp.col[2],self.cp)))
        self.file.write("beamWidth=\"%s\" " % (round(beamWidth,self.cp)))
        self.file.write("cutOffAngle=\"%s\" " % (round(cutOffAngle,self.cp)))
        self.file.write("direction=\"%s %s %s\" " % (round(dx,3),round(dy,3),round(dz,3)))
        self.file.write("location=\"%s %s %s\" />\n\n" % (round(location[0],3), round(location[1],3), round(location[2],3)))


    def writeDirectionalLight(self, ob, mtx, lamp, world):
        safeName = self.cleanStr(ob.name)
        if world:
            ambi = world.ambient_color
            # ambi = world.amb
            ambientIntensity = ((float(ambi[0] + ambi[1] + ambi[2]))/3)/2.5
        else:
            ambi = 0
            ambientIntensity = 0

        intensity=min(lamp.energy/1.75,1.0)
        (dx,dy,dz)=self.computeDirection(mtx)
        self.file.write("<DirectionalLight DEF=\"%s\" " % safeName)
        self.file.write("ambientIntensity=\"%s\" " % (round(ambientIntensity,self.cp)))
        self.file.write("color=\"%s %s %s\" " % (round(lamp.color[0],self.cp), round(lamp.color[1],self.cp), round(lamp.color[2],self.cp)))
        # self.file.write("color=\"%s %s %s\" " % (round(lamp.col[0],self.cp), round(lamp.col[1],self.cp), round(lamp.col[2],self.cp)))
        self.file.write("intensity=\"%s\" " % (round(intensity,self.cp)))
        self.file.write("direction=\"%s %s %s\" />\n\n" % (round(dx,4),round(dy,4),round(dz,4)))

    def writePointLight(self, ob, mtx, lamp, world):
        safeName = self.cleanStr(ob.name)
        if world:
            ambi = world.ambient_color
            # ambi = world.amb
            ambientIntensity = ((float(ambi[0] + ambi[1] + ambi[2]))/3)/2.5
        else:
            ambi = 0
            ambientIntensity = 0

        # location=(ob.matrixWorld*MATWORLD).translationPart() # now passed
        location= (mtx*MATWORLD).translationPart()

        self.file.write("<PointLight DEF=\"%s\" " % safeName)
        self.file.write("ambientIntensity=\"%s\" " % (round(ambientIntensity,self.cp)))
        self.file.write("color=\"%s %s %s\" " % (round(lamp.color[0],self.cp), round(lamp.color[1],self.cp), round(lamp.color[2],self.cp)))
        # self.file.write("color=\"%s %s %s\" " % (round(lamp.col[0],self.cp), round(lamp.col[1],self.cp), round(lamp.col[2],self.cp)))
        self.file.write("intensity=\"%s\" " % (round( min(lamp.energy/1.75,1.0) ,self.cp)))
        self.file.write("radius=\"%s\" " % lamp.distance )
        # self.file.write("radius=\"%s\" " % lamp.dist )
        self.file.write("location=\"%s %s %s\" />\n\n" % (round(location[0],3), round(location[1],3), round(location[2],3)))
    '''
    def writeNode(self, ob, mtx):
        obname=str(ob.name)
        if obname in self.namesStandard:
            return
        else:
            dx,dy,dz = self.computeDirection(mtx)
            # location=(ob.matrixWorld*MATWORLD).translationPart()
            location=(mtx*MATWORLD).translationPart()
            self.writeIndented("<%s\n" % obname,1)
            self.writeIndented("direction=\"%s %s %s\"\n" % (round(dx,3),round(dy,3),round(dz,3)))
            self.writeIndented("location=\"%s %s %s\"\n" % (round(location[0],3), round(location[1],3), round(location[2],3)))
            self.writeIndented("/>\n",-1)
            self.writeIndented("\n")
    '''
    def secureName(self, name):
        name = name + str(self.nodeID)
        self.nodeID=self.nodeID+1
        if len(name) <= 3:
            newname = "_" + str(self.nodeID)
            return "%s" % (newname)
        else:
            for bad in ['"','#',"'",',','.','[','\\',']','{','}']:
                name=name.replace(bad,'_')
            if name in self.namesReserved:
                newname = name[0:3] + "_" + str(self.nodeID)
                return "%s" % (newname)
            elif name[0].isdigit():
                newname = "_" + name + str(self.nodeID)
                return "%s" % (newname)
            else:
                newname = name
                return "%s" % (newname)

    def writeIndexedFaceSet(self, ob, mesh, mtx, world, EXPORT_TRI = False):
        imageMap={}   # set of used images
        sided={}	  # 'one':cnt , 'two':cnt
        vColors={}	# 'multi':1
        meshName = self.cleanStr(ob.name)

        meshME = self.cleanStr(ob.data.name) # We dont care if its the mesh name or not
        # meshME = self.cleanStr(ob.getData(mesh=1).name) # We dont care if its the mesh name or not
        if len(mesh.faces) == 0: return
        mode = []
        # mode = 0
        if mesh.active_uv_texture:
        # if mesh.faceUV:
            for face in mesh.active_uv_texture.data:
            # for face in mesh.faces:
                if face.halo and 'HALO' not in mode:
                    mode += ['HALO']
                if face.billboard and 'BILLBOARD' not in mode:
                    mode += ['BILLBOARD']
                if face.object_color and 'OBJECT_COLOR' not in mode:
                    mode += ['OBJECT_COLOR']
                if face.collision and 'COLLISION' not in mode:
                    mode += ['COLLISION']
                # mode |= face.mode

        if 'HALO' in mode and self.halonode == 0:
        # if mode & Mesh.FaceModes.HALO and self.halonode == 0:
            self.writeIndented("<Billboard axisOfRotation=\"0 0 0\">\n",1)
            self.halonode = 1
        elif 'BILLBOARD' in mode and self.billnode == 0:
        # elif mode & Mesh.FaceModes.BILLBOARD and self.billnode == 0:
            self.writeIndented("<Billboard axisOfRotation=\"0 1 0\">\n",1)
            self.billnode = 1
        elif 'OBJECT_COLOR' in mode and self.matonly == 0:
        # elif mode & Mesh.FaceModes.OBCOL and self.matonly == 0:
            self.matonly = 1
        # TF_TILES is marked as deprecated in DNA_meshdata_types.h
        # elif mode & Mesh.FaceModes.TILES and self.tilenode == 0:
        # 	self.tilenode = 1
        elif 'COLLISION' not in mode and self.collnode == 0:
        # elif not mode & Mesh.FaceModes.DYNAMIC and self.collnode == 0:
            self.writeIndented("<Collision enabled=\"false\">\n",1)
            self.collnode = 1

        nIFSCnt=self.countIFSSetsNeeded(mesh, imageMap, sided, vColors)

        if nIFSCnt > 1:
            self.writeIndented("<Group DEF=\"%s%s\">\n" % ("G_", meshName),1)

        if 'two' in sided and sided['two'] > 0:
            bTwoSided=1
        else:
            bTwoSided=0

        # mtx = ob.matrixWorld * MATWORLD # mtx is now passed
        mtx = mtx * MATWORLD

        loc= mtx.translationPart()
        sca= mtx.scalePart()
        quat = mtx.toQuat()
        rot= quat.axis

        self.writeIndented('<Transform DEF="%s" translation="%.6f %.6f %.6f" scale="%.6f %.6f %.6f" rotation="%.6f %.6f %.6f %.6f">\n' % \
                           (meshName, loc[0], loc[1], loc[2], sca[0], sca[1], sca[2], rot[0], rot[1], rot[2], quat.angle) )
        # self.writeIndented('<Transform DEF="%s" translation="%.6f %.6f %.6f" scale="%.6f %.6f %.6f" rotation="%.6f %.6f %.6f %.6f">\n' % \
        #   (meshName, loc[0], loc[1], loc[2], sca[0], sca[1], sca[2], rot[0], rot[1], rot[2], quat.angle*DEG2RAD) )

        self.writeIndented("<Shape>\n",1)
        maters=mesh.materials
        hasImageTexture=0
        issmooth=0

        if len(maters) > 0 or mesh.active_uv_texture:
        # if len(maters) > 0 or mesh.faceUV:
            self.writeIndented("<Appearance>\n", 1)
            # right now this script can only handle a single material per mesh.
            if len(maters) >= 1:
                mat=maters[0]
                # matFlags = mat.getMode()
                if not mat.face_texture:
                # if not matFlags & Blender.Material.Modes['TEXFACE']:
                    self.writeMaterial(mat, self.cleanStr(mat.name,''), world)
                    # self.writeMaterial(mat, self.cleanStr(maters[0].name,''), world)
                    if len(maters) > 1:
                        print("Warning: mesh named %s has multiple materials" % meshName)
                        print("Warning: only one material per object handled")

                #-- textures
                face = None
                if mesh.active_uv_texture:
                # if mesh.faceUV:
                    for face in mesh.active_uv_texture.data:
                    # for face in mesh.faces:
                        if face.image:
                        # if (hasImageTexture == 0) and (face.image):
                            self.writeImageTexture(face.image)
                            # hasImageTexture=1  # keep track of face texture
                            break
                if self.tilenode == 1 and face and face.image:
                # if self.tilenode == 1:
                    self.writeIndented("<TextureTransform	scale=\"%s %s\" />\n" % (face.image.xrep, face.image.yrep))
                    self.tilenode = 0
                self.writeIndented("</Appearance>\n", -1)

        #-- IndexedFaceSet or IndexedLineSet

        # user selected BOUNDS=1, SOLID=3, SHARED=4, or TEXTURE=5
        ifStyle="IndexedFaceSet"
        # look up mesh name, use it if available
        if meshME in self.meshNames:
            self.writeIndented("<%s USE=\"ME_%s\">" % (ifStyle, meshME), 1)
            self.meshNames[meshME]+=1
        else:
            if int(mesh.users) > 1:
                self.writeIndented("<%s DEF=\"ME_%s\" " % (ifStyle, meshME), 1)
                self.meshNames[meshME]=1
            else:
                self.writeIndented("<%s " % ifStyle, 1)

            if bTwoSided == 1:
                self.file.write("solid=\"false\" ")
            else:
                self.file.write("solid=\"true\" ")

            for face in mesh.faces:
                if face.smooth:
                     issmooth=1
                     break
            if issmooth==1:
                creaseAngle=(mesh.autosmooth_angle)*(math.pi/180.0)
                # creaseAngle=(mesh.degr)*(math.pi/180.0)
                self.file.write("creaseAngle=\"%s\" " % (round(creaseAngle,self.cp)))

            #--- output textureCoordinates if UV texture used
            if mesh.active_uv_texture:
            # if mesh.faceUV:
                if self.matonly == 1 and self.share == 1:
                    self.writeFaceColors(mesh)
                elif hasImageTexture == 1:
                    self.writeTextureCoordinates(mesh)
            #--- output coordinates
            self.writeCoordinates(ob, mesh, meshName, EXPORT_TRI)

            self.writingcoords = 1
            self.writingtexture = 1
            self.writingcolor = 1
            self.writeCoordinates(ob, mesh, meshName, EXPORT_TRI)

            #--- output textureCoordinates if UV texture used
            if mesh.active_uv_texture:
            # if mesh.faceUV:
                if hasImageTexture == 1:
                    self.writeTextureCoordinates(mesh)
                elif self.matonly == 1 and self.share == 1:
                    self.writeFaceColors(mesh)
            #--- output vertexColors
        self.matonly = 0
        self.share = 0

        self.writingcoords = 0
        self.writingtexture = 0
        self.writingcolor = 0
        #--- output closing braces
        self.writeIndented("</%s>\n" % ifStyle, -1)
        self.writeIndented("</Shape>\n", -1)
        self.writeIndented("</Transform>\n", -1)

        if self.halonode == 1:
            self.writeIndented("</Billboard>\n", -1)
            self.halonode = 0

        if self.billnode == 1:
            self.writeIndented("</Billboard>\n", -1)
            self.billnode = 0

        if self.collnode == 1:
            self.writeIndented("</Collision>\n", -1)
            self.collnode = 0

        if nIFSCnt > 1:
            self.writeIndented("</Group>\n", -1)

        self.file.write("\n")

    def writeCoordinates(self, ob, mesh, meshName, EXPORT_TRI = False):
        # create vertex list and pre rotate -90 degrees X for VRML

        if self.writingcoords == 0:
            self.file.write('coordIndex="')
            for face in mesh.faces:
                fv = face.verts
                # fv = face.v

                if len(fv)==3:
                # if len(face)==3:
                    self.file.write("%i %i %i -1, " % (fv[0], fv[1], fv[2]))
                    # self.file.write("%i %i %i -1, " % (fv[0].index, fv[1].index, fv[2].index))
                else:
                    if EXPORT_TRI:
                        self.file.write("%i %i %i -1, " % (fv[0], fv[1], fv[2]))
                        # self.file.write("%i %i %i -1, " % (fv[0].index, fv[1].index, fv[2].index))
                        self.file.write("%i %i %i -1, " % (fv[0], fv[2], fv[3]))
                        # self.file.write("%i %i %i -1, " % (fv[0].index, fv[2].index, fv[3].index))
                    else:
                        self.file.write("%i %i %i %i -1, " % (fv[0], fv[1], fv[2], fv[3]))
                        # self.file.write("%i %i %i %i -1, " % (fv[0].index, fv[1].index, fv[2].index, fv[3].index))

            self.file.write("\">\n")
        else:
            #-- vertices
            # mesh.transform(ob.matrixWorld)
            self.writeIndented("<Coordinate DEF=\"%s%s\" \n" % ("coord_",meshName), 1)
            self.file.write("\t\t\t\tpoint=\"")
            for v in mesh.verts:
                self.file.write("%.6f %.6f %.6f, " % tuple(v.co))
            self.file.write("\" />")
            self.writeIndented("\n", -1)

    def writeTextureCoordinates(self, mesh):
        texCoordList=[]
        texIndexList=[]
        j=0

        for face in mesh.active_uv_texture.data:
        # for face in mesh.faces:
            uvs = face.uv
            # uvs = [face.uv1, face.uv2, face.uv3, face.uv4] if face.verts[3] else [face.uv1, face.uv2, face.uv3]

            for uv in uvs:
            # for uv in face.uv:
                texIndexList.append(j)
                texCoordList.append(uv)
                j=j+1
            texIndexList.append(-1)
        if self.writingtexture == 0:
            self.file.write("\n\t\t\ttexCoordIndex=\"")
            texIndxStr=""
            for i in range(len(texIndexList)):
                texIndxStr = texIndxStr + "%d, " % texIndexList[i]
                if texIndexList[i]==-1:
                    self.file.write(texIndxStr)
                    texIndxStr=""
            self.file.write("\"\n\t\t\t")
        else:
            self.writeIndented("<TextureCoordinate point=\"", 1)
            for i in range(len(texCoordList)):
                self.file.write("%s %s, " % (round(texCoordList[i][0],self.tp), round(texCoordList[i][1],self.tp)))
            self.file.write("\" />")
            self.writeIndented("\n", -1)

    def writeFaceColors(self, mesh):
        if self.writingcolor == 0:
            self.file.write("colorPerVertex=\"false\" ")
        elif mesh.active_vertex_color:
        # else:
            self.writeIndented("<Color color=\"", 1)
            for face in mesh.active_vertex_color.data:
                c = face.color1
                if self.verbose > 2:
                    print("Debug: face.col r=%d g=%d b=%d" % (c[0], c[1], c[2]))
                    # print("Debug: face.col r=%d g=%d b=%d" % (c.r, c.g, c.b))
                aColor = self.rgbToFS(c)
                self.file.write("%s, " % aColor)

            # for face in mesh.faces:
            # 	if face.col:
            # 		c=face.col[0]
            # 		if self.verbose > 2:
            # 			print("Debug: face.col r=%d g=%d b=%d" % (c.r, c.g, c.b))
            # 		aColor = self.rgbToFS(c)
            # 		self.file.write("%s, " % aColor)
            self.file.write("\" />")
            self.writeIndented("\n",-1)

    def writeMaterial(self, mat, matName, world):
        # look up material name, use it if available
        if matName in self.matNames:
            self.writeIndented("<Material USE=\"MA_%s\" />\n" % matName)
            self.matNames[matName]+=1
            return;

        self.matNames[matName]=1

        ambient = mat.ambient/3
        # ambient = mat.amb/3
        diffuseR, diffuseG, diffuseB = tuple(mat.diffuse_color)
        # diffuseR, diffuseG, diffuseB = mat.rgbCol[0], mat.rgbCol[1],mat.rgbCol[2]
        if world:
            ambi = world.ambient_color
            # ambi = world.getAmb()
            ambi0, ambi1, ambi2 = (ambi[0]*mat.ambient)*2, (ambi[1]*mat.ambient)*2, (ambi[2]*mat.ambient)*2
            # ambi0, ambi1, ambi2 = (ambi[0]*mat.amb)*2, (ambi[1]*mat.amb)*2, (ambi[2]*mat.amb)*2
        else:
            ambi0, ambi1, ambi2 = 0, 0, 0
        emisR, emisG, emisB = (diffuseR*mat.emit+ambi0)/2, (diffuseG*mat.emit+ambi1)/2, (diffuseB*mat.emit+ambi2)/2

        shininess = mat.specular_hardness/512.0
        # shininess = mat.hard/512.0
        specR = (mat.specular_color[0]+0.001)/(1.25/(mat.specular_intensity+0.001))
        # specR = (mat.specCol[0]+0.001)/(1.25/(mat.spec+0.001))
        specG = (mat.specular_color[1]+0.001)/(1.25/(mat.specular_intensity+0.001))
        # specG = (mat.specCol[1]+0.001)/(1.25/(mat.spec+0.001))
        specB = (mat.specular_color[2]+0.001)/(1.25/(mat.specular_intensity+0.001))
        # specB = (mat.specCol[2]+0.001)/(1.25/(mat.spec+0.001))
        transp = 1-mat.alpha
        # matFlags = mat.getMode()
        if mat.shadeless:
        # if matFlags & Blender.Material.Modes['SHADELESS']:
          ambient = 1
          shine = 1
          specR = emitR = diffuseR
          specG = emitG = diffuseG
          specB = emitB = diffuseB
        self.writeIndented("<Material DEF=\"MA_%s\" " % matName, 1)
        self.file.write("diffuseColor=\"%s %s %s\" " % (round(diffuseR,self.cp), round(diffuseG,self.cp), round(diffuseB,self.cp)))
        self.file.write("specularColor=\"%s %s %s\" " % (round(specR,self.cp), round(specG,self.cp), round(specB,self.cp)))
        self.file.write("emissiveColor=\"%s %s %s\" \n" % (round(emisR,self.cp), round(emisG,self.cp), round(emisB,self.cp)))
        self.writeIndented("ambientIntensity=\"%s\" " % (round(ambient,self.cp)))
        self.file.write("shininess=\"%s\" " % (round(shininess,self.cp)))
        self.file.write("transparency=\"%s\" />" % (round(transp,self.cp)))
        self.writeIndented("\n",-1)

    def writeImageTexture(self, image):
        name = image.name
        filename = image.filename.split('/')[-1].split('\\')[-1]
        if name in self.texNames:
            self.writeIndented("<ImageTexture USE=\"%s\" />\n" % self.cleanStr(name))
            self.texNames[name] += 1
            return
        else:
            self.writeIndented("<ImageTexture DEF=\"%s\" " % self.cleanStr(name), 1)
            self.file.write("url=\"%s\" />" % name)
            self.writeIndented("\n",-1)
            self.texNames[name] = 1

    def writeBackground(self, world, alltextures):
        if world:	worldname = world.name
        else:		return
        blending = (world.blend_sky, world.paper_sky, world.real_sky)
        # blending = world.getSkytype()
        grd = world.horizon_color
        # grd = world.getHor()
        grd0, grd1, grd2 = grd[0], grd[1], grd[2]
        sky = world.zenith_color
        # sky = world.getZen()
        sky0, sky1, sky2 = sky[0], sky[1], sky[2]
        mix0, mix1, mix2 = grd[0]+sky[0], grd[1]+sky[1], grd[2]+sky[2]
        mix0, mix1, mix2 = mix0/2, mix1/2, mix2/2
        self.file.write("<Background ")
        if worldname not in self.namesStandard:
            self.file.write("DEF=\"%s\" " % self.secureName(worldname))
        # No Skytype - just Hor color
        if blending == (0, 0, 0):
        # if blending == 0:
            self.file.write("groundColor=\"%s %s %s\" " % (round(grd0,self.cp), round(grd1,self.cp), round(grd2,self.cp)))
            self.file.write("skyColor=\"%s %s %s\" " % (round(grd0,self.cp), round(grd1,self.cp), round(grd2,self.cp)))
        # Blend Gradient
        elif blending == (1, 0, 0):
        # elif blending == 1:
            self.file.write("groundColor=\"%s %s %s, " % (round(grd0,self.cp), round(grd1,self.cp), round(grd2,self.cp)))
            self.file.write("%s %s %s\" groundAngle=\"1.57, 1.57\" " %(round(mix0,self.cp), round(mix1,self.cp), round(mix2,self.cp)))
            self.file.write("skyColor=\"%s %s %s, " % (round(sky0,self.cp), round(sky1,self.cp), round(sky2,self.cp)))
            self.file.write("%s %s %s\" skyAngle=\"1.57, 1.57\" " %(round(mix0,self.cp), round(mix1,self.cp), round(mix2,self.cp)))
        # Blend+Real Gradient Inverse
        elif blending == (1, 0, 1):
        # elif blending == 3:
            self.file.write("groundColor=\"%s %s %s, " % (round(sky0,self.cp), round(sky1,self.cp), round(sky2,self.cp)))
            self.file.write("%s %s %s\" groundAngle=\"1.57, 1.57\" " %(round(mix0,self.cp), round(mix1,self.cp), round(mix2,self.cp)))
            self.file.write("skyColor=\"%s %s %s, " % (round(grd0,self.cp), round(grd1,self.cp), round(grd2,self.cp)))
            self.file.write("%s %s %s\" skyAngle=\"1.57, 1.57\" " %(round(mix0,self.cp), round(mix1,self.cp), round(mix2,self.cp)))
        # Paper - just Zen Color
        elif blending == (0, 0, 1):
        # elif blending == 4:
            self.file.write("groundColor=\"%s %s %s\" " % (round(sky0,self.cp), round(sky1,self.cp), round(sky2,self.cp)))
            self.file.write("skyColor=\"%s %s %s\" " % (round(sky0,self.cp), round(sky1,self.cp), round(sky2,self.cp)))
        # Blend+Real+Paper - komplex gradient
        elif blending == (1, 1, 1):
        # elif blending == 7:
            self.writeIndented("groundColor=\"%s %s %s, " % (round(sky0,self.cp), round(sky1,self.cp), round(sky2,self.cp)))
            self.writeIndented("%s %s %s\" groundAngle=\"1.57, 1.57\" " %(round(grd0,self.cp), round(grd1,self.cp), round(grd2,self.cp)))
            self.writeIndented("skyColor=\"%s %s %s, " % (round(sky0,self.cp), round(sky1,self.cp), round(sky2,self.cp)))
            self.writeIndented("%s %s %s\" skyAngle=\"1.57, 1.57\" " %(round(grd0,self.cp), round(grd1,self.cp), round(grd2,self.cp)))
        # Any Other two colors
        else:
            self.file.write("groundColor=\"%s %s %s\" " % (round(grd0,self.cp), round(grd1,self.cp), round(grd2,self.cp)))
            self.file.write("skyColor=\"%s %s %s\" " % (round(sky0,self.cp), round(sky1,self.cp), round(sky2,self.cp)))

        alltexture = len(alltextures)

        for i in range(alltexture):
            tex = alltextures[i]

            if tex.type != 'IMAGE' or tex.image == None:
                continue

            namemat = tex.name
            # namemat = alltextures[i].name

            pic = tex.image

            # using .expandpath just in case, os.path may not expect //
            basename = os.path.basename(pic.get_abs_filename())

            pic = alltextures[i].image
            # pic = alltextures[i].getImage()
            if (namemat == "back") and (pic != None):
                self.file.write("\n\tbackUrl=\"%s\" " % basename)
                # self.file.write("\n\tbackUrl=\"%s\" " % pic.filename.split('/')[-1].split('\\')[-1])
            elif (namemat == "bottom") and (pic != None):
                self.writeIndented("bottomUrl=\"%s\" " % basename)
                # self.writeIndented("bottomUrl=\"%s\" " % pic.filename.split('/')[-1].split('\\')[-1])
            elif (namemat == "front") and (pic != None):
                self.writeIndented("frontUrl=\"%s\" " % basename)
                # self.writeIndented("frontUrl=\"%s\" " % pic.filename.split('/')[-1].split('\\')[-1])
            elif (namemat == "left") and (pic != None):
                self.writeIndented("leftUrl=\"%s\" " % basename)
                # self.writeIndented("leftUrl=\"%s\" " % pic.filename.split('/')[-1].split('\\')[-1])
            elif (namemat == "right") and (pic != None):
                self.writeIndented("rightUrl=\"%s\" " % basename)
                # self.writeIndented("rightUrl=\"%s\" " % pic.filename.split('/')[-1].split('\\')[-1])
            elif (namemat == "top") and (pic != None):
                self.writeIndented("topUrl=\"%s\" " % basename)
                # self.writeIndented("topUrl=\"%s\" " % pic.filename.split('/')[-1].split('\\')[-1])
        self.writeIndented("/>\n\n")

##########################################################
# export routine
##########################################################

    def export(self, scene, world, alltextures,\
            EXPORT_APPLY_MODIFIERS = False,\
            EXPORT_TRI=				False,\
        ):

        print("Info: starting X3D export to " + self.filename + "...")
        self.writeHeader()
        # self.writeScript()
        self.writeNavigationInfo(scene)
        self.writeBackground(world, alltextures)
        self.writeFog(world)
        self.proto = 0


        # # COPIED FROM OBJ EXPORTER
        # if EXPORT_APPLY_MODIFIERS:
        # 	temp_mesh_name = '~tmp-mesh'

        # 	# Get the container mesh. - used for applying modifiers and non mesh objects.
        # 	containerMesh = meshName = tempMesh = None
        # 	for meshName in Blender.NMesh.GetNames():
        # 		if meshName.startswith(temp_mesh_name):
        # 			tempMesh = Mesh.Get(meshName)
        # 			if not tempMesh.users:
        # 				containerMesh = tempMesh
        # 	if not containerMesh:
        # 		containerMesh = Mesh.New(temp_mesh_name)
        # --------------------------


        for ob_main in [o for o in scene.objects if o.is_visible()]:
        # for ob_main in scene.objects.context:

            free, derived = create_derived_objects(ob_main)

            if derived == None: continue

            for ob, ob_mat in derived:
            # for ob, ob_mat in BPyObject.getDerivedObjects(ob_main):
                objType=ob.type
                objName=ob.name
                self.matonly = 0
                if objType == "CAMERA":
                # if objType == "Camera":
                    self.writeViewpoint(ob, ob_mat, scene)
                elif objType in ("MESH", "CURVE", "SURF", "TEXT") :
                # elif objType in ("Mesh", "Curve", "Surf", "Text") :
                    if EXPORT_APPLY_MODIFIERS or objType != 'MESH':
                    # if  EXPORT_APPLY_MODIFIERS or objType != 'Mesh':
                        me = ob.create_mesh(EXPORT_APPLY_MODIFIERS, 'PREVIEW')
                        # me= BPyMesh.getMeshFromObject(ob, containerMesh, EXPORT_APPLY_MODIFIERS, False, scene)
                    else:
                        me = ob.data
                        # me = ob.getData(mesh=1)

                    self.writeIndexedFaceSet(ob, me, ob_mat, world, EXPORT_TRI = EXPORT_TRI)

                    # free mesh created with create_mesh()
                    if me != ob.data:
                        bpy.data.remove_mesh(me)

                elif objType == "LAMP":
                # elif objType == "Lamp":
                    data= ob.data
                    datatype=data.type
                    if datatype == 'POINT':
                    # if datatype == Lamp.Types.Lamp:
                        self.writePointLight(ob, ob_mat, data, world)
                    elif datatype == 'SPOT':
                    # elif datatype == Lamp.Types.Spot:
                        self.writeSpotLight(ob, ob_mat, data, world)
                    elif datatype == 'SUN':
                    # elif datatype == Lamp.Types.Sun:
                        self.writeDirectionalLight(ob, ob_mat, data, world)
                    else:
                        self.writeDirectionalLight(ob, ob_mat, data, world)
                # do you think x3d could document what to do with dummy objects?
                #elif objType == "Empty" and objName != "Empty":
                #	self.writeNode(ob, ob_mat)
                else:
                    #print "Info: Ignoring [%s], object type [%s] not handle yet" % (object.name,object.getType)
                    pass

            if free:
                free_derived_objects(ob_main)

        self.file.write("\n</Scene>\n</X3D>")

        # if EXPORT_APPLY_MODIFIERS:
        # 	if containerMesh:
        # 		containerMesh.verts = None

        self.cleanup()

##########################################################
# Utility methods
##########################################################

    def cleanup(self):
        self.file.close()
        self.texNames={}
        self.matNames={}
        self.indentLevel=0
        print("Info: finished X3D export to %s\n" % self.filename)

    def cleanStr(self, name, prefix='rsvd_'):
        """cleanStr(name,prefix) - try to create a valid VRML DEF name from object name"""

        newName=name[:]
        if len(newName) == 0:
            self.nNodeID+=1
            return "%s%d" % (prefix, self.nNodeID)

        if newName in self.namesReserved:
            newName='%s%s' % (prefix,newName)

        if newName[0].isdigit():
            newName='%s%s' % ('_',newName)

        for bad in [' ','"','#',"'",',','.','[','\\',']','{','}']:
            newName=newName.replace(bad,'_')
        return newName

    def countIFSSetsNeeded(self, mesh, imageMap, sided, vColors):
        """
        countIFFSetsNeeded() - should look at a blender mesh to determine
        how many VRML IndexFaceSets or IndexLineSets are needed.  A
        new mesh created under the following conditions:

         o - split by UV Textures / one per mesh
         o - split by face, one sided and two sided
         o - split by smooth and flat faces
         o - split when faces only have 2 vertices * needs to be an IndexLineSet
        """

        imageNameMap={}
        faceMap={}
        nFaceIndx=0

        if mesh.active_uv_texture:
        # if mesh.faceUV:
            for face in mesh.active_uv_texture.data:
            # for face in mesh.faces:
                sidename='';
                if face.twoside:
                # if  face.mode & Mesh.FaceModes.TWOSIDE:
                    sidename='two'
                else:
                    sidename='one'

                if sidename in sided:
                    sided[sidename]+=1
                else:
                    sided[sidename]=1

                image = face.image
                if image:
                    faceName="%s_%s" % (face.image.name, sidename);
                    try:
                        imageMap[faceName].append(face)
                    except:
                        imageMap[faceName]=[face.image.name,sidename,face]

            if self.verbose > 2:
                for faceName in imageMap.keys():
                    ifs=imageMap[faceName]
                    print("Debug: faceName=%s image=%s, solid=%s facecnt=%d" % \
                          (faceName, ifs[0], ifs[1], len(ifs)-2))

        return len(imageMap)

    def faceToString(self,face):

        print("Debug: face.flag=0x%x (bitflags)" % face.flag)
        if face.sel:
            print("Debug: face.sel=true")

        print("Debug: face.mode=0x%x (bitflags)" % face.mode)
        if face.mode & Mesh.FaceModes.TWOSIDE:
            print("Debug: face.mode twosided")

        print("Debug: face.transp=0x%x (enum)" % face.transp)
        if face.transp == Mesh.FaceTranspModes.SOLID:
            print("Debug: face.transp.SOLID")

        if face.image:
            print("Debug: face.image=%s" % face.image.name)
        print("Debug: face.materialIndex=%d" % face.materialIndex)

    # XXX not used
    # def getVertexColorByIndx(self, mesh, indx):
    # 	c = None
    # 	for face in mesh.faces:
    # 		j=0
    # 		for vertex in face.v:
    # 			if vertex.index == indx:
    # 				c=face.col[j]
    # 				break
    # 			j=j+1
    # 		if c: break
    # 	return c

    def meshToString(self,mesh):
        # print("Debug: mesh.hasVertexUV=%d" % mesh.vertexColors)
        print("Debug: mesh.faceUV=%d" % (len(mesh.uv_textures) > 0))
        # print("Debug: mesh.faceUV=%d" % mesh.faceUV)
        print("Debug: mesh.hasVertexColours=%d" % (len(mesh.vertex_colors) > 0))
        # print("Debug: mesh.hasVertexColours=%d" % mesh.hasVertexColours())
        print("Debug: mesh.verts=%d" % len(mesh.verts))
        print("Debug: mesh.faces=%d" % len(mesh.faces))
        print("Debug: mesh.materials=%d" % len(mesh.materials))

    def rgbToFS(self, c):
        s="%s %s %s" % (round(c[0]/255.0,self.cp),
                        round(c[1]/255.0,self.cp),
                        round(c[2]/255.0,self.cp))

        # s="%s %s %s" % (
        # 	round(c.r/255.0,self.cp),
        # 	round(c.g/255.0,self.cp),
        # 	round(c.b/255.0,self.cp))
        return s

    def computeDirection(self, mtx):
        x,y,z=(0,-1.0,0) # point down

        ax,ay,az = (mtx*MATWORLD).toEuler()

        # ax *= DEG2RAD
        # ay *= DEG2RAD
        # az *= DEG2RAD

        # rot X
        x1=x
        y1=y*math.cos(ax)-z*math.sin(ax)
        z1=y*math.sin(ax)+z*math.cos(ax)

        # rot Y
        x2=x1*math.cos(ay)+z1*math.sin(ay)
        y2=y1
        z2=z1*math.cos(ay)-x1*math.sin(ay)

        # rot Z
        x3=x2*math.cos(az)-y2*math.sin(az)
        y3=x2*math.sin(az)+y2*math.cos(az)
        z3=z2

        return [x3,y3,z3]


    # swap Y and Z to handle axis difference between Blender and VRML
    #------------------------------------------------------------------------
    def rotatePointForVRML(self, v):
        x = v[0]
        y = v[2]
        z = -v[1]

        vrmlPoint=[x, y, z]
        return vrmlPoint

    # For writing well formed VRML code
    #------------------------------------------------------------------------
    def writeIndented(self, s, inc=0):
        if inc < 1:
            self.indentLevel = self.indentLevel + inc

        spaces=""
        for x in range(self.indentLevel):
            spaces = spaces + "\t"
        self.file.write(spaces + s)

        if inc > 0:
            self.indentLevel = self.indentLevel + inc

    # Converts a Euler to three new Quaternions
    # Angles of Euler are passed in as radians
    #------------------------------------------------------------------------
    def eulerToQuaternions(self, x, y, z):
        Qx = [math.cos(x/2), math.sin(x/2), 0, 0]
        Qy = [math.cos(y/2), 0, math.sin(y/2), 0]
        Qz = [math.cos(z/2), 0, 0, math.sin(z/2)]

        quaternionVec=[Qx,Qy,Qz]
        return quaternionVec

    # Multiply two Quaternions together to get a new Quaternion
    #------------------------------------------------------------------------
    def multiplyQuaternions(self, Q1, Q2):
        result = [((Q1[0] * Q2[0]) - (Q1[1] * Q2[1]) - (Q1[2] * Q2[2]) - (Q1[3] * Q2[3])),
                  ((Q1[0] * Q2[1]) + (Q1[1] * Q2[0]) + (Q1[2] * Q2[3]) - (Q1[3] * Q2[2])),
                  ((Q1[0] * Q2[2]) + (Q1[2] * Q2[0]) + (Q1[3] * Q2[1]) - (Q1[1] * Q2[3])),
                  ((Q1[0] * Q2[3]) + (Q1[3] * Q2[0]) + (Q1[1] * Q2[2]) - (Q1[2] * Q2[1]))]

        return result

    # Convert a Quaternion to an Angle Axis (ax, ay, az, angle)
    # angle is in radians
    #------------------------------------------------------------------------
    def quaternionToAngleAxis(self, Qf):
        scale = math.pow(Qf[1],2) + math.pow(Qf[2],2) + math.pow(Qf[3],2)
        ax = Qf[1]
        ay = Qf[2]
        az = Qf[3]

        if scale > .0001:
            ax/=scale
            ay/=scale
            az/=scale

        angle = 2 * math.acos(Qf[0])

        result = [ax, ay, az, angle]
        return result

##########################################################
# Callbacks, needed before Main
##########################################################

def x3d_export(filename,
               context,
               EXPORT_APPLY_MODIFIERS=False,
               EXPORT_TRI=False,
               EXPORT_GZIP=False):

    if EXPORT_GZIP:
        if not filename.lower().endswith('.x3dz'):
            filename = '.'.join(filename.split('.')[:-1]) + '.x3dz'
    else:
        if not filename.lower().endswith('.x3d'):
            filename = '.'.join(filename.split('.')[:-1]) + '.x3d'


    scene = context.scene
    # scene = Blender.Scene.GetCurrent()
    world = scene.world

    # XXX these are global textures while .Get() returned only scene's?
    alltextures = bpy.data.textures
    # alltextures = Blender.Texture.Get()

    wrlexport=x3d_class(filename)
    wrlexport.export(\
        scene,\
        world,\
        alltextures,\
        \
        EXPORT_APPLY_MODIFIERS = EXPORT_APPLY_MODIFIERS,\
        EXPORT_TRI = EXPORT_TRI,\
        )


def x3d_export_ui(filename):
    if not filename.endswith(extension):
        filename += extension
    #if _safeOverwrite and sys.exists(filename):
    #	result = Draw.PupMenu("File Already Exists, Overwrite?%t|Yes%x1|No%x0")
    #if(result != 1):
    #	return

    # Get user options
    EXPORT_APPLY_MODIFIERS = Draw.Create(1)
    EXPORT_TRI = Draw.Create(0)
    EXPORT_GZIP = Draw.Create( filename.lower().endswith('.x3dz') )

    # Get USER Options
    pup_block = [\
    ('Apply Modifiers', EXPORT_APPLY_MODIFIERS, 'Use transformed mesh data from each object.'),\
    ('Triangulate', EXPORT_TRI, 'Triangulate quads.'),\
    ('Compress', EXPORT_GZIP, 'GZip the resulting file, requires a full python install'),\
    ]

    if not Draw.PupBlock('Export...', pup_block):
        return

    Blender.Window.EditMode(0)
    Blender.Window.WaitCursor(1)

    x3d_export(filename,\
        EXPORT_APPLY_MODIFIERS = EXPORT_APPLY_MODIFIERS.val,\
        EXPORT_TRI = EXPORT_TRI.val,\
        EXPORT_GZIP = EXPORT_GZIP.val\
    )

    Blender.Window.WaitCursor(0)



#########################################################
# main routine
#########################################################


# if __name__ == '__main__':
# 	Blender.Window.FileSelector(x3d_export_ui,"Export X3D", Blender.Get('filename').replace('.blend', '.x3d'))

from bpy.props import *

class ExportX3D(bpy.types.Operator):
    '''Export selection to Extensible 3D file (.x3d)'''
    bl_idname = "export.x3d"
    bl_label = 'Export X3D'

    # List of operator properties, the attributes will be assigned
    # to the class instance from the operator settings before calling.
    path = StringProperty(name="File Path", description="File path used for exporting the X3D file", maxlen= 1024, default= "")

    apply_modifiers = BoolProperty(name="Apply Modifiers", description="Use transformed mesh data from each object.", default=True)
    triangulate = BoolProperty(name="Triangulate", description="Triangulate quads.", default=False)
    compress = BoolProperty(name="Compress", description="GZip the resulting file, requires a full python install.", default=False)


    def execute(self, context):
        x3d_export(self.properties.path, context, self.properties.apply_modifiers, self.properties.triangulate, self.properties.compress)
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.manager
        wm.add_fileselect(self)
        return {'RUNNING_MODAL'}

bpy.types.register(ExportX3D)

import dynamic_menu

def menu_func(self, context):
    default_path = bpy.data.filename.replace(".blend", ".x3d")
    self.layout.operator(ExportX3D.bl_idname, text="X3D Extensible 3D (.x3d)...").path = default_path

menu_item = dynamic_menu.add(bpy.types.INFO_MT_file_export, menu_func)

# NOTES
# - blender version is hardcoded
