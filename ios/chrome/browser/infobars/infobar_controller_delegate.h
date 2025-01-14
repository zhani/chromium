// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

// Interface for delegating events from infobar.
class InfoBarControllerDelegate {
 public:
  // Notifies that the target size has been changed (e.g. after rotation).
  virtual void SetInfoBarTargetHeight(int height) = 0;

  // Returns whether the infobar is owned.
  virtual bool IsOwned() = 0;

  // Notifies that the infobar must be removed.
  virtual void RemoveInfoBar() = 0;

 protected:
  virtual ~InfoBarControllerDelegate() {}
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTROLLER_DELEGATE_H_
