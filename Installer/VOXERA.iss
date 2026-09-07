#define AppVersion "1.0.0-beta"
[Setup]
; A distinct AppId from 0.8's, or installing this would be treated as an upgrade
; and remove the build that is being kept as the reference.
AppId={{2F1C8D46-7B03-4E59-A6D2-5C48E9137AB1}
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
