# PyPI Release Publishing

### Setup

Install Twine.

    pip3 install twine

Create ~/.pypirc with the following contents. Token is available in same place
as other credentials used for publishing releases.

    [distutils]
      index-servers =
        pypi
        bpy
    [pypi]
      username = __token__
      password = <SECRET_PYPI_TOKEN>
    [bpy]
      repository = https://upload.pypi.org/legacy/
      username = __token__
      password = <SECRET_PYPI_TOKEN>

### Release

Trigger release buildbot build with Python Module and Package Delivery enabled.
Check download page for Git hash.

Run checks:

    ./upload-release.py --version X.X.X --git-hash abcd1234 --check

Upload:

    ./upload-release.py --version X.X.X --git-hash abcd1234
