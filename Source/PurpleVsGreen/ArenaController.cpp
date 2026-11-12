#include "ArenaController.h"

#include "ArenaMatch.h"
#include "ArenaSoldier.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
const FLinearColor Paper(.89f, .93f, .96f);
const FLinearColor Muted(.54f, .65f, .73f);
const FLinearColor Accent(.44f, .75f, .77f);
const FLinearColor PanelTint(.021f, .036f, .057f, .98f);
constexpr const TCHAR* SettingsSection = TEXT("PurpleVsGreen.Player");

class SArenaMenu final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SArenaMenu) {}
        SLATE_DEFAULT_SLOT(FArguments, Content)
        SLATE_EVENT(FSimpleDelegate, OnBack)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        OnBack = Args._OnBack;
        ChildSlot[Args._Content.Widget];
    }

    FReply OnPreviewKeyDown(const FGeometry&, const FKeyEvent& Event) override
    {
        const FKey Key = Event.GetKey();
        if (Key != EKeys::Gamepad_FaceButton_Right && Key != EKeys::Gamepad_Special_Right &&
            Key != EKeys::Escape)
            return FReply::Unhandled();
        if (!Event.IsRepeat())
            OnBack.ExecuteIfBound();
        return FReply::Handled();
    }

private:
    FSimpleDelegate OnBack;
};

float StickRate(float Axis, float Rate)
{
    return Axis * FMath::Lerp(.38f, 1.f, FMath::Square(Axis)) * Rate;
}

TSharedRef<STextBlock> Text(const FString& Label, int Size, FLinearColor Color = Paper, bool Heavy = false)
{
    return SNew(STextBlock)
        .Text(FText::FromString(Label))
        .Font(FCoreStyle::GetDefaultFontStyle(Heavy ? TEXT("Bold") : TEXT("Regular"), Size))
        .ColorAndOpacity(Color);
}

void Line(const TSharedRef<SVerticalBox>& Box, const FString& Label, int Size,
          FLinearColor Color = Paper, float After = 8.f, bool Heavy = false)
{
    Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, After)[Text(Label, Size, Color, Heavy)];
}

TSharedRef<SButton> Button(FString Name, TFunction<void()> Action, FLinearColor Color = Paper)
{
    return SNew(SButton)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .ContentPadding(FMargin(13.f, 10.f))
        .OnClicked_Lambda([Action = MoveTemp(Action)] { Action(); return FReply::Handled(); })
        [Text(Name, 13, Color, true)];
}

void Row(const TSharedRef<SVerticalBox>& Box, const TSharedRef<SWidget>& Widget, float Space = 8.f)
{
    Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, Space)[Widget];
}

void Choice(const TSharedRef<SVerticalBox>& Box, FString Label, TSharedRef<SWidget> Value)
{
    Row(Box, SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(.41f).VAlign(VAlign_Center)[Text(Label, 12, Muted)]
        + SHorizontalBox::Slot().FillWidth(.59f)[Value]);
}
}

AArenaController::AArenaController()
{
    bAutoManageActiveCameraTarget = false;
}

AArenaMatch* AArenaController::Match() const
{
    return GetWorld()->GetAuthGameMode<AArenaMatch>();
}

void AArenaController::BeginPlay()
{
    Super::BeginPlay();
    GConfig->GetFloat(SettingsSection, TEXT("Sensitivity"), Sensitivity, GGameUserSettingsIni);
    Sensitivity = FMath::Clamp(Sensitivity, .3f, 2.4f);
    GConfig->GetFloat(SettingsSection, TEXT("PadSensitivity"), PadSensitivity, GGameUserSettingsIni);
    PadSensitivity = FMath::Clamp(PadSensitivity, .35f, 2.5f);
    GConfig->GetBool(SettingsSection, TEXT("InvertPadPitch"), InvertPadPitch, GGameUserSettingsIni);
    GConfig->GetBool(SettingsSection, TEXT("PadVibration"), PadVibration, GGameUserSettingsIni);
    PadConnection = IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange()
        .AddUObject(this, &AArenaController::DeviceConnectionChanged);
    if (AArenaMatch* Arena = Match())
    {
        GConfig->GetFloat(SettingsSection, TEXT("Volume"), Arena->MasterVolume, GGameUserSettingsIni);
        Arena->MasterVolume = FMath::Clamp(Arena->MasterVolume, 0.f, 1.f);
    }
    Smoke = FParse::Param(FCommandLine::Get(), TEXT("PvGSmoke"));
    SmokeStarted = FPlatformTime::Seconds();
}

void AArenaController::EndPlay(const EEndPlayReason::Type Reason)
{
    IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(PadConnection);
    Hide();
    Super::EndPlay(Reason);
}

void AArenaController::SaveSettings() const
{
    GConfig->SetFloat(SettingsSection, TEXT("Sensitivity"), Sensitivity, GGameUserSettingsIni);
    GConfig->SetFloat(SettingsSection, TEXT("PadSensitivity"), PadSensitivity, GGameUserSettingsIni);
    GConfig->SetBool(SettingsSection, TEXT("InvertPadPitch"), InvertPadPitch, GGameUserSettingsIni);
    GConfig->SetBool(SettingsSection, TEXT("PadVibration"), PadVibration, GGameUserSettingsIni);
    GConfig->SetFloat(SettingsSection, TEXT("Volume"), Match()->MasterVolume, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}

void AArenaController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAxis(TEXT("Forward"), this, &AArenaController::MoveForward);
    InputComponent->BindAxis(TEXT("Strafe"), this, &AArenaController::MoveRight);
    InputComponent->BindAxis(TEXT("Yaw"), this, &AArenaController::LookYaw);
    InputComponent->BindAxis(TEXT("Pitch"), this, &AArenaController::LookPitch);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AArenaController::PressCast);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AArenaController::ReleaseCast);
    InputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AArenaController::Sprint);
    InputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &AArenaController::Walk);
    InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AArenaController::JumpPressed);
    InputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &AArenaController::JumpReleased);
    InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AArenaController::CastWard);
    InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AArenaController::SelectThread);
    InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AArenaController::SelectFan);
    InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AArenaController::SelectLance);
    InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AArenaController::SelectComet);
    InputComponent->BindAxis(TEXT("PadForward"), this, &AArenaController::PadForward);
    InputComponent->BindAxis(TEXT("PadStrafe"), this, &AArenaController::PadStrafe);
    InputComponent->BindAxis(TEXT("PadYaw"), this, &AArenaController::PadYaw);
    InputComponent->BindAxis(TEXT("PadPitch"), this, &AArenaController::PadPitch);
    InputComponent->BindKey(EKeys::Gamepad_RightTrigger, IE_Pressed, this, &AArenaController::PadCastPressed);
    InputComponent->BindKey(EKeys::Gamepad_RightTrigger, IE_Released, this, &AArenaController::PadCastReleased);
    InputComponent->BindKey(EKeys::Gamepad_LeftTrigger, IE_Pressed, this, &AArenaController::PadWard);
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &AArenaController::PadJump);
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Released, this, &AArenaController::JumpReleased);
    InputComponent->BindKey(EKeys::Gamepad_LeftThumbstick, IE_Pressed, this, &AArenaController::PadSprint);
    InputComponent->BindKey(EKeys::Gamepad_LeftShoulder, IE_Pressed, this, &AArenaController::PadPreviousSpell);
    InputComponent->BindKey(EKeys::Gamepad_RightShoulder, IE_Pressed, this, &AArenaController::PadNextSpell);
    InputComponent->BindKey(EKeys::Gamepad_Special_Right, IE_Pressed, this, &AArenaController::PadPause).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AArenaController::TogglePause).bExecuteWhenPaused = true;
}

void AArenaController::Hide()
{
    if (Panel && GEngine && GEngine->GameViewport)
        GEngine->GameViewport->RemoveViewportWidgetContent(Panel.ToSharedRef());
    Panel.Reset();
    MenuFocus.Reset();
}

void AArenaController::ShowMenu()
{
    Show(Screen::Main);
}

void AArenaController::ShowResults()
{
    Show(Screen::Results);
}

void AArenaController::Show(Screen Page)
{
    int FocusIndex = Page == Screen::Main ? 4 : 0;
    if (Page == Current)
        for (int Index = 0; Index < MenuFocus.Num(); ++Index)
            if (MenuFocus[Index]->HasKeyboardFocus())
                FocusIndex = Index;
    Hide();
    Current = Page;
    Casting = false;
    PadCasting = false;
    KeyboardSprinting = false;
    PadSprinting = false;
    ApplySprint();
    CastQueued = false;
    if (!GEngine || !GEngine->GameViewport)
        return;
    AArenaMatch* Arena = Match();
    const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
    const auto MenuButton = [this, Arena](FString Name, TFunction<void()> Action, FLinearColor Color = Paper)
    {
        const auto Widget = Button(MoveTemp(Name), [Arena, Action = MoveTemp(Action)]
        {
            Arena->Feedback(FVector::ZeroVector, 9, .28f);
            Action();
        }, Color);
        MenuFocus.Add(Widget);
        return Widget;
    };
    const auto Slider = [this, &Box](const TCHAR* Name, float& Value, float Min, float Max, float Step)
    {
        const TSharedRef<SSlider> Widget = SNew(SSlider).MinValue(Min).MaxValue(Max).StepSize(Step)
            .RequiresControllerLock(false)
            .Value_Lambda([&Value] { return Value; })
            .OnValueChanged_Lambda([&Value](float NewValue) { Value = NewValue; })
            .OnMouseCaptureEnd_Lambda([this] { SaveSettings(); });
        MenuFocus.Add(Widget);
        Choice(Box, Name, Widget);
    };
    const FString Team = Arena->ChosenTeam == PvG::Team::Purple ? TEXT("Purple") : TEXT("Green");

    if (Page == Screen::Main)
    {
        Line(Box, TEXT("PURPLE VS GREEN"), 32, Paper, 4.f, true);
        Line(Box, TEXT("The Meridian  /  Team Deathmatch"), 14, Accent, 22.f);
        Choice(Box, TEXT("TEAM"), MenuButton(Team, [this, Arena]
        {
            Arena->ChosenTeam = PvG::EnemyOf(Arena->ChosenTeam);
            Show(Screen::Main);
        }, AArenaMatch::TeamTint(Arena->ChosenTeam)));
        Choice(Box, TEXT("ELIMINATIONS"), MenuButton(FString::FromInt(Arena->ScoreLimit), [this, Arena]
        {
            Arena->ScoreLimit = Arena->ScoreLimit == 10 ? 20 : Arena->ScoreLimit == 20 ? 30 : 10;
            Show(Screen::Main);
        }));
        Choice(Box, TEXT("TIME LIMIT"), MenuButton(FString::Printf(TEXT("%d min"), Arena->Minutes), [this, Arena]
        {
            Arena->Minutes = Arena->Minutes == 3 ? 5 : Arena->Minutes == 5 ? 8 : 3;
            Show(Screen::Main);
        }));
        const TCHAR* Difficulties[] = {TEXT("Apprentice"), TEXT("Adept"), TEXT("Master")};
        Choice(Box, TEXT("AI DIFFICULTY"), MenuButton(Difficulties[Arena->Difficulty], [this, Arena]
        {
            Arena->Difficulty = (Arena->Difficulty + 1) % 3;
            Show(Screen::Main);
        }));
        Line(Box, TEXT("WASD Move   Shift Sprint   Space Jump"), 11, Muted, 3.f);
        Line(Box, TEXT("Left Click Cast   1–4 Select Spell   Q Ward"), 11, Muted, 8.f);
        Line(Box, TEXT("Sticks Move / Aim   L3 Sprint   Cross / A Jump"), 11, Muted, 3.f);
        Line(Box, TEXT("R2 / RT Cast   L2 / LT Ward   Bumpers Select Spell"), 11, Muted, 3.f);
        Line(Box, TEXT("D-pad Navigate   Cross / A Confirm   Options / Menu Pause"), 11, Muted, 17.f);
        Row(Box, MenuButton(TEXT("Enter the Meridian"), [Arena] { Arena->StartMatch(); }, Accent));
        Row(Box, MenuButton(TEXT("Settings"), [this] { OpenSettings(Screen::Main); }));
        Row(Box, MenuButton(TEXT("Exit"), [this] { Quit(); }), 0.f);
    }
    else if (Page == Screen::Pause)
    {
        Line(Box, TEXT("MATCH PAUSED"), 27, Paper, 22.f, true);
        Row(Box, MenuButton(TEXT("Resume"), [this] { Resume(); }, Accent));
        Row(Box, MenuButton(TEXT("Settings"), [this] { OpenSettings(Screen::Pause); }));
        Row(Box, MenuButton(TEXT("Return to menu"), [this] { MainMenu(); }));
        Row(Box, MenuButton(TEXT("Exit"), [this] { Quit(); }), 0.f);
    }
    else if (Page == Screen::Settings)
    {
        Line(Box, TEXT("SETTINGS"), 28, Paper, 19.f, true);
        UGameUserSettings* Settings = GEngine->GetGameUserSettings();
        const FIntPoint Resolution = Settings->GetScreenResolution();
        Choice(Box, TEXT("RESOLUTION"), MenuButton(FString::Printf(TEXT("%d × %d"), Resolution.X, Resolution.Y), [this, Settings, Resolution]
        {
            Settings->SetScreenResolution(Resolution.X >= 1920 ? FIntPoint(1280, 720) :
                Resolution.X >= 1600 ? FIntPoint(1920, 1080) : FIntPoint(1600, 900));
            Settings->ApplySettings(false);
            Settings->ConfirmVideoMode();
            Settings->SaveSettings();
            Show(Screen::Settings);
        }));
        const EWindowMode::Type Mode = Settings->GetFullscreenMode();
        Choice(Box, TEXT("DISPLAY"), MenuButton(Mode == EWindowMode::Windowed ? TEXT("Windowed") :
            Mode == EWindowMode::WindowedFullscreen ? TEXT("Borderless") : TEXT("Fullscreen"), [this, Settings, Mode]
        {
            Settings->SetFullscreenMode(Mode == EWindowMode::Windowed ? EWindowMode::WindowedFullscreen :
                Mode == EWindowMode::WindowedFullscreen ? EWindowMode::Fullscreen : EWindowMode::Windowed);
            Settings->ApplySettings(false);
            Settings->ConfirmVideoMode();
            Settings->SaveSettings();
            Show(Screen::Settings);
        }));
        const int Quality = FMath::Clamp(Settings->GetViewDistanceQuality(), 0, 3);
        const TCHAR* Qualities[] = {TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Ultra")};
        Choice(Box, TEXT("GRAPHICS"), MenuButton(Qualities[Quality], [this, Settings, Quality]
        {
            Settings->SetOverallScalabilityLevel((Quality + 1) % 4);
            Settings->SetResolutionScaleValueEx(100.f);
            Settings->ApplySettings(false);
            Settings->SaveSettings();
            Show(Screen::Settings);
        }));
        Slider(TEXT("MOUSE SENSITIVITY"), Sensitivity, .3f, 2.4f, .1f);
        Slider(TEXT("STICK SENSITIVITY"), PadSensitivity, .35f, 2.5f, .1f);
        Choice(Box, TEXT("INVERT STICK Y"), MenuButton(InvertPadPitch ? TEXT("On") : TEXT("Off"), [this]
        {
            InvertPadPitch = !InvertPadPitch;
            SaveSettings();
            Show(Screen::Settings);
        }));
        Choice(Box, TEXT("VIBRATION"), MenuButton(PadVibration ? TEXT("On") : TEXT("Off"), [this]
        {
            PadVibration = !PadVibration;
            SaveSettings();
            Show(Screen::Settings);
        }));
        Slider(TEXT("MASTER VOLUME"), Arena->MasterVolume, 0.f, 1.f, .05f);
        Line(Box, TEXT("D-pad Left / Right adjusts sliders"), 11, Muted, 9.f);
        Row(Box, MenuButton(TEXT("Back"), [this] { SaveSettings(); Show(SettingsFrom); }), 0.f);
    }
    else
    {
        const int Own = Arena->Rules.Score[PvG::Index(Arena->ChosenTeam)];
        const int Opponent = Arena->Rules.Score[PvG::Index(PvG::EnemyOf(Arena->ChosenTeam))];
        Line(Box, Own == Opponent ? TEXT("DRAW") : Own > Opponent ? TEXT("VICTORY") : TEXT("DEFEAT"), 34, Paper, 13.f, true);
        Line(Box, FString::Printf(TEXT("Purple  %d      Green  %d"), Arena->Rules.Score[0], Arena->Rules.Score[1]), 20, Accent);
        const AArenaSoldier* Player = Arena->PlayerSoldier();
        Line(Box, FString::Printf(TEXT("%d eliminations   /   %d defeats"), Player->Eliminations, Player->Defeats), 13, Muted, 19.f);
        Row(Box, MenuButton(TEXT("Rematch"), [Arena] { Arena->StartMatch(); }, Accent));
        Row(Box, MenuButton(TEXT("Return to menu"), [this] { MainMenu(); }));
        Row(Box, MenuButton(TEXT("Exit"), [this] { Quit(); }), 0.f);
    }

    const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    Panel = SNew(SArenaMenu).OnBack(FSimpleDelegate::CreateUObject(this, &AArenaController::TogglePause))
    [
        SNew(SOverlay)
        + SOverlay::Slot()
        [SNew(SBorder).BorderImage(Brush).BorderBackgroundColor(FLinearColor(.01f, .02f, .04f, .39f))]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(SBorder).BorderImage(Brush).BorderBackgroundColor(PanelTint).Padding(FMargin(30.f, 24.f))
            [SNew(SBox).WidthOverride(406.f)[Box]]
        ]
    ];
    GEngine->GameViewport->AddViewportWidgetContent(Panel.ToSharedRef(), 20);
    FInputModeGameAndUI InputMode;
    InputMode.SetWidgetToFocus(MenuFocus[FMath::Clamp(FocusIndex, 0, MenuFocus.Num() - 1)]);
    InputMode.SetHideCursorDuringCapture(false);
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    bShowMouseCursor = true;
    SetIgnoreLookInput(true);
    SetIgnoreMoveInput(true);
    if (Smoke)
        SmokeMenuOpened = FPlatformTime::Seconds();
}

void AArenaController::BeginRound()
{
    Selected = PvG::Spell::Thread;
    Resume();
}

void AArenaController::Resume()
{
    Hide();
    SetPause(false);
    FInputModeGameOnly Mode;
    SetInputMode(Mode);
    bShowMouseCursor = false;
    ResetIgnoreLookInput();
    ResetIgnoreMoveInput();
    Casting = false;
    PadCasting = false;
    CastQueued = false;
}

void AArenaController::MainMenu()
{
    SetPause(false);
    Match()->ReturnToMenu();
}

void AArenaController::Quit()
{
    UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, true);
}

void AArenaController::OpenSettings(Screen From)
{
    SettingsFrom = From;
    Show(Screen::Settings);
}

void AArenaController::TogglePause()
{
    if (Panel.IsValid())
    {
        if (Current == Screen::Pause)
            Resume();
        else if (Current == Screen::Results)
            MainMenu();
        else if (Current == Screen::Settings)
        {
            SaveSettings();
            Show(SettingsFrom);
        }
        return;
    }
    if (Match()->IsPlaying() && SetPause(true))
    {
        Walk();
        Show(Screen::Pause);
    }
}

void AArenaController::MoveForward(float Amount)
{
    if (!Panel.IsValid() && GetPawn() && Amount != 0.f)
        GetPawn()->AddMovementInput(FRotator(0, GetControlRotation().Yaw, 0).Vector(), Amount);
}

void AArenaController::MoveRight(float Amount)
{
    if (!Panel.IsValid() && GetPawn() && Amount != 0.f)
        GetPawn()->AddMovementInput(FRotator(0, GetControlRotation().Yaw + 90., 0).Vector(), Amount);
}

void AArenaController::LookYaw(float Amount)
{
    if (!Panel.IsValid() && !FMath::IsNearlyZero(Amount))
    {
        UsingGamepad = false;
        AddYawInput(Amount * Sensitivity);
    }
}

void AArenaController::LookPitch(float Amount)
{
    if (!Panel.IsValid() && !FMath::IsNearlyZero(Amount))
    {
        UsingGamepad = false;
        AddPitchInput(Amount * Sensitivity);
    }
}

void AArenaController::PadForward(float Amount)
{
    if (!Panel.IsValid() && Amount != 0.f)
    {
        UsingGamepad = true;
        MoveForward(Amount);
    }
}

void AArenaController::PadStrafe(float Amount)
{
    if (!Panel.IsValid() && Amount != 0.f)
    {
        UsingGamepad = true;
        MoveRight(Amount);
    }
}

void AArenaController::PadYaw(float Amount)
{
    if (!IsLookInputIgnored() && Amount != 0.f)
    {
        UsingGamepad = true;
        RotationInput.Yaw += StickRate(Amount, 270.f) * PadSensitivity *
            FMath::Min(GetWorld()->GetDeltaSeconds(), .05f);
    }
}

void AArenaController::PadPitch(float Amount)
{
    if (!IsLookInputIgnored() && Amount != 0.f)
    {
        UsingGamepad = true;
        RotationInput.Pitch += StickRate(Amount, 180.f) * PadSensitivity *
            FMath::Min(GetWorld()->GetDeltaSeconds(), .05f) * (InvertPadPitch ? -1.f : 1.f);
    }
}

void AArenaController::PadCastPressed()
{
    if (!Panel.IsValid())
    {
        UsingGamepad = true;
        PadCasting = true;
        CastQueued = true;
    }
}

void AArenaController::PadCastReleased() { PadCasting = false; }

void AArenaController::PadSprint()
{
    if (!Panel.IsValid())
    {
        UsingGamepad = true;
        PadSprinting = !PadSprinting;
        ApplySprint();
    }
}

void AArenaController::PadJump() { UsingGamepad = true; JumpPressed(); }
void AArenaController::PadWard() { UsingGamepad = true; CastWard(); }
void AArenaController::PadPause() { UsingGamepad = true; TogglePause(); }
void AArenaController::PadPreviousSpell() { CycleSpell(-1); }
void AArenaController::PadNextSpell() { CycleSpell(1); }

void AArenaController::CycleSpell(int Step)
{
    if (!Panel.IsValid())
    {
        UsingGamepad = true;
        constexpr int Count = PvG::Index(PvG::Spell::Count);
        Selected = static_cast<PvG::Spell>((PvG::Index(Selected) + Count + Step) % Count);
    }
}

void AArenaController::PulseGamepad(float Strength, float Seconds)
{
    if (UsingGamepad && PadVibration && IsLocalController())
        PlayDynamicForceFeedback(Strength, Seconds, true, false, true, true);
}

void AArenaController::DeviceConnectionChanged(EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId)
{
    if (State != EInputDeviceConnectionState::Disconnected ||
        !GetLocalPlayer() || User != GetLocalPlayer()->GetPlatformUserId())
        return;

    if (PlayerInput)
        PlayerInput->FlushPressedKeys();
    PadCasting = false;
    PadSprinting = false;
    CastQueued = false;
    UsingGamepad = false;
    ApplySprint();
    JumpReleased();
}
void AArenaController::PressCast()
{
    if (!Panel.IsValid())
    {
        UsingGamepad = false;
        Casting = true;
        CastQueued = true;
    }
}
void AArenaController::ReleaseCast() { Casting = false; }

void AArenaController::Sprint()
{
    if (!Panel.IsValid())
    {
        UsingGamepad = false;
        KeyboardSprinting = true;
        ApplySprint();
    }
}

void AArenaController::Walk()
{
    KeyboardSprinting = false;
    ApplySprint();
}

void AArenaController::ApplySprint()
{
    if (AArenaSoldier* Soldier = Cast<AArenaSoldier>(GetPawn()))
        Soldier->SetSprinting(KeyboardSprinting || PadSprinting);
}

void AArenaController::JumpPressed()
{
    if (!Panel.IsValid())
        if (AArenaSoldier* Soldier = Cast<AArenaSoldier>(GetPawn()))
            Soldier->Jump();
}

void AArenaController::JumpReleased()
{
    if (AArenaSoldier* Soldier = Cast<AArenaSoldier>(GetPawn()))
        Soldier->StopJumping();
}

void AArenaController::CastWard()
{
    if (!Panel.IsValid())
        if (AArenaSoldier* Soldier = Cast<AArenaSoldier>(GetPawn()))
        {
            const double Before = Soldier->Rules.WardUntil;
            Soldier->Ward();
            if (Soldier->Rules.WardUntil > Before)
                PulseGamepad(.23f, .18f);
        }
}

void AArenaController::PlayerTick(float DeltaSeconds)
{
    Super::PlayerTick(DeltaSeconds);
    if (const AArenaSoldier* Soldier = Cast<AArenaSoldier>(GetPawn()); Soldier && !Soldier->Rules.Alive())
        PadSprinting = false;
    if (Casting || PadCasting || CastQueued)
        if (AArenaSoldier* Soldier = Cast<AArenaSoldier>(GetPawn()))
            Soldier->Cast(Selected);
    CastQueued = false;
    if (Smoke && Match())
        RunSmoke();
}

void AArenaController::RunSmoke()
{
    AArenaMatch* Arena = Match();
    const double WallTime = FPlatformTime::Seconds();
    if (WallTime - SmokeStarted > 180.)
    {
        UE_LOG(LogTemp, Error, TEXT("PvG smoke timed out in round %d"), SmokeRound);
        FPlatformMisc::RequestExitWithStatus(false, 3);
        return;
    }
    if (Panel.IsValid() && WallTime - SmokeMenuOpened < 1.2)
        return;
    if (Arena->Rules.State == PvG::Phase::Menu)
    {
        Arena->ChosenTeam = SmokeRound == 0 ? PvG::Team::Green : PvG::Team::Purple;
        Arena->ScoreLimit = 5;
        Arena->Minutes = 2;
        Arena->Difficulty = SmokeRound == 0 ? 1 : 2;
        FrameTimes.Reset();
        LastFrameAt = WallTime;
        SmokeWardUsed = false;
        Arena->StartMatch();
        return;
    }
    if (Arena->Rules.State == PvG::Phase::Results)
    {
        if (FMath::Max(Arena->Rules.Score[0], Arena->Rules.Score[1]) != Arena->ScoreLimit ||
            Arena->PlayerHits == 0 || Arena->PlayerWards == 0 || FrameTimes.Num() < 200 ||
            (SmokeRound == 1 && Arena->PlayerCometImpacts == 0))
        {
            UE_LOG(LogTemp, Error, TEXT("PvG smoke missing score limit, hits, ward, frames or a second-round comet impact"));
            FPlatformMisc::RequestExitWithStatus(false, 4);
            return;
        }
        FrameTimes.Sort();
        double Sum = 0.;
        int SlowFrames = 0;
        for (const float Ms : FrameTimes)
        {
            Sum += Ms;
            SlowFrames += Ms > 33.33f;
        }
        const FString Summary = FString::Printf(
            TEXT("PvG smoke round %d team %d score Purple %d Green %d hits %d wards %d comets %d comet impacts %d samples %d mean FPS %.2f p95 %.3f ms p99 %.3f ms max %.3f ms over33 %d"),
            SmokeRound + 1, PvG::Index(Arena->ChosenTeam), Arena->Rules.Score[0], Arena->Rules.Score[1],
            Arena->PlayerHits, Arena->PlayerWards, Arena->PlayerComets, Arena->PlayerCometImpacts,
            FrameTimes.Num(), 1000. * FrameTimes.Num() / Sum, FrameTimes[FrameTimes.Num() * 95 / 100],
            FrameTimes[FrameTimes.Num() * 99 / 100], FrameTimes.Last(), SlowFrames);
        SmokeEvidence += Summary + TEXT("\n");
        UE_LOG(LogTemp, Display, TEXT("%s"), *Summary);
        if (++SmokeRound < 2)
            MainMenu();
        else
        {
            SmokeEvidence += TEXT("PvG smoke passed\n");
            if (!FFileHelper::SaveStringToFile(SmokeEvidence, *(FPaths::ProjectSavedDir() / TEXT("PvGSmoke.txt"))))
            {
                UE_LOG(LogTemp, Error, TEXT("PvG smoke could not write its result"));
                FPlatformMisc::RequestExitWithStatus(false, 5);
                return;
            }
            UE_LOG(LogTemp, Display, TEXT("PvG smoke passed"));
            Quit();
        }
        return;
    }

    FrameTimes.Add((WallTime - LastFrameAt) * 1000.);
    LastFrameAt = WallTime;
    AArenaSoldier* Soldier = Arena->PlayerSoldier();
    if (!Soldier || !Soldier->Rules.Alive())
        return;
    const FVector Eye = Soldier->Eye();
    AArenaSoldier* Target = nullptr;
    double Nearest = 6000.;
    for (const auto& Other : TActorRange<AArenaSoldier>(GetWorld()))
    {
        if (Other->Team == Soldier->Team || !Other->Rules.Alive())
            continue;
        const FVector Chest = Other->GetActorLocation() + FVector(0, 0, 27);
        const double Distance = FVector::Dist(Eye, Chest);
        FHitResult Hit;
        if (Distance < Nearest && GetWorld()->LineTraceSingleByChannel(Hit, Eye, Chest, ECC_Visibility,
            FCollisionQueryParams(SCENE_QUERY_STAT(PvGSmoke), false, Soldier)) && Hit.GetActor() == Other)
        {
            Target = Other;
            Nearest = Distance;
        }
    }
    if (Target)
    {
        if (!SmokeWardUsed)
        {
            CastWard();
            SmokeWardUsed = true;
        }
        const FVector Aim = Target->GetActorLocation() + FVector(0, 0, 25) - Eye;
        Soldier->SetAim(Aim.Rotation());
        if (SmokeRound == 1 && Arena->PlayerCometImpacts == 0 && Nearest < 2800.)
            Selected = PvG::Spell::Comet;
        else
            Selected = Nearest < 650. ? PvG::Spell::Fan : Nearest > 2350. ? PvG::Spell::Lance : PvG::Spell::Thread;
        Soldier->Cast(Selected);
        MoveRight(FMath::Sin(GetWorld()->GetTimeSeconds() * 2.0) > 0. ? .75f : -.75f);
    }
    else
    {
        const FVector Position = Soldier->GetActorLocation();
        const FVector Goal(0., Position.Y > 0. ? 1240. : -1240., Position.Z);
        Soldier->SetAim((Goal - Position).Rotation());
        if (FVector::DistSquared2D(Position, Goal) > FMath::Square(180.))
            MoveForward(1.f);
    }
}
