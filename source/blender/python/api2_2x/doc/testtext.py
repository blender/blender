# Test Blender.Text

import Blender
from Blender import Text

txt = Text.New("MyText")
all_texts = Text.Get()

for i in [1,4,7]:
  txt.write("%d\n%d\n%d little indians\n" % (i, i+1, i+2))

x = txt.getNLines()

txt.write("%d little indian boys!" % x)

lines = txt.asLines()

txt.clear()
txt.write("... Yo-ho-ho! And a bottle of rum!")

for s in lines:
  print s

print all_texts
print txt.asLines()

Text.unlink(txt)

print all_texts
