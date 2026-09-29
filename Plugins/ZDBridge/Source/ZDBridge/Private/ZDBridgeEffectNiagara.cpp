// 特效面片 + Niagara 系统生成。
//
// 设计见 Docs/特效Niagara-面片与序列同步-设计.md：
// - 面片（Mesh 渲染器，不是公告板）播整条网格序列帧，和 Paper2D 那条链路共用同一张图集；
// - 帧号绑到用户参数 User.FrameIndex，由驱动方按**角色序列时间**写入，不让 Niagara 自己按寿命跑。
#include "ZDBridgeLibrary.h"

#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Factories/FbxImportUI.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Interfaces/IPluginManager.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionFloor.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NiagaraCommon.h"
#include "NiagaraEmitter.h"
#include "NiagaraMeshRendererMeshProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraTypes.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/SavePackage.h"

namespace
{
    /** 引擎里"一个面片播一整条序列帧"的现成底子；我们只把它换渲染器。 */
    const TCHAR* EffectEmitterTemplatePath =
        TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst");

    const TCHAR* FrameIndexParameterName = TEXT("User.FrameIndex");

    FString ToJson(const TSharedRef<FJsonObject>& Object)
    {
        FString Text;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
        FJsonSerializer::Serialize(Object, Writer);
        return Text;
    }

    TSharedRef<FJsonObject> MakeErrorJson(const FString& Message)
    {
        const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("ok"), false);
        Result->SetStringField(TEXT("error"), Message);
        return Result;
    }

    FString ObjectPathFor(const FString& PackagePath, const FString& AssetName)
    {
        return FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
    }

    /** 把包落盘。资产不显式保存的话，离线 commandlet 退出后不落盘（这条踩过）。 */
    bool SaveAssetPackage(UObject* Asset, FString& OutError)
    {
        if (!Asset)
        {
            OutError = TEXT("asset is null");
            return false;
        }

        UPackage* Package = Asset->GetOutermost();
        Package->MarkPackageDirty();
        const FString FileName = FPackageName::LongPackageNameToFilename(
            Package->GetName(), FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Package, Asset, *FileName, Args))
        {
            OutError = FString::Printf(TEXT("failed to save %s"), *FileName);
            return false;
        }

        FAssetRegistryModule::AssetCreated(Asset);
        return true;
    }
}

FString UZDBridgeLibrary::EnsureEffectPlaneMesh(
    const FString& SourceFbxPath,
    const FString& PackagePath,
    const FString& AssetName)
{
    // 默认落**插件自己的 Content**（`/ZDBridge/FX`）：面片是运行时资产，跟着插件走，
    // 换工程不用重新导入。插件已声明 CanContainContent + 带一个 Runtime 模块，
    // 所以这份内容在打包后的游戏里是可用的（不然编辑器专用插件的内容进不了包）。
    const FString TargetPackagePath = PackagePath.IsEmpty()
        ? TEXT("/ZDBridge/FX")
        : PackagePath;
    const FString TargetAssetName = AssetName.IsEmpty() ? TEXT("FXDefault") : AssetName;

    FString FbxPath = SourceFbxPath;
    if (FbxPath.IsEmpty())
    {
        // 插件内置的那一份：源文件跟着插件走，导入出来的资产落在工程里。
        if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ZDBridge")))
        {
            FbxPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/FX/FXDefault.fbx"));
        }
    }

    FbxPath = FPaths::ConvertRelativePathToFull(FbxPath);
    if (FbxPath.IsEmpty() || !FPaths::FileExists(FbxPath))
    {
        return ToJson(MakeErrorJson(FString::Printf(TEXT("effect plane FBX not found: %s"), *FbxPath)));
    }

    const FString ObjectPath = ObjectPathFor(TargetPackagePath, TargetAssetName);
    if (UStaticMesh* Existing = LoadObject<UStaticMesh>(nullptr, *ObjectPath))
    {
        const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("ok"), true);
        Result->SetStringField(TEXT("action"), TEXT("reused"));
        Result->SetStringField(TEXT("meshPath"), Existing->GetPathName());
        Result->SetStringField(TEXT("sourceFbx"), FbxPath);
        return ToJson(Result);
    }

    UAssetImportTask* Task = NewObject<UAssetImportTask>();
    Task->AddToRoot(); // 导入回来之前别被 GC 收走
    Task->Filename = FbxPath;
    Task->DestinationPath = TargetPackagePath;
    Task->DestinationName = TargetAssetName;
    Task->bReplaceExisting = true;
    Task->bAutomated = true;
    Task->bSave = true;

    UFbxImportUI* Options = NewObject<UFbxImportUI>(Task);
    Options->MeshTypeToImport = EFBXImportType::FBXIT_StaticMesh;
    Options->bImportAsSkeletal = false;
    Options->bImportMesh = true;
    // 面片是纯几何：材质要走我们自己的（勾了 Used with Niagara Meshes 的那张），
    // 所以这里不导入 FBX 自带的材质/贴图，免得工程里多出一堆无主资产。
    Options->bImportMaterials = false;
    Options->bImportTextures = false;
    Options->bCreatePhysicsAsset = false;
    Task->Options = Options;

    TArray<UAssetImportTask*> Tasks;
    Tasks.Add(Task);
    FAssetToolsModule::GetModule().Get().ImportAssetTasks(Tasks);

    // 先把导入出来的对象取走，再放掉 root：反过来的话中间一旦触发 GC，任务和它持有的对象都可能没了。
    const TArray<UObject*> ImportedObjects = Task->GetObjects();
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *ObjectPath);
    if (!Mesh && ImportedObjects.Num() > 0)
    {
        Mesh = Cast<UStaticMesh>(ImportedObjects[0]);
    }
    Task->RemoveFromRoot();

    if (!Mesh)
    {
        return ToJson(MakeErrorJson(FString::Printf(
            TEXT("FBX import produced no static mesh at %s"), *ObjectPath)));
    }

    FString SaveError;
    if (!SaveAssetPackage(Mesh, SaveError))
    {
        return ToJson(MakeErrorJson(SaveError));
    }

    const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("ok"), true);
    Result->SetStringField(TEXT("action"), TEXT("created"));
    Result->SetStringField(TEXT("meshPath"), Mesh->GetPathName());
    Result->SetStringField(TEXT("sourceFbx"), FbxPath);
    Result->SetStringField(TEXT("bounds"), Mesh->GetBounds().BoxExtent.ToString());
    return ToJson(Result);
}

FString UZDBridgeLibrary::EnsureEffectSheetMaterial(
    const FString& PackagePath,
    const FString& AssetName,
    bool bAdditive)
{
    if (PackagePath.IsEmpty() || AssetName.IsEmpty())
    {
        return ToJson(MakeErrorJson(TEXT("PackagePath and AssetName are required")));
    }

    const FString ObjectPath = ObjectPathFor(PackagePath, AssetName);
    if (UMaterial* Existing = LoadObject<UMaterial>(nullptr, *ObjectPath))
    {
        const TSharedRef<FJsonObject> ExistingResult = MakeShared<FJsonObject>();
        ExistingResult->SetBoolField(TEXT("ok"), true);
        ExistingResult->SetStringField(TEXT("action"), TEXT("reused"));
        ExistingResult->SetStringField(TEXT("materialPath"), Existing->GetPathName());
        return ToJson(ExistingResult);
    }

    UMaterial* Material = Cast<UMaterial>(FAssetToolsModule::GetModule().Get().CreateAsset(
        AssetName, PackagePath, UMaterial::StaticClass(), NewObject<UMaterialFactoryNew>(), NAME_None));
    if (!Material)
    {
        return ToJson(MakeErrorJson(FString::Printf(
            TEXT("failed to create material %s/%s"), *PackagePath, *AssetName)));
    }

    // 2D 特效：不光照、双面、加法或半透；三个"用作"标记必须都给上 ——
    // 漏掉对应的那个，运行时就只是一个不显示的面片（引擎侧不会报错，只会静默不画）。
    Material->SetShadingModel(MSM_Unlit);
    Material->BlendMode = bAdditive ? BLEND_Additive : BLEND_Translucent;
    Material->TwoSided = true;
    Material->bUsedWithParticleSprites = true;
    Material->bUsedWithNiagaraSprites = true;
    Material->bUsedWithNiagaraMeshParticles = true;

    UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);

    // ── 节点：把"第几帧"折算成格子 UV ─────────────────────────────────────────
    // 列 = idx % Columns；行 = floor(idx / Columns)；UV = (cell + frac(UV)) / (Columns, Rows)。
    // 全是参数，所以这一份材质可以被所有特效的 MI 覆盖 —— 系统才只需要一份。
    const auto CreateExpr = [&](UClass* ExpressionClass, int32 X, int32 Y) -> UMaterialExpression*
    {
        return UMaterialEditingLibrary::CreateMaterialExpression(Material, ExpressionClass, X, Y);
    };
    const auto Connect = [](UMaterialExpression* From, const FString& FromOutput,
                            UMaterialExpression* To, const FString& ToInput) -> bool
    {
        return From && To &&
            UMaterialEditingLibrary::ConnectMaterialExpressions(From, FromOutput, To, ToInput);
    };
    const auto CreateScalar = [&](const TCHAR* Name, float Default, int32 X, int32 Y)
        -> UMaterialExpressionScalarParameter*
    {
        auto* Node = Cast<UMaterialExpressionScalarParameter>(CreateExpr(
            UMaterialExpressionScalarParameter::StaticClass(), X, Y));
        if (Node)
        {
            Node->ParameterName = Name;
            Node->DefaultValue = Default;
        }
        return Node;
    };

    UMaterialExpressionScalarParameter* FrameIndex = CreateScalar(TEXT("FrameIndex"), 0.0f, -1400, 0);
    UMaterialExpressionScalarParameter* Columns = CreateScalar(TEXT("Columns"), 1.0f, -1400, 160);
    UMaterialExpressionScalarParameter* Rows = CreateScalar(TEXT("Rows"), 1.0f, -1400, 320);
    // Frames 暂时只用于"将来做循环/夹取"，先建出来当参数占位（MI 里也会写进去）。
    UMaterialExpressionScalarParameter* Frames = CreateScalar(TEXT("Frames"), 1.0f, -1400, 480);

    auto* Sheet = Cast<UMaterialExpressionTextureSampleParameter2D>(CreateExpr(
        UMaterialExpressionTextureSampleParameter2D::StaticClass(), -400, 400));
    if (!Sheet)
    {
        return ToJson(MakeErrorJson(TEXT("failed to create the texture parameter node")));
    }
    Sheet->ParameterName = TEXT("Sheet");
    Sheet->SamplerType = SAMPLERTYPE_Color;

    auto* TexCoord = CreateExpr(UMaterialExpressionTextureCoordinate::StaticClass(), -1400, 640);

    auto* RowDivide = CreateExpr(UMaterialExpressionDivide::StaticClass(), -1150, 0);
    Connect(FrameIndex, TEXT(""), RowDivide, TEXT("A"));
    Connect(Columns, TEXT(""), RowDivide, TEXT("B"));
    auto* Row = CreateExpr(UMaterialExpressionFloor::StaticClass(), -1000, 0);
    Connect(RowDivide, TEXT(""), Row, TEXT(""));

    auto* RowTimesColumns = CreateExpr(UMaterialExpressionMultiply::StaticClass(), -1000, 200);
    Connect(Row, TEXT(""), RowTimesColumns, TEXT("A"));
    Connect(Columns, TEXT(""), RowTimesColumns, TEXT("B"));
    auto* Column = CreateExpr(UMaterialExpressionSubtract::StaticClass(), -850, 100);
    Connect(FrameIndex, TEXT(""), Column, TEXT("A"));
    Connect(RowTimesColumns, TEXT(""), Column, TEXT("B"));

    auto* Cell = CreateExpr(UMaterialExpressionAppendVector::StaticClass(), -700, 100);
    Connect(Column, TEXT(""), Cell, TEXT("A"));
    Connect(Row, TEXT(""), Cell, TEXT("B"));

    auto* Fraction = CreateExpr(UMaterialExpressionFrac::StaticClass(), -700, 300);
    Connect(TexCoord, TEXT(""), Fraction, TEXT(""));

    auto* CellUv = CreateExpr(UMaterialExpressionAdd::StaticClass(), -550, 150);
    Connect(Cell, TEXT(""), CellUv, TEXT("A"));
    Connect(Fraction, TEXT(""), CellUv, TEXT("B"));

    auto* Grid = CreateExpr(UMaterialExpressionAppendVector::StaticClass(), -550, 420);
    Connect(Columns, TEXT(""), Grid, TEXT("A"));
    Connect(Rows, TEXT(""), Grid, TEXT("B"));

    auto* Uv = CreateExpr(UMaterialExpressionDivide::StaticClass(), -430, 250);
    Connect(CellUv, TEXT(""), Uv, TEXT("A"));
    Connect(Grid, TEXT(""), Uv, TEXT("B"));
    Connect(Uv, TEXT(""), Sheet, TEXT("Coordinates"));

    UMaterialEditingLibrary::ConnectMaterialProperty(Sheet, TEXT("RGB"), MP_EmissiveColor);
    UMaterialEditingLibrary::ConnectMaterialProperty(Sheet, TEXT("A"), MP_Opacity);
    UMaterialEditingLibrary::RecompileMaterial(Material);

    FString SaveError;
    if (!SaveAssetPackage(Material, SaveError))
    {
        return ToJson(MakeErrorJson(SaveError));
    }

    const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("ok"), true);
    Result->SetStringField(TEXT("action"), TEXT("created"));
    Result->SetStringField(TEXT("materialPath"), Material->GetPathName());
    Result->SetStringField(TEXT("blendMode"), bAdditive ? TEXT("additive") : TEXT("translucent"));
    return ToJson(Result);
}

FString UZDBridgeLibrary::CreateEffectSheetMaterialInstance(
    const FString& PackagePath,
    const FString& AssetName,
    UTexture2D* Sheet,
    int32 Columns,
    int32 Rows,
    int32 Frames)
{
    if (PackagePath.IsEmpty() || AssetName.IsEmpty() || !Sheet)
    {
        return ToJson(MakeErrorJson(TEXT("PackagePath, AssetName and Sheet are required")));
    }
    if (Columns <= 0 || Rows <= 0)
    {
        return ToJson(MakeErrorJson(FString::Printf(TEXT("invalid sub image size %dx%d"), Columns, Rows)));
    }

    // 母材质（共享的那一份）：没有就先建。路径固定 /ZDBridge/FX，跟着插件走。
    const FString MotherPath = TEXT("/ZDBridge/FX/M_FXSheet.M_FXSheet");
    UMaterial* Mother = LoadObject<UMaterial>(nullptr, *MotherPath);
    if (!Mother)
    {
        const FString MotherJson = EnsureEffectSheetMaterial(TEXT("/ZDBridge/FX"), TEXT("M_FXSheet"), true);
        Mother = LoadObject<UMaterial>(nullptr, *MotherPath);
        if (!Mother)
        {
            return ToJson(MakeErrorJson(
                FString::Printf(TEXT("母材质不可用：%s"), *MotherJson)));
        }
    }

    const FString ObjectPath = ObjectPathFor(PackagePath, AssetName);
    if (UMaterialInstanceConstant* Existing = LoadObject<UMaterialInstanceConstant>(nullptr, *ObjectPath))
    {
        const TSharedRef<FJsonObject> ExistingResult = MakeShared<FJsonObject>();
        ExistingResult->SetBoolField(TEXT("ok"), true);
        ExistingResult->SetStringField(TEXT("action"), TEXT("reused"));
        ExistingResult->SetStringField(TEXT("materialInstancePath"), Existing->GetPathName());
        return ToJson(ExistingResult);
    }

    UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
    Factory->InitialParent = Mother;
    UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(
        FAssetToolsModule::GetModule().Get().CreateAsset(
            AssetName, PackagePath, UMaterialInstanceConstant::StaticClass(), Factory, NAME_None));
    if (!Instance)
    {
        return ToJson(MakeErrorJson(FString::Printf(
            TEXT("failed to create material instance %s/%s"), *PackagePath, *AssetName)));
    }

    UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(Instance, TEXT("Sheet"), Sheet);
    UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Columns"), Columns);
    UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Rows"), Rows);
    UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(
        Instance, TEXT("Frames"), Frames > 0 ? Frames : 1);
    UMaterialEditingLibrary::UpdateMaterialInstance(Instance);

    FString SaveError;
    if (!SaveAssetPackage(Instance, SaveError))
    {
        return ToJson(MakeErrorJson(SaveError));
    }

    const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("ok"), true);
    Result->SetStringField(TEXT("action"), TEXT("created"));
    Result->SetStringField(TEXT("materialInstancePath"), Instance->GetPathName());
    Result->SetStringField(TEXT("parentPath"), Mother->GetPathName());
    Result->SetStringField(TEXT("sheetPath"), Sheet->GetPathName());
    Result->SetNumberField(TEXT("columns"), Columns);
    Result->SetNumberField(TEXT("rows"), Rows);
    Result->SetNumberField(TEXT("frames"), Frames);
    return ToJson(Result);
}

FString UZDBridgeLibrary::EnsureEffectNiagaraTemplate(
    const FString& PackagePath,
    const FString& AssetName,
    UStaticMesh* PlaneMesh,
    UMaterialInterface* Material,
    float PlaneScale)
{
    const FString TargetPath = PackagePath.IsEmpty() ? TEXT("/ZDBridge/FX") : PackagePath;
    const FString TargetName = AssetName.IsEmpty() ? TEXT("NS_FXSheet") : AssetName;
    if (!PlaneMesh)
    {
        return ToJson(MakeErrorJson(TEXT("PlaneMesh is required")));
    }

    const FString ObjectPath = ObjectPathFor(TargetPath, TargetName);
    if (UNiagaraSystem* Existing = LoadObject<UNiagaraSystem>(nullptr, *ObjectPath))
    {
        const TSharedRef<FJsonObject> ExistingResult = MakeShared<FJsonObject>();
        ExistingResult->SetBoolField(TEXT("ok"), true);
        ExistingResult->SetStringField(TEXT("action"), TEXT("reused"));
        ExistingResult->SetStringField(TEXT("systemPath"), Existing->GetPathName());
        return ToJson(ExistingResult);
    }

    IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
    UNiagaraSystem* System = Cast<UNiagaraSystem>(AssetTools.CreateAsset(
        TargetName, TargetPath, UNiagaraSystem::StaticClass(), NewObject<UNiagaraSystemFactoryNew>(), NAME_None));
    if (!System)
    {
        return ToJson(MakeErrorJson(FString::Printf(
            TEXT("failed to create Niagara system %s/%s"), *TargetPath, *TargetName)));
    }

    // 空发射器当底子（引擎的 Minimal）：里面没有任何渲染器和模块残留 ——
    // 之前复制 SimpleSpriteBurst 会带一堆用不上的模块，看起来就是"空白粒子系统 + 主发射器"。
    UObject* Template = LoadObject<UObject>(
        nullptr, TEXT("/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal"));
    if (!Template)
    {
        return ToJson(MakeErrorJson(TEXT("engine Minimal emitter template not found")));
    }

    const FString EmitterName = TargetName + TEXT("_Emitter");
    UObject* EmitterAsset = AssetTools.DuplicateAsset(EmitterName, TargetPath, Template);
    UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(EmitterAsset);
    if (!Emitter)
    {
        return ToJson(MakeErrorJson(TEXT("failed to duplicate the Minimal emitter template")));
    }

    const FGuid VersionGuid = Emitter->GetExposedVersion().VersionGuid;
    System->AddEmitterHandle(*Emitter, FName(*EmitterName), VersionGuid);

    UNiagaraMeshRendererProperties* Renderer = NewObject<UNiagaraMeshRendererProperties>(Emitter);
    // 网格由**材质**算（母材质的 Columns/Rows + FrameIndex），渲染器的 Sub UV 不参与 ——
    // 这正是"系统只需要一份、参数全在 MI 里"的前提。
    Renderer->SubImageSize = FVector2D(1.0, 1.0);

    FNiagaraMeshRendererMeshProperties MeshProperties;
    MeshProperties.Mesh = PlaneMesh;
    MeshProperties.Scale = FVector(PlaneScale);
    // 面片的原点已经在底边（用户按"和角色一样站在地上"改过 FBX），这里不动 PivotOffset。
    MeshProperties.PivotOffset = FVector::ZeroVector;
    Renderer->Meshes.Add(MeshProperties);

    if (Material)
    {
        Renderer->bOverrideMaterials = true;
        FNiagaraMeshMaterialOverride MaterialOverride;
        // 默认挂共享母材质；每个动作具体用哪张 sheet，由玩法侧把 MI 传给 User.EffectMaterial 决定。
        MaterialOverride.ExplicitMat = Material;
        Renderer->OverrideMaterials.Add(MaterialOverride);
    }

    const FNiagaraTypeDefinition FloatType = FNiagaraTypeDefinition::GetFloatDef();
    const FNiagaraVariable FrameIndexVariable(FloatType, FName(FrameIndexParameterName));
    // 帧号：既绑渲染器的 Sub Image Index（给不走参数化母材质的材质用），
    // 也当动态材质参数传给材质（母材质的 FrameIndex 参数）。
    Renderer->SubImageIndexBinding.Setup(
        FrameIndexVariable, FrameIndexVariable, ENiagaraRendererSourceDataMode::Particles);
    Renderer->DynamicMaterialBinding.Setup(
        FrameIndexVariable, FrameIndexVariable, ENiagaraRendererSourceDataMode::Particles);
    Emitter->AddRenderer(Renderer, VersionGuid);

    // 系统级用户参数：材质（每个动作一个 MI）+ 帧号。
    const FNiagaraVariable MaterialVariable(
        FNiagaraTypeDefinition(UObject::StaticClass()), FName(TEXT("User.EffectMaterial")));
    System->GetExposedParameters().AddParameter(MaterialVariable, /*bInitialize*/ true);
    System->GetExposedParameters().AddParameter(FrameIndexVariable, /*bInitialize*/ true);

    System->MarkPackageDirty();
    Emitter->MarkPackageDirty();
    FString SaveError;
    const bool bSystemSaved = SaveAssetPackage(System, SaveError);
    FString EmitterSaveError;
    const bool bEmitterSaved = SaveAssetPackage(Emitter, EmitterSaveError);

    const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("ok"), bSystemSaved && bEmitterSaved);
    Result->SetStringField(TEXT("action"), TEXT("created"));
    Result->SetStringField(TEXT("systemPath"), System->GetPathName());
    Result->SetStringField(TEXT("emitterPath"), Emitter->GetPathName());
    Result->SetStringField(TEXT("rendererClass"), Renderer->GetClass()->GetName());
    Result->SetStringField(TEXT("meshPath"), PlaneMesh->GetPathName());
    Result->SetStringField(TEXT("materialPath"), Material ? Material->GetPathName() : TEXT(""));
    Result->SetStringField(TEXT("materialParameter"), TEXT("User.EffectMaterial"));
    Result->SetStringField(TEXT("frameParameter"), FrameIndexParameterName);
    Result->SetNumberField(TEXT("planeScale"), PlaneScale);
    return ToJson(Result);
}

FString UZDBridgeLibrary::CreateEffectNiagaraSystem(
    const FString& PackagePath,
    const FString& AssetName,
    UStaticMesh* PlaneMesh,
    UMaterialInterface* Material,
    int32 Columns,
    int32 Rows,
    int32 Frames,
    float FramesPerSecond,
    float PlaneScale,
    bool bLoop)
{
    if (PackagePath.IsEmpty() || AssetName.IsEmpty())
    {
        return ToJson(MakeErrorJson(TEXT("PackagePath and AssetName are required")));
    }
    if (!PlaneMesh)
    {
        return ToJson(MakeErrorJson(TEXT("PlaneMesh is required (call EnsureEffectPlaneMesh first)")));
    }
    if (Columns <= 0 || Rows <= 0)
    {
        return ToJson(MakeErrorJson(FString::Printf(
            TEXT("invalid sub image size %dx%d"), Columns, Rows)));
    }

    const FString ObjectPath = ObjectPathFor(PackagePath, AssetName);
    const TArray<FString> Warnings;

    if (UNiagaraSystem* Existing = LoadObject<UNiagaraSystem>(nullptr, *ObjectPath))
    {
        TSharedRef<FJsonObject> ExistingResult = MakeShared<FJsonObject>();
        ExistingResult->SetBoolField(TEXT("ok"), true);
        ExistingResult->SetStringField(TEXT("action"), TEXT("reused"));
        ExistingResult->SetStringField(TEXT("systemPath"), Existing->GetPathName());
        return ToJson(ExistingResult);
    }

    IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
    UNiagaraSystem* System = Cast<UNiagaraSystem>(AssetTools.CreateAsset(
        AssetName, PackagePath, UNiagaraSystem::StaticClass(), NewObject<UNiagaraSystemFactoryNew>(), NAME_None));
    if (!System)
    {
        return ToJson(MakeErrorJson(FString::Printf(
            TEXT("failed to create Niagara system %s/%s"), *PackagePath, *AssetName)));
    }

    // 拿引擎的 "SimpleSpriteBurst" 当底子：spawn burst / initialize particle / emitter state 这些模块已经接好了，
    // 我们只把它的精灵渲染器换成面片渲染器。这样 v1 不需要动模块栈（那部分要更多编译往返）。
    const FString EmitterName = AssetName + TEXT("_Emitter");
    // 5.8 的 IAssetTools::DuplicateAsset 要的是**对象**，不是路径（旧版本才有路径重载）。
    UObject* TemplateEmitter = LoadObject<UObject>(nullptr, EffectEmitterTemplatePath);
    if (!TemplateEmitter)
    {
        return ToJson(MakeErrorJson(FString::Printf(
            TEXT("emitter template not found: %s"), EffectEmitterTemplatePath)));
    }
    UObject* EmitterAsset = AssetTools.DuplicateAsset(EmitterName, PackagePath, TemplateEmitter);
    UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(EmitterAsset);
    if (!Emitter)
    {
        return ToJson(MakeErrorJson(FString::Printf(
            TEXT("failed to duplicate emitter template %s"), EffectEmitterTemplatePath)));
    }

    const FGuid VersionGuid = Emitter->GetExposedVersion().VersionGuid;
    System->AddEmitterHandle(*Emitter, FName(*EmitterName), VersionGuid);

    // 模板自带的精灵渲染器要去掉：我们只留一个面片渲染器，免得两层都画。
    if (const FVersionedNiagaraEmitterData* EmitterData = Emitter->GetLatestEmitterData())
    {
        const TArray<UNiagaraRendererProperties*> ExistingRenderers = EmitterData->GetRenderers();
        TArray<UNiagaraRendererProperties*> ToRemove = ExistingRenderers;
        for (UNiagaraRendererProperties* Renderer : ToRemove)
        {
            Emitter->RemoveRenderer(Renderer, VersionGuid);
        }
    }

    UNiagaraMeshRendererProperties* MeshRenderer = NewObject<UNiagaraMeshRendererProperties>(Emitter);
    MeshRenderer->SubImageSize = FVector2D(static_cast<double>(Columns), static_cast<double>(Rows));
    MeshRenderer->bSubImageBlend = true;

    FNiagaraMeshRendererMeshProperties MeshProperties;
    MeshProperties.Mesh = PlaneMesh;
    MeshProperties.Scale = FVector(PlaneScale);
    // 面片的原点已经在底边（用户按"和角色一样站在地上"改过 FBX），所以这里**不动 PivotOffset**。
    MeshProperties.PivotOffset = FVector::ZeroVector;
    MeshRenderer->Meshes.Add(MeshProperties);

    if (Material)
    {
        MeshRenderer->bOverrideMaterials = true;
        FNiagaraMeshMaterialOverride MaterialOverride;
        MaterialOverride.ExplicitMat = Material;
        MeshRenderer->OverrideMaterials.Add(MaterialOverride);
    }

    // 帧号来自用户参数：驱动方按角色序列时间写 User.FrameIndex，Niagara 不自作主张推进。
    const FNiagaraTypeDefinition FloatType = FNiagaraTypeDefinition::GetFloatDef();
    const FNiagaraVariable FrameIndexVariable(FloatType, FName(FrameIndexParameterName));
    MeshRenderer->SubImageIndexBinding.Setup(
        FrameIndexVariable, FrameIndexVariable, ENiagaraRendererSourceDataMode::Particles);
    Emitter->AddRenderer(MeshRenderer, VersionGuid);

    // 用户参数要显式登记，否则渲染器绑过去的那个变量在系统里不存在。
    System->GetExposedParameters().AddParameter(FrameIndexVariable, /*bInitialize*/ true);

    System->MarkPackageDirty();
    Emitter->MarkPackageDirty();

    FString SaveError;
    const bool bSystemSaved = SaveAssetPackage(System, SaveError);
    FString EmitterSaveError;
    const bool bEmitterSaved = SaveAssetPackage(Emitter, EmitterSaveError);

    const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("ok"), bSystemSaved && bEmitterSaved);
    Result->SetStringField(TEXT("action"), TEXT("created"));
    Result->SetStringField(TEXT("systemPath"), System->GetPathName());
    Result->SetStringField(TEXT("emitterPath"), Emitter->GetPathName());
    Result->SetStringField(TEXT("rendererClass"), MeshRenderer->GetClass()->GetName());
    Result->SetStringField(
        TEXT("subImageSize"),
        FString::Printf(TEXT("%dx%d"), Columns, Rows));
    Result->SetStringField(TEXT("frameIndexBinding"), FrameIndexParameterName);
    Result->SetStringField(TEXT("meshPath"), PlaneMesh->GetPathName());
    Result->SetStringField(TEXT("materialPath"), Material ? Material->GetPathName() : TEXT(""));
    Result->SetNumberField(TEXT("frames"), Frames);
    Result->SetNumberField(TEXT("framesPerSecond"), FramesPerSecond);
    Result->SetBoolField(TEXT("loop"), bLoop);

    TArray<TSharedPtr<FJsonValue>> WarningValues;
    for (const FString& Warning : Warnings)
    {
        WarningValues.Add(MakeShared<FJsonValueString>(Warning));
    }
    if (!bSystemSaved && !SaveError.IsEmpty())
    {
        WarningValues.Add(MakeShared<FJsonValueString>(SaveError));
    }
    if (!bEmitterSaved && !EmitterSaveError.IsEmpty())
    {
        WarningValues.Add(MakeShared<FJsonValueString>(EmitterSaveError));
    }
    // v1 只把 loop 记下来：改 emitter 侧/系统侧的循环行为要动模块栈，留到下一轮。
    WarningValues.Add(MakeShared<FJsonValueString>(
        TEXT("v1: loop/lifetime 还没写进模块栈，先用模板默认值 + 由驱动方在动作结束时停组件")));
    Result->SetArrayField(TEXT("warnings"), WarningValues);
    return ToJson(Result);
}
