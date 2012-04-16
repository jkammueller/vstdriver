!include "x64.nsh"
!include MUI2.nsh
!include WinVer.nsh
; The name of the installer
Name "VST MIDI System Synth"

; The file to write
OutFile "vstmididrv.exe"
; Request application privileges for Windows Vista
RequestExecutionLevel admin
SetCompressor /solid lzma 
;--------------------------------
; Pages
!insertmacro MUI_PAGE_WELCOME
Page Custom LockedListShow
!insertmacro MUI_PAGE_INSTFILES
UninstPage Custom un.LockedListShow
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

!macro DeleteOnReboot Path
  IfFileExists `${Path}` 0 +3
    SetFileAttributes `${Path}` NORMAL
    Delete /rebootok `${Path}`
!macroend
!define DeleteOnReboot `!insertmacro DeleteOnReboot`

Function LockedListShow
 ${If} ${AtLeastWinVista}
  !insertmacro MUI_HEADER_TEXT `File in use check` `Drive use check`
  LockedList::AddModule \vstmididrv.dll
  LockedList::Dialog  /autonext   
  Pop $R0
  ${EndIf}
FunctionEnd
Function un.LockedListShow
 ${If} ${AtLeastWinVista}
  !insertmacro MUI_HEADER_TEXT `File in use check` `Drive use check`
  LockedList::AddModule \vstmididrv.dll
  LockedList::Dialog  /autonext   
  Pop $R0
 ${EndIf}
FunctionEnd
;--------------------------------
Function .onInit
ReadRegStr $R0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VST MIDI System Synth" "UninstallString"
  StrCmp $R0 "" done
  MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
  "The MIDI driver is already installed. $\n$\nClick `OK` to remove the \
  previous version or `Cancel` to cancel this upgrade." \
  IDOK uninst
  Abort
;Run the uninstaller
uninst:
  ClearErrors
  Exec $R0
  Abort
done:
   MessageBox MB_YESNO "This will install the VST MIDI System Synth. Continue?" IDYES NoAbort
     Abort ; causes installer to quit.
   NoAbort:
 FunctionEnd
; The stuff to install
Section "Needed (required)"
  SectionIn RO
  ; Copy files according to whether its x64 or not.
   DetailPrint "Copying driver and synth..."
   ${If} ${RunningX64}
   SetOutPath $WINDIR\SysWow64
   File vstmididrv.dll 
   File vstmididrvcfg.exe
   RegDLL "$WINDIR\SysWow64\vstmididrv.dll"
   ${Else}
   SetOutPath $WINDIR\System32
   File vstmididrv.dll 
   File vstmididrvcfg.exe
   RegDLL "$WINDIR\System32\vstmididrv.dll"
   ${EndIf}

   ;check if already installed
   StrCpy  $1 "0"
LOOP1:
  ;k not installed, do checks
  IntOp $1 $1 + 1
  ClearErrors
  ReadRegStr $0  HKLM "Software\Microsoft\Windows NT\CurrentVersion\Drivers32" "midi$1"
  StrCmp $0 "" INSTALLDRIVER NEXTCHECK
  NEXTCHECK:
  StrCmp $0 "wdmaud.drv" 0  NEXT1
INSTALLDRIVER:
  WriteRegStr HKLM "Software\Microsoft\Windows NT\CurrentVersion\Drivers32" "midi$1" "vstmididrv.dll"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VST MIDI System Synth\Backup" \
      "MIDI" "midi$1"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VST MIDI System Synth\Backup" \
      "MIDIDRV" "$0"
  Goto REGDONE
NEXT1:
  StrCmp $1 "9" 0 LOOP1
REGDONE:
  ; Write the uninstall keys
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VST MIDI System Synth" "DisplayName" "VST MIDI System Synth"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VST MIDI System Synth" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VST MIDI System Synth" "NoRepair" 1
  CreateDirectory "$SMPROGRAMS\VST MIDI System Synth"
 ${If} ${RunningX64}
   WriteUninstaller "$WINDIR\SysWow64\vstmididrvuninstall.exe"
   WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VST MIDI System Synth" "UninstallString" '"$WINDIR\SysWow64\vstmididrvuninstall.exe"'
   CreateShortCut "$SMPROGRAMS\VST MIDI System Synth\Uninstall.lnk" "$WINDIR\SysWow64\vstmididrvuninstall.exe" "" "$WINDIR\SysWow64\vstmididrvuninstall.exe" 0
   CreateShortCut "$SMPROGRAMS\VST MIDI System Synth\Configure Driver.lnk" "$WINDIR\SysWow64\vstmididrvcfg.exe" "" "$WINDIR\SysWow64\vstmididrvcfg.exe" 0
   ${Else}
   WriteUninstaller "$WINDIR\System32\vstmididrvuninstall.exe"
   WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VST MIDI System Synth" "UninstallString" '"$WINDIR\System32\vstmididrvuninstall.exe"'
   CreateShortCut "$SMPROGRAMS\VST MIDI System Synth\Uninstall.lnk" "$WINDIR\System32\vstmididrvuninstall.exe" "" "$WINDIR\System32\vstmididrvuninstall.exe" 0
   CreateShortCut "$SMPROGRAMS\VST MIDI System Synth\Configure Driver.lnk" "$WINDIR\System32\vstmididrvcfg.exe" "" "$WINDIR\System32\vstmididrvcfg.exe" 0
   ${EndIf}
   MessageBox MB_OK "Installation complete! Use the driver configuration tool which is in the 'VST MIDI System Synth' program shortcut directory to configure the driver."

SectionEnd
;--------------------------------
; Uninstaller
Section "Uninstall"
   ; Remove registry keys
  ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VST MIDI System Synth\Backup" \
       "MIDI"
  ReadRegStr $1 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VST MIDI System Synth\Backup" \
      "MIDIDRV"
  WriteRegStr HKLM "Software\Microsoft\Windows NT\CurrentVersion\Drivers32" "$0" "$1"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VST MIDI System Synth"
  DeleteRegKey HKCU "Software\VST MIDI Driver"
  RMDir /r "$SMPROGRAMS\VST MIDI System Synth"
 ${If} ${RunningX64}
 ${If} ${AtLeastWinVista}
  UnRegDLL "$WINDIR\SysWow64\vstmididrv.dll"
  Delete $WINDIR\SysWow64\vstmididrv.dll
  Delete $WINDIR\SysWow64\vstmididrvuninstall.exe
  Delete $WINDIR\SysWow64\vstmididrvcfg.exe
${Else}
  UnRegDLL "$WINDIR\SysWow64\vstmididrv.dll"
  MessageBox MB_OK "Note: The uninstaller will reboot your system to remove drivers."
  ${DeleteOnReboot} $WINDIR\SysWow64\vstmididrv.dll
  ${DeleteOnReboot} $WINDIR\SysWow64\vstmididrvuninstall.exe
  ${DeleteOnReboot} $WINDIR\SysWow64\vstmididrvcfg.exe
  Reboot
${Endif}
${Else}
${If} ${AtLeastWinVista}
  UnRegDLL "$WINDIR\System32\vstmididrv.dll"
  Delete $WINDIR\System32\vstmididrv.dll
  Delete $WINDIR\System32\vstmidiuninstall.exe
  Delete $WINDIR\System32\vstmidicfg.exe
${Else}
  UnRegDLL "$WINDIR\System32\vstmididrv.dll"
  MessageBox MB_OK "Note: The uninstaller will reboot your system to remove drivers."
  ${DeleteOnReboot} $WINDIR\System32\vstmididrv.dll
  ${DeleteOnReboot} $WINDIR\System32\vstmididrvuninstall.exe
  ${DeleteOnReboot} $WINDIR\System32\vstmididrvcfg.exe
  Reboot
${Endif}
${EndIf}
SectionEnd
