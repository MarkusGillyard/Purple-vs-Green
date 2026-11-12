#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ArenaRules.h"
#include "ArenaSoldier.generated.h"

class AArenaMatch;
class UCameraComponent;
class UStaticMeshComponent;

UCLASS()
class AArenaSoldier final : public ACharacter
{
    GENERATED_BODY()

public:
    AArenaSoldier();
    void Initialize(AArenaMatch* Match, PvG::Team Side, int Slot);
    void Tick(float DeltaSeconds) override;
    void Respawn(const FTransform& Spawn);
    void Cast(PvG::Spell Spell);
    void Ward();
    bool ReceiveSpell(AArenaSoldier* Caster, float Damage);
    FVector Eye() const;
    void SetAim(FRotator Rotation);
    void SetSprinting(bool Enabled);

    PvG::SoldierRules Rules;
    PvG::Team Team = PvG::Team::Purple;
    int Slot = 0;
    int Eliminations = 0;
    int Defeats = 0;
    double RespawnAt = 0.;

private:
    UStaticMeshComponent* AddPart(FName Name, USceneComponent* Parent, UStaticMesh* Mesh,
                                  FVector Location, FVector Scale);
    void Release();
    void Animate(float DeltaSeconds, double Now);
    void SetTeamAppearance();
    FRotator Aim = FRotator::ZeroRotator;
    double ReleaseAt = 0.;
    double LastCast = -10.;
    double LastHit = -10.;
    double LastStep = 0.;
    float Stride = 0.f;

    UPROPERTY() TObjectPtr<AArenaMatch> Arena;
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<USceneComponent> RightArm;
    UPROPERTY() TObjectPtr<USceneComponent> LeftArm;
    UPROPERTY() TObjectPtr<USceneComponent> RightLeg;
    UPROPERTY() TObjectPtr<USceneComponent> LeftLeg;
    UPROPERTY() TObjectPtr<USceneComponent> CastingHands;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Armor;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Livery;
};
