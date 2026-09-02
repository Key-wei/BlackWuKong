#include "AbilitySystem/CCAttributeSet.h"

#include "AbilitySystem/CCGameplayTags.h"
#include "GameplayEffectExtension.h"

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

	return FMath::Max(0.f, RawDamage * Mitigation);
}

void UCCAttributeSet::ClampAttributes()
{
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

		if (GetHealth() <= 0.f)
		{
			Data.Target.AddLooseGameplayTag(CCTags::State_Dead.GetTag());
		}
		return;
	}

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
		OnHealthChanged.Broadcast(GetHealth(), GetMaxHealth());
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
