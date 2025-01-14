// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * DOM Element containing (page-dependent) navigation buttons for the
 * MultiDevice Setup WebUI.
 */
Polymer({
  is: 'button-bar',

  properties: {
    /** Whether the forward button should be hidden. */
    forwardButtonHidden: {
      type: Boolean,
      value: false,
    },

    /** Whether the backward button should be hidden. */
    backwardButtonHidden: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onForwardButtonClicked_: function() {
    this.fire('forward-navigation-requested');
  },

  /** @private */
  onBackwardButtonClicked_: function() {
    this.fire('backward-navigation-requested');
  },
});
