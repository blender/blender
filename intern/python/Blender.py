#! /usr/bin/env python

#######################
# (c) Jan Walter 2000 #
#######################

# CVS
# $Author$
# $Date$
# $RCSfile$
# $Revision$

"""This is the Python API for Blender"""

def _findNewName(name, names):
    import string
    words = string.split(name, ".")
    basename = words[0]
    newname  = name
    num = 1
    while newname in names:
        newname = basename + ".%03d" % num
        num = num + 1
    return newname

###################
# Blender classes #
###################

class Camera:
    def __init__(self, name, Lens = 35.0, ClipSta = 0.1, ClipEnd = 100.0):
        self.name = name
        self.ipos = {}
        self.Lens = Lens
        self.ClipSta = ClipSta
        self.ClipEnd = ClipEnd

class Curve:
    def __init__(self, name):
        # ...
        self.name = name
        self.ipos = {}
        self.materials = []

class Ika:
    def __init__(self, name):
        self.name = name

class Ipo:
    def __init__(self, name):
        self.name = name

class Lamp:
    def __init__(self, name, Energ = 1.0, R = 1.0, G = 1.0, B = 1.0,
                 SpoSi = 45.0,
                 OfsX = 0.0, OfsY = 0.0, OfsZ = 0.0,
                 SizeX = 1.0, SizeY = 1.0, SizeZ = 1.0):
        self.name = name
        self.ipos = {}
        self.Energ = Energ
        self.R = R
        self.G = G
        self.B = B
        self.SpoSi = SpoSi
        self.OfsX = OfsX
        self.OfsY = OfsY
        self.OfsZ = OfsZ
        self.SizeX = SizeX
        self.SizeY = SizeY
        self.SizeZ = SizeZ

class Material:
    def __init__(self, name,
                 R = 0.8, G = 0.8, B = 0.8,
                 SpecR = 1.0, SpecG = 1.0, SpecB = 1.0,
                 MirR = 1.0, MirG = 1.0, MirB = 1.0,
                 Ref = 0.8, Alpha = 1.0, Emit = 0.0, Amb = 0.5,
                 Spec = 0.5, Hard = 50):
        self.name = name
        self.ipos = {}
        self.R = R
        self.G = G
        self.B = B
        self.SpecR = SpecR
        self.SpecG = SpecG
        self.SpecB = SpecB
        self.MirR = MirR
        self.MirG = MirG
        self.MirB = MirB
        self.Ref = Ref
        self.Alpha = Alpha
        self.Emit = Emit
        self.Amb = Amb
        self.Spec = Spec
        self.Hard = Hard

class Matrix:
    def __init__(self):
        self.elements = [[1, 0, 0, 0],
                         [0, 1, 0, 0],
                         [0, 0, 1, 0],
                         [0, 0, 0, 1]]

    def __repr__(self):
        str = "%s" % self.elements
        return str

class Mesh:
    """Creates an (empty) instance of a Blender mesh.\n\
    E.g.: "m = Blender.Mesh('Plane')"\n\
    To create faces first add vertices with \n\
    "i1 = m.addVertex(x, y, z, nx, ny, nz, r = -1.0, r = 0.0, b = 0.0)"\n\
    then create faces with "index = m.addFace(i1, i2, i3, i4, isSmooth)"."""

    _meshes = {}

    def __init__(self, name):
        self.name = name
        self.ipos = {}
        self.materials = []
        self.vertices = []
        self.normals = []
        self.colors = []
        self.faces = []
        if name in Mesh._meshes.keys():
            print 'Mesh "%s" already exists ...' % name
            self.name = _findNewName(name, Mesh._meshes.keys())
            print '... so it will be called "%s"' % self.name
        Mesh._meshes[self.name] = self

    def __repr__(self):
        str = 'Mesh(name = "%s",\n' % self.name
        str = str + '     vertices = %s,\n' % len(self.vertices)
        str = str + '     faces = %s)' % len(self.faces)
        return str

    def addFace(self, i1, i2, i3, i4, isSmooth):
        """addFace(self, i1, i2, i3, i4)"""
        self.faces.append([i1, i2, i3, i4, isSmooth])
        return (len(self.faces) - 1)

    def addVertex(self, x, y, z, nx, ny, nz, r = -1.0, g = 0.0, b = 0.0):
        """addVertex(self, x, y, z, nx, ny, nz, r = -1.0, g = 0.0, b = 0.0)"""
        self.vertices.append([x, y, z])
        self.normals.append([nx, ny, nz])
        if r != -1.0:
            self.colors.append([r, g, b])
        return (len(self.vertices) - 1)

class MetaBall:
    def __init__(self, name):
        self.name = name

class Object:
    """Creates an instance of a Blender object"""

    _objects = {}

    def __init__(self, name):
        self.name = name
        self.ipos = {}
        self.materials = []
        self.matrix = Matrix()
        self.data = None
        self.type = None
        if name in Object._objects.keys():
            print 'Object "%s" already exists ...' % name
            self.name = _findNewName(name, Object._objects.keys())
            print '... so it will be called "%s"' % self.name
        Object._objects[self.name] = self

    def __repr__(self):
        str = 'Object(name = "%s",\n' % self.name
        str = str + '       matrix = %s,\n' % self.matrix
        if self.type:
            str = str + '       data = %s("%s"))' % (self.type, self.data)
        else:
            str = str + '       data = None)'
        return str

class Scene:
    """Creates an instance of a Blender scene"""

    _scenes = {}

    def __init__(self, name):
        self.name = name
        self.objects = []
##         self.camera = None
##         self.world = None
        Scene._scenes[self.name] = self

    def __repr__(self):
        str = 'Scene(name = "%s", \n' % self.name
        str = str + '      objects = %s)' % len(self.objects)
        return str

    def addObject(self, object):
        """addObject(self, object)"""
        self.objects.append(object.name)
        return (len(self.objects) - 1)

class Surface:
    def __init__(self, name):
        self.name = name
        self.ipos = {}
        self.materials = []
        # ...

class Text(Surface):
    def __init__(self, name):
        Surface.__init__(name)

##############
# primitives #
##############

def addMesh(type, sceneName):
    """Blender.addMesh(type, sceneName)\n\
    where type is one of ["Plane"]"""

    if type == "Plane":
        object = Object(type)
        mesh = Mesh(type)
        i1 = mesh.addVertex(+1.0, +1.0, 0.0, 0.0, 0.0, 1.0)
        i2 = mesh.addVertex(+1.0, -1.0, 0.0, 0.0, 0.0, 1.0)
        i3 = mesh.addVertex(-1.0, -1.0, 0.0, 0.0, 0.0, 1.0)
        i4 = mesh.addVertex(-1.0, +1.0, 0.0, 0.0, 0.0, 1.0)
        mesh.addFace(i1, i4, i3, i2, 0)
        connect("OB" + object.name, "ME" + mesh.name)
        connect("SC" + sceneName,  "OB" + object.name)
        return object.name, mesh.name
    elif type == "Cube":
        pass
    elif type == "Circle":
        pass
    elif type == "UVsphere":
        pass
    elif type == "Icosphere":
        pass
    elif type == "Cylinder":
        pass
    elif type == "Tube":
        pass
    elif type == "Cone":
        pass
    elif type == "Grid":
        pass
    else:
        raise TypeError

def addCurve(type):
    if type == "Bezier Curve":
        pass
    elif type == "Bezier Circle":
        pass
    elif type == "Nurbs Curve":
        pass
    elif type == "Nurbs Circle":
        pass
    elif type == "Path":
        pass
    else:
        raise TypeError

def addSurface(type):
    if type == "Curve":
        pass
    elif type == "Circle":
        pass
    elif type == "Surface":
        pass
    elif type == "Tube":
        pass
    elif type == "Sphere":
        pass
    elif type == "Donut":
        pass
    else:
        raise TypeError

def connect(objName1, objName2):
    """connect(objName1, objName2)"""

    if objName1[:2] == "OB" and objName2[:2] == "ME":
        obj1 = getObject(objName1[2:])
        obj1.data = objName2[2:]
        obj1.type = "Mesh"
    elif objName1[:2] == "SC" and objName2[:2] == "OB":
        obj1 = getScene(objName1[2:])
        obj2 = getObject(objName2[2:])
        obj1.addObject(obj2)
    else:
        print "ERROR: connect(%s, %s)" % (objName1, objName2)

def getCurrentScene():
    """getCurrentScene()"""

    return Scene._scenes[0]

def getMesh(name):
    """getMesh(name)"""

    if name in Mesh._meshes.keys():
        return Mesh._meshes[name]
    else:
        return None

def getObject(name):
    """getObject(name)"""

    if name in Object._objects.keys():
        return Object._objects[name]
    else:
        return None

def getScene(name):
    """getScene(name)"""

    if name in Scene._scenes.keys():
        return Scene._scenes[name]
    else:
        return None

def testBlender():
    scene = Scene("1")
    print scene
    objName, meshName = addMesh("Plane", "1")
    print scene
    obj = Object("Plane")
    connect("OB" + obj.name, "ME" + meshName)
    connect("SC" + scene.name, "OB" + obj.name)
    print scene
    for name in scene.objects:
        obj = getObject(name)
        print obj
        if obj.type == "Mesh":
            mesh = getMesh(obj.data)
            print mesh
            print mesh.vertices
            print mesh.faces
    Mesh("Plane")
    # print global data
    print Scene._scenes
    print Object._objects
    print Mesh._meshes

if __name__ == "__main__":
    testBlender()
