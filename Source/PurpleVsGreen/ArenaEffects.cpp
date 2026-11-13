#include "ArenaMatch.h"
#include "ArenaSoldier.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"

namespace
{
constexpr int MotesPerColor = 46;
constexpr int WardFacets = 12;
constexpr int WardSlots = 6;
const FLinearColor WardTint(.20f, .67f, .81f);
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
    for (int Slot = 0; Slot < WardSlots; ++Slot)
    {
        UInstancedStaticMeshComponent* Batch = NewObject<UInstancedStaticMeshComponent>(Emitter);
        Batch->SetupAttachment(Origin);
        Batch->SetStaticMesh(CrystalMesh ? CrystalMesh.Get() : Sphere.Get());
        Batch->SetMaterial(0, Paint(Emitter, WardTint, true));
        Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Batch->SetCastShadow(false);
        Batch->SetCanEverAffectNavigation(false);
        Batch->RegisterComponent();
        TArray<FTransform> Facets;
        Facets.Init(HiddenMote, WardFacets);
        Batch->AddInstances(Facets, false, false, false);
        Batch->SetVisibility(false);
        WardMeshes.Add(Batch);
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

void AArenaMatch::ClearSpellEffects()
{
    for (UInstancedStaticMeshComponent* Ward : WardMeshes)
        Ward->SetVisibility(false);

    for (FArenaMotePool& Pool : MotePools)
        for (FArenaMote& Item : Pool.Items)
            Item.Lifetime = 0.f;
    UpdateMotes(0.f);
}

void AArenaMatch::ShowCharge(const AArenaSoldier* Soldier, PvG::Spell Spell, float Strength)
{
    const FVector Palm = Soldier->SpellOrigin();
    const FVector Forward = Soldier->GetActorForwardVector();
    const FVector Gather = Palm + Forward * (18. - Strength * 10.);
    EmitMotes(PvG::Index(Spell), Gather, -Forward, Spell == PvG::Spell::Lance ? 3 : 2,
        .48f, 38.f + Strength * 50.f);
}

void AArenaMatch::DrawSpellTrace(PvG::Spell Spell, FVector From, FVector To)
{
    const FLinearColor Tint = SpellTint(Spell);
    if (Spell == PvG::Spell::Lance)
    {
        Beam(From, To, Tint * .90f, 3.2f, .09f);
        Beam(From, To, FLinearColor(.34f, .42f, .84f), 6.2f, .035f);
    }
    else if (Spell == PvG::Spell::Fan)
    {
        Beam(From, To, Tint * .81f, 2.55f, .091f);
    }
    else
    {
        Beam(From, To, Tint * .94f, 1.7f, .063f);
    }
}

void AArenaMatch::ShowSpellImpact(PvG::Spell Spell, FVector Location, FVector Normal)
{
    const FLinearColor Tint = SpellTint(Spell);
    const FVector Origin = Location + Normal * 3.;
    if (Spell == PvG::Spell::Comet)
    {
        Spark(Location, Tint * .66f, 24.f, .094f);
        EmitMotes(PvG::Index(Spell), Origin, Normal, 22, 1.83f, 460.f);
        if (CometBurst && PlayerSoldier() &&
            FVector::DistSquared(PlayerSoldier()->Eye(), Location) < FMath::Square(3500.))
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, CometBurst, Origin,
                FQuat::FindBetweenNormals(FVector::UpVector, Normal).Rotator(), FVector(.50),
                true, true, ENCPoolMethod::AutoRelease);
        }
        return;
    }

    const bool Lance = Spell == PvG::Spell::Lance;
    Spark(Origin, Tint, Lance ? 7.f : 4.6f, .077f);
    EmitMotes(PvG::Index(Spell), Origin, Normal, Lance ? 12 : 6, Lance ? .44f : 1.13f,
        Lance ? 295.f : 195.f);
}

void AArenaMatch::ShowWard(const AArenaSoldier* Soldier)
{
    EmitMotes(4, Soldier->SpellOrigin(true), FVector::UpVector, 9, 1.12f, 115.f);
}

void AArenaMatch::UpdateWard(const AArenaSoldier* Soldier, double Now)
{
    UInstancedStaticMeshComponent* Mesh = WardMeshes[Soldier->Slot];
    const double TimeLeft = Soldier->Rules.WardUntil - Now;
    if (!Soldier->Rules.Alive() || TimeLeft <= 0.)
    {
        if (Mesh->IsVisible())
        {
            if (Soldier->Rules.Alive())
                EmitMotes(4, Soldier->GetActorLocation() + FVector(0, 0, 38), FVector::UpVector, 7, 1.9f, 89.f);
            Mesh->SetVisibility(false);
        }
        return;
    }

    Mesh->SetVisibility(true);
    const double Fade = FMath::Clamp(FMath::Min(TimeLeft / .24, (1.65 - TimeLeft) / .13), .0, 1.);
    const bool FirstPerson = Soldier == PlayerSoldier();
    const FVector Center = Soldier->GetActorLocation() + FVector(0, 0, FirstPerson ? 24. : 31.);
    const double FacetSize = FirstPerson ? .65 : 1.;
    TArray<FTransform, TInlineAllocator<WardFacets>> Facets;
    for (int Index = 0; Index < WardFacets; ++Index)
    {
        const double Angle = Index * UE_TWO_PI / WardFacets + Now * 1.12;
        const FVector Radial(FMath::Cos(Angle), FMath::Sin(Angle), 0.);
        const FVector Position = Center + Radial * (51. + 8. * Fade) + FVector(0, 0, 16. * FMath::Sin(Angle * 2.));
        Facets.Emplace(FRotator(0, FMath::RadiansToDegrees(Angle), 32.), Position,
            FVector(.043, .061, .15) * (Fade * FacetSize));
    }
    Mesh->BatchUpdateInstancesTransforms(0, MakeArrayView(Facets), false, true, true);
}
