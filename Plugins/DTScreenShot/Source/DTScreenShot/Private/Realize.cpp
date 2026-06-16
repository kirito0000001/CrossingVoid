#include "Realize.h"
#include "ImageUtils.h"

void URealize::GetViewportScreenshot(FString ImageName, FString Path, bool SaveUI, EPathMode PathMode)
{
	FString SavePath;//新建一个字符串变量
	
	switch (PathMode)//根据保存模式选择路径
	{
		case  EPathMode::EPM_Custom:
			SavePath = Path + ImageName;
		break;

		case  EPathMode::EPM_ProjectDir:
			SavePath = FPaths::Combine(FPaths::ProjectDir(),Path,ImageName);
		break;

		case  EPathMode::EPM_ProjectSaveDir:
			SavePath = FPaths::Combine(FPaths::ProjectSavedDir(),Path,ImageName);
		break;

		default:
			SavePath = FPaths::Combine(FPaths::ProjectSavedDir(),Path,ImageName);
		break;
	}

	FScreenshotRequest::RequestScreenshot(SavePath, SaveUI, false);//利用内置的截图，设置对应的结构体
	//交给委托去实现功能
	FDelegateHandle ScreenShotHandle;
	ScreenShotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddLambda([ScreenShotHandle]
	{
		FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenShotHandle);
	});
}

UTexture2D* URealize::OpenImage(FString ImageName, FString Path, EPathMode PathMode)
{
	FString SavePath;
	switch (PathMode)
	{
	case EPathMode::EPM_Custom:
		return FImageUtils::ImportFileAsTexture2D(FPaths::Combine(Path,ImageName));

	case EPathMode::EPM_ProjectDir:
		SavePath=FPaths::ProjectDir()+Path;
		break;

	case EPathMode::EPM_ProjectSaveDir:
		SavePath = FPaths::ProjectSavedDir();
		break;

	default:
		SavePath = FPaths::ProjectSavedDir();
		break;
	}
	//拼接路径，返回texture2d
	return FImageUtils::ImportFileAsTexture2D(FPaths::Combine(SavePath,ImageName));
}
