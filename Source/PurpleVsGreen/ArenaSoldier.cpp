#include "ArenaSoldier.h"

#include "ArenaMatch.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AArenaSoldier::AArenaSoldier()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessAI = EAutoPossessAI::Disabled;
    bUseControllerRotationYaw = true;
    GetCapsuleComponent()->InitCapsuleSize(34.f, 90.f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    UCharacterMovementComponent* Move = GetCharacterMovement();
    Move->MaxWalkSpeed = 590.f;
    Move->bRunPhysicsWithNoController = true;
    Move->MaxAcceleration = 3400.f;
    Move->BrakingDecelerationWalking = 2800.f;
    Move->GroundFriction = 8.f;
    Move->JumpZVelocity = 510.f;
    Move->AirControl = 0.43f;
    Move->GravityScale = 1.35f;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Eyes"));
    Camera->SetupAttachment(RootComponent);
    Camera->SetRelativeLocation(FVector(0, 0, 63));
    Camera->bUsePawnControlRotation = true;
    Camera->SetFieldOfView(96.f);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> LeftArm(
        TEXT("/Game/Battlemage/SM_CastingArm_L.SM_CastingArm_L"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> RightArm(
        TEXT("/Game/Battlemage/SM_CastingArm_R.SM_CastingArm_R"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Finger(
        TEXT("/Game/Battlemage/SM_CastingFinger.SM_CastingFinger"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Fingertip(
        TEXT("/Game/Battlemage/SM_CastingFingertip.SM_CastingFingertip"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(
        TEXT("/Game/Battlemage/SK_Battlemage.SK_Battlemage"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Surface(
        TEXT("/Game/Materials/M_Armor.M_Armor"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> GearSurface(
        TEXT("/Game/Materials/M_CastingGear.M_CastingGear"));
    ArmorMaterial = Surface.Object;
    CastingMaterial = GearSurface.Object;
    Battlemage = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Battlemage"));
    Battlemage->SetupAttachment(RootComponent);
    Battlemage->SetSkinnedAssetAndUpdate(Body.Object);
    Battlemage->SetRelativeLocation({0, 0, -90});
    Battlemage->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Battlemage->SetOwnerNoSee(true);
    Battlemage->bCastHiddenShadow = true;

    CastingHands = CreateDefaultSubobject<USceneComponent>(TEXT("FirstPersonRig"));
    CastingHands->SetupAttachment(Camera);
    auto GearPart = [&](FName Name, USceneComponent* Parent, UStaticMesh* Mesh)
    {
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Part->SetupAttachment(Parent);
        Part->SetStaticMesh(Mesh);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetOnlyOwnerSee(true);
        Part->SetCastShadow(false);
        CastingGear.Add(Part);
    };

    for (const int Side : {-1, 1})
    {
        const bool Right = Side > 0;
        const FString Prefix = Right ? TEXT("CastRight") : TEXT("CastLeft");
        USceneComponent* Wrist = CreateDefaultSubobject<USceneComponent>(*Prefix);
        Wrist->SetupAttachment(CastingHands);
        Wrist->SetRelativeLocation(FVector(Right ? 56 : 52, Side * 23, Right ? -23 : -26));
        Wrist->SetRelativeRotation(FRotator(8, Side * -18, Side * 12));
        (Right ? CastingRight : CastingLeft) = Wrist;
        GearPart(FName(Prefix + TEXT("Arm")), Wrist, Right ? RightArm.Object : LeftArm.Object);

        const FVector Knuckles[] = {{7.25, -3.55, -.25}, {8.05, -1.18, -.09},
                                    {7.85, 1.13, -.22}, {6.75, 3.27, -.48}};
        const float Length[] = {.94f, 1.10f, 1.02f, .76f};
        for (int Digit = 0; Digit < UE_ARRAY_COUNT(Knuckles); ++Digit)
        {
            const FString Name = Prefix + FString::Printf(TEXT("Finger%d"), Digit);
            USceneComponent* Knuckle = CreateDefaultSubobject<USceneComponent>(*Name);
            Knuckle->SetupAttachment(Wrist);
            Knuckle->SetRelativeLocation(Knuckles[Digit] * FVector(1., Side, 1.));
            Knuckle->SetRelativeScale3D({Length[Digit], Digit == 3 ? .82 : 1., Digit == 3 ? .83 : 1.});
            GearPart(FName(Name + TEXT("Base")), Knuckle, Finger.Object);

            USceneComponent* Hinge = CreateDefaultSubobject<USceneComponent>(*(Name + TEXT("Hinge")));
            Hinge->SetupAttachment(Knuckle);
            Hinge->SetRelativeLocation({4.03, 0, -.11});
            GearPart(FName(Name + TEXT("Tip")), Hinge, Fingertip.Object);
            Digits.Add(Knuckle);
            FingerHinges.Add(Hinge);
        }
    }
}

void AArenaSoldier::Initialize(AArenaMatch* Match, PvG::Team Side, int InSlot)
{
    Arena = Match;
    Team = Side;
    Slot = InSlot;
    SetTeamAppearance();

    if (const USkeletalMesh* Body = Cast<USkeletalMesh>(Battlemage->GetSkinnedAsset()))
    {
        const FReferenceSkeleton& Skeleton = Body->GetRefSkeleton();
        ComponentReference.SetNum(Skeleton.GetNum());
        PoseComponents.SetNum(Skeleton.GetNum());
        for (int Index = 0; Index < Skeleton.GetNum(); ++Index)
        {
            const FTransform& Local = Skeleton.GetRefBonePose()[Index];
            const int Parent = Skeleton.GetParentIndex(Index);
            ComponentReference[Index] = Parent == INDEX_NONE ? Local : Local * ComponentReference[Parent];
        }
    }
}

void AArenaSoldier::SetTeamAppearance()
{
    const FLinearColor Dye = AArenaMatch::TeamTint(Team);
    const auto Finish = [this](UMaterialInterface* Base, FLinearColor Color, float Metallic, float Roughness)
    {
        if (!Base)
            return Arena->Paint(this, Color);
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Material->SetScalarParameterValue(TEXT("Metallic"), Metallic);
        Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
        return Material;
    };
    struct FCastingFinish
    {
        FName Name;
        FLinearColor Color;
        float Metallic;
        float Roughness;
    };
    const FCastingFinish Finishes[] = {
        {TEXT("Leather"), {.106f, .056f, .028f}, .0f, .72f},
        {TEXT("Cloth"), {.058f, .067f, .074f}, .0f, .91f},
        {TEXT("Steel"), {.205f, .225f, .253f}, .62f, .56f},
        {TEXT("Bronze"), {.43f, .25f, .105f}, .62f, .49f},
        {TEXT("Livery"), Dye * .54f, .24f, .62f},
        {TEXT("Thread"), {.34f, .225f, .132f}, .0f, .84f},
    };
    for (const FCastingFinish& Entry : Finishes)
    {
        UMaterialInstanceDynamic* Material = Finish(CastingMaterial, Entry.Color, Entry.Metallic, Entry.Roughness);
        for (UStaticMeshComponent* Part : CastingGear)
        {
            if (const int Index = Part->GetMaterialIndex(Entry.Name); Index != INDEX_NONE)
                Part->SetMaterial(Index, Material);
        }
    }

    const auto Surface = [this](FName Name, UMaterialInterface* Material)
    {
        if (const int Index = Battlemage->GetMaterialIndex(Name); Index != INDEX_NONE)
            Battlemage->SetMaterial(Index, Material);
    };
    Surface(TEXT("Obsidian"), Finish(ArmorMaterial, {.19f, .21f, .23f}, .65f, .47f));
    Surface(TEXT("Bronze"), Finish(ArmorMaterial, {.43f, .25f, .105f}, .62f, .49f));
    Surface(TEXT("Livery"), Finish(ArmorMaterial, Dye * .60f, .0f, .79f));
    Surface(TEXT("Gaps"), Finish(ArmorMaterial, {.072f, .057f, .050f}, .0f, .81f));
}

void AArenaSoldier::Respawn(const FTransform& Spawn)
{
    Rules = {};
    Rules.ProtectedUntil = GetWorld()->GetTimeSeconds() + 1.15;
    SetActorTransform(Spawn, false, nullptr, ETeleportType::TeleportPhysics);
    SetAim(Spawn.Rotator());
    SetActorHiddenInGame(false);
    CastingHands->SetVisibility(true, true);
    Battlemage->SetRelativeLocation({0, 0, -90});
    Battlemage->SetRelativeRotation(FRotator::ZeroRotator);
    SetActorEnableCollision(true);
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    SetSprinting(false);
    Stride = 0.f;
    PoseCast = 0.f;
    PoseWard = 0.f;
    AirPose = 0.f;
    Landing = 0.f;
    WasAirborne = false;
    HasViewHistory = false;
    ViewSway = FVector2D::ZeroVector;
    LastCast = -10.;
    LastHit = -10.;
    LastStep = GetWorld()->GetTimeSeconds();
}

void AArenaSoldier::SetAim(FRotator Rotation)
{
    Aim = Rotation;
    SetActorRotation(FRotator(0, Aim.Yaw, 0));
    if (Controller)
        Controller->SetControlRotation(Aim);
}

FVector AArenaSoldier::Eye() const
{
    return Camera->GetComponentLocation();
}

void AArenaSoldier::SetSprinting(bool Enabled)
{
    GetCharacterMovement()->MaxWalkSpeed = Enabled ? 820.f : 590.f;
}

void AArenaSoldier::Cast(PvG::Spell Spell)
{
    if (!Arena || !Arena->IsPlaying())
        return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (!Rules.StartCast(Spell, Now, ReleaseAt))
        return;
    Rules.ProtectedUntil = Now;
    if (ReleaseAt <= Now)
        Release();
    else
        Arena->Feedback(Eye(), 4, 0.27f);
}

void AArenaSoldier::Release()
{
    Rules.Charging = false;
    LastCast = GetWorld()->GetTimeSeconds();
    Arena->Fire(this, Rules.ActiveSpell, Controller ? Controller->GetControlRotation().Vector() : Aim.Vector());
}

void AArenaSoldier::Ward()
{
    if (Arena && Arena->IsPlaying() && Rules.Ward(GetWorld()->GetTimeSeconds()))
    {
        Arena->PlayerWards += this == Arena->PlayerSoldier();
        Arena->ShowWard(this);
        Arena->Feedback(Eye(), 6);
    }
}

bool AArenaSoldier::ReceiveSpell(AArenaSoldier* Caster, float Damage)
{
    if (!Caster || !Arena->IsPlaying())
        return false;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Rules.Hit(Team, Caster->Team, Damage, Now) <= 0.f)
        return false;
    LastHit = Now;
    const FVector Inward = (GetActorLocation() - Caster->GetActorLocation()).GetSafeNormal2D();
    HitSide = FVector::DotProduct(GetActorRightVector(), Inward);
    Arena->MarkHit(Caster, this);
    if (Now < Rules.WardUntil)
        Arena->ShowWard(this);
    if (!Rules.Alive())
    {
        CastingHands->SetVisibility(false, true);
        SetActorEnableCollision(false);
        GetCharacterMovement()->DisableMovement();
        RespawnAt = LastHit + PvG::RespawnDelay;
        Arena->Defeated(this, Caster);
    }
    return true;
}

void AArenaSoldier::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Arena)
        return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (!Rules.Alive())
    {
        AnimateDefeat(Now);
        return;
    }
    if (!Arena->IsPlaying())
        return;
    Rules.Refill(DeltaSeconds);
    if (Rules.Charging && Now >= ReleaseAt)
        Release();
    Animate(DeltaSeconds, Now);
}
