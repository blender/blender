"""
Intro
-----

.. warning::
   Most of this object should only be useful if you actually manipulate i18n stuff from Python.
   If you are a regular addon, you should only bother about :const:`contexts` member,
   and the :func:`register`/:func:`unregister` functions! The :func:`pgettext` family of functions
   should only be used in rare, specific cases (like e.g. complex "composited" UI strings...).

| To add translations to your python script, you must define a dictionary formatted like that:
|    ``{locale: {msg_key: msg_translation, ...}, ...}``
| where:

* locale is either a lang iso code (e.g. ``fr``), a lang+country code (e.g. ``pt_BR``),
  a lang+variant code (e.g. ``sr@latin``), or a full code (e.g. ``uz_UZ@cyrilic``).
* msg_key is a tuple (context, org message) - use, as much as possible, the predefined :const:`contexts`.
* msg_translation is the translated message in given language!

Then, call ``bpy.app.translations.register(__name__, your_dict)`` in your ``register()`` function, and \n"
``bpy.app.translations.unregister(__name__)`` in your ``unregister()`` one.

The ``Manage UI translations`` addon has several functions to help you collect strings to translate, and
generate the needed python code (the translation dictionary), as well as optional intermediary po files
if you want some... See
`How to Translate Blender <http://wiki.blender.org/index.php/Dev:Doc/Process/Translate_Blender>`_ and
`Using i18n in Blender Code <http://wiki.blender.org/index.php/Dev:Source/Interface/Internationalization>`_
for more info.

Module References
-----------------

"""
