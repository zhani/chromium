// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/test/mock_platform_window_delegate.h"

namespace ui {

MockPlatformWindowDelegate::MockPlatformWindowDelegate() {}

MockPlatformWindowDelegate::~MockPlatformWindowDelegate() {}

int MockPlatformWindowDelegate::OnDragMotion(const gfx::PointF& point,
                                             uint32_t time,
                                             int operation,
                                             gfx::AcceleratedWidget* widget) {
  return 0;
}

}  // namespace ui
