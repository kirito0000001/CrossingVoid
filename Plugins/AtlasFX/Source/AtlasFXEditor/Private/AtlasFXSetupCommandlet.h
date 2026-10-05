// AtlasFX 收尾命令：给两个模板打上 CrossingvoidAtlas 标签，并造出「创建 Niagara 系统」向导的分类扩展资产。
//
// 用法（**编辑器必须先关掉**，否则保存包会失败）：
//   "D:\UnrealEngine-5.8.2\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\CrossingVoid\CrossingVoid.uproject" ^
//       -run=AtlasFXSetup -stdout -FullStdOutLogOutput -unattended -nopause -nosplash

#pragma once

#include "Commandlets/Commandlet.h"

#include "AtlasFXSetupCommandlet.generated.h"

UCLASS()
class UAtlasFXSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UAtlasFXSetupCommandlet();

	//~ Begin UCommandlet interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet interface

private:
	/** 造/更新 /AtlasFX/TABC_Atlas2DWizard：bIsExtension=true、ProfileName=NiagaraWizard.System，内含 CrossingvoidAtlas 分类。 */
	bool BuildWizardConfig();

	/**
	 * 给两个模板里的渲染器补一个「Sheet」纹理槽（渲染器的 材质参数 → 纹理参数）。
	 * 有了它，用模板建系统之后**不需要材质实例** —— 直接在渲染器上选图集贴图，
	 * 引擎会自己生成 MID（NiagaraMeshRendererProperties.h:440 NeedsMIDsForMaterials）。
	 */
	bool SeedRendererMaterialParameters();

	/** 给两个模板资产写 UAT.CrossingvoidAtlas 包元数据（顺手清掉改名前的旧标签），然后保存。 */
	bool TagTemplates();

	/** 把标签推进 Asset Registry。必须排在强制重扫之后，否则重扫会用文件头重建条目、把标签冲掉。 */
	void RefreshAssetRegistryTags();
};
