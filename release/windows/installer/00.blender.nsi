;
; $Id$
;
; Blender Self-Installer for Windows (NSIS - http://nsis.sourceforge.net)
;
; 09.02.2003    aphex (mediocre@mixmojo.com)
;

Name "Blender"
Caption "Blender VERSION Installer"
OutFile "DISTDIR\..\VERSION\blender-VERSION-windows.exe"
Icon "00.installer.ico"

EnabledBitmap "00.checked.bmp"
DisabledBitmap "00.unchecked.bmp"

InstallDir "$PROGRAMFILES\Blender Foundation\Blender-VERSION"

LicenseText "Please read and agree to the license below:"
LicenseData "DISTDIR\Copyright.txt"

Function .onInstSuccess
	MessageBox MB_YESNO "Blender was successfully setup on your computer. $\rDo you wish to start Blender now ?" IDNO NoThanks
		ExecShell "open" '"$INSTDIR\blender.exe"'
	NoThanks:
FunctionEnd

BrandingText "http://www.blender.org/bf"
ComponentText "This will install Blender VERSION on your computer."

DirText "Use the field below to specify the folder where you want Blender to be copied to. To specify a different folder, type a new name or use the Browse button to select an existing folder."

Section "Blender-VERSION (required)"
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  ; Put file there
  File DISTDIR\blender.exe
  File DISTDIR\python22.dll
  File DISTDIR\Copyright.txt
  File DISTDIR\Readme.txt
  File DISTDIR\GPL-license.txt
  File DISTDIR\Help.url
  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\BlenderFoundation "Install_Dir" "$INSTDIR"
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BlenderSHORTVERS" "DisplayName" "Blender VERSION (remove only)"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BlenderSHORTVERS" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteUninstaller "uninstall.exe"
SectionEnd

SectionDivider

Section "Add Start Menu shortcuts"
  CreateDirectory "$SMPROGRAMS\Blender Foundation\Blender-VERSION\"
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender-VERSION\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender-VERSION\Blender.lnk" "$INSTDIR\Blender.exe" "" "$INSTDIR\blender.exe" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender-VERSION\Readme.lnk" "$INSTDIR\Readme.txt" "" "" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender-VERSION\Copyright.lnk" "$INSTDIR\Copyright.txt" "" "$INSTDIR\copyright.txt" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender-VERSION\GPL-license.lnk" "$INSTDIR\GPL-license.txt" "" "$INSTDIR\GPL-license.txt" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender-VERSION\Help.lnk" "$INSTDIR\Help.url"
  ; MessageBox MB_YESNO "Do you wish to create a shortcut on your desktop?" IDNO NoDeskShortcut
  ;     CreateShortCut "$DESKTOP\Blender-VERSION.lnk" "$INSTDIR\blender.exe" "" "$INSTDIR\blender.exe" 0
  ; NoDeskShortcut:
SectionEnd

Section "Add Desktop Blender-VERSION shortcut"
  CreateShortCut "$DESKTOP\Blender-VERSION.lnk" "$INSTDIR\blender.exe" "" "$INSTDIR\blender.exe" 0
SectionEnd

Section "Open .blend files with Blender-VERSION"
  ExecShell "open" '"$INSTDIR\blender.exe"' "-R -b"
SectionEnd

UninstallText "This will uninstall Blender VERSION. Hit next to continue."

Section "Uninstall"
  ; remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BlenderSHORTVERS"
  DeleteRegKey HKLM SOFTWARE\BlenderFoundation
  ; remove files
  Delete $INSTDIR\blender.exe
  Delete $INSTDIR\python22.dll
  Delete $INSTDIR\Copyright.txt
  Delete $INSTDIR\Readme.txt
  Delete $INSTDIR\GPL-license.txt
  Delete $INSTDIR\Help.url
  Delete $INSTDIR\uninstall.exe
  ; remove shortcuts, if any.
  Delete "$SMPROGRAMS\Blender Foundation\Blender-VERSION\*.*"
  Delete "$DESKTOP\Blender-VERSION.lnk"
  ; remove directories used.
  RMDir "$SMPROGRAMS\Blender Foundation\Blender-VERSION"
  RMDir "$SMPROGRAMS\Blender Foundation"
  RMDir "$INSTDIR"
  RMDir "$INSTDIR\.."
SectionEnd
