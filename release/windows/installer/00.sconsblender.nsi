;
; $Id$
;
; Blender Self-Installer for Windows (NSIS - http://nsis.sourceforge.net)
;
; Requires the MoreInfo plugin - http://nsis.sourceforge.net/MoreInfo_plug-in
;

!include "MUI.nsh"
!include "WinVer.nsh"
!include "FileFunc.nsh"
!include "WordFunc.nsh"
!include "nsDialogs.nsh"
!include "x64.nsh"


SetCompressor /SOLID lzma

Name "Blender [VERSION]" 

!define MUI_ABORTWARNING

!define MUI_WELCOMEPAGE_TEXT  "This wizard will guide you through the installation of Blender. It is recommended that you close all other applications before starting Setup."
!define MUI_WELCOMEFINISHPAGE_BITMAP "[RELDIR]\01.installer.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP  "[RELDIR]\00.header.bmp"
!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_FINISHPAGE_RUN "$INSTDIR\blender.exe"
!define MUI_CHECKBITMAP "[RELDIR]\00.checked.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "[RELDIR]\01.installer.bmp"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "[DISTDIR]\Copyright.txt"
!insertmacro MUI_PAGE_COMPONENTS
    
!insertmacro MUI_PAGE_DIRECTORY
Page custom DataLocation DataLocationOnLeave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
  
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro Locate
!insertmacro VersionCompare


Icon "[RELDIR]\00.installer.ico"
UninstallIcon "[RELDIR]\00.installer.ico"

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
  LangString TEXT_IO_TITLE ${LANG_ENGLISH} "Specify User Data Location"
;--------------------------------
;Data

Caption "Blender [VERSION] Installer"
OutFile "[DISTDIR]\..\blender-[VERSION]-windows[BITNESS].exe"
InstallDir "$PROGRAMFILES[BITNESS]\Blender Foundation\Blender"

BrandingText "Blender Foundation | http://www.blender.org"
ComponentText "This will install Blender [VERSION] on your computer."

DirText "Use the field below to specify the folder where you want Blender to be copied to. To specify a different folder, type a new name or use the Browse button to select an existing folder."

SilentUnInstall normal

Var BLENDERHOME
Var SHORTVERSION ; This is blender_version_decimal() from path_util.c

; custom controls
Var HWND

Var HWND_APPDATA
Var HWND_INSTDIR
Var HWND_HOMEDIR

Function .onInit
  ClearErrors
  StrCpy $SHORTVERSION "[SHORTVERSION]"
FunctionEnd

Function DataLocation
  nsDialogs::Create /NOUNLOAD 1018
  Pop $HWND
  
  ${If} $HWND == error
    Abort
  ${EndIf}
  
  ${NSD_CreateLabel} 0 0 100% 12u "Please specify where you wish to install Blender's user data files."
  ${NSD_CreateRadioButton} 0 20 100% 12u "Use the Application Data directory"
  Pop $HWND_APPDATA
  ${NSD_CreateRadioButton} 0 50 100% 12u "Use the installation directory (ie. location chosen to install blender.exe)."
  Pop $HWND_INSTDIR
  ${NSD_CreateRadioButton} 0 80 100% 12u "I have defined a %HOME% variable, please install files here."
  Pop $HWND_HOMEDIR
  
  ${If} ${AtMostWinME}
    GetDlgItem $0 $HWND $HWND_APPDATA
    EnableWindow $0 0
    SendMessage $HWND_INSTDIR ${BM_SETCHECK} 1 0
  ${Else}
    SendMessage $HWND_APPDATA ${BM_SETCHECK} 1 0
  ${EndIf}
  
  nsDialogs::Show
  
FunctionEnd

Function DataLocationOnLeave
  ${NSD_GetState} $HWND_APPDATA $R0
  ${If} $R0 == "1"
    StrCpy $BLENDERHOME "$APPDATA\Blender Foundation\Blender"
  ${Else}
    ${NSD_GetState} $HWND_INSTDIR $R0
    ${If} $R0 == "1"
      StrCpy $BLENDERHOME $INSTDIR
    ${Else}
      ${NSD_GetState} $HWND_HOMEDIR $R0
      ${If} $R0 == "1"
        ReadEnvStr $BLENDERHOME "HOME"
      ${EndIf}
    ${EndIf}
  ${EndIf}
FunctionEnd

Section "Blender-[VERSION] (required)" SecCopyUI
  SectionIn RO

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  ; the contents of Blender installation root dir
  [ROOTDIRCONTS]
  
  ; all datafiles (python, scripts, config)
  [DODATAFILES]
  
  SetOutPath $INSTDIR
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
  ; Write the installation path into the registry
  WriteRegStr HKLM "SOFTWARE\BlenderFoundation" "Install_Dir" "$INSTDIR"
  WriteRegStr HKLM "SOFTWARE\BlenderFoundation" "ConfigData_Dir" "$BLENDERHOME"
  WriteRegStr HKLM "SOFTWARE\BlenderFoundation" "ShortVersion" "[SHORTVERSION]"
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blender" "DisplayName" "Blender (remove only)"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blender" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteUninstaller "uninstall.exe"

SectionEnd

Section "Add Start Menu shortcuts" Section2
  SetShellVarContext all
  CreateDirectory "$SMPROGRAMS\Blender Foundation\Blender\"
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\Blender.lnk" "$INSTDIR\Blender.exe" "" "$INSTDIR\blender.exe" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\Readme.lnk" "$INSTDIR\readme.html" "" "" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\Copyright.lnk" "$INSTDIR\Copyright.txt" "" "$INSTDIR\copyright.txt" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\GPL-license.lnk" "$INSTDIR\GPL-license.txt" "" "$INSTDIR\GPL-license.txt" 0
SectionEnd

Section "Add Desktop Blender-[VERSION] shortcut" Section3
  CreateShortCut "$DESKTOP\Blender.lnk" "$INSTDIR\blender.exe" "" "$INSTDIR\blender.exe" 0
SectionEnd

Section "Open .blend files with Blender-[VERSION]" Section4
  
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
  WriteRegStr HKCR ".blend" "" "blendfile"
  WriteRegStr HKCR "blendfile" "" "Blender .blend File"
  WriteRegStr HKCR "blendfile\shell" "" "open"
  WriteRegStr HKCR "blendfile\DefaultIcon" "" $INSTDIR\blender.exe,1
  WriteRegStr HKCR "blendfile\shell\open\command" "" \
    '"$INSTDIR\blender.exe" "%1"'
  
SectionEnd

UninstallText "This will uninstall Blender [VERSION], and all installed files. Before continuing make sure you have created backup of all the files you may want to keep: startup.blend, bookmarks.txt, recent-files.txt. Hit next to continue."

Section "Uninstall"
  ; remove registry keys
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
  ReadRegStr $BLENDERHOME HKLM "SOFTWARE\BlenderFoundation" "ConfigData_Dir"
  ReadRegStr $SHORTVERSION HKLM "SOFTWARE\BlenderFoundation" "ShortVersion"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blender"
  DeleteRegKey HKLM "SOFTWARE\BlenderFoundation"
  SetShellVarContext all
 
  ; remove files
  [DELROOTDIRCONTS]

  ; remove bundled python
  RmDir /r $INSTDIR\$SHORTVERSION\python

  Delete "$INSTDIR\uninstall.exe"

  MessageBox MB_YESNO "Recursively erase contents of $BLENDERHOME\$SHORTVERSION\scripts? NOTE: This includes all installed scripts and *any* file and directory you have manually created, installed later or copied. This also including .blend files." IDNO NextNoScriptRemove
  RMDir /r "$BLENDERHOME\$SHORTVERSION\scripts"
NextNoScriptRemove:
  MessageBox MB_YESNO "Recursively erase contents from $BLENDERHOME\$SHORTVERSION\config? NOTE: This includes your startup.blend, bookmarks and any other file and directory you may have created in that directory" IDNO NextNoConfigRemove
  RMDir /r "$BLENDERHOME\$SHORTVERSION\config"
NextNoConfigRemove:
  MessageBox MB_YESNO "Recursively erase contents from $BLENDERHOME\$SHORTVERSION\plugins? NOTE: This includes files and subdirectories in this directory" IDNO NextNoPluginRemove
  RMDir /r "$BLENDERHOME\$SHORTVERSION\plugins"
NextNoPluginRemove:
  ; try to remove dirs, but leave them if they contain anything
  RMDir "$BLENDERHOME\$SHORTVERSION\plugins"
  RMDir "$BLENDERHOME\$SHORTVERSION\config"
  RMDir "$BLENDERHOME\$SHORTVERSION\scripts"
  RMDir "$BLENDERHOME\$SHORTVERSION"
  RMDir "$BLENDERHOME"
  ; remove shortcuts
  Delete "$SMPROGRAMS\Blender Foundation\Blender\*.*"
  Delete "$DESKTOP\Blender.lnk"
  ; remove all link related directories and files
  RMDir "$SMPROGRAMS\Blender Foundation\Blender"
  RMDir "$SMPROGRAMS\Blender Foundation"
  ; Clear out installation dir
  RMDir "$INSTDIR"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecCopyUI} $(DESC_SecCopyUI)
  !insertmacro MUI_DESCRIPTION_TEXT ${Section2} $(DESC_Section2)
  !insertmacro MUI_DESCRIPTION_TEXT ${Section3} $(DESC_Section3)
  !insertmacro MUI_DESCRIPTION_TEXT ${Section4} $(DESC_Section4)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

