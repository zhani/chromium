// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_TEST_MOCK_PLATFORM_WINDOW_DELEGATE_H_
#define UI_OZONE_TEST_MOCK_PLATFORM_WINDOW_DELEGATE_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

class MockPlatformWindowDelegate : public PlatformWindowDelegate {
 public:
  MockPlatformWindowDelegate();
  ~MockPlatformWindowDelegate();

  MOCK_METHOD1(OnBoundsChanged, void(const gfx::Rect& new_bounds));
  MOCK_METHOD1(OnDamageRect, void(const gfx::Rect& damaged_region));
  MOCK_METHOD1(DispatchEvent, void(Event* event));
  MOCK_METHOD0(OnCloseRequest, void());
  MOCK_METHOD0(OnClosed, void());
  MOCK_METHOD1(OnWindowStateChanged, void(PlatformWindowState new_state));
  MOCK_METHOD0(OnLostCapture, void());
  MOCK_METHOD1(OnAcceleratedWidgetAvailable,
               void(gfx::AcceleratedWidget widget));
  MOCK_METHOD0(OnAcceleratedWidgetDestroyed, void());
  MOCK_METHOD1(OnActivationChanged, void(bool active));
  MOCK_METHOD1(OnDragSessionClosed, void(int operation));

  void OnDragEnter(ui::PlatformWindow* window,
                   const gfx::PointF& point,
                   std::unique_ptr<OSExchangeData> data,
                   int operation) override {}

  int OnDragMotion(const gfx::PointF& point,
                   uint32_t time,
                   int operation,
                   gfx::AcceleratedWidget* widget) override;
  MOCK_METHOD1(OnDragDrop, void(std::unique_ptr<OSExchangeData> data));
  MOCK_METHOD0(OnDragLeave, void());
  MOCK_METHOD2(OnMouseMoved,
               void(const gfx::Point& point, gfx::AcceleratedWidget* widget));
  MOCK_METHOD1(OnDragSessionClose, void(int operation));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPlatformWindowDelegate);
};

}  // namespace ui

#endif  // UI_OZONE_TEST_MOCK_PLATFORM_WINDOW_DELEGATE_H_
