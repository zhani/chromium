// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_window.h"

#include <xdg-shell-unstable-v5-client-protocol.h>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/events/event.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

namespace {

static WaylandWindow* g_current_capture_ = nullptr;

static const xdg_popup_listener xdg_popup_listener = {
    &WaylandWindow::PopupDone,
};

// TODO(msisov, tonikitoo): fix customization according to screen resolution
// once we are able to get global coordinates of wayland windows.
gfx::Rect TranslateBoundsToScreenCoordinates(const gfx::Rect& child_bounds,
                                            const gfx::Rect& parent_bounds) {
  int x = child_bounds.x() - parent_bounds.x();
  int y = child_bounds.y() - parent_bounds.y();
  return gfx::Rect(gfx::Point(x, y), child_bounds.size());
}
}

WaylandWindow::WaylandWindow(PlatformWindowDelegate* delegate,
                             WaylandConnection* connection,
                             const gfx::Rect& bounds)
    : delegate_(delegate), connection_(connection), bounds_(bounds) {}

WaylandWindow::~WaylandWindow() {
  if (xdg_surface_) {
    PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
    connection_->RemoveWindow(surface_.id());
  }
}

// static
WaylandWindow* WaylandWindow::FromSurface(wl_surface* surface) {
  return static_cast<WaylandWindow*>(
      wl_proxy_get_user_data(reinterpret_cast<wl_proxy*>(surface)));
}

bool WaylandWindow::Initialize() {
  surface_.reset(wl_compositor_create_surface(connection_->compositor()));
  if (!surface_) {
    LOG(ERROR) << "Failed to create wl_surface";
    return false;
  }
  wl_surface_set_user_data(surface_.get(), this);

  // There is now default initialization for this type. Initialize it
  // to ::WINDOW here. It will be changed by delelgate if it know the
  // type of the window.
  ui::mojom::WindowType window_type = ui::mojom::WindowType::WINDOW;
  delegate_->GetWindowType(&window_type);
  if (window_type == ui::mojom::WindowType::WINDOW) {
    static const xdg_surface_listener xdg_surface_listener = {
        &WaylandWindow::Configure, &WaylandWindow::Close,
    };
    xdg_surface_.reset(
        xdg_shell_get_xdg_surface(connection_->shell(), surface_.get()));
    if (!xdg_surface_) {
      LOG(ERROR) << "Failed to create xdg_surface";
      return false;
    }
    xdg_surface_add_listener(xdg_surface_.get(), &xdg_surface_listener, this);
  } else {
    parent_window_ = connection_->GetCurrentFocusedWindow();
    if (!parent_window_) {
      LOG(ERROR) << "Failed to get parent window";
      return false;
    }

    CreatePopupWindow();
    if (!xdg_popup_) {
      LOG(ERROR) << "Failed to create xdg_popup";
      return false;
    }
    parent_window_->set_child_window(this);
  }
  connection_->ScheduleFlush();

  connection_->AddWindow(surface_.id(), this);
  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  delegate_->OnAcceleratedWidgetAvailable(surface_.id(), 1.f);

  return true;
}

void WaylandWindow::ApplyPendingBounds() {
  if (pending_bounds_.IsEmpty())
    return;

  SetBounds(pending_bounds_);
  DCHECK(xdg_surface_);
  xdg_surface_ack_configure(xdg_surface_.get(), pending_configure_serial_);
  pending_bounds_ = gfx::Rect();
  connection_->ScheduleFlush();
}

void WaylandWindow::CreatePopupWindow() {
  DCHECK(parent_window_);
  gfx::Rect bounds =
      TranslateBoundsToScreenCoordinates(bounds_, parent_window_->GetBounds());
  xdg_popup_.reset(xdg_shell_get_xdg_popup(
      connection_->shell(), surface_.get(), parent_window_->surface(),
      connection_->seat(), connection_->serial(), bounds.x(), bounds.y()));
  xdg_popup_add_listener(xdg_popup_.get(), &xdg_popup_listener, this);
}

// TODO(msisov, tonikitoo): we will want to trigger show and hide of all
// windows once we pass minimize events from chrome.
void WaylandWindow::Show() {
  if (xdg_surface_)
    return;
  if (!xdg_popup_) {
    CreatePopupWindow();
    connection_->ScheduleFlush();
  }
}

// TODO(msisov, tonikitoo): we will want to trigger show and hide of all
// windows once we pass minimize events from chrome.
void WaylandWindow::Hide() {
  if (child_window_)
    child_window_->Hide();
  if (xdg_popup_)
    xdg_popup_.reset();
}

void WaylandWindow::Close() {
  NOTIMPLEMENTED();
}

void WaylandWindow::PrepareForShutdown() {}

void WaylandWindow::SetBounds(const gfx::Rect& bounds) {
  if (bounds == bounds_)
    return;
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
}

gfx::Rect WaylandWindow::GetBounds() {
  return bounds_;
}

void WaylandWindow::SetTitle(const base::string16& title) {
  DCHECK(xdg_surface_);
  xdg_surface_set_title(xdg_surface_.get(), UTF16ToUTF8(title).c_str());
  connection_->ScheduleFlush();
}

void WaylandWindow::SetCapture() {
  if (g_current_capture_ == this)
    return;

  WaylandWindow* old_capture = g_current_capture_;
  if (old_capture)
    old_capture->delegate()->OnLostCapture();

  g_current_capture_ = this;
}

void WaylandWindow::ReleaseCapture() {
  if (g_current_capture_ == this)
    g_current_capture_ = nullptr;
}

void WaylandWindow::ToggleFullscreen() {
  NOTIMPLEMENTED();
}

void WaylandWindow::Maximize() {
  DCHECK(xdg_surface_);
  xdg_surface_set_maximized(xdg_surface_.get());
  connection_->ScheduleFlush();
}

void WaylandWindow::Minimize() {
  DCHECK(xdg_surface_);
  xdg_surface_set_minimized(xdg_surface_.get());
  connection_->ScheduleFlush();
}

void WaylandWindow::Restore() {
  DCHECK(xdg_surface_);
  xdg_surface_unset_maximized(xdg_surface_.get());
  connection_->ScheduleFlush();
}

void WaylandWindow::SetCursor(PlatformCursor cursor) {
  NOTIMPLEMENTED();
}

void WaylandWindow::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void WaylandWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

PlatformImeController* WaylandWindow::GetPlatformImeController() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool WaylandWindow::CanDispatchEvent(const PlatformEvent& native_event) {
  Event* event = static_cast<Event*>(native_event);
  if (event->IsMouseEvent()) {
    if (g_current_capture_ == this) {
      if (has_pointer_focus_ || child_window_->is_focused_popup())
        return true;
      else
        return false;
    } else {
      return has_pointer_focus_;
    }
  }
  if (event->IsKeyEvent())
    return has_keyboard_focus_;
  return false;
}

uint32_t WaylandWindow::DispatchEvent(const PlatformEvent& native_event) {
  Event* event = static_cast<Event*>(native_event);
  if (event->IsLocatedEvent() && !has_pointer_focus_)
    ConvertEventLocationToCurrentWindowLocation(event);

  DispatchEventFromNativeUiEvent(
      native_event, base::Bind(&PlatformWindowDelegate::DispatchEvent,
                               base::Unretained(delegate_)));
  return POST_DISPATCH_STOP_PROPAGATION;
}

void WaylandWindow::ConvertEventLocationToCurrentWindowLocation(
    ui::Event* event) {
  WaylandWindow* wayland_window = connection_->GetCurrentFocusedWindow();
  DCHECK_NE(wayland_window, this);
  if (wayland_window && event->IsLocatedEvent()) {
    gfx::Vector2d offset =
        wayland_window->GetBounds().origin() - GetBounds().origin();
    ui::LocatedEvent* located_event = event->AsLocatedEvent();
    gfx::PointF location_in_pixel_in_host =
        located_event->location_f() + gfx::Vector2dF(offset);
    located_event->set_location_f(location_in_pixel_in_host);
    located_event->set_root_location_f(location_in_pixel_in_host);
  }
}

// static
void WaylandWindow::Configure(void* data,
                              xdg_surface* obj,
                              int32_t width,
                              int32_t height,
                              wl_array* states,
                              uint32_t serial) {
  WaylandWindow* window = static_cast<WaylandWindow*>(data);

  // Width or height set 0 means that we should decide on width and height by
  // ourselves, but we don't want to set to anything else. Use previous size.
  if (width == 0 || height == 0) {
    width = window->GetBounds().width();
    height = window->GetBounds().height();
  }

  // Rather than call SetBounds here for every configure event, just save the
  // most recent bounds, and have WaylandConnection call ApplyPendingBounds
  // when it has finished processing events. We may get many configure events
  // in a row during an interactive resize, and only the last one matters.
  window->pending_bounds_ = gfx::Rect(0, 0, width, height);
  window->pending_configure_serial_ = serial;
}

// static
void WaylandWindow::Close(void* data, xdg_surface* obj) {
  NOTIMPLEMENTED();
}

// static
void WaylandWindow::PopupDone(void* data, xdg_popup* obj) {
  WaylandWindow* window = static_cast<WaylandWindow*>(data);
  window->Hide();
  window->delegate_->OnCloseRequest();
}

}  // namespace ui
