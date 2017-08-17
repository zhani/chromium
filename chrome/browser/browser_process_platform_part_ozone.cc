// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_ozone.h"

#include "chrome/browser/embedded_ui_service_info_factory.h"
#include "services/ui/common/image_cursors_set.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/service.h"

BrowserProcessPlatformPart::BrowserProcessPlatformPart() = default;

BrowserProcessPlatformPart::~BrowserProcessPlatformPart() = default;

void BrowserProcessPlatformPart::RegisterInProcessServices(
    content::ContentBrowserClient::StaticServiceMap* services) {
  image_cursors_set_ = base::MakeUnique<ui::ImageCursorsSet>();
  service_manager::EmbeddedServiceInfo info =
      CreateEmbeddedUIServiceInfo(image_cursors_set_->GetWeakPtr());
  services->insert(std::make_pair(ui::mojom::kServiceName, info));
}
