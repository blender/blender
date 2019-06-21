tell application "Finder"
          tell disk "Blender"
               open
               set current view of container window to icon view
               set toolbar visible of container window to false
               set statusbar visible of container window to false
               set the bounds of container window to {100, 100, 640, 472}
               set theViewOptions to icon view options of container window
               set arrangement of theViewOptions to not arranged
               set icon size of theViewOptions to 128
               set background picture of theViewOptions to file ".background:background.tif"
               set position of item " " of container window to {400, 190}
               set position of item "blender.app" of container window to {135, 190}
               update without registering applications
               delay 5
               close
     end tell
end tell
