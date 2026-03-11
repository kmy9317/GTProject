// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GtHumanBase.h"
#include "GtHeroCharacter.generated.h"

class AGtTestWeaponBase;
struct FInputActionValue;

class AGtWeaponItem;
class UGtLoadoutComponent;
class UGtItemManagerComponent;
class UGtHeroMovementComponent;
class UCameraComponent;
class USpringArmComponent;
class UGtInputConfig;

UCLASS()
class GIGANTES_API AGtHeroCharacter : public AGtHumanBase
{
	GENERATED_BODY()

public:
	AGtHeroCharacter(const FObjectInitializer & ObjectInitializer = FObjectInitializer::Get());

	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;
	virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnJumped_Implementation() override;

	void IncrementJumpCount() { JumpCount++; }

	/**
	 * Crouch 관련 함수들
	 */
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual bool CanCrouch() const override;

	UFUNCTION(BlueprintCallable, Category = "Character")
	virtual void Sprint();
    
	UFUNCTION(BlueprintCallable, Category = "Character")
	virtual void UnSprint();

	void OnStartSprint();
	void OnEndSprint();
	
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	UGtHeroMovementComponent* GetHeroMovementComponent() const { return HeroMovementComponent; }
	UGtLoadoutComponent* GetLoadoutComponent() const { return LoadoutComponent; }
	TSubclassOf<UAnimInstance> GetUnarmedAnimLayer() const { return UnarmedAnimLayer; }

	// IK 비활성화를 요청하는 태그를 추가
	void AddIKDisableTag(const FGameplayTag& DisableTag);
	// IK 비활성화 요청이 끝난 태그를 제거
	void RemoveIKDisableTag(const FGameplayTag& DisableTag);
	// 현재 IK 비활성화 태그들을 반환
	const FGameplayTagContainer& GetIKDisableTags() const { return IKDisableTags; }


	// UI용 탄약 정보 헬퍼 함수
	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	int32 GetCurrentWeaponAmmo() const;
    
	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	int32 GetCurrentWeaponMaxAmmo() const;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void Die() override;
	
	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Look(const FInputActionValue& InputActionValue);
	void Input_Jump(const FInputActionValue& InputActionValue);
	void Input_Crouch(const FInputActionValue& InputActionValue);
	void Input_SprintStart(const FInputActionValue& InputActionValue);
	void Input_SprintStop(const FInputActionValue& InputActionValue);
	void Input_PrimaryActionPressed(const FInputActionValue& InputActionValue);
	void Input_PrimaryActionReleased(const FInputActionValue& InputActionValue);
	void Input_SecondaryActionPressed(const FInputActionValue& InputActionValue);
	void Input_SecondaryActionReleased(const FInputActionValue& InputActionValue);
	void Input_Reload(const FInputActionValue& InputActionValue);

	// 슬롯 제어를 위한 입력 핸들러
	void Input_EquipSlot1(const FInputActionValue& InputActionValue);
	void Input_EquipSlot2(const FInputActionValue& InputActionValue);
	void Input_UseGrenadeSlot(const FInputActionValue& InputActionValue);
	void Input_UseConsumableSlot(const FInputActionValue& InputActionValue);

	UFUNCTION()
	void OnLandedCallback(const FHitResult& Hit);
	
	UFUNCTION()
	void OnCharacterStatusTagChanged(const FGameplayTag& StatusTag, bool bAdded);

	UFUNCTION()
	void OnEquipmentChanged(AGtTestWeaponBase* NewWeapon);

	// [추가]
	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	
	bool CanPerformAction() const;
	
private:

	/**
	 * MovementComponent 델리게이트 핸들러
	 */
	void HandleCapsuleSizeChanged(float HalfHeightAdjust, float ScaledHalfHeightAdjust);

	void UpdateAimOffsetState();

	// 조준 상태를 업데이트하는 중앙 함수
	void TryUpdateAimingState();

	// 캐릭터가 현재 조준할 수 있는 상태인지 확인하는 헬퍼 함수
	bool CanAim() const;

public:
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	uint8 bIsSprinting : 1;

	// TEMP: 애니메이션 테스트용
	UPROPERTY(BlueprintReadWrite, Category = "Character|Test")
	bool bIsEquipped = false;

	// TEMP: 애니메이션 테스트용
	UPROPERTY(BlueprintReadWrite, Category = "Character|Test")
	bool bUseAimOffset = false;

	FVector JointTargetLocation = FVector::ZeroVector;
	
protected:
	UPROPERTY()
	TObjectPtr<UGtHeroMovementComponent> HeroMovementComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ItemManager")
	TObjectPtr<UGtItemManagerComponent> ItemManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loadout")
	TObjectPtr<UGtLoadoutComponent> LoadoutComponent;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UGtInputConfig> InputConfigDataAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> UnarmedAnimLayer;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump")
	int32 MaxJumpCount = 2;
	
	int32 JumpCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation|IK")
	FGameplayTagContainer IKDisableTags;

	bool bAimInputHeld = false;
};
