Exponential Sequence
====================

*destination after Beta: Number*

Functionality
-------------

This node produces specified number of items from exponential sequence, defined by formula x_n = x0*exp(alpha*n)
or x_n = x0*base^n. Obviously, these formulas are equivalent when alpha = log(base).

Sequence can be re-scaled so that maximum of absolute values of produced items will be equal to specified value.

**Note**. Please do not forget well-known properties of exponential sequences:

- They grow very quickly when base is greater than 1.0 (or alpha greater than 0.0).
- They decrease very quickly when base is less than 1.0 (or alpha less than 0.0).

Inputs & Parameters
-------------------

All parameters except for **Mode** can be given by the node or an external input.
This node has the following parameters:

+----------------+---------------+-------------+----------------------------------------------------+
| Parameter      | Type          | Default     | Description                                        |  
+================+===============+=============+====================================================+
| **Mode**       | Enum: Log or  | Log         | If Log, then x_n = x0*exp(alpha*n).                |
|                | Base          |             | If Base, then x_n = x0*base^n.                     |
+----------------+---------------+-------------+----------------------------------------------------+
| **X0**         | Float         | 1.0         | Item of sequence for N=0.                          |
+----------------+---------------+-------------+----------------------------------------------------+
| **Alpha**      | Float         | 0.1         | Coefficient in formula exp(alpha*n). Used only in  |
|                |               |             | Log mode.                                          |
+----------------+---------------+-------------+----------------------------------------------------+
| **Base**       | Float         | 2.0         | Exponential base in formula base^n. Used only in   |
|                |               |             | Base mode.                                         |
+----------------+---------------+-------------+----------------------------------------------------+
| **N from**     | Int           | 0           | Minimal value of N.                                |
+----------------+---------------+-------------+----------------------------------------------------+
| **N to**       | Int           | 10          | Maximal value of N.                                |
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

.. image:: https://cloud.githubusercontent.com/assets/284644/5692369/d73d4846-9914-11e4-9bf3-133427d8d069.png

you will have something like:

.. image:: https://cloud.githubusercontent.com/assets/284644/5692368/d6e3f8fe-9914-11e4-81cf-d6dffda3b359.png

