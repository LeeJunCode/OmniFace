param(
    [string]$Config = "Release",
    [string]$OutputDir = "",
    [string]$AppName = "OmniFace"
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path "$PSScriptRoot\.."
if ($OutputDir -eq "") { $OutputDir = Join-Path $repoRoot "dist" }
$buildDir = Join-Path $repoRoot "build\app\$Config"
$stagingDir = Join-Path $OutputDir $AppName

Write-Host "=== OmniFace Packaging ===" -ForegroundColor Cyan
Write-Host "Source : $buildDir"
Write-Host "Staging: $stagingDir"

# 1. Get version (git first, fallback to date)
$version = $null
try {
    $version = git -C $repoRoot describe --tags --always --dirty 2>$null
    if ($LASTEXITCODE -ne 0) { $version = $null }
} catch { $version = $null }
if (!$version) {
    $version = (Get-Date -Format "yyyyMMdd")
}
Write-Host "Version: $version" -ForegroundColor Cyan

# 2. 验证构建产物存在
if (!(Test-Path "$buildDir\$AppName.exe")) {
    Write-Error "Build output not found: $buildDir\$AppName.exe. Build first: cmake --build build --config $Config --target $AppName"
}

# 3. 清理并创建暂存目录
if (Test-Path $stagingDir) {
    Remove-Item -Recurse -Force $stagingDir
}
New-Item -ItemType Directory -Force -Path "$stagingDir\data\output" | Out-Null
New-Item -ItemType Directory -Force -Path "$stagingDir\data\upload\video" | Out-Null
New-Item -ItemType Directory -Force -Path "$stagingDir\data\upload\img" | Out-Null

# 4. 复制运行时文件（排除调试符号和构建产物）
Write-Host "Copying files..."
$exclude = @("*.pdb", "*.ilk", "*.exp", "*.lib", "test_*", "vc_red*")
Get-ChildItem $buildDir -File | Where-Object {
    foreach ($pat in $exclude) {
        if ($_.Name -like $pat) { return $false }
    }
    return $true
} | Copy-Item -Destination $stagingDir -Force

# 复制子目录（Qt 插件、模型等）
$copyDirs = @("models", "openface_model", "AU_predictors", "platforms", "styles", "imageformats", "iconengines", "generic", "tls", "translations", "multimedia", "networkinformation")
$copied = 0
$missing = 0
foreach ($dir in $copyDirs) {
    $src = Join-Path $buildDir $dir
    if (Test-Path $src) {
        Copy-Item -Recurse -Force $src "$stagingDir\"
        Write-Host "  + $dir" -ForegroundColor Green
        $copied = $copied + 1
    } else {
        Write-Host "  - $dir (not found in build output, skipped)" -ForegroundColor Yellow
        $missing = $missing + 1
    }
}
Write-Host "Subdirs copied: $copied, missing: $missing"

# 验证关键目录存在
foreach ($d in @("platforms", "models")) {
    $checkPath = Join-Path $stagingDir $d
    if (!(Test-Path $checkPath)) {
        Write-Error "CRITICAL: $checkPath not found - app may not run!"
    }
}

# 5. 复制启动脚本
Copy-Item -Force "$repoRoot\scripts\OmniFace.bat" "$stagingDir\"

# 6. 复制配置文件
$configSrc = "$repoRoot\config.yaml"
if (Test-Path $configSrc) {
    Copy-Item -Force $configSrc "$stagingDir\"
} elseif (!(Test-Path "$stagingDir\config.yaml")) {
    Write-Host "WARNING: config.yaml not found, app will use defaults" -ForegroundColor Yellow
}

$totalSize = "{0:N0} MB" -f ((Get-ChildItem $stagingDir -Recurse | Measure-Object -Property Length -Sum).Sum / 1MB)
Write-Host "Copy done, total size: $totalSize" -ForegroundColor Green

# 7. 创建 zip 压缩包
Write-Host "Creating zip..."
$zipName = "${AppName}_v${version}.zip"
$zipPath = "$OutputDir\$zipName"
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
Compress-Archive -Path $stagingDir -DestinationPath $zipPath -CompressionLevel Optimal

$zipSize = "{0:N0} MB" -f ((Get-Item $zipPath).Length / 1MB)
Write-Host "Zip created: $zipPath ($zipSize)" -ForegroundColor Green

# 8. 打印部署说明
Write-Host ""
Write-Host "=== Deployment ===" -ForegroundColor Yellow
Write-Host "Portable: extract $zipName, double-click OmniFace.bat"
Write-Host "Installer (Inno Setup): iscc scripts\installer.iss /DAppVersion=$version"
Write-Host ""
