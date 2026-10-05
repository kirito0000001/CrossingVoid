# GetTheMeaning Project Risk Analysis Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add generic project-level Blueprint risk analysis and read-only SaveGame auditing without hardcoding host-project business names.

**Architecture:** `BlueprintRiskAnalyzer` converts Unreal graph nodes into neutral facts. `ProjectRiskReport` applies deterministic rules to facts and writes Markdown/JSON. `SaveGameAuditor` loads arbitrary `.sav` files from a selected directory and recursively inspects reflected values. Existing exporters call these components but preserve current output schemas.

**Tech Stack:** Unreal Engine 5.6 Editor C++, BlueprintGraph, Unreal Automation Tests, JSON serialization, DesktopPlatform, SaveGame reflection.

---

### Task 1: Pure risk model and rule tests

**Files:**
- Create: `Source/GetTheMeaning/Private/ProjectRiskReport.h`
- Create: `Source/GetTheMeaning/Private/ProjectRiskReport.cpp`
- Create: `Source/GetTheMeaning/Private/Tests/ProjectRiskAnalyzerTests.cpp`

- [ ] **Step 1: Write failing automation tests**

Define synthetic facts for `NoKnownWriter`, `DefaultOnly`, matching/mismatching Map identity sources, unchecked Map Find, and unvalidated Server RPC. Tests call:

```cpp
const TArray<FProjectRiskFinding> Findings = FProjectRiskReport::Analyze(Facts);
TestTrue(TEXT("unchecked map find"), Findings.ContainsByPredicate([](const FProjectRiskFinding& Finding)
{
    return Finding.Category == TEXT("UncheckedMapFind");
}));
```

- [ ] **Step 2: Run build and verify RED**

Run:

```powershell
& "D:\YuanMa\UnrealEngine-release\Engine\Build\BatchFiles\Build.bat" FantasyProjectEditor Win64 Development "D:\UnrealMap\FantasyProject\FantasyProject.uproject" -WaitMutex -NoHotReloadFromIDE
```

Expected: FAIL because `ProjectRiskReport.h` and its API are not implemented.

- [ ] **Step 3: Implement neutral fact and finding types**

Provide these stable types:

```cpp
enum class EProjectRiskAccessKind : uint8 { Read, ExplicitWrite, DefaultWrite, ExternalSource };

struct FStructFieldAccessFact;
struct FContainerIdentityFact;
struct FMapFindFact;
struct FRpcIdentityFact;
struct FBlueprintRiskFacts;
struct FProjectRiskFinding;

class FProjectRiskReport
{
public:
    static TArray<FProjectRiskFinding> Analyze(const TArray<FBlueprintRiskFacts>& Facts);
    static bool WriteReports(const TArray<FBlueprintRiskFacts>& Facts, const FString& OutputDirectory);
};
```

Implement category generation for `NoKnownWriter`, `DefaultOnly`, `IntentionalMirror`, `UnverifiedMirror`, `InconsistentIdentity`, `UncheckedMapFind`, and `UnvalidatedIdentityRpc`.

- [ ] **Step 4: Run build and automation tests GREEN**

Run the build command, then:

```powershell
& "D:\YuanMa\UnrealEngine-release\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealMap\FantasyProject\FantasyProject.uproject" -ExecCmds="Automation RunTests GetTheMeaning.ProjectRisk;Quit" -unattended -nop4 -NullRHI -nosplash -log="D:\UnrealMap\FantasyProject\Saved\Logs\GetTheMeaningTests.log"
```

Expected: tests report Success in `GetTheMeaningTests.log`.

### Task 2: Blueprint fact collection

**Files:**
- Create: `Source/GetTheMeaning/Private/BlueprintRiskAnalyzer.h`
- Create: `Source/GetTheMeaning/Private/BlueprintRiskAnalyzer.cpp`
- Modify: `Source/GetTheMeaning/Private/Tests/ProjectRiskAnalyzerTests.cpp`

- [ ] **Step 1: Add failing collector classification tests**

Test identity-name matching, default-value classification, and result-consumer classification through public pure helpers:

```cpp
TestTrue(TEXT("guid is identity-like"), FBlueprintRiskAnalyzer::IsIdentityLikeName(TEXT("EntityGuid"), Patterns));
TestFalse(TEXT("grid is not identity-like"), FBlueprintRiskAnalyzer::IsIdentityLikeName(TEXT("GridSize"), Patterns));
```

- [ ] **Step 2: Verify RED**

Run the editor build and confirm missing collector API failure.

- [ ] **Step 3: Implement graph traversal**

Collect facts from `UK2Node_BreakStruct`, `UK2Node_MakeStruct`, `UK2Node_SetFieldsInStruct`, `UK2Node_CallFunction`, custom Server RPC events, variable properties, and linked pins. Use reflection paths and pin types as identifiers; names are only pattern hints.

- [ ] **Step 4: Verify GREEN**

Build and run `GetTheMeaning.ProjectRisk` tests.

### Task 3: Project report integration

**Files:**
- Modify: `Source/GetTheMeaning/Private/GetTheMeaning.cpp`
- Modify: `Source/GetTheMeaning/Private/BlueprintToTextExporter.cpp`
- Modify: `Source/GetTheMeaning/Private/ProjectRiskReport.cpp`

- [ ] **Step 1: Add failing report serialization test**

Assert generated JSON has `schemaVersion`, `findings`, `category`, `severity`, `confidence`, `assetPath`, `graphName`, and `nodeId`. Assert Markdown contains a generic explanation and no host-project class names.

- [ ] **Step 2: Verify RED**

Run tests and confirm report serialization assertions fail.

- [ ] **Step 3: Generate reports after batch export**

During `ExportAssetDataList`, collect facts for every loaded Blueprint and call:

```cpp
FProjectRiskReport::WriteReports(R.RiskFacts, GetExportRootDir());
```

Write `ProjectRiskReport.json` and `ProjectRiskReport.md`. Append compatible per-Blueprint structured facts to Logic JSON without renaming existing fields.

- [ ] **Step 4: Verify GREEN and inspect report**

Build, run tests, export the project assets, and verify both report files parse and contain only generic categories.

### Task 4: Configurable identity patterns

**Files:**
- Create: `Source/GetTheMeaning/Public/GetTheMeaningSettings.h`
- Create: `Source/GetTheMeaning/Private/GetTheMeaningSettings.cpp`
- Modify: `Source/GetTheMeaning/GetTheMeaning.Build.cs`
- Modify: `Source/GetTheMeaning/Private/BlueprintRiskAnalyzer.cpp`

- [ ] **Step 1: Add failing settings default test**

Assert default patterns contain `Guid`, `Uid`, `Id`, and `Identifier`, with case-insensitive token matching that does not match unrelated substrings such as `Grid`.

- [ ] **Step 2: Verify RED**

Build and confirm the settings class is missing.

- [ ] **Step 3: Implement Editor project settings**

Create `UGetTheMeaningSettings : UDeveloperSettings` using `Config=EditorPerProjectUserSettings` and expose `IdentityNamePatterns`. Add the `DeveloperSettings` dependency and use settings in the collector.

- [ ] **Step 4: Verify GREEN**

Build and run project-risk tests.

### Task 5: Read-only SaveGame auditor

**Files:**
- Create: `Source/GetTheMeaning/Private/SaveGameAuditor.h`
- Create: `Source/GetTheMeaning/Private/SaveGameAuditor.cpp`
- Create: `Source/GetTheMeaning/Private/Tests/SaveGameAuditorTests.cpp`
- Modify: `Source/GetTheMeaning/GetTheMeaning.Build.cs`
- Modify: `Source/GetTheMeaning/Private/GetTheMeaning.cpp`

- [ ] **Step 1: Write failing value-comparison tests**

Use neutral synthetic audit records to verify invalid FGuid, invalid Map key, key/value mismatch, valid mirror, and unreadable file findings.

- [ ] **Step 2: Verify RED**

Build and confirm the auditor API is missing.

- [ ] **Step 3: Implement file and reflection audit**

Load `.sav` bytes with `FFileHelper::LoadFileToArray`, deserialize using `UGameplayStatics::LoadGameFromMemory`, and recursively inspect `FStructProperty`, `FArrayProperty`, `FSetProperty`, and `FMapProperty`. Add cycle/depth protection. Never call save APIs.

Write `SaveGameAudit.json` and `SaveGameAudit.md`, continuing after individual file failures.

- [ ] **Step 4: Add menu action**

Add `DesktopPlatform` dependency and a Window menu entry named `审计 SaveGame 存档...`. Use `OpenDirectoryDialog`, defaulting to `Saved/SaveGames`, then run the auditor and show a notification linking to the report directory.

- [ ] **Step 5: Verify GREEN**

Build and run `GetTheMeaning.SaveGameAudit` tests. Manually invoke the action against the project SaveGames directory and confirm no `.sav` timestamp changes.

### Task 6: Final integration verification

**Files:**
- Modify: `GetTheMeaning.uplugin`
- Modify: `docs/superpowers/specs/2026-07-13-project-risk-analysis-design.md` only if implementation details differ

- [ ] **Step 1: Update plugin metadata**

Increment `Version` and `VersionName`, and describe project-level risk reports plus read-only SaveGame auditing.

- [ ] **Step 2: Run complete build and tests**

Build `FantasyProjectEditor`, run both automation prefixes, and scan logs for `Error:`, failed tests, assertions, or module load failures.

- [ ] **Step 3: Verify backward compatibility**

Run a batch export and confirm existing `ReadableCode.txt`, `Logic.json`, `ExportIndex`, and `ExportGraph` files are still produced. Parse all new JSON outputs with PowerShell `ConvertFrom-Json`.

- [ ] **Step 4: Verify genericity**

Search production plugin code for host-project business class and variable names. Expected: no project-specific rule strings. Confirm synthetic tests use neutral names.
