# Blender.Effect module and the Effect PyType effect

"""
The Blender.Effect submodule

INTRODUCTION

The module effect allows you to access all the data of an effect.
An effect can modify an object (typically a mesh) in three different ways.

a) the build effect : makes the mesh appear progressivly.

b) the wave effect : waves appear on the mesh (which should be fine-grained)

c) the particle effect : every vertex of the mesh emits particles, which can themselves emit new particles. This effect is the most parametrizable.

In the blender internals, the effect object is just a placeholder for the "real"
effect, which can be a wave, particle or build effect. The python API follows
this structure : the Effect module grants access to (the few) data which
are shared between all effects. It has three submodules : Wave, Build, Particle
, which grant r/w access to the real parameters of these effects.

Example::
  import Blender
	listffects = Blender.Effect.Get()
	print listeffects
	eff = listeffects[0]
	#we suppose the first effect is a build effect
	print eff.getLen()
	eff.setLen(500)	
"""

def New (type):
  """
  Creates a new Effect.
  @type type: string
  @param type: Effect type. Can be "wave", "particle" or "build"
  @rtype: Blender Effect
  @return: The created Effect.
  """

def Get (objname,position):
  """
  Get an Effect from Blender.
  @type objname: string
  @param objname: The name of object to which is linked the effect.
  @type position: string
  @param position: The position of the effect in the list of effects liked to the object.
  @rtype: Blender Effect or a list of Blender Effects
  @return: It depends on the 'objname,position' parameters:
      - (objname,position): The Effect linked to the given object at the given position;
      - ():     A list with all Effects in the current scene.
  """


class Effect:
  """
  The Effect object
  =================
  This object gives access to generic data from all effects in Blender.
  Its attributes depend upon its type.
	
  @cvar seed: (Particle effects) seed of the RNG.
  @cvar nabla: (Particle effects) The nabla value .
  @cvar sta: (Particle effects) start time of the effect.
  @cvar end: (Particle effects) end time of the effect
  @cvar lifetime: (Particle and Wave effects)lifetime of the effect
  @cvar normfac: (Particle effects) normal strength of the particles (relatively to mesh).
  @cvar obfac: (Particle effects)initial strength of the particles relatively to objects.
  @cvar randfac: (Particle effects) initial random speed of the particles.
  @cvar texfac: (Particle effects) initial speed of the particles caused by the texture.
  @cvar randlife: (Particle effects) variability of the life of the particles.
  @cvar vectsize: (Particle effects) size of vectors associated to the particles (if any).
  @cvar totpart: (Particle effects) total number of particles.
  @cvar force: (Particle effects) constant force applied to the parts.
  @cvar mult: (Particle effects) probabilities of a particle having a child.
  @cvar child: (Particle effects) number of children a particle may have.
  @cvar mat: (Particle effects) materials used by the 4 generation particles.
  @cvar defvec: (Particle effects)x, y and z axis of the force defined by the texture.
  @cvar sfra: (Build effects)  starting frame of the build effect.
  @cvar len: (Build effects)  length     of the build effect. 
  @cvar timeoffs: (Wave effects)  time offset of the wave effect.  
  @cvar damp: (Wave effects)    damp factor  of the wave effect.   
  @cvar minfac: (Wave effects)   
  @cvar speed: (Wave effects)  speed of the wave effect.    
  @cvar narrow: (Wave effects)narrowness   of the wave effect.   
  @cvar width: (Wave effects) width of the wave effect.  
  @cvar height: (Wave effects)  height of the wave effect.    
  @cvar startx: (Wave effects) x-position of the origin  of the wave effect.   
  @cvar starty: (Wave effects) y-position of the origin  of the wave effect. 
  """

  def getType():
    """
    Retreives the type of an effect object
    @rtype: int 
    @return:  the type of an effect object : 0 = build effect;  1 = wave effect;2 = particle effect;
    """

	
  def setType(name):
    """
    Sets the type of an effect object
    @type name: int
    @param name : the new type. 
    @rtype: PyNone
    @return:  PyNone
    """

  def getFlag():
    """
    Retreives the flag of an effect object
    @rtype: int 
    @return:  the flag of an effect object : 0 = build effect;  1 = wave effect;2 = particle effect;
    """

	
  def setFlag(newflag):
    """
    Sets the flag of an effect object
    @type newflag: int
    @param newflag: the new flag. 
    @rtype: PyNone
    @return:  PyNone
    """

	

  def getLen():
    """
    (Build Effect) Retreives the length of an build effect object
    @rtype: int 
    @return:  the length of the effect.
    """

	
  def setLen(newlength):
    """
    (Build Effect) Sets the length of an build effect object
    @type newlength: int
    @param newlength: the new length. 
    @rtype: PyNone
    @return:  PyNone
    """

	

  def getSfra():
    """
    (Build Effect) Retreives the starting frame of an build effect object
    @rtype: int 
    @return:  the starting frame of the effect.
    """

	
  def setSfra(sfra):
    """
    (Build Effect) Sets the starting frame of an build effect object
    @type sfra: int
    @param sfra: the new starting frame. 
    @rtype: PyNone
    @return:  PyNone
    """

  def getStartx():
    """
    (Wave Effect) Retreives the x-coordinate of the starting point of the wave.
    @rtype: float
    @return:  the x-coordinate of the starting point of the wave.
    """

	
  def setStartx(startx):
    """
    (Wave Effect) Sets the x-coordinate of the starting point of the wave.
    @type startx: float
    @param startx: the new x-coordinate of the starting point of the wave.
    @rtype: PyNone
    @return:  PyNone
    """

	

  def getStarty():
    """
    (Wave Effect) Retreives the y-coordinate of the starting point of the wave.
    @rtype: float
    @return:  the y-coordinate of the starting point of the wave.
    """

	
  def setStarty(starty):
    """
    (Wave Effect) Sets the y-coordinate of the starting point of the wave.
    @type starty: float
    @param starty: the new y-coordinate of the starting point of the wave.
    @rtype: PyNone
    @return:  PyNone
    """

	

  def getHeight():
    """
    (Wave Effect) Retreives the height of the wave.
    @rtype: float
    @return:  the height of the wave.
    """

	
  def setHeight(height):
    """
    (Wave Effect) Sets the height of the wave.
    @type height: float
    @param height:  the height of the wave.
    @rtype: PyNone
    @return:  PyNone
    """


  def getWidth():
    """
    (Wave Effect) Retreives the width of the wave.
    @rtype: float
    @return:  the width of the wave.
    """

	
  def setWidth(width):
    """
    (Wave Effect) Sets the width of the wave.
    @type width: float
    @param width:  the width of the wave.
    @rtype: PyNone
    @return:  PyNone
    """

  def getNarrow():
    """
    (Wave Effect) Retreives the narrowness of the wave.
    @rtype: float
    @return:  the narrowness of the wave.
    """

	
  def setNarrow(narrow):
    """
    (Wave Effect) Sets the narrowness of the wave.
    @type narrow: float
    @param narrow:  the narrowness of the wave.
    @rtype: PyNone
    @return:  PyNone
    """

  def getSpeed():
    """
    (Wave Effect) Retreives the speed of the wave.
    @rtype: float
    @return:  the speed of the wave.
    """

	
  def setSpeed(speed):
    """
    (Wave Effect) Sets the speed of the wave.
    @type speed: float
    @param speed:  the speed of the wave.
    @rtype: PyNone
    @return:  PyNone
    """

	
  def getMinfac():
    """
    (Wave Effect) Retreives the minfac of the wave.
    @rtype: float
    @return:  the minfac of the wave.
    """

	
  def setMinfac(minfac):
    """
    (Wave Effect) Sets the minfac of the wave.
    @type minfac: float
    @param minfac:  the minfac of the wave.
    @rtype: PyNone
    @return:  PyNone
    """

	
  def getDamp():
    """
    (Wave Effect) Retreives the damp of the wave.
    @rtype: float
    @return:  the damp of the wave.
    """

	
  def setDamp(damp):
    """
    (Wave Effect) Sets the damp of the wave.
    @type damp: float
    @param damp:  the damp of the wave.
    @rtype: PyNone
    @return:  PyNone
    """

	
  def getTimeoffs():
    """
    (Wave Effect) Retreives the time offset of the wave.
    @rtype: float
    @return:  the time offset of the wave.
    """

	
  def setTimeoffs(timeoffs):
    """
    (Wave Effect) Sets the time offset of the wave.
    @type timeoffs: float
    @param timeoffs:  the time offset of the wave.
    @rtype: PyNone
    @return:  PyNone
    """

		
  def getLifetime():
    """
    (Wave Effect) Retreives the life time of the wave.
    @rtype: float
    @return:  the life time of the wave.
    """

	
  def setLifetime(lifetime):
    """
    (Wave Effect) Sets the life time of the wave.
    @type lifetime: float
    @param lifetime:  the life time of the wave.
    @rtype: PyNone
    @return:  PyNone
    """


  def getSta():
    """
    (Particle Effect) Retreives the starting time of a particle effect object
    @rtype: float
    @return:  the starting time of the effect.
    """

	
  def setSta(newstart):
    """
    (Particle Effect) Sets the starting time of an particle effect object
    @type newstart: float
    @param newstart: the new starting time. 
    @rtype: PyNone
    @return:  PyNone
    """

  def getEnd():
    """
    (Particle Effect) Retreives the endr time of a particle effect object
    @rtype: float 
    @return:  the end time of the effect.
    """

	
  def setEnd(newendrt):
    """
    (Particle Effect) Sets the end time of an particle effect object
    @type newendrt: float
    @param newendrt: the new end time. 
    @rtype: PyNone
    @return:  PyNone
    """
		
  def getLifetime():
    """
    (Particle Effect) Retreives the lifetime of a particle effect object
    @rtype: float 
    @return:  the lifetime of the effect.
    """

	
  def setLifetime(newlifetime):
    """
    (Particle Effect) Sets the lifetime of a particle effect object
    @type newlifetime: float
    @param newlifetime: the new lifetime. 
    @rtype: PyNone
    @return:  PyNone
    """

  def getNormfac():
    """
    (Particle Effect) Retreives the  normal strength of the particles (relatively to mesh).
    @rtype: float 
    @return:  normal strength of the particles (relatively to mesh).
    """

	
  def setNormfac(newnormfac):
    """
    (Particle Effect) Sets the normal strength of the particles (relatively to mesh).
    @type newnormfac: float
    @param newnormfac: the normal strength of the particles (relatively to mesh). 
    @rtype: PyNone
    @return:  PyNone
    """
		
  def getObfac():
    """
    (Particle Effect) Retreives the initial strength of the particles relatively to objects.
    @rtype: float 
    @return: initial strength of the particles (relatively to mesh).
    """

	
  def setObfac(newobfac):
    """
    (Particle Effect) Sets the initial strength of the particles relatively to objects.
    @type newobfac: float
    @param newobfac: the initial strength of the particles relatively to objects.
    @rtype: PyNone
    @return:  PyNone
    """

  def getRandfac():
    """
    (Particle Effect) Retreives the random  strength applied to the particles.
    @rtype: float 
    @return: random  strength applied to the particles.
    """

	
  def setRandfac(newrandfac):
    """
    (Particle Effect) Sets the random  strength applied to the particles. 
    @type newrandfac: float
    @param newrandfac: the random  strength applied to the particles.
    @rtype: PyNone
    @return:  PyNone
    """

  def getTexfac():
    """
    (Particle Effect) Retreives the strength applied to the particles from the texture of the object.
    @rtype: float 
    @return: strength applied to the particles from the texture of the object.
    """

	
  def setTexfac(newtexfac):
    """
    (Particle Effect) Sets the strength applied to the particles from the texture of the object. 
    @type newtexfac: float
    @param newtexfac: the strength applied to the particles from the texture of the object.
    @rtype: PyNone
    @return:  PyNone
    """

  def getRandlife():
    """
    (Particle Effect) Retreives the  variability of the life of the particles.
    @rtype: float 
    @return: variability of the life of the particles.
    """

	
  def setRandlife(newrandlife):
    """
    (Particle Effect) Sets the variability of the life of the particles.
    @type newrandlife: float
    @param newrandlife: the variability of the life of the particles.
    @rtype: PyNone
    @return:  PyNone
    """

  def getNabla():
    """
    (Particle Effect) Retreives the sensibility of te particles to the variations of the texture.
    @rtype: float 
    @return: sensibility of te particles to the variations of the texture.
    """

	
  def setNabla(newnabla):
    """
    (Particle Effect) Sets the sensibility of te particles to the variations of the texture.
    @type newnabla: float
    @param newnabla: the sensibility of te particles to the variations of the texture.
    @rtype: PyNone
    @return:  PyNone
    """

  def getVectsize():
    """
    (Particle Effect) Retreives the size of the vector which is associated to the particles.
    @rtype: float 
    @return: size of the vector which is associated to the particles.
    """

	
  def setVectsize(newvectsize):
    """
    (Particle Effect) Sets the size of the vector which is associated to the particles.
    @type newvectsize: float
    @param newvectsize: the size of the vector which is associated to the particles.
    @rtype: PyNone
    @return:  PyNone
    """

  def getTotpart():
    """
    (Particle Effect) Retreives the total number of particles.
    @rtype: int 
    @return: the total number of particles.
    """

	
  def setTotpart(newtotpart):
    """
    (Particle Effect) Sets the the total number of particles.
    @type newtotpart: int
    @param newtotpart: the the total number of particles.
    @rtype: PyNone
    @return:  PyNone
    """

  def getTotkey():
    """
    (Particle Effect) Retreives the number of keys associated to the particles (kinda degree of freedom)
    @rtype: int 
    @return: number of keys associated to the particles.
    """

	
  def setTotkey(newtotkey):
    """
    (Particle Effect) Sets the number of keys associated to the particles.
    @type newtotkey: int
    @param newtotkey: number of keys associated to the particles.
    @rtype: PyNone
    @return:  PyNone
    """

  def getSeed():
    """
    (Particle Effect) Retreives the RNG seed.
    @rtype: int 
    @return:  RNG seed.
    """

	
  def setSeed(newseed):
    """
    (Particle Effect) Sets the  RNG seed.
    @type newseed: int
    @param newseed:  RNG seed.
    @rtype: PyNone
    @return:  PyNone
    """

  def getForce():
    """
    (Particle Effect) Retreives the force applied to the particles.
    @rtype: list of three floats 
    @return:   force applied to the particles.
    """

	
  def setForce(newforce):
    """
    (Particle Effect) Sets the force applied to the particles.
    @type newforce: list of 3 floats
    @param newforce:  force applied to the particles.
    @rtype: PyNone
    @return:  PyNone
    """

  def getMult():
    """
    (Particle Effect) Retreives the probabilities of a particle having a child.
    @rtype: list of 4 floats 
    @return:  probabilities of a particle having a child.
    """

	
  def setMult(newmult):
    """
    (Particle Effect) Sets the probabilities of a particle having a child.
    @type newmult: list of 4 floats
    @param newmult:  probabilities of a particle having a child.
    @rtype: PyNone
    @return:  PyNone
    """
		
  def getLife():
    """
    (Particle Effect) Retreives the average life of the particles (4 generations)
    @rtype: list of 4 floats 
    @return: average life of the particles (4 generations)
    """

	
  def setLife(newlife):
    """
    (Particle Effect) Sets the average life of the particles (4 generations).
    @type newlife: list of 4 floats
    @param newlife:  average life of the particles (4 generations).
    @rtype: PyNone
    @return:  PyNone
    """
		
  def getChild():
    """
    (Particle Effect) Retreives the average number of children of the particles (4 generations).
    @rtype: list of 4 floats 
    @return: average number of children of the particles (4 generations).
    """

	
  def setChild(newchild):
    """
    (Particle Effect) Sets the average number of children of the particles (4 generations).
    @type newchild: list of 4 floats
    @param newchild:  average number of children of the particles (4 generations).
    @rtype: PyNone
    @return:  PyNone
    """

  def getMat():
    """
    (Particle Effect) Retreives the indexes of the materials associated to the particles (4 generations).
    @rtype: list of 4 floats 
    @return: indexes of the materials associated to the particles (4 generations).
    """

	
  def setMat(newmat):
    """
    (Particle Effect) Sets the indexes of the materials associated to the particles (4 generations).
    @type newmat: list of 4 floats
    @param newmat:   the indexes of the materials associated to the particles (4 generations).
    @rtype: PyNone
    @return:  PyNone
    """


  def getDefvec():
    """
    (Particle Effect) Retreives the x, y and z components of the force defined by the texture.
    @rtype: list of 3 floats 
    @return: x, y and z components of the force defined by the texture.
    """

	
  def setDefvec(newdefvec):
    """
    (Particle Effect) Sets the x, y and z components of the force defined by the texture.
    @type newdefvec: list of 3 floats
    @param newdefvec:   the x, y and z components of the force defined by the texture.
    @rtype: PyNone
    @return:  PyNone
    """
