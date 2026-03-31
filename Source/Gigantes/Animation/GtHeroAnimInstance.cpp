#include "GtHeroAnimInstance.h"

#include "Gigantes/GtGameplayTags.h"
#include "Gigantes/Character/GtHeroCharacter.h"
#include "Gigantes/Character/Components/GtHeroMovementComponent.h"
#include "Gigantes/Character/Test/GtTestWeaponBase.h"
#include "Gigantes/Equipments/Components/GtLoadoutComponent.h"
#include "Gigantes/Player/GtPlayerCameraManager.h"

FGtHeroAnimInstanceProxy::FGtHeroAnimInstanceProxy(UAnimInstance* Instance)
	: FGtBaseAnimInstanceProxy(Instance) 
{
}

void FGtHeroAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FGtBaseAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	if (!InAnimInstance) return;
	
	AGtHeroCharacter* OwningHeroCharacter = Cast<AGtHeroCharacter>(InAnimInstance->GetOwningActor());
	if (!IsValid(OwningHeroCharacter)) return;
    
	// 각 카테고리별로 업데이트 함수 호출
	UpdateAimOffsetData(OwningHeroCharacter);
	UpdateMovementData(OwningHeroCharacter);
	UpdateWeaponData(OwningHeroCharacter);
	
	CachedIKDisableTags = OwningHeroCharacter->GetIKDisableTags();
	CachedCharacterMesh = OwningHeroCharacter->GetMesh();
}

FAnimInstanceProxy* UGtHeroAnimInstance::CreateAnimInstanceProxy()
{
	return new FGtHeroAnimInstanceProxy(this);
}

void UGtHeroAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

void FGtHeroAnimInstanceProxy::UpdateAimOffsetData(const AGtHeroCharacter* HeroCharacter)
{
	if (!HeroCharacter) return;
    
	APlayerController* PC = Cast<APlayerController>(HeroCharacter->GetController());
	if (!PC) return;
    
	AGtPlayerCameraManager* CameraManager = Cast<AGtPlayerCameraManager>(PC->PlayerCameraManager);
	if (CameraManager)
	{
		CachedAimOffsetYaw = CameraManager->GetAimOffsetYaw();
		CachedAimOffsetPitch = CameraManager->GetAimOffsetPitch();
	}
	else
	{
		CachedAimOffsetYaw = 0.0f;
		CachedAimOffsetPitch = 0.0f;
	}
}

void FGtHeroAnimInstanceProxy::UpdateMovementData(const AGtHeroCharacter* HeroCharacter)
{
	if (!HeroCharacter) return;
    
	UGtHeroMovementComponent* HeroMovementComponent = Cast<UGtHeroMovementComponent>(HeroCharacter->GetCharacterMovement());
	if (HeroMovementComponent)
	{
		CachedGroundDistance = HeroMovementComponent->GetGroundDistance();
	}
}

void FGtHeroAnimInstanceProxy::UpdateWeaponData(const AGtHeroCharacter* HeroCharacter)
{
	if (!HeroCharacter) return;
    
	// TEMP: 애니메이션 테스트용
	bCachedIsEquipped = HeroCharacter->bIsEquipped;
	bCachedUseAimOffset = HeroCharacter->bUseAimOffset;
	CachedJointTargetLocation = HeroCharacter->JointTargetLocation;
    
	UGtLoadoutComponent* LoadoutComponent = HeroCharacter->GetLoadoutComponent();
	if (!LoadoutComponent) return;
    
	if (LoadoutComponent->GetCurrentEquippedWeapon())
	{
		// TODO : 테스트 코드로써 추후 WeaponItem에서 가져오도록 해야 함
		AGtTestWeaponBase* TestWeapon = Cast<AGtTestWeaponBase>(LoadoutComponent->GetCurrentEquippedWeapon());
		if (TestWeapon)
		{
			CachedWeaponItemMesh = TestWeapon->GetWeaponMesh();
		}
	}
	else
	{
		CachedWeaponItemMesh.Reset();
	}
}

void UGtHeroAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	const auto& HeroAnimProxy = GetProxyOnAnyThread<FGtHeroAnimInstanceProxy>();

	UpdateMovementStates(HeroAnimProxy);
	UpdateAimOffsets(HeroAnimProxy);
	UpdateWeaponStates(HeroAnimProxy);
	UpdateIKState(HeroAnimProxy);
	UpdateHandIK(HeroAnimProxy);

	bShouldBlendCombat = bIsEquipped && bUseAimOffset;
	bShouldApplyHandIK = bIsEquipped && bIsLeftHandIKEnabled;
}

void UGtHeroAnimInstance::UpdateMovementStates(const FGtHeroAnimInstanceProxy& Proxy)
{
	bIsWallRunning = StatusTags.HasTag(GtGameplayTags::Status_Action_WallRunning);
	bIsWallRunningRight = StatusTags.HasTag(GtGameplayTags::Status_Action_WallRunning_Right);
	bIsCrouching = StatusTags.HasTag(GtGameplayTags::Status_Action_Crouching);
	bIsSliding = StatusTags.HasTag(GtGameplayTags::Status_Action_Sliding);
	
	GroundDistance = Proxy.CachedGroundDistance;
}

void UGtHeroAnimInstance::UpdateAimOffsets(const FGtHeroAnimInstanceProxy& Proxy)
{
	AimOffsetYaw = Proxy.CachedAimOffsetYaw;
	AimOffsetPitch = Proxy.CachedAimOffsetPitch;
}

void UGtHeroAnimInstance::UpdateWeaponStates(const FGtHeroAnimInstanceProxy& Proxy)
{
	bIsEquipped = Proxy.bCachedIsEquipped;
	bUseAimOffset = Proxy.bCachedUseAimOffset;
}

void UGtHeroAnimInstance::UpdateHandIK(const FGtHeroAnimInstanceProxy& Proxy)
{
	if  (bIsLeftHandIKEnabled && Proxy.bCachedIsEquipped && 
		Proxy.CachedCharacterMesh.IsValid() && Proxy.CachedWeaponItemMesh.IsValid())
	{
		// TODO : JointTargetLocation을 무기에서 관리할 것 같음
		JointTargetLocation = Proxy.CachedJointTargetLocation;
		const USkeletalMeshComponent* CharacterMesh = Proxy.CachedCharacterMesh.Get();
		const USkeletalMeshComponent* WeaponMesh = Proxy.CachedWeaponItemMesh.Get();
        
		FTransform LeftHandIKTransform = WeaponMesh->GetSocketTransform(
			"LeftHandIK", 
			ERelativeTransformSpace::RTS_World);
        
		FVector LeftHandSocketLocation = FVector::ZeroVector;
		FRotator LeftHandSocketRotation = FRotator::ZeroRotator;
        
		CharacterMesh->TransformToBoneSpace(
			"hand_r",
			LeftHandIKTransform.GetLocation(),
			LeftHandIKTransform.GetRotation().Rotator(),
			LeftHandSocketLocation,
			LeftHandSocketRotation);
        
		LeftHandTransform = FTransform(LeftHandSocketRotation, LeftHandSocketLocation);
	}
	else
	{
		LeftHandTransform = FTransform::Identity;
	}
}

void UGtHeroAnimInstance::UpdateIKState(const FGtHeroAnimInstanceProxy& Proxy)
{
	// 기본 규칙: 원거리 전투 상태인지
	const bool bShouldBeEnabledByDefault = 
		Proxy.CachedStatusTags.HasTag(GtGameplayTags::Status_Combat_Ranged) ||
		Proxy.CachedStatusTags.HasTag(GtGameplayTags::Status_Action_Aiming); 

	// 비활성화 규칙: IK를 비활성화하라는 요청 존재 확인
	const bool bIsOverriddenToDisable = !Proxy.CachedIKDisableTags.IsEmpty();

	// 최종 결정: 기본적으로 켜져야 하지만 태그 블록으로 비활성화되지 않았을 때만 최종적으로 활성화
	bIsLeftHandIKEnabled = bShouldBeEnabledByDefault && !bIsOverriddenToDisable;
}


