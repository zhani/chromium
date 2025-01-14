// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_model_type_controller.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system.h"

namespace browser_sync {

ExtensionModelTypeController::ExtensionModelTypeController(
    syncer::ModelType type,
    syncer::OnceModelTypeStoreFactory store_factory,
    SyncableServiceProvider syncable_service_provider,
    Profile* profile)
    : SyncableServiceBasedModelTypeController(
          type,
          std::move(store_factory),
          std::move(syncable_service_provider)),
      profile_(profile) {
  DCHECK(type == syncer::EXTENSIONS || type == syncer::APPS ||
         type == syncer::THEMES);
}

ExtensionModelTypeController::~ExtensionModelTypeController() {}

void ExtensionModelTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  extensions::ExtensionSystem::Get(profile_)->InitForRegularProfile(
      /*extensions_enabled=*/true);
  ModelTypeController::LoadModels(configure_context, model_load_callback);
}

}  // namespace browser_sync
