#include "ArenaMatch.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
constexpr int MotesPerColor = 46;
const FTransform HiddenMote(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
}

void AArenaMatch::BuildMotes()
{
    AActor* Emitter = GetWorld()->SpawnActor<AActor>();
    USceneComponent* Origin = NewObject<USceneComponent>(Emitter);
    Emitter->SetRootComponent(Origin);
    Origin->RegisterComponent();
    for (int Index = 0; Index < UE_ARRAY_COUNT(MotePools); ++Index)
    {
        UInstancedStaticMeshComponent* Batch = NewObject<UInstancedStaticMeshComponent>(Emitter);
        Batch->SetupAttachment(Origin);
        Batch->SetStaticMesh(CrystalMesh ? CrystalMesh.Get() : Sphere.Get());
        const FLinearColor Tint = Index < PvG::Index(PvG::Spell::Count) ?
            SpellTint(static_cast<PvG::Spell>(Index)) : FLinearColor(.22f, .69f, .82f);
        Batch->SetMaterial(0, Paint(Emitter, Tint * .81f, true));
        Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Batch->SetCastShadow(false);
        Batch->SetCanEverAffectNavigation(false);
        Batch->RegisterComponent();
        MoteMeshes.Add(Batch);

        FArenaMotePool& Pool = MotePools[Index];
        Pool.Items.SetNum(MotesPerColor);
        Pool.Transforms.Init(HiddenMote, MotesPerColor);
        Batch->AddInstances(Pool.Transforms, false, false, false);
    }
}

void AArenaMatch::EmitMotes(int Batch, FVector Location, FVector Axis, int Count, float Spread, float Speed)
{
    FArenaMotePool& Pool = MotePools[Batch];
    for (int Index = 0; Index < Count; ++Index)
    {
        FArenaMote& Mote = Pool.Items[Pool.Next];
        Pool.Next = (Pool.Next + 1) % MotesPerColor;
        const FVector Heading = EffectsRandom.VRandCone(Axis, Spread);
        Mote.Position = Location + EffectsRandom.VRand() * 8.;
        Mote.Velocity = Heading * EffectsRandom.FRandRange(Speed * .25f, Speed);
        Mote.Age = 0.f;
        Mote.Lifetime = EffectsRandom.FRandRange(.13f, .39f);
        Mote.Size = EffectsRandom.FRandRange(.012f, .038f);
    }
}

void AArenaMatch::UpdateMotes(float DeltaSeconds)
{
    for (int Batch = 0; Batch < MoteMeshes.Num(); ++Batch)
    {
        FArenaMotePool& Pool = MotePools[Batch];
        bool Visible = false;
        for (int Index = 0; Index < MotesPerColor; ++Index)
        {
            FArenaMote& Mote = Pool.Items[Index];
            Mote.Age += DeltaSeconds;
            if (Mote.Age >= Mote.Lifetime)
            {
                Pool.Transforms[Index] = HiddenMote;
                continue;
            }
            Visible = true;
            Mote.Position += Mote.Velocity * DeltaSeconds;
            Mote.Velocity.Z -= 137. * DeltaSeconds;
            const float Scale = Mote.Size * (1.f - .9f * Mote.Age / Mote.Lifetime);
            const FQuat Facing = FQuat::FindBetweenNormals(FVector::UpVector, Mote.Velocity.GetSafeNormal());
            Pool.Transforms[Index] = FTransform(Facing, Mote.Position, FVector(Scale, Scale, Scale * 1.8f));
        }

        if (Visible || Pool.Visible)
            MoteMeshes[Batch]->BatchUpdateInstancesTransforms(0, Pool.Transforms, false, true, true);
        Pool.Visible = Visible;
    }
}
