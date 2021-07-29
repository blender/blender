Range Integer
=============

Functionality
-------------

*alias: List Range Int*

Useful for generating sequences of Integer values. The code perhaps describes best what the two modes do::

    def intRange(start=0, step=1, stop=1):
        '''
        "lazy range"
        - step is always |step| (absolute)
        - step is converted to negative if stop is less than start
        '''
        if start == stop:
            return []
        step = max(step, 1)
        if stop < start:
            step *= -1
        return list(range(start, stop, step))


    def countRange(start=0, step=1, count=10):
        count = max(count, 0)
        if count == 0:
            return []
        stop = (count*step) + start
        return list(range(start, stop, step))


Inputs and Parameters
---------------------

One UI parameter controls the behaviour of this Node; the ``Range | Count`` Mode switch. The last input changes accordingly.

+-------+-------+--------------------------------------------------------+
| Mode  | Input | Description                                            |
+=======+=======+========================================================+ 
|       |       |                                                        |
| Both  | Start | value to start at                                      |
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

This Node accepts only Integers and lists of Integers, so you must convert Floats to Int first. 
The reason is purely superficial - there is no reasonable argument not to automatically cast values.

Outputs
-------

Integers only, in list form.

Examples
--------

**Non-vectorized**

Int Range

::

    intRange(0,1,10)
    >>> [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

    intRange(0,2,10)
    >>> [0, 2, 4, 6, 8]

    intRange(-4,1,6)
    >>> [-4, -3, -2, -1, 0, 1, 2, 3, 4, 5]

    intRange(2,1,-4)
    >>> [2, 1, 0, -1, -2, -3]

Count Range

::

    countRange(0,1,5)
    >>> [0, 1, 2, 3, 4]

    countRange(0,2,5)
    >>> [0, 2, 4, 6, 8]

    countRange(-4,1,6)
    >>> [-4, -3, -2, -1, 0, 1]

    countRange(2,1,4)
    >>> [2, 3, 4, 5]

**Vectorized**

`Progress Thread <https://github.com/nortikin/sverchok/issues/156>`_ in the issue tracker shows several examples.

.. image:: https://cloud.githubusercontent.com/assets/619340/4163189/29d5fb56-34e4-11e4-9b00-baa15a8ddf00.png



