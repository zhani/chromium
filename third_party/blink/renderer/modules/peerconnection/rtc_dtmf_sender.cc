/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/peerconnection/rtc_dtmf_sender.h"

#include <memory>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_rtc_dtmf_sender_handler.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_dtmf_tone_change_event.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

static const int kMinToneDurationMs = 40;
static const int kDefaultToneDurationMs = 100;
static const int kMaxToneDurationMs = 6000;
// TODO(hta): Adjust kMinInterToneGapMs to 30 once WebRTC code has changed
// CL in progress: https://webrtc-review.googlesource.com/c/src/+/55260
static const int kMinInterToneGapMs = 50;
static const int kMaxInterToneGapMs = 6000;
static const int kDefaultInterToneGapMs = 70;

RTCDTMFSender* RTCDTMFSender::Create(
    ExecutionContext* context,
    std::unique_ptr<WebRTCDTMFSenderHandler> dtmf_sender_handler) {
  DCHECK(dtmf_sender_handler);
  return new RTCDTMFSender(context, std::move(dtmf_sender_handler));
}

RTCDTMFSender::RTCDTMFSender(ExecutionContext* context,
                             std::unique_ptr<WebRTCDTMFSenderHandler> handler)
    : ContextLifecycleObserver(context),
      handler_(std::move(handler)),
      stopped_(false) {
  handler_->SetClient(this);
}

RTCDTMFSender::~RTCDTMFSender() = default;

void RTCDTMFSender::Dispose() {
  // Promptly clears a raw reference from content/ to an on-heap object
  // so that content/ doesn't access it in a lazy sweeping phase.
  handler_->SetClient(nullptr);
  handler_.reset();
}

bool RTCDTMFSender::canInsertDTMF() const {
  return handler_->CanInsertDTMF();
}

String RTCDTMFSender::toneBuffer() const {
  return tone_buffer_;
}

void RTCDTMFSender::insertDTMF(const String& tones,
                               ExceptionState& exception_state) {
  insertDTMF(tones, kDefaultToneDurationMs, kDefaultInterToneGapMs,
             exception_state);
}

void RTCDTMFSender::insertDTMF(const String& tones,
                               int duration,
                               ExceptionState& exception_state) {
  insertDTMF(tones, duration, kDefaultInterToneGapMs, exception_state);
}

void RTCDTMFSender::insertDTMF(const String& tones,
                               int duration,
                               int inter_tone_gap,
                               ExceptionState& exception_state) {
  // https://w3c.github.io/webrtc-pc/#dom-rtcdtmfsender-insertdtmf
  // TODO(hta): Add check on transceiver's "stopped" and "currentDirection"
  // attributes
  if (!canInsertDTMF()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The 'canInsertDTMF' attribute is false: "
                                      "this sender cannot send DTMF.");
    return;
  }
  // Spec: Throw on illegal characters
  if (strspn(tones.Ascii().data(), "0123456789abcdABCD#*,") != tones.length()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "Illegal characters in InsertDTMF tone argument");
    return;
  }

  // Spec: Clamp the duration to between 40 and 6000 ms
  duration = std::max(duration, kMinToneDurationMs);
  duration = std::min(duration, kMaxToneDurationMs);
  // Spec: Clamp the inter-tone gap to between 30 and 6000 ms
  inter_tone_gap = std::max(inter_tone_gap, kMinInterToneGapMs);
  inter_tone_gap = std::min(inter_tone_gap, kMaxInterToneGapMs);

  // Spec: a-d should be represented in the tone buffer as A-D
  if (!handler_->InsertDTMF(tones.UpperASCII(), duration, inter_tone_gap)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Could not send provided tones, '" + tones + "'.");
    return;
  }
  tone_buffer_ = handler_->CurrentToneBuffer();
}

void RTCDTMFSender::DidPlayTone(const WebString& tone,
                                const WebString& tone_buffer) {
  tone_buffer_ = tone_buffer;
  Member<Event> event = RTCDTMFToneChangeEvent::Create(tone);
  DispatchEvent(*event.Release());
}

const AtomicString& RTCDTMFSender::InterfaceName() const {
  return EventTargetNames::RTCDTMFSender;
}

ExecutionContext* RTCDTMFSender::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

void RTCDTMFSender::ContextDestroyed(ExecutionContext*) {
  stopped_ = true;
  handler_->SetClient(nullptr);
}

void RTCDTMFSender::Trace(blink::Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
