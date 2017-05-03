// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/x11_window_ozone.h"

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

#include "base/bind.h"
#include "ui/events/event.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/geometry/point.h"
#include "ui/platform_window/x11/x11_cursor_ozone.h"
#include "ui/platform_window/x11/x11_window_manager_ozone.h"

namespace ui {

namespace {

XID GetEventXWindow(EventWithPlatformEvent* ewpe) {
  auto* xev = static_cast<XEvent*>(ewpe->platform_event);
  XID target = xev->xany.window;
  if (xev->type == GenericEvent)
    target = static_cast<XIDeviceEvent*>(xev->xcookie.data)->event;
  return target;
}

}  // namespace

X11WindowOzone::X11WindowOzone(X11WindowManagerOzone* window_manager,
                               PlatformWindowDelegate* delegate,
                               const gfx::Rect& bounds)
    : X11WindowBase(delegate, bounds), window_manager_(window_manager) {
  DCHECK(window_manager);
  window_manager_->AddX11Window(this);

  auto* event_source = X11EventSourceLibevent::GetInstance();
  if (event_source) {
    event_source->AddPlatformEventDispatcher(this);
    event_source->AddXEventDispatcher(this);
  }
}

X11WindowOzone::~X11WindowOzone() {
  X11WindowOzone::PrepareForShutdown();
}

void X11WindowOzone::PrepareForShutdown() {
  DCHECK(window_manager_);
  window_manager_->DeleteX11Window(this);

  auto* event_source = X11EventSourceLibevent::GetInstance();
  if (event_source) {
    event_source->RemovePlatformEventDispatcher(this);
    event_source->RemoveXEventDispatcher(this);
  }
}

void X11WindowOzone::SetCapture() {
  window_manager_->GrabEvents(this);
}

void X11WindowOzone::ReleaseCapture() {
  window_manager_->UngrabEvents(this);
}

void X11WindowOzone::SetCursor(PlatformCursor cursor) {
  X11CursorOzone* cursor_ozone = static_cast<X11CursorOzone*>(cursor);
  XDefineCursor(xdisplay(), xwindow(), cursor_ozone->xcursor());
}

bool X11WindowOzone::DispatchXEvent(XEvent* xev) {
  if (!IsEventForXWindow(*xev))
    return false;

  ProcessXWindowEvent(xev);
  return true;
}

bool X11WindowOzone::CanDispatchEvent(const PlatformEvent& platform_event) {
  if (xwindow() == None)
    return false;

  // If there is a grab, capture events here.
  X11WindowOzone* grabber = window_manager_->event_grabber();
  if (grabber)
    return grabber == this;

  // TODO(kylechar): We may need to do something special for TouchEvents similar
  // to how DrmWindowHost handles them.
  auto* ewpe = static_cast<EventWithPlatformEvent*>(platform_event);
  auto* xev = static_cast<XEvent*>(ewpe->platform_event);
  DCHECK(xev);
  if (!IsEventForXWindow(*xev))
    return false;

  Event* event = ewpe->event;
  DCHECK(event);
  if (event->IsLocatedEvent())
    return GetBounds().Contains(event->AsLocatedEvent()->root_location());

  return true;
}

uint32_t X11WindowOzone::DispatchEvent(const PlatformEvent& platform_event) {
  // This is unfortunately needed otherwise events that depend on global state
  // (eg. double click) are broken.
  auto* ewpe = static_cast<EventWithPlatformEvent*>(platform_event);
  auto* event = static_cast<ui::Event*>(ewpe->event);

  XID target = GetEventXWindow(ewpe);

  // If the event came originally from another native window (was rerouted by
  // PlatformEventSource), its location must be adapted using current window
  // location.
  if (target != xwindow())
    ConvertEventLocationToCurrentWindowLocation(target, event);

  DispatchEventFromNativeUiEvent(
      event, base::Bind(&PlatformWindowDelegate::DispatchEvent,
                        base::Unretained(delegate())));
  return POST_DISPATCH_STOP_PROPAGATION;
}

void X11WindowOzone::OnLostCapture() {
  delegate()->OnLostCapture();
}

void X11WindowOzone::ConvertEventLocationToCurrentWindowLocation(
    const XID& target,
    ui::Event* event) {
  X11WindowOzone* x11_window = window_manager_->GetX11WindowByTarget(target);
  if (x11_window && x11_window != this && event->IsLocatedEvent()) {
    gfx::Vector2d offset =
        x11_window->GetBounds().origin() - GetBounds().origin();
    ui::LocatedEvent* located_event = event->AsLocatedEvent();
    gfx::PointF location_in_pixel_in_host =
        located_event->location_f() + gfx::Vector2dF(offset);
    located_event->set_location_f(location_in_pixel_in_host);
    located_event->set_root_location_f(location_in_pixel_in_host);
  }
}

}  // namespace ui
