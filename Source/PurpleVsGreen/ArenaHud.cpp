#include "ArenaHud.h"

#include "ArenaController.h"
#include "ArenaMatch.h"
#include "ArenaSoldier.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

namespace
{
const FLinearColor Paper(.89f, .93f, .96f);
const FLinearColor Ink(.015f, .024f, .04f, .88f);
const FLinearColor Muted(.54f, .64f, .71f);
}

void AArenaHud::Label(const FString& Text, float X, float Y, float Size, FLinearColor Color, bool Centered)
{
    UFont* Font = GEngine->GetMediumFont();
    if (Centered)
    {
        float Width = 0.f, Height = 0.f;
        GetTextSize(Text, Width, Height, Font, Size * Unit);
        X -= Width * .5f;
    }
    DrawText(Text, Color, X, Y, Font, Size * Unit, false);
}

void AArenaHud::Meter(float X, float Y, float Width, float Height, float Fraction, FLinearColor Color)
{
    DrawRect(FLinearColor(.13f, .18f, .22f, .95f), X, Y, Width, Height);
    DrawRect(Color, X, Y, Width * FMath::Clamp(Fraction, 0.f, 1.f), Height);
}

void AArenaHud::DrawHUD()
{
    Super::DrawHUD();
    const AArenaMatch* Arena = GetWorld()->GetAuthGameMode<AArenaMatch>();
    const AArenaController* Controller = Cast<AArenaController>(GetOwningPlayerController());
    const AArenaSoldier* Player = Arena ? Arena->PlayerSoldier() : nullptr;
    if (!Arena || !Arena->IsPlaying() || !Player || !Controller || Controller->IsMenuOpen() || !Canvas)
        return;

    const float Width = Canvas->ClipX;
    const float Height = Canvas->ClipY;
    Unit = FMath::Clamp(Height / 900.f, .62f, 1.65f);
    const float Mid = Width / 2.f;
    const double Now = GetWorld()->GetTimeSeconds();
    const int Time = FMath::Max(0, FMath::CeilToInt(Arena->Rules.EndsAt - Now));
    const FLinearColor Team = AArenaMatch::TeamTint(Player->Team);
    DrawRect(Ink, Mid - 190.f * Unit, 19.f * Unit, 380.f * Unit, 54.f * Unit);
    DrawRect(AArenaMatch::TeamTint(PvG::Team::Purple), Mid - 190.f * Unit, 19.f * Unit, 4.f * Unit, 54.f * Unit);
    DrawRect(AArenaMatch::TeamTint(PvG::Team::Green), Mid + 186.f * Unit, 19.f * Unit, 4.f * Unit, 54.f * Unit);
    Label(FString::Printf(TEXT("PURPLE   %02d"), Arena->Rules.Score[0]), Mid - 116.f * Unit, 34.f * Unit, 1.0f, Paper, true);
    Label(FString::Printf(TEXT("%d:%02d"), Time / 60, Time % 60), Mid, 31.f * Unit, 1.32f, Paper, true);
    Label(FString::Printf(TEXT("%02d   GREEN"), Arena->Rules.Score[1]), Mid + 116.f * Unit, 34.f * Unit, 1.0f, Paper, true);
    Label(FString::Printf(TEXT("FIRST TO %d"), Arena->ScoreLimit), Mid, 80.f * Unit, .73f, Paper, true);

    const float Left = 27.f * Unit;
    const float Bottom = Height - 138.f * Unit;
    DrawRect(Ink, Left, Bottom, 235.f * Unit, 109.f * Unit);
    DrawRect(Team, Left, Bottom, 3.f * Unit, 109.f * Unit);
    Label(FString::Printf(TEXT("VITALITY   %03d"), FMath::CeilToInt(Player->Rules.Health)),
        Left + 14.f * Unit, Bottom + 12.f * Unit, 1.05f, Paper);
    Meter(Left + 14.f * Unit, Bottom + 40.f * Unit, 207.f * Unit, 4.f * Unit, Player->Rules.Health / PvG::FullHealth, Team);
    Label(FString::Printf(TEXT("FOCUS   %03d"), FMath::FloorToInt(Player->Rules.Focus)),
        Left + 14.f * Unit, Bottom + 53.f * Unit, .93f, Paper);
    Meter(Left + 14.f * Unit, Bottom + 78.f * Unit, 207.f * Unit, 3.f * Unit,
        Player->Rules.Focus / PvG::FullFocus, FLinearColor(.23f, .70f, .86f));
    const TCHAR* WardButton = Controller->IsUsingGamepad() ? TEXT("L2 / LT") : TEXT("Q");
    const FString Ward = Now < Player->Rules.WardUntil ? TEXT("WARD ACTIVE") :
        Now < Player->Rules.WardReadyAt ? FString::Printf(TEXT("%s WARD   %.1fs"), WardButton, Player->Rules.WardReadyAt - Now) :
        FString::Printf(TEXT("%s WARD READY"), WardButton);
    Label(Ward, Left + 14.f * Unit, Bottom + 86.f * Unit, .70f, Muted);

    const int Selected = PvG::Index(Controller->GetSpell());
    const float Right = Width - 262.f * Unit;
    if (Controller->IsUsingGamepad())
        Label(TEXT("L1 / R1   •   LB / RB"), Right + 13.f * Unit, Height - 180.f * Unit, .75f, Paper);
    for (int Spell = 0; Spell < PvG::Index(PvG::Spell::Count); ++Spell)
    {
        const float Y = Height - (160.f - 33.f * Spell) * Unit;
        const PvG::SpellSpec& Spec = PvG::Arsenal[Spell];
        const FLinearColor Tint = AArenaMatch::SpellTint(static_cast<PvG::Spell>(Spell));
        DrawRect(Spell == Selected ? FLinearColor(.05f, .09f, .14f, .97f) : Ink, Right, Y, 235.f * Unit, 28.f * Unit);
        if (Spell == Selected)
        {
            DrawRect(Tint, Right, Y, 3.f * Unit, 28.f * Unit);
            if (Now < Player->Rules.ReadyAt && !Player->Rules.Charging)
            {
                const float Delay = PvG::Arsenal[PvG::Index(Player->Rules.ActiveSpell)].Cooldown;
                const float Ready = 1.f - static_cast<float>(Player->Rules.ReadyAt - Now) / Delay;
                Meter(Right + 3.f * Unit, Y + 26.f * Unit, 232.f * Unit, 2.f * Unit, Ready, Tint);
            }
        }
        const FString Name = Controller->IsUsingGamepad() ? FString(UTF8_TO_TCHAR(Spec.Name)) :
            FString::Printf(TEXT("%d   %s"), Spell + 1, UTF8_TO_TCHAR(Spec.Name));
        Label(Name, Right + 13.f * Unit, Y + 5.f * Unit, .89f, Spell == Selected ? Paper : Muted);
        Label(FString::Printf(TEXT("%.0f"), Spec.Cost), Right + 195.f * Unit, Y + 5.f * Unit, .83f,
            Player->Rules.Focus >= Spec.Cost ? Tint : Muted);
    }

    if (!Player->Rules.Alive())
    {
        DrawRect(Ink, Mid - 145.f * Unit, Height / 2.f - 27.f * Unit, 290.f * Unit, 77.f * Unit);
        Label(TEXT("DISPELLED"), Mid, Height / 2.f - 14.f * Unit, 1.35f, Paper, true);
        Label(FString::Printf(TEXT("Reforming in %.1fs"), FMath::Max(0., Player->RespawnAt - Now)),
            Mid, Height / 2.f + 17.f * Unit, .91f, Muted, true);
        return;
    }

    const float Y = Height / 2.f;
    const float Hurt = FMath::Clamp(1.f - static_cast<float>(Now - Arena->LastPlayerHurtAt) * 3.8f, 0.f, 1.f);
    if (Hurt > 0.f)
    {
        const FLinearColor Damage(.87f, .23f, .18f, Hurt * .46f);
        DrawRect(Damage, 0.f, 0.f, 8.f * Unit, Height);
        DrawRect(Damage, Width - 8.f * Unit, 0.f, 8.f * Unit, Height);
    }
    if (Now - Arena->LastPlayerHitAt < .15)
    {
        const float D = 12.f * Unit;
        for (const float X : {-1.f, 1.f})
            for (const float Sign : {-1.f, 1.f})
                DrawLine(Mid + X * D, Y + Sign * D, Mid + X * (D + 5.f * Unit), Y + Sign * (D + 5.f * Unit), Paper, 1.5f * Unit);
    }
    const FLinearColor Aim = Player->Rules.Charging ? AArenaMatch::SpellTint(Player->Rules.ActiveSpell) : Paper;
    const float Gap = Player->Rules.Charging ? 3.f * Unit : 5.f * Unit;
    DrawRect(Ink, Mid - 2.f, Y - 2.f, 4.f, 4.f);
    DrawRect(Aim, Mid - 1.f, Y - 1.f, 2.f, 2.f);
    DrawRect(Aim, Mid - Gap - 7.f * Unit, Y, 7.f * Unit, 1.25f * Unit);
    DrawRect(Aim, Mid + Gap, Y, 7.f * Unit, 1.25f * Unit);
    if (Player->Rules.Charging)
        Label(TEXT("CHANNELING"), Mid, Y + 24.f * Unit, .71f, Aim, true);
    else if (Now < Player->Rules.ProtectedUntil)
        Label(TEXT("SPAWN PROTECTION"), Mid, Y + 24.f * Unit, .71f, Paper, true);
    else if (Now < Player->Rules.WardUntil)
        Label(TEXT("WARD"), Mid, Y + 24.f * Unit, .71f, FLinearColor(.25f, .86f, .96f), true);
}
