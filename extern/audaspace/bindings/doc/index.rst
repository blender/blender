.. audaspace documentation master file, created by
   sphinx-quickstart on Tue Sep  9 01:48:48 2014.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Welcome to audaspace's documentation!
=====================================

.. automodule:: aud

This documentation is valid for both the Python and C bindings of audaspace. If you are looking for installation instructions check the `C++ API documentation <../index.html>`_. As C is not an object oriented language everything is accessible via functions where the first paramter is always the object. For methods these are named as ``AUD_ClassName_method()`` and properties are accessed via ``AUD_ClassName_property_get/set()``. Python users simply ``import aud`` to access the library.

.. toctree::
   :maxdepth: 2

   tutorials

Classes:

.. toctree::
   :maxdepth: 2

   device
   sound
   handle
   sequence
   sequence_entry

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

