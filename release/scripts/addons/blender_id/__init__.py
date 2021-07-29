# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

bl_info = {
    'name': 'Blender ID authentication',
    'author': 'Francesco Siddi, Inês Almeida and Sybren A. Stüvel',
    'version': (1, 4, 1),
    'blender': (2, 77, 0),
    'location': 'Add-on preferences',
    'description':
        'Stores your Blender ID credentials for usage with other add-ons',
    'wiki_url': 'http://wiki.blender.org/index.php/Extensions:2.6/Py/'
                'Scripts/System/BlenderID',
    'category': 'System',
    'support': 'OFFICIAL',
}

import datetime
import typing

import bpy
from bpy.types import AddonPreferences, Operator, PropertyGroup
from bpy.props import PointerProperty, StringProperty

if 'communication' in locals():
    import importlib

    # noinspection PyUnboundLocalVariable
    communication = importlib.reload(communication)
    # noinspection PyUnboundLocalVariable
    profiles = importlib.reload(profiles)
else:
    from . import communication, profiles
BlenderIdProfile = profiles.BlenderIdProfile
BlenderIdCommError = communication.BlenderIdCommError

__all__ = ('get_active_profile', 'get_active_user_id', 'is_logged_in', 'create_subclient_token',
           'BlenderIdProfile', 'BlenderIdCommError')


# Public API functions
def get_active_user_id() -> str:
    """Get the id of the currently active profile. If there is no
    active profile on the file, this function will return an empty string.
    """

    return BlenderIdProfile.user_id


def get_active_profile() -> BlenderIdProfile:
    """Returns the active Blender ID profile. If there is no
    active profile on the file, this function will return None.

    :rtype: BlenderIdProfile
    """

    if not BlenderIdProfile.user_id:
        return None

    return BlenderIdProfile


def is_logged_in() -> bool:
    """Returns whether the user is logged in on Blender ID or not."""

    return bool(BlenderIdProfile.user_id)


def create_subclient_token(subclient_id: str, webservice_endpoint: str) -> dict:
    """Lets the Blender ID server create a subclient token.

    :param subclient_id: the ID of the subclient
    :param webservice_endpoint: the URL of the endpoint of the webservice
        that belongs to this subclient.
    :returns: the token along with its expiry timestamp, in a {'scst': 'token',
        'expiry': datetime.datetime} dict.
    :raises: blender_id.communication.BlenderIdCommError when the
        token cannot be created.
    """

    # Communication between us and Blender ID.
    profile = get_active_profile()
    scst_info = communication.subclient_create_token(profile.token, subclient_id)
    subclient_token = scst_info['token']

    # Send the token to the webservice.
    user_id = communication.send_token_to_subclient(webservice_endpoint, profile.user_id,
                                                    subclient_token, subclient_id)

    # Now that everything is okay we can store the token locally.
    profile.subclients[subclient_id] = {'subclient_user_id': user_id, 'token': subclient_token}
    profile.save_json()

    return scst_info


def get_subclient_user_id(subclient_id: str) -> str:
    """Returns the user ID at the given subclient.

    Requires that the user has been authenticated at the subclient using
    a call to create_subclient_token(...)

    :returns: the subclient-local user ID, or None if not logged in.
    """

    if not BlenderIdProfile.user_id:
        return None

    return BlenderIdProfile.subclients[subclient_id]['subclient_user_id']


def validate_token() -> typing.Optional[str]:
    """Validates the current user's token with Blender ID.

    Also refreshes the stored token expiry time.

    :returns: None if everything was ok, otherwise returns an error message.
    """

    expires, err = communication.blender_id_server_validate(token=BlenderIdProfile.token)
    if err is not None:
        return err

    BlenderIdProfile.expires = expires
    BlenderIdProfile.save_json()

    return None


def token_expires() -> typing.Optional[datetime.datetime]:
    """Returns the token expiry timestamp.

    Returns None if the token expiry is unknown. This can happen when
    the last login/validation was performed using a version of this
    add-on that was older than 1.3.
    """

    exp = BlenderIdProfile.expires
    if not exp:
        return None

    # Try parsing as different formats. A new Blender ID is coming,
    # which may change the format in which timestamps are sent.
    formats = [
        '%Y-%m-%dT%H:%M:%SZ', # ISO 8601 with Z-suffix
        '%Y-%m-%dT%H:%M:%S.%fZ', # ISO 8601 with fractional seconds and Z-suffix
        '%a, %d %b %Y %H:%M:%S GMT', # RFC 1123, used by old Blender ID
    ]
    for fmt in formats:
        try:
            return datetime.datetime.strptime(exp, fmt)
        except ValueError:
            # Just use the next format string and try again.
            pass

    # Unable to parse, may as well not be there then.
    return None


class BlenderIdPreferences(AddonPreferences):
    bl_idname = __name__

    error_message = StringProperty(
        name='Error Message',
        default='',
        options={'HIDDEN', 'SKIP_SAVE'}
    )
    ok_message = StringProperty(
        name='Message',
        default='',
        options={'HIDDEN', 'SKIP_SAVE'}
    )
    blender_id_username = StringProperty(
        name='E-mail address',
        default='',
        options={'HIDDEN', 'SKIP_SAVE'}
    )
    blender_id_password = StringProperty(
        name='Password',
        default='',
        options={'HIDDEN', 'SKIP_SAVE'},
        subtype='PASSWORD'
    )

    def reset_messages(self):
        self.ok_message = ''
        self.error_message = ''

    def draw(self, context):
        layout = self.layout

        if self.error_message:
            sub = layout.row()
            sub.alert = True  # labels don't display in red :(
            sub.label(self.error_message, icon='ERROR')
        if self.ok_message:
            sub = layout.row()
            sub.label(self.ok_message, icon='FILE_TICK')

        active_profile = get_active_profile()
        if active_profile:
            expiry = token_expires()
            now = datetime.datetime.utcnow()

            if expiry is None:
                layout.label(text='We do not know when your token expires, please validate it.')
            elif now >= expiry:
                layout.label(text='Your login has expired! Log out and log in again to refresh it.',
                             icon='ERROR')
            else:
                time_left = expiry - now
                if time_left.days > 14:
                    exp_str = 'on {:%Y-%m-%d}'.format(expiry)
                elif time_left.days > 1:
                    exp_str = 'in %i days.' % time_left.days
                elif time_left.seconds >= 7200:
                    exp_str = 'in %i hours.' % round(time_left.seconds / 3600)
                elif time_left.seconds >= 120:
                    exp_str = 'in %i minutes.' % round(time_left.seconds / 60)
                else:
                    exp_str = 'within seconds'

                if time_left.days < 14:
                    layout.label('You are logged in as %s.' % active_profile.username,
                                 icon='WORLD_DATA')
                    layout.label(text='Your token will expire %s. Please log out and log in again '
                                 'to refresh it.' % exp_str, icon='PREVIEW_RANGE')
                else:
                    layout.label('You are logged in as %s. Your authentication token expires %s.'
                                 % (active_profile.username, exp_str), icon='WORLD_DATA')

            row = layout.row().split(0.8)
            row.operator('blender_id.logout')
            row.operator('blender_id.validate')
        else:
            layout.prop(self, 'blender_id_username')
            layout.prop(self, 'blender_id_password')

            layout.operator('blender_id.login')


class BlenderIdMixin:
    @staticmethod
    def addon_prefs(context):
        preferences = context.user_preferences.addons[__name__].preferences
        preferences.reset_messages()
        return preferences


class BlenderIdLogin(BlenderIdMixin, Operator):
    bl_idname = 'blender_id.login'
    bl_label = 'Login'

    def execute(self, context):
        import random
        import string

        addon_prefs = self.addon_prefs(context)

        auth_result = communication.blender_id_server_authenticate(
            username=addon_prefs.blender_id_username,
            password=addon_prefs.blender_id_password
        )

        if auth_result.success:
            # Prevent saving the password in user preferences. Overwrite the password with a
            # random string, as just setting to '' might only replace the first byte with 0.
            pwlen = len(addon_prefs.blender_id_password)
            rnd = ''.join(random.choice(string.ascii_uppercase + string.digits)
                          for _ in range(pwlen + 16))
            addon_prefs.blender_id_password = rnd
            addon_prefs.blender_id_password = ''

            profiles.save_as_active_profile(
                auth_result,
                addon_prefs.blender_id_username,
                {}
            )
            addon_prefs.ok_message = 'Logged in'
        else:
            addon_prefs.error_message = auth_result.error_message
            if BlenderIdProfile.user_id:
                profiles.logout(BlenderIdProfile.user_id)

        BlenderIdProfile.read_json()

        return {'FINISHED'}


class BlenderIdValidate(BlenderIdMixin, Operator):
    bl_idname = 'blender_id.validate'
    bl_label = 'Validate'

    def execute(self, context):
        addon_prefs = self.addon_prefs(context)

        err = validate_token()
        if err is None:
            addon_prefs.ok_message = 'Authentication token is valid.'
        else:
            addon_prefs.error_message = '%s; you probably want to log out and log in again.' % err

        BlenderIdProfile.read_json()

        return {'FINISHED'}


class BlenderIdLogout(BlenderIdMixin, Operator):
    bl_idname = 'blender_id.logout'
    bl_label = 'Logout'

    def execute(self, context):
        addon_prefs = self.addon_prefs(context)

        communication.blender_id_server_logout(BlenderIdProfile.user_id,
                                               BlenderIdProfile.token)

        profiles.logout(BlenderIdProfile.user_id)
        BlenderIdProfile.read_json()

        addon_prefs.ok_message = 'You have been logged out.'
        return {'FINISHED'}


def register():
    profiles.register()
    BlenderIdProfile.read_json()

    bpy.utils.register_module(__name__)

    preferences = bpy.context.user_preferences.addons[__name__].preferences
    preferences.reset_messages()


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == '__main__':
    register()
