// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_SCREEN_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_SCREEN_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/display/display_list.h"
#include "ui/ozone/platform/wayland/wayland_output.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_screen.h"

namespace ui {

// A PlatformScreen implementation for Wayland.
class WaylandScreen : public PlatformScreen {
 public:
  WaylandScreen();
  ~WaylandScreen() override;

  void OnOutputAdded(uint32_t output_id, bool is_primary);
  void OnOutputRemoved(uint32_t output_id);
  void OnOutputMetricsChanged(uint32_t output_id,
                              const gfx::Rect& bounds,
                              float device_pixel_ratio,
                              bool is_primary);

  base::WeakPtr<WaylandScreen> GetWeakPtr();

  // display::Screen implementation.
  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetPrimaryDisplay() const override;
  display::Display GetDisplayForAcceleratedWidget(
      gfx::AcceleratedWidget widget) const override;
  gfx::Point GetCursorScreenPoint() const override;
  gfx::AcceleratedWidget GetAcceleratedWidgetAtScreenPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;

 private:
  display::DisplayList display_list_;

  base::ObserverList<display::DisplayObserver>::Unchecked observers_;

  base::WeakPtrFactory<WaylandScreen> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WaylandScreen);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_SCREEN_H_
