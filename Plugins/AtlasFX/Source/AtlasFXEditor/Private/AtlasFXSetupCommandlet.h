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

	/**
	 * 自查 PaperZD 特效预览扩展赖以工作的反射链：播放器上 private 的 RegisteredRenderComponent 能不能读到，
	 * 以及用户实际在用的序列（Misaka 的 DefAtk）里那些通知能不能解析出 Niagara 系统与 Offset/Rotation/Scale/NotAttach。
	 */
	void DumpPaperZDPreviewTargets();

	/**
	 * 可选的收尾动作：`-ReimportMesh=/ZDBridge/FX/FXDefault` —— 从源 FBX 重导一个静态网格，
	 * 并把重导前后的包围盒中心打出来。
	 *
	 * 为什么需要它：网格面片的**轴心**决定网格特效整体偏高还是居中 ——
	 * 中心 Z = 0 ⇒ 轴心在几何中心（网格版与精灵版的四边形天然对齐）；
	 * 中心 Z = 高度的一半 ⇒ 轴心在底边（面片从粒子原点往上长，整体高出一截）。
	 * 命令行重导走的是 FReimportManager，会沿用资产里存的导入设置，比 Python 的 import_asset_tasks 稳。
	 */
	void ReimportMeshAsset(const FString& MeshPath);
};
