package org.legoisland.isle;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

/** What this device offers that a TV may not: the system document pickers, and touch controls. */
final class DeviceSupport {
    private DeviceSupport() {}

    static boolean hasTouchControls(Context context) {
        PackageManager packages = context.getPackageManager();
        return TvSupport.hasTouchControls(packages.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN),
            packages.hasSystemFeature(PackageManager.FEATURE_LEANBACK));
    }

    // The pickers below are asked about before one is launched, because on Android TV the intents
    // resolve to a stub that returns a cancel, which cannot be told from the player backing out.
    // Each intent needs a matching queries entry in AndroidManifest.xml, or from API 30 nothing
    // resolves at all.
    static boolean canPickFolder(Context context) {
        return usable(context, new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE));
    }

    static boolean canCreate(Context context, String type) {
        return usable(context, new Intent(Intent.ACTION_CREATE_DOCUMENT)
            .addCategory(Intent.CATEGORY_OPENABLE).setType(type));
    }

    /** As ActivityResultContracts.OpenDocument asks: any type, narrowed by EXTRA_MIME_TYPES. */
    static boolean canOpen(Context context, String[] types) {
        return usable(context, new Intent(Intent.ACTION_OPEN_DOCUMENT)
            .addCategory(Intent.CATEGORY_OPENABLE).setType("*/*").putExtra(Intent.EXTRA_MIME_TYPES, types));
    }

    private static boolean usable(Context context, Intent intent) {
        ResolveInfo info = context.getPackageManager().resolveActivity(intent, PackageManager.MATCH_DEFAULT_ONLY);
        return TvSupport.isPicker(info == null || info.activityInfo == null ? null : info.activityInfo.packageName);
    }
}
