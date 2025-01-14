// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_transport.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_certificate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_stream.h"

namespace blink {

RTCQuicTransport* RTCQuicTransport::Create(
    ExecutionContext* context,
    RTCIceTransport* transport,
    const HeapVector<Member<RTCCertificate>>& certificates,
    ExceptionState& exception_state) {
  if (transport->IsClosed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot construct an RTCQuicTransport with a closed RTCIceTransport.");
    return nullptr;
  }
  if (transport->HasConsumer()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot construct an RTCQuicTransport "
                                      "with an RTCIceTransport that already "
                                      "has a connected RTCQuicTransport.");
    return nullptr;
  }
  for (const auto& certificate : certificates) {
    if (certificate->expires() < ConvertSecondsToDOMTimeStamp(CurrentTime())) {
      exception_state.ThrowTypeError(
          "Cannot construct an RTCQuicTransport with an expired certificate.");
      return nullptr;
    }
  }
  return new RTCQuicTransport(context, transport, certificates,
                              exception_state);
}

RTCQuicTransport::RTCQuicTransport(
    ExecutionContext* context,
    RTCIceTransport* transport,
    const HeapVector<Member<RTCCertificate>>& certificates,
    ExceptionState& exception_state)
    : ContextLifecycleObserver(context),
      transport_(transport),
      certificates_(certificates) {
  transport->ConnectConsumer(this);
}

RTCQuicTransport::~RTCQuicTransport() {
  DCHECK(!proxy_);
}

void RTCQuicTransport::Close(RTCQuicTransportState new_state) {
  DCHECK(!IsClosed());
  for (RTCQuicStream* stream : streams_) {
    stream->Stop();
  }
  streams_.clear();
  transport_->DisconnectConsumer(this);
  proxy_.reset();
  state_ = new_state;
  DCHECK(IsClosed());
}

RTCIceTransport* RTCQuicTransport::transport() const {
  return transport_;
}

String RTCQuicTransport::state() const {
  switch (state_) {
    case RTCQuicTransportState::kNew:
      return "new";
    case RTCQuicTransportState::kConnecting:
      return "connecting";
    case RTCQuicTransportState::kConnected:
      return "connected";
    case RTCQuicTransportState::kClosed:
      return "closed";
    case RTCQuicTransportState::kFailed:
      return "failed";
  }
  return String();
}

void RTCQuicTransport::getLocalParameters(RTCQuicParameters& result) const {
  HeapVector<RTCDtlsFingerprint> fingerprints;
  for (const auto& certificate : certificates_) {
    // TODO(github.com/w3c/webrtc-quic/issues/33): The specification says that
    // getLocalParameters should return one fingerprint per certificate but is
    // not clear which one to pick if an RTCCertificate has multiple
    // fingerprints.
    for (const auto& certificate_fingerprint : certificate->getFingerprints()) {
      fingerprints.push_back(certificate_fingerprint);
    }
  }
  result.setFingerprints(fingerprints);
}

void RTCQuicTransport::getRemoteParameters(
    base::Optional<RTCQuicParameters>& result) const {
  result = remote_parameters_;
}

const HeapVector<Member<RTCCertificate>>& RTCQuicTransport::getCertificates()
    const {
  return certificates_;
}

const HeapVector<Member<DOMArrayBuffer>>&
RTCQuicTransport::getRemoteCertificates() const {
  return remote_certificates_;
}

static quic::Perspective QuicPerspectiveFromIceRole(cricket::IceRole ice_role) {
  switch (ice_role) {
    case cricket::ICEROLE_CONTROLLED:
      return quic::Perspective::IS_CLIENT;
    case cricket::ICEROLE_CONTROLLING:
      return quic::Perspective::IS_SERVER;
    default:
      NOTREACHED();
  }
  return quic::Perspective::IS_CLIENT;
}

void RTCQuicTransport::start(const RTCQuicParameters& remote_parameters,
                             ExceptionState& exception_state) {
  if (RaiseExceptionIfClosed(exception_state)) {
    return;
  }
  if (remote_parameters_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot start() multiple times.");
    return;
  }
  remote_parameters_ = remote_parameters;
  if (transport_->IsStarted()) {
    StartConnection();
  }
}

static std::unique_ptr<rtc::SSLFingerprint> RTCDtlsFingerprintToSSLFingerprint(
    const RTCDtlsFingerprint& dtls_fingerprint) {
  std::string algorithm = WebString(dtls_fingerprint.algorithm()).Utf8();
  std::string value = WebString(dtls_fingerprint.value()).Utf8();
  std::unique_ptr<rtc::SSLFingerprint> rtc_fingerprint(
      rtc::SSLFingerprint::CreateFromRfc4572(algorithm, value));
  DCHECK(rtc_fingerprint);
  return rtc_fingerprint;
}

void RTCQuicTransport::StartConnection() {
  DCHECK_EQ(state_, RTCQuicTransportState::kNew);
  DCHECK(remote_parameters_);

  state_ = RTCQuicTransportState::kConnecting;

  std::vector<rtc::scoped_refptr<rtc::RTCCertificate>> rtc_certificates;
  for (const auto& certificate : certificates_) {
    rtc_certificates.push_back(certificate->Certificate());
  }
  IceTransportProxy* transport_proxy = transport_->ConnectConsumer(this);
  proxy_.reset(new QuicTransportProxy(
      this, transport_proxy, QuicPerspectiveFromIceRole(transport_->GetRole()),
      rtc_certificates));

  std::vector<std::unique_ptr<rtc::SSLFingerprint>> rtc_fingerprints;
  for (const RTCDtlsFingerprint& fingerprint :
       remote_parameters_->fingerprints()) {
    rtc_fingerprints.push_back(RTCDtlsFingerprintToSSLFingerprint(fingerprint));
  }
  proxy_->Start(std::move(rtc_fingerprints));
}

void RTCQuicTransport::OnTransportStarted() {
  // The RTCIceTransport has now been started.
  if (remote_parameters_) {
    StartConnection();
  }
}

void RTCQuicTransport::stop() {
  if (IsClosed()) {
    return;
  }
  if (proxy_) {
    proxy_->Stop();
  }
  Close(RTCQuicTransportState::kClosed);
}

RTCQuicStream* RTCQuicTransport::createStream(ExceptionState& exception_state) {
  if (RaiseExceptionIfClosed(exception_state)) {
    return nullptr;
  }
  RTCQuicStream* stream = new RTCQuicStream(this);
  streams_.insert(stream);
  return stream;
}

void RTCQuicTransport::OnConnected() {
  state_ = RTCQuicTransportState::kConnected;
  DispatchEvent(*Event::Create(EventTypeNames::statechange));
}

void RTCQuicTransport::OnConnectionFailed(const std::string& error_details,
                                          bool from_remote) {
  Close(RTCQuicTransportState::kFailed);
  DispatchEvent(*Event::Create(EventTypeNames::statechange));
}

void RTCQuicTransport::OnRemoteStopped() {
  Close(RTCQuicTransportState::kClosed);
  DispatchEvent(*Event::Create(EventTypeNames::statechange));
}

bool RTCQuicTransport::RaiseExceptionIfClosed(
    ExceptionState& exception_state) const {
  if (IsClosed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The RTCQuicTransport's state is 'closed'.");
    return true;
  }
  return false;
}

const AtomicString& RTCQuicTransport::InterfaceName() const {
  return EventTargetNames::RTCQuicTransport;
}

ExecutionContext* RTCQuicTransport::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

void RTCQuicTransport::ContextDestroyed(ExecutionContext*) {
  stop();
}

bool RTCQuicTransport::HasPendingActivity() const {
  return static_cast<bool>(proxy_);
}

void RTCQuicTransport::Trace(blink::Visitor* visitor) {
  visitor->Trace(transport_);
  visitor->Trace(certificates_);
  visitor->Trace(remote_certificates_);
  visitor->Trace(remote_parameters_);
  visitor->Trace(streams_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
