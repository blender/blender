#!BPY
""" Registration info for Blender menus:
Name: 'VRML 2.0 (.wrl)...'
Blender: 232
Group: 'Export'
Submenu: 'All Objects...' all
Submenu: 'Selected Objects...' selected
Tooltip: 'Export to VRML2 (.wrl) file.'
"""

# $Id$
#
#------------------------------------------------------------------------
# VRML2 exporter for blender 2.28a or above
#
# Source: http://blender.kimballsoftware.com/
#
# Authors: Rick Kimball with much inspiration
#         from the forum at www.elysiun.com
#         and irc://irc.freenode.net/blenderchat
#         Ken Miller and Steve Matthews (Added Camera Support)
#
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2003,2004: Rick Kimball rick@vrmlworld.net
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
# To use script:
# 1.) load this file in the text window.
#     (press SHIFT+F11, Open New via Datablock button)
# 2.) make sure your mouse is over the text edit window and
#     run this script. (press ALT+P)
# Or:
#   copy to the scripts directory and it will appear in the
#   export list. (Needs 2.32 or higher)
#
# Notes:
#  a.) output filename is same as current blender file with .wrl extension
#  b.) error messages go to the Blender DOS console window
#
# The latest version of this python export script:
#   http://blender.kimballsoftware.com/
#
# If you like this script, try using http://vrmlworld.net/
# to show off your VRML world.
#
# 2004-01-19 by Rick Kimball <rick@vrmlworld.net>
#  o added sub menus and file selector dialog
#
# 2004-01-17 by Rick Kimball <rick@vrmlworld.net>
#  o add meta comments so script will appear in export menu list
#
# 2003-11-01 by Rick Kimball <rick@vrmlworld.net>
#  o fixed issues related to Lamp object and 2.28a API.
#
# 2003-07-19 by Rick Kimball <rick@vrmlworld.net>
#  o made compatible with new Python API in 2.28
#
# 2003-01-16 by Ken Miller - with math help from Steve Matthews :)
#   o Added support for exporting cameras out of Blender
#   o Sets the name of the camera as the object name
#   o sets the description of the camera as the object name,
#     which should be modified to something meaningful
#   o sets the position and orientation
#
# 2003-01-19 Rick Kimball <rick@vrmlworld.net>
#   o Added Support For PointLight, SpotLight and DirectionalLight using Lamps
#   o Creates multi singlesided or doublesided IFS
#   o Creates IndexedLineSets if DrawTypes is WIRE instead of Shaded
#
# 2003-02-03 Rick Kimball <rick@vrmlworld.net>
#   o attempts to catch exceptions for empty objects
#
# 2003-02-04 Rick Kimball <rick@vrmlworld.net>
#   o fixed file overwrite problem when blender filename is all uppercase
#
# 2003-02-08 Rick Kimball <rick@vrmlworld.net>
#   o cleanStr() creates valid VRML DEF names even if object.name
#     is zero length or uses VRML reserved names or characters
#
#------------------------------------------------------------------------
# Known Issue:
#  o doesn't handle multiple materials (don't use material indices)
#  o doesn't handle multiple UV textures on a single mesh. (create a mesh for each texture)
#  o material colors need work
#  o spotlight softness needs work
#  o can't get the texture array associated with material * not the UV ones
#  o can't set smoothing, crease angle and mesh smoothing * setting not accesible
#
# Still Todo:
#
#  - Support for material indexes
#  - Automatically Split IFS when multiple UV textures found * warning only now
#  - Automatically Split IFS when combination of single vs double sided
#  - Automatically Split IFS when face with only 2 vertices is found should be an ILS
#  - Export common coordinate map for split IFS
#  - Intelligent color array vs color index
#  - Support more blender objects: World
#  - Figure out how to output Animation
#  - Add GUI to control the following:
#    o All/Layer/Object output radio button
#    o Color per vertex toggle yes/no
#    o Complex/Simple VRML output radio button
#    o Compressed/Uncompressed output radio button
#    o Decimal precision dropdown 1,2,3,4,5,6
#    o IFS/Elevation Grid output radio button
#    o Normals output toggle yes/no
#    o Proto output toggle yes/no
#    o Verbose console progress

import Blender
from Blender import NMesh, Lamp
import math


#-- module constants
radD=math.pi/180.0
rad90=90.0*radD      # for rotation
rad30=30.0*radD      # default crease angle
ARG=''

#------------------------------------------------------------------------
#-- utility functions and classes --
#------------------------------------------------------------------------
def rad2deg(v):
    return round(v*180.0/math.pi,4)

def deg2rad(v):
    return (v*math.pi)/180.0;

class DrawTypes:
    """Object DrawTypes enum values
    BOUNDS - draw only the bounding box of the object
    WIRE - draw object as a wire frame
    SOLID - draw object with flat shading
    SHADED - draw object with OpenGL shading
"""
    BOUNDBOX  = 1
    WIRE      = 2
    SOLID     = 3
    SHADED    = 4
    TEXTURE   = 5

#------------------------------------------------------
# the Blender.Object class seems to be missing this...
#------------------------------------------------------
if not hasattr(Blender.Object,'DrawTypes'):
    Blender.Object.DrawTypes = DrawTypes()

#------------------------------------------------------------------------
#-- VRML2Export --
#------------------------------------------------------------------------
class VRML2Export:
    #------------------------------------------------------------------------
    def __init__(self, filename):
        #--- public you can change these ---
        self.verbose=1     # level of verbosity in console 0-none, 1-some, 2-most
        self.cp=3          # decimals for material color values     0.000 - 1.000
        self.vp=3          # decimals for vertex coordinate values  0.000 - n.000
        self.tp=3          # decimals for texture coordinate values 0.000 - 1.000
        self.ambientIntensity=.2
        self.defCreaseAngle=rad30
        self.smooth=0
        
        #--- class private don't touch ---
        self.texNames={}   # dictionary of textureNames
        self.matNames={}   # dictionary of materiaNames
        self.indentLevel=0 # keeps track of current indenting
        self.filename=filename
        self.file = open(filename, "w")
        self.bNav=0
        self.nNodeID=0
        self.VRMLReserved=[ "Anchor","Appearance","Anchor","AudioClip","Background",
                            "Billboard", "Box", "Collision", "Color", "ColorInterpolator",
                            "Cone", "Coordinate", "CoordinateInterpolator", "Cylinder",
                            "CylinderSensor", "DirectionalLight", "ElevationGrid",
                            "Extrustion", "Fog", "FontStyle", "Group", "ImageTexture",
                            "IndexedFaceSet", "IndexedLineSet", "Inline", "LOD",
                            "Material", "MovieTexture", "NavigationInfo", "Normal",
                            "NormalInterpolator","OrientationInterpolator", "PixelTexture",
                            "PlaneSensor", "PointLight", "PointSet", "PositionInterpolator",
                            "ProxmimitySensor", "ScalarInterpolator", "Script", "Shape",
                            "Sound", "Sphere", "SphereSensor", "SpotLight", "Switch",
                            "Text", "TextureCoordinate", "TextureTransform", "TimeSensor",
                            "TouchSensor", "Transform", "Viewpoint", "VisibilitySensor",
                            "WorldInfo"
                            ]

    #------------------------------------------------------------------------
    # writeHeader, export file, cleanup
    #------------------------------------------------------------------------
    def writeHeader(self):
        self.file.write("#VRML V2.0 utf8\n")
        self.file.write("# modeled using blender3d http://blender.org/$\n")
        self.file.write("# exported using wrl2export.py version $Revision$\n")
        self.file.write("# get latest exporter at http://kimballsoftware.com/blender/\n\n")
    
    def export(self, scene):
        print "Info: starting VRML2 export to " + self.filename + "..."
    
        self.writeHeader()
        theObjects = []
        if ARG == 'selected':
            theObjects = Blender.Object.GetSelected()
        else:
            theObjects = scene.getChildren()
            
        for object in theObjects:
            try:
                objType=object.getType()

                if objType == "Mesh":
                    self.writeIndexedFaceSet(object, normals = 0)
                elif objType == "Camera":
                    self.writeCameraInfo(object)
                elif objType == "Lamp":
                    # if there is a lamp then we probably want to turn off the headlight
                    if self.bNav == 0:
                        self.writeNavigationInfo()
                        self.bNav=1
                    #endif
                
                    lamp=Lamp.Get(object.data.getName())
                    try:
                        lampType=lamp.getType()

                        if lampType == Lamp.Types.Lamp:
                            self.writePointLight(object, lamp)
                        elif lampType == Lamp.Types.Spot:
                            self.writeSpotLight(object, lamp)
                        elif lampType == Lamp.Types.Sun:
                            self.writeDirectionalLight(object, lamp)
                        else:
                            self.writeDirectionalLight(object, lamp)
                        #endif
                    except AttributeError:
                        print "Error: Unable to get type info for %s" % object.name
                else:
                    print "Info: Ignoring [%s], object type [%s] not handle yet" % \
                          (object.name,object.getType())
                #endif
            except ValueError:
                print "Error: object named %s has problem with accessing an attribute" % object.name
            #end try
        #endfor
        self.cleanup()

    def cleanup(self):
        self.file.close()
        self.texNames={}
        self.matNames={}
        self.indentLevel=0
        print "Info: finished VRML2 export to %s\n" % self.filename

    #------------------------------------------------------------------------
    # Writes out camera info as a viewpoint
    # Handles orientation, position
    # Use camera object name to set description
    #------------------------------------------------------------------------
    def writeCameraInfo(self, object):
        if self.verbose > 0:
            print "Info: exporting camera named="+object.name
        #endif
        
        self.writeIndented("DEF %s Viewpoint {\n" % (self.cleanStr(object.name)), 1)
        
        self.writeIndented("description \"%s\" \n" % (object.name))
        
        # get the camera location, subtract 90 degress from X to orient like VRML does
        loc = self.rotatePointForVRML(object.loc)
        rot = [object.RotX - 1.57, object.RotY, object.RotZ]
        nRot = self.rotatePointForVRML(rot)
        
        # convert to Quaternion and to Angle Axis
        Q  = self.eulerToQuaternions(nRot[0], nRot[1], nRot[2])
        Q1 = self.multiplyQuaternions(Q[0], Q[1])
        Qf = self.multiplyQuaternions(Q1, Q[2])
        angleAxis = self.quaternionToAngleAxis(Qf)
        
        # write orientation statement
        self.writeIndented("orientation %3.2f %3.2f %3.2f %3.2f\n" %
                           (angleAxis[0], angleAxis[1], -angleAxis[2], angleAxis[3]))
        
        # write position statement
        self.writeIndented("position %3.2f %3.2f %3.2f\n" %
                           (loc[0], loc[1], loc[2]))
        
        self.writeIndented("} # Viewpoint\n", -1)
        
        self.writeIndented("\n")

    #------------------------------------------------------------------------
    def writeIndexedFaceSet(self, object, normals = 0):
        if self.verbose > 0:
            print "Info: exporting mesh named=["+object.name+"]"
        #endif

        imageMap={}   # set of used images
        sided={}      # 'one':cnt , 'two':cnt
        vColors={}    # 'multi':1

        mesh=object.getData()

        nIFSCnt=self.countIFSSetsNeeded(mesh, imageMap, sided, vColors)

        meshName = self.cleanStr(object.name)
        
        if nIFSCnt > 1:
            self.writeIndented("DEF %s%s Group {\n" % ("G_", meshName),1)
            self.writeIndented("children [\n",1)
        #endif
        
        if self.verbose > 0:
            print "Debug: [%s] has %d UV Textures" % (object.name, nIFSCnt)
        #endif

        if sided.has_key('two') and sided['two'] > 0:
            bTwoSided=1
        else:
            bTwoSided=0

        self.writeIndented("DEF %s Shape {\n" % meshName,1)
    
        # show script debugging info
        if self.verbose > 1:
            self.meshToString(mesh)
            print "Debug: mesh.faces["
            for face in mesh.faces:
                self.faceToString(face)
            #endfor
            print "Debug: ]"
        #endif
        
        maters=mesh.materials
        hasImageTexture=0

        if len(maters) > 0 or mesh.hasFaceUV():
            self.writeIndented("appearance Appearance {\n", 1)
            
            # right now this script can only handle a single material per mesh.
            if len(maters) >= 1:
                mat=Blender.Material.Get(maters[0].name)
                self.writeMaterial(mat, self.cleanStr(maters[0].name,'mat_'))
                if len(maters) > 1:
                    print "Warning: mesh named %s has multiple materials" % meshName
                    print "Warning: only one material per object handled"
                #endif
            else:
                self.writeIndented("material NULL\n")
            #endif
        
            #-- textures
            if mesh.hasFaceUV():
                for face in mesh.faces:
                    if (hasImageTexture == 0) and (face.image):
                        self.writeImageTexture(face.image.name)
                        hasImageTexture=1  # keep track of face texture
                    #endif
                #endfor
            #endif

            self.writeIndented("} # Appearance\n", -1)
        #endif

        #-------------------------------------------------------------------
        #--
        #-- IndexedFaceSet or IndexedLineSet
        #

        # check if object is wireframe only
        if object.drawType == Blender.Object.DrawTypes.WIRE:
            # user selected WIRE=2 on the Drawtype=Wire on (F9) Edit page
            ifStyle="IndexedLineSet"
        else:
            # user selected BOUNDS=1, SOLID=3, SHARED=4, or TEXTURE=5
            ifStyle="IndexedFaceSet"
        #endif
        
        self.writeIndented("geometry %s {\n" % ifStyle, 1)
        if object.drawType != Blender.Object.DrawTypes.WIRE:
            if bTwoSided == 1:
                self.writeIndented("solid FALSE # two sided\n")
            else:
                self.writeIndented("solid TRUE # one sided\n")
            #endif
        #endif
        
        #---
        #--- output coordinates
        self.writeCoordinates(object, mesh, meshName)
        
        if object.drawType != Blender.Object.DrawTypes.WIRE:
            #---
            #--- output textureCoordinates if UV texture used
            if mesh.hasFaceUV():
                if hasImageTexture == 1:
                    self.writeTextureCoordinates(mesh)
                    if vColors.has_key('multi'):
                        self.writeVertexColors(mesh) # experiment
                    #endif
                else:
                    self.writeFaceColors(mesh)
                #endif hasImageTexture
            #endif hasFaceUV

            # TBD: figure out how to get this properly
            if self.smooth:
                creaseAngle=self.defCreaseAngle;
                self.writeIndented("creaseAngle %s\n" % creaseAngle)
            else:
                self.writeIndented("creaseAngle 0.0 # in radians\n")
            #endif mesh.smooth
        #endif WIRE

        #--- output vertexColors
        if mesh.hasVertexColours() and vColors.has_key('multi'):
            self.writeVertexColors(mesh)
        #endif

        #--- output closing braces
        self.writeIndented("} # %s\n" % ifStyle, -1)
        self.writeIndented("} # Shape\n", -1)

        if nIFSCnt > 1:
            self.writeIndented("] # children\n", -1)
            self.writeIndented("} # Group\n", -1)
        #endif

        self.writeIndented("\n")

    #------------------------------------------------------------------------
    def writeCoordinates(self, object, mesh, meshName):
        #-- vertices
        self.writeIndented("coord DEF %s%s Coordinate {\n" % ("coord_",meshName), 1)
        self.writeIndented("point [\n", 1)
        meshVertexList = mesh.verts

        # create vertex list and pre rotate -90 degrees X for VRML
        mm=object.getMatrix()
        for vertex in meshVertexList:
            v=self.rotVertex(mm, vertex);
            self.writeIndented("%s %s %s,\n" %
                               (round(v[0],self.vp),
                                round(v[1],self.vp),
                                round(v[2],self.vp) ))
        #endfor
        self.writeIndented("] # point\n", -1)
        self.writeIndented("} # Coordinate\n", -1)

        self.writeIndented("coordIndex [\n", 1)
        coordIndexList=[]  
        for face in mesh.faces:
            cordStr=""
            for i in range(len(face)):
                indx=meshVertexList.index(face[i])
                cordStr = cordStr + "%s, " % indx
            #endfor
            self.writeIndented(cordStr + "-1,\n")
        #endfor
        self.writeIndented("] # coordIndex\n", -1)

    #------------------------------------------------------------------------
    def writeTextureCoordinates(self, mesh):
        texCoordList=[] 
        texIndexList=[]
        j=0

        for face in mesh.faces:
            for i in range(len(face)):
                texIndexList.append(j)
                texCoordList.append(face.uv[i])
                j=j+1
            #endfor
            texIndexList.append(-1)
        #endfor

        self.writeIndented("texCoord TextureCoordinate {\n", 1)
        self.writeIndented("point [\n", 1)
        for i in range(len(texCoordList)):
            self.writeIndented("%s %s," %
                               (round(texCoordList[i][0],self.tp), 
                                round(texCoordList[i][1],self.tp))+"\n")
        #endfor
        self.writeIndented("] # point\n", -1)
        self.writeIndented("} # texCoord\n", -1)

        self.writeIndented("texCoordIndex [\n", 1)
        texIndxStr=""
        for i in range(len(texIndexList)):
            texIndxStr = texIndxStr + "%d, " % texIndexList[i]
            if texIndexList[i]==-1:
                self.writeIndented(texIndxStr + "\n")
                texIndxStr=""
            #endif
        #endfor
        self.writeIndented("] # texCoordIndex\n", -1)

    #------------------------------------------------------------------------
    def writeFaceColors(self, mesh):
        self.writeIndented("colorPerVertex FALSE\n")
        self.writeIndented("color Color {\n",1)
        self.writeIndented("color [\n",1)

        for face in mesh.faces:
            if face.col:
                c=face.col[0]
                if self.verbose > 1:
                    print "Debug: face.col r=%d g=%d b=%d" % (c.r, c.g, c.b)
                #endif

                aColor = self.rgbToFS(c)
                self.writeIndented("%s,\n" % aColor)
        #endfor

        self.writeIndented("] # color\n",-1)
        self.writeIndented("} # Color\n",-1)

    #------------------------------------------------------------------------
    def writeVertexColors(self, mesh):
        self.writeIndented("colorPerVertex TRUE\n")
        self.writeIndented("color Color {\n",1)
        self.writeIndented("color [\n",1)

        for i in range(len(mesh.verts)):
            c=self.getVertexColorByIndx(mesh,i)
            if self.verbose > 1:
                print "Debug: vertex[%d].col r=%d g=%d b=%d" % (i, c.r, c.g, c.b)
            #endif

            aColor = self.rgbToFS(c)
            self.writeIndented("%s,\n" % aColor)
        #endfor

        self.writeIndented("] # color\n",-1)
        self.writeIndented("} # Color\n",-1)

    #------------------------------------------------------------------------
    def writeMaterial(self, mat, matName):
        # look up material name, use it if available
        if self.matNames.has_key(matName):
            self.writeIndented("material USE %s\n" % matName)
            self.matNames[matName]+=1
            return;
        #endif

        self.matNames[matName]=1

        ambient = mat.amb
        diffuseR, diffuseG, diffuseB = mat.rgbCol[0], mat.rgbCol[1],mat.rgbCol[2]
        emisR, emisG, emisB = diffuseR*mat.emit, diffuseG*mat.emit, diffuseB*mat.emit
        shininess = mat.hard/255.0
        specR = mat.specCol[0]
        specG = mat.specCol[1]
        specB = mat.specCol[2]
        transp = 1-mat.alpha

        self.writeIndented("material DEF %s Material {\n" % matName, 1)
        self.writeIndented("diffuseColor %s %s %s" %
                           (round(diffuseR,self.cp), round(diffuseG,self.cp), round(diffuseB,self.cp)) +
                           "\n")
        self.writeIndented("ambientIntensity %s" %
                           (round(ambient,self.cp))+
                           "\n")
        self.writeIndented("specularColor %s %s %s" %
                           (round(specR,self.cp), round(specG,self.cp), round(specB,self.cp)) +
                           "\n" )
        self.writeIndented("emissiveColor  %s %s %s" %
                           (round(emisR,self.cp), round(emisG,self.cp), round(emisB,self.cp)) +
                           "\n" )
        self.writeIndented("shininess %s" %
                           (round(shininess,self.cp)) +
                           "\n" )
        self.writeIndented("transparency %s" %
                           (round(transp,self.cp)) +
                           "\n")
        self.writeIndented("} # Material\n",-1)

    #------------------------------------------------------------------------
    def writeImageTexture(self, name):
        if self.texNames.has_key(name):
            self.writeIndented("texture USE %s\n" % self.cleanStr(name))
            self.texNames[name] += 1
            return
        else:
            self.writeIndented("texture DEF %s ImageTexture {\n" % self.cleanStr(name), 1)
            self.writeIndented("url \"%s\"\n" % name)
            self.writeIndented("} # ImageTexture \n",-1)
            self.texNames[name] = 1
        #endif

    #------------------------------------------------------------------------
    def writeSpotLight(self, object, lamp):
        safeName = self.cleanStr(object.name)

        # compute cutoff and beamwidth
        intensity=min(lamp.energy/1.5,1.0) # TBD: figure out the right value

        beamWidth=deg2rad(lamp.spotSize)*.5;
        cutOffAngle=beamWidth*.99

        (dx,dy,dz)=self.computeDirection(object)
        # note -dx seems to equal om[3][0]
        # note -dz seems to equal om[3][1]
        # note  dy seems to equal om[3][2]
        om = object.getMatrix()
            
        location=self.rotVertex(om, (0,0,0));
        radius = lamp.dist*math.cos(beamWidth)
        self.writeIndented("DEF %s SpotLight {\n" % safeName,1)
        self.writeIndented("radius %s\n" % radius )
        self.writeIndented("intensity %s\n" % intensity )
        self.writeIndented("beamWidth %s # lamp.spotSize %s\n" % (beamWidth, lamp.spotSize) )
        self.writeIndented("cutOffAngle %s # lamp.spotBlend %s\n" % (cutOffAngle, lamp.spotBlend))
        self.writeIndented("direction %s %s %s # lamp.RotX=%s RotY=%s RotZ=%s\n" % \
                           (round(dx,3),round(dy,3),round(dz,3),
                            round(rad2deg(object.RotX),3),
                            round(rad2deg(object.RotY),3),
                            round(rad2deg(object.RotZ),3)))
        self.writeIndented("location %s %s %s\n" % (round(location[0],3),
                                                    round(location[1],3),
                                                    round(location[2],3)))
        self.writeIndented("} # SpotLight\n",-1)

        # export a cone that matches the spotlight in verbose mode
        if self.verbose > 1:
            self.writeIndented("#generated visible spotlight cone\n")
            self.writeIndented("Transform { # Spotlight Cone\n",1)
            self.writeIndented("translation %s %s %s\n" % (round(location[0],3),
                                                           round(location[1],3),
                                                           round(location[2],3)))
            rot = [object.RotX, object.RotY, object.RotZ]
            nRot = self.rotatePointForVRML(rot)
            
            # convert to Quaternion and to Angle Axis
            Q  = self.eulerToQuaternions(nRot[0], nRot[1], nRot[2])
            Q1 = self.multiplyQuaternions(Q[0], Q[1])
            Qf = self.multiplyQuaternions(Q1, Q[2])
            angleAxis = self.quaternionToAngleAxis(Qf)
            
            # write orientation statement
            self.writeIndented("rotation %3.2f %3.2f %3.2f %3.2f\n" %
                               (angleAxis[0], angleAxis[1], -angleAxis[2], angleAxis[3]))
            
            self.writeIndented("children [\n",1)
            
            ch=radius
            br=ch*math.sin(beamWidth)
            self.writeIndented("Transform {\n",1)
            self.writeIndented("translation 0 -%s 0\n" % (ch/2))
            self.writeIndented("children ")
            self.writeIndented("Collision {\n",1)
            self.writeIndented("collide FALSE children Shape {\n",1)
            self.writeIndented("geometry Cone { height %s bottomRadius %s }\n" % (ch, br))
            self.writeIndented("appearance Appearance{\n",1)
            self.writeIndented("material Material { diffuseColor 1 1 1 transparency .8 }\n")
            self.writeIndented("} # Appearance\n",-1)
            self.writeIndented("} # Shape\n",-1)
            self.writeIndented("} # Collision\n",-1)
            self.writeIndented("} # Transform visible cone \n",-1)
            self.writeIndented("] # Spot children\n",-1)
            self.writeIndented("} # SpotLight Cone Transform\n",-1)
        #endif debug cone
        self.writeIndented("\n")
        
    #------------------------------------------------------------------------
    def writeDirectionalLight(self, object, lamp):
        safeName = self.cleanStr(object.name)

        intensity=min(lamp.energy/1.5, 1.0) # TBD: figure out the right value
        (dx,dy,dz)=self.computeDirection(object)

        self.writeIndented("DEF %s DirectionalLight {\n" % safeName,1)
        self.writeIndented("ambientIntensity %s\n" % self.ambientIntensity )
        self.writeIndented("intensity %s\n" % intensity )
        self.writeIndented("direction %s %s %s\n" % (round(dx,4),round(dy,4),round(dz,4)))
        self.writeIndented("} # DirectionalLight\n",-1)
        self.writeIndented("\n")

    #------------------------------------------------------------------------
    def writePointLight(self, object, lamp):
        safeName = self.cleanStr(object.name)

        om = object.getMatrix()
        location=self.rotVertex(om, (0,0,0));
        intensity=min(lamp.energy/1.5,1.0) # TBD: figure out the right value

        radius = lamp.dist
        self.writeIndented("DEF %s PointLight {\n" % safeName,1)
        self.writeIndented("ambientIntensity %s\n" % self.ambientIntensity )
        self.writeIndented("intensity %s\n" % intensity )
        self.writeIndented("location %s %s %s\n" % (round(location[0],3),
                                                    round(location[1],3),
                                                    round(location[2],3)))
        self.writeIndented("radius %s\n" % radius )
        self.writeIndented("} # PointLight\n",-1)
        self.writeIndented("\n")

    #------------------------------------------------------------------------
    def writeNavigationInfo(self):
        self.writeIndented("NavigationInfo {\n",1)
        self.writeIndented("headlight FALSE\n")
        self.writeIndented("avatarSize [0.25, 1.75, 0.75]\n")
        self.writeIndented("} # NavigationInfo\n",-1)
        self.writeIndented("\n")

    #------------------------------------------------------------------------
    #--- Utility methods
    #------------------------------------------------------------------------

    def cleanStr(self, name, prefix='rsvd_'):
        """cleanStr(name,prefix) - try to create a valid VRML DEF name from object name"""

        newName=name[:]
        if len(newName) == 0:
            self.nNodeID+=1
            return "%s%d" % (prefix, self.nNodeID)
        
        if newName in self.VRMLReserved:
            newName='%s%s' % (prefix,newName)
        #endif
        
        if newName[0] in ['0','1','2','3','4','5','6','7','8','9']:
            newName='%s%s' % ('_',newName)
        #endif

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
        
        for face in mesh.faces:
            sidename='';
            if (face.mode & NMesh.FaceModes.TWOSIDE) == NMesh.FaceModes.TWOSIDE:
                sidename='two'
            else:
                sidename='one'
            #endif

            if not vColors.has_key('multi'):
                for face in mesh.faces:
                    if face.col:
                        c=face.col[0]
                        if c.r != 255 and c.g != 255 and c.b !=255:
                            vColors['multi']=1
                        #endif
                    #endif
                #endfor
            #endif

            if sided.has_key(sidename):
                sided[sidename]+=1
            else:
                sided[sidename]=1
            #endif

            if face.image:
                faceName="%s_%s" % (face.image.name, sidename);

                if imageMap.has_key(faceName):
                    imageMap[faceName].append(face)
                else:
                    imageMap[faceName]=[face.image.name,sidename,face]
                #endif
            #endif
        #endfor

        if self.verbose > 0:
            for faceName in imageMap.keys():
                ifs=imageMap[faceName]
                print "Debug: faceName=%s image=%s, solid=%s facecnt=%d" % \
                      (faceName, ifs[0], ifs[1], len(ifs)-2)
            #endif
        #endif

        return len(imageMap.keys())
    
    def faceToString(self,face):
        print "Debug: face.flag=0x%x (bitflags)" % face.flag
        if face.flag & NMesh.FaceFlags.SELECT == NMesh.FaceFlags.SELECT:
            print "Debug: face.flag.SELECT=true"
        #endif

        print "Debug: face.mode=0x%x (bitflags)" % face.mode
        if (face.mode & NMesh.FaceModes.TWOSIDE) == NMesh.FaceModes.TWOSIDE:
            print "Debug: face.mode twosided"
        #endif

        print "Debug: face.transp=0x%x (enum)" % face.transp
        if face.transp == NMesh.FaceTranspModes.SOLID:
            print "Debug: face.transp.SOLID"
        #

        if face.image:
            print "Debug: face.image=%s" % face.image.name
        #endif
        print "Debug: face.materialIndex=%d" % face.materialIndex

    def getVertexColorByIndx(self, mesh, indx):
        for face in mesh.faces:
            j=0
            for vertex in face.v:
                if vertex.index == indx:
                    c=face.col[j]
                #endif
                j=j+1
            #endfor
        #endfor
        return c

    def meshToString(self,mesh):
        print "Debug: mesh.hasVertexUV=%d" % mesh.hasVertexUV()
        print "Debug: mesh.hasFaceUV=%d" % mesh.hasFaceUV()
        print "Debug: mesh.hasVertexColours=%d" % mesh.hasVertexColours()
        print "Debug: mesh.verts=%d" % len(mesh.verts)
        print "Debug: mesh.faces=%d" % len(mesh.faces)
        print "Debug: mesh.materials=%d" % len(mesh.materials)

    def rgbToFS(self, c):
        s="%s %s %s" % (
            round(c.r/255.0,self.cp),
            round(c.g/255.0,self.cp),
            round(c.b/255.0,self.cp))
        return s

    def computeDirection(self, object):
        x,y,z=(0,-1.0,0) # point down
        ax,ay,az = (object.RotX,object.RotZ,object.RotY)

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
    
    def rotVertex(self, mm, v):
        lx,ly,lz=v[0],v[1],v[2]
        gx=(mm[0][0]*lx + mm[1][0]*ly + mm[2][0]*lz) + mm[3][0]
        gy=((mm[0][2]*lx + mm[1][2]*ly+ mm[2][2]*lz) + mm[3][2])
        gz=-((mm[0][1]*lx + mm[1][1]*ly + mm[2][1]*lz) + mm[3][1])
        rotatedv=[gx,gy,gz]
        return rotatedv

    def writeIndented(self, s, inc=0):
        if inc < 1:
            self.indentLevel = self.indentLevel + inc
        #endif

        spaces=""
        for x in xrange(self.indentLevel):
            spaces = spaces + "   "
        #endfor
        self.file.write(spaces + s)

        if inc > 0:
            self.indentLevel = self.indentLevel + inc
        #endif

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
        #endif
        
        angle = 2 * math.acos(Qf[0])
        
        result = [ax, ay, az, angle]
        return result

def file_callback(filename):
    if filename.find('.wrl', -4) < 0: filename += '.wrl'
    wrlexport=VRML2Export(filename)
    scene = Blender.Scene.getCurrent()
    wrlexport.export(scene)
#enddef
    
#------------------------------------------------------------------------
# main routine
#------------------------------------------------------------------------
try:
    ARG = __script__['arg'] # user selected argument
except:
    print "older version"

if Blender.Get('version') < 225:
    print "Warning: VRML2 export failed, wrong blender version!"
    print " You aren't running blender version 2.25 or greater"
    print " download a newer version from http://blender.org/"
else:
    if ARG == 'all' or ARG == 'selected':
        Blender.Window.FileSelector(file_callback,"Export VRML 2.0")
    else:
        baseFileName=Blender.Get('filename')
        if baseFileName.find('.') != -1:
            dots=Blender.Get('filename').split('.')[0:-1]
        else:
            dots=[baseFileName]
        #endif
        dots+=["wrl"]
        vrmlFile=".".join(dots)

        file_callback(vrmlFile)
    #endif
#endif
