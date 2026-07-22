// Copyright Qibo Pang 2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "Components/ContentWidget.h"
#include "PostProcessWidget.generated.h"

class SPostProcessWidget;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 *
 */
UCLASS()
class POSTPROCESSWIDGET_API UPostProcessWidget : public UContentWidget
{
	GENERATED_UCLASS_BODY()

protected:
	UPROPERTY(EditAnywhere, Category="Render Rules")
	bool bPostProcessRender = true;

public:
	/**
	 * Should this widget redraw the contents it has every time it receives an invalidation request
	 * from it's children, similar to the invalidation panel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Render Rules", meta=(EditCondition=bPostProcessRender))
	bool RenderOnInvalidation;

	/**
	 * Should this widget redraw the contents it has every time the phase occurs.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Render Rules", meta=(EditCondition=bPostProcessRender))
	bool RenderOnPhase;

	/**
	 * The Phase this widget will draw on.
	 *
	 * If the Phase is 0, and the PhaseCount is 1, the widget will be drawn fresh every frame.
	 * If the Phase were 0, and the PhaseCount were 2, this PostProcess would draw a fresh frame every
	 * other frame.  So in a 60Hz game, the UI would render at 30Hz.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Render Rules", meta=(UIMin=0, ClampMin=0))
	int32 Phase;

	/**
	 * The PhaseCount controls how many phases are possible know what to modulus the current frame 
	 * count by to determine if this is the current frame to draw the widget on.
	 * 
	 * If the Phase is 0, and the PhaseCount is 1, the widget will be drawn fresh every frame.  
	 * If the Phase were 0, and the PhaseCount were 2, this PostProcess would draw a fresh frame every 
	 * other frame.  So in a 60Hz game, the UI would render at 30Hz.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Render Rules", meta=(UIMin=1, ClampMin=1))
	int32 PhaseCount;

public:

	/**
	 * Requests the PostProcess redrawn the contents it has.
	 */
	UFUNCTION(BlueprintCallable, Category="PostProcess")
	void SetRenderingPhase(int32 RenderPhase, int32 TotalPhases);

	/**
	 * Requests the PostProcess redrawn the contents it has.
	 */
	UFUNCTION(BlueprintCallable, Category="PostProcess")
	void RequestRender();

	/**
	 * Get the current dynamic effect material applied to the PostProcess box.
	 */
	UFUNCTION(BlueprintCallable, Category="PostProcess|Effect")
	UMaterialInstanceDynamic* GetEffectMaterial() const;

	/**
	 * Get the target render texture.
	 */
	UFUNCTION(BlueprintCallable, Category = "PostProcess|Effect")
		UTexture* GetTexture() const;

	/**
	 * Set a new effect material to the PostProcess widget.
	 */
	UFUNCTION(BlueprintCallable, Category="PostProcess|Effect")
	void SetEffectMaterial(UMaterialInterface* EffectMaterial);

	/**
	 * Sets the name of the texture parameter to set the render target to on the material.
	 */
	UFUNCTION(BlueprintCallable, Category="PostProcess|Effect")
	void SetTextureParameter(FName TextureParameter);
	/**
	* Set the flag for if we PostProcess the render or pass-through
	*/
	UFUNCTION(BlueprintCallable, Category = "PostProcess")
	void SetPostProcessRendering(bool bInPostProcessRendering);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

	FGeometry GetCachedAllottedGeometry() const;

protected:

	/**
	 * The effect to optionally apply to the render target.  We will set the texture sampler based on the name
	 * set in the @TextureParameter property.
	 * 
	 * If you want to adjust transparency of the final image, make sure you set Blend Mode to AlphaComposite (Pre-Multiplied Alpha)
	 * and make sure to multiply the alpha you're apply across the surface to the color and the alpha of the render target, otherwise
	 * you won't see the expected color.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	UMaterialInterface* EffectMaterial;

	/**
	 * The texture sampler parameter of the @EffectMaterial, that we'll set to the render target.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	FName TextureParameter;

	//~ Begin UPanelWidget interface
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	//~ End UPanelWidget interface

	//~ Begin UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	//~ End of UWidget interface

	//~ Begin UObject interface
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif
	//~ End UObject interface

protected:
	TSharedPtr<class SPostProcessWidget> MyPostProcessWidget;
};
