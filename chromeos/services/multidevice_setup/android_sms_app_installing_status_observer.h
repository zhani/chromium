// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_ANDROID_SMS_APP_INSTALLING_STATUS_OBSERVER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_ANDROID_SMS_APP_INSTALLING_STATUS_OBSERVER_H_

#include <memory>

#include "chromeos/services/multidevice_setup/host_status_provider.h"

namespace chromeos {

namespace multidevice_setup {

class AndroidSmsAppHelperDelegate;

// Listens for status changes in multidevice state and installs the Android
// Messages PWA if needed.
//
// TODO(crbug.com/884290): Also observe FeatureStateManager to make sure
// Messages is supported.
class AndroidSmsAppInstallingStatusObserver
    : public HostStatusProvider::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<AndroidSmsAppInstallingStatusObserver>
    BuildInstance(HostStatusProvider* host_status_provider,
                  std::unique_ptr<AndroidSmsAppHelperDelegate>
                      android_sms_app_helper_delegate);

   private:
    static Factory* test_factory_;
  };

  ~AndroidSmsAppInstallingStatusObserver() override;

 private:
  AndroidSmsAppInstallingStatusObserver(
      HostStatusProvider* host_status_provider,
      std::unique_ptr<AndroidSmsAppHelperDelegate>
          android_sms_app_helper_delegate);

  // HostStatusProvider::Observer:
  void OnHostStatusChange(const HostStatusProvider::HostStatusWithDevice&
                              host_status_with_device) override;

  HostStatusProvider* host_status_provider_;
  std::unique_ptr<AndroidSmsAppHelperDelegate> android_sms_app_helper_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsAppInstallingStatusObserver);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_ANDROID_SMS_APP_INSTALLING_STATUS_OBSERVER_H_
