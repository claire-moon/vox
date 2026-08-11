package org.vox.digs;

import android.content.pm.ActivityInfo;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.RelativeLayout;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

/** Android host for the unchanged SDL2 DIGS demo. */
public final class DigsActivity extends SDLActivity {
    private static final String TAG = "DIGS";

    private DigsControlOverlay controls;

    private static native void nativeSetDataRoot(String path);

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "digs_demo" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
        super.onCreate(savedInstanceState);
        if (SDLActivity.mBrokenLibraries) {
            return;
        }
        try {
            File dataRoot = new File(getFilesDir(), "digs-data");
            copyAssetTree("share", new File(dataRoot, "share"));
            nativeSetDataRoot(dataRoot.getAbsolutePath());
        } catch (IOException error) {
            Log.e(TAG, "Unable to install the packaged DIGS data", error);
            finish();
            return;
        }

        controls = new DigsControlOverlay(this);
        mLayout.addView(controls, new RelativeLayout.LayoutParams(
                RelativeLayout.LayoutParams.MATCH_PARENT,
                RelativeLayout.LayoutParams.MATCH_PARENT));
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        enableImmersiveMode();
    }

    @Override
    protected void onPause() {
        if (!SDLActivity.mBrokenLibraries) {
            DigsControlOverlay.releaseAll();
        }
        super.onPause();
    }

    @Override
    public void onBackPressed() {
        if (SDLActivity.mBrokenLibraries) {
            super.onBackPressed();
            return;
        }
        DigsControlOverlay.back();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            enableImmersiveMode();
        }
    }

    private void enableImmersiveMode() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    private void copyAssetTree(String assetPath, File destination)
            throws IOException {
        AssetManager assets = getAssets();
        String[] children = assets.list(assetPath);
        if (children != null && children.length > 0) {
            if (!destination.exists() && !destination.mkdirs()) {
                throw new IOException("Could not create " + destination);
            }
            for (String child : children) {
                copyAssetTree(assetPath + "/" + child,
                        new File(destination, child));
            }
            return;
        }
        File parent = destination.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new IOException("Could not create " + parent);
        }
        try (InputStream input = assets.open(assetPath);
             FileOutputStream output = new FileOutputStream(destination)) {
            byte[] buffer = new byte[16384];
            int read;
            while ((read = input.read(buffer)) != -1) {
                output.write(buffer, 0, read);
            }
        }
    }
}
