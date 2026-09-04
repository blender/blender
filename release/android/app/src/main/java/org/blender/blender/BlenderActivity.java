package org.blender.blender;

import android.content.res.AssetManager;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.system.Os;
import android.util.Log;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import org.libsdl.app.SDLActivity;

/**
 * Main Blender Activity, extending SDLActivity.
 *
 * Takes care of extracting the Blender portable version runtime directory from the APK assets,
 * in addition to some SDL specific overrides to load the main Blender shared library and an
 * argument helper.
 */
public class BlenderActivity extends SDLActivity {
    private static final String LOG_TAG = "blender";

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
        /* Utility hook to pass command-line start-up arguments via ADB.
         *
         * Usage example:
         *   adb shell am start -n org.blender.blender/.BlenderActivity --es args '"--debug-gpu --log ghost.*,gpu.* --log-level debug"'
         *
         * TODO: Disable for non-debug builds, as otherwise this could lead to injection (any other app can launch
         *       the Blender activity with a --python-expr)
         */

        /* NOTE: naively whitespace-split, quoting (as in --python-expr "something space something") is not supported. */
        List<String> arguments = new ArrayList<>();
        Intent intent = getIntent();

        String extraArguments = intent.getStringExtra("args");
        if (extraArguments != null && !extraArguments.trim().isEmpty()) {
            Collections.addAll(arguments, extraArguments.trim().split("\\s+"));
        }

        /* Support opening a .blend file from an Android file browser, copying the file from the opening itent (if any)
           to app-private storage to access the file via a normal path, and add the resulting path to the argument list.*/
        try {
            /* TODO: Handle file intents received while Blender is already running. */
            String blendFile = copyBlendFileFromIntent(intent);
            if (blendFile != null) {
                arguments.add(blendFile);
            }
        }
        catch (IOException e) {
            Log.e(LOG_TAG, "Failed to copy blend file from Android file browser", e);
        }
        return arguments.toArray(new String[0]);
    }

    private String copyBlendFileFromIntent(Intent intent) throws IOException {
        Uri uri = Intent.ACTION_VIEW.equals(intent.getAction()) ? intent.getData() : null;
        if (uri == null) {
            return null;
        }

        File incomingDir = new File(getCacheDir(), "incoming");
        deleteRecursive(incomingDir);
        Files.createDirectories(incomingDir.toPath());

        File destination = Files.createTempFile(incomingDir.toPath(), "blend-", ".blend").toFile();
        try (InputStream source = getContentResolver().openInputStream(uri)) {
            if (source == null) {
                throw new IOException("Failed to open incoming URI: " + uri);
            }
            Files.copy(source, destination.toPath(), StandardCopyOption.REPLACE_EXISTING);
        }

        Log.i(LOG_TAG, "Copied incoming .blend file to " + destination);
        return destination.getAbsolutePath();
    }

    private static void extractAssetDir(AssetManager assets, String path, File targetDir) throws IOException {
        String[] names = assets.list(path);
        if (names == null || names.length == 0) {
            try (InputStream in = assets.open(path)) {
                Files.copy(in, new File(targetDir, path).toPath(), StandardCopyOption.REPLACE_EXISTING);
            }
            return;
        }
        File dir = new File(targetDir, path);
        if (!dir.mkdirs() && !dir.isDirectory()) {
            throw new IOException("Failed to create " + dir);
        }
        for (String name : names) {
            extractAssetDir(assets, path + "/" + name, targetDir);
        }
    }

    private static void deleteRecursive(File file) throws IOException {
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) {
                deleteRecursive(child);
            }
        }
        if (file.exists() && !file.delete()) {
            throw new IOException("Failed to delete " + file);
        }
    }

    /* Ensure the extracted portable version directory is up-to-date and re-extract it if needed. */
    private File extractIfNeeded() throws Exception {
        String extractDirName = "extract";
        String stampFileName = ".extract_stamp";

        File extractDir = new File(getFilesDir(), extractDirName);
        File stampFile = new File(getFilesDir(), stampFileName);

        /* Stamp token: `lastUpdateTime` changes when the app has been updated or re-installed, clean
         * and re-extract in this case. */
        String stampToken = Long.toString(
            getPackageManager().getPackageInfo(getPackageName(), 0).lastUpdateTime);

        if (extractDir.isDirectory()
            && stampFile.isFile()
            && stampToken.equals(new String(Files.readAllBytes(stampFile.toPath()), StandardCharsets.UTF_8))) {
            return extractDir;
        }

        Log.i(LOG_TAG, "Portable version directory missing or outdated, clearing and extracting...");

        /* Fully clean rather than overwrite to ensure files dropped since the previous version do not
         * linger in the extracted tree. */
        deleteRecursive(extractDir);
        extractAssetDir(getAssets(), extractDirName, getFilesDir());

        /* Written last to ensure an interrupted extraction leaves no stamp and re-runs on the next launch. */
        Files.write(stampFile.toPath(), stampToken.getBytes(StandardCharsets.UTF_8));

        Log.i(LOG_TAG, "Portable version directory extracted to " + extractDir);
        return extractDir;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try {
            File extractDir = extractIfNeeded();

            /* Pass the extractDir path via an environment variable as getFilesDir() isn't directly
             * exposed by the NDK. Also allows defining its paths in less places. Temporary, shall be
             * replaced by a NativeActivity property access, or direct access to the APK assets which
             * would remove the need for extraction altogether. */
            Os.setenv("BLENDER_ANDROID_EXTRACT_DIR", extractDir.getAbsolutePath(), true);

            /* User home and temp directories, emulated from Linux and fed via environment variables.
             * For now, give a custom sub-folder for $HOME, without which the user drops into the app
             * files root (getFilesDir()).
             * TODO: Replace with a proper Blender-side Android implementation. */
            File homeDir = new File(getFilesDir(), "home");
            homeDir.mkdir();
            Os.setenv("HOME", homeDir.getAbsolutePath(), true);
            Os.setenv("TMPDIR", getCacheDir().getAbsolutePath(), true);
        } catch (Exception e) {
            throw new RuntimeException("Failed to extract Blender runtime files", e);
        }

        super.onCreate(savedInstanceState);
    }
}
