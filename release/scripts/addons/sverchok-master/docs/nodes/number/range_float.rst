Range Float
===========

Functionality
-------------

*alias: List Range Float*

Useful for generating sequences of Float values. The code perhaps describes best what the three modes do::

def frange(start, stop, step):
    '''Behaves like range but for floats'''
    if start == stop:
        stop += 1
    step = max(1e-5, abs(step))
    if start < stop:
        while start < stop:
            yield start
            start += step
    else:
        step = -abs(step)
        while start > stop:
            yield start
            start += step


def frange_count(start, stop, count):
    ''' Gives count total values in [start,stop] '''
    if count < 2:
        yield start
    else:
        count = int(count)
        step = (stop - start) / (count - 1)
        yield start
        for i in range(count - 2):
            start += step
            yield start
        yield stop


def frange_step(start, step, count):
    ''' Gives count values with step from start'''
    if abs(step) < 1e-5:
        step = 1
    for i in range(int(count)):
        yield start
        start += step



Inputs and Parameters
---------------------

One UI parameter controls the behaviour of this Node; the ``Range | Count`` Mode switch. The last input changes accordingly.

+-------+-------+--------------------------------------------------------+
| Mode  | Input | Description                                            |
+=======+=======+========================================================+
|       |       |                                                        |
| Step  | Start | value to start at                                      |
|       +-------+--------------------------------------------------------+
|       | Step  | value of the skip distance to the next value. The Step |
|       |       | value is considered the absolute difference between    |
|       |       | successive numbers.                                    |
+-------+-------+--------------------------------------------------------+
| Range | Stop  | last value to generate, don't make values beyond this. |
|       |       | If this value is lower than the start value then the   |
|       |       | sequence will be of descending values.                 |
+-------+-------+--------------------------------------------------------+
| Count | Count | number of values to produce given Start and Step.      |
|       |       | **Never negative** - negative produces an empty list   |
+-------+-------+--------------------------------------------------------+

**A word on implementation:**

This Node accepts Integers and Floats and lists of them.

Outputs
-------

floats or Integers, in list form.

Examples
--------

**Non-vectorized**

Float Range _(start, stop, step)_

    FloatRange(0.0, 1.1 ,10.0)
    >>> [0.0, 1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8, 9.9]

    FloatRange(0.0 ,2.2, 10.0)
    >>> [0.0, 2.2, 4.4, 6.6, 8.8]

    FloatRange(-3.97, 0.97, 6.0)
    >>> [-3.97, -3.0, -2.029, -1.059, -0.089, 0.88, 1.85, 2.82, 3.79, 4.76, 5.73]

    FloatRange(2.0, 1.0, -4.0)
    >>> [2.0, 1.0, 0.0, -1.0, -2.0, -3.0]

Count Range _(start, stop, count)_

    CountRange(0.0, 1.0, 5)
    >>> [0.0, 1.0, 2.0, 3.0, 4.0]

    CountRange(0.0, 2.5, 5)
    >>> [0.0, 0.625, 1.25, 1.875, 2.5]

    CountRange(-4.0, 1.2, 6)
    >>> [-4.0, -2.96, -1.91, -0.879, 0.16, 1.2]

    CountRange(2.0, 1.0, 4)
    >>> [2.0, 1.6, 1.3, 1.0]

Step Range _(start, step, count)_

    StepRange(0.0, 1.0, 10)
    >>> [0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0]

    StepRange(0.0, 2.4, 5)
    >>> [0.0, 2.4, 4.8, 7.2, 9.6]

    StepRange(-4.0, 1.2, 6)
    >>> [-4.0, -2.8, -1.6, -0.4, 0.8, 2.0]

    StepRange(2.0, 1.0, 4)
    >>> [2.0, 3.0, 4.0, 5.0]


**Vectorized**

`Progress Thread for the IntRange <https://github.com/nortikin/sverchok/issues/156>`_ in the issue tracker shows several examples.
