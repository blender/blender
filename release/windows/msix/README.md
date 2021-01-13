create_msix_package
===================

This tool is used to create MSIX packages from a given ZiP archive. The MSIX
package is distributed mainly through the Microsoft Store. It can also be
installed when downloaded from blender.org. For that to work the MSIX package
needs to be signed.

Requirements
============

* MakeAppX.exe - this tool is distributed with the Windows 10 SDK and is used to build the .appx package.
* MakePri.exe - this tool is distributed with the Windows 10 SDK and is used to generate a resources file.
* SignTool.exe - this tool is distributed with the Windows 10 SDK and is used to sign the .appx package.
* Python 3 (3.7 or later tested) - to run the create_msix_package.py script
* requests module - can be installed with `pip install requests`
* PFX file (optional, but strongly recommended) - for signing the resulting MSIX
  package. **NOTE:** If the MSIX package is not signed when uploaded to the Microsoft
  store the validation and certification process can take up to three full
  business day.

Usage
=====

On the command-line:
```batch
set VERSION=2.83.4.0
set URL=https://download.blender.org/release/Blender2.83/blender-2.83.4-windows64.zip
set PUBID=CN=PUBIDHERE
set PFX=X:\path\to\cert.pfx
set PFXPW=pwhere

python create_msix_package.py --version %VERSION% --url %URL% --publisher %PUBID% --pfx %PFX% --password %PFXPW%
```

Result will be a MSIX package with the name `blender-2.83.4-windows64.msix`.
With the above usage it will be signed. If the signing options are left out the
package will not be signed.

Optional arguments
==================

In support of testing and developing the manifest and scripts there are a few
optional arguments:

* `--skipdl` : If a `blender.zip` is available already next to the tool use this
  to skip actual downloading of the archive designated by `--url`. The latter
  option is still required
* `--overwrite` : When script fails the final clean-up may be incomplete leaving
  the `Content` folder with its structure. Specify this argument to automatically
  clean up this folder before starting to seed the `Content` folder
* `--leavezip` : When specified leave the `blender.zip` file while cleaning up
  all other intermediate files, including the `Content` folder. This is useful
  to not have to re-download the same archive from `--url` on each usage


What it does
============

The tool creates in the directory it lives a subfolder called `Content`. This is
where all necessary files are placed.

The `Assets` folder is copied to the `Content` folder.

From the application manifest template a version with necessary parts replaced as
their actual values as specified on the command-line is realized. This manifest controls the packaging of Blender into the MSIX format.

Next the tool downloads the designated ZIP archive locally as blender.zip. From
this archive the files are extracted into the `Content\Blender` folder, but skip
the leading part of paths in the ZIP. We want to write the files to the
content_blender_folder where blender.exe ends up as
`Content\Blender\blender.exe`, and not
`Content\Blender\blender-2.83.4-windows64\blender.exe`

Once the extraction is completed the MakeAppX tool is executed with the `Content`
folder as input. The result will be the MSIX package with the name in the form
`blender-X.YY.Z-windows64.msix`.

If the PFX file and its password are given on the command-line this MSIX package
will be signed.

All intermediate files and directories will be removed.
