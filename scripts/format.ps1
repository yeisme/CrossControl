Param(
    [switch]$Verbose
)

# 格式化脚本（PowerShell）
# 用法：
#   .\scripts\format.ps1            # 递归格式化仓库下所有 .h .hpp .c .cpp .cc .cxx 文件（排除 build/.git 等）
#   .\scripts\format.ps1 -Verbose  # 详细输出（显示每个文件的处理状态）

$extensions = @('.h', '.hpp', '.c', '.cpp', '.cc', '.cxx')
$excludePattern = '\\build\\|\\.git\\|/build/|/\\.git/|\\.vscode\\|/\\.vscode/'

# 查找 clang-format
$clang = Get-Command clang-format -ErrorAction SilentlyContinue
if (-not $clang) {
    Write-Error "clang-format 未找到。请先安装 clang-format 并将其加入 PATH（例如通过 vcpkg、choco 或 LLVM 官方安装包）。"
    exit 2
}

# 收集目标文件列表
$files = Get-ChildItem -Path . -Recurse -File | Where-Object {
    $extensions -contains $_.Extension.ToLower() -and ($_.FullName -notmatch $excludePattern)
}

if (-not $files -or $files.Count -eq 0) {
    Write-Host "未找到需要格式化的源文件。"
    exit 0
}

$processed = 0
$changedFiles = @()

foreach ($f in $files) {
    $processed++
    if ($Verbose) { Write-Host "Processing: $($f.FullName)" }

    # 将 clang-format 的输出写到临时文件（不使用 -i），以便比较是否发生实际变化
    $tmp = [System.IO.Path]::GetTempFileName()
    try {
        $args = @('-style=file', $f.FullName)
        $proc = Start-Process -FilePath $clang.Path -ArgumentList $args -NoNewWindow -Wait -RedirectStandardOutput $tmp -PassThru -ErrorAction Stop
    } catch {
        Write-Warning "无法运行 clang-format ($($f.FullName))： $_"
        Remove-Item $tmp -ErrorAction SilentlyContinue
        continue
    }

    if ($proc.ExitCode -ne 0) {
        Write-Warning "clang-format 处理 $($f.FullName) 时返回错误（exit code: $($proc.ExitCode)）。"
        Remove-Item $tmp -ErrorAction SilentlyContinue
        continue
    }

    try {
        $origBytes = [System.IO.File]::ReadAllBytes($f.FullName)
        $newBytes = [System.IO.File]::ReadAllBytes($tmp)
    } catch {
        Write-Warning "读取文件时出错： $($f.FullName) -> $_"
        Remove-Item $tmp -ErrorAction SilentlyContinue
        continue
    }

    $identical = $false
    if ($origBytes.Length -eq $newBytes.Length) {
        $identical = $true
        for ($i = 0; $i -lt $origBytes.Length; $i++) {
            if ($origBytes[$i] -ne $newBytes[$i]) { $identical = $false; break }
        }
    }

    if (-not $identical) {
        try {
            [System.IO.File]::WriteAllBytes($f.FullName, $newBytes)
            $changedFiles += $f.FullName
            if ($Verbose) { Write-Host "Formatted: $($f.FullName)" } else { Write-Host $f.FullName }
        } catch {
            Write-Warning "写回文件失败： $($f.FullName) -> $_"
        }
    } else {
        if ($Verbose) { Write-Host "Unchanged: $($f.FullName)" }
    }

    Remove-Item $tmp -ErrorAction SilentlyContinue
}

Write-Host "处理完成： $processed 个文件，已修改： $($changedFiles.Count) 个。"
if ($changedFiles.Count -gt 0) {
    Write-Host "已修改的文件列表："
    foreach ($cf in $changedFiles) { Write-Host $cf }
}
exit 0
