Mask from Index
==============

*destination after Beta:*

Functionality
-------------

It can create mask list and set certain index positions to True. Length of mask list can be set using
either data list, or by setting int value.

Inputs
------

This node has the following inputs:

- **Index** - List of indices of values inside of mask list to set True.
- **Mask Length** - Length of mask list. Seen only when **Data Mask** mode not used.
- **Data to Mask** - Data from which mask lists will be formed. Seen only when **Data Mask** mode is used.


Outputs
-------

This node has the following outputs:

- **Mask** - Mask list with values of indices from Index list set to True, and others set to False.

Examples of usage
-----------------

.. image:: https://cloud.githubusercontent.com/assets/22656834/25303147/32c29ce8-2766-11e7-8671-7d96a0e0dcbf.png

.. image:: https://cloud.githubusercontent.com/assets/22656834/25303145/2c8349cc-2766-11e7-8d98-ffbba971ca49.png
