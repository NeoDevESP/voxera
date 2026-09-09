#ifndef AppVersion
#define AppVersion "1.5.0"
#endif
[Setup]
; Its own AppId. Sharing one with either earlier build would make Windows treat
; this install as an upgrade of that build and uninstall it, which defeats the
; point of keeping them side by side.
AppId={{4E82D71A-9C36-4B05-A7F8-1D69B3E5C204}
AppName=VOXERA 1.5
AppVersion={#AppVersion}
AppPublisher=Trevor Deey Labs
DefaultDirName={autopf}\VOXERA 1.5
DefaultGroupName=VOXERA 1.5
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=VOXERA-1.5-{#AppVersion}-Windows-x64-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\VOXERA 1.5.exe
[Files]
Source: "..\build\VOXERA_artefacts\Release\VST3\VOXERA 1.5.vst3\*"; DestDir: "{commoncf64}\VST3\VOXERA 1.5.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build\VOXERA_artefacts\Release\Standalone\VOXERA 1.5.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE_NOTES.md"; DestDir: "{app}"; Flags: ignoreversion
[Icons]
Name: "{group}\VOXERA 1.5"; Filename: "{app}\VOXERA 1.5.exe"
[Run]
Filename: "{app}\VOXERA 1.5.exe"; Description: "Abrir VOXERA 1.5"; Flags: nowait postinstall skipifsilent
