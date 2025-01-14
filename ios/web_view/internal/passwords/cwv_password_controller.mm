// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/cwv_password_controller_internal.h"

#include <memory>

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/ios/browser/autofill_util.h"
#import "components/password_manager/core/browser/form_parsing/ios_form_parser.h"
#include "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/ios/password_controller_helper.h"
#import "ios/web/public/origin_util.h"
#include "ios/web/public/url_scheme_util.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_client.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_driver.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FormData;
using autofill::PasswordForm;
using ios_web_view::WebViewPasswordManagerClient;
using ios_web_view::WebViewPasswordManagerDriver;
using password_manager::GetPageURLAndCheckTrustLevel;

@interface CWVPasswordController ()<CRWWebStateObserver,
                                    CWVPasswordManagerClientDelegate,
                                    CWVPasswordManagerDriverDelegate,
                                    PasswordControllerHelperDelegate>

// The PasswordManagerDriver owned by this PasswordController.
@property(nonatomic, readonly)
    password_manager::PasswordManagerDriver* passwordManagerDriver;

// Helper contains common password controller logic.
@property(nonatomic, readonly) PasswordControllerHelper* helper;

// Informs the |_passwordManager| of the password forms (if any were present)
// that have been found on the page.
- (void)didFinishPasswordFormExtraction:
    (const std::vector<autofill::PasswordForm>&)forms;

// Finds all password forms in DOM and sends them to the password manager for
// further processing.
- (void)findPasswordFormsAndSendThemToPasswordManager;

@end

@implementation CWVPasswordController {
  std::unique_ptr<password_manager::PasswordManager> _passwordManager;
  std::unique_ptr<WebViewPasswordManagerClient> _passwordManagerClient;
  std::unique_ptr<WebViewPasswordManagerDriver> _passwordManagerDriver;

  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* _webState;

  // Bridge to observe WebState from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // True indicates that credentials has been sent to the password manager.
  BOOL _credentialsSentToPasswordManager;

  // Bridge to observe form activity in |webState_|.
  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;

  // TODO(crbug.com/865114): Add suggestion logic.
}

#pragma mark - Properties

@synthesize helper = _helper;

- (password_manager::PasswordManagerDriver*)passwordManagerDriver {
  return _passwordManagerDriver.get();
}

#pragma mark - Initialization

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    DCHECK(webState);
    _webState = webState;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    _helper = [[PasswordControllerHelper alloc] initWithWebState:webState
                                                        delegate:self];

    _passwordManagerClient =
        std::make_unique<WebViewPasswordManagerClient>(self);
    _passwordManager = std::make_unique<password_manager::PasswordManager>(
        _passwordManagerClient.get());
    _passwordManagerDriver =
        std::make_unique<WebViewPasswordManagerDriver>(self);

    _credentialsSentToPasswordManager = NO;

    // TODO(crbug.com/865114): Credential manager related logic
  }
  return self;
}

#pragma mark - Dealloc

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  // Clear per-page state
  // TODO(crbug.com/865114): Clear fillData and suggestion.
  _credentialsSentToPasswordManager = NO;

  // Retrieve the identity of the page. In case the page might be malicous,
  // returns early.
  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(webState, &pageURL)) {
    return;
  }

  if (!web::UrlHasWebScheme(pageURL)) {
    return;
  }

  // Notify the password manager that the page loaded so it can clear its own
  // per-page state.
  _passwordManager->DidNavigateMainFrame();

  if (!webState->ContentIsHTML()) {
    // If the current page is not HTML, it does not contain any HTML forms.
    [self
        didFinishPasswordFormExtraction:std::vector<autofill::PasswordForm>()];
  }

  [self findPasswordFormsAndSendThemToPasswordManager];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
  }
  _passwordManagerDriver.reset();
  _passwordManager.reset();
  _passwordManagerClient.reset();
}

#pragma mark - CWVPasswordManagerClientDelegate

- (ios_web_view::WebViewBrowserState*)browserState {
  return _webState ? ios_web_view::WebViewBrowserState::FromBrowserState(
                         _webState->GetBrowserState())
                   : nullptr;
}

- (password_manager::PasswordManager*)passwordManager {
  return _passwordManager.get();
}

- (const GURL&)lastCommittedURL {
  return self.helper.lastCommittedURL;
}

- (void)showSavePasswordInfoBar:
    (std::unique_ptr<password_manager::PasswordFormManagerForUI>)formToSave {
  // Always save password for Demo.
  // TODO(crbug.com/865114): Implement remaining logic.
  formToSave.get()->Save();
}

- (void)showUpdatePasswordInfoBar:
    (std::unique_ptr<password_manager::PasswordFormManagerForUI>)formToUpdate {
  // TODO(crbug.com/865114): Implement remaining logic.
}

- (void)showAutosigninNotification:
    (std::unique_ptr<autofill::PasswordForm>)formSignedIn {
  // TODO(crbug.com/865114): Implement remaining logic.
}

#pragma mark - CWVPasswordManagerDriverDelegate

- (void)fillPasswordForm:(const autofill::PasswordFormFillData&)formData {
  // TODO(crbug.com/865114): Add suggestion related logic.
  [self.helper fillPasswordForm:formData completionHandler:nil];
}

// Informs delegate that there are no saved credentials for the current page.
- (void)informNoSavedCredentials {
  // TODO(crbug.com/865114): Implement remaining logic.
}

#pragma mark - PasswordControllerHelperDelegate

- (void)helper:(PasswordControllerHelper*)helper
    didSubmitForm:(const PasswordForm&)form
      inMainFrame:(BOOL)inMainFrame {
  if (inMainFrame) {
    self.passwordManager->OnPasswordFormSubmitted(self.passwordManagerDriver,
                                                  form);
  } else {
    // Show a save prompt immediately because for iframes it is very hard to
    // figure out correctness of password forms submission.
    self.passwordManager->OnPasswordFormSubmittedNoChecks(
        self.passwordManagerDriver, form);
  }
}

#pragma mark - Private methods

- (void)didFinishPasswordFormExtraction:
    (const std::vector<autofill::PasswordForm>&)forms {
  // Do nothing if |self| has been detached.
  if (!_passwordManager) {
    return;
  }

  if (!forms.empty()) {
    // TODO(crbug.com/865114):
    // Notify web_state about password forms, so that this can be taken into
    // account for the security state.

    _credentialsSentToPasswordManager = YES;
    // Invoke the password manager callback to autofill password forms
    // on the loaded page.
    _passwordManager->OnPasswordFormsParsed(self.passwordManagerDriver, forms);
  } else {
    [self informNoSavedCredentials];
  }
  // Invoke the password manager callback to check if password was
  // accepted or rejected.
  _passwordManager->OnPasswordFormsRendered(self.passwordManagerDriver, forms,
                                            /*did_stop_loading=*/true);
}

- (void)findPasswordFormsAndSendThemToPasswordManager {
  // Read all password forms from the page and send them to the password
  // manager.
  __weak CWVPasswordController* weakSelf = self;
  [self.helper findPasswordFormsWithCompletionHandler:^(
                   const std::vector<autofill::PasswordForm>& forms) {
    [weakSelf didFinishPasswordFormExtraction:forms];
  }];
}

@end
