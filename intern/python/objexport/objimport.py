#! /usr/bin/env python

#######################
# (c) Jan Walter 2000 #
#######################

# CVS
# $Author$
# $Date$
# $RCSfile$
# $Revision$

import BlendImport
import os
import sys
import string

def usage():
    print "usage: python objimport.py objfile"

def testObjImport(filename):
    print filename
    vcount = 0
    vncount = 0
    fcount = 0
    file = open(filename, "r")
    lines = file.readlines()
    linenumber = 1
    for line in lines:
        words = string.split(line)
        if words and words[0] == "#":
            pass # ignore comments
        elif words and words[0] == "v":
            vcount = vcount + 1
            x = string.atof(words[1])
            y = string.atof(words[2])
            z = string.atof(words[3])
            ##print "Vertex %s: (%s, %s, %s)" % (vcount, x, y, z)
            index = BlendImport.addVertex(x, y, z)
            ##print "addVertex(%s)" % index
        elif words and words[0] == "vn":
            vncount = vncount + 1
            i = string.atof(words[1])
            j = string.atof(words[2])
            k = string.atof(words[3])
            ##print "VertexNormal %s: (%s, %s, %s)" % (vncount, i, j, k)
            index = BlendImport.addVertexNormal(i, j, k)
            ##print "addVertexNormal(%s)" % index
        elif words and words[0] == "f":
            fcount = fcount + 1
            vi = [] # vertex  indices
            ti = [] # texture indices
            ni = [] # normal  indices
            words = words[1:]
            lcount = len(words)
            for index in (xrange(lcount)):
                vtn = string.split(words[index], "/")
                vi.append(string.atoi(vtn[0]))
                if len(vtn) > 1 and vtn[1]:
                    ti.append(string.atoi(vtn[1]))
                if len(vtn) > 2 and vtn[2]:
                    ni.append(string.atoi(vtn[2]))
            ##print "Face %s: (%s, %s, %s)" % (fcount, vi, ti, ni)
            index = BlendImport.addFace(vi, ti, ni)
            ##print "addFace(%s)" % index
        elif words and words[0] == "mtllib":
            # try to export materials
            directory, dummy = os.path.split(filename)
            filename = os.path.join(directory, words[1])
            try:
                file = open(filename, "r")
            except:
                print "no material file %s" % filename
            else:
                file = open(filename, "r")
                line = file.readline()
                while line:
                    words = string.split(line)
                    if words and words[0] == "newmtl":
                        name = words[1]
                        file.readline() # Ns .
                        file.readline() # d .
                        file.readline() # illum .
                        line = file.readline()
                        words = string.split(line)
                        Kd = [string.atof(words[1]),
                              string.atof(words[2]),
                              string.atof(words[3])]
                        line = file.readline()
                        words = string.split(line)
                        Ks = [string.atof(words[1]),
                              string.atof(words[2]),
                              string.atof(words[3])]
                        line = file.readline()
                        words = string.split(line)
                        Ka = [string.atof(words[1]),
                              string.atof(words[2]),
                              string.atof(words[3])]
                        index = BlendImport.addMaterial(name, Kd, Ks, Ka)
                    line = file.readline()
                file.close()
        elif words and words[0] == "usemtl":
            name = words[1]
            BlendImport.setCurrentMaterial(name)
        elif words:
            pass
            ##print "%s: %s" % (linenumber, words)
        linenumber = linenumber + 1
    file.close()
    # export to OpenInventor
    BlendImport.OpenInventor().export("test.iv",
                                      useNormals = 1, useMaterials = 1)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        usage()
    else:
        filename = sys.argv[1]
        testObjImport(filename)
