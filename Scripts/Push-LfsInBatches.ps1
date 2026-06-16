param(
    [int]$BatchSize = 200,
    [int]$RetryBatchSize = 25,
    [switch]$PushMain
)

$ErrorActionPreference = 'Stop'

function Invoke-Git {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Args)
    & git @Args
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Args -join ' ') failed with exit code $LASTEXITCODE"
    }
}

Set-Location -LiteralPath (Split-Path -Parent $PSScriptRoot)

Invoke-Git config --local http.proxy http://127.0.0.1:7897
Invoke-Git config --local https.proxy http://127.0.0.1:7897
Invoke-Git config --local http.version HTTP/1.1
Invoke-Git config --local lfs.concurrenttransfers 3
Invoke-Git config --local lfs.basictransfersonly true

$logDir = Join-Path (Get-Location) '.git\codex-upload-logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$logFile = Join-Path $logDir ("lfs-push-{0:yyyyMMdd-HHmmss}.log" -f (Get-Date))

"[$(Get-Date -Format s)] Collecting LFS object IDs..." | Tee-Object -FilePath $logFile -Append
$oids = git lfs ls-files -l | ForEach-Object {
    ($_ -split '\s+')[0]
} | Where-Object { $_ -match '^[0-9a-f]{64}$' } | Sort-Object -Unique

if (-not $oids -or $oids.Count -eq 0) {
    throw "No LFS object IDs found."
}

$total = $oids.Count
$totalBatches = [Math]::Ceiling($total / $BatchSize)
"[$(Get-Date -Format s)] Found $total LFS objects. BatchSize=$BatchSize RetryBatchSize=$RetryBatchSize" | Tee-Object -FilePath $logFile -Append

function Push-OidBatch {
    param(
        [string[]]$Batch,
        [string]$Label
    )

    $tempFile = Join-Path $env:TEMP ("lfs-oids-{0}-{1}.txt" -f $PID, ([guid]::NewGuid().ToString('N')))
    try {
        $Batch | Set-Content -LiteralPath $tempFile -Encoding ascii
        $message = "[$(Get-Date -Format s)] Pushing $Label ($($Batch.Count) objects)"
        $message | Tee-Object -FilePath $logFile -Append
        Get-Content -LiteralPath $tempFile | git lfs push --object-id origin --stdin 2>&1 | Tee-Object -FilePath $logFile -Append
        if ($LASTEXITCODE -ne 0) {
            throw "git lfs push failed for $Label"
        }
    }
    finally {
        Remove-Item -LiteralPath $tempFile -Force -ErrorAction SilentlyContinue
    }
}

for ($offset = 0; $offset -lt $total; $offset += $BatchSize) {
    $end = [Math]::Min($offset + $BatchSize - 1, $total - 1)
    $batch = @($oids[$offset..$end])
    $batchNumber = [Math]::Floor($offset / $BatchSize) + 1
    $label = "batch $batchNumber/$totalBatches objects $($offset + 1)-$($end + 1)"

    Write-Progress -Activity 'Uploading Git LFS objects' -Status $label -PercentComplete (($offset / $total) * 100)

    try {
        Push-OidBatch -Batch $batch -Label $label
    }
    catch {
        "[$(Get-Date -Format s)] $label failed, retrying in smaller chunks: $($_.Exception.Message)" | Tee-Object -FilePath $logFile -Append
        for ($retryOffset = 0; $retryOffset -lt $batch.Count; $retryOffset += $RetryBatchSize) {
            $retryEnd = [Math]::Min($retryOffset + $RetryBatchSize - 1, $batch.Count - 1)
            $retryBatch = @($batch[$retryOffset..$retryEnd])
            Push-OidBatch -Batch $retryBatch -Label "$label retry objects $($retryOffset + 1)-$($retryEnd + 1)"
        }
    }
}

Write-Progress -Activity 'Uploading Git LFS objects' -Completed
"[$(Get-Date -Format s)] LFS upload finished." | Tee-Object -FilePath $logFile -Append

if ($PushMain) {
    "[$(Get-Date -Format s)] Pushing main with force-with-lease..." | Tee-Object -FilePath $logFile -Append
    $env:GIT_LFS_SKIP_PUSH = '1'
    git push --force-with-lease -u origin main 2>&1 | Tee-Object -FilePath $logFile -Append
    if ($LASTEXITCODE -ne 0) {
        throw "git push --force-with-lease -u origin main failed with exit code $LASTEXITCODE"
    }
    "[$(Get-Date -Format s)] main push finished." | Tee-Object -FilePath $logFile -Append
}

"[$(Get-Date -Format s)] Done. Log: $logFile" | Tee-Object -FilePath $logFile -Append
Read-Host 'Press Enter to close'
