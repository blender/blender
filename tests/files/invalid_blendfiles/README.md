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

Corrupt Point Caches
====================

Files named `invalid_pointcache_*.blend` with a point cache baked for frames 1 to 3 to the
`blendcache_invalid_pointcache_*` directory. The frame 2 `.bphys` file is corrupt, and the
file is saved at frame 2 so opening it reads the corrupt frame.

`invalid_pointcache_particle_bad_index.blend`
---------------------------------------------

A particle cache with a negative particle index, passing the upper bound check (crash).

`invalid_pointcache_softbody_index_out_of_range.blend`
------------------------------------------------------

A soft body cache with an out-of-range point index (buffer overflow).

`invalid_pointcache_totpoint_overflow.blend`
--------------------------------------------

A point count overflowing the data allocation size (buffer overflow).

`invalid_pointcache_compressed_size_mismatch.blend`
---------------------------------------------------

Compressed data decompressing to fewer bytes than the point count requires (buffer overflow).

`invalid_pointcache_compressed_*.blend`
---------------------------------------

Compressed data with an unknown compression type, a size larger than the file, or no size
and data following the type (buffer overflow).

`invalid_pointcache_cloth_extra_zero_totdata.blend`
---------------------------------------------------

Cloth acceleration extra data without items, read as a float3 (buffer overflow).

`invalid_pointcache_extra_data_truncated.blend`
-----------------------------------------------

Extra data with more items than present in the file (buffer overflow).

`invalid_pointcache_dynamicpaint_truncated.blend`
-------------------------------------------------

A dynamic paint cache without version and surface type after the header (buffer overflow).

Corrupt File-Blocks
===================

Saves of a simple scene, or of a file linking a collection from a library, with file-blocks altered.

`invalid_thumbnail_*.blend`
---------------------------

The `TEST` thumbnail block is too short, has zero width or height, or is larger than the maximum size.
Extracting the thumbnail with `blender-thumbnailer` overflowed the image size (crash).

`invalid_preview_rect_struct_type.blend`
----------------------------------------

An object preview's pixel block has struct type `ListBase`.
Loading the preview from the file browser read it as pixels (buffer overflow).

`invalid_node_tree_failed_read.blend`
-------------------------------------

A node group is followed by an ID block with an out-of-range SDNA struct index.
Freeing the node group after the failed read accessed nodes without types (crash).

`invalid_glob_short.blend`
--------------------------

The `GLOB` block is truncated to 2 bytes, shorter than its sub-version string (buffer overflow).

`invalid_id_short.blend`
------------------------

An object block's data is 8 bytes, too short for the ID name and flag (buffer overflow).

`invalid_id_small.blend`
------------------------

An object block's data is truncated to `sizeof(ID)` (buffer overflow).

`invalid_struct_count_zero.blend`
---------------------------------

An object block has a zero struct count, with SDNA modified to require reconstruction (buffer overflow).

`invalid_glob_small.blend`
--------------------------

The `GLOB` block's struct type is `ListBase`, smaller than `FileGlobal` (buffer overflow).

`invalid_id_struct_type.blend`
------------------------------

An object block's struct type is `ID`, smaller than `Object` (buffer overflow).

`invalid_library_id_type.blend`
-------------------------------

A library block is renamed to a scene, which is larger than `Library` (buffer overflow).

`invalid_library_filepath_unterminated.blend`
---------------------------------------------

A library filepath without null terminator. Linking from the file read past the end (buffer overflow).

`invalid_library_missing.blend`
-------------------------------

The library block is removed, leaving link placeholders without library (crash).

`invalid_library_as_text.blend`
-------------------------------

The library block is renamed to a text, leaving link placeholders without library (crash).

`invalid_asset_metadata_missing.blend`
--------------------------------------

An asset's `AssetMetaData` block has an unresolvable address.
Listing assets and reading the ID dereferenced the missing metadata (crash).

`invalid_library_filepath_unterminated_listing.blend`
-----------------------------------------------------

A library filepath without null terminator until the block end, also overwriting the following
members. Listing the file read past the end (buffer overflow). Opening it read link placeholders
as full IDs, due to the library flag marking it as archive library (crash).

`invalid_preview_rect_short.blend`
----------------------------------

An object preview's pixel block is truncated to 4 bytes.
Loading the preview from the file browser read past the end (buffer overflow).
Opening it leaked the rejected pixel block.
