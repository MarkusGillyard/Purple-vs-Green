#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ArenaRules.h"
#include "ArenaSoldier.generated.h"

class AArenaMatch;
class UCameraComponent;
class UStaticMeshComponent;
class UPoseableMeshComponent;
class UMaterialInterface;

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
    void UpdateArmorPose(float Speed, float Swing, float Casting, float Ward, float Hit, float AimPitch);
    FRotator Aim = FRotator::ZeroRotator;
    double ReleaseAt = 0.;
    double LastCast = -10.;
    double LastHit = -10.;
    double LastStep = 0.;
    float Stride = 0.f;
    float PoseCast = 0.f;
    float PoseWard = 0.f;
    TArray<FQuat> ComponentReferenceRotations;

    UPROPERTY() TObjectPtr<AArenaMatch> Arena;
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<UPoseableMeshComponent> Battlemage;
    UPROPERTY() TObjectPtr<UMaterialInterface> ArmorMaterial;
    UPROPERTY() TObjectPtr<USceneComponent> CastingHands;
    UPROPERTY() TObjectPtr<USceneComponent> CastingRight;
    UPROPERTY() TObjectPtr<USceneComponent> CastingLeft;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Armor;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Livery;
};
