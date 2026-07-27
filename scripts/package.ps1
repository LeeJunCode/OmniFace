param(
    [string]$Config = "Release",
    [string]$OutputDir = "$PSScriptRoot\..\dist",
    [string]$AppName = "OmniFace"
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path "$PSScriptRoot\.."
$buildDir = "$repoRoot\build\app\$Config"
$stagingDir = "$OutputDir\$AppName"

Write-Host "=== OmniFace 部署打包 ===" -ForegroundColor Cyan
Write-Host "源目录 : $buildDir"
Write-Host "暂存区 : $stagingDir"

# 1. 获取版本号（优先 git，回退到日期）
$version = $null
try {
    $version = git -C $repoRoot describe --tags --always --dirty 2>$null
    if ($LASTEXITCODE -ne 0) { $version = $null }
} catch { $version = $null }
if (!$version) {
    $version = (Get-Date -Format "yyyyMMdd")
}
Write-Host "版本号 : $version" -ForegroundColor Cyan

# 2. 校验构建产物
if (!(Test-Path "$buildDir\$AppName.exe")) {
    Write-Error "未找到 $Config 构建产物，请先执行: cmake --build build --config $Config --target $AppName"
}

# 3. 清理并创建暂存区
if (Test-Path $stagingDir) {
    Remove-Item -Recurse -Force $stagingDir
}
New-Item -ItemType Directory -Force -Path "$stagingDir\data\output" | Out-Null
New-Item -ItemType Directory -Force -Path "$stagingDir\data\upload\video" | Out-Null
New-Item -ItemType Directory -Force -Path "$stagingDir\data\upload\img" | Out-Null

# 4. 复制运行时文件（排除调试符号与构建中间文件）
Write-Host "复制文件中..."
$exclude = @("*.pdb", "*.ilk", "*.exp", "*.lib", "test_*", "vc_red*")
Get-ChildItem $buildDir -File | Where-Object {
    foreach ($pat in $exclude) {
        if ($_.Name -like $pat) { return $false }
    }
    return $true
} | Copy-Item -Destination $stagingDir -Force

# 复制子目录（Qt 插件、模型等）
$copyDirs = @("models", "openface_model", "AU_predictors", "platforms", "styles", "imageformats", "iconengines", "generic", "tls", "translations", "multimedia", "networkinformation")
foreach ($dir in $copyDirs) {
    $src = Join-Path $buildDir $dir
    if (Test-Path $src) {
        Copy-Item -Recurse -Force $src "$stagingDir\"
    }
}

# 5. 复制启动脚本
Copy-Item -Force "$repoRoot\scripts\OmniFace.bat" "$stagingDir\"

# 6. 确保 config.yaml 存在
$configSrc = "$repoRoot\config.yaml"
if (Test-Path $configSrc) {
    Copy-Item -Force $configSrc "$stagingDir\"
} elseif (!(Test-Path "$stagingDir\config.yaml")) {
    Write-Warning "未找到 config.yaml，应用将使用默认配置"
}

$totalSize = "{0:N0} MB" -f ((Get-ChildItem $stagingDir -Recurse | Measure-Object -Property Length -Sum).Sum / 1MB)
Write-Host "复制完成，总大小: $totalSize" -ForegroundColor Green

# 7. 打包为 zip
Write-Host "创建 zip 包..."
$zipName = "${AppName}_v${version}.zip"
$zipPath = "$OutputDir\$zipName"
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
Compress-Archive -Path $stagingDir -DestinationPath $zipPath -CompressionLevel Optimal

$zipSize = "{0:N0} MB" -f ((Get-Item $zipPath).Length / 1MB)
Write-Host "打包完成: $zipPath ($zipSize)" -ForegroundColor Green

# 8. 输出部署说明
Write-Host @"

=== 部署方式 ===

方式一（便携版）：
    解压 $zipName 到任意目录，双击 OmniFace.bat 启动

方式二（安装版）：
    安装 Inno Setup 后执行: iscc scripts\installer.iss /DAppVersion=$version

"@ -ForegroundColor Yellow
