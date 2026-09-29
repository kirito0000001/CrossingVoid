#include "ZDBridgeLibrary.h"

#include "AssetToolsModule.h"
#include "Engine/Blueprint.h"
#include "Factories/Factory.h"
#include "Misc/PackageName.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "PaperSpriteFactory.h"
#include "PaperFlipbookFactory.h"
#include "AnimSequences/PaperZDAnimSequence.h"
#include "AnimSequences/PaperZDAnimSequence_Flipbook.h"
#include "AnimSequences/Sources/PaperZDAnimationSource.h"
#include "AnimSequences/PaperZDFlipbookAnimDataSource.h"
#include "UObject/UnrealType.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Editor.h"
#include "MetasoundBuilderBase.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundEditorSubsystem.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "ObjectTools.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/DataTable.h"
#include "JsonObjectConverter.h"
#include "Internationalization/Text.h"
#include "Internationalization/TextPackageNamespaceUtil.h"


namespace
{
    UObject* CreateAsset(const FString& PackagePath, const FString& AssetName, UClass* Class, UFactory* Factory, FString& Error)
    {
        if (!Class || !Factory)
        {
            Error = TEXT("asset class or factory is unavailable");
            return nullptr;
        }
        UObject* Existing = StaticLoadObject(Class, nullptr, *(PackagePath / AssetName + TEXT(".") + AssetName));
        if (Existing)
        {
            return Existing;
        }
        UObject* Asset = FAssetToolsModule::GetModule().Get().CreateAsset(
            AssetName, PackagePath, Class, Factory, NAME_None);
        if (!Asset)
        {
            Error = FString::Printf(TEXT("failed to create %s/%s"), *PackagePath, *AssetName);
        }
        return Asset;
    }

    bool SetProperty(UObject* Object, const FName Name, const void* Value)
    {
        if (FProperty* Property = Object ? Object->GetClass()->FindPropertyByName(Name) : nullptr)
        {
            if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
            {
                ObjectProperty->SetObjectPropertyValue_InContainer(Object, *static_cast<UObject* const*>(Value));
                return true;
            }
        }
        return false;
    }
}

UPaperSprite* UZDBridgeLibrary::CreatePaperSpriteFromTexture(UTexture2D* Texture, const FString& PackagePath, const FString& AssetName, FString& Error)
{
    if (!Texture)
    {
        Error = TEXT("texture is null");
        return nullptr;
    }
    UPaperSpriteFactory* Factory = NewObject<UPaperSpriteFactory>();
    Factory->InitialTexture = Texture;
    UPaperSprite* Sprite = Cast<UPaperSprite>(CreateAsset(PackagePath, AssetName, UPaperSprite::StaticClass(), Factory, Error));
    if (Sprite)
    {
        Sprite->MarkPackageDirty();
    }
    return Sprite;
}

UPaperFlipbook* UZDBridgeLibrary::CreatePaperFlipbookFromSprites(const TArray<UPaperSprite*>& Sprites, const TArray<int32>& FrameRuns, float FramesPerSecond, const FString& PackagePath, const FString& AssetName, FString& Error)
{
    UPaperFlipbookFactory* Factory = NewObject<UPaperFlipbookFactory>();
    UPaperFlipbook* Flipbook = Cast<UPaperFlipbook>(CreateAsset(PackagePath, AssetName, UPaperFlipbook::StaticClass(), Factory, Error));
    if (!Flipbook)
    {
        return nullptr;
    }
    TArray<FPaperFlipbookKeyFrame> KeyFrames;
    for (int32 Index = 0; Index < Sprites.Num(); ++Index)
    {
        FPaperFlipbookKeyFrame& Key = KeyFrames.AddDefaulted_GetRef();
        Key.Sprite = Sprites[Index];
        Key.FrameRun = FrameRuns.IsValidIndex(Index) ? FMath::Max(1, FrameRuns[Index]) : 1;
    }
    Flipbook->Modify();
    if (FFloatProperty* FpsProperty = FindFProperty<FFloatProperty>(Flipbook->GetClass(), TEXT("FramesPerSecond")))
    {
        FpsProperty->SetPropertyValue_InContainer(Flipbook, FMath::Max(0.01f, FramesPerSecond));
    }
    if (FArrayProperty* FramesProperty = FindFProperty<FArrayProperty>(Flipbook->GetClass(), TEXT("KeyFrames")))
    {
        FScriptArrayHelper Helper(FramesProperty, FramesProperty->ContainerPtrToValuePtr<void>(Flipbook));
        Helper.EmptyAndAddValues(KeyFrames.Num());
        for (int32 Index = 0; Index < KeyFrames.Num(); ++Index)
        {
            FramesProperty->Inner->CopyCompleteValue(Helper.GetRawPtr(Index), &KeyFrames[Index]);
        }
    }
    Flipbook->MarkPackageDirty();
    return Flipbook;
}

UPaperZDAnimSequence* UZDBridgeLibrary::CreatePaperZDSequence(UPaperFlipbook* Flipbook, UObject* AnimationSource, const FString& PackagePath, const FString& AssetName, FString& Error)
{
    if (!Flipbook)
    {
        Error = TEXT("flipbook is null");
        return nullptr;
    }
    if (!AnimationSource)
    {
        Error = TEXT("animation source is null; PaperZD sequence creation requires an AnimMaps source");
        return nullptr;
    }
    UClass* SequenceClass = LoadClass<UPaperZDAnimSequence>(nullptr, TEXT("/Script/PaperZD.PaperZDAnimSequence_Flipbook"));
    UClass* FactoryClass = LoadClass<UFactory>(nullptr, TEXT("/Script/PaperZDEditor.PaperZDAnimSequenceFactory"));
    UFactory* Factory = FactoryClass ? NewObject<UFactory>(GetTransientPackage(), FactoryClass) : nullptr;
    if (!Factory || !SequenceClass)
    {
        Error = TEXT("PaperZD sequence class or factory is unavailable");
        return nullptr;
    }
    if (AnimationSource)
    {
        FProperty* Target = Factory->GetClass()->FindPropertyByName(TEXT("TargetAnimSource"));
        if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Target))
        {
            ObjectProperty->SetObjectPropertyValue_InContainer(Factory, AnimationSource);
        }
    }
    UPaperZDAnimSequence* Sequence = Cast<UPaperZDAnimSequence>(CreateAsset(PackagePath, AssetName, SequenceClass, Factory, Error));
    UPaperZDAnimSequence_Flipbook* FlipbookSequence = Cast<UPaperZDAnimSequence_Flipbook>(Sequence);
    if (!FlipbookSequence)
    {
        Error = TEXT("created asset is not a PaperZD flipbook sequence");
        return nullptr;
    }
    FlipbookSequence->Modify();
    if (FArrayProperty* AnimDataProperty = FindFProperty<FArrayProperty>(FlipbookSequence->GetClass(), TEXT("AnimData")))
    {
        FScriptArrayHelper Helper(AnimDataProperty, AnimDataProperty->ContainerPtrToValuePtr<void>(FlipbookSequence));
        Helper.EmptyAndAddValues(1);
        if (FStructProperty* StructProperty = CastField<FStructProperty>(AnimDataProperty->Inner))
        {
            if (FObjectProperty* AnimationProperty = FindFProperty<FObjectProperty>(StructProperty->Struct, TEXT("Animation")))
            {
                AnimationProperty->SetObjectPropertyValue_InContainer(Helper.GetRawPtr(0), Flipbook);
            }
        }
    }
    FlipbookSequence->MarkPackageDirty();
    return Sequence;
}

bool UZDBridgeLibrary::AddOrUpdateSupportedAnimation(UObject* AnimationSource, FName AnimationName, UPaperZDAnimSequence* Sequence, bool& Created, FString& Error)
{
    Created = false;
    if (!AnimationSource || !Sequence)
    {
        Error = TEXT("animation source or sequence is null");
        return false;
    }

    // PaperZD 2.2 does not store a SupportedAnimations array on the
    // AnimationSource. The editor's Supported Animations list is an asset
    // browser filtered by the AnimSource asset-registry tag. Creating a
    // sequence with TargetAnimSource set is therefore the native '+' action.
    UPaperZDAnimationSource* Source = Cast<UPaperZDAnimationSource>(AnimationSource);
    if (!Source)
    {
        Error = FString::Printf(TEXT("object is not a PaperZD AnimationSource: %s"), *AnimationSource->GetClass()->GetName());
        return false;
    }
    UE_LOG(LogTemp, Log, TEXT("ZDBridge AddOrUpdateSupportedAnimation: source=%s sequence=%s current=%s"), *Source->GetPathName(), *Sequence->GetPathName(), Sequence->GetAnimSource() ? *Sequence->GetAnimSource()->GetPathName() : TEXT("<none>"));
    if (Sequence->GetAnimSource() != Source)
    {
        Sequence->Modify();
        Sequence->SetAnimSource(Source);
        if (Sequence->GetAnimSource() != Source)
        {
            Error = TEXT("PaperZD sequence AnimSource could not be assigned");
            return false;
        }
        Sequence->MarkPackageDirty();
        Created = true;
    }
    return true;
}


FString UZDBridgeLibrary::SetMetaSoundGraphVariableDefault(UObject* MetaSound, FName VariableName, const FMetasoundFrontendLiteral& DefaultLiteral)
{
    if (!MetaSound)
    {
        return TEXT("error: MetaSound is null");
    }

    TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface(MetaSound);
    if (!DocumentInterface)
    {
        return FString::Printf(TEXT("error: object is not a MetaSound document: %s"), *MetaSound->GetClass()->GetName());
    }

    UMetaSoundEditorSubsystem* EditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UMetaSoundEditorSubsystem>() : nullptr;
    if (!EditorSubsystem)
    {
        return TEXT("error: MetaSound editor subsystem is unavailable");
    }

    EMetaSoundBuilderResult BuilderResult = EMetaSoundBuilderResult::Failed;
    UMetaSoundBuilderBase* Builder = EditorSubsystem->FindOrBeginBuilding(DocumentInterface, BuilderResult);
    if (!Builder || BuilderResult != EMetaSoundBuilderResult::Succeeded)
    {
        return FString::Printf(TEXT("error: no document builder for %s"), *MetaSound->GetPathName());
    }

    MetaSound->Modify();
    if (!Builder->GetBuilder().SetGraphVariableDefault(VariableName, DefaultLiteral))
    {
        return FString::Printf(TEXT("error: graph variable is missing or not writable: %s"), *VariableName.ToString());
    }

    MetaSound->MarkPackageDirty();
    UE_LOG(LogTemp, Log, TEXT("ZDBridge SetMetaSoundGraphVariableDefault: %s.%s"), *MetaSound->GetPathName(), *VariableName.ToString());
    return FString();
}

FString UZDBridgeLibrary::ScanAnimationSource(UObject* AnimationSource)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Sequences;
    int32 ExcludedGeneratedSequenceCount = 0;
    if (!AnimationSource)
    {
        Root->SetStringField(TEXT("error"), TEXT("animation source is null"));
    }
    else
    {
        UPaperZDAnimationSource* Source = Cast<UPaperZDAnimationSource>(AnimationSource);
        if (!Source)
        {
            Root->SetStringField(TEXT("error"), TEXT("object is not a PaperZD AnimationSource"));
        }
        else
        {
            Root->SetStringField(TEXT("protocolName"), TEXT("ZDBridge.AnimationSourceScan"));
            Root->SetNumberField(TEXT("protocolVersion"), 3);
            Root->SetStringField(TEXT("animationSource"), Source->GetPathName());
            FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
            TArray<FAssetData> Assets;
            AssetRegistryModule.Get().GetAssetsByClass(UPaperZDAnimSequence::StaticClass()->GetClassPathName(), Assets, true);
            for (const FAssetData& Asset : Assets)
            {
                UPaperZDAnimSequence* Sequence = Cast<UPaperZDAnimSequence>(Asset.GetAsset());
                if (!Sequence || Sequence->GetAnimSource() != Source)
                {
                    continue;
                }
                // AnimSequences 是工具箱同步生成的正式序列目录，不能整体排除。
                // 之前把该目录当成编辑器辅助资产排除，导致同步后复扫永远看不到 Sk1。
                TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
                Item->SetStringField(TEXT("assetPath"), Sequence->GetPathName());
                Item->SetStringField(TEXT("assetClass"), Sequence->GetClass()->GetName());
                Item->SetStringField(TEXT("name"), Sequence->GetName());
                if (const UPaperZDAnimSequence_Flipbook* FlipbookSequence = Cast<UPaperZDAnimSequence_Flipbook>(Sequence))
                {
                    const TArray<FPaperZDFlipbookAnimDataSource>& Data = FlipbookSequence->GetAnimDataSource();
                    Item->SetNumberField(TEXT("dataSourceCount"), Data.Num());
                    if (Data.Num() > 0 && Data[0].Animation)
                    {
                        const UPaperFlipbook* Flipbook = Data[0].Animation;
                        Item->SetStringField(TEXT("flipbookPath"), Flipbook->GetPathName());
                        Item->SetNumberField(TEXT("frameCount"), Flipbook->GetNumKeyFrames());
                        Item->SetNumberField(TEXT("framesPerSecond"), Flipbook->GetFramesPerSecond());
                        TArray<TSharedPtr<FJsonValue>> Frames;
                        for (int32 FrameIndex = 0; FrameIndex < Flipbook->GetNumKeyFrames(); ++FrameIndex)
                        {
                            const FPaperFlipbookKeyFrame& KeyFrame = Flipbook->GetKeyFrameChecked(FrameIndex);
                            TSharedRef<FJsonObject> Frame = MakeShared<FJsonObject>();
                            Frame->SetNumberField(TEXT("index"), FrameIndex + 1);
                            Frame->SetNumberField(TEXT("frameRun"), KeyFrame.FrameRun);
                            if (KeyFrame.Sprite)
                            {
                                Frame->SetStringField(TEXT("spritePath"), KeyFrame.Sprite->GetPathName());
                                if (KeyFrame.Sprite->GetSourceTexture())
                                {
                                    Frame->SetStringField(TEXT("texturePath"), KeyFrame.Sprite->GetSourceTexture()->GetPathName());
                                }
                            }
                            Frames.Add(MakeShared<FJsonValueObject>(Frame));
                        }
                        Item->SetArrayField(TEXT("frames"), Frames);
                    }
                }
                Sequences.Add(MakeShared<FJsonValueObject>(Item));
            }
            Root->SetArrayField(TEXT("sequences"), Sequences);
            Root->SetNumberField(TEXT("sequenceCount"), Sequences.Num());
            Root->SetNumberField(TEXT("excludedGeneratedSequenceCount"), ExcludedGeneratedSequenceCount);
        }
    }
    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}

FString UZDBridgeLibrary::EnsureSequenceAnimationSource(UObject* AnimationSource, UPaperZDAnimSequence* Sequence)
{
    if (!AnimationSource || !Sequence)
    {
        return TEXT("error: animation source or sequence is null");
    }
    UPaperZDAnimationSource* Source = Cast<UPaperZDAnimationSource>(AnimationSource);
    if (!Source)
    {
        return FString::Printf(TEXT("error: object is not a PaperZD AnimationSource: %s"), *AnimationSource->GetClass()->GetName());
    }
    const bool bWasBound = Sequence->GetAnimSource() == Source;
    if (!bWasBound)
    {
        Sequence->Modify();
        Sequence->SetAnimSource(Source);
        if (Sequence->GetAnimSource() != Source)
        {
            return TEXT("error: PaperZD sequence AnimSource could not be assigned");
        }
        Sequence->MarkPackageDirty();
        return TEXT("created");
    }
    return TEXT("existing");
}


namespace
{
    /// 引用方先删、被引用方后删：序列 -> Flipbook -> Sprite -> 贴图。
    /// 反过来删，Unreal 会因为还有人引用而拒绝，留下一地孤儿。
    int32 PurgeRankOf(const UObject* Object)
    {
        if (!Object)
        {
            return 5;
        }
        const FString ClassName = Object->GetClass()->GetName();
        if (ClassName.Contains(TEXT("PaperZDAnimSequence"))) return 0;
        if (ClassName.Contains(TEXT("PaperFlipbook")))       return 1;
        if (ClassName.Contains(TEXT("PaperSprite")))         return 2;
        if (ClassName.Contains(TEXT("Texture")))             return 3;
        return 4;
    }

    FName PurgePackageNameOf(const FString& ObjectPath)
    {
        return FName(*FPackageName::ObjectPathToPackageName(ObjectPath));
    }

    bool PurgeAssetIsGone(const FString& ObjectPath)
    {
        // 只认事实：内存里没有、盘上也没有，才算删掉了。
        return FindObject<UObject>(nullptr, *ObjectPath) == nullptr &&
               !FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(ObjectPath));
    }

    void PurgeCollectReferencers(
        IAssetRegistry& AssetRegistry,
        FName PackageName,
        TArray<FName>& OutHard,
        TArray<FName>& OutSoft)
    {
        using namespace UE::AssetRegistry;
        AssetRegistry.GetReferencers(PackageName, OutHard, EDependencyCategory::Package, EDependencyQuery::Hard);
        AssetRegistry.GetReferencers(PackageName, OutSoft, EDependencyCategory::Package, EDependencyQuery::Soft);
        OutHard.Remove(PackageName);
        OutSoft.Remove(PackageName);
    }
}

FString UZDBridgeLibrary::PurgeAssets(const TArray<FString>& ObjectPaths)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("protocolName"), TEXT("ZDBridge.PurgeAssets"));
    Root->SetNumberField(TEXT("protocolVersion"), 1);

    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

    struct FPurgeTarget
    {
        FString ObjectPath;
        UObject* Object = nullptr;
        FString AssetClass;
        int32 Rank = 5;
        bool bDeleted = false;
        FString Action;
        FString Error;
        TArray<FName> Hard;
        TArray<FName> Soft;
    };

    TArray<FPurgeTarget> Targets;
    for (const FString& ObjectPath : ObjectPaths)
    {
        if (ObjectPath.IsEmpty())
        {
            continue;
        }

        FPurgeTarget Target;
        Target.ObjectPath = ObjectPath;
        if (PurgeAssetIsGone(ObjectPath))
        {
            Target.bDeleted = true;
            Target.Action = TEXT("already-absent");
            Targets.Add(MoveTemp(Target));
            continue;
        }

        Target.Object = LoadObject<UObject>(nullptr, *ObjectPath);
        Target.AssetClass = Target.Object ? Target.Object->GetClass()->GetName() : FString();
        Target.Rank = PurgeRankOf(Target.Object);
        PurgeCollectReferencers(AssetRegistry, PurgePackageNameOf(ObjectPath), Target.Hard, Target.Soft);
        Targets.Add(MoveTemp(Target));
    }

    // 引用方排在被引用方前面。
    Targets.Sort([](const FPurgeTarget& A, const FPurgeTarget& B)
    {
        return A.Rank != B.Rank ? A.Rank < B.Rank : A.ObjectPath < B.ObjectPath;
    });

    // ForceDeleteObjects 一遍删不干净。实测：8 个一批只删掉 7 个，
    // 剩下那个单独重试一次就成功了 —— 尤其是源贴图已经不在的孤儿 Sprite。
    // 所以多跑几遍，直到某一遍不再有进展为止。
    auto CountRemaining = [&Targets]()
    {
        int32 Remaining = 0;
        for (const FPurgeTarget& Target : Targets)
        {
            if (!Target.bDeleted && !PurgeAssetIsGone(Target.ObjectPath))
            {
                ++Remaining;
            }
        }
        return Remaining;
    };

    const int32 MaxPasses = 3;
    for (int32 Pass = 0; Pass < MaxPasses; ++Pass)
    {
        TArray<UObject*> Pending;
        for (const FPurgeTarget& Target : Targets)
        {
            if (Target.bDeleted || PurgeAssetIsGone(Target.ObjectPath))
            {
                continue;
            }

            // 每遍都重新解析：上一遍可能已经把对象销毁了，缓存的指针不能再碰。
            if (UObject* Object = LoadObject<UObject>(nullptr, *Target.ObjectPath))
            {
                Pending.AddUnique(Object);
            }
        }

        if (Pending.Num() == 0)
        {
            break;
        }

        const int32 BeforePass = CountRemaining();
        // 硬引用交给 ForceDeleteObjects 置空；bShowConfirmation=false 保持无人值守。
        ObjectTools::ForceDeleteObjects(Pending, false);
        CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

        if (CountRemaining() >= BeforePass)
        {
            // 这一遍毫无进展，再试也是白费。
            break;
        }
    }

    int32 DeletedCount = 0;
    TArray<TSharedPtr<FJsonValue>> Items;
    for (FPurgeTarget& Target : Targets)
    {
        if (!Target.bDeleted)
        {
            // 不信任何返回值，只复核资产是不是真的没了。
            Target.bDeleted = PurgeAssetIsGone(Target.ObjectPath);
            Target.Action = Target.bDeleted ? TEXT("force-deleted") : TEXT("failed");
            if (!Target.bDeleted)
            {
                Target.Hard.Reset();
                Target.Soft.Reset();
                PurgeCollectReferencers(AssetRegistry, PurgePackageNameOf(Target.ObjectPath), Target.Hard, Target.Soft);
                Target.Error = TEXT("still present after force delete");
            }
        }

        if (Target.bDeleted)
        {
            ++DeletedCount;
        }

        TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("objectPath"), Target.ObjectPath);
        Item->SetBoolField(TEXT("deleted"), Target.bDeleted);
        Item->SetStringField(TEXT("assetClass"), Target.AssetClass);
        Item->SetStringField(TEXT("action"), Target.Action);
        Item->SetStringField(TEXT("error"), Target.Error);

        TArray<TSharedPtr<FJsonValue>> HardValues;
        for (const FName& Name : Target.Hard)
        {
            HardValues.Add(MakeShared<FJsonValueString>(Name.ToString()));
        }
        TArray<TSharedPtr<FJsonValue>> SoftValues;
        for (const FName& Name : Target.Soft)
        {
            SoftValues.Add(MakeShared<FJsonValueString>(Name.ToString()));
        }
        Item->SetArrayField(TEXT("hardReferencers"), HardValues);
        Item->SetArrayField(TEXT("softReferencers"), SoftValues);
        Items.Add(MakeShared<FJsonValueObject>(Item));
    }

    Root->SetNumberField(TEXT("deletedCount"), DeletedCount);
    Root->SetNumberField(TEXT("requestedCount"), Targets.Num());
    Root->SetArrayField(TEXT("items"), Items);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}


FString UZDBridgeLibrary::DetachSequencesFromAnimationSource(const TArray<FString>& SequenceObjectPaths)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("protocolName"), TEXT("ZDBridge.DetachSequences"));
    Root->SetNumberField(TEXT("protocolVersion"), 1);

    int32 DetachedCount = 0;
    TArray<TSharedPtr<FJsonValue>> Items;
    for (const FString& ObjectPath : SequenceObjectPaths)
    {
        if (ObjectPath.IsEmpty())
        {
            continue;
        }

        TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("objectPath"), ObjectPath);

        UObject* Object = LoadObject<UObject>(nullptr, *ObjectPath);
        UPaperZDAnimSequence* Sequence = Cast<UPaperZDAnimSequence>(Object);
        Item->SetStringField(TEXT("assetClass"), Object ? Object->GetClass()->GetName() : FString());
        if (!Sequence)
        {
            Item->SetBoolField(TEXT("detached"), false);
            Item->SetStringField(TEXT("previousSource"), FString());
            Item->SetStringField(TEXT("error"),
                Object ? TEXT("object is not a PaperZD sequence") : TEXT("sequence could not be loaded"));
            Items.Add(MakeShared<FJsonValueObject>(Item));
            continue;
        }

        const UPaperZDAnimationSource* PreviousSource = Sequence->GetAnimSource();
        Item->SetStringField(TEXT("previousSource"), PreviousSource ? PreviousSource->GetPathName() : FString());
        if (!PreviousSource)
        {
            // 已经不挂在任何源上了，视为已完成。
            Item->SetBoolField(TEXT("detached"), true);
            Item->SetStringField(TEXT("error"), FString());
            ++DetachedCount;
            Items.Add(MakeShared<FJsonValueObject>(Item));
            continue;
        }

        Sequence->Modify();
        Sequence->SetAnimSource(nullptr);

        // 不信调用本身，只认结果：指针真的空了才算解绑成功。
        const bool bDetached = Sequence->GetAnimSource() == nullptr;
        if (bDetached)
        {
            Sequence->MarkPackageDirty();
            const FString FileName = FPackageName::LongPackageNameToFilename(
                Sequence->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
            FSavePackageArgs SaveArgs;
            SaveArgs.TopLevelFlags = RF_Standalone;
            SaveArgs.SaveFlags = SAVE_NoError;
            UPackage::SavePackage(Sequence->GetOutermost(), nullptr, *FileName, SaveArgs);
            ++DetachedCount;
        }

        Item->SetBoolField(TEXT("detached"), bDetached);
        Item->SetStringField(TEXT("error"), bDetached ? FString() : TEXT("AnimSource pointer could not be cleared"));
        Items.Add(MakeShared<FJsonValueObject>(Item));
    }

    Root->SetNumberField(TEXT("detachedCount"), DetachedCount);
    Root->SetNumberField(TEXT("requestedCount"), Items.Num());
    Root->SetArrayField(TEXT("items"), Items);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}


namespace
{
    /**
     * 用给定的命名空间和键重建一段 FText。
     *
     * 走的是 NSLOCTEXT 宏底下那条路：这样写回资产时序列化成
     * NSLOCTEXT("ns", "key", "文本")，和美术在编辑器里填的没有区别。
     * 直接 FText::FromString 出来的是文化无关文本，会把本地化条目降级。
     */
    FText MakeKeyedText(const FString& Namespace, const FString& Key, const FString& Source)
    {
        if (Namespace.IsEmpty() || Key.IsEmpty())
        {
            return FText::FromString(Source);
        }
        return FText::AsLocalizable_Advanced(FTextKey(*Namespace), FTextKey(*Key), Source);
    }

    /** 取一段 FText 现有的命名空间和键；没有就返回空串。 */
    void InspectText(const FText& Text, FString& OutNamespace, FString& OutKey)
    {
        OutNamespace.Reset();
        OutKey.Reset();
        if (TOptional<FString> Namespace = FTextInspector::GetNamespace(Text))
        {
            OutNamespace = TextNamespaceUtil::StripPackageNamespace(*Namespace);
        }
        if (TOptional<FString> Key = FTextInspector::GetKey(Text))
        {
            OutKey = *Key;
        }
    }

    /**
     * 把一个 JSON 值写进 FText 属性，尽量沿用原位置的命名空间和键。
     *
     * 键沿用的意义：同一句技能介绍改了错别字，本地化条目还是同一条，
     * 不会每同步一次就多出一条孤儿条目。原来没有键时才新生成。
     */
    void ApplyTextValue(const TSharedPtr<FJsonValue>& Value, FText& Target, const FString& FallbackNamespace)
    {
        FString Namespace;
        FString Key;
        InspectText(Target, Namespace, Key);
        if (Namespace.IsEmpty())
        {
            Namespace = FallbackNamespace;
        }
        if (Key.IsEmpty())
        {
            Key = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        }
        Target = MakeKeyedText(Namespace, Key, Value.IsValid() ? Value->AsString() : FString());
    }

    /** FText 数组：逐下标沿用原有的键，多出来的新元素各自生成新键。 */
    bool ApplyTextArrayProperty(
        const FArrayProperty& ArrayProperty,
        const FTextProperty& InnerProperty,
        void* ValuePtr,
        const TArray<TSharedPtr<FJsonValue>>& JsonValues,
        const FString& FallbackNamespace)
    {
        FScriptArrayHelper Helper(&ArrayProperty, ValuePtr);
        TArray<FText> Existing;
        Existing.Reserve(Helper.Num());
        for (int32 Index = 0; Index < Helper.Num(); ++Index)
        {
            Existing.Add(*InnerProperty.GetPropertyValuePtr(Helper.GetRawPtr(Index)));
        }

        Helper.Resize(JsonValues.Num());
        for (int32 Index = 0; Index < JsonValues.Num(); ++Index)
        {
            FText* Slot = InnerProperty.GetPropertyValuePtr(Helper.GetRawPtr(Index));
            if (Existing.IsValidIndex(Index))
            {
                *Slot = Existing[Index];
            }
            ApplyTextValue(JsonValues[Index], *Slot, FallbackNamespace);
        }
        return true;
    }

    /** 把 JSON 里的一个字段写进行结构体的对应属性。 */
    bool ApplyStructField(
        const UScriptStruct& Struct,
        uint8* StructData,
        const FString& FieldName,
        const TSharedPtr<FJsonValue>& JsonValue,
        const FString& FallbackNamespace,
        FString& OutError);

    /** 把一个 JSON 对象逐字段覆盖到结构体上；没出现的字段保持原样。 */
    bool ApplyJsonObjectToStruct(
        const UScriptStruct& Struct,
        uint8* StructData,
        const FJsonObject& Fields,
        const FString& FallbackNamespace,
        FString& OutError)
    {
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Fields.Values)
        {
            if (!ApplyStructField(Struct, StructData, Pair.Key, Pair.Value, FallbackNamespace, OutError))
            {
                return false;
            }
        }
        return true;
    }

    /** 把结构体上某个字段的现值读成 JSON，写完之后自查用。 */
    TSharedPtr<FJsonValue> ReadStructField(const UScriptStruct& Struct, const uint8* StructData, const FString& FieldName)
    {
        FProperty* Property = Struct.FindPropertyByName(FName(*FieldName));
        if (!Property)
        {
            return MakeShared<FJsonValueString>(TEXT("<no such property>"));
        }
        const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(StructData);
        TSharedPtr<FJsonValue> Value = FJsonObjectConverter::UPropertyToJsonValue(Property, ValuePtr, 0, 0);
        return Value.IsValid() ? Value : MakeShared<FJsonValueString>(TEXT("<unreadable>"));
    }

    /**
     * 按键合并一个映射属性，而不是整体替换。
     *
     * 连携技就是这么存的：12SkInfor 每行有一个 SubCharName 映射，键是搭档名字。
     * 同步某一个搭档时如果整个映射被替换掉，同角色其余搭档的连携技会一起没掉。
     * 只有 JSON 里出现的键会被改，值是结构体时继续按字段递归覆盖。
     */
    bool ApplyStringKeyedMapMerge(
        const FMapProperty& MapProperty,
        void* ValuePtr,
        const FJsonObject& Entries,
        const FString& FallbackNamespace,
        FString& OutError)
    {
        FStrProperty* KeyProperty = CastField<FStrProperty>(MapProperty.KeyProp);
        FStructProperty* ValueProperty = CastField<FStructProperty>(MapProperty.ValueProp);
        if (!KeyProperty || !ValueProperty || !ValueProperty->Struct)
        {
            return false;
        }

        FScriptMapHelper Helper(&MapProperty, ValuePtr);
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Entries.Values)
        {
            const TSharedPtr<FJsonObject>* EntryFields = nullptr;
            if (!Entry.Value.IsValid() || !Entry.Value->TryGetObject(EntryFields) || !EntryFields->IsValid())
            {
                OutError = FString::Printf(TEXT("map entry %s expects a JSON object"), *Entry.Key);
                return false;
            }

            // 已有的键就地改，没有的才新增，这样原有内容不会被重建。
            int32 Index = INDEX_NONE;
            for (FScriptMapHelper::FIterator It(Helper); It; ++It)
            {
                if (*KeyProperty->GetPropertyValuePtr(Helper.GetKeyPtr(It.GetInternalIndex())) == Entry.Key)
                {
                    Index = It.GetInternalIndex();
                    break;
                }
            }
            if (Index == INDEX_NONE)
            {
                Index = Helper.AddDefaultValue_Invalid_NeedsRehash();
                KeyProperty->SetPropertyValue(Helper.GetKeyPtr(Index), Entry.Key);
                Helper.Rehash();
                // Rehash 会搬动元素，重新定位一次。
                for (FScriptMapHelper::FIterator It(Helper); It; ++It)
                {
                    if (*KeyProperty->GetPropertyValuePtr(Helper.GetKeyPtr(It.GetInternalIndex())) == Entry.Key)
                    {
                        Index = It.GetInternalIndex();
                        break;
                    }
                }
            }

            if (!ApplyJsonObjectToStruct(
                    *ValueProperty->Struct,
                    Helper.GetValuePtr(Index),
                    **EntryFields,
                    FallbackNamespace,
                    OutError))
            {
                return false;
            }
        }
        return true;
    }

    /** 把 JSON 里的一个字段写进结构体的对应属性。 */
    bool ApplyStructField(
        const UScriptStruct& Struct,
        uint8* StructData,
        const FString& FieldName,
        const TSharedPtr<FJsonValue>& JsonValue,
        const FString& FallbackNamespace,
        FString& OutError)
    {
        FProperty* Property = Struct.FindPropertyByName(FName(*FieldName));
        if (!Property)
        {
            OutError = FString::Printf(TEXT("struct has no property named %s"), *FieldName);
            return false;
        }

        void* ValuePtr = Property->ContainerPtrToValuePtr<void>(StructData);

        // FText 单独处理：JsonObjectConverter 会把它降级成文化无关文本。
        if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
        {
            FText* Target = TextProperty->GetPropertyValuePtr(ValuePtr);
            ApplyTextValue(JsonValue, *Target, FallbackNamespace);
            return true;
        }
        if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
        {
            if (FTextProperty* InnerText = CastField<FTextProperty>(ArrayProperty->Inner))
            {
                const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
                if (!JsonValue.IsValid() || !JsonValue->TryGetArray(JsonArray))
                {
                    OutError = FString::Printf(TEXT("%s expects a JSON array"), *FieldName);
                    return false;
                }
                return ApplyTextArrayProperty(*ArrayProperty, *InnerText, ValuePtr, *JsonArray, FallbackNamespace);
            }
        }
        // 嵌套结构体：继续逐字段覆盖，否则整块替换会把没提到的字段清成默认值，
        // 里面的 FText 本地化键也会一起丢掉。
        if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
        {
            const TSharedPtr<FJsonObject>* Fields = nullptr;
            if (StructProperty->Struct && JsonValue.IsValid() && JsonValue->TryGetObject(Fields) && Fields->IsValid())
            {
                return ApplyJsonObjectToStruct(
                    *StructProperty->Struct, reinterpret_cast<uint8*>(ValuePtr), **Fields, FallbackNamespace, OutError);
            }
        }
        if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
        {
            const TSharedPtr<FJsonObject>* Entries = nullptr;
            if (JsonValue.IsValid() && JsonValue->TryGetObject(Entries) && Entries->IsValid() &&
                ApplyStringKeyedMapMerge(*MapProperty, ValuePtr, **Entries, FallbackNamespace, OutError))
            {
                return true;
            }
            if (!OutError.IsEmpty())
            {
                return false;
            }
            // 不是「字符串键 -> 结构体」的映射就退回整体转换（等级倍率那种就走这条）。
        }

        if (!FJsonObjectConverter::JsonValueToUProperty(JsonValue, Property, ValuePtr, 0, 0))
        {
            OutError = FString::Printf(TEXT("failed to convert JSON value for %s"), *FieldName);
            return false;
        }
        return true;
    }
}

FString UZDBridgeLibrary::UpsertDataTableRow(const FString& TableObjectPath, FName RowName, const FString& RowJson)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("tableObjectPath"), TableObjectPath);
    Root->SetStringField(TEXT("rowName"), RowName.ToString());
    Root->SetBoolField(TEXT("ok"), false);
    Root->SetBoolField(TEXT("created"), false);
    TArray<TSharedPtr<FJsonValue>> WrittenFields;

    const auto Finish = [&Root, &WrittenFields](const FString& Error) -> FString
    {
        Root->SetArrayField(TEXT("writtenFields"), WrittenFields);
        Root->SetStringField(TEXT("error"), Error);
        Root->SetBoolField(TEXT("ok"), Error.IsEmpty());
        FString Output;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
        FJsonSerializer::Serialize(Root, Writer);
        return Output;
    };

    UDataTable* Table = LoadObject<UDataTable>(nullptr, *TableObjectPath);
    if (!Table)
    {
        return Finish(TEXT("data table could not be loaded"));
    }
    const UScriptStruct* RowStruct = Table->GetRowStruct();
    if (!RowStruct)
    {
        return Finish(TEXT("data table has no row struct"));
    }
    if (RowName.IsNone())
    {
        return Finish(TEXT("row name is empty"));
    }

    TSharedPtr<FJsonObject> Fields;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RowJson);
    if (!FJsonSerializer::Deserialize(Reader, Fields) || !Fields.IsValid())
    {
        return Finish(TEXT("row JSON could not be parsed"));
    }

    // 先复制一份现有行，再往上覆盖：没出现在 JSON 里的字段必须原样保留，
    // 否则勾选一个字段就会把同一行其余手工填的内容清空。
    uint8* Existing = Table->FindRowUnchecked(RowName);
    Root->SetBoolField(TEXT("created"), Existing == nullptr);

    TArray<uint8> Buffer;
    Buffer.SetNumZeroed(RowStruct->GetStructureSize());
    RowStruct->InitializeStruct(Buffer.GetData());
    if (Existing)
    {
        RowStruct->CopyScriptStruct(Buffer.GetData(), Existing);
    }

    const FString FallbackNamespace = FPackageName::GetShortName(Table->GetOutermost()->GetName());
    FString Error;
    if (!ApplyJsonObjectToStruct(*RowStruct, Buffer.GetData(), *Fields, FallbackNamespace, Error))
    {
        RowStruct->DestroyStruct(Buffer.GetData());
        return Finish(Error);
    }
    TSharedRef<FJsonObject> ReadBack = MakeShared<FJsonObject>();
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Fields->Values)
    {
        WrittenFields.Add(MakeShared<FJsonValueString>(Pair.Key));
        ReadBack->SetField(Pair.Key, ReadStructField(*RowStruct, Buffer.GetData(), Pair.Key));
    }
    Root->SetObjectField(TEXT("readBack"), ReadBack);

    Table->Modify();
    Table->AddRow(RowName, Buffer.GetData(), RowStruct);
    RowStruct->DestroyStruct(Buffer.GetData());

    Table->MarkPackageDirty();
    const FString FileName = FPackageName::LongPackageNameToFilename(
        Table->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Table->GetOutermost(), nullptr, *FileName, SaveArgs))
    {
        return Finish(TEXT("data table package could not be saved"));
    }

    return Finish(FString());
}

FString UZDBridgeLibrary::ApplyJsonToStructProperty(UObject* Owner, FName StructPropertyName, const FString& Json)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("structProperty"), StructPropertyName.ToString());
    TArray<TSharedPtr<FJsonValue>> WrittenFields;

    const auto Finish = [&Root, &WrittenFields](const FString& Error) -> FString
    {
        Root->SetArrayField(TEXT("writtenFields"), WrittenFields);
        Root->SetStringField(TEXT("error"), Error);
        Root->SetBoolField(TEXT("ok"), Error.IsEmpty());
        FString Output;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
        FJsonSerializer::Serialize(Root, Writer);
        return Output;
    };

    if (!Owner)
    {
        return Finish(TEXT("owner object is null"));
    }

    FStructProperty* StructProperty = CastField<FStructProperty>(
        Owner->GetClass()->FindPropertyByName(StructPropertyName));
    if (!StructProperty || !StructProperty->Struct)
    {
        return Finish(TEXT("owner has no struct property with that name"));
    }

    TSharedPtr<FJsonObject> Fields;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Fields) || !Fields.IsValid())
    {
        return Finish(TEXT("JSON could not be parsed"));
    }

    // 直接改 CDO 上那份结构体内存：没出现在 JSON 里的字段原样保留。
    uint8* StructData = StructProperty->ContainerPtrToValuePtr<uint8>(Owner);
    const FString FallbackNamespace = FPackageName::GetShortName(Owner->GetOutermost()->GetName());

    Owner->Modify();
    FString Error;
    if (!ApplyJsonObjectToStruct(*StructProperty->Struct, StructData, *Fields, FallbackNamespace, Error))
    {
        return Finish(Error);
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Fields->Values)
    {
        WrittenFields.Add(MakeShared<FJsonValueString>(Pair.Key));
    }

    Owner->MarkPackageDirty();
    return Finish(FString());
}

FString UZDBridgeLibrary::ReadDataTableRow(const FString& TableObjectPath, FName RowName)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("tableObjectPath"), TableObjectPath);
    Root->SetStringField(TEXT("rowName"), RowName.ToString());
    Root->SetBoolField(TEXT("found"), false);

    const auto Finish = [&Root](const FString& Error) -> FString
    {
        Root->SetStringField(TEXT("error"), Error);
        Root->SetBoolField(TEXT("ok"), Error.IsEmpty());
        FString Output;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
        FJsonSerializer::Serialize(Root, Writer);
        return Output;
    };

    UDataTable* Table = LoadObject<UDataTable>(nullptr, *TableObjectPath);
    if (!Table)
    {
        return Finish(TEXT("data table could not be loaded"));
    }
    const UScriptStruct* RowStruct = Table->GetRowStruct();
    if (!RowStruct)
    {
        return Finish(TEXT("data table has no row struct"));
    }

    const uint8* RowData = Table->FindRowUnchecked(RowName);
    if (!RowData)
    {
        // 行不存在不是错误：界面要据此显示成「待新增」。
        return Finish(FString());
    }

    TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
    if (!FJsonObjectConverter::UStructToJsonObject(RowStruct, RowData, Row, 0, 0))
    {
        return Finish(TEXT("row could not be converted to JSON"));
    }

    Root->SetBoolField(TEXT("found"), true);
    Root->SetObjectField(TEXT("row"), Row);
    return Finish(FString());
}

FString UZDBridgeLibrary::ResolveLightConfigurationAssets(
    const FString& ItemObjectPath, const FString& TeamSelectObjectPath, const FString& MetaSoundObjectPath)
{
    // 三个角色别名和工具箱第 3 步的约定一致：
    // item = Item_<代号>（Blueprint）、team = UI_TeamSelect（WidgetBlueprint）、meta = <代号>_OnDM（MetaSoundSource）。
    const TArray<TPair<FString, FString>> Requests = {
        TPair<FString, FString>(TEXT("item"), ItemObjectPath),
        TPair<FString, FString>(TEXT("team"), TeamSelectObjectPath),
        TPair<FString, FString>(TEXT("meta"), MetaSoundObjectPath),
    };

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("protocolName"), TEXT("ZDBridge.LightConfigAssets"));
    Root->SetNumberField(TEXT("protocolVersion"), 1);
    // 诊断：这两类的类由编辑器模块提供 —— 模块不在，就必然读不到（离线 commandlet 的典型症状）。
    Root->SetBoolField(TEXT("hasWidgetBlueprintClass"),
        FindObject<UClass>(nullptr, TEXT("/Script/UMGEditor.WidgetBlueprint")) != nullptr);
    Root->SetBoolField(TEXT("hasMetaSoundSourceClass"),
        FindObject<UClass>(nullptr, TEXT("/Script/MetasoundEngine.MetaSoundSource")) != nullptr);

    TArray<TSharedPtr<FJsonValue>> Items;
    int32 LoadedCount = 0;
    for (const TPair<FString, FString>& Request : Requests)
    {
        TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("role"), Request.Key);
        Entry->SetStringField(TEXT("objectPath"), Request.Value);

        // 存在性**用包名问**（不是对象路径）：这一句不依赖任何模块加载，
        // 所以它才是"资产到底在不在"的权威答案 —— 也才分得清"没有"和"读不到"。
        const FString PackageName = FPackageName::ObjectPathToPackageName(Request.Value);
        const bool bPackageExists = !Request.Value.IsEmpty() && FPackageName::DoesPackageExist(PackageName);
        Entry->SetBoolField(TEXT("packageExists"), bPackageExists);

        UObject* Asset = Request.Value.IsEmpty()
            ? nullptr
            : StaticLoadObject(UObject::StaticClass(), nullptr, *Request.Value);
        Entry->SetBoolField(TEXT("loaded"), Asset != nullptr);
        Entry->SetStringField(TEXT("className"), Asset ? Asset->GetClass()->GetName() : FString());

        // 蓝图默认对象：Python 那边是拼 `Default__<名>_C` 字符串拿的，
        // 名字带后缀或改过名就会拼错；这里直接问 GeneratedClass。
        UObject* DefaultObject = nullptr;
        if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
        {
            DefaultObject = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
        }
        else if (UClass* AssetClass = Cast<UClass>(Asset))
        {
            DefaultObject = AssetClass->GetDefaultObject();
        }

        Entry->SetBoolField(TEXT("hasDefaultObject"), DefaultObject != nullptr);
        if (Asset)
        {
            ++LoadedCount;
        }

        Items.Add(MakeShared<FJsonValueObject>(Entry));
    }

    Root->SetBoolField(TEXT("ok"), LoadedCount == Requests.Num());
    Root->SetNumberField(TEXT("loadedCount"), LoadedCount);
    Root->SetArrayField(TEXT("items"), Items);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}
