// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone.h"

#include "base/run_loop.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"

namespace views {

DesktopDragDropClientOzone::DesktopDragDropClientOzone(
    aura::Window* root_window,
    views::DesktopNativeCursorManager* cursor_manager,
    ui::PlatformWindow* platform_window)
    : root_window_(root_window),
      cursor_manager_(cursor_manager),
      platform_window_(platform_window) {}

DesktopDragDropClientOzone::~DesktopDragDropClientOzone() = default;

int DesktopDragDropClientOzone::StartDragAndDrop(
    const ui::OSExchangeData& data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& root_location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  quit_closure_ = run_loop.QuitClosure();

  // Chrome expects starting drag and drop to release capture.
  aura::Window* capture_window =
      aura::client::GetCaptureClient(root_window)->GetGlobalCaptureWindow();
  if (capture_window)
    capture_window->ReleaseCapture();

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);

  initial_cursor_ = source_window->GetHost()->last_cursor();
  drag_operation_ = operation;
  cursor_client->SetCursor(
      cursor_manager_->GetInitializedCursor(ui::CursorType::kGrabbing));

  platform_window_->StartDrag(data, operation, cursor_client->GetCursor());
  in_move_loop_ = true;
  run_loop.Run();
  return drag_operation_;
}

void DesktopDragDropClientOzone::DragCancel() {
  QuitClosure();
  DragDropSessionCompleted();
}

bool DesktopDragDropClientOzone::IsDragDropInProgress() {
  return in_move_loop_;
}

void DesktopDragDropClientOzone::AddObserver(
    aura::client::DragDropClientObserver* observer) {
  NOTIMPLEMENTED();
}

void DesktopDragDropClientOzone::RemoveObserver(
    aura::client::DragDropClientObserver* observer) {
  NOTIMPLEMENTED();
}

void DesktopDragDropClientOzone::OnWindowDestroyed(aura::Window* window) {
  NOTIMPLEMENTED();
}

void DesktopDragDropClientOzone::OnDragSessionClosed(int dnd_action) {
  drag_operation_ = dnd_action;
  QuitClosure();
  DragDropSessionCompleted();
}

void DesktopDragDropClientOzone::DragDropSessionCompleted() {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window_);
  cursor_client->SetCursor(initial_cursor_);
}

void DesktopDragDropClientOzone::QuitClosure() {
  in_move_loop_ = false;
  if (quit_closure_.is_null())
    return;
  std::move(quit_closure_).Run();
}

}  // namespace views
