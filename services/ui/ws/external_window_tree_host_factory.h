// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_EXTERNAL_WINDOW_TREE_HOST_FACTORY_H_
#define SERVICES_UI_WS_EXTERNAL_WINDOW_TREE_HOST_FACTORY_H_

#include <stdint.h>

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/ui/common/types.h"
#include "services/ui/public/interfaces/external_window_tree_host_factory.mojom.h"

namespace ui {
namespace ws {

class WindowServer;

class ExternalWindowTreeHostFactory
    : public mojom::ExternalWindowTreeHostFactory {
 public:
  explicit ExternalWindowTreeHostFactory(WindowServer* window_server);
  ~ExternalWindowTreeHostFactory() override;

  void AddBinding(mojom::ExternalWindowTreeHostFactoryRequest request);

 private:
  using TransportProperties =
      std::unordered_map<std::string, std::vector<uint8_t>>;

  void CreatePlatformWindow(
      mojom::WindowTreeHostRequest tree_host_request,
      Id transport_window_id,
      const TransportProperties& transport_properties) override;

  WindowServer* window_server_;
  mojo::BindingSet<mojom::ExternalWindowTreeHostFactory> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ExternalWindowTreeHostFactory);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_EXTERNAL_WINDOW_TREE_HOST_FACTORY_H_
