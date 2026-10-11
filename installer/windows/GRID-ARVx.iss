#define AppName "GRID-ARVx"
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
[Setup]
AppId={{E431C1A4-79B3-4517-A151-17CE74407AE9}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Kone Labie
DefaultDirName={autopf}\Kone Labie\GRID-ARVx
DefaultGroupName=GRID-ARVx
OutputBaseFilename=GRID-ARVx-Installer-Windows
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes
UninstallDisplayName=GRID-ARVx
[Types]
Name: "full"; Description: "Standalone and DAW plugins"
Name: "custom"; Description: "Select components"; Flags: iscustom
[Components]
Name: "app"; Description: "Standalone application"; Types: full custom
Name: "vst3"; Description: "VST3 plugin"; Types: full custom
Name: "clap"; Description: "CLAP plugin"; Types: full custom
[Files]
Source: "{#StageDir}\LICENSE-NOTICE.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\ThirdPartyNotices\*"; DestDir: "{app}\ThirdPartyNotices"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\Standalone\*"; DestDir: "{app}"; Components: app; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\VST3\*"; DestDir: "{commoncf}\VST3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\CLAP\*"; DestDir: "{commoncf}\CLAP"; Components: clap; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{autoprograms}\GRID-ARVx"; Filename: "{app}\GRID-ARVx.exe"; Components: app
[Run]
Filename: "{app}\GRID-ARVx.exe"; Description: "Open GRID-ARVx"; Components: app; Flags: postinstall nowait skipifsilent
