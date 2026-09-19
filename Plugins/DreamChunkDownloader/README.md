# DreamChunkDownloader

本插件正在逐步开发中。。。目前已实现基本功能，可能会有小问题。欢迎提交 [Issues](https://github.com/TypeDreamMoon/DreamChunkDownloader/issues)

基于虚幻引擎ChunkDownloader 重构后的分包下载插件。

将原有.txt文件转换为json文件 更简单的配置`GoTo ProjectSettings/DreamPlugin/DreamChunkDownloaderSettings`

## 配置文件说明

### BuildManifest-Platform.json

构建清单文件包含以下主要字段：

-   `build-id`: 构建版本号，例如："1.0.0.0"
-   `entries-count`: 文件条目总数
-   `entries`: 文件条目列表，每个条目包含：
    -   `file-name`: PAK 文件名称
    -   `file-size`: 文件大小（字节）
    -   `file-version`: 文件版本
    -   `chunk-id`: 分包 ID
    -   `relative-url`: 相对下载路径
-   `download-chunk-id-list`: 需要下载的分包 ID 列表
-   `client-build-version` : 客户端构建版本 [可选] 需要在设置开启`bUseStaticRemoteHost`
-   `download-chunk-id-list` : 客户端需要下载的分包 ID 列表 [可选] 需要在设置开启`bUseStaticRemoteHost`

## 主要功能

### 下载管理

-   `StartDownload()`: 开始下载指定的分包
-   `GetDownloadProgress()`: 获取当前下载进度
-   `GetStats()`: 获取当前下载状态

### 分包管理

-   `MountChunk(int32 ChunkID)`: 挂载指定分包

### 版本控制

-   `ValidateManifestFile()`: 验证清单文件完整性

## 使用示例

```cpp
UDreamChunkDownloaderSubsystem* Subsystem = GWorld->GetGameInstance()->GetSubsystem<UDreamChunkDownloaderSubsystem>();

// 开始自动与服务器进行补丁
bool bSuccess = Subsystem->StartPatchGame(0);

// 监听下载进度
EDreamChunkStatus Status = Subsystem->GetStats();

// 监听补丁完成 
void OnPatchCompleted(bool bSuccess)
Subsystem->OnPatchCompleted.AddUObject(this, &YourClass::OnPatchCompleted);

// 监听分包完成
void OnMountCompleted(bool bSuccess)
Subsystem->OnMountCompleted.AddUObject(this, &YourClass::OnMountCompleted);

```

## 注意事项

1. 确保在项目配置中正确设置下载服务器地址
2. 建议在游戏启动时初始化下载器
3. 下载完成后需要手动调用 MountChunk 来挂载分包

## 支持与反馈

如有问题或建议，请访问：[GitHub Issues](https://github.com/TypeDreamMoon/DreamChunkDownloader/issues)

>> Developer Dream Moon 2025
