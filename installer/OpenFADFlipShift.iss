#define AppName "openFAD FlipShift"
#define AppVersion "0.1.0"
#define AppPublisher "UnpureBloom / openFAD"
#define AppURL "https://github.com/Junziren/openFAD-FlipShift"
#define PluginBundle "vst3\build\OpenFADFlipShift_artefacts\Release\VST3\openFAD FlipShift.vst3"
#define WebView2Installer "MicrosoftEdgeWebView2RuntimeInstallerX64.exe"
#define VCRuntimeInstaller "VC_redist.x64.exe"

[Setup]
AppId={{39BC7200-261E-468F-AED3-64631D75103D}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
VersionInfoVersion=0.1.0.0
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} VST3 Installer
VersionInfoProductName={#AppName}
SourceDir=..
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
UninstallDisplayName={#AppName} VST3
UninstallDisplayIcon={commoncf64}\VST3\openFAD FlipShift.vst3\Contents\x86_64-win\openFAD FlipShift.vst3
UninstallFilesDir={commonappdata}\openFAD FlipShift\Installer
OutputDir=dist\installer
OutputBaseFilename=openFAD-FlipShift-Setup-{#AppVersion}-Windows-x64
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
PrivilegesRequired=admin
CloseApplications=yes
RestartApplications=no
RestartIfNeededByRun=no
SetupLogging=yes
SetupMutex=openFAD-FlipShift-Installer
WizardStyle=modern dynamic
Compression=lzma2/normal
SolidCompression=no

[Files]
Source: "installer\prerequisites\{#WebView2Installer}"; Flags: dontcopy
Source: "installer\prerequisites\{#VCRuntimeInstaller}"; Flags: dontcopy
Source: "{#PluginBundle}\*"; DestDir: "{commoncf64}\VST3\openFAD FlipShift.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs restartreplace uninsrestartdelete

[InstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\openFAD FlipShift.vst3"

[Registry]
Root: HKLM64; Subkey: "Software\openFAD\FlipShift"; ValueType: string; ValueName: "Version"; ValueData: "{#AppVersion}"; Flags: uninsdeletekey
Root: HKLM64; Subkey: "Software\openFAD\FlipShift"; ValueType: string; ValueName: "InstallPath"; ValueData: "{commoncf64}\VST3\openFAD FlipShift.vst3"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\openFAD FlipShift.vst3"
Type: dirifempty; Name: "{commonappdata}\openFAD FlipShift"

[Code]
const
  WebView2ClientKey = 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}';

function ForcePrerequisites: Boolean;
begin
  Result := ExpandConstant('{param:FORCEPREREQS|0}') = '1';
end;

function IsWebView2RuntimeInstalled: Boolean;
var
  Version: String;
begin
  Result := RegQueryStringValue(HKLM32, WebView2ClientKey, 'pv', Version);
  if not Result then
    Result := RegQueryStringValue(HKCU32, WebView2ClientKey, 'pv', Version);
  Result := Result and (Version <> '') and (Version <> '0.0.0.0');
  if Result then
    Log('Detected Microsoft Edge WebView2 Runtime ' + Version)
  else
    Log('Microsoft Edge WebView2 Runtime was not detected');
end;

function IsVCRuntimeInstalled: Boolean;
var
  Installed: Cardinal;
begin
  Result := RegQueryDWordValue(
    HKLM64,
    'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
    'Installed',
    Installed) and (Installed = 1);

  Result := Result
    and FileExists(ExpandConstant('{sys}\MSVCP140.dll'))
    and FileExists(ExpandConstant('{sys}\VCRUNTIME140.dll'))
    and FileExists(ExpandConstant('{sys}\VCRUNTIME140_1.dll'));

  if Result then
    Log('Detected Microsoft Visual C++ x64 Runtime')
  else
    Log('Microsoft Visual C++ x64 Runtime was not detected');
end;

function IsAcceptedExitCode(ExitCode: Integer): Boolean;
begin
  Result := (ExitCode = 0) or (ExitCode = 1638) or
    (ExitCode = 1641) or (ExitCode = 3010);
end;

function InstallPrerequisite(
  const FileName, Parameters, DisplayName: String;
  var NeedsRestart: Boolean): String;
var
  ExitCode: Integer;
  FullPath: String;
begin
  Result := '';
  ExtractTemporaryFile(FileName);
  FullPath := ExpandConstant('{tmp}\') + FileName;
  Log('Installing prerequisite: ' + DisplayName);
  WizardForm.StatusLabel.Caption := 'Installing ' + DisplayName + '...';

  if not Exec(FullPath, Parameters, ExpandConstant('{tmp}'), SW_HIDE,
    ewWaitUntilTerminated, ExitCode) then
  begin
    Result := 'Could not start the ' + DisplayName + ' installer.';
    Exit;
  end;

  Log(Format('%s installer exit code: %d', [DisplayName, ExitCode]));
  if not IsAcceptedExitCode(ExitCode) then
  begin
    Result := Format('%s installation failed with exit code %d.', [DisplayName, ExitCode]);
    Exit;
  end;

  if (ExitCode = 1641) or (ExitCode = 3010) then
    NeedsRestart := True;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';

  if ForcePrerequisites or not IsVCRuntimeInstalled then
  begin
    Result := InstallPrerequisite(
      '{#VCRuntimeInstaller}',
      '/install /quiet /norestart',
      'Microsoft Visual C++ 2015-2022 x64 Runtime',
      NeedsRestart);
    if Result <> '' then
      Exit;
  end;

  if not IsVCRuntimeInstalled then
  begin
    Result := 'The Microsoft Visual C++ x64 Runtime is still unavailable.';
    Exit;
  end;

  if ForcePrerequisites or not IsWebView2RuntimeInstalled then
  begin
    Result := InstallPrerequisite(
      '{#WebView2Installer}',
      '/silent /install',
      'Microsoft Edge WebView2 Runtime',
      NeedsRestart);
    if Result <> '' then
      Exit;
  end;

  if not IsWebView2RuntimeInstalled then
  begin
    Result := 'The Microsoft Edge WebView2 Runtime is still unavailable. Setup cannot install a usable WebView interface.';
    Exit;
  end;
end;
