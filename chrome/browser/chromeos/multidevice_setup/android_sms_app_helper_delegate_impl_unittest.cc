// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/multidevice_setup/android_sms_app_helper_delegate_impl.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/web_applications/components/test_pending_app_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

class AndroidSmsAppHelperDelegateImplTest : public testing::Test {
 protected:
  AndroidSmsAppHelperDelegateImplTest() = default;
  ~AndroidSmsAppHelperDelegateImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    test_pending_app_manager_ =
        std::make_unique<web_app::TestPendingAppManager>();
    android_sms_app_helper_delegate_ = base::WrapUnique(
        new AndroidSmsAppHelperDelegateImpl(test_pending_app_manager_.get()));
  }

  web_app::TestPendingAppManager* test_pending_app_manager() {
    return test_pending_app_manager_.get();
  }

  void InstallApp() {
    android_sms_app_helper_delegate_->InstallAndroidSmsApp();
  }

  void InstallAndLaunchApp() {
    android_sms_app_helper_delegate_->InstallAndLaunchAndroidSmsApp();
  }

 private:
  std::unique_ptr<web_app::TestPendingAppManager> test_pending_app_manager_;
  std::unique_ptr<AndroidSmsAppHelperDelegate> android_sms_app_helper_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsAppHelperDelegateImplTest);
};

TEST_F(AndroidSmsAppHelperDelegateImplTest, TestInstallMessagesApp) {
  InstallApp();

  std::vector<web_app::PendingAppManager::AppInfo> expected_apps_to_install;
  expected_apps_to_install.emplace_back(
      chromeos::android_sms::GetAndroidMessagesURLWithExperiments(),
      web_app::PendingAppManager::LaunchContainer::kWindow,
      web_app::PendingAppManager::InstallSource::kInternal,
      web_app::PendingAppManager::AppInfo::kDefaultCreateShortcuts,
      true);  // override_previous_user_uninstall
  EXPECT_EQ(expected_apps_to_install,
            test_pending_app_manager()->install_requests());
}

TEST_F(AndroidSmsAppHelperDelegateImplTest, TestInstallAndLaunchMessagesApp) {
  // TODO(crbug/876972): Figure out how to actually test the launching of the
  // app here.
}

}  // namespace multidevice_setup

}  // namespace chromeos
