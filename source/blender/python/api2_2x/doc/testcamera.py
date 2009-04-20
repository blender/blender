# Blender.Camera module and Camera PyType test file
# This also works with Lamp and Material, simply uncomment the right
# line below

MODULE = "Camera"
#MODULE = "Lamp"
#MODULE = "Material"
BPY_OBJECT = MODULE

LONG_STRING = "Supercalifragilisticspialidous"

import types, sys
import Blender
exec ("from Blender import %s" % MODULE)

def PRINT_HEADER(header, sep):
  print "\n", sep * 79 
  print header
  print sep * 79

def PRINT_UNDERLINED(str):
  print "\n", str
  print "-" * len(str)

def PRINT_AND_RM(arg, branch, d):
  for a in arg:
    if a in d:
      d.remove(a)
      print "\n%s.%s:" % (branch, a),
      exec("print %s.%s" % (branch, a))

PRINT_HEADER("Testing the Blender.%s module" % MODULE, '=')

exec ("Module_dir = dir (%s)" % MODULE)
print "\ndir (%s):" % MODULE
print Module_dir

PRINT_AND_RM (["__name__", "__doc__"], MODULE, Module_dir)

for item in Module_dir:
  hooked = 0
  branch = "%s.%s" % (MODULE, item)
  PRINT_HEADER(branch, "-")
  exec ("item_type = type (%s)" % branch)
  print item_type
  exec ("sub_dir = dir(%s)" % branch)
  PRINT_AND_RM (["__name__", "__doc__"], branch, sub_dir)
  if item_type == types.BuiltinFunctionType:
    PRINT_UNDERLINED ("Executing %s:" % branch)
    exec ("result = %s()" % branch)
    print "Returned value is: ", result
    if item in ["Get", "get"] and not hooked:
      if len(result):
        obj = result[0]
        hooked = 1

if hooked:
  PRINT_HEADER(obj, "=")
  exec ("obj_dir = dir(obj)")
  print "\ndir():"
  print obj_dir

  methods = []
  member_vars = []

  for item in obj_dir:
    exec ("item_type = type (obj.%s)" % item)
    if item_type == types.BuiltinMethodType:
      methods.append(item)
    else:
      member_vars.append(item)

  PRINT_HEADER("%s Methods" % BPY_OBJECT, '-')
  if methods: print methods
  else: print "XXX No methods found in %s" % BPY_OBJECT

  PRINT_HEADER("%s Member Variables" % BPY_OBJECT, '-')
  if member_vars:
    for m in member_vars:
      PRINT_UNDERLINED(m)
      exec ("mvalue = obj.%s" % m)
      exec ("mtype = type (obj.%s)" % m)
      mtype = str(mtype).split("'")[1]
      print "%s: %s" % (mtype, mvalue)

      M = m[0].upper() + m[1:]
      setM = "set%s" % M
      getM = "get%s" % M
      if setM in methods:
        print "There is a .%s() method." % setM
        methods.remove(setM)
        if mtype == 'str':
          try:
            print "Trying to set string to %s" % LONG_STRING
            exec("obj.%s('%s')" % (setM, LONG_STRING))
            exec("get_str = obj.%s()" % getM)
            print "It returned:", get_str
            len_str = len(get_str)
            if len_str < 100:
              print "It correctly clamped the string to %s chars." % len_str
          except:
            PRINT_HEADER("FAILED in .%s()" % setM, "X")
            print sys.exc_info()[0]
        elif mtype == 'float':
          try:
            exec("obj.%s(%d)" % (setM, -999999))
            exec("result = obj.%s()" % getM)
            print "%s's minimum value is %f" % (m, result)
            exec("obj.%s(%d)" % (setM, 999999))
            exec("result = obj.%s()" % getM)
            print "%s's maximum value is %f" % (m, result)
          except:
            PRINT_HEADER("FAILED in %s or %s" % (setM, getM), "X")
            print sys.exc_info()[0]
        elif mtype == 'int':
          try:
            dict = M+"s"
            if dict in member_vars:
              exec("key = obj.%s.keys()[1]" % dict)
              exec("obj.%s('%s')" % (setM, key))
            exec("result = obj.%s()" % getM)
          except:
            PRINT_HEADER("FAILED in %s or %s" % (setM, getM), "X")
            print sys.exc_info()[0]

      if getM in methods:
        print "There is a .%s() method." % getM,
        methods.remove(getM)
        exec("result = obj.%s()" % getM)
        print "It returned:", result

  else: print "XXX No member variables found in %s" % BPY_OBJECT

else: # the module .Get() function found nothing
  PRINT_HEADER("Failed trying to %s.Get() a %s object"
                % (MODULE, BPY_OBJECT), 'X')

