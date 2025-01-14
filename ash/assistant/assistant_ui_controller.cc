// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_ui_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_screen_context_controller.h"
#include "ash/assistant/ui/assistant_container_view.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_data.h"
#include "ash/system/toast/toast_manager.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "base/optional.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// When hidden, Assistant will automatically close after |kAutoCloseThreshold|.
constexpr base::TimeDelta kAutoCloseThreshold = base::TimeDelta::FromMinutes(5);

// Toast -----------------------------------------------------------------------

constexpr int kToastDurationMs = 2500;
constexpr char kUnboundServiceToastId[] =
    "assistant_controller_unbound_service";

void ShowToast(const std::string& id, int message_id) {
  ToastData toast(id, l10n_util::GetStringUTF16(message_id), kToastDurationMs,
                  base::nullopt);
  Shell::Get()->toast_manager()->Show(toast);
}

}  // namespace

// AssistantUiController -------------------------------------------------------

AssistantUiController::AssistantUiController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller), weak_factory_(this) {
  AddModelObserver(this);
  assistant_controller_->AddObserver(this);
  Shell::Get()->highlighter_controller()->AddObserver(this);
}

AssistantUiController::~AssistantUiController() {
  Shell::Get()->highlighter_controller()->RemoveObserver(this);
  assistant_controller_->RemoveObserver(this);
  RemoveModelObserver(this);
}

void AssistantUiController::SetAssistant(
    chromeos::assistant::mojom::Assistant* assistant) {
  assistant_ = assistant;
}

void AssistantUiController::AddModelObserver(
    AssistantUiModelObserver* observer) {
  model_.AddObserver(observer);
}

void AssistantUiController::RemoveModelObserver(
    AssistantUiModelObserver* observer) {
  model_.RemoveObserver(observer);
}

void AssistantUiController::OnWidgetActivationChanged(views::Widget* widget,
                                                      bool active) {
  if (active) {
    container_view_->RequestFocus();
  } else {
    // When the widget is deactivated the UI should hide. Interacting with
    // the metalayer does not cause widget deactivation.
    HideUi(AssistantSource::kUnspecified);
  }
}

void AssistantUiController::OnWidgetVisibilityChanged(views::Widget* widget,
                                                      bool visible) {
  UpdateUiMode();
}

void AssistantUiController::OnWidgetDestroying(views::Widget* widget) {
  // We need to update the model when the widget is destroyed as this may have
  // happened outside our control. This can occur as the result of pressing the
  // ESC key, for example.
  model_.SetVisibility(AssistantVisibility::kClosed,
                       AssistantSource::kUnspecified);

  ResetContainerView();
}

void AssistantUiController::OnInputModalityChanged(
    InputModality input_modality) {
  UpdateUiMode();
}

void AssistantUiController::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state != InteractionState::kActive)
    return;

  // If there is an active interaction, we need to show Assistant UI if it is
  // not already showing. We don't have enough information here to know what
  // the interaction source is, but at the moment we have no need to know.
  ShowUi(AssistantSource::kUnspecified);
}

void AssistantUiController::OnMicStateChanged(MicState mic_state) {
  // When the mic is opened we update the UI mode to ensure that the user is
  // being presented with the main stage. When closing the mic it is appropriate
  // to stay in whatever UI mode we are currently in.
  if (mic_state == MicState::kOpen)
    UpdateUiMode();
}

void AssistantUiController::OnScreenContextRequestStateChanged(
    ScreenContextRequestState request_state) {
  if (model_.visibility() != AssistantVisibility::kVisible)
    return;

  // Once screen context request state has become idle, it is safe to activate
  // the Assistant widget without causing complications.
  if (request_state == ScreenContextRequestState::kIdle)
    container_view_->GetWidget()->Activate();
}

void AssistantUiController::OnAssistantMiniViewPressed() {
  InputModality input_modality = assistant_controller_->interaction_controller()
                                     ->model()
                                     ->input_modality();

  // When not using stylus input modality, pressing the Assistant mini view
  // will cause the UI to expand.
  if (input_modality != InputModality::kStylus)
    UpdateUiMode(AssistantUiMode::kMainUi);
}

bool AssistantUiController::OnCaptionButtonPressed(CaptionButtonId id) {
  switch (id) {
    case CaptionButtonId::kBack:
      UpdateUiMode(AssistantUiMode::kMainUi);
      return true;
    case CaptionButtonId::kClose:
      return false;
    case CaptionButtonId::kMinimize:
      UpdateUiMode(AssistantUiMode::kMiniUi);
      return true;
  }
  return false;
}

// TODO(dmblack): This event doesn't need to be handled here anymore. Move it
// out of AssistantUiController.
void AssistantUiController::OnDialogPlateButtonPressed(DialogPlateButtonId id) {
  if (id != DialogPlateButtonId::kSettings)
    return;

  // Launch Assistant Settings via deep link.
  assistant_controller_->OpenUrl(
      assistant::util::CreateAssistantSettingsDeepLink());
}

void AssistantUiController::OnHighlighterEnabledChanged(
    HighlighterEnabledState state) {
  switch (state) {
    case HighlighterEnabledState::kEnabled:
      if (model_.visibility() != AssistantVisibility::kVisible)
        ShowUi(AssistantSource::kStylus);
      break;
    case HighlighterEnabledState::kDisabledByUser:
      if (model_.visibility() == AssistantVisibility::kVisible)
        HideUi(AssistantSource::kStylus);
      break;
    case HighlighterEnabledState::kDisabledBySessionComplete:
    case HighlighterEnabledState::kDisabledBySessionAbort:
      // No action necessary.
      break;
  }
}

void AssistantUiController::OnAssistantControllerConstructed() {
  assistant_controller_->interaction_controller()->AddModelObserver(this);
  assistant_controller_->screen_context_controller()->AddModelObserver(this);
}

void AssistantUiController::OnAssistantControllerDestroying() {
  assistant_controller_->screen_context_controller()->RemoveModelObserver(this);
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);

  if (container_view_) {
    // Our view hierarchy should not outlive our controllers.
    container_view_->GetWidget()->CloseNow();
    DCHECK_EQ(nullptr, container_view_);
  }
}

void AssistantUiController::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  if (!assistant::util::IsWebDeepLinkType(type))
    return;

  ShowUi(AssistantSource::kDeepLink);
  UpdateUiMode(AssistantUiMode::kWebUi);
}

void AssistantUiController::OnUrlOpened(const GURL& url) {
  // We hide Assistant UI when opening a URL in a new tab.
  if (model_.visibility() == AssistantVisibility::kVisible)
    HideUi(AssistantSource::kUnspecified);
}

void AssistantUiController::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    AssistantSource source) {
  Shell::Get()->voice_interaction_controller()->NotifyStatusChanged(
      new_visibility == AssistantVisibility::kVisible
          ? mojom::VoiceInteractionState::RUNNING
          : mojom::VoiceInteractionState::STOPPED);

  if (new_visibility == AssistantVisibility::kHidden) {
    // When hiding the UI, start a timer to automatically close ourselves after
    // |kAutoCloseThreshold|. This is to give the user an opportunity to resume
    // their previous session before it is automatically finished.
    auto_close_timer_.Start(FROM_HERE, kAutoCloseThreshold,
                            base::BindRepeating(&AssistantUiController::CloseUi,
                                                weak_factory_.GetWeakPtr(),
                                                AssistantSource::kUnspecified));
  } else {
    auto_close_timer_.Stop();
  }

  // Metalayer should not be sticky. Disable when the UI is no longer visible.
  if (old_visibility == AssistantVisibility::kVisible)
    Shell::Get()->highlighter_controller()->AbortSession();
}

void AssistantUiController::ShowUi(AssistantSource source) {
  if (!Shell::Get()->voice_interaction_controller()->settings_enabled())
    return;

  // TODO(dmblack): Show a more helpful message to the user.
  if (Shell::Get()->voice_interaction_controller()->voice_interaction_state() ==
      mojom::VoiceInteractionState::NOT_READY) {
    ShowToast(kUnboundServiceToastId, IDS_ASH_ASSISTANT_ERROR_GENERIC);
    return;
  }

  if (!assistant_) {
    ShowToast(kUnboundServiceToastId, IDS_ASH_ASSISTANT_ERROR_GENERIC);
    return;
  }

  if (model_.visibility() == AssistantVisibility::kVisible) {
    // If Assistant window is already visible, we just try to retake focus.
    container_view_->GetWidget()->Activate();
    return;
  }

  if (!container_view_)
    CreateContainerView();

  // Note that we initially show the Assistant widget as inactive. This is
  // necessary due to limitations imposed by retrieving screen context. Once we
  // have finished retrieving screen context, the Assistant widget is activated.
  container_view_->GetWidget()->ShowInactive();
  model_.SetVisibility(AssistantVisibility::kVisible, source);
}

void AssistantUiController::HideUi(AssistantSource source) {
  if (model_.visibility() == AssistantVisibility::kHidden)
    return;

  if (container_view_)
    container_view_->GetWidget()->Hide();

  model_.SetVisibility(AssistantVisibility::kHidden, source);
}

void AssistantUiController::CloseUi(AssistantSource source) {
  if (model_.visibility() == AssistantVisibility::kClosed)
    return;

  model_.SetVisibility(AssistantVisibility::kClosed, source);

  if (container_view_) {
    container_view_->GetWidget()->CloseNow();
    DCHECK_EQ(nullptr, container_view_);
  }
}

void AssistantUiController::ToggleUi(AssistantSource source) {
  // When not visible, toggling will show the UI.
  if (model_.visibility() != AssistantVisibility::kVisible) {
    ShowUi(source);
    return;
  }

  // When in mini state, toggling will restore the main UI.
  if (model_.ui_mode() == AssistantUiMode::kMiniUi) {
    UpdateUiMode(AssistantUiMode::kMainUi);
    return;
  }

  // In all other cases, toggling closes the UI.
  CloseUi(source);
}

void AssistantUiController::UpdateUiMode(
    base::Optional<AssistantUiMode> ui_mode) {
  // If a UI mode is provided, we will use it in lieu of updating UI mode on the
  // basis of interaction/widget visibility state.
  if (ui_mode.has_value()) {
    model_.SetUiMode(ui_mode.value());
    return;
  }

  InputModality input_modality = assistant_controller_->interaction_controller()
                                     ->model()
                                     ->input_modality();

  // When stylus input modality is selected, we should be in mini UI mode.
  // Otherwise we fall back to main UI mode.
  model_.SetUiMode(input_modality == InputModality::kStylus
                       ? AssistantUiMode::kMiniUi
                       : AssistantUiMode::kMainUi);
}

void AssistantUiController::OnKeyboardWorkspaceOccludedBoundsChanged(
    const gfx::Rect& new_bounds) {
  DCHECK(container_view_);

  // Check the display for root window and where the keyboard shows to handle
  // the case when there are multiple monitors and the virtual keyboard is shown
  // on a different display other than Assistant UI.
  aura::Window* root_window =
      container_view_->GetWidget()->GetNativeWindow()->GetRootWindow();
  display::Display keyboard_display =
      display::Screen::GetScreen()->GetDisplayMatching(new_bounds);
  if (!new_bounds.IsEmpty() &&
      root_window !=
          Shell::Get()->GetRootWindowForDisplayId(keyboard_display.id())) {
    return;
  }

  // Cache the keyboard workspace occluded bounds.
  keyboard_workspace_occluded_bounds_ = new_bounds;

  // This keyboard event handles the Assistant UI change when:
  // 1. accessibility keyboard or normal virtual keyboard pops up or
  // dismisses. 2. display metrics change (zoom in/out or rotation) when
  // keyboard shows.
  UpdateUsableWorkArea(root_window);
}

void AssistantUiController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  DCHECK(container_view_);

  // Disable this display event when virtual keyboard shows for solving the
  // inconsistency between normal virtual keyboard and accessibility keyboard in
  // changing the work area (accessibility keyboard will change the display work
  // area but virtual keyboard won't). Display metrics change with keyboard
  // showing is instead handled by OnKeyboardWorkspaceOccludedBoundsChanged.
  if (keyboard_workspace_occluded_bounds_.IsEmpty()) {
    aura::Window* root_window =
        container_view_->GetWidget()->GetNativeWindow()->GetRootWindow();
    if (root_window == Shell::Get()->GetRootWindowForDisplayId(display.id())) {
      UpdateUsableWorkArea(root_window);
    }
  }
}

void AssistantUiController::UpdateUsableWorkArea(aura::Window* root_window) {
  gfx::Rect usable_work_area;
  gfx::Rect screen_bounds = root_window->GetBoundsInScreen();

  if (keyboard_workspace_occluded_bounds_.height() != 0) {
    // When keyboard shows. Unlike accessibility keyboard, normal virtual
    // keyboard won't change the display work area, so the new usable work
    // area needs to be calculated manually by subtracting the keyboard
    // occluded bounds from the screen bounds.
    usable_work_area = gfx::Rect(
        screen_bounds.x(), screen_bounds.y(), screen_bounds.width(),
        screen_bounds.height() - keyboard_workspace_occluded_bounds_.height());
  } else {
    // When keyboard hides, the new usable display work area is the same
    // as the whole display work area for the root window.
    display::Display display =
        display::Screen::GetScreen()->GetDisplayMatching(screen_bounds);
    usable_work_area = display.work_area();
  }

  usable_work_area.Inset(kMarginDip, kMarginDip);
  model_.SetUsableWorkArea(usable_work_area);
}

AssistantContainerView* AssistantUiController::GetViewForTest() {
  return container_view_;
}

void AssistantUiController::CreateContainerView() {
  container_view_ = new AssistantContainerView(assistant_controller_);
  container_view_->GetWidget()->AddObserver(this);

  // To save resources, only watch these events while Assistant UI exists.
  display::Screen::GetScreen()->AddObserver(this);
  keyboard::KeyboardController::Get()->AddObserver(this);

  // Retrieve the current keyboard occluded bounds.
  keyboard_workspace_occluded_bounds_ =
      keyboard::KeyboardController::Get()->GetWorkspaceOccludedBounds();

  // Set the initial usable work area for Assistant views.
  aura::Window* root_window =
      container_view_->GetWidget()->GetNativeWindow()->GetRootWindow();
  UpdateUsableWorkArea(root_window);
}

void AssistantUiController::ResetContainerView() {
  // Remove observers when the Assistant UI is closed.
  keyboard::KeyboardController::Get()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);

  container_view_->GetWidget()->RemoveObserver(this);
  container_view_ = nullptr;
}

}  // namespace ash
