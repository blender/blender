import Blender


list = Blender.Curve.Get()

print list

cur = Blender.Curve.Get('Curve')


for prop in ["Name","PathLen","Totcol","Flag","Bevresol","Resolu","Resolv","Width","Ext1","Ext2","Loc","Rot","Size"]:
    value =  eval("cur.get%s()"%prop)
    print prop,"-->",value
    exec("cur.set%s(value)"%prop)



for attr in [ "name","pathlen","totcol","flag","bevresol","resolu","resolv","width","ext1","ext2","loc","rot","size"]:
    value = eval("cur.%s"%attr)
    exec("cur.%s = value"%attr)