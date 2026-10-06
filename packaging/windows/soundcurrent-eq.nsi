; SPDX-License-Identifier: GPL-3.0-only
Unicode true
!include "MUI2.nsh"
!include "nsDialogs.nsh"
!include "LogicLib.nsh"

!ifndef APP_EXE
  !error "Pass /DAPP_EXE=path-to-soundcurrent-eq.exe"
!endif
!ifndef OUTPUT
  !error "Pass /DOUTPUT=path-to-installer.exe"
!endif
!ifndef SOURCE_ROOT
  !error "Pass /DSOURCE_ROOT=path-to-repository"
!endif
!ifndef CABLE_ZIP
  !error "Pass /DCABLE_ZIP=path-to-verified-VBCABLE_Driver_Pack45.zip"
!endif

!ifndef APP_VERSION
  !define APP_VERSION "0.7.3"
!endif

Var CableCheck
Var CableChoice
Var InstallCable

Name "SoundCurrent EQ"
OutFile "${OUTPUT}"
InstallDir "$LOCALAPPDATA\Programs\SoundCurrent EQ"
RequestExecutionLevel user
SetCompressor /SOLID lzma
BrandingText "SoundCurrent EQ • GPL-3.0-only"
Icon "${SOURCE_ROOT}\data\soundcurrent-eq.ico"
UninstallIcon "${SOURCE_ROOT}\data\soundcurrent-eq.ico"

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
  StrCpy $InstallCable 0 ; Silent app updates never install/elevate a driver.
  InitPluginsDir
  SetOutPath "$PLUGINSDIR"
  File "/oname=audio-setup.ps1" "${SOURCE_ROOT}\packaging\windows\audio-setup.ps1"
  nsExec::ExecToStack /TIMEOUT=20000 '"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -ExecutionPolicy RemoteSigned -File "$PLUGINSDIR\audio-setup.ps1" -Check'
  Pop $CableCheck
  Pop $0
FunctionEnd

Function AudioPage
  !insertmacro MUI_HEADER_TEXT "Connect your audio" "Set up the virtual cable used by SoundCurrent EQ."
  nsDialogs::Create 1018
  Pop $0
  ${If} $0 == error
    Abort
  ${EndIf}
  ${NSD_CreateLabel} 0 0 100% 25u "VB-CABLE connects Windows playback to the equalizer. Your speakers or headphones remain the physical output."
  Pop $0
  ${NSD_CreateCheckbox} 0 30u 100% 15u "Install the standard VB-CABLE driver"
  Pop $CableChoice
  ${If} $CableCheck == 10
    ${NSD_Check} $CableChoice
    ${NSD_CreateLabel} 0 50u 100% 28u "Windows will ask for administrator approval. In VB-Audio's setup, click Install Driver. Restart Windows afterward."
  ${ElseIf} $CableCheck == 0
    EnableWindow $CableChoice 0
    ${NSD_CreateLabel} 0 50u 100% 28u "VB-CABLE is already installed. Setup will keep the existing driver."
  ${Else}
    EnableWindow $CableChoice 0
    ${NSD_CreateLabel} 0 50u 100% 28u "Setup could not check for an existing driver. After setup, use the Install VB-CABLE shortcut in the Start menu to retry."
  ${EndIf}
  Pop $0
  ${NSD_CreateLabel} 0 84u 100% 30u "VB-CABLE is separate VB-Audio software under its own donationware terms. If useful, donate/pay for a license. Professional deployments may require paid licenses."
  Pop $0
  ${NSD_CreateButton} 0 117u 48% 17u "VB-CABLE website / donations"
  Pop $0
  ${NSD_OnClick} $0 CableWebsite
  ${NSD_CreateButton} 52% 117u 48% 17u "VB-Audio licensing terms"
  Pop $0
  ${NSD_OnClick} $0 CableLicense
  nsDialogs::Show
FunctionEnd

Function CableWebsite
  Pop $0
  ExecShell "open" "https://www.vb-cable.com/"
FunctionEnd
Function CableLicense
  Pop $0
  ExecShell "open" "https://vb-audio.com/Services/licensing.htm"
FunctionEnd
Function AudioPageLeave
  ${NSD_GetState} $CableChoice $InstallCable
FunctionEnd

Section "SoundCurrent EQ" main
  FindWindow $0 "" "SoundCurrent EQ"
  StrCmp $0 0 +3
    MessageBox MB_ICONEXCLAMATION "Quit SoundCurrent EQ before installing this version."
    Abort
  SetOutPath "$INSTDIR"
!ifdef DLL_DIR
  File /r "${DLL_DIR}\*"
!else
  File "/oname=soundcurrent-eq.exe" "${APP_EXE}"
!endif
  File "${SOURCE_ROOT}\LICENSE"
  File "${SOURCE_ROOT}\COPYRIGHT"
  File "${SOURCE_ROOT}\README.md"
  File "${SOURCE_ROOT}\packaging\windows\audio-setup.ps1"
  File "/oname=VBCABLE_Driver_Pack45.zip" "${CABLE_ZIP}"
  File "${SOURCE_ROOT}\packaging\windows\VB-CABLE-NOTICE.txt"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  CreateDirectory "$SMPROGRAMS\SoundCurrent EQ"
  CreateShortcut "$SMPROGRAMS\SoundCurrent EQ\SoundCurrent EQ.lnk" "$INSTDIR\soundcurrent-eq.exe"
  CreateShortcut "$SMPROGRAMS\SoundCurrent EQ\Uninstall.lnk" "$INSTDIR\uninstall.exe"
  CreateShortcut "$SMPROGRAMS\SoundCurrent EQ\Install VB-CABLE.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" '-NoProfile -ExecutionPolicy RemoteSigned -File "$INSTDIR\audio-setup.ps1" -Install' "$INSTDIR\soundcurrent-eq.exe"
  CreateShortcut "$DESKTOP\SoundCurrent EQ.lnk" "$INSTDIR\soundcurrent-eq.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "DisplayName" "SoundCurrent EQ"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "DisplayIcon" "$INSTDIR\soundcurrent-eq.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "Publisher" "SoundCurrent EQ contributors"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "DisplayVersion" "${APP_VERSION}"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "NoRepair" 1
  ${If} $InstallCable == ${BST_CHECKED}
    DetailPrint "Opening VB-Audio's signed driver installer..."
    nsExec::ExecToStack '"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy RemoteSigned -File "$INSTDIR\audio-setup.ps1" -Install -Quiet'
    Pop $0
    Pop $1
    DetailPrint $1
    ${If} $0 == 3010
      SetRebootFlag true
    ${ElseIf} $0 != 0
      DetailPrint "VB-CABLE setup did not finish. Retry using the Start menu shortcut."
      MessageBox MB_OK|MB_ICONINFORMATION "VB-CABLE was not installed. SoundCurrent EQ itself is installed. Use Install VB-CABLE in the Start menu to retry; see setup details for the reason."
    ${EndIf}
  ${EndIf}
SectionEnd

Section "Uninstall"
  FindWindow $0 "" "SoundCurrent EQ"
  StrCmp $0 0 +3
    MessageBox MB_ICONEXCLAMATION "Quit SoundCurrent EQ before uninstalling it."
    Abort
  Delete "$DESKTOP\SoundCurrent EQ.lnk"
  Delete "$SMPROGRAMS\SoundCurrent EQ\SoundCurrent EQ.lnk"
  Delete "$SMPROGRAMS\SoundCurrent EQ\Uninstall.lnk"
  Delete "$SMPROGRAMS\SoundCurrent EQ\Install VB-CABLE.lnk"
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
  Delete "$INSTDIR\VBCABLE_Driver_Pack45.zip"
  Delete "$INSTDIR\VB-CABLE-NOTICE.txt"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ"
SectionEnd
