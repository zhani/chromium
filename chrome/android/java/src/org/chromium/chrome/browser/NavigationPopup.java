// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.graphics.Bitmap;
import android.support.annotation.IntDef;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.ListPopupWindow;
import android.widget.ListView;
import android.widget.PopupWindow;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashSet;
import java.util.Set;

/**
 * A popup that handles displaying the navigation history for a given tab.
 */
public class NavigationPopup extends ListPopupWindow implements AdapterView.OnItemClickListener {

    private static final int MAXIMUM_HISTORY_ITEMS = 8;
    private static final int FULL_HISTORY_ENTRY_INDEX = -1;

    /** Specifies the type of navigation popup being shown */
    @IntDef({Type.ANDROID_SYSTEM_BACK, Type.TABLET_BACK, Type.TABLET_FORWARD})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        int ANDROID_SYSTEM_BACK = 0;
        int TABLET_BACK = 1;
        int TABLET_FORWARD = 2;
    }

    private final Profile mProfile;
    private final Context mContext;
    private final NavigationController mNavigationController;
    private NavigationHistory mHistory;
    private final NavigationAdapter mAdapter;
    private final @Type int mType;

    private final int mFaviconSize;

    private DefaultFaviconHelper mDefaultFaviconHelper;

    /**
     * Loads the favicons asynchronously.
     */
    private FaviconHelper mFaviconHelper;

    private boolean mInitialized;

    /**
     * Constructs a new popup with the given history information.
     *
     * @param profile The profile used for fetching favicons.
     * @param context The context used for building the popup.
     * @param navigationController The controller which takes care of page navigations.
     * @param type The type of navigation popup being triggered.
     */
    public NavigationPopup(Profile profile, Context context,
            NavigationController navigationController, @Type int type) {
        super(context, null, android.R.attr.popupMenuStyle);
        mProfile = profile;
        mContext = context;
        mNavigationController = navigationController;
        mType = type;

        boolean isForward = type == Type.TABLET_FORWARD;
        boolean anchorToBottom = type == Type.ANDROID_SYSTEM_BACK;

        mHistory = mNavigationController.getDirectedNavigationHistory(
                isForward, MAXIMUM_HISTORY_ITEMS);
        mHistory.addEntry(new NavigationEntry(FULL_HISTORY_ENTRY_INDEX, UrlConstants.HISTORY_URL,
                null, null, null, mContext.getResources().getString(R.string.show_full_history),
                null, 0));

        setBackgroundDrawable(ApiCompatibilityUtils.getDrawable(mContext.getResources(),
                anchorToBottom ? R.drawable.popup_bg_bottom : R.drawable.popup_bg));
        // By default ListPopupWindow uses the top & bottom padding of the background to determine
        // the vertical offset applied to the window.  This causes the popup to be shifted up
        // by the top padding, and thus we forcibly need to specify a vertical offset of 0 to
        // prevent that.
        if (anchorToBottom) setVerticalOffset(0);

        mAdapter = new NavigationAdapter();
        if (anchorToBottom) mAdapter.reverseOrder();

        setModal(true);
        setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
        setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        setOnItemClickListener(this);
        setAdapter(mAdapter);

        mFaviconSize = mContext.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
    }

    private String buildComputedAction(String action) {
        return (mType == Type.TABLET_FORWARD ? "ForwardMenu_" : "BackMenu_") + action;
    }

    @Override
    public void show() {
        if (!mInitialized) initialize();
        if (!isShowing()) RecordUserAction.record(buildComputedAction("Popup"));
        super.show();
        if (mAdapter.mInReverseOrder) scrollToBottom();
    }

    @Override
    public void dismiss() {
        if (mInitialized) mFaviconHelper.destroy();
        mInitialized = false;
        if (mDefaultFaviconHelper != null) mDefaultFaviconHelper.clearCache();
        super.dismiss();
    }

    private void scrollToBottom() {
        getListView().addOnAttachStateChangeListener(new OnAttachStateChangeListener() {
            @Override
            public void onViewDetachedFromWindow(View v) {
                if (v != null) v.removeOnAttachStateChangeListener(this);
            }

            @Override
            public void onViewAttachedToWindow(View v) {
                ((ListView) v).smoothScrollToPosition(mHistory.getEntryCount() - 1);
            }
        });
    }

    private void initialize() {
        ThreadUtils.assertOnUiThread();
        mInitialized = true;
        mFaviconHelper = new FaviconHelper();

        Set<String> requestedUrls = new HashSet<String>();
        for (int i = 0; i < mHistory.getEntryCount(); i++) {
            NavigationEntry entry = mHistory.getEntryAtIndex(i);
            if (entry.getFavicon() != null) continue;
            final String pageUrl = entry.getUrl();
            if (!requestedUrls.contains(pageUrl)) {
                FaviconImageCallback imageCallback =
                        (bitmap, iconUrl) -> NavigationPopup.this.onFaviconAvailable(pageUrl,
                                bitmap);
                mFaviconHelper.getLocalFaviconImageForURL(
                        mProfile, pageUrl, mFaviconSize, imageCallback);
                requestedUrls.add(pageUrl);
            }
        }
    }

    /**
     * Called when favicon data requested by {@link #initialize()} is retrieved.
     * @param pageUrl the page for which the favicon was retrieved.
     * @param favicon the favicon data.
     */
    private void onFaviconAvailable(String pageUrl, Bitmap favicon) {
        if (favicon == null) {
            if (mDefaultFaviconHelper == null) mDefaultFaviconHelper = new DefaultFaviconHelper();
            favicon = mDefaultFaviconHelper.getDefaultFaviconBitmap(mContext, pageUrl, true);
        }
        for (int i = 0; i < mHistory.getEntryCount(); i++) {
            NavigationEntry entry = mHistory.getEntryAtIndex(i);
            if (TextUtils.equals(pageUrl, entry.getUrl())) entry.updateFavicon(favicon);
        }
        mAdapter.notifyDataSetChanged();
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        NavigationEntry entry = (NavigationEntry) parent.getItemAtPosition(position);
        if (entry.getIndex() == FULL_HISTORY_ENTRY_INDEX) {
            RecordUserAction.record(buildComputedAction("ShowFullHistory"));
            assert mContext instanceof ChromeActivity;
            ChromeActivity activity = (ChromeActivity) mContext;
            HistoryManagerUtils.showHistoryManager(activity, activity.getActivityTab());
        } else {
            int originalPosition =
                    mAdapter.mInReverseOrder ? mAdapter.getCount() - position - 1 : position;
            // 1-based index to keep in line with Desktop implementation.
            RecordUserAction.record(buildComputedAction("HistoryClick" + (originalPosition + 1)));
            mNavigationController.goToNavigationIndex(entry.getIndex());
        }

        dismiss();
    }

    private class NavigationAdapter extends BaseAdapter {
        private Integer mTopPadding;
        boolean mInReverseOrder;

        public void reverseOrder() {
            mInReverseOrder = true;
        }

        @Override
        public int getCount() {
            return mHistory.getEntryCount();
        }

        @Override
        public Object getItem(int position) {
            position = mInReverseOrder ? getCount() - position - 1 : position;
            return mHistory.getEntryAtIndex(position);
        }

        @Override
        public long getItemId(int position) {
            return ((NavigationEntry) getItem(position)).getIndex();
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            EntryViewHolder viewHolder;
            if (convertView == null) {
                LayoutInflater inflater = LayoutInflater.from(parent.getContext());
                convertView = inflater.inflate(R.layout.navigation_popup_item, parent, false);
                viewHolder = new EntryViewHolder();
                viewHolder.mContainer = convertView;
                viewHolder.mImageView = convertView.findViewById(R.id.favicon_img);
                viewHolder.mTextView = convertView.findViewById(R.id.entry_title);
                convertView.setTag(viewHolder);
            } else {
                viewHolder = (EntryViewHolder) convertView.getTag();
            }

            NavigationEntry entry = (NavigationEntry) getItem(position);
            setViewText(entry, viewHolder.mTextView);
            viewHolder.mImageView.setImageBitmap(entry.getFavicon());

            if (mInReverseOrder) {
                View container = viewHolder.mContainer;
                if (mTopPadding == null) {
                    mTopPadding = container.getResources().getDimensionPixelSize(
                            R.dimen.navigation_popup_top_padding);
                }
                viewHolder.mContainer.setPadding(container.getPaddingLeft(),
                        position == 0 ? mTopPadding : 0, container.getPaddingRight(),
                        container.getPaddingBottom());
            }

            return convertView;
        }

        private void setViewText(NavigationEntry entry, TextView view) {
            String entryText = entry.getTitle();
            if (TextUtils.isEmpty(entryText)) entryText = entry.getVirtualUrl();
            if (TextUtils.isEmpty(entryText)) entryText = entry.getUrl();

            view.setText(entryText);
        }
    }

    private static class EntryViewHolder {
        View mContainer;
        ImageView mImageView;
        TextView mTextView;
    }


}
