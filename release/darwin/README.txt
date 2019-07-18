
macOS app bundling guide
========================

Install Code Signing Certificate
--------------------------------

* Go to https://developer.apple.com/account/resources/certificates/list
* Download the Developer ID Application certifate.
* Double click the file and add to key chain (default options).
* Delete the file from the Downloads folder.

Find the codesigning identity by running:

$ security find-identity -v -p codesigning

"Developer ID Application: Stichting Blender Foundation" is the identity needed.
The long code at the start of the line is used as <identity> below.

Setup Apple ID
--------------

* The Apple ID must have two step verification enabled.
* Create an app specific password for the code signing app (label can be anything):
https://support.apple.com/en-us/HT204397
* Add the app specific password to keychain:

$ security add-generic-password -a <apple-id> -w <app-specific-password> -s altool-password

When running the bundle script, there will be a popup. To avoid that either:
* Click Always Allow in the popup
* In the Keychain Access app, change the Access Control settings on altool-password

Bundle
------

Then the bundle is created as follows:

$ ./bundle.sh --source <sourcedir> --dmg <dmg> --bundle-id <bundleid> --username <apple-id> --password "@keychain:altool-password" --codesign <identity>

<sourcedir>  directory where built Blender.app is
<dmg>	       location and name of the final disk image
<bundleid>   id on notarization, for example org.blenderfoundation.blender.release
<apple-id>   your appleid email
<identity>   codesigning identity

When specifying only --sourcedir and --dmg, the build will not be signed.

Example :
$ ./bundle.sh --source /data/build/bin --dmg /data/Blender-2.8-alpha-macOS-10.11.dmg --bundle-id org.blenderfoundation.blender.release --username "foo@mac.com" --password "@keychain:altool-password" --codesign AE825E26F12D08B692F360133210AF46F4CF7B97
