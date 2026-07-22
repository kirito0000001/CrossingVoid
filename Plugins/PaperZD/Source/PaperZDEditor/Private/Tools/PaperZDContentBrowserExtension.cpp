// Copyright 2017 ~ 2022 Critical Failure Studio Ltd. All rights reserved.

#include "PaperZDContentBrowserExtension.h"
#include "ContentBrowserModule.h"
#include "PaperZDEditorStyle.h"
#include "PaperZDExtractFlipbookFromTextureDialog.h"
#include "Engine/Texture2D.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "PaperZDTools"

static FContentBrowserMenuExtender_SelectedAssets ContentBrowserExtenderDelegate;
static FDelegateHandle ContentBrowserExtenderDelegateHandle;

struct FContentBrowserSelectedAssetExtensionBase
{
public:
    TArray<struct FAssetData> SelectedAssets;

public:
    virtual ~FContentBrowserSelectedAssetExtensionBase() {}

    virtual void Execute() {}
};

struct FExtractFlipbookFromTextureExtension : public FContentBrowserSelectedAssetExtensionBase
{
    void Execute() override
    {
        for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
        {
            const FAssetData& AssetData = *AssetIt;
            if (UTexture2D* Texture = Cast<UTexture2D>(AssetData.GetAsset()))
            {
                SPaperZDExtractFlipbookFromTextureDialog::ShowWindow(Texture);
            }
        }
    }

    static TArray<FAssetData> ValidateAssets(const TArray<FAssetData>& Assets)
    {
        TArray<FAssetData> ValidAssets;

        const UClass* TextureClass = UTexture2D::StaticClass();

        for (auto AssetIt = Assets.CreateConstIterator(); AssetIt; ++AssetIt)
        {
            const FAssetData& Asset = *AssetIt;
            const UClass* AssetClass = Asset.GetClass();
            if (AssetClass && AssetClass->IsChildOf(TextureClass))
            {
                ValidAssets.Add(Asset);
            }
        }

        return ValidAssets;
    }

    static bool HasValidAssets(const TArray<FAssetData>& Assets)
    {
        return !ValidateAssets(Assets).IsEmpty();
    }
};

class FPaperZDContentBrowserExtension_Impl
{
public:
    static void ExecuteSelectedContentFunctor(TSharedPtr<FContentBrowserSelectedAssetExtensionBase> SelectedAssetFunctor)
    {
        SelectedAssetFunctor->Execute();
    }
    
    static void PopulateActionsMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
    {
        // Extract Flipbook
        TArray<FAssetData> ExtractFlipbookAssets = FExtractFlipbookFromTextureExtension::ValidateAssets(SelectedAssets);
        if (!ExtractFlipbookAssets.IsEmpty())
        {
            TSharedPtr<FExtractFlipbookFromTextureExtension> ExtractFlipbookFunctor = MakeShareable(new FExtractFlipbookFromTextureExtension());
            ExtractFlipbookFunctor->SelectedAssets = ExtractFlipbookAssets;

            FUIAction Action_ExtractFlipbook(
                FExecuteAction::CreateStatic(&FPaperZDContentBrowserExtension_Impl::ExecuteSelectedContentFunctor, StaticCastSharedPtr<FContentBrowserSelectedAssetExtensionBase>(ExtractFlipbookFunctor)));
        
            MenuBuilder.AddMenuEntry(
                LOCTEXT("CB_Extension_Texture_ExtractFlipbook", "Extract Flipbook"),
                LOCTEXT("CB_Extension_Texture_ExtractFlipbook_Tooltip", "Extracts a Flipbook from the selected Texture"),
                FSlateIcon(FPaperZDEditorStyle::Get().GetStyleSetName(), "PaperZDEditor.Tools.ExtractAction"),
                Action_ExtractFlipbook,
                NAME_None,
                EUserInterfaceActionType::Button);
        }
    }

    static void CreateActionsSubMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
    {
        MenuBuilder.AddSubMenu(
            LOCTEXT("PaperZDActionsSubMenuLabel", "PaperZD Actions"),
            LOCTEXT("PaperZDActionsSubMenuToolTip", "PaperZD Asset Actions"),
            FNewMenuDelegate::CreateStatic(&FPaperZDContentBrowserExtension_Impl::PopulateActionsMenu, SelectedAssets),
            false,
            FSlateIcon(FPaperZDEditorStyle::Get().GetStyleSetName(), "PaperZDEditor.Tools.PluginAction")
        );
    }

    static TArray<FContentBrowserMenuExtender_SelectedAssets>& GetExtenderDelegates()
    {
        FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
        return ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
    }

    static TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
    {
        TSharedRef<FExtender> Extender(new FExtender());

        // Add other asset checks here for making the paper zd menu appear
        const bool bHasAnyValidAssets = FExtractFlipbookFromTextureExtension::HasValidAssets(SelectedAssets);

        if (bHasAnyValidAssets)
        {
            Extender->AddMenuExtension(
                "GetAssetActions",
                EExtensionHook::After,
                nullptr,
                FMenuExtensionDelegate::CreateStatic(&FPaperZDContentBrowserExtension_Impl::CreateActionsSubMenu, SelectedAssets));
        }

        return Extender;
    }
};

void FPaperZDContentBrowserExtension::InstallHooks()
{
    ContentBrowserExtenderDelegate = FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FPaperZDContentBrowserExtension_Impl::OnExtendContentBrowserAssetSelectionMenu);

    TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = FPaperZDContentBrowserExtension_Impl::GetExtenderDelegates();
    CBMenuExtenderDelegates.Add(ContentBrowserExtenderDelegate);
    ContentBrowserExtenderDelegateHandle = CBMenuExtenderDelegates.Last().GetHandle();
}

void FPaperZDContentBrowserExtension::RemoveHooks()
{
    TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = FPaperZDContentBrowserExtension_Impl::GetExtenderDelegates();
    CBMenuExtenderDelegates.RemoveAll([](const FContentBrowserMenuExtender_SelectedAssets& Delegate) { return Delegate.GetHandle() == ContentBrowserExtenderDelegateHandle; });
}

#undef LOCTEXT_NAMESPACE