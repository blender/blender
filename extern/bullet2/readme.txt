***
Apply bullet_compound_raycast.patch if not already applied in Bullet source
This patch is needed to return correct raycast results on compound shape.
/ben


*** These files in extern/bullet2 are NOT part of the Blender build yet ***

This is the new refactored version of Bullet physics library version 2.x

Soon this will replace the old Bullet version in extern/bullet.
First the integration in Blender Game Engine needs to be updated.
Once that is done all build systems can be updated to use/build extern/bullet2 files.

Questions? mail blender at erwincoumans.com, or check the bf-blender mailing list.
Thanks,
Erwin

Apply patches/make_id.patch to prevent duplicated define of MAKE_ID macro in blender
side and bullet side.
Sergey
