// AtlasFX —— 精灵图集 × Niagara
//
// 注意（来自勘察，必须守住）：
//   * 自定义 DataInterface 必须放在插件的 **Runtime** 模块里。放进 Editor 模块时编辑器一切正常，
//     但打包后的游戏加载不到该模块，DI 无法实例化 / 反序列化。
//   * 该 DI 只需要 CPU sim，所以暂时不依赖 shader 相关的模块布局（第二步做 GPU 时再加
//     Shaders/ 目录 + AddShaderSourceDirectoryMapping("/Plugin/AtlasFX", ...)）。

using UnrealBuildTool;

public class AtlasFX : ModuleRules
{
	public AtlasFX(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Niagara",
			"NiagaraCore",
			"VectorVM",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
			"RHI",
			"Paper2D",
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
