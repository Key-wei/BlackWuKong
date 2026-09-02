#include "Misc/AutomationTest.h"
#include "AbilitySystem/CCAttributeSet.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCCDamageFormulaTest,
    "CoreCombat.Attributes.DamageFormula",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCCDamageFormulaTest::RunTest(const FString& Parameters)
{
    // 防御为 0 时，伤害原样穿透
    TestEqual(TEXT("零防御全额承伤"),
        UCCAttributeSet::CalculateDamage(100.f, 0.f), 100.f);

    // 防御按公式 Damage * (100 / (100 + Defense)) 递减
    TestEqual(TEXT("防御 100 时伤害减半"),
        UCCAttributeSet::CalculateDamage(100.f, 100.f), 50.f);

    TestEqual(TEXT("防御 300 时伤害为四分之一"),
        UCCAttributeSet::CalculateDamage(100.f, 300.f), 25.f);

    // 伤害与原始值成正比：同一防御下换不同原始值，结果同比例缩放
    TestEqual(TEXT("伤害与原始值成正比"),
        UCCAttributeSet::CalculateDamage(50.f, 100.f), 25.f);

    // 负伤害不允许倒扣血量
    TestEqual(TEXT("负伤害钳制为 0"),
        UCCAttributeSet::CalculateDamage(-50.f, 0.f), 0.f);

    // 负防御钳制到 0，等同于零防御（SafeDefense = max(0, D)），伤害原样穿透
    TestEqual(TEXT("负防御按零防御处理"),
        UCCAttributeSet::CalculateDamage(100.f, -500.f), 100.f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCCAttributeClampTest,
    "CoreCombat.Attributes.Clamping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCCAttributeClampTest::RunTest(const FString& Parameters)
{
    // 注意：这里必须用 InitXxx() 而不是 SetXxx()。
    // GAMEPLAYATTRIBUTE_VALUE_SETTER 生成的 SetXxx() 会走
    // GetOwningAbilitySystemComponent()->SetNumericAttributeBase()，
    // 而裸 NewObject 出来的属性集没有宿主 ASC，会触发 ensure 且写入无效。
    // InitXxx() 直接写 BaseValue/CurrentValue，适合纯逻辑测试。
    UCCAttributeSet* Set = NewObject<UCCAttributeSet>();

    Set->InitMaxHealth(200.f);
    Set->InitHealth(500.f);
    Set->ClampAttributesForInit();
    TestEqual(TEXT("血量不超过上限"), Set->GetHealth(), 200.f);

    Set->InitHealth(-30.f);
    Set->ClampAttributesForInit();
    TestEqual(TEXT("血量不低于 0"), Set->GetHealth(), 0.f);

    Set->InitMaxStance(5.f);
    Set->InitStance(9.f);
    Set->ClampAttributesForInit();
    TestEqual(TEXT("棍势不超过上限"), Set->GetStance(), 5.f);

    Set->InitMaxMana(100.f);
    Set->InitMana(-10.f);
    Set->ClampAttributesForInit();
    TestEqual(TEXT("法力不低于 0"), Set->GetMana(), 0.f);

    Set->InitMaxPoise(80.f);
    Set->InitPoise(120.f);
    Set->ClampAttributesForInit();
    TestEqual(TEXT("韧性不超过上限"), Set->GetPoise(), 80.f);

    // MaxHealth 为负时，ClampAttributesForInit 应先把上限钳制到下限，
    // 再钳制当前值——结果不得出现负血量。
    Set->InitMaxHealth(-50.f);
    Set->InitHealth(50.f);
    Set->ClampAttributesForInit();
    TestTrue(TEXT("负 MaxHealth 下血量不为负"), Set->GetHealth() >= 0.f);

    return true;
}
