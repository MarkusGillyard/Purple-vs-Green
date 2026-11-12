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
    void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type Reason) override;
    void SetupInputComponent() override;
    void PlayerTick(float DeltaSeconds) override;
    void ShowMenu();
    void BeginRound();
    void ShowResults();
    bool IsMenuOpen() const { return Panel.IsValid(); }
    PvG::Spell GetSpell() const { return Selected; }

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
    Screen Current = Screen::Main;
    Screen SettingsFrom = Screen::Main;
    PvG::Spell Selected = PvG::Spell::Thread;
    bool Casting = false;
    bool Smoke = false;
    int SmokeRound = 0;
    double SmokeStarted = 0.;
    double SmokeMenuOpened = 0.;
    double LastFrameAt = 0.;
    bool SmokeWardUsed = false;
    TArray<float> FrameTimes;
    float Sensitivity = 1.f;
};
