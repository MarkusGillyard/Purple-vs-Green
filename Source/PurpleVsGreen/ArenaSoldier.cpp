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

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Box(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(
        TEXT("/Game/Battlemage/SK_Battlemage.SK_Battlemage"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Surface(
        TEXT("/Game/Materials/M_Armor.M_Armor"));
    ArmorMaterial = Surface.Object;
    Battlemage = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Battlemage"));
    Battlemage->SetupAttachment(RootComponent);
    Battlemage->SetSkinnedAssetAndUpdate(Body.Object);
    Battlemage->SetRelativeLocation({0, 0, -90});
    Battlemage->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Battlemage->SetOwnerNoSee(true);
    Battlemage->bCastHiddenShadow = true;

    CastingHands = CreateDefaultSubobject<USceneComponent>(TEXT("FirstPersonRig"));
    CastingHands->SetupAttachment(Camera);
    auto GauntletPart = [&](FName Name, USceneComponent* Parent, UStaticMesh* Mesh, FVector Location,
                            FVector Scale, FRotator Angle, bool Dyed)
    {
        UStaticMeshComponent* Shape = AddPart(Name, Parent, Mesh, Location, Scale);
        Shape->SetRelativeRotation(Angle);
        Shape->SetOnlyOwnerSee(true);
        Shape->SetCastShadow(false);
        (Dyed ? Livery : Armor).Add(Shape);
    };
    for (const int Side : {-1, 1})
    {
        const bool Right = Side > 0;
        const FString Prefix = Right ? TEXT("CastRight") : TEXT("CastLeft");
        USceneComponent* Wrist = CreateDefaultSubobject<USceneComponent>(*Prefix);
        Wrist->SetupAttachment(CastingHands);
        Wrist->SetRelativeLocation(FVector(Right ? 61 : 55, Side * 23, Right ? -23 : -25));
        Wrist->SetRelativeRotation(FRotator(8, Side * -18, Side * 12));
        (Right ? CastingRight : CastingLeft) = Wrist;

        auto Segment = [&](const TCHAR* Label, UStaticMesh* Mesh, FVector Location, FVector Scale,
                           FRotator Angle, bool Dyed)
        {
            GauntletPart(FName(Prefix + Label), Wrist, Mesh, Location, Scale, Angle, Dyed);
        };
        Segment(TEXT("Sleeve"), Cylinder.Object, {-18, 0, -1}, {.153, .158, .46}, {90, 0, 0}, false);
        Segment(TEXT("Cuff"), Cylinder.Object, {-5, 0, -.1}, {.157, .162, .071}, {90, 0, 0}, true);
        Segment(TEXT("Palm"), Sphere.Object, {4.8, 0, 0}, {.179, .163, .090}, {0, 0, 0}, false);
        Segment(TEXT("Plate"), Box.Object, {4, 0, 3.8}, {.122, .117, .018}, {0, 0, 0}, true);
        Segment(TEXT("Seal"), Box.Object, {5, 0, 5.1}, {.040, .040, .016}, {0, 45, 0}, false);
        Segment(TEXT("Thumb"), Sphere.Object, {7.0, Side * -7.0, -1.5}, {.089, .043, .046}, {5, Side * -28.f, 0}, false);
        for (int Digit = 0; Digit < 4; ++Digit)
        {
            const float Length[] = {.091f, .116f, .103f, .077f};
            const FString Name = FString::Printf(TEXT("Finger%d"), Digit);
            Segment(*Name, Sphere.Object, {13.5, Side * (-4.7 + Digit * 3.06), -.8},
                {Length[Digit], .029, .037}, {15, Side * (-4.f + Digit * 4.f), 0}, false);
        }
    }
}

UStaticMeshComponent* AArenaSoldier::AddPart(FName Name, USceneComponent* Parent, UStaticMesh* Mesh,
                                           FVector Location, FVector Scale)
{
    UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
    Part->SetupAttachment(Parent);
    Part->SetStaticMesh(Mesh);
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Part->SetRelativeLocation(Location);
    Part->SetRelativeScale3D(Scale);
    return Part;
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
        ComponentReferenceRotations.SetNum(Skeleton.GetNum());
        for (int Index = 0; Index < Skeleton.GetNum(); ++Index)
        {
            const FQuat Local = Skeleton.GetRefBonePose()[Index].GetRotation();
            const int Parent = Skeleton.GetParentIndex(Index);
            ComponentReferenceRotations[Index] = Parent == INDEX_NONE ? Local : ComponentReferenceRotations[Parent] * Local;
        }
    }
}

void AArenaSoldier::SetTeamAppearance()
{
    UMaterialInstanceDynamic* Steel = Arena->Paint(this, {0.092f, 0.118f, 0.147f});
    UMaterialInstanceDynamic* Tint = Arena->Paint(this, AArenaMatch::TeamTint(Team));
    for (UStaticMeshComponent* Part : Armor)
        Part->SetMaterial(0, Steel);
    for (UStaticMeshComponent* Part : Livery)
        Part->SetMaterial(0, Tint);

    if (!ArmorMaterial)
        return;
    const auto Surface = [this](FName MaterialSlot, FLinearColor Color, float Metallic, float Roughness)
    {
        if (const int Index = Battlemage->GetMaterialIndex(MaterialSlot); Index != INDEX_NONE)
        {
            UMaterialInstanceDynamic* Finish = UMaterialInstanceDynamic::Create(ArmorMaterial, Battlemage);
            Finish->SetVectorParameterValue(TEXT("Color"), Color);
            Finish->SetScalarParameterValue(TEXT("Metallic"), Metallic);
            Finish->SetScalarParameterValue(TEXT("Roughness"), Roughness);
            Battlemage->SetMaterial(Index, Finish);
        }
    };
    Surface(TEXT("Obsidian"), {.105f, .14f, .183f}, .36f, .48f);
    Surface(TEXT("Bronze"), {.57f, .32f, .10f}, .63f, .41f);
    Surface(TEXT("Livery"), AArenaMatch::TeamTint(Team), .06f, .69f);
    Surface(TEXT("Gaps"), {.033f, .041f, .056f}, .04f, .78f);
}

void AArenaSoldier::Respawn(const FTransform& Spawn)
{
    Rules = {};
    Rules.ProtectedUntil = GetWorld()->GetTimeSeconds() + 1.15;
    SetActorTransform(Spawn, false, nullptr, ETeleportType::TeleportPhysics);
    SetAim(Spawn.Rotator());
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    SetSprinting(false);
    Stride = 0.f;
    PoseCast = 0.f;
    PoseWard = 0.f;
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
    Arena->MarkHit(Caster, this);
    if (Now < Rules.WardUntil)
        Arena->ShowWard(this);
    if (!Rules.Alive())
    {
        SetActorHiddenInGame(true);
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
    if (!Arena || !Arena->IsPlaying() || !Rules.Alive())
        return;
    const double Now = GetWorld()->GetTimeSeconds();
    Rules.Refill(DeltaSeconds);
    if (Rules.Charging && Now >= ReleaseAt)
        Release();
    Animate(DeltaSeconds, Now);
}

void AArenaSoldier::Animate(float DeltaSeconds, double Now)
{
    const float Speed = GetVelocity().Size2D();
    const float Pace = FMath::Clamp(Speed / 590.f, 0.f, 1.f);
    Stride = FMath::Fmod(Stride + DeltaSeconds * (2.f + Speed / 62.f), 2.f * UE_PI);
    const float Swing = 31.f * Pace * FMath::Sin(Stride);
    const float CastWeight = Rules.Charging ? 1.f : FMath::Clamp(1.f - static_cast<float>(Now - LastCast) * 5.f, 0.f, 1.f);
    const float HitWeight = FMath::Clamp(1.f - static_cast<float>(Now - LastHit) * 8.f, 0.f, 1.f);
    const float TargetPitch = Controller ? Controller->GetControlRotation().Pitch : Aim.Pitch;
    PoseCast = FMath::FInterpTo(PoseCast, CastWeight, DeltaSeconds, 18.f);
    PoseWard = FMath::FInterpTo(PoseWard, Now < Rules.WardUntil ? 1.f : 0.f, DeltaSeconds, 15.f);
    UpdateArmorPose(Speed, Swing, PoseCast, PoseWard, HitWeight, FRotator::NormalizeAxis(TargetPitch));
    const float Lift = FMath::Cos(Stride * 2.f) * Pace;
    if (Speed > 150.f && !GetCharacterMovement()->IsFalling() && Now >= LastStep)
    {
        Arena->Feedback(GetActorLocation(), 8, .22f);
        LastStep = Now + FMath::GetMappedRangeValueClamped(FVector2D(150., 820.), FVector2D(.47, .27), Speed);
    }
    const float WardRaise = PoseWard * 7.f;
    CastingHands->SetRelativeLocation({CastWeight * 12.f, Pace * FMath::Sin(Stride) * 1.3f, Lift * 1.1f + CastWeight * 6.f + WardRaise});
    CastingHands->SetRelativeRotation(FRotator(-CastWeight * 6.f + HitWeight * 3.f, 0, Lift * .6f));
    const float Opening = Rules.Charging ? 17.f : 0.f;
    const float Fan = Rules.ActiveSpell == PvG::Spell::Fan ? CastWeight * 9.f : 0.f;
    const float Lance = Rules.ActiveSpell == PvG::Spell::Lance ? CastWeight * 9.f : 0.f;
    CastingRight->SetRelativeRotation(FRotator(8.f + Opening + Fan, -18.f + CastWeight * 11.f, 12.f));
    CastingLeft->SetRelativeRotation(FRotator(8.f + WardRaise * 3.5f + Lance, 18.f - Lance, -12.f));
    Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView,
        Rules.Charging && Rules.ActiveSpell == PvG::Spell::Lance ? 74.f : (Speed > 635.f ? 102.f : 96.f), DeltaSeconds, 10.f));
}

void AArenaSoldier::UpdateArmorPose(float Speed, float Swing, float Casting, float Ward, float Hit, float AimPitch)
{
    const USkeletalMesh* Body = Cast<USkeletalMesh>(Battlemage->GetSkinnedAsset());
    if (!Body || Battlemage->BoneSpaceTransforms.Num() != ComponentReferenceRotations.Num())
        return;

    const FReferenceSkeleton& Skeleton = Body->GetRefSkeleton();
    auto Rotate = [&](FName Name, FVector Axis, float Degrees)
    {
        const int Bone = Skeleton.FindBoneIndex(Name);
        if (Bone == INDEX_NONE)
            return;
        const FQuat Reference = Skeleton.GetRefBonePose()[Bone].GetRotation();
        const FVector LocalAxis = ComponentReferenceRotations[Bone].UnrotateVector(Axis);
        Battlemage->BoneSpaceTransforms[Bone].SetRotation(Reference * FQuat(LocalAxis, FMath::DegreesToRadians(Degrees)));
    };
    const FVector Pitch = FVector::RightVector;
    const bool Airborne = GetCharacterMovement()->IsFalling();
    const float Run = FMath::Min(Speed / 590.f, 1.f);
    Rotate(TEXT("Pelvis"), Pitch, Run * 5.f);
    Rotate(TEXT("Chest"), Pitch, Run * 6.f - Hit * 15.f);
    Rotate(TEXT("Head"), Pitch, -AimPitch * .61f + Hit * 10.f);
    Rotate(TEXT("UpperArm_R"), Pitch, FMath::Lerp(Swing * -.68f - 13.f, -71.f - AimPitch * .65f, Casting));
    Rotate(TEXT("UpperArm_L"), Pitch, Swing * .68f - 14.f - 50.f * Ward);
    Rotate(TEXT("Forearm_R"), Pitch, -18.f - 24.f * Casting);
    Rotate(TEXT("Forearm_L"), Pitch, -23.f - 54.f * Ward);
    Rotate(TEXT("Thigh_R"), Pitch, Airborne ? -28.f : Swing);
    Rotate(TEXT("Thigh_L"), Pitch, Airborne ? 11.f : -Swing);
    Rotate(TEXT("Shin_R"), Pitch, Airborne ? 43.f : 7.f + FMath::Max(0.f, -Swing) * .77f);
    Rotate(TEXT("Shin_L"), Pitch, Airborne ? 39.f : 7.f + FMath::Max(0.f, Swing) * .77f);
    Battlemage->MarkRefreshTransformDirty();
}
