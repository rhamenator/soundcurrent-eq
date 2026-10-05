; SPDX-License-Identifier: GPL-3.0-only
Unicode true
!include "MUI2.nsh"

!ifndef APP_EXE
  !error "Pass /DAPP_EXE=path-to-soundcurrent-eq.exe"
!endif
!ifndef OUTPUT
  !error "Pass /DOUTPUT=path-to-installer.exe"
!endif
!ifndef SOURCE_ROOT
  !error "Pass /DSOURCE_ROOT=path-to-repository"
!endif

Name "SoundCurrent EQ"
OutFile "${OUTPUT}"
InstallDir "$LOCALAPPDATA\Programs\SoundCurrent EQ"
RequestExecutionLevel user
SetCompressor /SOLID lzma
BrandingText "SoundCurrent EQ • GPL-3.0-only"
Icon "${SOURCE_ROOT}/data/soundcurrent-eq.ico"
UninstallIcon "${SOURCE_ROOT}/data/soundcurrent-eq.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SOURCE_ROOT}/LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\soundcurrent-eq.exe"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "SoundCurrent EQ" main
  FindWindow $0 "SoundCurrentEQWindow"
  StrCmp $0 0 +3
    MessageBox MB_ICONEXCLAMATION "Quit SoundCurrent EQ before installing this version."
    Abort
  SetOutPath "$INSTDIR"
  File "/oname=soundcurrent-eq.exe" "${APP_EXE}"
  File "${SOURCE_ROOT}/LICENSE"
  File "${SOURCE_ROOT}/COPYRIGHT"
  File "${SOURCE_ROOT}/README.md"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  CreateDirectory "$SMPROGRAMS\SoundCurrent EQ"
  CreateShortcut "$SMPROGRAMS\SoundCurrent EQ\SoundCurrent EQ.lnk" "$INSTDIR\soundcurrent-eq.exe"
  CreateShortcut "$SMPROGRAMS\SoundCurrent EQ\Uninstall.lnk" "$INSTDIR\uninstall.exe"
  CreateShortcut "$DESKTOP\SoundCurrent EQ.lnk" "$INSTDIR\soundcurrent-eq.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "DisplayName" "SoundCurrent EQ"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "DisplayIcon" "$INSTDIR\soundcurrent-eq.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "Publisher" "SoundCurrent EQ contributors"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "DisplayVersion" "0.6.0"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ" "NoRepair" 1
SectionEnd

Section "Uninstall"
  FindWindow $0 "SoundCurrentEQWindow"
  StrCmp $0 0 +3
    MessageBox MB_ICONEXCLAMATION "Quit SoundCurrent EQ before uninstalling it."
    Abort
  Delete "$DESKTOP\SoundCurrent EQ.lnk"
  Delete "$SMPROGRAMS\SoundCurrent EQ\SoundCurrent EQ.lnk"
  Delete "$SMPROGRAMS\SoundCurrent EQ\Uninstall.lnk"
  RMDir "$SMPROGRAMS\SoundCurrent EQ"
  Delete "$INSTDIR\soundcurrent-eq.exe"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\COPYRIGHT"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SoundCurrentEQ"
SectionEnd
