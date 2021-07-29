Blender ID addon
================

This addon allows you to authenticate your Blender with your
[Blender ID](https://www.blender.org/id/) account. This authentication
can then be used by other addons, such as the
[Blender Cloud addon](https://developer.blender.org/diffusion/BCA/)

Blender compatibility
---------------------

Blender ID add-on version 1.2.0 removed some workarounds necessary for
Blender 2.77a. As such, versions 1.1.x are the last versions compatible with
Blender 2.77a, and 1.2.0 and newer require at least Blender 2.78.

Building & Bundling
-------------------

* To build the addon, run `python3 setup.py bdist`
* To bundle the addon with Blender, run `python3 setup.py bdist bundle --path
  ../blender-git/blender/release/scripts/addons`.
* If you don't want to bundle, you can install the addon from Blender
  (User Preferences → Addons → Install from file...) by pointing it to
  `dist/blender_id*.addon.zip`.


Using the addon
---------------

* Install the addon as described above.
* Enable the addon in User Preferences → Addons → System.
* Sign up for an account at the
  [Blender ID site](https://www.blender.org/id/) if you don't have an
  account yet.
* Log in with your Blender ID and password. You only have to do this
  once.

Your password is never saved on your machine, just an access token. It
is stored next to your Blender configuration files, in

* Linux and similar: `$HOME/.config/blender/{version}/config/blender_id`
* MacOS: `$HOME/Library/Application Support/Blender/{version}/config/blender_id`
* Windows: `%APPDATA%\Blender Foundation\Blender\{version}\config\blender_id`

where `{version}` is the Blender version.


Using the addon from another addon
----------------------------------

The following functions can be used from other addons to use the Blender
ID functionality:

**blender_id.get_active_profile()** returns the `BlenderIdProfile` that
represents the currently logged in user, or `None` when the user isn't
logged in:

    lang=python
    class BlenderIdProfile:
        user_id = '41234'
        username = 'username@example.com'
        token = '41344124-auth-token-434134'


**blender_id.get_active_user_id()** returns the user ID of the logged
in user, or `''` when the user isn't logged in.

**blender_id.is_logged_in()** returns `True` if the user is logged
in, and `False` otherwise.


Here is an example of a simple addon that shows your username in its
preferences panel:

    lang=python,name=demo_blender_id_addon.py
    # Extend this with your info
    bl_info = {
        'name': 'Demo addon using Blender ID',
        'location': 'Add-on preferences',
        'category': 'System',
        'support': 'TESTING',
    }

    import bpy


    class DemoPreferences(bpy.types.AddonPreferences):
        bl_idname = __name__

        def draw(self, context):
            import blender_id

            profile = blender_id.get_active_profile()
            if profile:
                self.layout.label('You are logged in as %s' % profile.username)
            else:
                self.layout.label('You are not logged in on Blender ID')


    def register():
        bpy.utils.register_module(__name__)


    def unregister():
        bpy.utils.unregister_module(__name__)


    if __name__ == '__main__':
        register()
