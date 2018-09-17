// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_DELEGATE_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_DELEGATE_H_

#include "base/callback.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Rect;
class PointF;
}

namespace ui {

class Event;
class PlatformWindow;
class OSExchangeData;

enum PlatformWindowState {
  PLATFORM_WINDOW_STATE_UNKNOWN,
  PLATFORM_WINDOW_STATE_MAXIMIZED,
  PLATFORM_WINDOW_STATE_MINIMIZED,
  PLATFORM_WINDOW_STATE_NORMAL,
  PLATFORM_WINDOW_STATE_FULLSCREEN,
};

class PlatformWindowDelegate {
 public:
  virtual ~PlatformWindowDelegate() {}

  // Note that |new_bounds| is in physical screen coordinates.
  virtual void OnBoundsChanged(const gfx::Rect& new_bounds) = 0;

  // Note that |damaged_region| is in the platform-window's coordinates, in
  // physical pixels.
  virtual void OnDamageRect(const gfx::Rect& damaged_region) = 0;

  virtual void DispatchEvent(Event* event) = 0;

  virtual void OnCloseRequest() = 0;
  virtual void OnClosed() = 0;

  virtual void OnWindowStateChanged(PlatformWindowState new_state) = 0;

  virtual void OnLostCapture() = 0;

  virtual void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) = 0;

  // Notifies the delegate that the widget cannot be used anymore until
  // a new widget is made available through OnAcceleratedWidgetAvailable().
  // Must not be called when the PlatformWindow is being destroyed.
  virtual void OnAcceleratedWidgetDestroyed() = 0;

  virtual void OnActivationChanged(bool active) = 0;

  // Notifies the delegate that Drag and Drop is completed or canceled and the
  // session is finished. If Drag and Drop is completed, |operation| has the
  // result operation.
  virtual void OnDragSessionClosed(int operation) = 0;

  // TODO(jkim): Make them pure virtual functions.
  // Notifies the delegate that dragging is entered to |window|.
  virtual void OnDragEnter(ui::PlatformWindow* window,
                           const gfx::PointF& point,
                           std::unique_ptr<OSExchangeData> data,
                           int operation) {}

  // Notifies the delegate that dragging is moved. |widget| will be set with the
  // widget located at |point|. It returns the operation selected by client.
  virtual int OnDragMotion(const gfx::PointF& point,
                           uint32_t time,
                           int operation,
                           gfx::AcceleratedWidget* widget) = 0;

  // Notifies the delegate that dragged data is dropped. When it doesn't deliver
  // the dragged data on OnDragEnter, it should put it to |data|.
  virtual void OnDragDrop(std::unique_ptr<ui::OSExchangeData> data) {}

  // Notifies the delegate that dragging is left.
  virtual void OnDragLeave() {}

  // Notifies the delegate that mouse is moved generally and expects that
  // |widget| is filled with the widget located at |point|.
  virtual void OnMouseMoved(const gfx::Point& point,
                            gfx::AcceleratedWidget* widget) {}

};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_DELEGATE_H_
