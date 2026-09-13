package org.legoisland.isle;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.util.Log;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

/** A folder picked through the storage access framework, as GameFileCopier reads it. */
final class DocumentTreeSource implements GameFileCopier.Source {
    private static final String TAG = "IsleActivity";
    private static final String[] COLUMNS = {
        DocumentsContract.Document.COLUMN_DOCUMENT_ID,
        DocumentsContract.Document.COLUMN_DISPLAY_NAME,
        DocumentsContract.Document.COLUMN_MIME_TYPE,
        DocumentsContract.Document.COLUMN_SIZE
    };

    private final ContentResolver mResolver;
    private final Uri mTree;

    DocumentTreeSource(ContentResolver resolver, Uri tree) {
        mResolver = resolver;
        mTree = tree;
    }

    @Override
    public GameFileCopier.Node root() {
        return new GameFileCopier.Node(DocumentsContract.getTreeDocumentId(mTree), "", true, -1);
    }

    @Override
    public List<GameFileCopier.Node> list(GameFileCopier.Node directory) {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(mTree, directory.id);
        Cursor cursor;
        try {
            cursor = mResolver.query(children, COLUMNS, null, null, null);
        }
        catch (RuntimeException e) {
            Log.e(TAG, "Failed to list children of " + directory.id, e);
            return null;
        }
        if (cursor == null) {
            return null;
        }

        List<GameFileCopier.Node> nodes = new ArrayList<>();
        try {
            while (cursor.moveToNext()) {
                nodes.add(new GameFileCopier.Node(
                        cursor.getString(0),
                        cursor.getString(1),
                        DocumentsContract.Document.MIME_TYPE_DIR.equals(cursor.getString(2)),
                        cursor.isNull(3) ? -1 : cursor.getLong(3)));
            }
        }
        finally {
            cursor.close();
        }
        return nodes;
    }

    @Override
    public InputStream open(GameFileCopier.Node file) throws IOException {
        return mResolver.openInputStream(DocumentsContract.buildDocumentUriUsingTree(mTree, file.id));
    }
}
