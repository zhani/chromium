// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/external_window_tree_host_factory.h"

#include "mojo/public/cpp/bindings/map.h"
#include "services/ui/display/viewport_metrics.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/ws/display.h"
#include "services/ui/ws/display_binding.h"
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/window_manager_display_root.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_tree.h"

namespace ui {
namespace ws {

ExternalWindowTreeHostFactory::ExternalWindowTreeHostFactory(
    WindowServer* window_server)
    : window_server_(window_server) {}

ExternalWindowTreeHostFactory::~ExternalWindowTreeHostFactory() {}

void ExternalWindowTreeHostFactory::AddBinding(
    mojom::ExternalWindowTreeHostFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void ExternalWindowTreeHostFactory::CreatePlatformWindow(
    mojom::WindowTreeHostRequest tree_host_request,
    Id transport_window_id,
    const TransportProperties& transport_properties) {
  WindowTree* tree = window_server_->GetTreeForExternalWindowMode();
  tree->prepare_to_create_root_display(transport_window_id);

  Display* ws_display = new Display(window_server_);

  std::unique_ptr<DisplayBindingImpl> display_binding(new DisplayBindingImpl(
      std::move(tree_host_request), ws_display, nullptr, window_server_));

  // Provide an initial size for the Display.
  std::map<std::string, std::vector<uint8_t>> properties =
      mojo::UnorderedMapToMap(transport_properties);

  display::ViewportMetrics metrics;
  metrics.bounds_in_pixels = gfx::Rect(1024, 768);
  metrics.device_scale_factor = 1.0f;
  metrics.ui_scale_factor = 1.0f;

  auto iter = properties.find(ui::mojom::WindowManager::kBounds_InitProperty);
  if (iter != properties.end())
    metrics.bounds_in_pixels = mojo::ConvertTo<gfx::Rect>(iter->second);

  iter = properties.find(ui::mojom::WindowManager::kWindowType_InitProperty);
  if (iter != properties.end()) {
    metrics.window_type = static_cast<ui::mojom::WindowType>(
        mojo::ConvertTo<int32_t>(iter->second));
  }

  iter =
      properties.find(ui::mojom::WindowManager::kParentWindowId_InitProperty);
  metrics.parent_window_widget_id = gfx::kNullAcceleratedWidget;
  if (iter != properties.end()) {
    ClientWindowId client_window_id(
        tree->MakeClientWindowId(mojo::ConvertTo<int32_t>(iter->second)));
    ServerWindow* server_window = tree->GetWindowByClientId(client_window_id);
    DCHECK(server_window);
    PlatformDisplay* platform_display = GetPlatformDisplay(server_window);
    metrics.parent_window_widget_id = platform_display->GetAcceleratedWidget();
    DCHECK_NE(gfx::kNullAcceleratedWidget, metrics.parent_window_widget_id);
  }

  ws_display->Init(metrics, std::move(display_binding));
}

PlatformDisplay* ExternalWindowTreeHostFactory::GetPlatformDisplay(
    ServerWindow* server_window) {
  WindowManagerDisplayRoot* display_root =
      window_server_->display_manager()->GetWindowManagerDisplayRoot(
          server_window);
  PlatformDisplay* platform_display =
      display_root->display()->platform_display();
  DCHECK(platform_display);
  return platform_display;
}

}  // namespace ws
}  // namespace ui
