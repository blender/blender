;
; $Id$
;
; Blender Self-Installer for Windows (NSIS - http://nsis.sourceforge.net)
;

!include "MUI.nsh"

Name "Blender VERSION" 

!define MUI_ABORTWARNING

!define MUI_WELCOMEPAGE_TEXT  "This wizard will guide you through the installation of Blender.\r\n\r\nIt is recommended that you close all other applications before starting Setup.\r\n\r\n"
!define MUI_WELCOMEFINISHPAGE_BITMAP "01.installer.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP  "00.header.bmp"
!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_FINISHPAGE_RUN "$INSTDIR\blender.exe"
!define MUI_CHECKBITMAP "00.checked.bmp"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "DISTDIR\Copyright.txt"
!insertmacro MUI_PAGE_COMPONENTS
    
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
  
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH


Icon "00.installer.ico"
UninstallIcon "00.installer.ico"

;--------------------------------
;Languages
 
  !insertmacro MUI_LANGUAGE "English"
    
;--------------------------------
;Language Strings

  ;Description
  LangString DESC_SecCopyUI ${LANG_ENGLISH} "Copy all required files to the application folder."
  LangString DESC_Section2 ${LANG_ENGLISH} "Add shortcut items to the Start Menu. (Recommended)"
  LangString DESC_Section3 ${LANG_ENGLISH} "Add a shortcut to Blender on your desktop."
  LangString DESC_Section4 ${LANG_ENGLISH} "Blender can register itself with .blend files to allow double-clicking from Windows Explorer, etc."
  
;--------------------------------
;Data

Caption "Blender VERSION Installer"
OutFile "DISTDIR\..\VERSION\blender-VERSION-windows.exe"

InstallDir "$PROGRAMFILES\Blender Foundation\Blender"

BrandingText "http://www.blender.org/bf"
ComponentText "This will install Blender VERSION on your computer."

DirText "Use the field below to specify the folder where you want Blender to be copied to. To specify a different folder, type a new name or use the Browse button to select an existing folder."

; GetWindowsVersion
;
; Based on Yazno's function, http://yazno.tripod.com/powerpimpit/
; Updated by Joost Verburg
;
; Returns on top of stack
;
; Windows Version (95, 98, ME, NT x.x, 2000, XP, 2003)
; or
; '' (Unknown Windows Version)
;
; Usage:
;   Call GetWindowsVersion
;   Pop $R0
;   ; at this point $R0 is "NT 4.0" or whatnot

Function GetWindowsVersion

  Push $R0
  Push $R1

  ReadRegStr $R0 HKLM \
  "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion

  IfErrors 0 lbl_winnt
   
  ; we are not NT
  ReadRegStr $R0 HKLM \
  "SOFTWARE\Microsoft\Windows\CurrentVersion" VersionNumber
 
  StrCpy $R1 $R0 1
  StrCmp $R1 '4' 0 lbl_error
 
  StrCpy $R1 $R0 3
 
  StrCmp $R1 '4.0' lbl_win32_95
  StrCmp $R1 '4.9' lbl_win32_ME lbl_win32_98
 
  lbl_win32_95:
    StrCpy $R0 '95'
  Goto lbl_done
 
  lbl_win32_98:
    StrCpy $R0 '98'
  Goto lbl_done
 
  lbl_win32_ME:
    StrCpy $R0 'ME'
  Goto lbl_done
 
  lbl_winnt:

  StrCpy $R1 $R0 1
 
  StrCmp $R1 '3' lbl_winnt_x
  StrCmp $R1 '4' lbl_winnt_x
 
  StrCpy $R1 $R0 3
 
  StrCmp $R1 '5.0' lbl_winnt_2000
  StrCmp $R1 '5.1' lbl_winnt_XP
  StrCmp $R1 '5.2' lbl_winnt_2003 lbl_error
 
  lbl_winnt_x:
    StrCpy $R0 "NT $R0" 6
  Goto lbl_done
 
  lbl_winnt_2000:
    Strcpy $R0 '2000'
  Goto lbl_done
 
  lbl_winnt_XP:
    Strcpy $R0 'XP'
  Goto lbl_done
 
  lbl_winnt_2003:
    Strcpy $R0 '2003'
  Goto lbl_done
 
  lbl_error:
    Strcpy $R0 ''
  lbl_done:
 
  Pop $R1
  Exch $R0

FunctionEnd

Var BLENDERHOME
Var dirchanged

Function SetWinXPPath
  StrCpy $BLENDERHOME "$APPDATA\Blender Foundation\Blender"
FunctionEnd

Function SetWin9xPath
  StrCpy $BLENDERHOME $INSTDIR
FunctionEnd
 
Function .onInit

  Strcpy $dirchanged '0'
  
; Sets $BLENDERHOME to suit Windows version...
  
  IfFileExists "$APPDATA\Blender Foundation\Blender\.blender\.bfont.tff" do_win2kXP
  
  Call GetWindowsVersion
  Pop $R0
  
  StrCpy $R1 $R0 2
  StrCmp $R1 "NT" do_win2kxp
  StrCmp $R0 "2000" do_win2kxp
  StrCmp $R0 "XP" do_win2kxp
  StrCmp $R0 "2003" do_win2kxp
  
  ;else...
  Call SetWin9xPath
  Goto end

  do_win2kXP:
    Call SetWinXPPath
    
  end:
    
FunctionEnd

Function .onInstSuccess
  Strcmp $dirchanged "0" done
  MessageBox MB_OK "Please note that your user defaults and python scripts can now found at $BLENDERHOME\.blender. This does not effect the functionality of Blender in any way and can safely be ignored unless you enjoy digging around in your App data directory!"
done:
  BringToFront
FunctionEnd 

Section "Blender-VERSION (required)" SecCopyUI
  SectionIn RO
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  ; Put file there
  File DISTDIR\blender.exe
  File DISTDIR\python22.dll
  File DISTDIR\sdl.dll
  File DISTDIR\solid.dll
  File DISTDIR\gnu_gettext.dll
  File DISTDIR\Copyright.txt
  File DISTDIR\Readme.txt
  File DISTDIR\Release_SHORTVERS.txt
  File DISTDIR\GPL-license.txt
  File DISTDIR\Help.url
     
  SetOutPath $BLENDERHOME\.blender
  File DISTDIR\.blender\.bfont.ttf
  
  ; If data (particularly .b.blend) exists in $INSTDIR\.blender but the OS is NT/Win2k/XP, copy to new location...
  
  IfFileExists "$INSTDIR\.blender\.b.blend" check_version done
  
  check_version:
    StrCmp $BLENDERHOME $INSTDIR done ; Win9x?
    ; else...
    CopyFiles /SILENT "$INSTDIR\.blender\.b*" "$BLENDERHOME\.blender"
    CopyFiles /SILENT "$INSTDIR\.blender\scripts\*.*" "$BLENDERHOME\.blender\scripts"
    
    ; Remove the old dir!
    
    RMDir /r $INSTDIR\.blender
    
    Strcpy $dirchanged '1'
    
  done:
  
  SetOutPath $BLENDERHOME\.blender\scripts
  File DISTDIR\.blender\scripts\ac3d_export.py
  File DISTDIR\.blender\scripts\ac3d_import.py
  File DISTDIR\.blender\scripts\blender2cal3d.py
  File DISTDIR\.blender\scripts\directxexporter.py
  File DISTDIR\.blender\scripts\mod_flags.py
  File DISTDIR\.blender\scripts\mod_meshtools.py
  File DISTDIR\.blender\scripts\off_export.py
  File DISTDIR\.blender\scripts\off_import.py
  File DISTDIR\.blender\scripts\radiosity_export.py
  File DISTDIR\.blender\scripts\radiosity_import.py
  File DISTDIR\.blender\scripts\raw_export.py
  File DISTDIR\.blender\scripts\raw_import.py
  File DISTDIR\.blender\scripts\uv_export.py
  File DISTDIR\.blender\scripts\videoscape_export.py
  File DISTDIR\.blender\scripts\wrl2export.py
  SetOutPath $BLENDERHOME\.blender\bpydata
  File DISTDIR\.blender\bpydata\readme.txt
  
  ; Additional Languages files
  SetOutPath $BLENDERHOME\.blender
  File DISTDIR\.blender\.Blanguages
  SetOutPath $BLENDERHOME\.blender\locale\ca\LC_MESSAGES
  File DISTDIR\.blender\locale\ca\LC_MESSAGES\blender.mo
  SetOutPath $BLENDERHOME\.blender\locale\cs\LC_MESSAGES
  File DISTDIR\.blender\locale\cs\LC_MESSAGES\blender.mo
  SetOutPath $BLENDERHOME\.blender\locale\de\LC_MESSAGES
  File DISTDIR\.blender\locale\de\LC_MESSAGES\blender.mo
  SetOutPath $BLENDERHOME\.blender\locale\fi\LC_MESSAGES
  File DISTDIR\.blender\locale\fi\LC_MESSAGES\blender.mo
  SetOutPath $BLENDERHOME\.blender\locale\es\LC_MESSAGES
  File DISTDIR\.blender\locale\es\LC_MESSAGES\blender.mo
  SetOutPath $BLENDERHOME\.blender\locale\fr\LC_MESSAGES
  File DISTDIR\.blender\locale\fr\LC_MESSAGES\blender.mo
  SetOutPath $BLENDERHOME\.blender\locale\it\LC_MESSAGES
  File DISTDIR\.blender\locale\it\LC_MESSAGES\blender.mo
  SetOutPath $BLENDERHOME\.blender\locale\ja\LC_MESSAGES
  File DISTDIR\.blender\locale\ja\LC_MESSAGES\blender.mo
  SetOutPath $BLENDERHOME\.blender\locale\nl\LC_MESSAGES
  File DISTDIR\.blender\locale\nl\LC_MESSAGES\blender.mo
  SetOutPath $BLENDERHOME\.blender\locale\sv\LC_MESSAGES
  File DISTDIR\.blender\locale\sv\LC_MESSAGES\blender.mo
  SetOutPath $BLENDERHOME\.blender\locale\zh_cn\LC_MESSAGES
  File DISTDIR\.blender\locale\zh_cn\LC_MESSAGES\blender.mo
  SetOutPath $BLENDERHOME\.blender\locale\pt_br\LC_MESSAGES
  File DISTDIR\.blender\locale\pt_br\LC_MESSAGES\blender.mo
  
  SetOutPath $INSTDIR
  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\BlenderFoundation "Install_Dir" "$INSTDIR"
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blender" "DisplayName" "Blender (remove only)"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blender" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteUninstaller "uninstall.exe"
SectionEnd

Section "Add Start Menu shortcuts" Section2
  SetOutPath $INSTDIR
  CreateDirectory "$SMPROGRAMS\Blender Foundation\Blender\"
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\Blender.lnk" "$INSTDIR\Blender.exe" "" "$INSTDIR\blender.exe" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\Readme.lnk" "$INSTDIR\Readme.txt" "" "" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\Copyright.lnk" "$INSTDIR\Copyright.txt" "" "$INSTDIR\copyright.txt" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\GPL-license.lnk" "$INSTDIR\GPL-license.txt" "" "$INSTDIR\GPL-license.txt" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\Help.lnk" "$INSTDIR\Help.url"
SectionEnd

Section "Add Desktop Blender-VERSION shortcut" Section3
  SetOutPath $INSTDIR
  CreateShortCut "$DESKTOP\Blender.lnk" "$INSTDIR\blender.exe" "" "$INSTDIR\blender.exe" 0
SectionEnd

Section "Open .blend files with Blender-VERSION" Section4
  SetOutPath $INSTDIR
  ;ExecShell "open" '"$INSTDIR\blender.exe"' "-R -b"
  ;do it the manual way! ;)
  
  WriteRegStr HKCR ".blend" "" "blendfile"
  WriteRegStr HKCR "blendfile" "" "Blender .blend File"
  WriteRegStr HKCR "blendfile\shell" "" "open"
  WriteRegStr HKCR "blendfile\DefaultIcon" "" $INSTDIR\blender.exe,1
  WriteRegStr HKCR "blendfile\shell\open\command" "" \
    '"$INSTDIR\blender.exe" "%1"'
  
SectionEnd

UninstallText "This will uninstall Blender VERSION. Hit next to continue."

Section "Uninstall"
  ; remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blender"
  DeleteRegKey HKLM SOFTWARE\BlenderFoundation
  ; remove files
  Delete $INSTDIR\blender.exe
  Delete $INSTDIR\python22.dll
  Delete $INSTDIR\sdl.dll
  Delete $INSTDIR\solid.dll
  Delete $INSTDIR\gnu_gettext.dll
  Delete $INSTDIR\Copyright.txt
  Delete $INSTDIR\Readme.txt
  Delete $INSTDIR\GPL-license.txt
  Delete $INSTDIR\Release_SHORTVERS.txt
  Delete $INSTDIR\Help.url
  Delete $INSTDIR\uninstall.exe
  Delete $BLENDERHOME\.blender\.bfont.ttf
  Delete $BLENDERHOME\.blender\.Blanguages
  ; remove shortcuts, if any.
  Delete "$SMPROGRAMS\Blender Foundation\Blender\*.*"
  Delete "$DESKTOP\Blender.lnk"
  ; remove directories used.
  RMDir /r $BLENDERHOME\.blender\locale 
  RMDir /r $BLENDERHOME\.blender\scripts
  RMDir /r $BLENDERHOME\.blender\bpydata
  RMDir $BLENDERHOME\.blender
  RMDir "$SMPROGRAMS\Blender Foundation\Blender"
  RMDir "$SMPROGRAMS\Blender Foundation"
  RMDir "$INSTDIR"
  RMDir "$INSTDIR\.."
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecCopyUI} $(DESC_SecCopyUI)
  !insertmacro MUI_DESCRIPTION_TEXT ${Section2} $(DESC_Section2)
  !insertmacro MUI_DESCRIPTION_TEXT ${Section3} $(DESC_Section3)
  !insertmacro MUI_DESCRIPTION_TEXT ${Section4} $(DESC_Section4)
!insertmacro MUI_FUNCTION_DESCRIPTION_END
