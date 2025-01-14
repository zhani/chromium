// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;

/** Factory to create CategoryCardViewHolder objects. */
class CategoryCardViewHolderFactory implements RecyclerViewAdapter.ViewHolderFactory<
        CategoryCardViewHolderFactory.CategoryCardViewHolder> {
    /** View holder for the recycler view. */
    public static class CategoryCardViewHolder extends RecyclerView.ViewHolder {
        public CategoryCardViewHolder(View view) {
            super(view);
        }
    }

    @Override
    public CategoryCardViewHolder createViewHolder(
            ViewGroup parent, @CategoryCardAdapter.ViewType int viewType) {
        View view;
        switch (viewType) {
            case CategoryCardAdapter.ViewType.HEADER:
                TextView titleView = new TextView(parent.getContext());
                view = new TextView(parent.getContext());
                break;
            case CategoryCardAdapter.ViewType.CATEGORY:
                view = (ExploreSitesCategoryCardView) LayoutInflater.from(parent.getContext())
                               .inflate(R.layout.explore_sites_category_card_view, null);
                break;
            case CategoryCardAdapter.ViewType.LOADING: // inflate loading spinny
            case CategoryCardAdapter.ViewType.ERROR: // inflate error
                view = new TextView(parent.getContext()); // dummy view.
                break;
            default:
                assert false;
                view = null;
        }
        return new CategoryCardViewHolder(view);
    }
}