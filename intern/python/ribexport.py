#######################
# (c) Jan Walter 2000 #
#######################

# CVS
# $Author$
# $Date$
# $RCSfile$
# $Revision$

import Blender
import math

exportAnimations = 0

class RenderManExport:
    def __init__(self, filename):
        self.file    = open(filename, "w")
        self.scene   = None
        self.display = None

    def export(self, scene):
        global exportAnimations

        print "exporting ..."
        self.scene = scene
        self.writeHeader()
        self.display = Blender.getDisplaySettings()
        if exportAnimations:
            for frame in xrange(self.display.startFrame,
                                self.display.endFrame + 1):
                self.writeFrame(frame)
        else:
            self.writeFrame(self.display.currentFrame)
        self.writeEnd()

    def writeCamera(self):
        camobj = self.scene.getCurrentCamera()
        camera = Blender.getCamera(camobj.data)
        factor = self.display.yResolution / float(self.display.xResolution)
        self.file.write('Projection "perspective" "fov" [%s]\n' %
                        (360.0 * math.atan(factor * 16.0 / camera.Lens) /
                         math.pi))
        self.file.write("Clipping %s %s\n" % (camera.ClSta, camera.ClEnd))
        self.file.write("Transform [" +
                        "%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s]\n" %
                        (camobj.inverseMatrix[0][0],
                         camobj.inverseMatrix[0][1],
                         -camobj.inverseMatrix[0][2],
                         camobj.inverseMatrix[0][3],
                         camobj.inverseMatrix[1][0],
                         camobj.inverseMatrix[1][1],
                         -camobj.inverseMatrix[1][2],
                         camobj.inverseMatrix[1][3],
                         camobj.inverseMatrix[2][0],
                         camobj.inverseMatrix[2][1],
                         -camobj.inverseMatrix[2][2],
                         camobj.inverseMatrix[2][3],
                         camobj.inverseMatrix[3][0],
                         camobj.inverseMatrix[3][1],
                         -camobj.inverseMatrix[3][2],
                         camobj.inverseMatrix[3][3]))

    def writeDisplaySettings(self, frame):
        self.file.write("Format %s %s %s\n" % (self.display.xResolution,
                                               self.display.yResolution,
                                               self.display.pixelAspectRatio))
        self.file.write('Display "%s" "file" "rgba"\n' %
                        ("frame" + "%04d" % frame + ".tif"))

    def writeEnd(self):
        self.file.close()
        print "... finished"

    def writeFrame(self, frame):
        print "frame:", frame
        Blender.setCurrentFrame(frame)
        self.file.write("FrameBegin %s\n" % (frame - self.display.startFrame))
        self.writeDisplaySettings(frame)
        self.writeCamera()
        self.writeWorld()
        self.file.write("FrameEnd\n")

    def writeHeader(self):
        self.file.write("##RenderMan RIB-Structure 1.0\n")
        self.file.write("version 3.03\n")

    def writeIdentifier(self, name):
        self.file.write("%s\n" % ("#" * (len(name) + 4)))
        self.file.write("# %s #\n" % name)
        self.file.write("%s\n" % ("#" * (len(name) + 4)))

    def writeLamp(self, name, num):
        self.writeIdentifier(name)
        lampobj = Blender.getObject(name)
        lamp    = Blender.getLamp(lampobj.data)
        x = lampobj.matrix[3][0] / lampobj.matrix[3][3]
        y = lampobj.matrix[3][1] / lampobj.matrix[3][3]
        z = lampobj.matrix[3][2] / lampobj.matrix[3][3]
        self.file.write('LightSource "pointlight" %s ' % num +
                        '"from" [%s %s %s] ' % (x, y, z) +
                        '"lightcolor" [%s %s %s] ' % (lamp.R, lamp.G, lamp.B) +
                        '"intensity" 50\n')

    def writeMatrix(self, matrix):
        self.file.write("Transform [" +
                        "%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s]\n" %
                        (matrix[0][0], matrix[0][1],
                         matrix[0][2], matrix[0][3],
                         matrix[1][0], matrix[1][1],
                         matrix[1][2], matrix[1][3],
                         matrix[2][0], matrix[2][1],
                         matrix[2][2], matrix[2][3],
                         matrix[3][0], matrix[3][1],
                         matrix[3][2], matrix[3][3]))

    def writeObject(self, name):
        if Blender.isMesh(name):
            self.writeIdentifier(name)
            meshobj = Blender.getObject(name)
            mesh    = Blender.getMesh(meshobj.data)
            if mesh.texcoords:
                self.file.write('Surface "paintedplastic" "texturename" ' +
                                '["%s.tif"]\n' % "st")
            else:
                self.file.write('Surface "plastic"\n')
            self.file.write("Color [%s %s %s]\n" % (0.8, 0.8, 0.8))
            self.file.write("AttributeBegin\n")
            self.writeMatrix(meshobj.matrix)
            index = 0
            for face in mesh.faces:
                if meshobj.materials and meshobj.materials[face[5]]:
                    material = Blender.getMaterial(meshobj.materials[face[5]])
                    self.file.write("Color [%s %s %s]\n" %
                                    (material.R, material.G, material.B))
                if face[3]:
                    # quad
                    if face[4]: # smooth
                        # first triangle
                        self.file.write('Polygon "P" [ ')
                        for i in xrange(3):
                            self.file.write("%s %s %s " %
                                            (mesh.vertices[face[i]][0],
                                             mesh.vertices[face[i]][1],
                                             mesh.vertices[face[i]][2]))
                        self.file.write('] "N" [ ')
                        for i in xrange(3):
                            self.file.write("%s %s %s " %
                                            (mesh.normals[face[i]][0],
                                             mesh.normals[face[i]][1],
                                             mesh.normals[face[i]][2]))
                        if mesh.colors:
                            self.file.write('] "Cs" [ ')
                            for i in xrange(3):
                                self.file.write("%s %s %s " %
                                                (mesh.colors[face[i]][0],
                                                 mesh.colors[face[i]][1],
                                                 mesh.colors[face[i]][2]))
                            self.file.write(']\n')
                        if mesh.texcoords:
                            self.file.write('] "st" [ ')
                            for i in xrange(3):
                                self.file.write("%s %s " %
                                                (mesh.texcoords[index+i][0],
                                                 1.0 -
                                                 mesh.texcoords[index+i][1]))
                            self.file.write(']\n')
                        else:
                            self.file.write(']\n')
                        # second triangle
                        self.file.write('Polygon "P" [ ')
                        for i in [0, 2, 3]:
                            self.file.write("%s %s %s " %
                                            (mesh.vertices[face[i]][0],
                                             mesh.vertices[face[i]][1],
                                             mesh.vertices[face[i]][2]))
                        self.file.write('] "N" [ ')
                        for i in [0, 2, 3]:
                            self.file.write("%s %s %s " %
                                            (mesh.normals[face[i]][0],
                                             mesh.normals[face[i]][1],
                                             mesh.normals[face[i]][2]))
                        if mesh.colors:
                            self.file.write('] "Cs" [ ')
                            for i in [0, 2, 3]:
                                self.file.write("%s %s %s " %
                                                (mesh.colors[face[i]][0],
                                                 mesh.colors[face[i]][1],
                                                 mesh.colors[face[i]][2]))
                            self.file.write(']\n')
                        if mesh.texcoords:
                            self.file.write('] "st" [ ')
                            for i in [0, 2, 3]:
                                self.file.write("%s %s " %
                                                (mesh.texcoords[index+i][0],
                                                 1.0 -
                                                 mesh.texcoords[index+i][1]))
                            self.file.write(']\n')
                        else:
                            self.file.write(']\n')
                    else: # not smooth
                        # first triangle
                        self.file.write('Polygon "P" [ ')
                        for i in xrange(3):
                            self.file.write("%s %s %s " %
                                            (mesh.vertices[face[i]][0],
                                             mesh.vertices[face[i]][1],
                                             mesh.vertices[face[i]][2]))
                        if mesh.colors:
                            self.file.write('] "Cs" [ ')
                            for i in xrange(3):
                                self.file.write("%s %s %s " %
                                                (mesh.colors[face[i]][0],
                                                 mesh.colors[face[i]][1],
                                                 mesh.colors[face[i]][2]))
                            self.file.write(']\n')
                        if mesh.texcoords:
                            self.file.write('] "st" [ ')
                            for i in xrange(3):
                                self.file.write("%s %s " %
                                                (mesh.texcoords[index+i][0],
                                                 1.0 -
                                                 mesh.texcoords[index+i][1]))
                            self.file.write(']\n')
                        else:
                            self.file.write(']\n')
                        # second triangle
                        self.file.write('Polygon "P" [ ')
                        for i in [0, 2, 3]:
                            self.file.write("%s %s %s " %
                                            (mesh.vertices[face[i]][0],
                                             mesh.vertices[face[i]][1],
                                             mesh.vertices[face[i]][2]))
                        if mesh.colors:
                            self.file.write('] "Cs" [ ')
                            for i in [0, 2, 3]:
                                self.file.write("%s %s %s " %
                                                (mesh.colors[face[i]][0],
                                                 mesh.colors[face[i]][1],
                                                 mesh.colors[face[i]][2]))
                            self.file.write(']\n')
                        if mesh.texcoords:
                            self.file.write('] "st" [ ')
                            for i in [0, 2, 3]:
                                self.file.write("%s %s " %
                                                (mesh.texcoords[index+i][0],
                                                 1.0 -
                                                 mesh.texcoords[index+i][1]))
                            self.file.write(']\n')
                        else:
                            self.file.write(']\n')
                else:
                    # triangle
                    if face[4]: # smooth
                        self.file.write('Polygon "P" [ ')
                        for i in xrange(3):
                            self.file.write("%s %s %s " %
                                            (mesh.vertices[face[i]][0],
                                             mesh.vertices[face[i]][1],
                                             mesh.vertices[face[i]][2]))
                        self.file.write('] "N" [ ')
                        for i in xrange(3):
                            self.file.write("%s %s %s " %
                                            (mesh.normals[face[i]][0],
                                             mesh.normals[face[i]][1],
                                             mesh.normals[face[i]][2]))
                        if mesh.colors:
                            self.file.write('] "Cs" [ ')
                            for i in xrange(3):
                                self.file.write("%s %s %s " %
                                                (mesh.colors[face[i]][0],
                                                 mesh.colors[face[i]][1],
                                                 mesh.colors[face[i]][2]))
                            self.file.write(']\n')
                        if mesh.texcoords:
                            self.file.write('] "st" [ ')
                            for i in xrange(3):
                                self.file.write("%s %s " %
                                                (mesh.texcoords[index+i][0],
                                                 1.0 -
                                                 mesh.texcoords[index+i][1]))
                            self.file.write(']\n')
                        else:
                            self.file.write(']\n')
                    else: # not smooth
                        self.file.write('Polygon "P" [ ')
                        for i in xrange(3):
                            self.file.write("%s %s %s " %
                                            (mesh.vertices[face[i]][0],
                                             mesh.vertices[face[i]][1],
                                             mesh.vertices[face[i]][2]))
                        if mesh.colors:
                            self.file.write('] "Cs" [ ')
                            for i in xrange(3):
                                self.file.write("%s %s %s " %
                                                (mesh.colors[face[i]][0],
                                                 mesh.colors[face[i]][1],
                                                 mesh.colors[face[i]][2]))
                            self.file.write(']\n')
                        if mesh.texcoords:
                            self.file.write('] "st" [ ')
                            for i in xrange(3):
                                self.file.write("%s %s " %
                                                (mesh.texcoords[index+i][0],
                                                 1.0 -
                                                 mesh.texcoords[index+i][1]))
                            self.file.write(']\n')
                        else:
                            self.file.write(']\n')
                index = index + 4
            self.file.write("AttributeEnd\n")
        else:
            print "Sorry can export meshes only ..."

    def writeWorld(self):
        self.file.write("WorldBegin\n")
        self.file.write('Attribute "light" "shadows" "on"\n')
        # first all lights
        lamps = 0
        for name in self.scene.objects:
            if Blender.isLamp(name):
                lamps = lamps + 1
                self.writeLamp(name, lamps)
        # now all objects which are not a camera or a light
        for name in self.scene.objects:
            if not Blender.isCamera(name) and not Blender.isLamp(name):
                self.writeObject(name)
        self.file.write("WorldEnd\n")

ribexport = RenderManExport("test.rib")
scene = Blender.getCurrentScene()
ribexport.export(scene)
