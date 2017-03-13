// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_OZONE_H_
#define CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_OZONE_H_

#include "chrome/browser/browser_process_platform_part_base.h"

namespace ui {
class ImageCursorsSet;
}

class BrowserProcessPlatformPart : public BrowserProcessPlatformPartBase {
 public:
  BrowserProcessPlatformPart();
  ~BrowserProcessPlatformPart() override;

  // Overridden from BrowserProcessPlatformPartBase:
  void RegisterInProcessServices(
      content::ContentBrowserClient::StaticServiceMap* services) override;

 private:
  // Used by the UI Service.
  std::unique_ptr<ui::ImageCursorsSet> image_cursors_set_;

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessPlatformPart);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_OZONE_H_
