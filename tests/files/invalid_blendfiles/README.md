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

Corrupt SDNA
============

Factory-startup saves with a few bytes altered to corrupt the file header (`DNA1`) or a file-block header.
Each crashed the reader before its fix; the expected rejection is checked in `tests/python/bl_blendfile_versioning.py`.

`invalid_sdna_struct_size.blend`
--------------------------------

An embedded struct's `TLEN` is increased, so its size no longer matches the sum of its members.
Reconstruction ran past the block end (heap-buffer-overflow).

`invalid_block_count.blend`
---------------------------

A file-block's element count (`nr`) is `0x7fffffff`. Reconstruction ran past the block end (heap-buffer-overflow).

`invalid_block_struct_index.blend`
----------------------------------

A file-block's SDNA struct index (`SDNAnr`) is out of range (`0x40000000`), reading past the SDNA struct array (crash).

`invalid_global_block.blend`
----------------------------

Like `invalid_block_struct_index.blend`, on the required global (`GLOB`) block. 
The failed read returned null and was dereferenced (crash).

`invalid_window_workspace_hook.blend`
-------------------------------------

As above, on a window's `WorkSpaceInstanceHook` sub-block.
The partially-read window was dereferenced while freeing the invalidated file (crash).
