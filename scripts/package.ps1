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

# 1. 校验
if (!(Test-Path "$buildDir\$AppName.exe")) {
    Write-Error "未找到 $Config 构建产物，请先执行: cmake --build build --config $Config --target $AppName"
}

# 2. 清理并创建暂存区
if (Test-Path $stagingDir) {
    Remove-Item -Recurse -Force $stagingDir
}
New-Item -ItemType Directory -Force -Path "$stagingDir\data\output" | Out-Null
New-Item -ItemType Directory -Force -Path "$stagingDir\data\upload\video" | Out-Null
New-Item -ItemType Directory -Force -Path "$stagingDir\data\upload\img" | Out-Null

# 3. 复制运行时文件（排除测试与构建中间文件）
Write-Host "复制文件中..."
$exclude = @("*.pdb", "*.ilk", "*.exp", "*.lib", "test_*", "vc_red*")
Get-ChildItem $buildDir -File | Where-Object {
    foreach ($pat in $exclude) {
        if ($_.Name -like $pat) { return $false }
    }
    return $true
} | Copy-Item -Destination $stagingDir -Force

# 复制子目录（models, openface_model, AU_predictors, Qt插件目录等）
$copyDirs = @("models", "openface_model", "AU_predictors",
              "platforms", "styles", "imageformats", "iconengines",
              "generic", "tls", "translations", "multimedia",
              "networkinformation")
foreach ($dir in $copyDirs) {
    $src = Join-Path $buildDir $dir
    if (Test-Path $src) {
        Copy-Item -Recurse -Force $src "$stagingDir\"
    }
}

# 4. 复制启动脚本
Copy-Item -Force "$repoRoot\scripts\OmniFace.bat" "$stagingDir\"

Write-Host "复制完成，总大小: $("{0:N0} MB" -f ((Get-ChildItem $stagingDir -Recurse | Measure-Object -Property Length -Sum).Sum / 1MB))" -ForegroundColor Green

# 5. 打包为 zip
Write-Host "创建 zip 包..."
$zipPath = "$OutputDir\${AppName}_v$(& "$stagingDir\$AppName.exe" --version 2>$null).zip"
if ($LASTEXITCODE -ne 0) {
    $zipPath = "$OutputDir\${AppName}_portable.zip"
}

if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
Compress-Archive -Path $stagingDir -DestinationPath $zipPath -CompressionLevel Optimal

$zipSize = "{0:N0} MB" -f ((Get-Item $zipPath).Length / 1MB)
Write-Host "打包完成: $zipPath ($zipSize)" -ForegroundColor Green

# 6. 输出部署说明
Write-Host @"

=== 部署方式 ===

方式一（便携版）：解压 zip 到任意目录，双击 OmniFace.bat 启动

方式二（安装版）：安装 Inno Setup (https://jrsoftware.org/isdl.php) 后，
    用 scripts/installer.iss 构建安装包

"@ -ForegroundColor Yellow
