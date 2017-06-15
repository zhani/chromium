// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_window.h"

#include <wayland-client.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/xdg_popup_wrapper_v5.h"
#include "ui/ozone/platform/wayland/xdg_popup_wrapper_v6.h"
#include "ui/ozone/platform/wayland/xdg_surface_wrapper_v5.h"
#include "ui/ozone/platform/wayland/xdg_surface_wrapper_v6.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

namespace {

// Factory, which decides which version type of xdg object to build.
class XDGShellObjectFactory {
 public:
  XDGShellObjectFactory() = default;
  ~XDGShellObjectFactory() = default;

  std::unique_ptr<XDGSurfaceWrapper> CreateXDGSurface(
      WaylandConnection* connection,
      WaylandWindow* wayland_window) {
    if (connection->shell_v6())
      return base::MakeUnique<XDGSurfaceWrapperV6>(wayland_window);

    DCHECK(connection->shell());
    return base::MakeUnique<XDGSurfaceWrapperV5>(wayland_window);
  }

  std::unique_ptr<XDGPopupWrapper> CreateXDGPopup(
      WaylandConnection* connection,
      WaylandWindow* wayland_window) {
    if (connection->shell_v6()) {
      std::unique_ptr<XDGSurfaceWrapper> surface =
          CreateXDGSurface(connection, wayland_window);
      surface->Initialize(connection, wayland_window->surface(), false);
      return base::MakeUnique<XDGPopupWrapperV6>(std::move(surface),
                                                 wayland_window);
    }
    DCHECK(connection->shell());
    return base::MakeUnique<XDGPopupWrapperV5>(wayland_window);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(XDGShellObjectFactory);
};

static WaylandWindow* g_current_capture_ = nullptr;

// TODO(msisov, tonikitoo): fix customization according to screen resolution
// once we are able to get global coordinates of wayland windows.
gfx::Rect TranslateBoundsToScreenCoordinates(const gfx::Rect& child_bounds,
                                             const gfx::Rect& parent_bounds) {
  int x = child_bounds.x() - parent_bounds.x();
  int y = child_bounds.y() - parent_bounds.y();
  return gfx::Rect(gfx::Point(x, y), child_bounds.size());
}

}  // namespace

WaylandWindow::WaylandWindow(PlatformWindowDelegate* delegate,
                             WaylandConnection* connection,
                             const gfx::Rect& bounds)
    : delegate_(delegate),
      connection_(connection),
      xdg_shell_objects_factory_(new XDGShellObjectFactory()),
      bounds_(bounds) {}

WaylandWindow::~WaylandWindow() {
  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  connection_->RemoveWindow(surface_.id());
  if (parent_window_)
    parent_window_->set_child_window(nullptr);
}

// static
WaylandWindow* WaylandWindow::FromSurface(wl_surface* surface) {
  return static_cast<WaylandWindow*>(
      wl_proxy_get_user_data(reinterpret_cast<wl_proxy*>(surface)));
}

bool WaylandWindow::Initialize() {
  DCHECK(xdg_shell_objects_factory_);

  surface_.reset(wl_compositor_create_surface(connection_->compositor()));
  if (!surface_) {
    LOG(ERROR) << "Failed to create wl_surface";
    return false;
  }
  wl_surface_set_user_data(surface_.get(), this);

  // There is now default initialization for this type. Initialize it
  // to ::WINDOW here. It will be changed by delelgate if it know the
  // type of the window.
  ui::PlatformWindowType ui_window_type =
      ui::PlatformWindowType::PLATFORM_WINDOW_TYPE_WINDOW;
  delegate_->GetWindowType(&ui_window_type);
  if (ui_window_type == ui::PlatformWindowType::PLATFORM_WINDOW_TYPE_WINDOW)
    CreateXdgSurface();
  else
    CreateXdgPopup();

  connection_->ScheduleFlush();

  connection_->AddWindow(surface_.id(), this);
  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  delegate_->OnAcceleratedWidgetAvailable(surface_.id(), 1.f);

  return true;
}

void WaylandWindow::CreateXdgPopup() {
  if (!parent_window_)
    parent_window_ = connection_->GetCurrentFocusedWindow();

  DCHECK(parent_window_);

  gfx::Rect bounds =
      TranslateBoundsToScreenCoordinates(bounds_, parent_window_->GetBounds());

  xdg_popup_ = xdg_shell_objects_factory_->CreateXDGPopup(connection_, this);
  if (!xdg_popup_.get() ||
      !xdg_popup_->Initialize(connection_, surface(), parent_window_->surface(),
                              bounds)) {
    CHECK(false) << "Failed to create xdg_popup";
  }

  parent_window_->set_child_window(this);
}

void WaylandWindow::CreateXdgSurface() {
  xdg_surface_ =
      xdg_shell_objects_factory_->CreateXDGSurface(connection_, this);
  if (!xdg_surface_ ||
      !xdg_surface_->Initialize(connection_, surface_.get(), true)) {
    CHECK(false) << "Failed to create xdg_surface";
  }
}

void WaylandWindow::ApplyPendingBounds() {
  if (pending_bounds_.IsEmpty())
    return;

  SetBounds(pending_bounds_);
  DCHECK(xdg_surface_);
  xdg_surface_->SetWindowGeometry(bounds_);
  xdg_surface_->AckConfigure();
  pending_bounds_ = gfx::Rect();
  connection_->ScheduleFlush();
}

bool WaylandWindow::HasCapture() {
  return g_current_capture_ == this;
}

void WaylandWindow::Show() {
  if (xdg_surface_)
    return;
  if (!xdg_popup_) {
    CreateXdgPopup();
    connection_->ScheduleFlush();
  }
}

void WaylandWindow::Hide() {
  if (child_window_)
    child_window_->Hide();
  if (xdg_popup_) {
    parent_window_->set_child_window(nullptr);
    xdg_popup_.reset();
    // Detach buffer from surface in order to completely shutdown popups and
    // release resources.
    wl_surface_attach(surface_.get(), NULL, 0, 0);
    wl_surface_commit(surface_.get());
  }
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
  xdg_surface_->SetTitle(title);
  connection_->ScheduleFlush();
}

void WaylandWindow::SetCapture() {
  if (HasCapture())
    return;

  WaylandWindow* old_capture = g_current_capture_;
  if (old_capture)
    old_capture->delegate()->OnLostCapture();

  g_current_capture_ = this;
}

void WaylandWindow::ReleaseCapture() {
  if (HasCapture())
    g_current_capture_ = nullptr;
}

void WaylandWindow::ToggleFullscreen() {
  // Don't fullscreen popup.
  if (xdg_popup_)
    return;

  DCHECK(xdg_surface_);

  // TODO(msisov, tonikitoo): add multiscreen support. As the documentation says
  // if xdg_surface_set_fullscreen() is not provided with wl_output, it's up to
  // the compositor to choose which display will be used to map this surface.
  if (!IsFullScreen())
    xdg_surface_->SetFullScreen();
  else
    xdg_surface_->UnSetFullScreen();

  connection_->ScheduleFlush();
}

void WaylandWindow::Maximize() {
  // Don't maximize popup.
  if (xdg_popup_ || IsMaximized())
    return;

  DCHECK(xdg_surface_);
  // Unfullscreen the window if it is fullscreen.
  if (IsFullScreen())
    ToggleFullscreen();

  xdg_surface_->SetMaximized();
  connection_->ScheduleFlush();
}

void WaylandWindow::Minimize() {
  // Don't Minimize popup.
  if (xdg_popup_)
    return;

  DCHECK(xdg_surface_);
  if (IsMinimized())
    return;

  xdg_surface_->SetMinimized();
  connection_->ScheduleFlush();
  is_minimized_ = true;
}

void WaylandWindow::Restore() {
  if (xdg_popup_ || !xdg_surface_)
    return;

  // Unfullscreen the window if it is fullscreen.
  if (IsFullScreen())
    ToggleFullscreen();

  if (IsMaximized()) {
    xdg_surface_->UnSetMaximized();
    connection_->ScheduleFlush();
  }
  is_minimized_ = false;
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

void WaylandWindow::PerformNativeWindowDragOrResize(uint32_t hittest) {
  if (hittest == HTCAPTION)
    xdg_surface_->SurfaceMove(connection_);
  else
    xdg_surface_->SurfaceResize(connection_, hittest);
}

bool WaylandWindow::CanDispatchEvent(const PlatformEvent& native_event) {
  if (HasCapture())
    return true;

  // If another window has capture, return early before checking focus.
  if (g_current_capture_)
    return false;

  Event* event = static_cast<Event*>(native_event);
  if (event->IsMouseEvent())
    return has_pointer_focus_;
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

void WaylandWindow::HandleSurfaceConfigure(int32_t width,
                                           int32_t height,
                                           bool is_maximized,
                                           bool is_fullscreen) {
  // Width or height set 0 means that we should decide on width and height by
  // ourselves, but we don't want to set to anything else. Use previous size.
  if (width == 0 || height == 0) {
    width = GetBounds().width();
    height = GetBounds().height();
  }

  ResetWindowStates();
  is_maximized_ = is_maximized;
  is_fullscreen_ = is_fullscreen;

  ui::PlatformWindowState state =
      ui::PlatformWindowState::PLATFORM_WINDOW_STATE_NORMAL;
  if (IsMaximized())
    state = ui::PlatformWindowState::PLATFORM_WINDOW_STATE_MAXIMIZED;
  if (IsFullScreen())
    state = ui::PlatformWindowState::PLATFORM_WINDOW_STATE_FULLSCREEN;
  delegate_->OnWindowStateChanged(state);

  // Rather than call SetBounds here for every configure event, just save the
  // most recent bounds, and have WaylandConnection call ApplyPendingBounds
  // when it has finished processing events. We may get many configure events
  // in a row during an interactive resize, and only the last one matters.
  pending_bounds_ = gfx::Rect(0, 0, width, height);
}

void WaylandWindow::OnCloseRequest() {
  // Before calling OnCloseRequest, the |xdg_popup_| must become hidden and
  // only then call OnCloseRequest().
  DCHECK(!xdg_popup_);
  delegate_->OnCloseRequest();
}

bool WaylandWindow::IsMinimized() {
  return is_minimized_;
}

bool WaylandWindow::IsMaximized() {
  return is_maximized_;
}

bool WaylandWindow::IsFullScreen() {
  return is_fullscreen_;
}

void WaylandWindow::ResetWindowStates() {
  is_maximized_ = false;
  is_fullscreen_ = false;
}

}  // namespace ui
