package org.blender.blender;

import org.libsdl.app.SDLActivity;

/**
 * Main Blender Activity, extending SDLActivity.
 */
public class BlenderActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        /* Add libblender.so (main Blender executable). */
        return new String[] {
            "SDL3",
            "blender",
        };
    }

    @Override
    protected String[] getArguments() {
        // Utility hook to pass command-line start-up arguments via ADB.

        // Usage example:
        //   adb shell am start -n org.blender.blender/.BlenderActivity --es args '"--debug-gpu --log ghost.*,gpu.* --log-level debug"'

        // NOTE: naively whitespace-split, quoting (as in --python-expr "something space something") is not supported.
        String extra = getIntent().getStringExtra("args");
        if (extra == null || extra.trim().isEmpty()) {
            return new String[0];
        }
        return extra.trim().split("\\s+");
    }
}
