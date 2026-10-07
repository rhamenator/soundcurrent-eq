; SPDX-License-Identifier: GPL-3.0-only
Unicode true
!include "MUI2.nsh"
!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"

!ifndef APP_EXE
  !error "Pass /DAPP_EXE=path-to-soundcurrent-eq.exe"
!endif
!ifndef OUTPUT
  !error "Pass /DOUTPUT=path-to-installer.exe"
!endif
!ifndef SOURCE_ROOT
  !error "Pass /DSOURCE_ROOT=path-to-repository"
!endif
!ifndef DLL_DIR
  !error "Pass /DDLL_DIR=complete-app-and-helper-payload"
!endif
!ifndef UNINSTALL_PAYLOAD
  !error "Pass /DUNINSTALL_PAYLOAD=exact-payload-deletion-manifest"
!endif
!ifndef CABLE_ZIP
  !error "Pass /DCABLE_ZIP=path-to-original-VBCABLE_Driver_Pack45.zip"
!endif

!ifndef APP_VERSION
  !define APP_VERSION "0.7.6"
!endif

Var DriverCheck
Var DriverChoice
Var InstallDriver

Name "SoundCurrent EQ"
OutFile "${OUTPUT}"
InstallDir "$LOCALAPPDATA\Programs\SoundCurrent EQ"
InstallDirRegKey HKCU "Software\SoundCurrent\SoundCurrent EQ" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
BrandingText "SoundCurrent EQ • GPL-3.0-only"
Icon "${SOURCE_ROOT}\data\soundcurrent-eq.ico"
UninstallIcon "${SOURCE_ROOT}\data\soundcurrent-eq.ico"

!define MUI_WELCOMEPAGE_TEXT "Install or update SoundCurrent EQ. You do not need to uninstall an older version. Your settings, presets and equipment profiles will be kept.$\r$\n$\r$\nSave your work and use Quit to exit the running app before continuing. Closing its window keeps it running in the background."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SOURCE_ROOT}\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
Page custom AudioPage AudioPageLeave
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_REBOOTLATER_DEFAULT
!define MUI_FINISHPAGE_RUN "$INSTDIR\soundcurrent-eq.exe"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ; Migrate the old install location, including custom folders.
  ReadRegStr $1 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "DisplayIcon"
  ${If} $1 != ""
    ${GetParent} $1 $2
    IfFileExists "$2\soundcurrent-eq.exe" 0 +2
      StrCpy $INSTDIR $2
  ${EndIf}
  StrCpy $InstallDriver 0 ; Silent app updates never install/elevate a driver.
  InitPluginsDir
  SetOutPath "$PLUGINSDIR"
  ; Readiness detection needs the helper and its runtime before installation.
  File "${DLL_DIR}\soundcurrent-cable-setup-guard.exe"
  File "${DLL_DIR}\Qt6Core.dll"
  File "${DLL_DIR}\msvcp140*.dll"
  File "${DLL_DIR}\vcruntime140*.dll"
  File "${DLL_DIR}\concrt140.dll"
  File "/oname=audio-setup.ps1" "${SOURCE_ROOT}\packaging\windows\cable-setup.ps1"
  nsExec::ExecToStack /TIMEOUT=20000 '"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -ExecutionPolicy RemoteSigned -File "$PLUGINSDIR\audio-setup.ps1" -Check'
  Pop $DriverCheck
  Pop $0
  ${If} $DriverCheck == 3010
    SetRebootFlag true
  ${EndIf}
FunctionEnd

Function AudioPage
  !insertmacro MUI_HEADER_TEXT "Connect your audio" "Set up VB-CABLE for SoundCurrent EQ."
  nsDialogs::Create 1018
  Pop $0
  ${If} $0 == error
    Abort
  ${EndIf}
  ${NSD_CreateLabel} 0 0 100% 32u "VB-CABLE routes playback through the app. Choose speakers inside SoundCurrent. VB-CABLE is VB-Audio donationware: https://vb-cable.com — donations are welcome."
  Pop $0
  ${NSD_CreateCheckbox} 0 38u 100% 18u "Install VB-CABLE if missing (administrator approval)"
  Pop $DriverChoice
  ${If} $DriverCheck == 0
    ${NSD_Check} $DriverChoice
    ${NSD_CreateLabel} 0 65u 100% 35u "VB-CABLE is already present. It will be reused. SoundCurrent restores your normal output when switched off or Quit."
  ${ElseIf} $DriverCheck == 3010
    ${NSD_CreateLabel} 0 65u 100% 35u "VB-CABLE setup requires a Windows restart. Restart before using the equalizer or opening VB-CABLE settings."
  ${ElseIf} $DriverCheck == 11
    ${NSD_Check} $DriverChoice
    ${NSD_CreateLabel} 0 65u 100% 35u "VB-CABLE has a driver record but no usable audio endpoints. Setup offers repair: remove the driver, restart, reinstall, and restart again."
  ${ElseIf} $DriverCheck == 10
    ${NSD_Check} $DriverChoice
    ${NSD_CreateLabel} 0 65u 100% 35u "Setup opens VB-Audio’s signed installer. Click Install Driver, then restart Windows before using the equalizer or VB-CABLE settings."
  ${Else}
    ${NSD_CreateLabel} 0 65u 100% 35u "Setup could not check the driver. You can retry with Audio driver setup in the app or Start menu."
  ${EndIf}
  Pop $0
  ${NSD_CreateLabel} 0 108u 100% 40u "Quit any running equalizer before driver setup. The last app’s uninstaller offers VB-CABLE removal. Other software may also need it. Extra A/B cables are not bundled."
  Pop $0
  nsDialogs::Show
FunctionEnd
Function AudioPageLeave
  ${NSD_GetState} $DriverChoice $InstallDriver
FunctionEnd

Section "SoundCurrent EQ" main
  FindWindow $0 "" "SoundCurrent EQ"
  StrCmp $0 0 +3
    MessageBox MB_ICONEXCLAMATION "Quit SoundCurrent EQ before updating. Closing the window keeps it running. No uninstall is needed."
    Abort
  SetOutPath "$INSTDIR"
!ifdef DLL_DIR
  File /r "${DLL_DIR}\*"
!else
  File "/oname=soundcurrent-eq.exe" "${APP_EXE}"
!endif
  File "/oname=soundcurrent-route-guardian.exe" "${DLL_DIR}\soundcurrent-route-guardian.exe"
  File "${SOURCE_ROOT}\LICENSE"
  File "${SOURCE_ROOT}\COPYRIGHT"
  File "${SOURCE_ROOT}\README.md"
  File "/oname=audio-setup.ps1" "${SOURCE_ROOT}\packaging\windows\cable-setup.ps1"
  File "${SOURCE_ROOT}\THIRD-PARTY-NOTICES.md"
  File "/oname=cable-setup.ps1" "${SOURCE_ROOT}\packaging\windows\cable-setup.ps1"
  File "/oname=VBCABLE_Driver_Pack45.zip" "${CABLE_ZIP}"
  File "${SOURCE_ROOT}\packaging\windows\VB-CABLE-NOTICE.txt"
  ; Do not remove a native driver or its ownership while changing app variants.
  WriteRegStr HKCU "Software\SoundCurrent\SoundCurrent EQ" "InstallDir" "$INSTDIR"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  CreateDirectory "$SMPROGRAMS\SoundCurrent EQ"
  CreateShortcut "$SMPROGRAMS\SoundCurrent EQ\SoundCurrent EQ.lnk" "$INSTDIR\soundcurrent-eq.exe"
  CreateShortcut "$SMPROGRAMS\SoundCurrent EQ\Uninstall.lnk" "$INSTDIR\uninstall.exe"
  CreateShortcut "$SMPROGRAMS\SoundCurrent EQ\Audio driver setup.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" '-NoProfile -ExecutionPolicy RemoteSigned -File "$INSTDIR\audio-setup.ps1" -Install' "$INSTDIR\soundcurrent-eq.exe"
  CreateShortcut "$SMPROGRAMS\SoundCurrent EQ\VB-CABLE settings.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" '-NoProfile -ExecutionPolicy RemoteSigned -File "$INSTDIR\cable-setup.ps1" -Settings'
  CreateShortcut "$DESKTOP\SoundCurrent EQ.lnk" "$INSTDIR\soundcurrent-eq.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "DisplayName" "SoundCurrent EQ"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "DisplayIcon" "$INSTDIR\soundcurrent-eq.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "Publisher" "SoundCurrent EQ contributors"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "DisplayVersion" "${APP_VERSION}"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "NoRepair" 1
  ${If} $InstallDriver == ${BST_CHECKED}
    DetailPrint "Opening VB-CABLE setup..."
    nsExec::ExecToStack '"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy RemoteSigned -File "$INSTDIR\audio-setup.ps1" -Install -Quiet'
    Pop $0
    Pop $1
    DetailPrint $1
    ${If} $0 == 3010
      SetRebootFlag true
      MessageBox MB_OK|MB_ICONINFORMATION "Restart Windows before using SoundCurrent or VB-CABLE settings. The audio driver installation needs a system restart." /SD IDOK
    ${ElseIf} $0 != 0
      DetailPrint "VB-CABLE setup did not finish. Retry using the Start menu shortcut."
      MessageBox MB_OK|MB_ICONINFORMATION "VB-CABLE setup did not finish. SoundCurrent EQ itself is installed. Use Audio driver setup in the Start menu to retry; see setup details for the reason."
    ${EndIf}
  ${EndIf}
SectionEnd

Section "Uninstall"
  FindWindow $0 "" "SoundCurrent EQ"
  StrCmp $0 0 +3
    MessageBox MB_ICONEXCLAMATION "Quit SoundCurrent EQ before uninstalling it."
    Abort
  ; Interactive removal offers the official shared cable remover. Silent app
  ; updates/uninstalls keep the cable; they never display UAC or vendor dialogs.
  IfSilent cable_keep cable_remove
  cable_remove:
  nsExec::ExecToStack '"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy RemoteSigned -File "$INSTDIR\audio-setup.ps1" -Remove -Quiet'
  Pop $0
  Pop $1
  DetailPrint $1
  ${If} $0 == 3010
    SetRebootFlag true
  ${ElseIf} $0 != 0
    MessageBox MB_ICONEXCLAMATION "VB-CABLE removal did not finish. This app was kept so you can retry.$\r$\n$1"
    Abort
  ${EndIf}
  cable_keep:
  Delete "$DESKTOP\SoundCurrent EQ.lnk"
  Delete "$SMPROGRAMS\SoundCurrent EQ\SoundCurrent EQ.lnk"
  Delete "$SMPROGRAMS\SoundCurrent EQ\Uninstall.lnk"
  Delete "$SMPROGRAMS\SoundCurrent EQ\Audio driver setup.lnk"
  Delete "$SMPROGRAMS\SoundCurrent EQ\Install VB-CABLE.lnk"
  Delete "$SMPROGRAMS\SoundCurrent EQ\VB-CABLE settings.lnk"
  RMDir "$SMPROGRAMS\SoundCurrent EQ"
!ifdef UNINSTALL_PAYLOAD
  !include "${UNINSTALL_PAYLOAD}"
!else
  Delete "$INSTDIR\soundcurrent-eq.exe"
!endif
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\COPYRIGHT"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\audio-setup.ps1"
  Delete "$INSTDIR\cable-setup.ps1"
  Delete "$INSTDIR\audio-driver\soundcurrentvad.inf"
  Delete "$INSTDIR\audio-driver\soundcurrentvad.sys"
  Delete "$INSTDIR\audio-driver\soundcurrentvad.cat"
  RMDir "$INSTDIR\audio-driver"
  Delete "$INSTDIR\licenses\SoundCurrent-driver-MS-PL.txt"
  RMDir "$INSTDIR\licenses"
  Delete "$INSTDIR\THIRD-PARTY-NOTICES.md"
  Delete "$INSTDIR\VBCABLE_Driver_Pack45.zip"
  Delete "$INSTDIR\VB-CABLE-NOTICE.txt"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ"
SectionEnd
