// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/webos_surface_wrapper.h"

#include <wayland-webos-shell-client-protocol.h>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/hit_test.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

namespace ui {

WebOSSurfaceWrapper::WebOSSurfaceWrapper(WaylandWindow* wayland_window)
    : wayland_window_(wayland_window),
      minimized_(false),
      maximized_(false),
      fullscreen_(false),
      active_(false) {}

WebOSSurfaceWrapper::~WebOSSurfaceWrapper() {}

bool WebOSSurfaceWrapper::Initialize(WaylandConnection* connection,
                                     wl_surface* surface,
                                     bool with_toplevel /* not used */) {
  wl_shell_surface_.reset(wl_shell_get_shell_surface(
                        connection->wayland_shell(), surface));

  if (!wl_shell_surface_) {
    LOG(ERROR) << "Failed to create wl_shell_surface_";
    return false;
  }

  wl_webos_shell_surface_.reset(wl_webos_shell_get_shell_surface(
                        connection->webos_shell(), surface));

  if (!wl_webos_shell_surface_) {
    LOG(ERROR) << "Failed to create wl_webos_surface_";
    return false;
  }

  static const wl_webos_shell_surface_listener webos_shell_surface_listener = {
      WebOSSurfaceWrapper::HandleStateChanged,
      WebOSSurfaceWrapper::HandlePositionChanged,
      WebOSSurfaceWrapper::HandleClose,
      WebOSSurfaceWrapper::HandleExposed,
      WebOSSurfaceWrapper::HandleStateAboutToChange};

  wl_webos_shell_surface_add_listener(wl_webos_shell_surface_.get(),
                                      &webos_shell_surface_listener, this);

  wl_shell_surface_set_title(wl_shell_surface_.get(), "MUS Web Browser");

  wl_webos_shell_surface_set_state(wl_webos_shell_surface_.get(),
                                   WL_WEBOS_SHELL_SURFACE_STATE_FULLSCREEN);

  wl_webos_shell_surface_set_property(
      wl_webos_shell_surface_.get(), "appId", "com.webos.app.browser-mus");
  wl_webos_shell_surface_set_property(
      wl_webos_shell_surface_.get(), "_WEBOS_LAUNCH_INFO_RECENT", "true");
  wl_webos_shell_surface_set_property(
      wl_webos_shell_surface_.get(), "_WEBOS_LAUNCH_INFO_REASON", "true");
  wl_webos_shell_surface_set_property(
      wl_webos_shell_surface_.get(), "title", "MUS Web Browser");

  return true;
}

void WebOSSurfaceWrapper::SetMaximized() {
  wl_webos_shell_surface_set_state(wl_webos_shell_surface_.get(),
                                   WL_WEBOS_SHELL_SURFACE_STATE_MAXIMIZED);
}

void WebOSSurfaceWrapper::UnSetMaximized() {
  // Currently this call means restoring normal state
  minimized_ = maximized_ = fullscreen_ = false;
  wl_webos_shell_surface_set_state(wl_webos_shell_surface_.get(),
                                   WL_WEBOS_SHELL_SURFACE_STATE_DEFAULT);
}

void WebOSSurfaceWrapper::SetFullscreen() {
  wl_webos_shell_surface_set_state(wl_webos_shell_surface_.get(),
                                   WL_WEBOS_SHELL_SURFACE_STATE_FULLSCREEN);
}

void WebOSSurfaceWrapper::UnSetFullscreen() {
  NOTIMPLEMENTED();
}

void WebOSSurfaceWrapper::SetMinimized() {
  wl_webos_shell_surface_set_state(wl_webos_shell_surface_.get(),
                                   WL_WEBOS_SHELL_SURFACE_STATE_MINIMIZED);
}

void WebOSSurfaceWrapper::SurfaceMove(WaylandConnection* connection) {
  NOTIMPLEMENTED();
}

void WebOSSurfaceWrapper::SurfaceResize(WaylandConnection* connection,
                                        uint32_t hittest) {
  NOTIMPLEMENTED();
}

void WebOSSurfaceWrapper::SetTitle(const base::string16& title) {
  wl_webos_shell_surface_set_property(
      wl_webos_shell_surface_.get(), "title",base::UTF16ToUTF8(title).c_str());
}

void WebOSSurfaceWrapper::AckConfigure() {
  NOTIMPLEMENTED();
}

void WebOSSurfaceWrapper::SetWindowGeometry(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

// static
void WebOSSurfaceWrapper::HandleStateChanged(
    void* data,
    struct wl_webos_shell_surface* webos_shell_surface,
    uint32_t state) {
  WebOSSurfaceWrapper* surface = static_cast<WebOSSurfaceWrapper*>(data);

  switch (state) {
    case WL_WEBOS_SHELL_SURFACE_STATE_MINIMIZED:
      // Currently surface wrapper getting minimized notify is not expected
      // because xdg has been the main focus and it doesn't provide this notify
      surface->minimized_ = true;
      break;
    case WL_WEBOS_SHELL_SURFACE_STATE_MAXIMIZED:
      surface->maximized_ = true;
      break;
    case WL_WEBOS_SHELL_SURFACE_STATE_FULLSCREEN:
      surface->fullscreen_ = true;
    default:
      break;
  }

  surface->wayland_window_->HandleSurfaceConfigure(0, 0, surface->maximized_,
                                                   surface->fullscreen_,
                                                   surface->active_);
}

void WebOSSurfaceWrapper::HandlePositionChanged(
    void* data,
    struct wl_webos_shell_surface* webos_shell_surface,
    int32_t x,
    int32_t y) {
  NOTIMPLEMENTED();
}

void WebOSSurfaceWrapper::HandleClose(
    void* data,
    struct wl_webos_shell_surface* webos_shell_surface) {
  NOTIMPLEMENTED();
}

void WebOSSurfaceWrapper::HandleExposed(
    void* data,
    struct wl_webos_shell_surface* webos_shell_surface,
    struct wl_array* rectangles) {
  WebOSSurfaceWrapper* surface = static_cast<WebOSSurfaceWrapper*>(data);

  surface->active_ = true;
  surface->wayland_window_->HandleSurfaceConfigure(0, 0, surface->maximized_,
                                                   surface->fullscreen_,
                                                   surface->active_);
}

void WebOSSurfaceWrapper::HandleStateAboutToChange(
    void* data,
    struct wl_webos_shell_surface* webos_shell_surface,
    uint32_t state) {
  NOTIMPLEMENTED();
}

}  // namespace ui
