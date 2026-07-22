// Copyright 2017 ~ 2022 Critical Failure Studio Ltd. All rights reserved.

#pragma once
#include "CoreMinimal.h"
#include "SEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "PaperZDAnimationSourceViewportClient.h"
#include "Runtime/Launch/Resources/Version.h"

class UPaperZDAnimationSource;

/**
 * Viewport widget for rendering an animation source and its animation sequences.
 */
class SPaperZDAnimationSourceViewport : public SEditorViewport, public ICommonEditorViewportToolbarInfoProvider
{
	/* Animation source we're displaying. */
	TWeakObjectPtr<const UPaperZDAnimationSource> AnimationSourcePtr;

	// Viewport client
	TSharedPtr<FPaperZDAnimationSourceViewportClient> EditorViewportClient;

public:
	SLATE_BEGIN_ARGS(SPaperZDAnimationSourceViewport)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const UPaperZDAnimationSource& InAnimationSource);

	// SEditorViewport interface
	virtual void BindCommands() override;
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual EVisibility GetTransformToolbarVisibility() const override;
	virtual void OnFocusViewportToSelection() override;

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7 
	virtual TSharedPtr<SWidget> BuildViewportToolbar() override { return SNew(SVerticalBox); }
#else
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override { return SNew(SVerticalBox); }
#endif
	
	// End of SEditorViewport interface

	// ICommonEditorViewportToolbarInfoProvider interface
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;
	// End of ICommonEditorViewportToolbarInfoProvider interface

	TSharedRef<FPaperZDAnimationSourceViewportClient> GetEditorViewportClient();
};