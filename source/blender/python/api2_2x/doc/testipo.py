import Blender

ipo = Blender.Ipo.Get()
print "Available ipos : ",ipo

ipo = Blender.Ipo.Get('ObIpo')# hope there is an ipo named "ObIpo"...

    
for prop in  ["Name","Blocktype","Showkey","Pad","Rctf"]:
    val = eval("ipo.get%s()"%prop)
    print prop,"-->",val
    #exec("ipo.set%s(%s)"%(prop,val))
      
try :
    val = ipo.getCurveBP(0)
    print "CurveBP -->",val
except  : print "error BP"      
val = ipo.getNcurves()
print "NCurves -->",val

curvebeztriple = ipo.getCurveBeztriple(0,0)
print "curvebeztriple",curvebeztriple
ipo.setCurveBeztriple(0,0,[1,2,3,4,5,6,7,8,9])
print  ipo.getCurveBeztriple(0,0)
