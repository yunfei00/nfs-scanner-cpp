#define MyAppName "NFS Scanner"
#define MyAppExeName "NFSScanner.exe"

#ifndef AppVersion
#define AppVersion "0.0.0"
#endif

#ifndef OutputFile
#define OutputFile "NFSScanner-Setup"
#endif

[Setup]
AppId={{NFSScannerCpp-近场扫描系统}}
AppName={#MyAppName}
AppVersion={#AppVersion}
DefaultDirName={autopf}\NFS Scanner
DefaultGroupName=NFS Scanner
OutputDir=..\artifacts
OutputBaseFilename={#OutputFile}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=NFS Scanner

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
Source: "..\dist\NFSScanner\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\NFS Scanner\NFS Scanner"; Filename: "{app}\NFSScanner.exe"
Name: "{autodesktop}\NFS Scanner"; Filename: "{app}\NFSScanner.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\NFSScanner.exe"; Description: "Run NFS Scanner"; Flags: nowait postinstall skipifsilent
