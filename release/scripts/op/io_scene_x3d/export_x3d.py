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

# Contributors: bart:neeneenee*de, http://www.neeneenee.de/vrml, Campbell Barton

"""
This script exports to X3D format.

Usage:
Run this script from "File->Export" menu.  A pop-up will ask whether you
want to export only selected or all relevant objects.

Known issues:
    Doesn't handle multiple materials (don't use material indices);<br>
    Doesn't handle multiple UV textures on a single mesh (create a mesh for each texture);<br>
    Can't get the texture array associated with material * not the UV ones;
"""

import math
import os

import bpy
import mathutils

from io_utils import create_derived_objects, free_derived_objects

DEG2RAD=0.017453292519943295
RAD_90D = -(math.pi / 2.0)
MATWORLD= mathutils.Matrix.Rotation(RAD_90D, 4, 'X')

def round_color(col, cp):
    return tuple([round(max(min(c, 1.0), 0.0), cp) for c in col])

####################################
# Global Variables
####################################

filepath = ""
_safeOverwrite = True

extension = ''

##########################################################
# Functions for writing output file
##########################################################

class x3d_class:

    def __init__(self, filepath):
        #--- public you can change these ---
        self.writingcolor = 0
        self.writingtexture = 0
        self.writingcoords = 0
        self.proto = 1
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
        self.filepath=filepath
        self.file = None
        if filepath.lower().endswith('.x3dz'):
            try:
                import gzip
                self.file = gzip.open(filepath, "w")
            except:
                print("failed to import compression modules, exporting uncompressed")
                self.filepath = filepath[:-1] # remove trailing z

        if self.file is None:
            self.file = open(self.filepath, "w")

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
        #bfile = sys.expandpath( Blender.Get('filepath') ).replace('<', '&lt').replace('>', '&gt')
        bfile = repr(os.path.basename(self.filepath).replace('<', '&lt').replace('>', '&gt'))[1:-1] # use outfile name
        self.file.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")
        self.file.write("<!DOCTYPE X3D PUBLIC \"ISO//Web3D//DTD X3D 3.0//EN\" \"http://www.web3d.org/specifications/x3d-3.0.dtd\">\n")
        self.file.write("<X3D version=\"3.0\" profile=\"Immersive\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema-instance\" xsd:noNamespaceSchemaLocation=\"http://www.web3d.org/specifications/x3d-3.0.xsd\">\n")
        self.file.write("<head>\n")
        self.file.write("\t<meta name=\"filename\" content=\"%s\" />\n" % bfile)
        # self.file.write("\t<meta name=\"filename\" content=\"%s\" />\n" % sys.basename(bfile))
        self.file.write("\t<meta name=\"generator\" content=\"Blender %s\" />\n" % bpy.app.version_string)
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
        context = scene.render

        loc, quat, scale = (MATWORLD * mat).decompose()
        angleAxis = tuple(quat.axis) + (quat.angle, )
        self.file.write("<Viewpoint DEF=\"%s\" " % (self.cleanStr(ob.name)))
        self.file.write("description=\"%s\" " % (ob.name))
        self.file.write("centerOfRotation=\"0 0 0\" ")
        self.file.write("position=\"%3.2f %3.2f %3.2f\" " % tuple(loc))
        self.file.write("orientation=\"%3.2f %3.2f %3.2f %3.2f\" " % angleAxis)
        self.file.write("fieldOfView=\"%.3f\" />\n\n" % ob.data.angle)

    def writeFog(self, world):
        if world:
            mtype = world.mist_settings.falloff
            mparam = world.mist_settings
        else:
            return
        if (mtype == 'LINEAR' or mtype == 'INVERSE_QUADRATIC'):
            mtype = 1 if mtype == 'LINEAR' else 2
        # if (mtype == 1 or mtype == 2):
            self.file.write("<Fog fogType=\"%s\" " % self.namesFog[mtype])
            self.file.write("color=\"%s %s %s\" " % round_color(world.horizon_color, self.cp))
            self.file.write("visibilityRange=\"%s\" />\n\n" % round(mparam[2],self.cp))
        else:
            return

    def writeNavigationInfo(self, scene):
        self.file.write('<NavigationInfo headlight="false" visibilityLimit="0.0" type=\'"EXAMINE","ANY"\' avatarSize="0.25, 1.75, 0.75" />\n')

    def writeSpotLight(self, ob, mtx, lamp, world):
        safeName = self.cleanStr(ob.name)
        if world:
            ambi = world.ambient_color
            ambientIntensity = ((ambi[0] + ambi[1] + ambi[2]) / 3.0) / 2.5
            del ambi
        else:
            ambientIntensity = 0.0

        # compute cutoff and beamwidth
        intensity=min(lamp.energy/1.75,1.0)
        beamWidth=lamp.spot_size * 0.37;
        # beamWidth=((lamp.spotSize*math.pi)/180.0)*.37;
        cutOffAngle=beamWidth*1.3

        dx, dy, dz = self.computeDirection(mtx)

        #location=(ob.matrix_world*MATWORLD).translation_part() # now passed
        location=(MATWORLD * mtx).translation_part()

        radius = lamp.distance*math.cos(beamWidth)
        # radius = lamp.dist*math.cos(beamWidth)
        self.file.write("<SpotLight DEF=\"%s\" " % safeName)
        self.file.write("radius=\"%s\" " % (round(radius,self.cp)))
        self.file.write("ambientIntensity=\"%s\" " % (round(ambientIntensity,self.cp)))
        self.file.write("intensity=\"%s\" " % (round(intensity,self.cp)))
        self.file.write("color=\"%s %s %s\" " % round_color(lamp.color, self.cp))
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
        dx, dy, dz = self.computeDirection(mtx)
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

        location= (MATWORLD * mtx).translation_part()

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
            # location=(MATWORLD * ob.matrix_world).translation_part()
            location=(MATWORLD * mtx).translation_part()
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
        meshName = self.cleanStr(ob.name)

        meshME = self.cleanStr(ob.data.name) # We dont care if its the mesh name or not
        # meshME = self.cleanStr(ob.getData(mesh=1).name) # We dont care if its the mesh name or not
        if len(mesh.faces) == 0: return
        mode = []
        # mode = 0
        if mesh.uv_textures.active:
        # if mesh.faceUV:
            for face in mesh.uv_textures.active.data:
            # for face in mesh.faces:
                if face.use_halo and 'HALO' not in mode:
                    mode += ['HALO']
                if face.use_billboard and 'BILLBOARD' not in mode:
                    mode += ['BILLBOARD']
                if face.use_object_color and 'OBJECT_COLOR' not in mode:
                    mode += ['OBJECT_COLOR']
                if face.use_collision and 'COLLISION' not in mode:
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
        # TF_TILES is marked as deprecated in DNA_meshdata_types.h
        # elif mode & Mesh.FaceModes.TILES and self.tilenode == 0:
        # 	self.tilenode = 1
        elif 'COLLISION' not in mode and self.collnode == 0:
        # elif not mode & Mesh.FaceModes.DYNAMIC and self.collnode == 0:
            self.writeIndented("<Collision enabled=\"false\">\n",1)
            self.collnode = 1

        nIFSCnt=self.countIFSSetsNeeded(mesh, imageMap, sided)

        if nIFSCnt > 1:
            self.writeIndented("<Group DEF=\"%s%s\">\n" % ("G_", meshName),1)

        if 'two' in sided and sided['two'] > 0:
            bTwoSided=1
        else:
            bTwoSided=0

        mtx = MATWORLD * mtx

        loc= mtx.translation_part()
        sca= mtx.scale_part()
        quat = mtx.to_quat()
        rot= quat.axis

        self.writeIndented('<Transform DEF="%s" translation="%.6f %.6f %.6f" scale="%.6f %.6f %.6f" rotation="%.6f %.6f %.6f %.6f">\n' % \
                           (meshName, loc[0], loc[1], loc[2], sca[0], sca[1], sca[2], rot[0], rot[1], rot[2], quat.angle) )
        # self.writeIndented('<Transform DEF="%s" translation="%.6f %.6f %.6f" scale="%.6f %.6f %.6f" rotation="%.6f %.6f %.6f %.6f">\n' % \
        #   (meshName, loc[0], loc[1], loc[2], sca[0], sca[1], sca[2], rot[0], rot[1], rot[2], quat.angle*DEG2RAD) )

        self.writeIndented("<Shape>\n",1)
        hasImageTexture = False
        is_smooth = False

        # XXX, lame, only exports first material.
        mat_first = None
        for mat_first in mesh.materials:
            if mat_first:
                break

        if mat_first or mesh.uv_textures.active:
            self.writeIndented("<Appearance>\n", 1)
            # right now this script can only handle a single material per mesh.
            if mat_first and mat_first.use_face_texture == False:
                self.writeMaterial(mat_first, self.cleanStr(mat_first.name, ""), world)
                if len(mesh.materials) > 1:
                    print("Warning: mesh named %s has multiple materials" % meshName)
                    print("Warning: only one material per object handled")

            if mat_first is None or mat_first.use_face_texture:
                #-- textures
                image = None
                if mesh.uv_textures.active:
                    for face in mesh.uv_textures.active.data:
                        if face.use_image:
                            image = face.image
                            if image:
                                self.writeImageTexture(image)
                                break

                # XXX, incorrect, uses last image
                if image:
                    hasImageTexture = True

                    if self.tilenode == 1:
                        self.writeIndented("<TextureTransform	scale=\"%s %s\" />\n" % (image.xrep, image.yrep))
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
                if face.use_smooth:
                    is_smooth = True
                    break
            if is_smooth == True:
                creaseAngle=(mesh.auto_smooth_angle)*(math.pi/180.0)
                # creaseAngle=(mesh.degr)*(math.pi/180.0)
                self.file.write("creaseAngle=\"%s\" " % (round(creaseAngle,self.cp)))

            #--- output textureCoordinates if UV texture used
            if mesh.uv_textures.active:
                self.writeTextureCoordinates(mesh)
            if mesh.vertex_colors.active:
                self.writeFaceColors(mesh)
            #--- output coordinates
            self.writeCoordinates(ob, mesh, meshName, EXPORT_TRI)

            self.writingcoords = 1
            self.writingtexture = 1
            self.writingcolor = 1
            self.writeCoordinates(ob, mesh, meshName, EXPORT_TRI)

            #--- output textureCoordinates if UV texture used
            if mesh.uv_textures.active:
                self.writeTextureCoordinates(mesh)
            if mesh.vertex_colors.active:
                self.writeFaceColors(mesh)
            #--- output vertexColors

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
                fv = face.vertices
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
            # mesh.transform(ob.matrix_world)
            self.writeIndented("<Coordinate DEF=\"%s%s\" \n" % ("coord_",meshName), 1)
            self.file.write("\t\t\t\tpoint=\"")
            for v in mesh.vertices:
                self.file.write("%.6f %.6f %.6f, " % tuple(v.co))
            self.file.write("\" />")
            self.writeIndented("\n", -1)

    def writeTextureCoordinates(self, mesh):
        texCoordList=[]
        texIndexList=[]
        j=0

        for face in mesh.uv_textures.active.data:
        # for face in mesh.faces:
            # workaround, since tface.uv iteration is wrong atm
            uvs = face.uv
            # uvs = [face.uv1, face.uv2, face.uv3, face.uv4] if face.vertices[3] else [face.uv1, face.uv2, face.uv3]

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
        elif mesh.vertex_colors.active:
        # else:
            self.writeIndented("<Color color=\"", 1)
            for face in mesh.vertex_colors.active.data:
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

        self.matNames[matName] = 1

        emit = mat.emit
        ambient = mat.ambient / 3.0
        diffuseColor = tuple(mat.diffuse_color)
        if world:
            ambiColor = tuple(((c * mat.ambient) * 2.0) for c in world.ambient_color)
        else:
            ambiColor = 0.0, 0.0, 0.0

        emitColor = tuple(((c * emit) + ambiColor[i]) / 2.0 for i, c in enumerate(diffuseColor))
        shininess = mat.specular_hardness / 512.0
        specColor = tuple((c + 0.001) / (1.25 / (mat.specular_intensity + 0.001)) for c in mat.specular_color)
        transp = 1.0 - mat.alpha

        if mat.use_shadeless:
            ambient = 1.0
            shininess = 0.0
            specColor = emitColor = diffuseColor

        self.writeIndented("<Material DEF=\"MA_%s\" " % matName, 1)
        self.file.write("diffuseColor=\"%s %s %s\" " % round_color(diffuseColor, self.cp))
        self.file.write("specularColor=\"%s %s %s\" " % round_color(specColor, self.cp))
        self.file.write("emissiveColor=\"%s %s %s\" \n" % round_color(emitColor, self.cp))
        self.writeIndented("ambientIntensity=\"%s\" " % (round(ambient, self.cp)))
        self.file.write("shininess=\"%s\" " % (round(shininess, self.cp)))
        self.file.write("transparency=\"%s\" />" % (round(transp, self.cp)))
        self.writeIndented("\n",-1)

    def writeImageTexture(self, image):
        name = image.name
        filepath = os.path.basename(image.filepath)
        if name in self.texNames:
            self.writeIndented("<ImageTexture USE=\"%s\" />\n" % self.cleanStr(name))
            self.texNames[name] += 1
        else:
            self.writeIndented("<ImageTexture DEF=\"%s\" " % self.cleanStr(name), 1)
            self.file.write("url=\"%s\" />" % filepath)
            self.writeIndented("\n",-1)
            self.texNames[name] = 1

    def writeBackground(self, world, alltextures):
        if world:	worldname = world.name
        else:		return
        blending = world.use_sky_blend, world.use_sky_paper, world.use_sky_real

        grd_triple = round_color(world.horizon_color, self.cp)
        sky_triple = round_color(world.zenith_color, self.cp)
        mix_triple = round_color(((grd_triple[i] + sky_triple[i]) / 2.0 for i in range(3)), self.cp)

        self.file.write("<Background ")
        if worldname not in self.namesStandard:
            self.file.write("DEF=\"%s\" " % self.secureName(worldname))
        # No Skytype - just Hor color
        if blending == (False, False, False):
            self.file.write("groundColor=\"%s %s %s\" " % grd_triple)
            self.file.write("skyColor=\"%s %s %s\" " % grd_triple)
        # Blend Gradient
        elif blending == (True, False, False):
            self.file.write("groundColor=\"%s %s %s, " % grd_triple)
            self.file.write("%s %s %s\" groundAngle=\"1.57, 1.57\" " % mix_triple)
            self.file.write("skyColor=\"%s %s %s, " % sky_triple)
            self.file.write("%s %s %s\" skyAngle=\"1.57, 1.57\" " % mix_triple)
        # Blend+Real Gradient Inverse
        elif blending == (True, False, True):
            self.file.write("groundColor=\"%s %s %s, " % sky_triple)
            self.file.write("%s %s %s\" groundAngle=\"1.57, 1.57\" " % mix_triple)
            self.file.write("skyColor=\"%s %s %s, " % grd_triple)
            self.file.write("%s %s %s\" skyAngle=\"1.57, 1.57\" " % mix_triple)
        # Paper - just Zen Color
        elif blending == (False, False, True):
            self.file.write("groundColor=\"%s %s %s\" " % sky_triple)
            self.file.write("skyColor=\"%s %s %s\" " % sky_triple)
        # Blend+Real+Paper - komplex gradient
        elif blending == (True, True, True):
            self.writeIndented("groundColor=\"%s %s %s, " % sky_triple)
            self.writeIndented("%s %s %s\" groundAngle=\"1.57, 1.57\" " % grd_triple)
            self.writeIndented("skyColor=\"%s %s %s, " % sky_triple)
            self.writeIndented("%s %s %s\" skyAngle=\"1.57, 1.57\" " % grd_triple)
        # Any Other two colors
        else:
            self.file.write("groundColor=\"%s %s %s\" " % grd_triple)
            self.file.write("skyColor=\"%s %s %s\" " % sky_triple)

        alltexture = len(alltextures)

        for i in range(alltexture):
            tex = alltextures[i]

            if tex.type != 'IMAGE' or tex.image is None:
                continue

            namemat = tex.name
            # namemat = alltextures[i].name

            pic = tex.image

            # using .expandpath just in case, os.path may not expect //
            basename = os.path.basename(bpy.path.abspath(pic.filepath))

            pic = alltextures[i].image
            if (namemat == "back") and (pic != None):
                self.file.write("\n\tbackUrl=\"%s\" " % basename)
            elif (namemat == "bottom") and (pic != None):
                self.writeIndented("bottomUrl=\"%s\" " % basename)
            elif (namemat == "front") and (pic != None):
                self.writeIndented("frontUrl=\"%s\" " % basename)
            elif (namemat == "left") and (pic != None):
                self.writeIndented("leftUrl=\"%s\" " % basename)
            elif (namemat == "right") and (pic != None):
                self.writeIndented("rightUrl=\"%s\" " % basename)
            elif (namemat == "top") and (pic != None):
                self.writeIndented("topUrl=\"%s\" " % basename)
        self.writeIndented("/>\n\n")

##########################################################
# export routine
##########################################################

    def export(self, scene, world, alltextures,\
            EXPORT_APPLY_MODIFIERS = False,\
            EXPORT_TRI=				False,\
        ):

        print("Info: starting X3D export to %r..." % self.filepath)
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


        for ob_main in [o for o in scene.objects if o.is_visible(scene)]:
        # for ob_main in scene.objects.context:

            free, derived = create_derived_objects(scene, ob_main)

            if derived is None: continue

            for ob, ob_mat in derived:
            # for ob, ob_mat in BPyObject.getDerivedObjects(ob_main):
                objType=ob.type
                objName=ob.name
                if objType == 'CAMERA':
                    self.writeViewpoint(ob, ob_mat, scene)
                elif objType in ('MESH', 'CURVE', 'SURF', 'FONT') :
                    if EXPORT_APPLY_MODIFIERS or objType != 'MESH':
                        me = ob.create_mesh(scene, EXPORT_APPLY_MODIFIERS, 'PREVIEW')
                    else:
                        me = ob.data

                    self.writeIndexedFaceSet(ob, me, ob_mat, world, EXPORT_TRI = EXPORT_TRI)

                    # free mesh created with create_mesh()
                    if me != ob.data:
                        bpy.data.meshes.remove(me)

                elif objType == 'LAMP':
                    data= ob.data
                    datatype=data.type
                    if datatype == 'POINT':
                        self.writePointLight(ob, ob_mat, data, world)
                    elif datatype == 'SPOT':
                        self.writeSpotLight(ob, ob_mat, data, world)
                    elif datatype == 'SUN':
                        self.writeDirectionalLight(ob, ob_mat, data, world)
                    else:
                        self.writeDirectionalLight(ob, ob_mat, data, world)
                else:
                    #print "Info: Ignoring [%s], object type [%s] not handle yet" % (object.name,object.getType)
                    pass

            if free:
                free_derived_objects(ob_main)

        self.file.write("\n</Scene>\n</X3D>")

        # if EXPORT_APPLY_MODIFIERS:
        # 	if containerMesh:
        # 		containerMesh.vertices = None

        self.cleanup()

##########################################################
# Utility methods
##########################################################

    def cleanup(self):
        self.file.close()
        self.texNames={}
        self.matNames={}
        self.indentLevel=0
        print("Info: finished X3D export to %r" % self.filepath)

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

    def countIFSSetsNeeded(self, mesh, imageMap, sided):
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

        if mesh.uv_textures.active:
        # if mesh.faceUV:
            for face in mesh.uv_textures.active.data:
            # for face in mesh.faces
                sidename = "two" if face.use_twoside else "one"

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

        print("Debug: face.transp=0x%x (enum)" % face.blend_type)
        if face.blend_type == Mesh.FaceTranspModes.SOLID:
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
        print("Debug: mesh.vertices=%d" % len(mesh.vertices))
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
        return (mathutils.Vector((0.0, 0.0, -1.0)) * (MATWORLD * mtx).rotation_part()).normalize()[:]

    # swap Y and Z to handle axis difference between Blender and VRML
    #------------------------------------------------------------------------
    def rotatePointForVRML(self, v):
        return v[0], v[2], -v[1]

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

##########################################################
# Callbacks, needed before Main
##########################################################

def save(operator, context, filepath="",
          use_apply_modifiers=False,
          use_triangulate=False,
          use_compress=False):

    if use_compress:
        if not filepath.lower().endswith('.x3dz'):
            filepath = '.'.join(filepath.split('.')[:-1]) + '.x3dz'
    else:
        if not filepath.lower().endswith('.x3d'):
            filepath = '.'.join(filepath.split('.')[:-1]) + '.x3d'

    scene = context.scene
    world = scene.world

    if bpy.ops.object.mode_set.poll():
        bpy.ops.object.mode_set(mode='OBJECT')

    # XXX these are global textures while .Get() returned only scene's?
    alltextures = bpy.data.textures
    # alltextures = Blender.Texture.Get()

    wrlexport = x3d_class(filepath)
    wrlexport.export(scene,
                     world,
                     alltextures,
                     EXPORT_APPLY_MODIFIERS=use_apply_modifiers,
                     EXPORT_TRI=use_triangulate,
                     )

    return {'FINISHED'}

