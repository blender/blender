KX_WordlInfo(PyObjectPlus)
=============================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

.. class:: KX_WorldInfo(PyObjectPlus)

   A wolrd object.

   .. code-block:: python

      # Set the mist color to red.
      import bge

      sce = bge.logic.getCurrentScene()

      sce.world.mist_color = [1.0, 0.0, 0.0]

*********
Constants
*********

   .. data:: KX_MIST_QUADRATIC

      Type of quadratic attenuation used to fade mist.

   .. data:: KX_MIST_LINEAR

      Type of linear attenuation used to fade mist.

   .. data:: KX_MIST_INV_QUADRATIC

      Type of inverse quadratic attenuation used to fade mist.

**********
Attributes
**********

   .. attribute:: mist_enable

      Return the state of the mist.

      :type: bool

   .. attribute:: mist_start

      The mist start point.

      :type: float

   .. attribute:: mist_distance

      The mist distance fom the start point to reach 100% mist.

      :type: float

   .. attribute:: mist_intensity

      The mist intensity.

      :type: float

   .. attribute:: mist_type

      The type of mist - must be KX_MIST_QUADRATIC, KX_MIST_LINEAR or KX_MIST_INV_QUADRATIC

   .. attribute:: mist_color

      The color of the mist. Black = [0.0, 0.0, 0.0], White = [1.0, 1.0, 1.0].
      Mist and background color sould always set to the same color.

      :type: :class:`mathutils.Vector`

   .. attribute:: background_color

      The color of the background. Black = [0.0, 0.0, 0.0], White = [1.0, 1.0, 1.0].
      Mist and background color sould always set to the same color.

      :type: :class:`mathutils.Vector`

   .. attribute:: ambient_color

      The color of the ambient light. Black = [0.0, 0.0, 0.0], White = [1.0, 1.0, 1.0].

      :type: :class:`mathutils.Vector`
