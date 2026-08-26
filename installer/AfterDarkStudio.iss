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
;
; SCREEN SAVERS. This installer ships none, and must not: they are Berkeley
; Systems' copyrighted work. Instead it IMPORTS them, at install time, from the
; user's own disc or existing installation, and copies them into {app}\modules.
;
; That is not a workaround, it is the point. After Dark 4's own installer is
; 16-bit and cannot run on Windows 11 at all, so someone holding the CD has no
; supported way to install it. This installer replaces that broken step: point it
; at the disc and everything works afterwards, with nothing licensed travelling
; in the download.
;
; For deploying to machines you own, see BundleModulesFrom at the top of this
; file -- it embeds your own modules into the installer. The result contains
; licensed content and is not redistributable.

; Set this to a folder to EMBED modules in the installer instead of importing
; them at install time. For deploying to machines you own. The resulting
; installer contains Berkeley Systems' copyrighted content and must not be
; redistributed. Leave undefined for the normal, shippable build.
; #define BundleModulesFrom "C:\Program Files (x86)\After Dark"

#define AppName        "After Dark Studio"
#define AppVersion     "0.1.6"
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

; --- independent clean-room Classic rewrites -----------------------------
; These contain no Berkeley Systems code or assets and need no ADXPL engine.
Source: "{#SourceDir}\rewrites\*.AD"; DestDir: "{app}\modules\rewrites"; \
  Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; Optional copy so the screensaver appears in every user's Windows dropdown.
; {sys} is the native System32 in a 64-bit install, which is what our x64 .scr
; wants. The copy still finds admhost32.exe via the HKLM InstallDir below.
Source: "{#SourceDir}\{#ScrName}"; DestDir: "{sys}"; Tasks: systemscr; \
    Flags: ignoreversion

#ifdef BundleModulesFrom
; Embedded modules -- see the warning at the top of this file.
Source: "{#BundleModulesFrom}\*.AD";       DestDir: "{app}\modules"; \
    Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#BundleModulesFrom}\ADXPL*.DLL"; DestDir: "{app}\modules"; \
    Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#BundleModulesFrom}\PICTURES\*.BMP"; DestDir: "{app}\modules\PICTURES"; \
  Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BundleModulesFrom}\PICTURES\*.GIF"; DestDir: "{app}\modules\PICTURES"; \
  Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BundleModulesFrom}\PICTURES\*.JPG"; DestDir: "{app}\modules\PICTURES"; \
  Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BundleModulesFrom}\PICTURES\*.JPEG"; DestDir: "{app}\modules\PICTURES"; \
  Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BundleModulesFrom}\3DMINOR.MID"; DestDir: "{app}\modules\Music"; \
  DestName: "3DMinor.MID"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BundleModulesFrom}\FIREBOMB.MID"; DestDir: "{app}\modules\Music"; \
  DestName: "FIREBOMB.MID"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BundleModulesFrom}\SEAPIXIE.MID"; DestDir: "{app}\modules\Music"; \
  DestName: "SEAPIXIE.mid"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BundleModulesFrom}\BABY.MID"; DestDir: "{app}\modules\Music"; \
  DestName: "Baby Toasters.mid"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BundleModulesFrom}\TOASTERS.MID"; DestDir: "{app}\modules\Music"; \
  DestName: "Flying Toasters.mid"; Flags: ignoreversion skipifsourcedoesntexist
#endif

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
; Imported at install time rather than installed by [Files], so the uninstaller
; does not track them.
Type: filesandordirs; Name: "{app}\modules"
; The machine-wide default is ours. Per-user settings are the user's own and
; are deliberately left alone -- an uninstall should not delete someone's
; preferences from their profile.
Type: files;      Name: "{commonappdata}\AfterDarkStudio\saver.cfg"
Type: dirifempty; Name: "{commonappdata}\AfterDarkStudio"

[Code]
const
  DesktopKey  = 'Control Panel\Desktop';
  EngineAD4   = 'ADXPL510.DLL';
  EngineAD3   = 'ADXPL300.DLL';
  MaxDepth    = 5;

var
  SourcePage: TInputDirWizardPage;
  FoundAD4Dir, FoundAD3Dir, FoundStarry: String;

function InstallDirScr(): String;
begin
  Result := ExpandConstant('{app}\{#ScrName}');
end;

function SystemDirScr(): String;
begin
  Result := ExpandConstant('{sys}\{#ScrName}');
end;

function ModulesDir(): String;
begin
  Result := ExpandConstant('{app}\modules');
end;

{ ---------------------------------------------------------------- searching }

{ Depth-limited search for a file, returning the directory that holds it.
  Layouts vary: the CD has ADE\FILES\AD40, an existing install may have almost
  anything, so we look rather than assume. }
function FindDirContaining(Root, Leaf: String; Depth: Integer): String;
var
  FindRec: TFindRec;
  Sub: String;
begin
  Result := '';
  if (Depth > MaxDepth) or not DirExists(Root) then Exit;

  if FileExists(AddBackslash(Root) + Leaf) then
  begin
    Result := Root;
    Exit;
  end;

  if FindFirst(AddBackslash(Root) + '*', FindRec) then
  try
    repeat
      if (FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
      begin
        if (FindRec.Name <> '.') and (FindRec.Name <> '..') then
        begin
          Sub := FindDirContaining(AddBackslash(Root) + FindRec.Name, Leaf, Depth + 1);
          if Sub <> '' then
          begin
            Result := Sub;
            Exit;
          end;
        end;
      end;
    until not FindNext(FindRec);
  finally
    FindClose(FindRec);
  end;
end;

procedure ScanSource(Root: String);
begin
  FoundAD4Dir := FindDirContaining(Root, EngineAD4, 0);
  FoundAD3Dir := FindDirContaining(Root, EngineAD3, 0);
  FoundStarry := FindDirContaining(Root, 'STARRYNI.AD', 0);
end;

{ Somewhere plausible to start: an existing install, or a mounted disc. }
function GuessSource(): String;
var
  Roots: array[0..4] of String;
  I: Integer;
  Drive: String;
begin
  Roots[0] := ExpandConstant('{commonpf32}\After Dark');
  Roots[1] := ExpandConstant('{commonpf}\After Dark');
  Roots[2] := 'C:\AFTERDRK';
  Roots[3] := ExpandConstant('{commonpf32}\Berkeley Systems\After Dark');
  Roots[4] := 'D:\';

  for I := 0 to 4 do
    if (Roots[I] <> '') and DirExists(Roots[I]) then
      if FindDirContaining(Roots[I], EngineAD4, 0) <> '' then
      begin
        Result := Roots[I];
        Exit;
      end;

  { A disc in any optical drive. Pascal Script has no Char loop variable. }
  for I := Ord('D') to Ord('H') do
  begin
    Drive := Chr(I) + ':\';
    if DirExists(Drive) then
      if FindDirContaining(Drive, EngineAD4, 0) <> '' then
      begin
        Result := Drive;
        Exit;
      end;
  end;

  Result := '';
end;

{ ----------------------------------------------------------------- copying }

function CopyPattern(SrcDir, DestDir, Pattern: String): Integer;
var
  FindRec: TFindRec;
begin
  Result := 0;
  if not DirExists(SrcDir) then Exit;
  ForceDirectories(DestDir);

  if FindFirst(AddBackslash(SrcDir) + Pattern, FindRec) then
  try
    repeat
      if (FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY) = 0 then
      begin
        WizardForm.StatusLabel.Caption := 'Importing ' + FindRec.Name + '...';
        WizardForm.Refresh;
        if CopyFile(AddBackslash(SrcDir) + FindRec.Name,
              AddBackslash(DestDir) + FindRec.Name, False) then
          Result := Result + 1;
      end;
    until not FindNext(FindRec);
  finally
    FindClose(FindRec);
  end;
end;

function CopyNamedFile(SrcDir, SrcName, DestDir, DestName: String): Integer;
begin
  Result := 0;
  if not FileExists(AddBackslash(SrcDir) + SrcName) then Exit;
  ForceDirectories(DestDir);
  if CopyFile(AddBackslash(SrcDir) + SrcName,
              AddBackslash(DestDir) + DestName, False) then
    Result := 1;
end;

{ Import the user's own screen savers.

  AD40 modules and the engine go to modules\, and the Classic set to
  modules\classic -- they must stay apart because RAIN.AD exists in both.
  Classic modules are 16-bit and cannot run on 64-bit Windows, but the
  catalogue reads them, so they are worth importing. }
function ImportModules(Root: String): Integer;
var
  N: Integer;
begin
  Result := 0;
  ScanSource(Root);
  if FoundAD4Dir = '' then Exit;

  N := CopyPattern(FoundAD4Dir, ModulesDir(), '*.AD');
  CopyPattern(FoundAD4Dir, ModulesDir(), 'ADXPL*.DLL');
  CopyPattern(FoundAD4Dir + '\PICTURES', ModulesDir() + '\PICTURES', '*.BMP');
  CopyPattern(FoundAD4Dir + '\PICTURES', ModulesDir() + '\PICTURES', '*.GIF');
  CopyPattern(FoundAD4Dir + '\PICTURES', ModulesDir() + '\PICTURES', '*.JPG');
  CopyPattern(FoundAD4Dir + '\PICTURES', ModulesDir() + '\PICTURES', '*.JPEG');
  CopyNamedFile(FoundAD4Dir, '3DMINOR.MID', ModulesDir() + '\Music', '3DMinor.MID');
  CopyNamedFile(FoundAD4Dir, 'FIREBOMB.MID', ModulesDir() + '\Music', 'FIREBOMB.MID');
  CopyNamedFile(FoundAD4Dir, 'SEAPIXIE.MID', ModulesDir() + '\Music', 'SEAPIXIE.mid');
  CopyNamedFile(FoundAD4Dir, 'BABY.MID', ModulesDir() + '\Music', 'Baby Toasters.mid');
  CopyNamedFile(FoundAD4Dir, 'TOASTERS.MID', ModulesDir() + '\Music', 'Flying Toasters.mid');
  Result := N;

  if (FoundStarry <> '') and (CompareText(FoundStarry, FoundAD4Dir) <> 0) then
    Result := Result + CopyPattern(FoundStarry, ModulesDir(), 'STARRYNI.AD');

  if FoundAD3Dir <> '' then
  begin
    Result := Result + CopyPattern(FoundAD3Dir, ModulesDir() + '\classic', '*.AD');
    CopyPattern(FoundAD3Dir, ModulesDir() + '\classic', 'ADXPL*.DLL');
  end;
end;

{ ------------------------------------------------------------------- wizard }

procedure InitializeWizard();
var
  Guess: String;
begin
  SourcePage := CreateInputDirPage(wpSelectTasks,
    'Your After Dark screen savers',
    'Where are your After Dark files?',
    'After Dark Studio ships no screen savers -- it uses the ones from the copy' + #13#10 +
    'you already own. Point Setup at your After Dark CD or an existing' + #13#10 +
    'installation and the modules will be copied in.' + #13#10 + #13#10 +
    'After Dark 4''s own installer is 16-bit and cannot run on Windows 11, so' + #13#10 +
    'importing from the disc here replaces that step entirely.' + #13#10 + #13#10 +
    'You can leave this blank and choose a folder later in the app.',
    False, '');
  SourcePage.Add('');

  Guess := GuessSource();
  SourcePage.Values[0] := Guess;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  Root: String;
begin
  Result := True;
  if CurPageID <> SourcePage.ID then Exit;

  Root := Trim(SourcePage.Values[0]);
  if Root = '' then Exit;   { skipping is allowed }

  if not DirExists(Root) then
  begin
    MsgBox('That folder does not exist.', mbError, MB_OK);
    Result := False;
    Exit;
  end;

  ScanSource(Root);
  if FoundAD4Dir = '' then
    Result := MsgBox('No After Dark engine (' + EngineAD4 + ') was found under:'
      + #13#10#13#10 + Root + #13#10#13#10
      + 'Setup can continue, but no screen savers will be imported and the app '
      + 'will have nothing to show until you point it at your files.'
      + #13#10#13#10 + 'Continue anyway?', mbConfirmation, MB_YESNO) = IDYES;
end;

{ ------------------------------------------------------------- config seed }

procedure WriteMachineDefault(InstallPath, ModulePath: String);
var
  Cfg: String;
begin
  Cfg := ExpandConstant('{commonappdata}\AfterDarkStudio\saver.cfg');
  ForceDirectories(ExpandConstant('{commonappdata}\AfterDarkStudio'));
  SaveStringToFile(Cfg,
    '# Machine-wide default, written by Setup.' + #13#10 +
    '# A user''s own %LOCALAPPDATA%\AfterDarkStudio\saver.cfg overrides this.' + #13#10 +
    'install=' + InstallPath + #13#10 +
    'module=' + ModulePath + #13#10 +
    'studio=' + ExpandConstant('{app}\{#AppExe}') + #13#10 +
    'controls=0,0,0,0' + #13#10 +
    'fps=30' + #13#10 +
    'scale=integer' + #13#10 +
    'bpp=8' + #13#10 +
    'width=640' + #13#10 +
    'height=480' + #13#10, False);
end;

{ Pick something worth showing by default. Flying Toasters is what people mean
  by After Dark; Starry Night is the engine's own default and needs no engine
  DLL at all, so it is the safest fallback. }
function DefaultModule(Dir: String): String;
var
  Candidates: array[0..3] of String;
  I: Integer;
begin
  Candidates[0] := 'TOASTERS.AD';
  Candidates[1] := 'STARRYNI.AD';
  Candidates[2] := 'FISH.AD';
  Candidates[3] := 'BADDOG.AD';
  for I := 0 to 3 do
    if FileExists(AddBackslash(Dir) + Candidates[I]) then
    begin
      Result := AddBackslash(Dir) + Candidates[I];
      Exit;
    end;
  Result := '';
end;

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
var
  Root, Module: String;
  Count: Integer;
begin
  if CurStep <> ssPostInstall then Exit;

  Root := Trim(SourcePage.Values[0]);
  Count := 0;
  if Root <> '' then
  begin
    WizardForm.StatusLabel.Caption := 'Importing your After Dark screen savers...';
    Count := ImportModules(Root);
  end;

  { Seed a machine-wide default so every account has something that works. If
    nothing was imported there is nothing to point at, and writing a config
    naming files that are not there would be worse than writing none. }
  if Count > 0 then
  begin
    Module := DefaultModule(ModulesDir());
    if Module <> '' then WriteMachineDefault(ModulesDir(), Module);
  end;

  if (Root <> '') and (Count = 0) then
    MsgBox('No screen savers were imported. You can point After Dark Studio at '
      + 'your files from inside the app.', mbInformation, MB_OK);
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
