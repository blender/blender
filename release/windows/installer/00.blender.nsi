;
; $Id$
;
; Blender Self-Installer for Windows (NSIS - http://nsis.sourceforge.net)
;
; 09.02.2003    aphex (mediocre@mixnmojo.com)
;

!include "MUI.nsh"

!define MUI_PRODUCT "Blender" ;Define your own software name here
!define MUI_VERSION "VERSION" ;Define your own software version here

!insertmacro MUI_LANGUAGEFILE_STRING MUI_TEXT_WELCOME_INFO_TEXT "This wizard will guide you through the installation of ${MUI_PRODUCT}.\r\n\r\nIt is recommended that you close all other applications before starting Setup.\r\n\r\n"

!define MUI_WELCOMEPAGE
!define MUI_LICENSEPAGE
!define MUI_COMPONENTSPAGE
  !define MUI_COMPONENTSPAGE_SMALLDESC
    
!define MUI_DIRECTORYPAGE

!define MUI_ABORTWARNING

!define MUI_FINISHPAGE
    !define MUI_FINISHPAGE_RUN "$INSTDIR\blender.exe"
  
!define MUI_UNINSTALLER
!define MUI_UNCONFIRMPAGE
  
!define MUI_HEADERBITMAP "00.header.bmp"
!define MUI_SPECIALBITMAP "01.installer.bmp"
!define MUI_ICON "00.installer.ico"
!define MUI_UNICON "00.installer.ico"
!define MUI_CHECKBITMAP "00.checked.bmp"

;--------------------------------
;Languages
 
  !insertmacro MUI_LANGUAGE "English"
    
;--------------------------------
;Language Strings

  ;Description
  LangString DESC_SecCopyUI ${LANG_ENGLISH} "Copy all required files to the application folder."
  LangString DESC_Section2 ${LANG_ENGLISH} "Add shortcut items to the Start Menu. (Recommended)"
  LangString DESC_Section3 ${LANG_ENGLISH} "Add a shortcut to Blender on your desktop."
  LangString DESC_Section4 ${LANG_ENGLISH} "Blender can register itself with .blend files to allow double-clicking from Explorer, etc."
  
;--------------------------------
;Data
  
  LicenseData "DISTDIR\Copyright.txt"
  

Caption "Blender VERSION Installer"
OutFile "DISTDIR\..\VERSION\blender-VERSION-windows.exe"

InstallDir "$PROGRAMFILES\Blender Foundation\Blender"

BrandingText "http://www.blender.org/bf"
ComponentText "This will install Blender VERSION on your computer."

DirText "Use the field below to specify the folder where you want Blender to be copied to. To specify a different folder, type a new name or use the Browse button to select an existing folder."

Section "Blender-VERSION (required)" SecCopyUI
  SectionIn RO
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  ; Put file there
  File DISTDIR\blender.exe
  File DISTDIR\python22.dll
  File DISTDIR\sdl.dll
  File DISTDIR\gnu_gettext.dll
  File DISTDIR\Copyright.txt
  File DISTDIR\Readme.txt
  File DISTDIR\Release_SHORTVERS.txt
  File DISTDIR\GPL-license.txt
  File DISTDIR\Help.url
  SetOutPath $INSTDIR\.blender
  File DISTDIR\.blender\.bfont.ttf
  SetOutPath $INSTDIR\.blender\scripts
  File DISTDIR\.blender\scripts\ac3d_export.py
  File DISTDIR\.blender\scripts\ac3d_import.py
  File DISTDIR\.blender\scripts\blender2cal3d.py
  File DISTDIR\.blender\scripts\directxexporter.py
  SetOutPath $INSTDIR\.blender\bpydata
  File DISTDIR\.blender\bpydata\readme.txt
  
  ; Additional Languages files
  SetOutPath $INSTDIR\.blender
  File DISTDIR\.blender\.Blanguages
  SetOutPath $INSTDIR\.blender\locale\ca\LC_MESSAGES
  File DISTDIR\.blender\locale\ca\LC_MESSAGES\blender.mo
  SetOutPath $INSTDIR\.blender\locale\cs\LC_MESSAGES
  File DISTDIR\.blender\locale\cs\LC_MESSAGES\blender.mo
  SetOutPath $INSTDIR\.blender\locale\de\LC_MESSAGES
  File DISTDIR\.blender\locale\de\LC_MESSAGES\blender.mo
  SetOutPath $INSTDIR\.blender\locale\fi\LC_MESSAGES
  File DISTDIR\.blender\locale\fi\LC_MESSAGES\blender.mo
  SetOutPath $INSTDIR\.blender\locale\es\LC_MESSAGES
  File DISTDIR\.blender\locale\es\LC_MESSAGES\blender.mo
  SetOutPath $INSTDIR\.blender\locale\fr\LC_MESSAGES
  File DISTDIR\.blender\locale\fr\LC_MESSAGES\blender.mo
  SetOutPath $INSTDIR\.blender\locale\it\LC_MESSAGES
  File DISTDIR\.blender\locale\it\LC_MESSAGES\blender.mo
  SetOutPath $INSTDIR\.blender\locale\ja\LC_MESSAGES
  File DISTDIR\.blender\locale\ja\LC_MESSAGES\blender.mo
  SetOutPath $INSTDIR\.blender\locale\nl\LC_MESSAGES
  File DISTDIR\.blender\locale\nl\LC_MESSAGES\blender.mo
  SetOutPath $INSTDIR\.blender\locale\sv\LC_MESSAGES
  File DISTDIR\.blender\locale\sv\LC_MESSAGES\blender.mo
  
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
    '$INSTDIR\blender.exe "%1"'
  
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
  Delete $INSTDIR\gnu_gettext.dll
  Delete $INSTDIR\Copyright.txt
  Delete $INSTDIR\Readme.txt
  Delete $INSTDIR\GPL-license.txt
  Delete $INSTDIR\Release_SHORTVERS.txt
  Delete $INSTDIR\Help.url
  Delete $INSTDIR\uninstall.exe
  Delete $INSTDIR\.blender\.bfont.ttf
  Delete $INSTDIR\.blender\.Blanguages
  ; remove shortcuts, if any.
  Delete "$SMPROGRAMS\Blender Foundation\Blender\*.*"
  Delete "$DESKTOP\Blender.lnk"
  ; remove directories used.
  RMDir /r $INSTDIR\.blender\locale 
  RMDir /r $INSTDIR\.blender\scripts
  RMDir /r $INSTDIR\.blender\bpydata
  RMDir $INSTDIR\.blender
  RMDir "$SMPROGRAMS\Blender Foundation\Blender"
  RMDir "$SMPROGRAMS\Blender Foundation"
  RMDir "$INSTDIR"
  RMDir "$INSTDIR\.."
SectionEnd

!insertmacro MUI_FUNCTIONS_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecCopyUI} $(DESC_SecCopyUI)
  !insertmacro MUI_DESCRIPTION_TEXT ${Section2} $(DESC_Section2)
  !insertmacro MUI_DESCRIPTION_TEXT ${Section3} $(DESC_Section3)
  !insertmacro MUI_DESCRIPTION_TEXT ${Section4} $(DESC_Section4)
!insertmacro MUI_FUNCTIONS_DESCRIPTION_END
