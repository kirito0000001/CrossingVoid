using UnrealBuildTool;

/// <summary>
/// 空壳运行时模块。存在的唯一理由：让**这个插件**在打包后的游戏里也算"被启用"。
///
/// 只带一个 Editor 模块的插件是编辑器专用插件 —— 它在游戏里不加载，于是 `Content/` 里的资产
/// （特效面片、SubUV 母材质、共享 Niagara 系统）没人引用、也不会进包。
/// 加了这个 Runtime 模块之后，插件的内容才是真正的"运行时资产"。
/// </summary>
public class ZDBridgeRuntime : ModuleRules
{
    public ZDBridgeRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });
    }
}
