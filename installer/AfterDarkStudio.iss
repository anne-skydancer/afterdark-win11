; After Dark Studio -- Inno Setup 7 script
;
; Requires Inno Setup 7.0 or later for SetupArchitecture (7.1.0 was current when
; this was written). Either the 32-bit or 64-bit edition of the compiler can
; build this; the output is a native 64-bit installer.
;
; This is a SYSTEM-WIDE install: Program Files, all users, one copy.
;
; Note what "system-wide" can and cannot mean here. The program installs for
; every user. Which screensaver is *active*, however, is per-user by Windows
; design -- the Group Policy that controls it lives under User Configuration and
; operates on HKEY_CURRENT_USER\Control Panel\Desktop, and no HKLM equivalent is
; honoured by the shell. So this installer:
;
;   * installs the binaries once, for everyone
;   * records the install directory in HKLM so the .scr can find admhost32.exe
;     even when Windows launches it from System32
;   * optionally seeds a machine-wide DEFAULT configuration under ProgramData,
;     so any user who turns the screensaver on gets a working module
;   * optionally copies the .scr into System32 so it appears in every user's
;     Screen Saver dropdown
;
; It still does NOT switch anyone's screensaver on at install time. That remains
; each user's choice -- an installer that silently commandeers the screensaver is
; exactly the behaviour this project exists to replace. See docs/PACKAGING.md for
; how to enforce it across a fleet with Group Policy, which is the supported way.

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

; System-wide: elevate, install into Program Files, one copy for all users.
PrivilegesRequired=admin

DefaultDirName={commonpf}\{#AppName}
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
Name: "desktopicon"; Description: "Create a &desktop shortcut"; \
    GroupDescription: "Shortcuts:"; Flags: unchecked

; Windows lists screensavers found in the system directory. Registration works
; by full path without this, but the copy is what makes "After Dark Modern"
; appear in the dropdown for every user.
Name: "systemscr"; Description: "Show in every user's Screen Saver list"; \
    GroupDescription: "System-wide options:"

; Give accounts that have never opened Studio something that works.
Name: "machinedefault"; Description: "Seed a default configuration for all users"; \
    GroupDescription: "System-wide options:"

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

; Optional copy so the screensaver appears in every user's Windows dropdown.
; {sys} is the native System32 in a 64-bit install, which is what our x64 .scr
; wants. The copy still finds admhost32.exe via the HKLM InstallDir below.
Source: "{#SourceDir}\{#ScrName}"; DestDir: "{sys}"; Tasks: systemscr; \
    Flags: ignoreversion

[Registry]
; The .scr must not assume it sits beside admhost32.exe -- with the System32
; task it does not. This is how it finds the rest of the program.
Root: HKLM; Subkey: "SOFTWARE\AfterDarkStudio"; ValueType: string; \
    ValueName: "InstallDir"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\AfterDarkStudio"; ValueType: string; \
    ValueName: "Version"; ValueData: "{#AppVersion}"

[Dirs]
; Readable by everyone, writable by administrators -- the default ProgramData
; ACL is exactly right for a machine-wide default nobody should edit casually.
Name: "{commonappdata}\AfterDarkStudio"; Tasks: machinedefault

[Icons]
Name: "{group}\{#AppName}";        Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#AppName}";  Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "Open {#AppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; The machine-wide default is ours. Per-user settings are the user's own and
; are deliberately left alone -- an uninstall should not delete someone's
; preferences from their profile.
Type: files;      Name: "{commonappdata}\AfterDarkStudio\saver.cfg"
Type: dirifempty; Name: "{commonappdata}\AfterDarkStudio"

[Code]
const
  DesktopKey = 'Control Panel\Desktop';

function InstallDirScr(): String;
begin
  Result := ExpandConstant('{app}\{#ScrName}');
end;

function SystemDirScr(): String;
begin
  Result := ExpandConstant('{sys}\{#ScrName}');
end;

{ Seed a machine-wide default so an account that has never opened Studio still
  gets a working screensaver if it turns one on. Finding an After Dark
  installation is best-effort: if there is none we write nothing, and the .scr
  falls back to doing nothing rather than to something broken. }
procedure SeedMachineDefault;
var
  Dir, Cfg, Engine, Module: String;
  Candidates: array[0..4] of String;
  I: Integer;
  Found: Boolean;
begin
  Candidates[0] := ExpandConstant('{commonpf32}\After Dark\FILES\AD40');
  Candidates[1] := ExpandConstant('{commonpf32}\After Dark');
  Candidates[2] := ExpandConstant('{commonpf}\After Dark\FILES\AD40');
  Candidates[3] := 'C:\AFTERDRK\FILES\AD40';
  Candidates[4] := 'C:\AFTERDRK';

  Found := False;
  Dir := '';
  for I := 0 to 4 do
  begin
    Engine := Candidates[I] + '\ADXPL510.DLL';
    if FileExists(Engine) then
    begin
      Dir := Candidates[I];
      Found := True;
      Break;
    end;
  end;
  if not Found then Exit;

  { TOASTERS.AD is the one everyone means by "After Dark". If it is absent we
    leave the module blank; the .scr treats an unconfigured file as "nothing to
    do" and simply blanks, which is the right failure. }
  Module := Dir + '\TOASTERS.AD';
  if not FileExists(Module) then Module := '';

  Cfg := ExpandConstant('{commonappdata}\AfterDarkStudio\saver.cfg');
  ForceDirectories(ExpandConstant('{commonappdata}\AfterDarkStudio'));

  SaveStringToFile(Cfg,
    '# Machine-wide default, written by Setup.' + #13#10 +
    '# A user''s own %LOCALAPPDATA%\AfterDarkStudio\saver.cfg overrides this.' + #13#10 +
    'install=' + Dir + #13#10 +
    'module=' + Module + #13#10 +
    'studio=' + ExpandConstant('{app}\{#AppExe}') + #13#10 +
    'controls=0,0,0,0' + #13#10 +
    'fps=30' + #13#10 +
    'scale=integer' + #13#10 +
    'bpp=8' + #13#10 +
    'width=640' + #13#10 +
    'height=480' + #13#10, False);
end;

{ If this user's active screensaver is one of our copies, stand down cleanly.
  Leaving SCRNSAVE.EXE pointing at a deleted file gives a screensaver that
  silently does nothing and a settings dialog that looks broken.

  This can only fix the account running the uninstaller: the setting lives in
  each user's own hive, and an uninstaller has no business walking other
  people's profiles. Any other user whose screensaver was ours will pick a new
  one from the dialog, which is a visible and recoverable state. }
procedure DeactivateOurScreenSaver;
var
  Current: String;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, DesktopKey, 'SCRNSAVE.EXE', Current) then
    Exit;
  Current := Trim(Current);
  if (CompareText(Current, InstallDirScr()) = 0) or
     (CompareText(Current, SystemDirScr()) = 0) then
  begin
    RegDeleteValue(HKEY_CURRENT_USER, DesktopKey, 'SCRNSAVE.EXE');
    RegWriteStringValue(HKEY_CURRENT_USER, DesktopKey, 'ScreenSaveActive', '0');
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('machinedefault') then
    SeedMachineDefault;
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
