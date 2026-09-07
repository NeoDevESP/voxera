#define AppVersion "0.8.0"
[Setup]
AppId={{9B862B74-547A-4D28-9A0B-7541A0C456E8}
AppName=VOXERA
AppVersion={#AppVersion}
AppPublisher=Trevor Deey Labs
DefaultDirName={autopf}\VOXERA
DefaultGroupName=VOXERA
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=VOXERA-{#AppVersion}-Windows-x64-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\VOXERA.exe
[Files]
Source: "..\build\VOXERA_artefacts\Release\VST3\VOXERA.vst3\*"; DestDir: "{commoncf64}\VST3\VOXERA.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build\VOXERA_artefacts\Release\Standalone\VOXERA.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE_NOTES.md"; DestDir: "{app}"; Flags: ignoreversion
[Icons]
Name: "{group}\VOXERA"; Filename: "{app}\VOXERA.exe"
[Run]
Filename: "{app}\VOXERA.exe"; Description: "Abrir VOXERA"; Flags: nowait postinstall skipifsilent
