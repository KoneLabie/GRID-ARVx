#define AppName "GRID-ARVx"
#ifndef AppVersion
  #error AppVersion must be supplied by the packaging script
#endif
#ifndef StandaloneOnly
  #define StandaloneOnly 0
#endif
#ifndef BundleEncoder
  #define BundleEncoder 1
#endif
[Setup]
AppId={{E431C1A4-79B3-4517-A151-17CE74407AE9}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Cenaceae
#if Int(StandaloneOnly)
DefaultDirName={localappdata}\Programs\GRID-ARVx
PrivilegesRequired=lowest
MinVersion=10.0.19041
#else
DefaultDirName={autopf}\Cenaceae\GRID-ARVx
PrivilegesRequired=admin
#endif
DefaultGroupName=GRID-ARVx
OutputBaseFilename=GRID-ARVx-Installer-Windows
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes
UninstallDisplayName=GRID-ARVx
UninstallDisplayIcon={app}\GRID-ARVx.exe
#if Int(StandaloneOnly) == 0
[Types]
Name: "full"; Description: "Standalone and DAW plugins"
Name: "custom"; Description: "Select components"; Flags: iscustom
[Components]
Name: "app"; Description: "Standalone application"; Types: full custom
Name: "vst3"; Description: "VST3 plugin"; Types: full custom
Name: "clap"; Description: "CLAP plugin"; Types: full custom
#endif
[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"
[Files]
Source: "{#StageDir}\LICENSE-NOTICE.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\ThirdPartyNotices\*"; DestDir: "{app}\ThirdPartyNotices"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs
#if Int(BundleEncoder)
  #if Int(StandaloneOnly)
Source: "{#StageDir}\Tools\*"; DestDir: "{app}\tools"; Flags: ignoreversion recursesubdirs createallsubdirs
  #else
Source: "{#StageDir}\Tools\*"; DestDir: "{commoncf}\Cenaceae\GRID-ARVx\tools"; Flags: ignoreversion recursesubdirs createallsubdirs
  #endif
#endif
#if Int(StandaloneOnly)
Source: "{#StageDir}\Standalone\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\release-manifest.json"; DestDir: "{app}"; Flags: ignoreversion
#else
Source: "{#StageDir}\Standalone\*"; DestDir: "{app}"; Components: app; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\VST3\*"; DestDir: "{commoncf}\VST3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\CLAP\*"; DestDir: "{commoncf}\CLAP"; Components: clap; Flags: ignoreversion recursesubdirs createallsubdirs
#endif
[Icons]
#if Int(StandaloneOnly)
Name: "{autoprograms}\GRID-ARVx"; Filename: "{app}\GRID-ARVx.exe"
Name: "{autodesktop}\GRID-ARVx"; Filename: "{app}\GRID-ARVx.exe"; Tasks: desktopicon
#else
Name: "{autoprograms}\GRID-ARVx"; Filename: "{app}\GRID-ARVx.exe"; Components: app
Name: "{autodesktop}\GRID-ARVx"; Filename: "{app}\GRID-ARVx.exe"; Components: app; Tasks: desktopicon
#endif
[Run]
#if Int(StandaloneOnly)
Filename: "{app}\GRID-ARVx.exe"; Description: "Open GRID-ARVx"; Flags: postinstall nowait skipifsilent
#else
Filename: "{app}\GRID-ARVx.exe"; Description: "Open GRID-ARVx"; Components: app; Flags: postinstall nowait skipifsilent
#endif
