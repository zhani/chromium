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

class WaylandWindow : public PlatformWindow, public PlatformEventDispatcher {
 public:
  WaylandWindow(PlatformWindowDelegate* delegate,
                WaylandConnection* connection,
                const gfx::Rect& bounds);
  ~WaylandWindow() override;

  static WaylandWindow* FromSurface(wl_surface* surface);

  bool Initialize();

  wl_surface* surface() { return surface_.get(); }

  // Apply the bounds specified in the most recent configure event. This should
  // be called after processing all pending events in the wayland connection.
  void ApplyPendingBounds();

  // Set whether this window has pointer focus and should dispatch mouse events.
  void set_pointer_focus(bool focus) { has_pointer_focus_ = focus; }

  // Set whether this window has keyboard focus and should dispatch key events.
  void set_keyboard_focus(bool focus) { has_keyboard_focus_ = focus; }

  bool has_pointer_focus() { return has_pointer_focus_; }

  // Tells if it is a focused popup.
  bool is_focused_popup() {
    return !!xdg_popup_.get() && has_pointer_focus();
  }

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

  // PlatformEventDispatcher
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  // xdg_surface_listener
  static void Configure(void* data,
                        xdg_surface* obj,
                        int32_t width,
                        int32_t height,
                        wl_array* states,
                        uint32_t serial);
  static void Close(void* data, xdg_surface* obj);

  // xdg_popup_listener
  static void PopupDone(void* data, xdg_popup* obj);

 protected:
  PlatformWindowDelegate* delegate() { return delegate_; }

 private:
  // TODO(msisov, tonikitoo): share this with X11WindowOzone and
  // DesktopWindowTreeHostX11.
  void ConvertEventLocationToCurrentWindowLocation(ui::Event* located_event);

  // Creates a popup window, which is visible as a menu window.
  void CreatePopupWindow();

  PlatformWindowDelegate* delegate_;
  WaylandConnection* connection_;
  WaylandWindow* parent_window_ = nullptr;
  WaylandWindow* child_window_ = nullptr;

  wl::Object<wl_surface> surface_;
  wl::Object<xdg_surface> xdg_surface_;
  wl::Object<xdg_popup> xdg_popup_;

  gfx::Rect bounds_;
  gfx::Rect pending_bounds_;
  uint32_t pending_configure_serial_;
  bool has_pointer_focus_ = false;
  bool has_keyboard_focus_ = false;

  DISALLOW_COPY_AND_ASSIGN(WaylandWindow);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_WINDOW_H_
