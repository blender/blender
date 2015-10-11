KX_WorldInfo(PyObjectPlus)
=============================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

.. class:: KX_WorldInfo(PyObjectPlus)

   A world object.

   .. code-block:: python

      # Set the mist color to red.
      import bge

      sce = bge.logic.getCurrentScene()

      sce.world.mistColor = [1.0, 0.0, 0.0]

   .. data:: KX_MIST_QUADRATIC

      Type of quadratic attenuation used to fade mist.

   .. data:: KX_MIST_LINEAR

      Type of linear attenuation used to fade mist.

   .. data:: KX_MIST_INV_QUADRATIC

      Type of inverse quadratic attenuation used to fade mist.

   .. attribute:: mistEnable

      Return the state of the mist.

      :type: bool

   .. attribute:: mistStart

      The mist start point.

      :type: float

   .. attribute:: mistDistance

      The mist distance fom the start point to reach 100% mist.

      :type: float

   .. attribute:: mistIntensity

      The mist intensity.

      :type: float

   .. attribute:: mistType

      The type of mist - must be KX_MIST_QUADRATIC, KX_MIST_LINEAR or KX_MIST_INV_QUADRATIC

   .. attribute:: mistColor

      The color of the mist. Black = [0.0, 0.0, 0.0], White = [1.0, 1.0, 1.0].
      Mist and background color sould always set to the same color.

      :type: :class:`mathutils.Color`

   .. attribute:: backgroundColor

      The color of the background. Black = [0.0, 0.0, 0.0], White = [1.0, 1.0, 1.0].
      Mist and background color sould always set to the same color.

      :type: :class:`mathutils.Color`

   .. attribute:: ambientColor

      The color of the ambient light. Black = [0.0, 0.0, 0.0], White = [1.0, 1.0, 1.0].

      :type: :class:`mathutils.Color`
