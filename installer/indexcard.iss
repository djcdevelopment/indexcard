[Setup]
AppName=IndexCard
AppVersion=1.0
AppPublisher=Derek Ciula
AppPublisherURL=https://github.com/dciula/reader
AppSupportURL=https://github.com/dciula/reader/issues
AppUpdatesURL=https://github.com/dciula/reader/releases
DefaultDirName={autopf}\IndexCard
DefaultGroupName=IndexCard
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=IndexCard-1.0-Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
AppMutex=IndexCard_SingleInstance

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\build\Release\IndexCard.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\IndexCard"; Filename: "{app}\IndexCard.exe"
Name: "{autodesktop}\IndexCard"; Filename: "{app}\IndexCard.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"

[Run]
Filename: "{app}\IndexCard.exe"; Description: "Launch IndexCard"; Flags: nowait postinstall skipifsilent
