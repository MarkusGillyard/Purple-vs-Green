#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ArenaRules.h"
#include "ArenaController.generated.h"

class AArenaMatch;
class SWidget;

UCLASS()
class AArenaController final : public APlayerController
{
    GENERATED_BODY()

public:
    AArenaController();
    void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type Reason) override;
    void SetupInputComponent() override;
    void PlayerTick(float DeltaSeconds) override;
    void ShowMenu();
    void BeginRound();
    void ShowResults();
    bool IsMenuOpen() const { return Panel.IsValid(); }
    PvG::Spell GetSpell() const { return Selected; }
    bool IsUsingGamepad() const { return UsingGamepad; }
    void PulseGamepad(float Heavy, float Light, float Seconds);

private:
    enum class Screen : uint8 { Main, Pause, Settings, Results };
    void Show(Screen Page);
    void Hide();
    void SaveSettings() const;
    void OpenSettings(Screen From);
    void TogglePause();
    void Resume();
    void MainMenu();
    void Quit();
    void MoveForward(float Amount);
    void MoveRight(float Amount);
    void LookYaw(float Amount);
    void LookPitch(float Amount);
    void PadForward(float Amount);
    void PadStrafe(float Amount);
    void PadYaw(float Amount);
    void PadPitch(float Amount);
    void PadCastPressed();
    void PadCastReleased();
    void PadSprint();
    void PadJump();
    void PadWard();
    void PadPause();
    void PadPreviousSpell();
    void PadNextSpell();
    void CycleSpell(int Step);
    void ApplySprint();
    void DeviceConnectionChanged(EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId Device);
    void PressCast();
    void ReleaseCast();
    void Sprint();
    void Walk();
    void JumpPressed();
    void JumpReleased();
    void CastWard();
    void SelectThread() { Selected = PvG::Spell::Thread; }
    void SelectFan() { Selected = PvG::Spell::Fan; }
    void SelectLance() { Selected = PvG::Spell::Lance; }
    void SelectComet() { Selected = PvG::Spell::Comet; }
    void RunSmoke();

    AArenaMatch* Match() const;
    TSharedPtr<SWidget> Panel;
    TArray<TSharedRef<SWidget>> MenuFocus;
    FDelegateHandle PadConnection;
    Screen Current = Screen::Main;
    Screen SettingsFrom = Screen::Main;
    PvG::Spell Selected = PvG::Spell::Thread;
    bool Casting = false;
    bool PadCasting = false;
    bool KeyboardSprinting = false;
    bool PadSprinting = false;
    bool UsingGamepad = false;
    bool InvertPadPitch = false;
    bool PadVibration = true;
    bool CastQueued = false;
    bool Smoke = false;
    int SmokeRound = 0;
    double SmokeStarted = 0.;
    double SmokeMenuOpened = 0.;
    double LastFrameAt = 0.;
    bool SmokeWardUsed = false;
    TArray<float> FrameTimes;
    FString SmokeEvidence;
    float Sensitivity = 1.f;
    float PadSensitivity = 1.f;
};
