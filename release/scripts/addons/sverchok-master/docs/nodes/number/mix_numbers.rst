Mix Numbers
===========

Functionality
-------------

This node mixes two values using a given factor and a selected interpolation and easing functions.

For a factor of 0.0 it outputs the first value while the factor of 1.0 it outputs the last value. For every factor value between 0-1 it will output a value between the first and second input value. (*)

Note:
(*) The Back and Elastic interpolations will generate outputs that are not strictly confined to the first-second value interval, but they will output values that start at first and end at second value.

Inputs & Parameters
-------------------

All parameters except for **Type**, **Interpolation** and **Easing** can be given by the node or an external input.

This node has the following parameters:

+-------------------+---------------+-------------+-----------------------------------------------------+
| Parameter         | Type          | Default     | Description                                         |
+===================+===============+=============+=====================================================+
| **Type**          | Enum:         | Float       | Type of inputs values to interpolate.               |
|                   |  Int          |             | When Float is selected the input value1 and value2  |
|                   |  Float        |             | expect float values                                 |
|                   |               |             | When Int is selected the input value1 and value2    |
|                   |               |             | expect int values.                                  |
+-------------------+---------------+-------------+-----------------------------------------------------+
| **Interpolation** | Enum:         | Linear      | Type of interpolation.                              |
|                   |  Linear       |             |   f(x) ~ x                                          |
|                   |  Sinusoidal   |             |   f(x) ~ sin(x)                                     |
|                   |  Quadratic    |             |   f(x) ~ x*2                                        |
|                   |  Cubic        |             |   f(x) ~ x^3                                        |
|                   |  Quadric      |             |   f(x) ~ x^4                                        |
|                   |  Quintic      |             |   f(x) ~ x^5                                        |
|                   |  Exponential  |             |   f(x) ~ e^x                                        |
|                   |  Circular     |             |   f(x) ~ sqrt(1-x*x)                                |
|                   |  Back         |             |   f(x) ~ x*x*x - x*sin(x)                           |
|                   |  Bounce       |             |   f(x) ~ series of geometric progression parabolas  |
|                   |  Elastic      |             |   f(x) ~ sin(x) * e^x                               |
+-------------------+---------------+-------------+-----------------------------------------------------+
| **Easing**        | Enum          | Ease In-Out | Type of easing.                                     |
|                   |  Ease In      |             |  Ease In = slowly departs the starting value        |
|                   |  Ease Out     |             |  Ease Out = slowly approaches the ending value      |
|                   |  Ease In-Out  |             |  Ease In-Out = slowly departs and approaches values |
+-------------------+---------------+-------------+-----------------------------------------------------+
| **Value1**        | Int or Float  | 0 or 0.0    | Starting value                                      |
+-------------------+---------------+-------------+-----------------------------------------------------+
| **Value2**        | Int or Float  | 1 or 1.0    | Ending value                                        |
+-------------------+---------------+-------------+-----------------------------------------------------+
| **Factor**        | Float         | 0.5         | Mixing factor (between 0.0 and 1.0)                 |
+-------------------+---------------+-------------+-----------------------------------------------------+

Extra Parameters
----------------
For certain interpolation types the node provides extra parameters on the property panel.

* Exponential
  Extra parameters to adjust the base and the exponent of the exponential function. The Defaults are 2 and 10.0.

* Back
  Extra parameters to adjust the scale of the overshoot. The default is 1.0.

* Bounce
  Extra parameters to adjust the attenuation of the bounce and the number of bounces. The defaults are 0.5 and 4.

* Elastic
  Extra parameters to adjust the base and the exponent of the damping oscillation as well as the number of bounces (oscillations).
 The defaults are 1.6, 6.0 and 6.

Outputs
-------

This node has one output: **Value**.

Inputs and outputs are vectorized, so if series of values is passed to one of inputs, then this node will produce several sequences.

Example of usage
----------------

Given simplest nodes setup:

#

you will have something like:

#
