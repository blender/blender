import Blender




d = Blender.Metaball.Get('Meta')

for prop in ["Name","Wiresize","Thresh","Rendersize","loc","rot","size"]:
    str = "d.get%s()"%prop
    val = eval(str)
    print str,"-->",val
    str1 = "d.set%s(%s)"%(prop,repr(val))
    val1 = eval(str1)
    print str1,"-->",val1



nelem = d.getNMetaElems()
print "getNMetaElems",nelem


for prop in ['lay','type','selcol','flag','pad','x','y','z','expx','expy','expz','rad','rad2','s','len','maxrad2']:
    str = "d.getMeta%s(0)"%prop	
    value = eval(str)
    print str,"-->",value
    str1 = "d.setMeta%s(0,%s)"%(prop,repr(value))
    value1 = eval(str1)
    print str1,"-->",value1
print;print

for field  in ["name","loc","rot","size"]:
    str = "d.%s"%field
    val = eval(str)
    print str,"-->",val
    exec("d.%s = val"%field)


