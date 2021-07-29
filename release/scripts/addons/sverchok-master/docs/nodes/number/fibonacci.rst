Fibonacci Sequence
==================

*destination after Beta: Number*

Functionality
-------------

This node produces specified number of items from Fibonacci sequence::

  1, 1, 2, 3, 5, 8, 13, 21 ...

Each next item is sum of two previous.

This node allows you to specify first two items for your sequence. Note that these numbers can be even negative.

Sequence can be re-scaled so that maximum of absolute values of produced items will be equal to specified value.

Inputs & Parameters
-------------------

All parameters can be given by the node or an external input.
This node has the following parameters:

+----------------+---------------+-------------+----------------------------------------------------+
| Parameter      | Type          | Default     | Description                                        |  
+================+===============+=============+====================================================+
| **X1**         | Float         | 1.0         | First item of sequence.                            |
+----------------+---------------+-------------+----------------------------------------------------+
| **X2**         | Float         | 1.0         | Second item of sequence.                           |
+----------------+---------------+-------------+----------------------------------------------------+
| **Count**      | Int           | 10          | Number of items to produce. Minimal value is 3.    |
+----------------+---------------+-------------+----------------------------------------------------+
| **Max**        | Float         | 0.0         | If non-zero, then all output sequence will be      |
|                |               |             | re-scaled so that maximum of absolute values will  |
|                |               |             | be equal to number specified.                      |
+----------------+---------------+-------------+----------------------------------------------------+

Outputs
-------

This node has one output: **Sequence**.

Inputs and outputs are vectorized, so if series of values is passed to one of
inputs, then this node will produce several sequences.

Example of usage
----------------

Given simplest nodes setup:

.. image:: https://cloud.githubusercontent.com/assets/284644/5691665/22a8bc0e-98f5-11e4-9ecc-924addf22178.png

you will have something like:

.. image:: https://cloud.githubusercontent.com/assets/284644/5691664/227c9d0e-98f5-11e4-87c9-fb6f552e1b89.png

