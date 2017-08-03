// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_WINDOW_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_WINDOW_H_

#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/wayland_object.h"
#include "ui/platform_window/platform_window.h"

namespace ui {

class PlatformWindowDelegate;
class WaylandConnection;
class XDGPopupWrapper;
class XDGSurfaceWrapper;

namespace {
class XDGShellObjectFactory;
}  // namespace

class WaylandWindow : public PlatformWindow, public PlatformEventDispatcher {
 public:
  WaylandWindow(PlatformWindowDelegate* delegate,
                WaylandConnection* connection,
                const gfx::Rect& bounds);
  ~WaylandWindow() override;

  static WaylandWindow* FromSurface(wl_surface* surface);

  bool Initialize();

  wl_surface* surface() { return surface_.get(); }
  XDGSurfaceWrapper* xdg_surface() { return xdg_surface_.get(); }
  XDGPopupWrapper* xdg_popup() { return xdg_popup_.get(); }

  // Apply the bounds specified in the most recent configure event. This should
  // be called after processing all pending events in the wayland connection.
  void ApplyPendingBounds();

  // Set whether this window has pointer focus and should dispatch mouse events.
  void set_pointer_focus(bool focus) { has_pointer_focus_ = focus; }

  // Set whether this window has keyboard focus and should dispatch key events.
  void set_keyboard_focus(bool focus) { has_keyboard_focus_ = focus; }

  // Set whether this window has touch focus and should dispatch touch events.
  void set_touch_focus(bool focus) { has_touch_focus_ = focus; }

  bool has_pointer_or_touch_focus() {
    return has_pointer_focus_ || has_touch_focus_;
  }

  // Tells if it is a focused popup.
  bool is_focused_popup() { return is_popup() && has_pointer_or_touch_focus(); }

  // Tells if this is a popup.
  bool is_popup() { return !!xdg_popup_.get(); }

  // Set a child of this window. It is very important in case of nested
  // xdg_popups as long as we must destroy the very last first and only then
  // its parent.
  void set_child_window(WaylandWindow* window) { child_window_ = window; }

  // PlatformWindow
  void Show() override;
  void Hide() override;
  void Close() override;
  void PrepareForShutdown() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() override;
  void SetTitle(const base::string16& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  PlatformImeController* GetPlatformImeController() override;
  void PerformNativeWindowDragOrResize(uint32_t hittest) override;

  // PlatformEventDispatcher
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  void HandleSurfaceConfigure(int32_t widht,
                              int32_t height,
                              bool is_maximized,
                              bool is_fullscreen);

  void OnCloseRequest();

 protected:
  PlatformWindowDelegate* delegate() { return delegate_; }

 private:
  // TODO(msisov, tonikitoo): share this with X11WindowOzone and
  // DesktopWindowTreeHostX11.
  void ConvertEventLocationToCurrentWindowLocation(ui::Event* located_event);

  // Creates a popup window, which is visible as a menu window.
  void CreateXdgPopup();
  // Creates a surface window, which is visible as a main window.
  void CreateXdgSurface();

  // Tells if |this| has capture.
  bool HasCapture();

  // TODO(msisov, tonikitoo): share this with X11WindowBase.
  bool IsMinimized();
  bool IsMaximized();
  bool IsFullScreen();

  // Resets the maximized and fullscreen window states.
  void ResetWindowStates();

  PlatformWindowDelegate* delegate_;
  WaylandConnection* connection_;
  WaylandWindow* parent_window_ = nullptr;
  WaylandWindow* child_window_ = nullptr;

  // Creates xdg objects based on xdg shell version.
  std::unique_ptr<XDGShellObjectFactory> xdg_shell_objects_factory_;

  wl::Object<wl_surface> surface_;

  // Wrappers around xdg v5 and xdg v6 objects. WaylandWindow doesn't
  // know anything about the version.
  std::unique_ptr<XDGSurfaceWrapper> xdg_surface_;
  std::unique_ptr<XDGPopupWrapper> xdg_popup_;

  gfx::Rect bounds_;
  gfx::Rect pending_bounds_;
  bool has_pointer_focus_ = false;
  bool has_keyboard_focus_ = false;
  bool has_touch_focus_ = false;

  bool is_minimized_ = false;
  bool is_maximized_ = false;
  bool is_fullscreen_ = false;

  DISALLOW_COPY_AND_ASSIGN(WaylandWindow);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_WINDOW_H_
