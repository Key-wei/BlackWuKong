#include "AbilitySystem/CCAttributeSet.h"

#include "AbilitySystem/CCGameplayTags.h"
#include "GameplayEffectExtension.h"

namespace
{
	/**
	 * Max 类属性的下限：为 0 或负数会让 FMath::Clamp 反向钳制出负的当前值。
	 *
	 * 注意：因为有这个下限，**不能**用 MaxPoise = 0 表示"该角色没有韧性"
	 * （会静默变成 1 点韧性、挨一下就破）。无韧性由 UCCCharacterInitData 的
	 * bUsesPoise 开关表示，见 spec §4.5（杨戬不被普通攻击打断）。
	 */
	constexpr float MinMaxAttributeValue = 1.f;
}

UCCAttributeSet::UCCAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitMana(100.f);
	InitMaxMana(100.f);
	InitStance(0.f);
	InitMaxStance(4.f);
	InitPoise(100.f);
	InitMaxPoise(100.f);
	InitAttackPower(10.f);
	InitDefensePower(0.f);
	InitDamage(0.f);
}

float UCCAttributeSet::CalculateDamage(float RawDamage, float DefensePower)
{
	if (RawDamage <= 0.f)
	{
		return 0.f;
	}

	// 防御钳制到非负，避免负防御放大伤害或触发除零
	const float SafeDefense = FMath::Max(0.f, DefensePower);
	const float Mitigation = 100.f / (100.f + SafeDefense);

	return RawDamage * Mitigation;
}

void UCCAttributeSet::ClampAttributesForInit()
{
	// 先把 Max 类属性钳制到下限，防止后续 Clamp(Current, 0, Max) 因负上限输出负值
	InitMaxHealth(FMath::Max(MinMaxAttributeValue, GetMaxHealth()));
	InitMaxMana(FMath::Max(MinMaxAttributeValue, GetMaxMana()));
	InitMaxStance(FMath::Max(MinMaxAttributeValue, GetMaxStance()));
	InitMaxPoise(FMath::Max(MinMaxAttributeValue, GetMaxPoise()));

	// 用 InitXxx 而非 SetXxx：InitXxx 直接写 Base/Current 值，
	// 不依赖宿主 ASC，因此在无宿主的单元测试里同样有效。
	InitHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	InitMana(FMath::Clamp(GetMana(), 0.f, GetMaxMana()));
	InitStance(FMath::Clamp(GetStance(), 0.f, GetMaxStance()));
	InitPoise(FMath::Clamp(GetPoise(), 0.f, GetMaxPoise()));
}

void UCCAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetManaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxMana());
	}
	else if (Attribute == GetStanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStance());
	}
	else if (Attribute == GetPoiseAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxPoise());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(MinMaxAttributeValue, NewValue);
	}
	else if (Attribute == GetMaxManaAttribute())
	{
		NewValue = FMath::Max(MinMaxAttributeValue, NewValue);
	}
	else if (Attribute == GetMaxStanceAttribute())
	{
		NewValue = FMath::Max(MinMaxAttributeValue, NewValue);
	}
	else if (Attribute == GetMaxPoiseAttribute())
	{
		NewValue = FMath::Max(MinMaxAttributeValue, NewValue);
	}
}

void UCCAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 伤害是元属性：在这里一次性完成减防、扣血、判死，然后清零。
	// 这是全项目唯一的伤害结算入口。
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float RawDamage = GetDamage();
		SetDamage(0.f);

		if (RawDamage <= 0.f)
		{
			return;
		}

		const float FinalDamage = CalculateDamage(RawDamage, GetDefensePower());
		SetHealth(FMath::Clamp(GetHealth() - FinalDamage, 0.f, GetMaxHealth()));

		OnHealthChanged.Broadcast(GetHealth(), GetMaxHealth());

		ApplyDeathTagIfNeeded(Data.Target);
		return;
	}

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
		OnHealthChanged.Broadcast(GetHealth(), GetMaxHealth());
		ApplyDeathTagIfNeeded(Data.Target);
	}
	else if (Data.EvaluatedData.Attribute == GetManaAttribute())
	{
		SetMana(FMath::Clamp(GetMana(), 0.f, GetMaxMana()));
		OnManaChanged.Broadcast(GetMana(), GetMaxMana());
	}
	else if (Data.EvaluatedData.Attribute == GetStanceAttribute())
	{
		SetStance(FMath::Clamp(GetStance(), 0.f, GetMaxStance()));
		OnStanceChanged.Broadcast(GetStance(), GetMaxStance());
	}
	else if (Data.EvaluatedData.Attribute == GetPoiseAttribute())
	{
		SetPoise(FMath::Clamp(GetPoise(), 0.f, GetMaxPoise()));
	}
}

void UCCAttributeSet::ApplyDeathTagIfNeeded(UAbilitySystemComponent& Target)
{
	// AddLooseGameplayTag 是引用计数的：同一个标记多次添加需要同等次数移除才能清除。
	// 用 HasMatchingGameplayTag 保证至多添加一次，避免多段伤害（连击尾帧、DoT、AoE 重叠）
	// 把计数推高，导致复活/重生后标记无法清除。
	if (GetHealth() <= 0.f && !Target.HasMatchingGameplayTag(CCTags::State_Dead.GetTag()))
	{
		Target.AddLooseGameplayTag(CCTags::State_Dead.GetTag());
	}
}
