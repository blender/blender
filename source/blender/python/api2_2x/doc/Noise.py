# Blender.Noise submodule

"""
The Blender.Noise submodule.

Noise and Turbulence
====================

This module can be used to generate noise of various types.  This can be used
for terrain generation, to create textures, make animations more 'animated',
object deformation, etc.  As an example, this code segment when scriptlinked
to a framechanged event, will make the camera sway randomly about, by changing
parameters this can look like anything from an earthquake to a very nervous or
maybe even drunk cameraman... (the camera needs an ipo with at least one Loc &
Rot key for this to work!):

Example::
  from Blender import Get, Scene, Noise
  ####################################################
  # This controls jitter speed
  sl = 0.025
  # This controls the amount of position jitter
  sp = 0.1
  # This controls the amount of rotation jitter
  sr = 0.25
  ####################################################

  time = Get('curtime')
  ob = Scene.GetCurrent().getCurrentCamera()
  ps = (sl*time, sl*time, sl*time)
  # To add jitter only when the camera moves, use this next line instead
  #ps = (sl*ob.LocX, sl*ob.LocY, sl*ob.LocZ)
  rv = Noise.vTurbulence(ps, 3, 0, Noise.NoiseTypes.NEWPERLIN)
  ob.dloc = (sp*rv[0], sp*rv[1], sp*rv[2])
  ob.drot = (sr*rv[0], sr*rv[1], sr*rv[2])

@type NoiseTypes: readonly dictionary
@var NoiseTypes: The available noise types.
    - BLENDER
    - STDPERLIN
    - NEWPERLIN
    - VORONOI_F1
    - VORONOI_F2
    - VORONOI_F3
    - VORONOI_F4
    - VORONOI_F2F1
    - VORONOI_CRACKLE
    - CELLNOISE

@type DistanceMetrics: readonly dictionary
@var DistanceMetrics: The available distance metrics values for Voronoi.
    - DISTANCE
    - DISTANCE_SQUARED
    - MANHATTAN
    - CHEBYCHEV
    - MINKOVSKY_HALF
    - MINKOVSKY_FOUR
    - MINKOVISKY
"""

NoiseTypes = {'BLENDER':0, 'STDPERLIN':1}

DistanceMetrics = {'DISTANCE':0}

def random ():
  """
  Returns a random floating point number."
  @rtype: float
  @return: a random number in [0, 1).
  """

def randuvec ():
  """
  Returns a random unit vector.
  @rtype: 3-float list
  @return: a list of three floats.
  """

def setRandomSeed (seed):
  """
  Initializes the random number generator.
  @type seed: int
  @param seed: the seed for the random number generator.  If seed = 0, the
      current time will be used as seed, instead.
  """

def noise (xyz, type = NoiseTypes['STDPERLIN']):
  """
  Returns general noise of the optional specified type.
  @type xyz: tuple of 3 floats
  @param xyz: (x,y,z) float values.
  @type type: int
  @param type: the type of noise to return.  See L{NoiseTypes}.
  @rtype: float
  @return: the generated noise value.
  """

def vNoise (xyz, type = NoiseTypes['STDPERLIN']):
  """
  Returns noise vector of the optional specified type.
  @type xyz: tuple of 3 floats
  @param xyz: (x,y,z) float values.
  @type type: int
  @param type: the type of noise to return.  See L{NoiseTypes}.
  @rtype: 3-float list 
  @return: the generated noise vector.
  """

def turbulence (xyz, octaves, hard, basis = NoiseTypes['STDPERLIN'],
    ampscale = 0.5, freqscale = 2.0):
  """
  Returns general turbulence value using the optional specified noise 'basis'
  function.
  @type xyz: 3-float tuple
  @param xyz: (x,y,z) float values.
  @type octaves: int
  @param octaves: number of noise values added.
  @type hard: bool
  @param hard: noise hardness: 0 - soft noise; 1 - hard noise.  (Returned value
      is always positive.)
  @type basis: int
  @param basis: type of noise used for turbulence, see L{NoiseTypes}.
  @type ampscale: float
  @param ampscale: amplitude scale value of the noise frequencies added.
  @type freqscale: float
  @param freqscale: frequency scale factor.
  @rtype: float
  @return: the generated turbulence value.
  """

def vTurbulence (xyz, octaves, hard, basis = NoiseTypes['STDPERLIN'],
    ampscale = 0.5, freqscale = 2.0):
  """
  Returns general turbulence vector using the optional specified noise basis
function.
  @type xyz: 3-float tuple
  @param xyz: (x,y,z) float values.
  @type octaves: int
  @param octaves: number of noise values added.
  @type hard: bool
  @param hard: noise hardness: 0 - soft noise; 1 - hard noise.  (Returned
      vector is always positive.)
  @type basis: int
  @param basis: type of noise used for turbulence, see L{NoiseTypes}.
  @type ampscale: float
  @param ampscale: amplitude scale value of the noise frequencies added.
  @type freqscale: float
  @param freqscale: frequency scale factor.
  @rtype: 3-float list
  @return: the generated turbulence vector.
  """

def fBm (xyz, H, lacunarity, octaves, basis = NoiseTypes['STDPERLIN']):
  """
  Returns Fractal Brownian Motion noise value (fBm).
  @type xyz: 3-float tuple
  @param xyz: (x,y,z) float values.
  @type H: float
  @param H: the fractal increment parameter.
  @type lacunarity: float
  @param lacunarity: the gap between successive frequencies.
  @type octaves: float
  @param octaves: the number of frequencies in the fBm.
  @type basis: int
  @param basis: type of noise used for the turbulence, see L{NoiseTypes}.
  @rtype: float
  @return: the generated noise value.
  """

def multiFractal (xyz, H, lacunarity, octaves, basis = NoiseTypes['STDPERLIN']):
  """
  Returns Multifractal noise value.
  @type xyz: 3-float tuple
  @param xyz: (x,y,z) float values.
  @type H: float
  @param H: the highest fractal dimension.
  @type lacunarity: float
  @param lacunarity: the gap between successive frequencies.
  @type octaves: float
  @param octaves: the number of frequencies in the fBm.
  @type basis: int
  @param basis: type of noise used for the turbulence, see L{NoiseTypes}.
  @rtype: float
  @return: the generated noise value.
  """

def vlNoise (xyz, distortion, type1 = NoiseTypes['STDPERLIN'],
    type2 = NoiseTypes['STDPERLIN']):
  """
  Returns Variable Lacunarity Noise value, a distorted variety of noise.
  @type xyz: 3-float tuple
  @param xyz: (x,y,z) float values.
  @type distortion: float
  @param distortion: the amount of distortion.
  @type type1: int
  @type type2: int
  @param type1: sets the noise type to distort.
  @param type2: sets the noise type used for the distortion.
  @rtype: float
  @return: the generated noise value.
  """

def heteroTerrain (xyz, H, lacunarity, octaves, offset,
    basis = NoiseTypes['STDPERLIN']):
  """
  Returns Heterogeneous Terrain value.
  @type xyz: 3-float tuple
  @param xyz: (x,y,z) float values.
  @type H: float
  @param H: fractal dimension of the roughest areas.
  @type lacunarity: float
  @param lacunarity: gap between successive frequencies.
  @type octaves: float
  @param octaves: number of frequencies in the fBm.
  @type offset: float
  @param offset: it raises the terrain from 'sea level'.
  @type basis: int
  @param basis: noise basis determines the type of noise used for the
      turbulence, see L{NoiseTypes}.
  @rtype: float
  @return: the generated value.
  """

def hybridMFractal (xyz, H, lacunarity, octaves, offset, gain,
    basis = NoiseTypes['STDPERLIN']):
  """
  Returns Hybrid Multifractal value.
  @type xyz: 3-float tuple
  @param xyz: (x,y,z) float values.
  @type H: float
  @param H: fractal dimension of the roughest areas.
  @type lacunarity: float
  @param lacunarity: gap between successive frequencies.
  @type octaves: float
  @param octaves: number of frequencies in the fBm.
  @type offset: float
  @param offset: it raises the terrain from 'sea level'.
  @type gain: float
  @param gain: scale factor.
  @type basis: int
  @param basis: noise basis determines the type of noise used for the
      turbulence, see L{NoiseTypes}.
  @rtype: float
  @return: the generated value.
  """

def ridgedMFractal (xyz, H, lacunarity, octaves, offset, gain,
    basis = NoiseTypes['STDPERLIN']):
  """
  Returns Ridged Multifractal value.
  @type xyz: 3-float tuple
  @param xyz: (x,y,z) float values.
  @type H: float
  @param H: fractal dimension of the roughest areas.
  @type lacunarity: float
  @param lacunarity: gap between successive frequencies.
  @type octaves: float
  @param octaves: number of frequencies in the fBm.
  @type offset: float
  @param offset: it raises the terrain from 'sea level'.
  @type gain: float
  @param gain: scale factor.
  @type basis: int
  @param basis: noise basis determines the type of noise used for the
      turbulence, see L{NoiseTypes}.
  @rtype: float
  @return: the generated value.
  """

def voronoi(xyz, distance_metric = DistanceMetrics['DISTANCE'], exponent = 2.5):
  """
  Returns Voronoi diagrams-related data.
  @type xyz: 3-float tuple
  @param xyz: (x,y,z) float values.
  @type distance_metric: int
  @param distance_metric: see L{DistanceMetrics}
  @type exponent: float
  @param exponent: only used with MINKOVSKY, default is 2.5.
  @rtype: list
  @return: a list containing a list of distances in order of closest feature,
  and a list containing the positions of the four closest features.
  """

def cellNoise (xyz):
  """
  Returns cellnoise.
  @type xyz: 3-float tuple
  @param xyz: (x,y,z) float values.
  @rtype: float
  @return: the generated value.
  """

def cellNoiseV (xyz):
  """
  Returns cellnoise vector/point/color.
  @type xyz: 3-float tuple
  @param xyz: (x,y,z) float values.
  @rtype: 3-float list
  @return: the generated vector.
  """
