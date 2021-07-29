SCA_RandomActuator(SCA_IActuator)
=================================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: SCA_RandomActuator(SCA_IActuator)

   Random Actuator

   .. attribute:: seed

      Seed of the random number generator.

      :type: integer.

      Equal seeds produce equal series. If the seed is 0, the generator will produce the same value on every call.

   .. attribute:: para1

      the first parameter of the active distribution.

      :type: float, read-only.

      Refer to the documentation of the generator types for the meaning of this value. 

   .. attribute:: para2

      the second parameter of the active distribution.

      :type: float, read-only

      Refer to the documentation of the generator types for the meaning of this value.

   .. attribute:: distribution

      Distribution type. (read-only). Can be one of :ref:`these constants <logic-random-distributions>`

      :type: integer

   .. attribute:: propName

      the name of the property to set with the random value.

      :type: string

      If the generator and property types do not match, the assignment is ignored.

   .. method:: setBoolConst(value)

      Sets this generator to produce a constant boolean value.

      :arg value: The value to return.
      :type value: boolean

   .. method:: setBoolUniform()

      Sets this generator to produce a uniform boolean distribution.

      The generator will generate True or False with 50% chance.

   .. method:: setBoolBernouilli(value)

      Sets this generator to produce a Bernouilli distribution.

      :arg value: Specifies the proportion of False values to produce.

         * 0.0: Always generate True
         * 1.0: Always generate False
      :type value: float

   .. method:: setIntConst(value)

      Sets this generator to always produce the given value.

      :arg value: the value this generator produces.
      :type value: integer

   .. method:: setIntUniform(lower_bound, upper_bound)

      Sets this generator to produce a random value between the given lower and
      upper bounds (inclusive).

      :type lower_bound: integer
      :type upper_bound: integer

   .. method:: setIntPoisson(value)

      Generate a Poisson-distributed number.

      This performs a series of Bernouilli tests with parameter value.
      It returns the number of tries needed to achieve succes.

      :type value: float

   .. method:: setFloatConst(value)

      Always generate the given value.

      :type value: float

   .. method:: setFloatUniform(lower_bound, upper_bound)

      Generates a random float between lower_bound and upper_bound with a
      uniform distribution.

      :type lower_bound: float
      :type upper_bound: float

   .. method:: setFloatNormal(mean, standard_deviation)

      Generates a random float from the given normal distribution.

      :arg mean: The mean (average) value of the generated numbers
      :type mean: float
      :arg standard_deviation: The standard deviation of the generated numbers.
      :type standard_deviation: float

   .. method:: setFloatNegativeExponential(half_life)

      Generate negative-exponentially distributed numbers.

      The half-life 'time' is characterized by half_life.
      
      :type half_life: float

