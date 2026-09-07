#define AppVersion "1.0.0-beta"
[Setup]
; Its own AppId. Sharing one with either earlier build would make Windows treat
; this install as an upgrade of that build and uninstall it, which defeats the
; point of keeping them side by side.
AppId={{7A4E9C21-D850-4F63-B19E-3D62A0F5C748}
AppName=VOXERA+CHOP
AppVersion={#AppVersion}
AppPublisher=Trevor Deey Labs
DefaultDirName={autopf}\VOXERA+CHOP
DefaultGroupName=VOXERA+CHOP
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=VOXERA+CHOP-{#AppVersion}-Windows-x64-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\VOXERA+CHOP.exe
[Files]
Source: "..\build\VOXERA_artefacts\Release\VST3\VOXERA+CHOP.vst3\*"; DestDir: "{commoncf64}\VST3\VOXERA+CHOP.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build\VOXERA_artefacts\Release\Standalone\VOXERA+CHOP.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE_NOTES.md"; DestDir: "{app}"; Flags: ignoreversion
[Icons]
Name: "{group}\VOXERA+CHOP"; Filename: "{app}\VOXERA+CHOP.exe"
[Run]
Filename: "{app}\VOXERA+CHOP.exe"; Description: "Abrir VOXERA+CHOP"; Flags: nowait postinstall skipifsilent
