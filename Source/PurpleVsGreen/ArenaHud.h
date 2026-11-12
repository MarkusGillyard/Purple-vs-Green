#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ArenaHud.generated.h"

UCLASS()
class AArenaHud final : public AHUD
{
    GENERATED_BODY()

public:
    void DrawHUD() override;

private:
    void Label(const FString& Text, float X, float Y, float Size, FLinearColor Color, bool Centered = false);
    void Meter(float X, float Y, float Width, float Height, float Fraction, FLinearColor Color);
    float Unit = 1.f;
};
