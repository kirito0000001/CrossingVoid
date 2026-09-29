#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MetasoundFrontendLiteral.h"
#include "ZDBridgeLibrary.generated.h"

class UPaperSprite;
class UPaperFlipbook;
class UPaperZDAnimSequence;
class UStaticMesh;
class UMaterialInterface;
class UMaterial;
class UTexture2D;
class UNiagaraSystem;

UCLASS()
class ZDBRIDGE_API UZDBridgeLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|Paper2D", CallInEditor)
    static UPaperSprite* CreatePaperSpriteFromTexture(UTexture2D* Texture, const FString& PackagePath, const FString& AssetName, FString& Error);

    UFUNCTION(BlueprintCallable, Category = "ZDBridge|Paper2D", CallInEditor)
    static UPaperFlipbook* CreatePaperFlipbookFromSprites(const TArray<UPaperSprite*>& Sprites, const TArray<int32>& FrameRuns, float FramesPerSecond, const FString& PackagePath, const FString& AssetName, FString& Error);

    UFUNCTION(BlueprintCallable, Category = "ZDBridge|PaperZD", CallInEditor)
    static UPaperZDAnimSequence* CreatePaperZDSequence(UPaperFlipbook* Flipbook, UObject* AnimationSource, const FString& PackagePath, const FString& AssetName, FString& Error);

    UFUNCTION(BlueprintCallable, Category = "ZDBridge|PaperZD", CallInEditor)
    static bool AddOrUpdateSupportedAnimation(UObject* AnimationSource, FName AnimationName, UPaperZDAnimSequence* Sequence, bool& Created, FString& Error);

    // Python-friendly wrapper: returns "created", "updated", or "existing";
    // returns an "error:" string instead of using reflected out parameters.
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|PaperZD", CallInEditor)
    static FString EnsureSequenceAnimationSource(UObject* AnimationSource, UPaperZDAnimSequence* Sequence);

    // Sets a MetaSound graph variable default on the frontend document.
    // The engine only exposes GetGraphVariableDefault to script; without a setter the toolbox
    // has to write through UMetasoundEditorGraphMemberDefault*Array::Defaults, which is declared
    // Transient and is therefore empty unless the asset was opened in the MetaSound editor.
    // Returns an empty string on success, or an "error: ..." message.
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|MetaSound", CallInEditor)
    static FString SetMetaSoundGraphVariableDefault(UObject* MetaSound, FName VariableName, const FMetasoundFrontendLiteral& DefaultLiteral);

    // Python-friendly diagnostic snapshot of all loaded PaperZD sequences bound to an AnimSource.
    // Returns a JSON string so the toolbox can compare stable asset facts without parsing editor UI state.
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|PaperZD", CallInEditor)
    static FString ScanAnimationSource(UObject* AnimationSource);

    // Deletes an action's legacy assets, clearing whatever still holds them.
    //
    // Script cannot do this reliably. EditorAssetLibrary::DeleteAsset refuses on any referencer;
    // ObjectTools::ForceDeleteObjects nulls hard references through FArchiveReplaceObjectRef but
    // cannot touch soft ones, and the project-wide sprite atlas holds every sprite through
    // TSoftObjectPtr in UPaperSpriteAtlas::AtlasSlots. Python also cannot tell a hard referencer
    // from a soft one: FindPackageReferencersForAsset reports no dependency kind.
    //
    // Deletes referencing classes before referenced ones (sequence -> flipbook -> sprite ->
    // texture), strips soft holders it knows about, force-deletes the rest, then verifies each
    // asset is really gone instead of trusting any return value.
    //
    // Returns JSON: { protocolName, protocolVersion, deletedCount, items: [ { objectPath,
    // deleted, assetClass, hardReferencers, softReferencers, action, error } ] }
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|Assets", CallInEditor)
    static FString PurgeAssets(const TArray<FString>& ObjectPaths);

    // Unbinds sequences from their PaperZD AnimationSource without touching the assets.
    //
    // PaperZD 2.2 keeps no SupportedAnimations array on the source: the editor's list is an
    // asset-registry query over sequences whose AnimSource points at it. So "removing an entry
    // from the source" means clearing that pointer on the sequence itself.
    //
    // The assets are deliberately left on disk. A sequence that drifted into the wrong source
    // usually still holds real artwork, and deleting it because it looked out of place would
    // destroy content that is merely mis-filed.
    //
    // Returns JSON: { protocolName, protocolVersion, detachedCount, items: [ { objectPath,
    // detached, assetClass, previousSource, error } ] }
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|PaperZD", CallInEditor)
    static FString DetachSequencesFromAnimationSource(const TArray<FString>& SequenceObjectPaths);

    /**
     * 按行写入数据表，只覆盖 RowJson 里出现的字段，其余字段保持原样。
     *
     * 不能用 UDataTable::FillFromJSONString 代替：FSkillData2D 自带一个 Name 属性，
     * 和数据表的行名字段同名，整表回灌会把每一行的行名覆盖成技能名字数组，
     * 27 行护援技会一次性全废。这里逐行取结构体内存、按字段覆盖、再写回。
     *
     * FText 字段会尽量沿用该位置原有的命名空间和键，只换文本；
     * 原来没有键（或是新行）时才生成新的，避免每次同步都刷掉本地化条目。
     *
     * @return JSON：{ ok, tableObjectPath, rowName, created, writtenFields[], error }
     */
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|DataTable", CallInEditor)
    static FString UpsertDataTableRow(const FString& TableObjectPath, FName RowName, const FString& RowJson);

    /**
     * 把 JSON 里出现的字段覆盖进 Owner 上某个结构体属性，其余字段保持原样。
     *
     * 角色蓝图的 SkillSlot1..3 是 FSkillData2D，里面的技能名字和介绍是 FText。
     * 走 Python 的 set_editor_property 只能塞进文化无关文本，本地化条目会被降级；
     * 这里和数据表用同一套写入逻辑，键沿用原位置的，改的只是文本。
     *
     * @return JSON：{ ok, structProperty, writtenFields[], error }
     */
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|Assets", CallInEditor)
    static FString ApplyJsonToStructProperty(UObject* Owner, FName StructPropertyName, const FString& Json);

    /**
     * 读一行数据表，返回这一行的完整 JSON。
     *
     * 不能用 UDataTable::GetTableAsJSON（Python 侧的 export_to_json_string）代替：
     * 那个导出器把行名字段（默认就叫 "Name"）当作 FieldToSkip 传给 WriteStruct，
     * 而 FSkillData2D 恰好也有一个 Name 属性，于是每一行的「技能名字」导出来
     * 永远是空数组。拿它做比对，这个字段会被永远判成待写入，写进去了也看不出变化。
     *
     * 这里用 FJsonObjectConverter 读，和 UpsertDataTableRow 的写入走同一套转换，
     * 读回来的形状和写进去的形状天然对得上。
     *
     * @return JSON：{ ok, found, rowName, row: {...}, error }
     */
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|DataTable", CallInEditor)
    static FString ReadDataTableRow(const FString& TableObjectPath, FName RowName);

    /**
     * 第 3 步「基础配置」的依赖预加载：把这一层要读的三个资产**在 C++ 侧**加载进内存。
     *
     * 为什么要搬到 C++：离线实例（`UnrealEditor-Cmd -run=pythonscript`）里，Python 的
     * `EditorAssetLibrary.load_asset` 对 WidgetBlueprint / MetaSoundSource 会返回 None ——
     * 实测同一份工程**在线 23 项 0 错、离线 4 项全报"未找到"**，而那两个资产一直都在。
     * C++ 侧加载靠的是**模块依赖**（UMGEditor / MetasoundEngine），不吃 Python 那层的行为；
     * 加载过之后 Python 再 `load_asset` 就是命中内存里的那一份。
     *
     * 它同时是**诊断**：`packageExists=true, loaded=false` 就是"资产在、这一次读不到"
     * （不该报成"工程里没有"）；两个 `has*Class` 直接说明那几个编辑器模块在不在。
     *
     * @return JSON：{ protocolName, protocolVersion, ok, loadedCount,
     *                 hasWidgetBlueprintClass, hasMetaSoundSourceClass,
     *                 items: [ { role, objectPath, packageExists, loaded, className, hasDefaultObject } ] }
     */
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|LightConfig", CallInEditor)
    static FString ResolveLightConfigurationAssets(
        const FString& ItemObjectPath, const FString& TeamSelectObjectPath, const FString& MetaSoundObjectPath);

    /**
     * 特效面片：把 FBX 导入成工程里的 UStaticMesh（已存在就直接复用）。
     *
     * SourceFbxPath 留空时会用**插件内置**的那一份（`<Plugin>/Content/FX/FXDefault.fbx`）——
     * 面片是运行时资产，所以导入到 `/Game/...` 而不是插件内容：插件是 Editor 类型，
     * 它的内容进不了打包后的游戏。源文件内置在插件里，资产按需导入，换工程也不用重做。
     *
     * @return JSON：{ ok, action:"created"|"reused", meshPath, sourceFbx, error }
     */
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|Niagara", CallInEditor)
    static FString EnsureEffectPlaneMesh(const FString& SourceFbxPath, const FString& PackagePath, const FString& AssetName);

    /**
     * 特效面片用的**共享母材质**：网格翻页的 UV 由它自己算，所以系统只需要一份。
     *
     * 参数（全部留给材质实例覆盖）：
     * - `Sheet`（Texture 参数）；
     * - `Columns` / `Rows` / `Frames`（标量）；
     * - `FrameIndex`（标量，帧号；Niagara 侧逐粒子传进来）。
     *
     * UV 算法：`列 = idx % Columns`、`行 = floor(idx / Columns)`，
     * `UV = (cell + frac(UV)) / (Columns, Rows)` —— 等分网格，和网格 sheet 一一对应。
     *
     * 为什么不让渲染器算（Sub Image Size + Sub Image Index）：那两个是**渲染器属性**，
     * 每个动作的列/行不同，系统就没法只有一份了。放材质里算，系统才真共享。
     *
     * 材质标记（Used with Niagara Mesh Particles / Niagara Sprites / Particle Sprites）必须都给上，
     * 漏一个运行时就是看不见的面片。已存在就直接复用（幂等）。
     *
     * @return JSON：{ ok, action, materialPath, blendMode, error }
     */
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|Niagara", CallInEditor)
    static FString EnsureEffectSheetMaterial(
        const FString& PackagePath,
        const FString& AssetName,
        bool bAdditive);

    /**
     * 每个动作一个材质实例：把母材质的 Sheet / Columns / Rows / Frames 填上。
     * 玩法侧 spawn 共享系统时把它传给 `User.EffectMaterial`。
     *
     * @return JSON：{ ok, action, materialInstancePath, parentPath, sheetPath, columns, rows, frames, error }
     */
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|Niagara", CallInEditor)
    static FString CreateEffectSheetMaterialInstance(
        const FString& PackagePath,
        const FString& AssetName,
        UTexture2D* Sheet,
        int32 Columns,
        int32 Rows,
        int32 Frames);

    /**
     * **共享**的特效面片系统：一个发射器 / 一个粒子 / 面片渲染器，落 `/ZDBridge/FX/NS_FXSheet`。
     *
     * 为什么只做一份：所有特效都是"一个面片播一条序列帧"，差别只在贴图和网格参数 ——
     * 那些参数都在材质实例里，所以系统本身对所有特效都一样。玩法侧
     * `SpawnSystemAttached(NS_FXSheet)` 之后把动作自己的 MI 传给 `User.EffectMaterial`，
     * 再按角色序列时间写 `User.FrameIndex` 即可。
     *
     * 底子用引擎的 `Minimal` 发射器（空发射器），不再复制 SimpleSpriteBurst ——
     * 那会带一堆用不上的模块，看起来就是个"空白粒子系统 + 主发射器"。
     *
     * @return JSON：{ ok, action, systemPath, emitterPath, rendererClass, meshPath, materialPath, error }
     */
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|Niagara", CallInEditor)
    static FString EnsureEffectNiagaraTemplate(
        const FString& PackagePath,
        const FString& AssetName,
        UStaticMesh* PlaneMesh,
        UMaterialInterface* Material,
        float PlaneScale);

    /**
     * 特效 Niagara 系统：**一个 Mesh 面片**播整条网格序列帧。
     *
     * 和 Paper2D 那条链路共用同一张网格图集：
     * - `Sub Image Size = (Columns, Rows)`，第 N 帧 = 列 `N % Columns`、行 `N / Columns`；
     * - 帧号绑到用户参数 **`User.FrameIndex`**（Float），由驱动方按角色序列时间写入
     *   （`FrameIndex = floor(AnimTime × 动作fps × 2)`）——这样角色动画暂停/倒放/循环都不会错位，
     *   而不是让 Niagara 自己按寿命跑。
     *
     * 用 Mesh 渲染器（不是公告板精灵）是因为面片要能改方向；面片默认"底边在 Z=0"，
     * 所以这里**不动 PivotOffset**。
     *
     * @return JSON：{ ok, action, systemPath, emitterPath, rendererClass, subImageSize,
     *                 frameIndexBinding, meshPath, materialPath, frames, framesPerSecond,
     *                 loop, warnings[], error }
     */
    UFUNCTION(BlueprintCallable, Category = "ZDBridge|Niagara", CallInEditor)
    static FString CreateEffectNiagaraSystem(
        const FString& PackagePath,
        const FString& AssetName,
        UStaticMesh* PlaneMesh,
        UMaterialInterface* Material,
        int32 Columns,
        int32 Rows,
        int32 Frames,
        float FramesPerSecond,
        float PlaneScale,
        bool bLoop);
};
