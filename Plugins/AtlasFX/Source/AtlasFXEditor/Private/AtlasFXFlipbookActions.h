// AtlasFX 的 Content Browser 右键菜单：从 Paper Flipbook 一键创建特效系统。
//
// 为什么做在编辑器模块里：
//   * 复制资产、开事务、通知 Content Browser 选中新资产，全是编辑器 API；
//   * 复用命令工具里那套「按类型找 AtlasFX 的 DI」的判据，不按名字找，使用者把 Atlas 改名也不影响。
//
// 菜单放在资产右键菜单的**最前面**（Paper Flipbook 本身没有别的插件分区，放最前面不挤别人）。
#pragma once

class FAtlasFXFlipbookActions
{
public:
	/** 注册右键菜单（走 UToolMenus 的启动回调，菜单系统还没建好时也能排上队）。 */
	static void Startup();

	/** 注销：按 owner 名清掉自己加的分区。 */
	static void Shutdown();
};
