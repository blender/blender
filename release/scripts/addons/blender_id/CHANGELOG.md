# Blender ID Add-on Changelog

## Version 1.4.1 (released 2017-12-15)

- Improved error reporting when validating a token fails due to
  connection errors.


## Version 1.4 (released 2017-12-05)

- Added an extra date/time format for parsing the authentication token expiry date.
- Always show the "Validate" button when the user is logged in. This actively checks the token with
  the server, whereas the "You are logged in" only bases that statement on locally-available
  information (there is a token and it hasn't expired yet).


## Version 1.3 (released 2017-06-14)

- Show a message after logging out.
- Store token expiry date in profile JSON.
- Show "validate" button when the token expiration is unknown.
- Urge the user to log out & back in again to refresh the auth token if it expires within 2 weeks.
- Added a method `validate_token()` to the public Blender ID Add-on API.


## Older versions

The history of older versions can be found in the
[Blender ID Add-on Git repository](https://developer.blender.org/diffusion/BIA/).
