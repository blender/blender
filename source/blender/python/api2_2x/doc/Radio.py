# Blender.Scene.Radio module and the Radiosity PyType object

"""
The Blender.Scene.Radio submodule.

Radio
=====

This module gives access to B{Scene Radiosity Contexts} in Blender.

Example::
  import Blender
  from Blender import Scene

  # Only the current scene has a radiosity context.
  # Naturally, any scene can be made the current one
  # with scene.makeCurrent()

  scn = Scene.GetCurrent()

  # this is the only way to access the radiosity object:

  radio = scn.getRadiosityContext()

  radio.setDrawType('Gouraud')
  radio.setMode('ShowLimits', 'Z')

  radio.collectMeshes() # prepare patches
  radio.go() # calculate radiosity
  Blender.Redraw(-1)


@type Modes: readonly dictionary
@var Modes:
    - ShowLimits
    - Z

@type DrawTypes: readonly dictionary
@var DrawTypes:
    - Wire
    - Solid
    - Gouraud
"""

class Radio:
  """
  The Radiosity object
  ====================
    This object wraps the current Scene's radiosity context in Blender.
  """
 
  def go():
    """
    Start the radiosity simulation.  It is necessary to call L{collectMeshes}
    first.
    """

  def collectMeshes():
    """
    Convert B{selected} visible meshes to patches for radiosity calculation.
    @note: L{Object.Object.select} can be used to (un)select objects via
       bpython.
    """

  def freeData():
    """
    Release all memory used by radiosity.
    """

  def addMesh():
    """
    Add the new mesh created by the radiosity simulation (see L{go}) to
    Blender.  The radiosity results are stored in this mesh's vertex colors.
    @note: see L{replaceMeshes} for a destructive alternative.
    """

  def replaceMeshes():
    """
    Replace the original input meshes with the B{one} calculated by the
    radiosity simulation.  The radiosity results are stored in this mesh's
    vertex colors.
    @note: see L{addMesh} for a non-destructive alternative.
    """

  def limitSubdivide():
    """
    Subdivide patches (optional, it may improve results).
    """

  def filterFaces():
    """
    Force an extra smoothing.  This method can be called only after the
    simulation has been calculated (L{go}).
    """

  def filterElems():
    """
    Filter elements to remove aliasing artifacts.  This method can be called
    only after the simulation has been calculated (L{go}).
    """

  def subdividePatches():
    """
    Pre-subdivision: detect high-energy patches and subdivide them
    (optional, it may improve results).
    """

  def subdivideElems():
    """
    Pre-subdivision: detect high-energy elements (nodes) and subdivide them
    (optional, it may improve results).
    """

  def removeDoubles():
    """
    Join elements (nodes) which differ less than the defined element limit.
    This method can be called only after the simulation has been calculated
    (L{go}).
    """

  def getHemiRes():
    """
    Get hemicube size.
    @rtype: int
    @return: the current hemicube size.
    """

  def setHemiRes(ival):
    """
    Set hemicube size.  The range is [100, 1000].
    @type ival: int
    @param ival: the new size.
    """

  def getMaxIter():
    """
    Get maximum number of radiosity rounds.
    @rtype: int
    @return: the current maxiter value.
    """

  def setMaxIter(ival):
    """
    Set maximum number of radiosity rounds.  The range is [0, 10000].
    @type ival: int
    @param ival: the maxiter new value.
    """

  def getSubShPatch():
    """
    Get maximum number of times the environment is tested to detect patches.
    @rtype: int
    @return: the current value.
    """

  def setSubShPatch(ival):
    """
    Set the maximum number of times the environment is tested to detect
    patches.  The range is [0, 10].
    @type ival: int
    @param ival: the new value.
    """

  def getSubShElem():
    """
    Get the number of times the environment is tested to detect elements.
    @rtype: int
    @return: the current value.
    """

  def setSubShElem(ival):
    """
    Set number of times the environment is tested to detect elements.  The
    range is [0, 10].
    @type ival: int
    @param ival: the new value.
    """

  def getElemLimit():
    """
    Get the range for removing doubles.
    @rtype: int
    @return: the current value.
    """

  def setElemLimit(ival):
    """
    Set the range for removing doubles.  The range is [0, 50].
    @type ival: int
    @param ival: the new value.
    """

  def getMaxSubdivSh():
    """
    Get the maximum number of initial shoot patches evaluated.
    @rtype: int
    @return: the current value.
    """

  def setMaxSubdivSh(ival):
    """
    Set the maximum number of initial shoot patches evaluated.  The range is
    [1, 250].
    @type ival: int
    @param ival: the new value.
    """

  def getPatchMax():
    """
    Get the maximum size of a patch.
    @rtype: int
    @return: the current value.
    """

  def setPatchMax(ival):
    """
    Set the maximum size of a patch.  The range is [10, 1000].
    @type ival: int
    @param ival: the new value.
    """

  def getPatchMin():
    """
    Get the minimum size of a patch.
    @rtype: int
    @return: the current value.
    """

  def setPatchMin(ival):
    """
    Set the minimum size of a patch.  The range is [10, 1000].
    @type ival: int
    @param ival: the new value.
    """

  def getElemMax():
    """
    Get the maximum size of an element.
    @rtype: int
    @return: the current value.
    """

  def setElemMax(ival):
    """
    Set the maximum size of an element.  The range is [1, 100].
    @type ival: int
    @param ival: the new value.
    """

  def getElemMin():
    """
    Get the minimum size of an element.  The range is [1, 100].
    @rtype: int
    @return: the current value.
    """

  def setElemMin(ival):
    """
    Set the minimum size of an element.  The range is [1, 100].
    @type ival: int
    @param ival: the new value.
    """

  def getMaxElems():
    """
    Get the maximum number of elements.
    @rtype: int
    @return: the current value.
    """

  def setMaxElems(ival):
    """
    Set the maximum number of elements.  The range is [1, 250000].
    @type ival: int
    @param ival: the new value.
    """

  def getConvergence():
    """
    Get lower thresholdo of unshot energy.
    @rtype: float
    @return: the current value.
    """

  def setConvergence(fval):
    """
    Set lower threshold of unshot energy.  The range is [0.0, 1.0].
    @type fval: float
    @param fval: the new value.
    """

  def getMult():
    """
    Get the energy value multiplier.
    @rtype: float
    @return: the current value.
    """

  def setMult (fval):
    """
    Set the energy value multiplier.  The range is [0.001, 250.0].
    @type fval: float
    @param fval: the new value.
    """

  def getGamma():
    """
    Get change in the contrast of energy values.
    @rtype: float
    @return: the current value.
    """

  def setGamma (fval):
    """
    Set change in the contrast of energy values.  The range is [0.2, 10.0].
    @type fval: float
    @param fval: the new value.
    """

  def getDrawType():
    """
    Get the draw type: Wire, Solid or Gouraud as an int value, see L{DrawTypes}.
    @rtype: int
    @return: the current draw type.
    """

  def setDrawType (dt):
    """
    Set the draw type.
    @type dt: string or int
    @param dt: either 'Wire', 'Solid' or 'Gouraud' or the equivalent entry in
        the L{DrawTypes} dictionary.
    """

  def getMode():
    """
    Get mode as an int (or'ed bitflags), see L{Modes} dictionary.
    @rtype: int
    @return: the current value.
    """

  def setMode (mode1 = None, mode2 = None):
    """
    Set mode flags as strings: 'ShowLimits' and 'Z'.  To set one give it as
    only argument.  Strings not passed in are unset, so setMode() unsets
    both.
    @type mode1: string
    @param mode1: optional mode string.
    @type mode2: string
    @param mode2: optional mode string.
    """
