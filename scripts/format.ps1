Param(
    [switch]$Verbose
)

# 格式化脚本（PowerShell）
# 用法：
#   .\scripts\format.ps1            # 递归格式化仓库下所有 .h .hpp .c .cpp .cc .cxx 文件（排除 build/.git 等）
#   .\scripts\format.ps1 -Verbose  # 详细输出

$extensions = @('.h', '.hpp', '.c', '.cpp', '.cc', '.cxx')
$excludePattern = '\\build\\|\\.git\\|/build/|/\.git/|\\.vscode\\|/\.vscode/'

# 查找 clang-format
$clang = Get-Command clang-format -ErrorAction SilentlyContinue
if (-not $clang) {
    Write-Error "clang-format 未找到。请先安装 clang-format 并将其加入 PATH（例如通过 vcpkg、choco 或 LLVM 官方安装包）。"
    exit 2
}

$files = Get-ChildItem -Path . -Recurse -File | Where-Object {
    $extensions -contains $_.Extension.ToLower() -and ($_.FullName -notmatch $excludePattern)
}

if (-not $files -or $files.Count -eq 0) {
    Write-Host "未找到需要格式化的源文件。"
    exit 0
}

$counter = 0
foreach ($f in $files) {
    if ($Verbose) { Write-Host "Formatting: $($f.FullName)" }
    & $clang.Path -i -style=file $f.FullName
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "clang-format 处理 $($f.FullName) 时返回错误（exit code: $LASTEXITCODE）。"
    } else {
        $counter++
    }
}

Write-Host "格式化完成： $counter 个文件已处理。"
exit 0
