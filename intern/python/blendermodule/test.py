import Blender

def printModuleInfo():
    # module information
    names = dir(Blender)
    print names
    for name in names:
        execString = ('print "Blender.' + name + ':",\n' +
                      'if type(Blender.' + name + ') == type(""):\n' +
                      '    print Blender.' + name + '\n' +
                      'elif type(Blender.' + name +
                      ') == type(Blender.addMesh) or type(Blender.' + name +
                      ') == type(Blender.Object):\n' +
                      '    print Blender.' + name + '.__doc__\n' +
                      'else:\n' +
                      '    print type(Blender.' + name + ')\n')
        exec execString
    print "#" * 79

def testModule():
    # get current scene
    scene = Blender.getCurrentScene()
    print scene
    # create object and mesh (primitives)
    obj, msh = Blender.addMesh("Plane", scene)
    print "obj ="
    print obj
    print "msh ="
    print msh
    print "vertices:"
    for vertex in msh.vertices:
        print vertex
    print "faces:"
    for face in msh.faces:
        print face
    # create object only and share mesh
    obj2 = Blender.Object("Plane2")
    print obj2
    Blender.connect(obj2, msh)
    Blender.connect(scene, obj2)
    print obj2
    print obj2.data
    print "vertices:"
    for vertex in obj2.data.vertices:
        print vertex
    print "faces:"
    for face in obj2.data.faces:
        print face
    print scene

printModuleInfo()
testModule()
