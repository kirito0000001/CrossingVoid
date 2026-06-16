#pragma once

class DREAMGAMEPLAYTASKEDITOR_API FDreamGameplayTaskEditorStyle
{
public:
	static FName StyleName();
	static void Initialize();
	static void Register();
	static void Unreginster();
	static TSharedRef<FSlateStyleSet> Create();
	static TSharedPtr<ISlateStyle> Get();
	static TSharedPtr<class FSlateStyleSet> StyleInstance;
};
