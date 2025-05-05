******************
Invalid Blendfiles
******************

This folder contains blendfiles that are known to be invalid (either from unknown reasons, or on purpose).

This file gathers as much information as possible about each of them.

`invalid_ca_la_id_code.blend`
=============================

This file was generated from a default startup scene, by altering the written IDcode of `Camera` and `Lamp` data-blocks, with the following patch:

```
--- a/source/blender/blenloader/intern/writefile.cc
+++ b/source/blender/blenloader/intern/writefile.cc
@@ -1894,7 +1894,14 @@ void BLO_write_struct_list_by_name(BlendWriter *writer, const char *struct_name,
 
 void blo_write_id_struct(BlendWriter *writer, int struct_id, const void *id_address, const ID *id)
 {
-  writestruct_at_address_nr(writer->wd, GS(id->name), struct_id, 1, id_address, id);
+  int filecode = GS(id->name);
+  if (filecode == ID_CA) {
+    filecode = MAKE_ID2('!', '?');
+  }
+  else if (filecode == ID_LA) {
+    filecode = BLEND_MAKE_ID('!', '?', 'L', 'A');
+  }
+  writestruct_at_address_nr(writer->wd, filecode, struct_id, 1, id_address, id);
 }
 
 int BLO_get_struct_id_by_name(const BlendWriter *writer, const char *struct_name)
```
