// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/external_window_tree_factory.h"

#include "services/ui/ws/external_window_access_policy.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_server_delegate.h"
#include "services/ui/ws/window_tree.h"

namespace ui {
namespace ws {

ExternalWindowTreeFactory::ExternalWindowTreeFactory(
    WindowServer* window_server,
    const UserId& user_id)
    : window_server_(window_server), user_id_(user_id) {}

ExternalWindowTreeFactory::~ExternalWindowTreeFactory() {}

void ExternalWindowTreeFactory::Register(
    mojom::WindowTreeRequest tree_request,
    mojom::WindowTreeClientPtr tree_client) {
  // NOTE: The code below is analogous to WS::CreateTreeForWindowManager,
  // but for the sake of an easier rebase, we are concentrating additions
  // like this here.

  bool automatically_create_display_roots = true;

  // TODO(tonikitoo,msisov): Maybe remove the "window manager" suffix
  // if the method name?
  window_server_->delegate()->OnWillCreateTreeForWindowManager(
      automatically_create_display_roots);

  std::unique_ptr<ws::WindowTree> tree(
      new ws::WindowTree(window_server_, user_id_, nullptr /*ServerWindow*/,
                         base::WrapUnique(new ExternalWindowAccessPolicy)));

  std::unique_ptr<ws::DefaultWindowTreeBinding> tree_binding(
      new ws::DefaultWindowTreeBinding(tree.get(), window_server_,
                                       std::move(tree_request),
                                       std::move(tree_client)));

  // Pass nullptr as mojom::WindowTreePtr (3rd parameter), because in external
  // window mode, the WindowTreePtr is created on the aura/WindowTreeClient
  // side.
  //
  // NOTE: WindowServer::AddTree calls WindowTree::Init, which can trigger a
  // WindowTreeClient::OnEmbed call. In the particular flow though, WTC::OnEmbed
  // will not get called because the WindowTree instance was created above
  // taking 'nullptr' as the ServerWindow parameter, hence the WindowTree has no
  // 'root' yet.
  WindowTree* tree_ptr = tree.get();
  window_server_->AddTree(std::move(tree), std::move(tree_binding),
                          nullptr /*mojom::WindowTreePtr*/);
  tree_ptr->ConfigureRootWindowTreeClient(automatically_create_display_roots);
}

}  // namespace ws
}  // namespace ui
