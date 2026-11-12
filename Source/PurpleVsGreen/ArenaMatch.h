#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArenaRules.h"
#include "ArenaMatch.generated.h"

class AArenaSoldier;
class AArenaController;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class USoundAttenuation;
class UInstancedStaticMeshComponent;

struct FArenaBot
{
    TWeakObjectPtr<AArenaSoldier> Target;
    FVector LastSeen = FVector::ZeroVector;
    FVector Destination = FVector::ZeroVector;
    FVector LastPosition = FVector::ZeroVector;
    TArray<int> Route;
    double SeesUntil = 0.;
    double NextThink = 0.;
    double FireAt = 0.;
    double RetryAt = 0.;
    int Patrol = 0;
    PvG::Spell Spell = PvG::Spell::Thread;
    bool Visible = false;
    bool Patrolling = false;
};

struct FArenaMissile
{
    TWeakObjectPtr<AArenaSoldier> Caster;
    TWeakObjectPtr<AActor> Visual;
    FVector Location;
    FVector Direction;
    float DistanceLeft = 0.f;
    float TrailElapsed = 0.f;
};

struct FArenaMote
{
    FVector Position;
    FVector Velocity;
    float Age = 0.f;
    float Lifetime = 0.f;
    float Size = 0.f;
};

struct FArenaMotePool
{
    TArray<FArenaMote> Items;
    TArray<FTransform> Transforms;
    int Next = 0;
    bool Visible = false;
};

UCLASS()
class AArenaMatch final : public AGameModeBase
{
    GENERATED_BODY()

public:
    AArenaMatch();
    void StartPlay() override;
    void Tick(float DeltaSeconds) override;
    void StartMatch();
    void ReturnToMenu();
    void Fire(AArenaSoldier* Caster, PvG::Spell Spell, FVector Direction);
    void Defeated(AArenaSoldier* Soldier, AArenaSoldier* Killer);
    void MarkHit(const AArenaSoldier* Caster, const AArenaSoldier* Victim);
    void ShowWard(const AArenaSoldier* Soldier);
    void Feedback(FVector Location, int Event, float Volume = 1.f);
    UMaterialInstanceDynamic* Paint(UObject* Outer, FLinearColor Color, bool Luminous = false) const;
    bool IsPlaying() const { return Rules.State == PvG::Phase::Playing; }
    static FLinearColor TeamTint(PvG::Team Side);
    static FLinearColor SpellTint(PvG::Spell Spell);
    AArenaSoldier* PlayerSoldier() const;

    PvG::MatchRules Rules;
    PvG::Team ChosenTeam = PvG::Team::Green;
    int ScoreLimit = 20;
    int Minutes = 5;
    int Difficulty = 1;
    float MasterVolume = 0.7f;
    int PlayerHits = 0;
    int PlayerDamageEvents = 0;
    int PlayerComets = 0;
    int PlayerCometImpacts = 0;
    int PlayerWards = 0;
    double LastPlayerHitAt = -10.;
    double LastPlayerHurtAt = -10.;

private:
    static constexpr float FloorCenter = 92.f;
    void BuildArena();
    void BuildRoutes();
    void BuildSounds();
    void BuildMotes();
    void EmitMotes(int Batch, FVector Location, FVector Axis, int Count, float Spread, float Speed);
    void UpdateMotes(float DeltaSeconds);
    void ClearRound();
    void ViewOverview() const;
    void UpdateBot(int Index, float DeltaSeconds, double Now);
    void Think(AArenaSoldier* Soldier, FArenaBot& Brain, double Now);
    void Finish();
    void UpdateMissiles(float DeltaSeconds);
    void Explode(AArenaSoldier* Caster, FVector Location);
    void Spark(FVector Location, FLinearColor Tint, float Radius, float Duration);
    void Beam(FVector From, FVector To, FLinearColor Tint, float Width, float Duration);
    bool Trace(FVector From, FVector To, const AActor* Ignore, FHitResult& Hit) const;
    bool Walkable(FVector From, FVector To) const;
    int NearestNode(FVector Point, bool RequireLine) const;
    TArray<int> FindRoute(int From, int To) const;
    FTransform SelectSpawn(PvG::Team Side, int Slot) const;
    AActor* Mesh(UStaticMesh* Shape, UMaterialInterface* Material, const FTransform& Transform,
                 bool Solid, float Lifetime = 0.f);

    UPROPERTY() TObjectPtr<AArenaController> LocalController;
    UPROPERTY() TObjectPtr<AActor> Overview;
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TObjectPtr<UStaticMesh> Sphere;
    UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
    UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> SpellMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> Masonry;
    UPROPERTY() TObjectPtr<UMaterialInterface> RidgeMaterial;
    UPROPERTY() TObjectPtr<UStaticMesh> RidgeMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> CrystalMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> ReedsMesh;
    UPROPERTY() TObjectPtr<AActor> MeridianCore;
    UPROPERTY() TArray<TObjectPtr<AArenaSoldier>> Soldiers;
    UPROPERTY() TObjectPtr<USoundAttenuation> SpatialAudio;
    TArray<TArray<uint8>> SoundSamples;
    int NavigationRecoveries = 0;
    int Respawns = 0;
    TArray<FArenaBot> Bots;
    TArray<FArenaMissile> Missiles;
    TArray<FVector> Nodes;
    TArray<TArray<int>> Links;
    TArray<FVector> Spawns[2];
    FRandomStream Random{8119};
    FRandomStream EffectsRandom{2573};
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> MoteMeshes;
    FArenaMotePool MotePools[5];
};
