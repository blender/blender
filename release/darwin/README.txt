Bundling guide:

Have your signing identity ready, you can check it by running:

$ secruity find-identity -v -p codesign

Check that your appleID has two step verification and app specified password generated. https://support.apple.com/en-us/HT204397
Add it to the login keychain so it won't be in cleartext.

$ security add-generic-password -a "AC_USERNAME" -w <secret> -s "AC_PASSWORD"

You need then to make sure altool can access your keychain. First time run, there is popup, always allow. Or you can also add it on Keychain Access.

Then you can make neat bundle using ./bundle.sh by

$ ./bundle.sh --source <sourcedir> --dmg <dmg> --bundle-id <bundleid> --username <username> --password <password> --codesign <identity>

where:

<sourcedir> directory where built blender.app is
<dmg>	    location and name of the final disk image
<bundleid>  id on notarization, you choose (for example org.blender.release)
<username>  your appleid
<password>  your password. having it in keychain, use "@keychain:AC_PASSWORD"
<identity>  codesigning identity

Only --sourcedir and --dmg are required flags.

Example :
$ ./bundle.sh --source /data/build --dmg /data/Blender-2.8-alpha-macOS-10.11.dmg --bundle-id org.blender.alpha --username "foo@mac.com" --password "@keychain:AC_PASSWORD" --codesign AE825E26F12D08B692F360133210AF46F4CF7B97




