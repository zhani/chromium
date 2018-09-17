// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone.h"

#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"

namespace views {

namespace {

aura::Window* GetTargetWindow(aura::Window* root_window, const gfx::Point& point) {
  gfx::Point root_location(point);

  root_window->GetHost()->ConvertScreenInPixelsToDIP(&root_location);
  return root_window->GetEventHandlerForPoint(root_location);
}

}  // namespace

DesktopDragDropClientOzone::DesktopDragDropClientOzone(
    aura::Window* root_window,
    views::DesktopNativeCursorManager* cursor_manager,
    ui::PlatformWindow* platform_window,
    gfx::AcceleratedWidget widget)
    : root_window_(root_window),
      cursor_manager_(cursor_manager),
      platform_window_(platform_window),
      target_window_(nullptr),
      delegate_(nullptr),
      os_exchange_data_(nullptr) {}

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

void DesktopDragDropClientOzone::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(target_window_, window);
  target_window_->RemoveObserver(this);
  target_window_ = nullptr;
}

void DesktopDragDropClientOzone::OnDragEnter(
    ui::PlatformWindow* window,
    const gfx::PointF& point,
    std::unique_ptr<ui::OSExchangeData> data,
    int operation) {
  DCHECK(window);

  point_ = point;
  drag_operation_ = operation;
  UpdateTargetWindowAndDelegate(point_);

  if (!data)
    return;

  os_exchange_data_ = std::move(data);

  std::unique_ptr<ui::DropTargetEvent> event = CreateDropTargetEvent();
  if (delegate_ && event)
    delegate_->OnDragEntered(*event);
}

void DesktopDragDropClientOzone::OnDragLeave() {
  if (delegate_)
    delegate_->OnDragExited();
}

int DesktopDragDropClientOzone::OnDragMotion(const gfx::PointF& point,
                                             uint32_t time,
                                             int operation,
                                             gfx::AcceleratedWidget* widget) {
  point_ = point;
  *widget = UpdateTargetWindowAndDelegate(point_);
  drag_operation_ = operation;

  int client_operation =
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE;

  // If it has valid data in |os_exchange_data_|, it gets operation.
  if (os_exchange_data_) {
    std::unique_ptr<ui::DropTargetEvent> event = CreateDropTargetEvent();
    if (event)
      client_operation = delegate_->OnDragUpdated(*event);
  }
  return client_operation;
}

void DesktopDragDropClientOzone::OnDragDrop(
    std::unique_ptr<ui::OSExchangeData> data) {
  // If it doesn't has |os_exchange_data_|, it needs to update it with |data|.
  if (!os_exchange_data_) {
    DCHECK(data);
    os_exchange_data_ = std::move(data);
    std::unique_ptr<ui::DropTargetEvent> event = CreateDropTargetEvent();
    if (event) {
      delegate_->OnDragEntered(*event);
      delegate_->OnDragUpdated(*event);
    }
  }
  PerformDrop();
}

void DesktopDragDropClientOzone::OnMouseMoved(const gfx::Point& point,
                                              gfx::AcceleratedWidget* widget) {
  DCHECK(widget);

  aura::Window* target_window = GetTargetWindow(root_window_, point);
  if (!target_window) {
    LOG(ERROR) << "Failed to find target_window at " << point.ToString();
    return;
  }

  *widget = target_window->GetHost()->GetAcceleratedWidget();
}

void DesktopDragDropClientOzone::OnDragSessionClosed(int dnd_action) {
  drag_operation_ = dnd_action;
  QuitClosure();
  DragDropSessionCompleted();
}

std::unique_ptr<ui::DropTargetEvent>
DesktopDragDropClientOzone::CreateDropTargetEvent(
    const gfx::PointF& root_location) const {
  if (!target_window_) {
    LOG(ERROR) << "Invalid target window";
    return nullptr;
  }

  gfx::PointF target_location = root_location;
  target_window_->GetHost()->ConvertDIPToPixels(&target_location);
  aura::Window::ConvertPointToTarget(root_window_, target_window_,
                                     &target_location);

  return std::make_unique<ui::DropTargetEvent>(
      *os_exchange_data_, target_location, root_location, drag_operation_);
}

std::unique_ptr<ui::DropTargetEvent>
DesktopDragDropClientOzone::CreateDropTargetEvent() const {
  gfx::Point root_location(point_.x(), point_.y());
  root_window_->GetHost()->ConvertScreenInPixelsToDIP(&root_location);

  return CreateDropTargetEvent(gfx::PointF(root_location));
}

gfx::AcceleratedWidget
DesktopDragDropClientOzone::UpdateTargetWindowAndDelegate(gfx::PointF& pointf) {
  const gfx::Point point(pointf.x(), pointf.y());
  aura::Window* target_window = GetTargetWindow(root_window_, point);
  if (target_window_ != target_window) {
    aura::client::DragDropDelegate* delegate = nullptr;
    if (target_window_) {
      target_window_->RemoveObserver(this);
      delegate_->OnDragExited();
    }
    if (target_window) {
      target_window->AddObserver(this);
      delegate = aura::client::GetDragDropDelegate(target_window);
    }
    target_window_ = target_window;
    delegate_ = delegate;
  }

  return target_window ? target_window->GetHost()->GetAcceleratedWidget()
                       : gfx::kNullAcceleratedWidget;
}

void DesktopDragDropClientOzone::DragDropSessionCompleted() {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window_);
  cursor_client->SetCursor(initial_cursor_);

  os_exchange_data_ = nullptr;
  if (!target_window_)
    return;
  target_window_->RemoveObserver(this);
  target_window_ = nullptr;
}

void DesktopDragDropClientOzone::PerformDrop() {
  DCHECK(delegate_);
  std::unique_ptr<ui::DropTargetEvent> event = CreateDropTargetEvent();
  delegate_->OnPerformDrop(*event);
  DragDropSessionCompleted();
}

void DesktopDragDropClientOzone::QuitClosure() {
  in_move_loop_ = false;
  if (quit_closure_.is_null())
    return;
  std::move(quit_closure_).Run();
}

}  // namespace views
