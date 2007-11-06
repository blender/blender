import Blender

##################  WARNING   ########################
#
# this script tests the Effect module, and its submodules : Wave, Build and Particle
# an object name "Plane" must be created, with three effects attached to it.
# the first effect must be a "Build" effect
# the second effect must be a "Wave" effect
# the third effect must be a "Particle" effect
#
########################################################



effect = Blender.Effect.New()
print effect



build = Blender.Effect.Get('Plane',0)


for param in ['Type','Flag','Len','Sfra'] :
    value = eval("build.get%s()"%param)
    print param,value
    value1 = eval("build.set%s(%s)"%(param,value))
    print value1

for param in ["sfra","len"]:
    str = "build.%s"%param
    value = eval(str)
    print str,value
    exec("build.%s = value"%param)



wave = Blender.Effect.Get('Plane',1)
for param in ['Type','Flag','Startx','Starty','Height', 'Width', 'Narrow', 'Speed', 'Minfac', 'Damp', 'Timeoffs' ,'Lifetime'] :
    value = eval("wave.get%s()"%param)
    print param,value
    value1 = eval("wave.set%s(%s)"%(param,value))
    print value1


for param in ["lifetime","timeoffs","damp","minfac","speed","narrow","width","height","startx","starty"]:
    str = "wave.%s"%param
    value = eval(str)
    print str,value
    exec("wave.%s = value"%param)




particle = Blender.Effect.Get('Plane',2)
for param in ['Type','Flag','StartTime','EndTime','Lifetime','Normfac','Obfac','Randfac','Texfac','Randlife','Nabla','Totpart','Totkey','Seed','Force','Mult','Life','Child','Mat','Defvec'] :
    value = eval("particle.get%s()"%param)
    print param,value
    value1 = eval("particle.set%s(%s)"%(param,value))
    print value1

for param in ['seed','nabla','sta','end','lifetime','normfac','obfac','randfac','texfac','randlife','vectsize','totpart','force','mult','life','child','mat','defvec']:
    str = "particle.%s"%param
    value = eval(str)
    print str,value
    exec("particle.%s = value"%param)