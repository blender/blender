;
; 00.blender.nsi
;
; Blender Self-Installer for Windows (NSIS - http://nsis.sourceforge.net)
;
; 09.02.2003    aphex (mediocre@mixmojo.com)
;

Name "Blender"
Caption "Blender Installer"
OutFile "blender-installer-win32-2.26.exe"
Icon "00.installer.ico"

EnabledBitmap "00.checked.bmp"
DisabledBitmap "00.unchecked.bmp"

InstallDir "$PROGRAMFILES\Blender Foundation\Blender"

LicenseText "Please read and agree to the license below:"
LicenseData "..\..\text\copyright.txt"

Function .onInstSuccess
	MessageBox MB_YESNO "Blender was successfully setup on your computer. Do you wish to start Blender now?" IDNO NoThanks
		ExecShell "open" '"$INSTDIR\blender-2.26.exe"'
	NoThanks:
FunctionEnd

BrandingText "http://www.blender.org/bf"
ComponentText "This will install Blender 2.26 on your computer."

DirText "Use the field below to specify the folder where you want Blender to be copied to. To specify a different folder, type a new name or use the Browse button to select an existing folder."

Section "Blender-2.26 (required)"
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  ; Put file there
  File ..\..\..\obj\windows\blender-2.26.exe
  File ..\..\..\lib\windows\python\lib\python22.dll
  File ..\..\text\copyright.txt
  File ..\..\text\README
  File ..\extra\help.url
  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\BlenderFoundation "Install_Dir" "$INSTDIR"
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blender226" "DisplayName" "Blender 2.26 (remove only)"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blender226" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteUninstaller "uninstall.exe"
SectionEnd

SectionDivider

Section "Start Menu Shortcuts"
  CreateDirectory "$SMPROGRAMS\Blender Foundation\Blender\"
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\blender-2.26.lnk" "$INSTDIR\blender-2.26.exe" "" "$INSTDIR\blender-2.26.exe" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\README.lnk" "$INSTDIR\README" "" "" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\copyright.lnk" "$INSTDIR\copyright.txt" "" "$INSTDIR\copyright.txt" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\help.lnk" "$INSTDIR\help.url"
  MessageBox MB_YESNO "Do you wish to create a shortcut on your desktop?" IDNO NoDeskShortcut
      CreateShortCut "$DESKTOP\Blender-2.26.lnk" "$INSTDIR\blender-2.26.exe" "" "$INSTDIR\blender-2.26.exe" 0
  NoDeskShortcut:
SectionEnd

UninstallText "This will uninstall Blender 2.26. Hit next to continue."

Section "Uninstall"
  ; remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blender226"
  DeleteRegKey HKLM SOFTWARE\BlenderFoundation
  ; remove files
  Delete $INSTDIR\blender-2.26.exe
  Delete $INSTDIR\python22.dll
  Delete $INSTDIR\copyright.txt
  Delete $INSTDIR\README
  Delete $INSTDIR\uninstall.exe
  ; remove shortcuts, if any.
  Delete "$SMPROGRAMS\Blender Foundation\Blender\*.*"
  Delete "$DESKTOP\Blender-2.26.lnk"
  ; remove directories used.
  RMDir "$SMPROGRAMS\Blender Foundation\Blender"
  RMDir "$SMPROGRAMS\Blender Foundation"
  RMDir "$INSTDIR"
SectionEnd