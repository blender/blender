#! /usr/bin/env python

#######################
# (c) Jan Walter 2000 #
#######################

# CVS
# $Author$
# $Date$
# $RCSfile$
# $Revision$

TAB = "  "

vertices        = []
vertexNormals   = []
faces           = []
materials       = []
switchMaterial  = []
currentMaterial = []
materialIndex = 0

def addVertex(x, y, z):
    global vertices
    vertices.append([x, y, z])
    return len(vertices)

def addVertexNormal(i, j, k):
    global vertexNormals
    vertexNormals.append([i, j, k])
    return len(vertexNormals)

def addFace(vi, ti, ni):
    global faces
    faces.append([vi, ti, ni])
    return len(faces)

def addMaterial(name, Kd, Ks, Ka):
    global materials
    materials.append([name, Kd, Ks, Ka])
    return len(materials)

def findMaterial(name):
    global materials
    for material in materials:
        if material[0] == name:
            return material

def setCurrentMaterial(name):
    global switchMaterial, faces, currentMaterial
    switchMaterial.append(len(faces))
    currentMaterial.append(name)

class OpenInventor:
    def export(self, filename, useNormals = 1, useMaterials = 1):
        global vertices, vertexNormals, faces, materials, switchMaterial
        global currentMaterial, materialIndex
        file = open(filename, "w")
        file.write("#Inventor V2.1 ascii\n\n")
        file.write("Separator {\n")
        ############
        # vertices #
        ############
        file.write("%sCoordinate3 {\n" % (TAB, ))
        file.write("%spoint [ \n" % (TAB*2, ))
        for i in xrange(len(vertices)-1):
            x, y, z = vertices[i]
            file.write("%s        %s %s %s,\n" % (TAB*2, x, y, z))
        x, y, z = vertices[i+1]
        file.write("%s        %s %s %s\n" % (TAB*2, x, y, z))
        file.write("%s      ] \n" % (TAB*2, ))
        file.write("%s}\n" % (TAB, ))
        ###########
        # normals #
        ###########
        if useNormals:
            file.write("%sNormal {\n" % (TAB, ))
            file.write("%svector [ \n" % (TAB*2, ))
            for i in xrange(len(vertexNormals)-1):
                x, y, z = vertexNormals[i]
                file.write("%s         %s %s %s,\n" % (TAB*2, x, y, z))
            x, y, z = vertexNormals[i-1]
            file.write("%s         %s %s %s\n" % (TAB*2, x, y, z))
            file.write("%s       ] \n" % (TAB*2, ))
            file.write("%s}\n" % (TAB, ))
        #########
        # faces #
        #########
        switchMaterial.append(len(faces))
        for si in xrange(len(switchMaterial) - 1):
            i1, i2 = switchMaterial[si], switchMaterial[si+1]
            # --------------
            # write material
            # --------------
            if materials:
                name = currentMaterial[materialIndex]
                material = findMaterial(name)
                if useMaterials:
                    file.write("%sMaterial {\n" % (TAB, ))
                    file.write("%sambientColor  %s %s %s\n" %
                               (TAB*2,
                                material[3][0],
                                material[3][1],
                                material[3][2]))
                    file.write("%sdiffuseColor  %s %s %s\n" %
                               (TAB*2,
                                material[1][0],
                                material[1][1],
                                material[1][2]))
                    file.write("%sspecularColor %s %s %s\n" %
                               (TAB*2,
                                material[2][0],
                                material[2][1],
                                material[2][2]))
                    file.write("%s}\n" % (TAB, ))
            # -----------
            # write faces
            # -----------
            file.write("%sIndexedFaceSet {\n" % (TAB, ))
            # indices for vertices
            file.write("%scoordIndex [ \n" % (TAB*2, ))
            for i in xrange(i1, i2-1):
                indices = faces[i][0]
                istring = ""
                for index in indices:
                    # indices begin with 0 in OpenInventor
                    istring = istring + "%s, " % (index - 1, )
                file.write("%s             %s-1, \n" % (TAB*2, istring))
            indices = faces[i+1][0]
            istring = ""
            for index in indices:
                # indices begin with 0 in OpenInventor
                istring = istring + "%s, " % (index - 1, )
            file.write("%s             %s-1\n" % (TAB*2, istring))
            file.write("%s           ] \n" % (TAB*2, ))
            # update materialIndex
            materialIndex = materialIndex + 1
            if useNormals:
                # indices for normals
                file.write("%snormalIndex [ \n" % (TAB*2, ))
                for i in xrange(i1, i2-1):
                    indices = faces[i][2]
                    istring = ""
                    for index in indices:
                        # indices begin with 0 in OpenInventor
                        istring = istring + "%s, " % (index - 1, )
                    file.write("%s              %s-1, \n" % (TAB*2, istring))
                indices = faces[i+1][2]
                istring = ""
                for index in indices:
                    # indices begin with 0 in OpenInventor
                    istring = istring + "%s, " % (index - 1, )
                file.write("%s              %s-1\n" % (TAB*2, istring))
                file.write("%s            ] \n" % (TAB*2, ))
            file.write("%s}\n" % (TAB, ))
        file.write("}\n")
        file.close()
