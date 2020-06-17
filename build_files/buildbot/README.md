Blender Buildbot
================

Code signing
------------

Code signing is done as part of INSTALL target, which makes it possible to sign
files which are aimed into a bundle and coming from a non-signed source (such as
libraries SVN).

This is achieved by specifying `worker_codesign.cmake` as a post-install script
run by CMake. This CMake script simply involves an utility script written in
Python which takes care of an actual signing.

### Configuration

Client configuration doesn't need anything special, other than variable
`SHARED_STORAGE_DIR` pointing to a location which is watched by a server.
This is done in `config_builder.py` file and is stored in Git (which makes it
possible to have almost zero-configuration buildbot machines).

Server configuration requires copying `config_server_template.py` under the
name of `config_server.py` and tweaking values, which are platform-specific.

#### Windows configuration

There are two things which are needed on Windows in order to have code signing
to work:

- `TIMESTAMP_AUTHORITY_URL` which is most likely set http://timestamp.digicert.com
- `CERTIFICATE_FILEPATH` which is a full file path to a PKCS #12 key (.pfx).

## Tips

### Self-signed certificate on Windows

It is easiest to test configuration using self-signed certificate.

The certificate manipulation utilities are coming with Windows SDK.
Unfortunately, they are not added to PATH. Here is an example of how to make
sure they are easily available:

```
set PATH=C:\Program Files (x86)\Windows Kits\10\App Certification Kit;%PATH%
set PATH=C:\Program Files (x86)\Windows Kits\10\bin\10.0.18362.0\x64;%PATH%
```

Generate CA:

```
makecert -r -pe -n "CN=Blender Test CA" -ss CA -sr CurrentUser -a sha256 ^
         -cy authority -sky signature -sv BlenderTestCA.pvk BlenderTestCA.cer
```

Import the generated CA:

```
certutil -user -addstore Root BlenderTestCA.cer
```

Create self-signed certificate and pack it into PKCS #12:

```
makecert -pe -n "CN=Blender Test SPC" -a sha256 -cy end ^
         -sky signature ^
         -ic BlenderTestCA.cer -iv BlenderTestCA.pvk ^
         -sv BlenderTestSPC.pvk BlenderTestSPC.cer

pvk2pfx -pvk BlenderTestSPC.pvk -spc BlenderTestSPC.cer -pfx BlenderTestSPC.pfx
```