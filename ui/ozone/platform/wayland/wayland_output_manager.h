// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_OUTPUT_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_OUTPUT_MANAGER_H_

#include "ui/ozone/platform/wayland/wayland_object.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/wayland_output.h"
#include "ui/ozone/platform/wayland/wayland_screen.h"

struct wl_output;

namespace ui {

class WaylandOutput;

class WaylandOutputManager : public WaylandOutput::Delegate {
 public:
  WaylandOutputManager();
  ~WaylandOutputManager() override;

  // The first output in the vector is always a primary output.
  bool IsPrimaryOutputReady() const;

  void AddWaylandOutput(wl_output* output);
  void RemoveWaylandOutput(wl_output* output);

  // Creates a platform screen and feeds it with existing outputs.
  std::unique_ptr<WaylandScreen> CreateWaylandScreen();

 private:
  void OnWaylandOutputAdded(uint32_t output_id);
  void OnWaylandOutputRemoved(uint32_t output_id);

  // WaylandOutput::Delegate:
  void OnOutputHandleMetrics(uint32_t output_id,
                             const gfx::Rect& new_bounds,
                             int32_t scale_factor) override;

  int64_t next_display_id_ = 0;
  std::vector<std::unique_ptr<WaylandOutput>> output_list_;

  // Non-owned wayland screen instance.
  base::WeakPtr<WaylandScreen> wayland_screen_;

  DISALLOW_COPY_AND_ASSIGN(WaylandOutputManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_OUTPUT_MANAGER_H_
