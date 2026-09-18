; ════════════════════════════════════════════════════════════════════════════
; DeliHub - Delivery Management System Installer
; Supports: Windows 10 (1809 / build 17763) and later, Windows 11 (all)
; Minimum enforced by Qt6 itself — nothing older than 1809 can run Qt6
; ════════════════════════════════════════════════════════════════════════════

#define AppName "DeliHub"
#define AppVersion "2.3.0"
#define AppPublisher "DeliHub"
#define AppExeName "DeliHub.exe"
#define AppURL "https://github.com/yourusername/delihub"

[Setup]
; Application identity
AppId={{A7F8B9C2-3D4E-5F6A-7B8C-9D0E1F2A3B4C}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}

; Installation paths
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes

; Output
OutputDir=..\installer_output
OutputBaseFilename=DeliHub_v{#AppVersion}_Setup
SetupIconFile=..\src\logo_installer.ico

; Compression
Compression=lzma2/ultra64
SolidCompression=yes

; Visual settings
WizardStyle=modern
WizardSizePercent=100,100

; Architecture (x64 only)
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; Privileges (require admin for Program Files installation)
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog

; Version info
VersionInfoVersion={#AppVersion}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} Installer
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}

; Uninstall
UninstallDisplayIcon={app}\{#AppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Main executable
Source: "..\dist\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; All DLLs in dist root (135 DLLs)
Source: "..\dist\*.dll"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

; Qt plugins
Source: "..\dist\generic\*"; DestDir: "{app}\generic"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\multimedia\*"; DestDir: "{app}\multimedia"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\sqldrivers\*"; DestDir: "{app}\sqldrivers"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion recursesubdirs createallsubdirs
; Translations folder is empty (windeployqt found no translations) - skip it
; Source: "..\dist\translations\*"; DestDir: "{app}\translations"; Flags: ignoreversion recursesubdirs createallsubdirs

; Runtime assets
Source: "..\dist\intro.mp4"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\logo.png"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\logo.ico"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\dist\config.ini"; DestDir: "{app}"; Flags: ignoreversion onlyifdoesntexist
Source: "..\dist\qt.conf"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\dist\ca-certificates.crt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\notification.mp3"; DestDir: "{app}"; Flags: ignoreversion

; PDF invoice reader tools (OCR pipeline)
Source: "..\dist\pdftotext.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\dist\pdftoppm.exe";  DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\dist\magick.exe";    DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\dist\tesseract.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\dist\tessdata\*";    DestDir: "{app}\tessdata"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; Excel invoice reader (Python zipapp + bundled runtime)
Source: "..\dist\xls_reader.pyz"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\dist\xls_reader.py";  DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\dist\python.exe";     DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\dist\libpython3.14.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\dist\python._pth";    DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\dist\python3.14\*";   DestDir: "{app}\python3.14"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; SVG icon engine plugin
Source: "..\dist\iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; Sidebar SVG icons
Source: "..\dist\sidebar\*"; DestDir: "{app}\sidebar"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
// ────────────────────────────────────────────────────────────────────────────
// Prerequisite checks and installation
// ────────────────────────────────────────────────────────────────────────────

// Check Windows version
// Minimum: Windows 10 version 1809 (build 17763) — the earliest Qt6 supports
// Also accepts all Windows 11 builds (22000+)
function IsSupportedWindowsVersion: Boolean;
var
  Version: TWindowsVersion;
begin
  GetWindowsVersionEx(Version);
  // Windows 11 and later: Major=10, Build >= 22000
  // Windows 10 1809+:     Major=10, Build >= 17763
  Result := (Version.Major > 10) or
            ((Version.Major = 10) and (Version.Minor = 0) and (Version.Build >= 17763));
end;

function InitializeSetup(): Boolean;
var
  Version: TWindowsVersion;
  VerStr: String;
begin
  if not IsSupportedWindowsVersion then
  begin
    GetWindowsVersionEx(Version);
    VerStr := IntToStr(Version.Major) + '.' + IntToStr(Version.Minor) +
              ' (build ' + IntToStr(Version.Build) + ')';
    MsgBox(
      'DeliHub requires Windows 10 version 1809 or later, or Windows 11.' + Chr(13) + Chr(10) +
      Chr(13) + Chr(10) +
      'Your Windows version: ' + VerStr + Chr(13) + Chr(10) +
      Chr(13) + Chr(10) +
      'Supported versions:' + Chr(13) + Chr(10) +
      '  - Windows 10  1809 (October 2018 Update)' + Chr(13) + Chr(10) +
      '  - Windows 10  1903, 1909, 2004, 20H2, 21H1, 21H2, 22H2' + Chr(13) + Chr(10) +
      '  - Windows 11  (all versions)',
      mbError, MB_OK);
    Result := False;
  end
  else
    Result := True;
end;

// No VC++ Redistributable required - we use MinGW/GCC runtime
// No Access Database Engine required - SQLite is default, PostgreSQL via libpq
// All dependencies are bundled in the dist folder

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    // Optional: Create an empty database file if it doesn't exist
    if not FileExists(ExpandConstant('{app}\delivery_system.db')) then
    begin
      // The app will create the DB on first run via schema_deployer
      Log('SQLite database will be created on first run');
    end;
  end;
end;

[UninstallDelete]
Type: files; Name: "{app}\*.log"
Type: files; Name: "{app}\*.db-shm"
Type: files; Name: "{app}\*.db-wal"
Type: files; Name: "{app}\app.log"
Type: filesandordirs; Name: "{app}\translations"

[Messages]
WelcomeLabel2=This will install [name/ver] on your computer.%n%nAll required dependencies are bundled - no Qt, MinGW, or other development tools are needed.%n%nRecommended: Close all other applications before continuing.
