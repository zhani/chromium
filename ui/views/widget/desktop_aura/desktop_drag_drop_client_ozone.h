// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_OZONE_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_OZONE_H_

#include "base/callback.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/window_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

namespace ui {
class PlatformWindow;
}  // namespace ui

namespace views {
class DesktopNativeCursorManager;

class VIEWS_EXPORT DesktopDragDropClientOzone
    : public aura::client::DragDropClient,
      public aura::WindowObserver {
 public:
  explicit DesktopDragDropClientOzone(
      aura::Window* root_window,
      views::DesktopNativeCursorManager* cursor_manager,
      ui::PlatformWindow* platform_window,
      gfx::AcceleratedWidget widget);
  ~DesktopDragDropClientOzone() override;
  // Overridden from aura::client::DragDropClient:
  int StartDragAndDrop(const ui::OSExchangeData& data,
                       aura::Window* root_window,
                       aura::Window* source_window,
                       const gfx::Point& root_location,
                       int operation,
                       ui::DragDropTypes::DragEventSource source) override;
  void DragCancel() override;
  bool IsDragDropInProgress() override;
  void AddObserver(aura::client::DragDropClientObserver* observer) override;
  void RemoveObserver(aura::client::DragDropClientObserver* observer) override;

  // Overridden from void aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

  void OnDragSessionClosed(int operation);

  void OnWindowDestroying(aura::Window* window) override;

  void OnDragEnter(ui::PlatformWindow* window,
                   const gfx::PointF& point,
                   std::unique_ptr<ui::OSExchangeData> data,
                   int operation);
  int OnDragMotion(const gfx::PointF& point,
                   uint32_t time,
                   int operation,
                   gfx::AcceleratedWidget* widget);
  void OnDragDrop(std::unique_ptr<ui::OSExchangeData> data);
  void OnDragLeave();
  void OnMouseMoved(const gfx::Point& point, gfx::AcceleratedWidget* widget);

 private:
  // Returns a DropTargetEvent to be passed to the DragDropDelegate, or null to
  // abort the drag, using the location of point_ if no root location is passed.
  std::unique_ptr<ui::DropTargetEvent> CreateDropTargetEvent(
      const gfx::PointF& point) const;
  std::unique_ptr<ui::DropTargetEvent> CreateDropTargetEvent() const;

  void DragDropSessionCompleted();

  // Update |target_window_| and |delegate_| along with |point|.
  gfx::AcceleratedWidget UpdateTargetWindowAndDelegate(gfx::PointF& point);

  void PerformDrop();

  void QuitClosure();

  aura::Window* root_window_;

  DesktopNativeCursorManager* cursor_manager_;

  ui::PlatformWindow* platform_window_;

  // Null unless all drag data has been received and the drag is unfinished.
  aura::Window* target_window_;

  // This is the interface that allows us to send drag events from Ozone-Wayland
  // to the cross-platform code.
  aura::client::DragDropDelegate* delegate_;

  std::unique_ptr<ui::OSExchangeData> os_exchange_data_;

  // The most recent native coordinates of a drag.
  gfx::PointF point_;

  // Cursor in use prior to the move loop starting. Restored when the move loop
  // quits.
  gfx::NativeCursor initial_cursor_;

  base::OnceClosure quit_closure_;

  // The operation bitfield.
  int drag_operation_;

  //  The flag that controls whether it has a nested run loop.
  bool in_move_loop_ = false;

  DISALLOW_COPY_AND_ASSIGN(DesktopDragDropClientOzone);
};

}  // namespace views
#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_OZONE_H_
