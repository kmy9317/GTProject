#pragma once

#include "CoreMinimal.h"
#include "GtBaseAnimInstance.h"
#include "GtHeroAnimInstance.generated.h"

class AGtWeaponItem;
class AGtHeroCharacter;

struct FGtHeroAnimInstanceProxy : public FGtBaseAnimInstanceProxy
{
	FGtHeroAnimInstanceProxy(UAnimInstance* Instance);
	
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	
	float CachedAimOffsetYaw;
	float CachedAimOffsetPitch;
	float CachedGroundDistance;

	// TEMP: 애니메이션 테스트용
	bool bCachedIsEquipped;
	bool bCachedUseAimOffset;

	FGameplayTagContainer CachedIKDisableTags;

	FVector CachedJointTargetLocation;
	TWeakObjectPtr<USkeletalMeshComponent> CachedWeaponItemMesh;
	TWeakObjectPtr<USkeletalMeshComponent> CachedCharacterMesh;

private:
	void UpdateAimOffsetData(const AGtHeroCharacter* HeroCharacter);
	void UpdateMovementData(const AGtHeroCharacter* HeroCharacter);
	void UpdateWeaponData(const AGtHeroCharacter* HeroCharacter);
};

UCLASS()
class GIGANTES_API UGtHeroAnimInstance : public UGtBaseAnimInstance
{
	GENERATED_BODY()

protected:
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	void UpdateMovementStates(const FGtHeroAnimInstanceProxy& Proxy);
	void UpdateAimOffsets(const FGtHeroAnimInstanceProxy& Proxy);
	void UpdateWeaponStates(const FGtHeroAnimInstanceProxy& Proxy);
	void UpdateHandIK(const FGtHeroAnimInstanceProxy& Proxy);
	void UpdateIKState(const FGtHeroAnimInstanceProxy& Proxy);
	
public:
	UPROPERTY(BlueprintReadOnly, Category = "AimOffset")
	float AimOffsetPitch;
	
	UPROPERTY(BlueprintReadOnly, Category = "AimOffset")
	float AimOffsetYaw;

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	float GroundDistance;
	
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	bool bIsWallRunning;
    
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	bool bIsWallRunningRight;  // true = 오른쪽, false = 왼쪽

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	bool bIsCrouching;

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	bool bIsSliding;

	// TEMP: 애니메이션 테스트용
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	bool bIsEquipped;

	// TEMP: 애니메이션 테스트용
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	bool bUseAimOffset;

	// IK가 최종적으로 활성화되었는지 여부를 나타내는 변수
	UPROPERTY(BlueprintReadOnly, Category = "HandBoneIK")
	bool bIsLeftHandIKEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "BlendControl")
	bool bShouldBlendCombat = false;

	UPROPERTY(BlueprintReadOnly, Category = "BlendControl")
	bool bShouldApplyHandIK = false;

	UPROPERTY(BlueprintReadOnly, Category = "HandBoneIK")
	FVector JointTargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "HandBoneIK")
	FTransform LeftHandTransform = FTransform::Identity;
};
