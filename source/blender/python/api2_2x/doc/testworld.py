import Blender
#testfile
list = Blender.World.Get()
print "available worlds",list

w = Blender.World.Get("World")

for param in ["Name","Colormodel","Fastcol","Skytype","Mode","Totex","Texact","Mistype","Hor","Zen","Amb","Star","Dof","Mist"]:
    val = eval("w.get%s()"%param)
    print param,val
    val1 = eval("w.set%s(val)"%param)
    print val1


for param in ["name","colormodel","fastcol","skytype","mode","totex","texact","mistype","hor","zen","amb","star","dof","mist"]:
    exec("val  = w.%s"%param)
    print param,val
    exec ("w.%s = val"%param)

    

