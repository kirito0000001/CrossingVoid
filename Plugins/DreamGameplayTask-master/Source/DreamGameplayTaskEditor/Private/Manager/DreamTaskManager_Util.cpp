#include "Manager/DreamTaskManager_Util.h"

#include "DreamGameplayTaskEditorSetting.h"

TSharedRef<SHorizontalBox> FDreamTaskManagerUtil::MakeIconAndTextWidget(const FText& Text, const FSlateBrush* IconBrush, int IconSize, float FontSize)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.WidthOverride(IconSize)
			.HeightOverride(IconSize)
			[
				SNew(SImage)
				.Image(IconBrush)
			]
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.f, 0.f)
		[
			SNew(STextBlock)
			.Font(GetTextFont(FontSize))
			.Text(Text)
		];
}

FSlateFontInfo FDreamTaskManagerUtil::GetTextFont(float Size)
{
	FSlateFontInfo Font = UDreamGameplayTaskEditorSetting::Get()->ManagerFont;
	Font.Size = Size;
	return Font;
}

TSharedRef<SWidget> FDreamTaskManagerUtil::MakeText(const FText& InText)
{
	return SNew(STextBlock)
		.Text(InText);
}

TSharedRef<SWidget> FDreamTaskManagerUtil::MakeText(const FString& InString)
{
	return SNew(STextBlock)
		.Text(FText::FromString(InString));
}
