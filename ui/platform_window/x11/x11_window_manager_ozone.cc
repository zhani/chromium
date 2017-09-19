// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/x11_window_manager_ozone.h"

#include <X11/Xlib.h>

#include "ui/base/x/x11_pointer_grab.h"

namespace ui {

X11WindowManagerOzone::X11WindowManagerOzone() : event_grabber_(nullptr) {}

X11WindowManagerOzone::~X11WindowManagerOzone() {
  DCHECK(x11_windows_.empty());
}

void X11WindowManagerOzone::GrabEvents(X11WindowOzone* window) {
  if (event_grabber_ == window)
    return;

  X11WindowOzone* old_grabber = event_grabber_;
  if (old_grabber)
    old_grabber->OnLostCapture();

  event_grabber_ = window;
}

void X11WindowManagerOzone::UngrabEvents(X11WindowOzone* window) {
  if (event_grabber_ != window)
    return;
  event_grabber_->OnLostCapture();
  event_grabber_ = nullptr;
}

void X11WindowManagerOzone::AddX11Window(X11WindowOzone* window) {
  auto it = std::find(x11_windows_.begin(), x11_windows_.end(), window);
  DCHECK(it == x11_windows_.end());
  x11_windows_.push_back(window);
}

void X11WindowManagerOzone::DeleteX11Window(X11WindowOzone* window) {
  auto it = std::find(x11_windows_.begin(), x11_windows_.end(), window);
  if (it != x11_windows_.end())
    x11_windows_.erase(it);
}

X11WindowOzone* X11WindowManagerOzone::GetX11WindowByTarget(
    const XID& xwindow) {
  for (auto* x11_window : x11_windows_) {
    if (x11_window->xwindow() == xwindow)
      return x11_window;
  }
  return nullptr;
}

}  // namespace ui
