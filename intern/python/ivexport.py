#######################
# (c) Jan Walter 2000 #
#######################

# CVS
# $Author$
# $Date$
# $RCSfile$
# $Revision$

import Blender

class InventorExport:
    def __init__(self, filename):
        self.file = open(filename, "w")

    def beginObject(self, object):
        self.file.write("  Separator {\n")

    def endObject(self, object):
        self.file.write("  }\n")

    def export(self, scene):
        print "exporting ..."
        self.writeHeader()
        for name in scene.objects:
            object = Blender.getObject(name)
            self.beginObject(object)
            self.writeObject(object)
            self.endObject(object)
        self.writeEnd()

    def writeEnd(self):
        self.file.write("}\n")
        self.file.close()
        print "... finished"

    def writeFaces(self, faces, smooth, colors, materials, texture):
        self.file.write("    IndexedFaceSet {\n")
        # colors
        if colors:
            self.file.write("      vertexProperty VertexProperty {\n")
            self.file.write("        orderedRGBA [\n")
            for color in colors[:-1]:
                r = hex(int(color[0] * 255))
                if len(r) == 3:
                    r = r + "0"
                g = hex(int(color[1] * 255))
                if len(g) == 3:
                    g = g + "0"
                b = hex(int(color[2] * 255))
                if len(b) == 3:
                    b = b + "0"
                colstr = r + g[2:] + b[2:]
                self.file.write("                      %sff,\n" % colstr)
            color = colors[-1]
            r = hex(int(color[0] * 255))
            if len(r) == 3:
                r = r + "0"
            g = hex(int(color[1] * 255))
            if len(g) == 3:
                g = g + "0"
            b = hex(int(color[2] * 255))
            if len(b) == 3:
                b = b + "0"
            colstr = r + g[2:] + b[2:]
            self.file.write("                      %sff\n" % colstr)
            self.file.write("                    ]\n")
            self.file.write("        materialBinding PER_VERTEX_INDEXED\n")
            self.file.write("      }\n")
        # coordinates
        self.file.write("      coordIndex [\n")
        for face in faces[:-1]:
            if face[4] != smooth:
                pass
            elif face[2] == 0 and face[3] == 0:
                print "can't export lines at the moment ..."
            elif face[3] == 0:
                self.file.write("                    %s, %s, %s, -1,\n" %
                                (face[0], face[1], face[2]))
            else:
                self.file.write("                    %s, %s, %s, %s, -1,\n"%
                                (face[0], face[1], face[2], face[3]))
        face = faces[-1]
        if face[4] != smooth:
            pass
        elif face[2] == 0 and face[3] == 0:
            print "can't export lines at the moment ..."
        elif face[3] == 0:
            self.file.write("                    %s, %s, %s, -1,\n" %
                            (face[0], face[1], face[2]))
        else:
            self.file.write("                    %s, %s, %s, %s, -1,\n"%
                            (face[0], face[1], face[2], face[3]))
        self.file.write("                 ]\n")
        # materials
        if not colors and materials:
            self.file.write("      materialIndex [\n")
            for face in faces[:-1]:
                if face[4] != smooth:
                    pass
                else:
                    self.file.write("                      %s,\n" % face[5])
            face = faces[-1]
            if face[4] != smooth:
                pass
            else:
                self.file.write("                      %s\n" % face[5])
            self.file.write("                    ]\n")
        # texture coordinates
        if texture:
            self.file.write("      textureCoordIndex [\n")
            index = 0
            for face in faces:
                if face[3] == 0:
                    self.file.write("                          " +
                                    "%s, %s, %s, -1,\n" %
                                    (index, index+1, index+2))
                else:
                    self.file.write("                          " +
                                    "%s, %s, %s, %s, -1,\n" %
                                    (index, index+1, index+2, index+3))
                index = index + 4
            self.file.write("                        ]\n")
        self.file.write("    }\n")

    def writeHeader(self):
        self.file.write("#Inventor V2.1 ascii\n\n")
        self.file.write("Separator {\n")
        self.file.write("  ShapeHints {\n")
        self.file.write("    vertexOrdering COUNTERCLOCKWISE\n")
        self.file.write("  }\n")

    def writeMaterials(self, materials):
        if materials:
            self.file.write("    Material {\n")
            self.file.write("               diffuseColor [\n")
            for name in materials[:-1]:
                material = Blender.getMaterial(name)
                self.file.write("                              %s %s %s,\n" %
                                (material.R, material.G, material.B))
            name = materials[-1]
            material = Blender.getMaterial(name)
            self.file.write("                              %s %s %s\n" %
                            (material.R, material.G, material.B))
            self.file.write("                            ]\n")
            self.file.write("             }\n")
            self.file.write("    MaterialBinding {\n")
            self.file.write("                      value PER_FACE_INDEXED\n")
            self.file.write("                    }\n")

    def writeMatrix(self, matrix):
        self.file.write("    MatrixTransform {\n")
        self.file.write("      matrix %s %s %s %s\n" %
                        (matrix[0][0], matrix[0][1],
                         matrix[0][2], matrix[0][3]))
        self.file.write("             %s %s %s %s\n" %
                        (matrix[1][0], matrix[1][1],
                         matrix[1][2], matrix[1][3]))
        self.file.write("             %s %s %s %s\n" %
                        (matrix[2][0], matrix[2][1],
                         matrix[2][2], matrix[2][3]))
        self.file.write("             %s %s %s %s\n" %
                        (matrix[3][0], matrix[3][1],
                         matrix[3][2], matrix[3][3]))
        self.file.write("    }\n")

    def writeNormals(self, normals):
        self.file.write("    Normal {\n")
        self.file.write("      vector [\n")
        for normal in normals[:-1]:
            self.file.write("               %s %s %s,\n" %
                            (normal[0], normal[1], normal[2]))
        normal = normals[-1]
        self.file.write("               %s %s %s\n" %
                        (normal[0], normal[1], normal[2]))
        self.file.write("             ]\n")
        self.file.write("    }\n")

    def writeObject(self, object):
        if object.type == "Mesh":
            mesh = Blender.getMesh(object.data)
            self.writeMatrix(object.matrix)
            self.writeMaterials(object.materials)
            self.writeTexture(mesh.texture, mesh.texcoords)
            self.writeVertices(mesh.vertices)
            self.writeFaces(mesh.faces, 0, mesh.colors, object.materials,
                            mesh.texture)
            self.writeNormals(mesh.normals)
            self.writeFaces(mesh.faces, 1, mesh.colors, object.materials,
                            mesh.texture)
        else:
            print "can't export %s at the moment ..." % object.type

    def writeTexture(self, texture, texcoords):
        if texture:
            self.file.write("    Texture2 {\n")
            self.file.write('      filename "%s"\n' % texture)
            self.file.write("    }\n")
            self.file.write("    TextureCoordinate2 {\n")
            self.file.write("      point [\n")
            for texcoord in texcoords:
                self.file.write("              %s %s,\n" %
                                (texcoord[0], texcoord[1]))
            self.file.write("            ]\n")
            self.file.write("    }\n")
            self.file.write("    TextureCoordinateBinding {\n")
            self.file.write("      value PER_VERTEX_INDEXED\n")
            self.file.write("    }\n")

    def writeVertices(self, vertices):
        self.file.write("    Coordinate3 {\n")
        self.file.write("      point [\n")
        for vertex in vertices[:-1]:
            self.file.write("              %s %s %s,\n" %
                            (vertex[0], vertex[1], vertex[2]))
        vertex = vertices[-1]
        self.file.write("              %s %s %s\n" %
                        (vertex[0], vertex[1], vertex[2]))
        self.file.write("            ]\n")
        self.file.write("    }\n")

ivexport = InventorExport("test.iv")
scene = Blender.getCurrentScene()
ivexport.export(scene)
