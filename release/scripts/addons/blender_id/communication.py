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

import functools
import logging
import typing

log = logging.getLogger(__name__)


class BlenderIdCommError(RuntimeError):
    """Raised when there was an error communicating with Blender ID"""


class AuthResult:
    def __init__(self, *, success: bool,
                 user_id: str=None, token: str=None, expires: str=None,
                 error_message: typing.Any=None):  # when success=False
        self.success = success
        self.user_id = user_id
        self.token = token
        self.error_message = str(error_message)
        self.expires = expires


@functools.lru_cache(maxsize=None)
def host_label():
    import socket

    return 'Blender running on %r' % socket.gethostname()


@functools.lru_cache(maxsize=None)
def blender_id_endpoint(endpoint_path=None):
    """Gets the endpoint for the authentication API. If the BLENDER_ID_ENDPOINT env variable
    is defined, it's possible to override the (default) production address.
    """
    import os
    import urllib.parse

    base_url = os.environ.get('BLENDER_ID_ENDPOINT', 'https://www.blender.org/id/')

    # urljoin() is None-safe for the 2nd parameter.
    return urllib.parse.urljoin(base_url, endpoint_path)


def blender_id_server_authenticate(username, password) -> AuthResult:
    """Authenticate the user with the server with a single transaction
    containing username and password (must happen via HTTPS).

    If the transaction is successful, status will be 'successful' and we
    return the user's unique blender id and a token (that will be used to
    represent that username and password combination).
    If there was a problem, status will be 'fail' and we return an error
    message. Problems may be with the connection or wrong user/password.
    """

    import requests
    import requests.exceptions

    payload = dict(
        username=username,
        password=password,
        host_label=host_label()
    )

    url = blender_id_endpoint('u/identify')
    try:
        r = requests.post(url, data=payload, verify=True)
    except (requests.exceptions.SSLError,
            requests.exceptions.HTTPError,
            requests.exceptions.ConnectionError) as e:
        msg = 'Exception POSTing to {}: {}'.format(url, e)
        print(msg)
        return AuthResult(success=False, error_message=msg)

    if r.status_code == 200:
        resp = r.json()
        status = resp['status']
        if status == 'success':
            return AuthResult(success=True,
                user_id=str(resp['data']['user_id']),
                token=resp['data']['oauth_token']['access_token'],
                expires=resp['data']['oauth_token']['expires'],
            )
        if status == 'fail':
            return AuthResult(success=False, error_message='Username and/or password is incorrect')

    return AuthResult(success=False,
                      error_message='There was a problem communicating with'
                                    ' the server. Error code is: %s' % r.status_code)


def blender_id_server_validate(token) -> typing.Tuple[typing.Optional[str], typing.Optional[str]]:
    """Validate the auth token with the server.

    @param token: the authentication token
    @type token: str
    @returns: tuple (expiry, error).
        The expiry is the expiry date of the token if it is valid, else None.
        The error is None if the token is valid, or an error message when it's invalid.
    """

    import requests
    import requests.exceptions

    url = blender_id_endpoint('u/validate_token')
    try:
        r = requests.post(url, data={'token': token}, verify=True)
    except requests.exceptions.ConnectionError:
        log.exception('error connecting to Blender ID at %s', url)
        return None, 'Unable to connect to Blender ID'
    except requests.exceptions.RequestException as e:
        log.exception('error validating token at %s', url)
        return None, str(e)

    if r.status_code != 200:
        return None, 'Authentication token invalid'

    response = r.json()
    return response['token_expires'], None


def blender_id_server_logout(user_id, token):
    """Logs out of the Blender ID service by removing the token server-side.

    @param user_id: the email address of the user.
    @type user_id: str
    @param token: the token to remove
    @type token: str
    @return: {'status': 'fail' or 'success', 'error_message': str}
    @rtype: dict
    """

    import requests
    import requests.exceptions

    payload = dict(
        user_id=user_id,
        token=token
    )
    try:
        r = requests.post(blender_id_endpoint('u/delete_token'),
                          data=payload, verify=True)
    except (requests.exceptions.SSLError,
            requests.exceptions.HTTPError,
            requests.exceptions.ConnectionError) as e:
        return dict(
            status='fail',
            error_message=format('There was a problem setting up a connection to '
                                 'the server. Error type is: %s' % type(e).__name__)
        )

    if r.status_code != 200:
        return dict(
            status='fail',
            error_message=format('There was a problem communicating with'
                                 ' the server. Error code is: %s' % r.status_code)
        )

    resp = r.json()
    return dict(
        status=resp['status'],
        error_message=None
    )


def subclient_create_token(auth_token: str, subclient_id: str) -> dict:
    """Creates a subclient-specific authentication token.

    :returns: the token along with its expiry timestamp, in a {'scst': 'token',
        'expiry': datetime.datetime} dict.
    """

    payload = {'subclient_id': subclient_id,
               'host_label': host_label()}

    r = make_authenticated_call('POST', 'subclients/create_token', auth_token, payload)
    if r.status_code == 401:
        raise BlenderIdCommError('Your Blender ID login is not valid, try logging in again.')

    if r.status_code != 201:
        raise BlenderIdCommError('Invalid response, HTTP code %i received' % r.status_code)

    resp = r.json()
    if resp['status'] != 'success':
        raise BlenderIdCommError(resp['message'])

    return resp['data']


def make_authenticated_call(method, url, auth_token, data):
    """Makes a HTTP call authenticated with the OAuth token."""

    import requests
    import requests.exceptions

    try:
        r = requests.request(method,
                             blender_id_endpoint(url),
                             data=data,
                             headers={'Authorization': 'Bearer %s' % auth_token},
                             verify=True)
    except (requests.exceptions.HTTPError,
            requests.exceptions.ConnectionError) as e:
        raise BlenderIdCommError(str(e))

    return r


def send_token_to_subclient(webservice_endpoint: str, user_id: str,
                            subclient_token: str, subclient_id: str) -> str:
    """Sends the subclient-specific token to the subclient.

    The subclient verifies this token with BlenderID. If it's accepted, the
    subclient ensures there is a valid user created server-side. The ID of
    that user is returned.

    :returns: the user ID at the subclient.
    """

    import requests
    import urllib.parse

    url = urllib.parse.urljoin(webservice_endpoint, 'blender_id/store_scst')
    try:
        r = requests.post(url,
                          data={'user_id': user_id,
                                'subclient_id': subclient_id,
                                'token': subclient_token},
                          verify=True)
        r.raise_for_status()
    except (requests.exceptions.HTTPError,
            requests.exceptions.ConnectionError) as e:
        raise BlenderIdCommError(str(e))
    resp = r.json()

    if resp['status'] != 'success':
        raise BlenderIdCommError('Error sending subclient-specific token to %s, error is: %s'
                                 % (webservice_endpoint, resp))

    return resp['subclient_user_id']
