param(
	[string]$Url1 = "https://raw.githubusercontent.com/davisking/dlib-models/refs/heads/master/taguchi_face_recognition_resnet_model_v1.dat.bz2",
	[string]$Url2 = "https://raw.githubusercontent.com/davisking/dlib-models/refs/heads/master/shape_predictor_5_face_landmarks.dat.bz2",
	[string]$OutDir = "$(Split-Path -Path $PSScriptRoot -Parent)\resources\models",
	[switch]$Force,
	[switch]$NoDecompress  # set this switch to skip decompression; decompression is ON by default
)

function Ensure-Dir([string]$d) {
	if (-not (Test-Path -Path $d)) {
		New-Item -ItemType Directory -Path $d -Force | Out-Null
	}
}

function Download-File([string]$url, [string]$outPath, [switch]$force) {
	if ((Test-Path $outPath) -and (-not $force)) {
		Write-Host "File already exists, skipping: $outPath"
		return $true
	}
	Write-Host "Downloading: $url -> $outPath"
	try {
		Invoke-WebRequest -Uri $url -OutFile $outPath -UseBasicParsing -ErrorAction Stop
		return $true
	} catch {
		Write-Host "Download failed: $_"
		return $false
	}
}

function Try-Decompress-BZ2([string]$bz2Path, [string]$outPath) {
	if (-not (Test-Path $bz2Path)) { return $false }
	# Prefer built-in Expand-Archive? Not for bzip2. Try 7z if available, else try bunzip2 from Git for Windows
	$sevenZ = Get-Command 7z -ErrorAction SilentlyContinue
	if ($sevenZ) {
		Write-Host "Decompressing with 7z: $bz2Path -> $outPath"
		& 7z e -so $bz2Path > $outPath
		return $true
	}
	$bunzip2 = Get-Command bunzip2 -ErrorAction SilentlyContinue
	if ($bunzip2) {
		Write-Host "Decompressing with bunzip2: $bz2Path -> $outPath"
		& bunzip2 -c $bz2Path > $outPath
		return $true
	}
	Write-Host "No bzip2 decompressor found (7z or bunzip2). Skipping decompression."
	return $false
}

Ensure-Dir $OutDir

$models = @(
	@{ url = $Url1 },
	@{ url = $Url2 }
)

# decompression enabled by default unless user passes -NoDecompress
$DoDecompress = -not $NoDecompress.IsPresent

foreach ($m in $models) {
	$u = $m.url
	$fn = [System.IO.Path]::GetFileName($u)
	if (-not $fn) { continue }
	$dest = Join-Path $OutDir $fn
	$ok = Download-File -url $u -outPath $dest -force:$Force
	if ($ok -and $DoDecompress) {
		if ($dest -like "*.bz2") {
			# Remove only the trailing .bz2 extension so files like name.dat.bz2 -> name.dat
			$outDat = $dest -replace '\.bz2$',''
			Try-Decompress-BZ2 $dest $outDat | Out-Null
		}
	}
}

Write-Host "Models are saved to: $OutDir"
