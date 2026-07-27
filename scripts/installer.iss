; Inno Setup 安装脚本 — OmniFace
; 使用前请先运行 scripts/package.ps1 生成 dist/OmniFace 目录
;
; 构建安装包:
;   iscc scripts\installer.iss /DAppVersion=1.0.0
;   (不指定版本则使用默认值)
;
; 下载 Inno Setup: https://jrsoftware.org/isdl.php

#ifndef AppVersion
#define AppVersion "1.0.0"
#endif

#define MyAppName "OmniFace"
#define MyAppPublisher "OmniFace"
#define MyAppExeName "OmniFace.exe"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#AppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=..\dist
OutputBaseFilename=OmniFace_Setup_v{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=..\resources\omniface.ico
UninstallDisplayIcon={app}\OmniFace.exe

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加图标:"

[Dirs]
Name: "{app}\data\output"
Name: "{app}\data\upload\video"
Name: "{app}\data\upload\img"
Name: "{app}\data\log"

[Files]
Source: "..\dist\OmniFace\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\OmniFace"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{group}\卸载 OmniFace"; Filename: "{uninstallexe}"
Name: "{autodesktop}\OmniFace"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "启动 OmniFace"; Flags: nowait postinstall skipifsilent
