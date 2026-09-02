#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "CCAttributeSet.generated.h"

// 模板遗留的 CoreCombatCharacter.h 也定义了同名宏，要到 Task 8 才删除。
// UBT 的 unity build 会把多个 cpp 合并进同一编译单元，两份定义可能撞车，
// 因此这里加 ifndef 保护。Task 8 之后本宏是全项目唯一定义。
#ifndef ATTRIBUTE_ACCESSORS
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
#endif

/** 属性变化广播：当前值 + 上限值，供 UI 绑定 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAttributeChangedSignature, float /*NewValue*/, float /*MaxValue*/);

/**
 * 项目属性集。
 * 伤害是唯一的元属性（Meta Attribute）：GE 只写入 Damage，
 * 真正的减防、扣血、判死全部在 PostGameplayEffectExecute 里完成，
 * 保证伤害公式全局只有一处。
 */
UCLASS()
class CORECOMBAT_API UCCAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UCCAttributeSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	/**
	 * 伤害公式的唯一实现。做成纯静态函数以便直接单测。
	 * 公式：RawDamage * (100 / (100 + Defense))，结果不小于 0。
	 */
	static float CalculateDamage(float RawDamage, float DefensePower);

	/** 把所有属性钳制回合法区间。测试与属性初始化后调用。 */
	void ClampAttributes();

	FOnAttributeChangedSignature OnHealthChanged;
	FOnAttributeChangedSignature OnManaChanged;
	FOnAttributeChangedSignature OnStanceChanged;

	// ---- 血量 ----
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vital")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vital")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, MaxHealth)

	// ---- 法力（定身术消耗）----
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vital")
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, Mana)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vital")
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, MaxMana)

	// ---- 棍势（轻攻积累、重攻消耗）----
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData Stance;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, Stance)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData MaxStance;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, MaxStance)

	// ---- 韧性（仅玩家使用，杨戬不设，见 spec §4.5）----
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData Poise;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, Poise)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData MaxPoise;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, MaxPoise)

	// ---- 伤害公式乘算项 ----
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, AttackPower)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData DefensePower;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, DefensePower)

	// ---- 元属性：只作为 GE 的传输通道，不直接显示 ----
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UCCAttributeSet, Damage)
};
