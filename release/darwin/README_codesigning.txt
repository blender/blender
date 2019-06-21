Tutorial for codesigning Blender releases on OSX ( atm. i do manually when needed ):
Updated as by august 01.2014 - removed deprecated rules and not recommended deep codesigning

1. You need to obtain the certificates for blender foundation, they can bw pulled at Apple developer account for BF
2. Run the following commands from terminal:

codesign -s <IDENTITY> <path_to_Blender.app>

codesign -s <IDENTITY> <path_to_blenderplayer.app>


3. Checking:

codesign -vv <path_to_blenderplayer.app>
codesign -vv <path_to_Blender.app>

The result should be something like:

<build_path>/Blender.app: valid on disk
<build_path>/Blender.app: satisfies its Designated Requirement

<build_path>/blenderplayer.app: valid on disk
<build_path>/blenderplayer.app: satisfies its Designated Requirement

Jens Verwiebe

