#include "Misc/AutomationTest.h"
#include "AbilitySystem/CCGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCCGameplayTagsTest,
    "CoreCombat.Tags.NativeTagsRegistered",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCCGameplayTagsTest::RunTest(const FString& Parameters)
{
    // FNativeGameplayTag 本身没有 IsValid()，须先 GetTag() 取出 FGameplayTag
    TestTrue(TEXT("State.Attacking 已注册"), CCTags::State_Attacking.GetTag().IsValid());
    TestTrue(TEXT("State.Dead 已注册"), CCTags::State_Dead.GetTag().IsValid());
    TestTrue(TEXT("Buff.PerfectDodgeFollowUp 已注册"), CCTags::Buff_PerfectDodgeFollowUp.GetTag().IsValid());
    TestTrue(TEXT("Data.Damage 已注册"), CCTags::Data_Damage.GetTag().IsValid());

    TestEqual(TEXT("State.Attacking 名称正确"),
        CCTags::State_Attacking.GetTag().GetTagName(), FName("State.Attacking"));

    return true;
}
