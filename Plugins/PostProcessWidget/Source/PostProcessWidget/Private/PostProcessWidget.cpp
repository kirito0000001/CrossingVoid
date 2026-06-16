// Copyright Qibo Pang 2023. All Rights Reserved.

#include "PostProcessWidget.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#include "SPostProcessWidget.h"

#define LOCTEXT_NAMESPACE "UMG"

static FName DefaultTextureParameterName("Texture");

/////////////////////////////////////////////////////
// UPostProcessWidget

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UPostProcessWidget::UPostProcessWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Visibility = ESlateVisibility::Visible;
	Phase = 0;
	PhaseCount = 1;
	RenderOnPhase = true;
	RenderOnInvalidation = false;
	TextureParameter = DefaultTextureParameterName;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

void UPostProcessWidget::SetRenderingPhase(int32 PhaseToRenderOn, int32 TotalRenderingPhases)
{
	Phase = PhaseToRenderOn;
	PhaseCount = TotalRenderingPhases;
	
	if ( PhaseCount < 1 )
	{
		PhaseCount = 1;
	}

	if (MyPostProcessWidget.IsValid())
	{
		MyPostProcessWidget->SetRenderingPhase(Phase, PhaseCount);
	}
}

void UPostProcessWidget::RequestRender()
{
	if ( MyPostProcessWidget.IsValid() )
	{
		MyPostProcessWidget->RequestRender();
	}
}

UMaterialInstanceDynamic* UPostProcessWidget::GetEffectMaterial() const
{
	if ( MyPostProcessWidget.IsValid() )
	{
		return MyPostProcessWidget->GetEffectMaterial();
	}

	return nullptr;
}

void UPostProcessWidget::SetEffectMaterial(UMaterialInterface* InEffectMaterial)
{
	EffectMaterial = InEffectMaterial;
	if ( MyPostProcessWidget.IsValid() )
	{
		MyPostProcessWidget->SetEffectMaterial(EffectMaterial);
	}
}

void UPostProcessWidget::SetTextureParameter(FName InTextureParameter)
{
	TextureParameter = InTextureParameter;
	if ( MyPostProcessWidget.IsValid() )
	{
		MyPostProcessWidget->SetTextureParameter(TextureParameter);
	}
}

void UPostProcessWidget::SetPostProcessRendering(bool bInPostProcessRendering)
{
	bPostProcessRender = bInPostProcessRendering;

	if (MyPostProcessWidget.IsValid())
	{
		MyPostProcessWidget->SetPostProcessRendering(bPostProcessRender);
	}
}

void UPostProcessWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyPostProcessWidget.Reset();
}

TSharedRef<SWidget> UPostProcessWidget::RebuildWidget()
{
	MyPostProcessWidget =
		SNew(SPostProcessWidget)
		.RenderOnInvalidation(RenderOnInvalidation)
		.RenderOnPhase(RenderOnPhase)
		.Phase(Phase)
		.PhaseCount(PhaseCount)
#if STATS
		.StatId( FName( *FString::Printf(TEXT("%s [%s]"), *GetFName().ToString(), *GetClass()->GetName() ) ) )
#endif//STATS
	;

	if ( GetChildrenCount() > 0 )
	{
		MyPostProcessWidget->SetContent(GetContentSlot()->Content ? GetContentSlot()->Content->TakeWidget() : SNullWidget::NullWidget);
	}
	
	return MyPostProcessWidget.ToSharedRef();
}

void UPostProcessWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyPostProcessWidget->SetPostProcessRendering(/*IsDesignTime() ? false : */bPostProcessRender);
	MyPostProcessWidget->SetEffectMaterial(EffectMaterial);
	MyPostProcessWidget->SetTextureParameter(TextureParameter);
	MyPostProcessWidget->SetWorld(GetWorld());
}

void UPostProcessWidget::OnSlotAdded(UPanelSlot* InSlot)
{
	// Add the child to the live slot if it already exists
	if ( MyPostProcessWidget.IsValid() )
	{
		MyPostProcessWidget->SetContent(InSlot->Content ? InSlot->Content->TakeWidget() : SNullWidget::NullWidget);
	}
}

void UPostProcessWidget::OnSlotRemoved(UPanelSlot* InSlot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyPostProcessWidget.IsValid() )
	{
		MyPostProcessWidget->SetContent(SNullWidget::NullWidget);
	}
}

UTexture* UPostProcessWidget::GetTexture() const
{
	if (MyPostProcessWidget.IsValid())
	{
		return MyPostProcessWidget->GetTexture();
	}
	else
	{
		return nullptr;
	}
}

#if WITH_EDITOR

const FText UPostProcessWidget::GetPaletteCategory()
{
	return LOCTEXT("SpecialFX", "Special Effects");
}

#endif

FGeometry UPostProcessWidget::GetCachedAllottedGeometry() const
{
	if (MyPostProcessWidget.IsValid())
	{
		return MyPostProcessWidget->GetTickSpaceGeometry();
	}

	static const FGeometry TempGeo;
	return TempGeo;
}

#if WITH_EDITOR
bool UPostProcessWidget::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPostProcessWidget, Phase)
		|| InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPostProcessWidget, PhaseCount))
	{
		return RenderOnPhase && bPostProcessRender;
	}
	return true;
}
#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
