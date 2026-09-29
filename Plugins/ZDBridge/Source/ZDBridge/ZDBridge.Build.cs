using UnrealBuildTool;

public class ZDBridge : ModuleRules
{
    public ZDBridge(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Paper2D",
            "PaperZD",
            // 公共头里用到 FMetasoundFrontendLiteral，必须是 Public 依赖。
            "MetasoundFrontend"
        });
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "AssetTools",
            "UnrealEd",
            "Paper2DEditor",
            "PaperZDEditor",
            // WidgetBlueprint 的类住在 UMGEditor：不依赖它，离线（commandlet）实例里
            // UI_TeamSelect 这类资产就加载不出来 —— 第 3 步「基础配置」要读它的 CDO
            // （见 ZDBridgeLibrary.h 的 ResolveLightConfigurationAssets）。
            "UMGEditor",
            "AssetRegistry",
            "Json",
            "JsonUtilities",
            "MetasoundEngine",
            "MetasoundEditor",
            // 特效面片 + Niagara 系统生成（见 Docs/特效Niagara-面片与序列同步-设计.md）。
            // Niagara 是运行时模块（渲染器/系统资产），NiagaraEditor 提供 UNiagaraSystemFactoryNew 等编辑器侧构造入口。
            "Niagara",
            "NiagaraCore",
            "NiagaraEditor",
            // IPluginManager（找插件内置的面片 FBX）住在 Projects 模块里。
            "Projects",
            // UMaterialEditingLibrary（建 SubUV 材质）住在 MaterialEditor 模块里。
            "MaterialEditor"
        });
    }
}
