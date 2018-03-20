// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WEBOS_SURFACE_WRAPPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_WEBOS_SURFACE_WRAPPER_H_

#include <wayland-webos-shell-client-protocol.h>

#include "ui/ozone/platform/wayland/xdg_surface_wrapper.h"

#include "base/macros.h"

namespace ui {

class WaylandConnection;
class WaylandWindow;

// not really XDG surface...
class WebOSSurfaceWrapper : public XDGSurfaceWrapper {
 public:
  WebOSSurfaceWrapper(WaylandWindow* wayland_window);
  ~WebOSSurfaceWrapper() override;

  // XDGSurfaceWrapper:
  bool Initialize(WaylandConnection* connection,
                  wl_surface* surface,
                  bool with_toplevel /* not used */) override;
  void SetMaximized() override;
  void UnSetMaximized() override;
  void SetFullscreen() override;
  void UnSetFullscreen() override;
  void SetMinimized() override;
  void SurfaceMove(WaylandConnection* connection) override;
  void SurfaceResize(WaylandConnection* connection, uint32_t hittest) override;
  void SetTitle(const base::string16& title) override;
  void AckConfigure() override;
  void SetWindowGeometry(const gfx::Rect& bounds) override;

  static void HandleStateChanged(
      void* data,
      struct wl_webos_shell_surface* webos_shell_surface,
      uint32_t state);

  static void HandlePositionChanged(
      void* data,
      struct wl_webos_shell_surface* webos_shell_surface,
      int32_t x,
      int32_t y);

  static void HandleClose(void* data,
                          struct wl_webos_shell_surface* webos_shell_surface);

  static void HandleExposed(void* data,
                            struct wl_webos_shell_surface* webos_shell_surface,
                            struct wl_array* rectangles);

  static void HandleStateAboutToChange(
      void* data,
      struct wl_webos_shell_surface* webos_shell_surface,
      uint32_t state);
 private:
  WaylandWindow* wayland_window_;
  bool minimized_;
  bool maximized_;
  bool fullscreen_;
  bool active_;
  wl::Object<wl_shell_surface> wl_shell_surface_;
  wl::Object<wl_webos_shell_surface> wl_webos_shell_surface_;

  DISALLOW_COPY_AND_ASSIGN(WebOSSurfaceWrapper);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WEBOS_SURFACE_WRAPPER_H_
