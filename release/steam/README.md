Creating Steam builds for Blender
=================================

This script automates creation of the Steam files: download of the archives,
extraction of the archives, preparation of the build scripts (VDF files), actual
building of the Steam game files.

Requirements
============

* MacOS machine - Tested on Catalina 10.15.6. Extracting contents from the DMG
  archive did not work Windows nor on Linux using 7-zip. All DMG archives tested
  failed to be extracted. As such only MacOS is known to work.
* Steam SDK downloaded from SteamWorks - The `steamcmd` is used to generate the
  Steam game files. The path to the `steamcmd` is what is actually needed.
* SteamWorks credentials - Needed to log in using `steamcmd`.
* Login to SteamWorks with the `steamcmd` from the command-line at least once -
  Needded to ensure the user is properly logged in. On a new machine the user
  will have to go through two-factor authentication.
* App ID and Depot IDs - Needed to create the VDF files.
* Python 3.x - 3.7 was tested.
* Base URL - for downloading the archives.

Usage
=====

```bash
$ export STEAMUSER=SteamUserName
$ export STEAMPW=SteamUserPW
$ export BASEURL=https://download.blender.org/release/Blender2.83/
$ export VERSION=2.83.3
$ export APPID=appidnr
$ export WINID=winidnr
$ export LINID=linuxidnr
$ export MACOSID=macosidnr

# log in to SteamWorks from command-line at least once

$ ../sdk/tools/ContentBuilder/builder_osx/steamcmd +login $STEAMUSER $STEAMPW

# once that has been done we can now actually start our tool

$ python3.7 create_steam_builds.py --baseurl $BASEURL --version $VERSION --appid $APPID --winid $WINID --linuxid $LINID --macosid $MACOSID --steamuser $STEAMUSER --steampw $STEAMPW --steamcmd ../sdk/tools/ContentBuilder/builder_osx/steamcmd
```

All arguments in the above example are required.

At the start the tool will login using `steamcmd`. This is necessary to let the
Steam SDK update itself if necessary.

There are a few optional arguments:

* `--dryrun`: If set building the game files will not actually happen. A set of
  log files and a preview manifest per depot will be created in the output folder.
  This can be used to double-check everything works as expected.
* `--skipdl`: If set will skip downloading of the archives. The tool expects the
  archives to already exist in the correct content location.
* `--skipextract`: If set will skip extraction of all archives. The tool expects
  the archives to already have been correctly extracted in the content location.

Run the tool with `-h` for detailed information on each argument.

The content and output folders are generated through appending the version
without dots to the words `content` and `output` respectively, e.g. `content2833`
and `output2833`. These folders are created next to the tool.

From all `.template` files the Steam build scripts will be generated also in the
same directory as the tool. The files will have the extension `.vdf`.

In case of errors the tool will have a non-zero return code.