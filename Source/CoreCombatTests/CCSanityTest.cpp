#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCCSanityTest,
    "CoreCombat.Sanity.ModuleLoads",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCCSanityTest::RunTest(const FString& Parameters)
{
    // 验证测试模块到主模块的依赖真的接上了：
    // 若 CoreCombatTests.Build.cs 的依赖配置有误，主模块不会被加载。
    TestTrue(TEXT("CoreCombat 主模块已加载"),
        FModuleManager::Get().IsModuleLoaded(TEXT("CoreCombat")));

    TestTrue(TEXT("CoreCombatTests 测试模块已加载"),
        FModuleManager::Get().IsModuleLoaded(TEXT("CoreCombatTests")));

    return true;
}
