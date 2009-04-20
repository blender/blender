# Blender.Effect module and the Effect PyType effect

"""
The Blender.Effect submodule

B{Deprecated}:
This module is now maintained but not actively developed.

Effect
======

INTRODUCTION

The Effect module allows you to access all the data of particle effects.
An effect can modify a mesh object using particles, where vertex of
the mesh emits particles, which can themselves emit new particles.

In the Blender internals, the effect object is just a placeholder for
the particle effect.  Prior to v2.39 build and wave effects were also
supported by Blender, and the Python API supported all three types of
effects.  They were removed in v2.39 when the build and wave modifiers
were implemented.


Example::
  import Blender
    listffects = Blender.Effect.Get()
    print listeffects
    eff = listeffects[0]
    #we suppose the first effect is a build effect
    print eff.getLen()
    eff.setLen(500)	

@type Flags: read-only dictionary
@var Flags: The particle effect flags.  Values can be ORed.
  - SELECTED: The particle effect is selected in the UI. (Read-only)
  - BSPLINE: Use a B-spline formula for particle interpolation
  - STATIC: Make static particles
  - ANIMATED: Recalculate static particles for each rendered frame
  - VERTS: Emit particles from vertices
  - FACES: Emit particles from faces
  - EVENDIST: Use even distribution based on face area (requires FACES)
  - TRUERAND: Use true random distribution based on face area (requires FACES)
  - UNBORN: Make particles appear before they are emitted
  - DIED: Make particles appear after they have died
  - EMESH: Render emitter mesh

@type SpeedTypes: read-only dictionary
@var SpeedTypes: The available settings for selecting particle speed vectors.
Only one setting is active at a time.
  - INTENSITY: Use texture intensity
  - RGB: Use RGB values
  - GRADIENT: Use texture gradient
"""

def New (name):
  """
  Creates a new particle effect and attaches to an object.
  @type name: string
  @param name: The name of object to associate with the effect.  Only mesh
   objects are supported.
  @rtype: Blender Effect
  @return: the new effect
  """

def Get (name = None, position = None):
  """
  Get an Effect from Blender.
  @type name: string
  @param name: The name of object linked to the effect.
  @type position: int
  @param position: The position of the effect in the list of effects linked to the object.
  @rtype: Blender Effect or a list of Blender Effects
  @return: It depends on the 'objname, position' parameters:
      - ():     A list with all Effects in the current scene;
      - (name): A list with all Effects linked to the given object;
      - (name, position): The Effect linked to the given object at the given position
  """

class Effect:
  """
  The Effect object
  =================
  This object gives access to particle effect data in Blender.

  @ivar child: The number of children a particle may have.
    Values are clamped to the range [1,600].
  @type child: tuple of 4 ints
  @ivar childMat: The materials used by the 4 generation particles.
    Values are clamped to the range [1,16].
  @type childMat: tuple of 4 ints
  @ivar damping: The particle damping factor.  This controls the rate at
    which particles decelerate.
    Values are clamped to the range [0.0,1.0].
  @type damping: float
  @ivar defvec: The x, y and z axis of the force defined by the texture.
    Values are clamped to the range [-1.0,1.0].
  @type defvec: tuple of 3 floats
  @ivar disp: The percentage of particles displayed.
    Value is clamped to the range [0,100].
  @type disp: int
  @ivar dispMat: The material used for the particles.
    Value is clamped to the range [1,16].
  @type dispMat: int
  @ivar emissionTex: The texture used for texture emission.
    Value is clamped to the range [1,10].
  @type emissionTex: int
  @ivar end: The end time of the effect.
    Value is clamped to the range [1.0,30000.0].
  @type end: float
  @ivar flag: The flag bitfield.  See L{Flags} for values.
  @type flag: int
  @ivar force: The constant force applied to the parts.
    Values are clamped to the range [-1.0,1.0].
  @type force: tuple of 3 floats
  @ivar forceTex: The texture used for force.
    Value is clamped to the range [1,10].
  @type forceTex: int
  @ivar jitter: Jitter table distribution: maximum particles per face.
    Values are clamped to the range [0,200].
  @type jitter: int
  @ivar life: The lifetime of of the next generation of particles.
    Values are clamped to the range [1.0,30000.0].
  @type life: tuple of 4 floats
  @ivar lifetime: The lifetime of the effect.
    Value is clamped to the range [1.0,30000.0].
  @type lifetime: float
  @ivar mult: The probabilities of a particle having a child.
    Values are clamped to the range [0.0,1.0].
  @type mult: tuple of 4 floats
  @ivar nabla: The nabla value.
    Value is clamped to the range [0.0001,1.0].
  @type nabla: float
  @ivar normfac: The normal strength of the particles relative to mesh.
    Value is clamped to the range [-2.0,2.0].
  @type normfac: float
  @ivar obfac: The strength of the particles relative to objects.
    Value is clamped to the range [-1.0,1.0].
  @type obfac: float
  @ivar randfac: The initial random speed of the particles.
    Value is clamped to the range [0.0,2.0].
  @type randfac: float
  @ivar randlife: The variability of the life of the particles.
    Value is clamped to the range [0.0,2.0].
  @type randlife: float
  @ivar seed: The seed of the random number generator.
    Value is clamped to the range [0,255].
  @type seed: int
  @ivar speedType: Controls which texture property affects particle speeds.
    See L{SpeedTypes} for values and their meanings.
  @type speedType: int
  @ivar speedVGroup: The name of the vertex group used for speed control.
  @type speedVGroup: str
  @ivar sta: The start time of the effect.
    Value is clamped to the range [-250.0,30000.0].
  @type sta: float
  @ivar staticStep: percentage of skipped particles in static display.
    Value is clamped to the range [1,100].
  @type staticStep: int  
  @ivar stype: The bitfield for vector.
  @type stype: int
  @ivar texfac: The initial speed of the particles caused by the texture.
    Value is clamped to the range [0.0,2.0].
  @type texfac: float
  @ivar totpart: The total number of particles.
    Value is clamped to the range [1,100000].
  @type totpart: int
  @ivar totkey: The total number of key positions.
    Value is clamped to the range [1,100].
  @type totkey: int
  @ivar type: The type of the effect.  Deprecated.
  @type type: int
  @ivar vectsize: The size of vectors associated to the particles (if any).
    Value is clamped to the range [0.0,1.0].
  @type vectsize: float
  @ivar vGroup: The name of the vertex group used for emitted particles.
  @type vGroup: str
  """

  def getType():
    """
    Retrieves the type of an effect object.
    Deprecated, since only particle effects are supported.
    @rtype: int 
    @return:  the type of an effect object : should always return 1
    (particle effect)
    """

  def setType(name):
    """
    Deprecated, since only particle effects are supported.
    @type name: int
    @param name : the new type. 
    @rtype: None
    @return:  None
    """

  def getFlag():
    """
    Retrieves the flag of an effect object.  The flag is a bit-mask.
    @rtype: int 
    @return:  The flag of the effect is a combination of parameters.  See
      L{Flags} for values.

    """

  def setFlag(newflag):
    """
    Sets the flag of an effect object. See L{Flags} for values.
    @type newflag: int
    @param newflag: the new flag. 
    @rtype: None
    @return:  None
    """

  def getStartTime():
    """
    Retrieves the starting time of a particle effect object
    @rtype: float
    @return:  the starting time of the effect.
    """

  def setSta(newstart):
    """
    Sets the starting time of an particle effect object
    @type newstart: float
    @param newstart: the new starting time. 
    @rtype: None
    @return:  None
    """

  def getEndTime():
    """
    Retrieves the end time of a particle effect object
    @rtype: float 
    @return:  the end time of the effect.
    """

  def setEnd(newendrt):
    """
    Sets the end time of an particle effect object
    @type newendrt: float
    @param newendrt: the new end time. 
    @rtype: None
    @return:  None
    """

  def getLifetime():
    """
    Retrieves the lifetime of a particle effect object
    @rtype: float 
    @return:  the lifetime of the effect.
    """

	
  def setLifetime(newlifetime):
    """
    Sets the lifetime of a particle effect object
    @type newlifetime: float
    @param newlifetime: the new lifetime. 
    @rtype: None
    @return:  None
    """

  def getNormfac():
    """
    Retrieves the  normal strength of the particles (relatively to mesh).
    @rtype: float 
    @return:  normal strength of the particles (relatively to mesh).
    """

  def setNormfac(newnormfac):
    """
    Sets the normal strength of the particles (relatively to mesh).
    @type newnormfac: float
    @param newnormfac: the normal strength of the particles (relatively to mesh). 
    @rtype: None
    @return:  None
    """

  def getObfac():
    """
    Retrieves the initial strength of the particles relatively to objects.
    @rtype: float 
    @return: initial strength of the particles (relatively to mesh).
    """

  def setObfac(newobfac):
    """
    Sets the initial strength of the particles relatively to objects.
    @type newobfac: float
    @param newobfac: the initial strength of the particles relatively to objects.
    @rtype: None
    @return:  None
    """

  def getRandfac():
    """
    Retrieves the random  strength applied to the particles.
    @rtype: float 
    @return: random  strength applied to the particles.
    """

  def setRandfac(newrandfac):
    """
    Sets the random  strength applied to the particles. 
    @type newrandfac: float
    @param newrandfac: the random  strength applied to the particles.
    @rtype: None
    @return:  None
    """
    
  def getStype():
    """
    Retrieves the vect state of an effect object.
    @rtype: int 
    @return:  the Stype (Vect) of an effect object : 0 , Vect is not enabled, 1, Vect is enabled 
    (particle effect)
    """

  def setStype(int):
    """
    @type int : int
    @param int : state of the Stype : 0 not enabled, 1 enabled. 
    @rtype: None
    @return:  None
    """

  def getTexfac():
    """
    Retrieves the strength applied to the particles from the texture of the object.
    @rtype: float 
    @return: strength applied to the particles from the texture of the object.
    """

  def setTexfac(newtexfac):
    """
    Sets the strength applied to the particles from the texture of the object. 
    @type newtexfac: float
    @param newtexfac: the strength applied to the particles from the texture of the object.
    @rtype: None
    @return:  None
    """

  def getRandlife():
    """
    Retrieves the  variability of the life of the particles.
    @rtype: float 
    @return: variability of the life of the particles.
    """

  def setRandlife(newrandlife):
    """
    Sets the variability of the life of the particles.
    @type newrandlife: float
    @param newrandlife: the variability of the life of the particles.
    @rtype: None
    @return:  None
    """

  def getNabla():
    """
    Retrieves the sensibility of the particles to the variations of the texture.
    @rtype: float 
    @return: sensibility of the particles to the variations of the texture.
    """

	
  def setNabla(newnabla):
    """
    Sets the sensibility of the particles to the variations of the texture.
    @type newnabla: float
    @param newnabla: the sensibility of the particles to the variations of the texture.
    @rtype: None
    @return:  None
    """

  def getVectsize():
    """
    Retrieves the size of the vector which is associated to the particles.
    @rtype: float 
    @return: size of the vector which is associated to the particles.
    """

	
  def setVectsize(newvectsize):
    """
    Sets the size of the vector which is associated to the particles.
    @type newvectsize: float
    @param newvectsize: the size of the vector which is associated to the particles.
    @rtype: None
    @return:  None
    """

  def getTotpart():
    """
    Retrieves the total number of particles.
    @rtype: int 
    @return: the total number of particles.
    """

	
  def setTotpart(newtotpart):
    """
    Sets the the total number of particles.
    @type newtotpart: int
    @param newtotpart: the the total number of particles.
    @rtype: None
    @return:  None
    """

  def getTotkey():
    """
    Retrieves the number of keys associated to the particles (kind of degree of freedom)
    @rtype: int 
    @return: number of keys associated to the particles.
    """

  def setTotkey(newtotkey):
    """
    Sets the number of keys associated to the particles.
    @type newtotkey: int
    @param newtotkey: number of keys associated to the particles.
    @rtype: None
    @return:  None
    """

  def getSeed():
    """
    Retrieves the random number generator seed.
    @rtype: int 
    @return:  current seed value.
    """

  def setSeed(newseed):
    """
    Sets the  random number generator seed.
    @type newseed: int
    @param newseed:  new seed value.
    @rtype: None
    @return:  None
    """

  def getForce():
    """
    Retrieves the force applied to the particles.
    @rtype: tuple of three floats 
    @return:   force applied to the particles.
    """

  def setForce(newforce):
    """
    Sets the force applied to the particles.
    @type newforce: tuple of 3 floats
    @param newforce:  force applied to the particles.
    @rtype: None
    @return:  None
    """

  def getMult():
    """
    Retrieves the probabilities of a particle having a child.
    @rtype: tuple of 4 floats 
    @return:  probabilities of a particle having a child.
    """

  def setMult(newmult):
    """
    Sets the probabilities of a particle having a child.
    @type newmult: tuple of 4 floats
    @param newmult:  probabilities of a particle having a child.
    @rtype: None
    @return:  None
    """

  def getLife():
    """
    Retrieves the average life of the particles (4 generations)
    @rtype: tuple of 4 floats 
    @return: average life of the particles (4 generations)
    """

  def setLife(newlife):
    """
    Sets the average life of the particles (4 generations).
    @type newlife: tuple of 4 floats
    @param newlife:  average life of the particles (4 generations).
    @rtype: None
    @return:  None
    """

  def getChild():
    """
    Retrieves the average number of children of the particles (4 generations).
    @rtype: tuple of 4 ints 
    @return: average number of children of the particles (4 generations).
    """

  def setChild(newchild):
    """
    Sets the average number of children of the particles (4 generations).
    @type newchild: tuple of 4 ints
    @param newchild:  average number of children of the particles (4 generations).
    @rtype: None
    @return:  None
    """

  def getMat():
    """
    Retrieves the indexes of the materials associated to the particles (4 generations).
    @rtype: tuple of 4 ints 
    @return: indexes of the materials associated to the particles (4 generations).
    """

  def setMat(newmat):
    """
    Sets the indexes of the materials associated to the particles (4 generations).
    @type newmat: tuple of 4 ints
    @param newmat:   the indexes of the materials associated to the particles (4 generations).
    @rtype: None
    @return:  None
    """

  def getDefvec():
    """
    Retrieves the x, y and z components of the force defined by the texture.
    @rtype: tuple of 3 floats 
    @return: x, y and z components of the force defined by the texture.
    """

  def setDefvec(newdefvec):
    """
    Sets the x, y and z components of the force defined by the texture.
    @type newdefvec: tuple of 3 floats
    @param newdefvec:   the x, y and z components of the force defined by the
    texture.
    @rtype: None
    @return:  None
    """

  def getParticlesLoc():
    """
    Gets the location of each particle at the current time in worldspace.
    @rtype: A list of vector or a list of vector lists.
    @return: The coordinates of each particle at the current time.
    If the "Vect" option is enabled a list Vector pairs will be returned with a start and end point for each particle.
    When static particles are enabled, a list of lists will be returned, each item a strand of particles.

    Example::

          import Blender
          from Blender import Effect, Object
          scn= Blender.Scene.GetCurrent()
          ob= scn.getActiveObject()
          effect= ob.effects[0]
          particles= effect.getParticlesLoc()

          # Check that particles are points only (not static and not vectors)
          if not effect.getFlag() & Effect.Flags.STATIC or not effect.getStype():
            for pt in particles: 
              ob_empty= scn.objects.new('Empty')
              ob_empty.setLocation(pt)
          
          else: # Particles will be a list
            for pt in particles:
              for pt_item in pt:
                ob_empty= scn.objects.new('Empty')
                ob_empty.setLocation(pt_item)

    Example::
          # Converts particles into a mesh with edges for strands
          from Blender import Scene, Mathutils, Effect, Mesh, Object
          scn= Scene.GetCurrent()
          ob= scn.getActiveObject()
          me= Mesh.New()
          
          effects= Effect.Get()
          for eff in effects:
              for p in eff.getParticlesLoc():
                  # p is either a vector or a list of vectors. me.verts.extend() will deal with either
                  print p
                  me.verts.extend(p)
          
                  if type(p)==list: # Are we a strand or a pair, then add edges.
                      if len(p)>1:
                          edges= [(i, i+1) for i in range(len(me.verts)-len(p), len(me.verts)-1)]
                          me.edges.extend( edges )
          
          print len(me.verts)
          ob= scn.objects.new(me)
    """
