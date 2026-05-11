#define MyAppName "NFS Scanner"
#define MyAppExeName "NFSScanner.exe"

#ifndef AppVersion
#define AppVersion "manual-build"
#endif

#ifndef OutputFile
#define OutputFile "NFSScanner-Setup-manual-build"
#endif

[Setup]
AppId={{8B89C52C-0552-4B6D-B15F-3347F2D5805B}
AppName={#MyAppName}
AppVersion={#AppVersion}
AppPublisher=NFS Scanner
DefaultDirName={autopf}\NFS Scanner
DefaultGroupName=NFS Scanner
DisableProgramGroupPage=no
OutputDir=..\dist_installer
OutputBaseFilename={#OutputFile}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
UninstallDisplayName={#MyAppName}
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\dist\NFSScanner\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\NFS Scanner\NFS Scanner"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\NFS Scanner"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
