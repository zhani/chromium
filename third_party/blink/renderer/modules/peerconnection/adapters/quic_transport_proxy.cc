// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_transport_proxy.h"

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_host.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_transport_host.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

namespace blink {

QuicTransportProxy::QuicTransportProxy(
    Delegate* delegate,
    IceTransportProxy* ice_transport_proxy,
    quic::Perspective perspective,
    const std::vector<rtc::scoped_refptr<rtc::RTCCertificate>>& certificates)
    : host_(nullptr,
            base::OnTaskRunnerDeleter(ice_transport_proxy->host_thread())),
      delegate_(delegate),
      ice_transport_proxy_(ice_transport_proxy),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
  DCHECK(ice_transport_proxy_);
  scoped_refptr<base::SingleThreadTaskRunner> proxy_thread =
      ice_transport_proxy->proxy_thread();
  DCHECK(proxy_thread->BelongsToCurrentThread());
  // Wait to initialize the host until the weak_ptr_factory_ is initialized.
  // The QuicTransportHost is constructed on the proxy thread but should only be
  // interacted with via PostTask to the host thread. The OnTaskRunnerDeleter
  // (configured above) will ensure it gets deleted on the host thread.
  host_.reset(
      new QuicTransportHost(proxy_thread, weak_ptr_factory_.GetWeakPtr()));
  // Connect to the IceTransportProxy. This gives us a reference to the
  // underlying IceTransportHost that should be connected by the
  // QuicTransportHost on the host thread. It is safe to post it unretained
  // since the IceTransportHost's ownership is determined by the
  // IceTransportProxy, and the IceTransportProxy is required to outlive this
  // object.
  IceTransportHost* ice_transport_host =
      ice_transport_proxy->ConnectConsumer(this);
  PostCrossThreadTask(
      *host_thread(), FROM_HERE,
      CrossThreadBind(&QuicTransportHost::Initialize,
                      CrossThreadUnretained(host_.get()),
                      CrossThreadUnretained(ice_transport_host), host_thread(),
                      perspective, certificates));
}

QuicTransportProxy::~QuicTransportProxy() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ice_transport_proxy_->DisconnectConsumer(this);
  // Note: The QuicTransportHost will be deleted on the host thread.
}

scoped_refptr<base::SingleThreadTaskRunner> QuicTransportProxy::proxy_thread()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return ice_transport_proxy_->proxy_thread();
}

scoped_refptr<base::SingleThreadTaskRunner> QuicTransportProxy::host_thread()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return ice_transport_proxy_->host_thread();
}

void QuicTransportProxy::Start(
    std::vector<std::unique_ptr<rtc::SSLFingerprint>> remote_fingerprints) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *host_thread(), FROM_HERE,
      CrossThreadBind(&QuicTransportHost::Start,
                      CrossThreadUnretained(host_.get()),
                      WTF::Passed(std::move(remote_fingerprints))));
}

void QuicTransportProxy::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(*host_thread(), FROM_HERE,
                      CrossThreadBind(&QuicTransportHost::Stop,
                                      CrossThreadUnretained(host_.get())));
}

void QuicTransportProxy::OnConnected() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnConnected();
}

void QuicTransportProxy::OnRemoteStopped() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnRemoteStopped();
}

void QuicTransportProxy::OnConnectionFailed(const std::string& error_details,
                                            bool from_remote) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnConnectionFailed(error_details, from_remote);
}

}  // namespace blink
