; After Dark Studio -- Inno Setup 7 script
;
; Requires Inno Setup 7.0 or later for SetupArchitecture (7.1.0 was current when
; this was written). Either the 32-bit or 64-bit edition of the compiler can
; build this; the output is a native 64-bit installer.
;
; Deliberately NOT done here:
;   * nothing is copied into System32 -- SCRNSAVE.EXE takes a full path
;   * no screensaver registry keys are forced on the user at install time
;
; Setting the screensaver is a per-user choice the app makes at HKCU when the
; user clicks the button. An installer that silently commandeers the
; screensaver is exactly the behaviour this project exists to replace.

#define AppName        "After Dark Studio"
#define AppVersion     "0.1.0"
#define AppPublisher   "After Dark Studio contributors"
#define AppExe         "AfterDark.Studio.exe"
#define ScrName        "AfterDarkModern.scr"
#define SourceDir      "..\dist"

[Setup]
AppId={{9E2C4B71-6B3D-4E56-9D0A-3F5A7C1E8B42}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
VersionInfoVersion={#AppVersion}

; Inno Setup 7: build a native 64-bit installer. With this set,
; ArchitecturesAllowed and ArchitecturesInstallIn64BitMode both default to
; x64compatible, so 64-bit is the install mode without further directives.
SetupArchitecture=x64

; Windows 10 1809 and later. The app is built for Windows 11 but nothing here
; requires it, and the screensaver contract is unchanged across both.
MinVersion=10.0.17763

; Per-user by default: no elevation, no UAC prompt, nothing written outside the
; user's own profile. Someone who wants a machine-wide install can ask for it.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog commandline

DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
AllowNoIcons=yes
OutputDir=..\dist\installer
OutputBaseFilename=AfterDarkStudio-{#AppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#AppExe}
LicenseFile=..\LICENSE-NOTE.txt

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
; --- the 64-bit shell (self-contained: no .NET runtime prerequisite) ---
Source: "{#SourceDir}\{#AppExe}";  DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\*.dll";      DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\*.json";     DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\data\*";     DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; --- the 64-bit screensaver front end ---
Source: "{#SourceDir}\{#ScrName}"; DestDir: "{app}"; Flags: ignoreversion

; --- the 32-bit module host. THIS ONE MUST STAY 32-BIT. -------------------
; After Dark 4 modules are 32-bit DLLs bound to a 32-bit engine; a 64-bit
; process cannot load them. It lives in {app} like everything else -- a 32-bit
; executable runs perfectly well from 64-bit Program Files.
Source: "{#SourceDir}\admhost32.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}";        Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";  Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "Open {#AppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; The .scr writes nothing, but Studio's projected config is ours to remove.
Type: files;      Name: "{localappdata}\AfterDarkStudio\saver.cfg"
Type: dirifempty; Name: "{localappdata}\AfterDarkStudio"

[Code]
const
  DesktopKey = 'Control Panel\Desktop';

{ If our .scr is the active screensaver, stand down cleanly on uninstall.
  Leaving SCRNSAVE.EXE pointing at a deleted file gives the user a screensaver
  that silently does nothing and a settings dialog that looks broken. }
procedure DeactivateOurScreenSaver;
var
  Current, Ours: String;
begin
  Ours := ExpandConstant('{app}\{#ScrName}');
  if RegQueryStringValue(HKEY_CURRENT_USER, DesktopKey, 'SCRNSAVE.EXE', Current) then
  begin
    if CompareText(Trim(Current), Ours) = 0 then
    begin
      RegDeleteValue(HKEY_CURRENT_USER, DesktopKey, 'SCRNSAVE.EXE');
      RegWriteStringValue(HKEY_CURRENT_USER, DesktopKey, 'ScreenSaveActive', '0');
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  { Nothing to do at install time. The user chooses a module first; the app
    registers the screensaver only when they ask it to. }
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    DeactivateOurScreenSaver;
end;

function InitializeSetup(): Boolean;
begin
  Result := True;
end;
