param(
    [string]$Version = 'v19.24.9',
    [string]$Dest = "$(Split-Path -Path $PSScriptRoot -Parent)\third_party\dlib",
    [string]$Preset = 'msvc-vcpkg-cuda-release'
)

Write-Host "Cloning dlib $Version into $Dest"
if (-Not (Test-Path -Path $Dest)) {
    New-Item -ItemType Directory -Path $Dest -Force | Out-Null
}

Push-Location $Dest
try {
    # Ensure this is a git repo and origin exists
    $isGitRepo = Test-Path -Path "$Dest\.git"
    if (-Not $isGitRepo) {
        git init .
    }

    $originUrl = git remote get-url origin 2>$null
    if (-not $originUrl) {
        git remote remove origin 2>$null | Out-Null
        git remote add origin https://github.com/davisking/dlib.git
        $originUrl = "https://github.com/davisking/dlib.git"
    }

    # Resolve the remote commit hash for the requested $Version (tag/branch/commit-ish)
    $ls = git ls-remote --refs origin $Version 2>$null
    if (-not $ls) {
        # fallback: try tags/refs explicitly
        $ls = git ls-remote origin "refs/tags/$Version" 2>$null
    }
    $desiredHash = $null
    if ($ls) {
        $firstLine = ($ls -split "`n" | Select-Object -First 1).Trim()
        if ($firstLine) {
            $desiredHash = ($firstLine -split '\s+')[0]
        }
    }

    # Get local HEAD (if any)
    $localHash = $null
    try {
        $localHash = git rev-parse --verify HEAD 2>$null
        if ($localHash) { $localHash = $localHash.Trim() }
    } catch {
        $localHash = $null
    }

    if ($desiredHash -and $localHash -and ($desiredHash -eq $localHash)) {
        Write-Host "dlib already at $Version ($desiredHash) — skipping fetch/checkout"
    } else {
        Write-Host "Fetching $Version from $originUrl"
        git fetch --depth 1 origin $Version
        git checkout -q FETCH_HEAD
    }
    Write-Host "Configuring and building dlib with CMake preset: $Preset"
    cmake --preset $Preset
    cmake --build --preset $Preset
    # cmake --install expects a binary directory, not a preset name
    $binaryDir = Join-Path $Dest "build\$Preset"
    Write-Host "Installing from binary dir: $binaryDir"
    cmake --install "$binaryDir"
    Write-Host "dlib installed to: $Dest/install/dlib/$Preset"
} finally {
    Pop-Location
}
