Tutorial for codesigning Blender releases on OSX ( atm. i do manually when needed ):

1. You need to obtain the certificates for blender foundation, they can bw pulled at Apple developer account for BF
2. Run the following commands from terminal:

codesign -s <IDENTITY> <path_to_blender.app> --resource-rules <path_to_blender_source>/release/darwin/codesigning_rules_blender.plist --deep

codesign -s <IDENTITY> <path_to_blenderplayer.app> --resource-rules <path_to_blender_source>/release/darwin/codesigning_rules_player.plist --deep


3. Checking:

codesign -vv <path_to_blenderplayer.app>
codesign -vv <path_to_blender.app>

The result should be something like:

<build_path>/blender.app: valid on disk
<build_path>/blender.app: satisfies its Designated Requirement

<build_path>/blenderplayer.app: valid on disk
<build_path>/blenderplayer.app: satisfies its Designated Requirement

Jens Verwiebe

