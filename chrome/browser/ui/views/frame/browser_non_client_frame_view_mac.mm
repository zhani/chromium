// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mac.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/cocoa/fullscreen/fullscreen_menubar_tracker.h"
#include "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller_views.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/hit_test.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"

namespace {
FullscreenToolbarStyle GetUserPreferredToolbarStyle(
    const PrefService* pref_service) {
  return pref_service->GetBoolean(prefs::kShowFullscreenToolbar)
             ? FullscreenToolbarStyle::TOOLBAR_PRESENT
             : FullscreenToolbarStyle::TOOLBAR_HIDDEN;
}
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, public:

BrowserNonClientFrameViewMac::BrowserNonClientFrameViewMac(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view) {
  fullscreen_toolbar_controller_.reset([[FullscreenToolbarControllerViews alloc]
      initWithBrowserView:browser_view]);
  PrefService* pref_service = browser_view->GetProfile()->GetPrefs();
  [fullscreen_toolbar_controller_
      setToolbarStyle:GetUserPreferredToolbarStyle(pref_service)];

  pref_registrar_.Init(pref_service);
  pref_registrar_.Add(
      prefs::kShowFullscreenToolbar,
      base::BindRepeating(&BrowserNonClientFrameViewMac::UpdateFullscreenTopUI,
                          base::Unretained(this), true));
}

BrowserNonClientFrameViewMac::~BrowserNonClientFrameViewMac() {
  if ([fullscreen_toolbar_controller_ isInFullscreen])
    [fullscreen_toolbar_controller_ exitFullscreenMode];
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, BrowserNonClientFrameView implementation:

void BrowserNonClientFrameViewMac::OnFullscreenStateChanged() {
  if (browser_view()->IsFullscreen()) {
    [fullscreen_toolbar_controller_ enterFullscreenMode];
  } else {
    // Exiting tab fullscreen requires updating Top UI.
    // Called from here so we can capture exiting tab fullscreen both by
    // pressing 'ESC' key and by clicking green traffic light button.
    UpdateFullscreenTopUI(false);
    [fullscreen_toolbar_controller_ exitFullscreenMode];
  }
  browser_view()->Layout();
}

bool BrowserNonClientFrameViewMac::CaptionButtonsOnLeadingEdge() const {
  return true;
}

gfx::Rect BrowserNonClientFrameViewMac::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  // TODO(weili): In the future, we should hide the title bar, and show the
  // tab strip directly under the menu bar. For now, just lay our content
  // under the native title bar. Use the default title bar height to avoid
  // calling through private APIs.
  DCHECK(tabstrip);

  const int x = GetTabStripLeftInset();
  const bool restored = !frame()->IsMaximized() && !frame()->IsFullscreen();
  return gfx::Rect(x, GetTopInset(restored), width() - x - GetTabstripPadding(),
                   tabstrip->GetPreferredSize().height());
}

int BrowserNonClientFrameViewMac::GetTopInset(bool restored) const {
  if (!browser_view()->IsTabStripVisible())
    return 0;

  // Mac seems to reserve 1 DIP of the top inset as a resize handle.
  constexpr int kResizeHandleHeight = 1;
  constexpr int kTabstripTopInset = 8;
  int top_inset = kTabstripTopInset;
  if (EverHasVisibleBackgroundTabShapes()) {
    top_inset =
        std::max(top_inset, BrowserNonClientFrameView::kMinimumDragHeight +
                                kResizeHandleHeight);
  }

  // Calculate the y offset for the tab strip because in fullscreen mode the tab
  // strip may need to move under the slide down menu bar.
  CGFloat y_offset = TopUIFullscreenYOffset();
  if (y_offset > 0) {
    // When menubar shows up, we need to update mouse tracking area.
    NSWindow* window = GetWidget()->GetNativeWindow();
    NSRect content_bounds = [[window contentView] bounds];
    // Backing bar tracking area uses native coordinates.
    CGFloat tracking_height =
        FullscreenBackingBarHeight() + top_inset + y_offset;
    NSRect backing_bar_area =
        NSMakeRect(0, NSMaxY(content_bounds) - tracking_height,
                   NSWidth(content_bounds), tracking_height);
    [fullscreen_toolbar_controller_ updateToolbarFrame:backing_bar_area];
  }

  return y_offset + top_inset;
}

int BrowserNonClientFrameViewMac::GetThemeBackgroundXInset() const {
  return 0;
}

void BrowserNonClientFrameViewMac::UpdateFullscreenTopUI(
    bool needs_check_tab_fullscreen) {
  FullscreenToolbarStyle old_style =
      [fullscreen_toolbar_controller_ toolbarStyle];

  // Update to the new toolbar style if needed.
  FullscreenToolbarStyle new_style;
  FullscreenController* controller =
      browser_view()->GetExclusiveAccessManager()->fullscreen_controller();
  if ((controller->IsWindowFullscreenForTabOrPending() ||
       controller->IsExtensionFullscreenOrPending()) &&
      needs_check_tab_fullscreen) {
    new_style = FullscreenToolbarStyle::TOOLBAR_NONE;
  } else {
    new_style =
        GetUserPreferredToolbarStyle(browser_view()->GetProfile()->GetPrefs());
  }
  [fullscreen_toolbar_controller_ setToolbarStyle:new_style];

  if (![fullscreen_toolbar_controller_ isInFullscreen] ||
      old_style == new_style)
    return;

  // Notify browser that top ui state has been changed so that we can update
  // the bookmark bar state as well.
  browser_view()->browser()->FullscreenTopUIStateChanged();

  // Re-layout if toolbar style changes in fullscreen mode.
  if (frame()->IsFullscreen())
    browser_view()->Layout();

  [FullscreenToolbarController recordToolbarStyle:new_style];
}

bool BrowserNonClientFrameViewMac::ShouldHideTopUIForFullscreen() const {
  if (frame()->IsFullscreen()) {
    return [fullscreen_toolbar_controller_ toolbarStyle] !=
           FullscreenToolbarStyle::TOOLBAR_PRESENT;
  }
  return false;
}

void BrowserNonClientFrameViewMac::UpdateThrobber(bool running) {
}

int BrowserNonClientFrameViewMac::GetTabStripLeftInset() const {
  constexpr int kTabstripLeftInset = 70;  // Make room for caption buttons.
  // Do not draw caption buttons on fullscreen.
  return frame()->IsFullscreen() ? 0 : kTabstripLeftInset;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, views::NonClientFrameView implementation:

gfx::Rect BrowserNonClientFrameViewMac::GetBoundsForClientView() const {
  return bounds();
}

gfx::Rect BrowserNonClientFrameViewMac::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return client_bounds;
}

int BrowserNonClientFrameViewMac::NonClientHitTest(const gfx::Point& point) {
  // BrowserView::NonClientHitTest will return HTNOWHERE for points that hit
  // the native title bar. On Mac, we need to explicitly return HTCAPTION for
  // those points.
  const int component = frame()->client_view()->NonClientHitTest(point);
  return (component == HTNOWHERE && bounds().Contains(point)) ? HTCAPTION
                                                              : component;
}

void BrowserNonClientFrameViewMac::GetWindowMask(const gfx::Size& size,
                                                 gfx::Path* window_mask) {
}

void BrowserNonClientFrameViewMac::ResetWindowControls() {
}

void BrowserNonClientFrameViewMac::UpdateWindowIcon() {
}

void BrowserNonClientFrameViewMac::UpdateWindowTitle() {
}

void BrowserNonClientFrameViewMac::SizeConstraintsChanged() {
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, views::View implementation:

gfx::Size BrowserNonClientFrameViewMac::GetMinimumSize() const {
  gfx::Size size = browser_view()->GetMinimumSize();
  constexpr gfx::Size kMinTabbedWindowSize(400, 272);
  constexpr gfx::Size kMinPopupWindowSize(100, 122);
  size.SetToMax(browser_view()->browser()->is_type_tabbed()
                    ? kMinTabbedWindowSize
                    : kMinPopupWindowSize);
  return size;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, protected:

// views::View:

void BrowserNonClientFrameViewMac::OnPaint(gfx::Canvas* canvas) {
  if (!browser_view()->IsBrowserTypeNormal())
    return;

  canvas->DrawColor(GetFrameColor());

  if (!GetThemeProvider()->UsingSystemTheme())
    PaintThemedFrame(canvas);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, private:

void BrowserNonClientFrameViewMac::PaintThemedFrame(gfx::Canvas* canvas) {
  gfx::ImageSkia image = GetFrameImage();
  canvas->TileImageInt(image, 0, TopUIFullscreenYOffset(), width(),
                       image.height());
  gfx::ImageSkia overlay = GetFrameOverlayImage();
  canvas->DrawImageInt(overlay, 0, 0);
}

CGFloat BrowserNonClientFrameViewMac::FullscreenBackingBarHeight() const {
  BrowserView* browser_view = this->browser_view();
  DCHECK(browser_view->IsFullscreen());

  CGFloat total_height = 0;
  if (browser_view->IsTabStripVisible())
    total_height += browser_view->GetTabStripHeight();

  if (browser_view->IsToolbarVisible())
    total_height += browser_view->GetToolbarBounds().height();

  if (browser_view->IsBookmarkBarVisible() &&
      browser_view->GetBookmarkBarView()->IsDetached()) {
    // Only when the bookmark bar is shown and not in 'detached' mode, it will
    // show up along with slide down panel.
    total_height += browser_view->GetBookmarkBarView()->height();
  }

  return total_height;
}

int BrowserNonClientFrameViewMac::TopUIFullscreenYOffset() const {
  if (!browser_view()->IsTabStripVisible() || !browser_view()->IsFullscreen())
    return 0;

  CGFloat menu_bar_height =
      [[[NSApplication sharedApplication] mainMenu] menuBarHeight];
  CGFloat title_bar_height =
      NSHeight([NSWindow frameRectForContentRect:NSZeroRect
                                       styleMask:NSWindowStyleMaskTitled]);
  return [[fullscreen_toolbar_controller_ menubarTracker] menubarFraction] *
         (menu_bar_height + title_bar_height);
}
