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

SetCompressor /SOLID lzma

Name "Blender VERSION" 

!define MUI_ABORTWARNING

!define MUI_WELCOMEPAGE_TEXT  "This wizard will guide you through the installation of Blender.\r\n\r\nIt is recommended that you close all other applications before starting Setup.\r\n\r\nNote to Win2k/XP users: You may require administrator privileges to install Blender successfully."
!define MUI_WELCOMEFINISHPAGE_BITMAP "RELDIR\01.installer.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP  "RELDIR\00.header.bmp"
!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_FINISHPAGE_RUN "$INSTDIR\blender.exe"
!define MUI_CHECKBITMAP "RELDIR\00.checked.bmp"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "DISTDIR\Copyright.txt"
!insertmacro MUI_PAGE_COMPONENTS
    
!insertmacro MUI_PAGE_DIRECTORY
Page custom DataLocation DataLocationOnLeave
;Page custom AppDataChoice AppDataChoiceOnLeave
Page custom PreMigrateUserSettings MigrateUserSettings
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
  
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro Locate
!insertmacro VersionCompare


Icon "RELDIR\00.installer.ico"
UninstallIcon "RELDIR\00.installer.ico"

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

Caption "Blender VERSION Installer"
OutFile "DISTDIR\..\blender-VERSION-windows.exe"
InstallDir "$PROGRAMFILES\Blender Foundation\Blender"

BrandingText "http://www.blender.org"
ComponentText "This will install Blender VERSION on your computer."

DirText "Use the field below to specify the folder where you want Blender to be copied to. To specify a different folder, type a new name or use the Browse button to select an existing folder."

SilentUnInstall normal

# Uses $0
Function openLinkNewWindow
  Push $3 
  Push $2
  Push $1
  Push $0
  ReadRegStr $0 HKCR "http\shell\open\command" ""
# Get browser path
    DetailPrint $0
  StrCpy $2 '"'
  StrCpy $1 $0 1
  StrCmp $1 $2 +2 # if path is not enclosed in " look for space as final char
    StrCpy $2 ' '
  StrCpy $3 1
  loop:
    StrCpy $1 $0 1 $3
    DetailPrint $1
    StrCmp $1 $2 found
    StrCmp $1 "" found
    IntOp $3 $3 + 1
    Goto loop
 
  found:
    StrCpy $1 $0 $3
    StrCmp $2 " " +2
      StrCpy $1 '$1"'
 
  Pop $0
  Exec '$1 $0'
  Pop $1
  Pop $2
  Pop $3
FunctionEnd

Var BLENDERHOME
Var DLL_found
Var PREVHOME

Function SetWinXPPathCurrentUser
  SetShellVarContext current
  StrCpy $BLENDERHOME "$APPDATA\Blender Foundation\Blender"
FunctionEnd

Function SetWinXPPathAllUsers
  SetShellVarContext all
  StrCpy $BLENDERHOME "$APPDATA\Blender Foundation\Blender"
FunctionEnd

Function SetWin9xPath
  StrCpy $BLENDERHOME $INSTDIR
FunctionEnd

; custom controls
Var HWND

Var HWND_APPDATA
Var HWND_INSTDIR
Var HWND_HOMEDIR

Var HWND_BUTTON_YES
Var HWND_BUTTON_NO

Var SETUSERCONTEXT

Function PreMigrateUserSettings
  StrCpy $PREVHOME "$PROFILE\Application Data\Blender Foundation\Blender"
  StrCpy $0 "$PROFILE\Application Data\Blender Foundation\Blender\.blender"
  
  IfFileExists $0 0 nochange
  
  StrCmp $BLENDERHOME $PREVHOME nochange
  
  nsDialogs::Create /NOUNLOAD 1018
  Pop $HWND
  
  ${If} $HWND == error
	Abort
  ${EndIf}
  
  ${NSD_CreateLabel} 0 0 100% 12u "You have existing settings at:"
  ${NSD_CreateLabel} 0 20 100% 12u $PREVHOME
  ${NSD_CreateLabel} 0 40 100% 12u "Do you wish to migrate this data to:"
  ${NSD_CreateLabel} 0 60 100% 12u $BLENDERHOME
  ${NSD_CreateLabel} 0 80 100% 12u "Please note: If you choose no, Blender will not be able to use these files!"
  ${NSD_CreateRadioButton} 0 100 100% 12u "Yes"
  Pop $HWND_BUTTON_YES
  ${NSD_CreateRadioButton} 0 120 100% 12u "No"
  Pop $HWND_BUTTON_NO
  
  SendMessage $HWND_BUTTON_YES ${BM_SETCHECK} 1 0
  
  nsDialogs::Show
  nochange:
  
FunctionEnd

Function MigrateUserSettings
  ${NSD_GetState} $HWND_BUTTON_YES $R0
  ${If} $R0 == "1"
    CreateDirectory $BLENDERHOME
    CopyFiles $PREVHOME\*.* $BLENDERHOME
    ;RMDir /r $PREVHOME
  ${EndIf}  
FunctionEnd

!define DLL_VER "8.00.50727.42"
!define DLL_VER2 "7.10.3052.4"

Function LocateCallback_80
	MoreInfo::GetProductVersion "$R9"
	Pop $0

        ${VersionCompare} "$0" "${DLL_VER}" $R1

        StrCmp $R1 0 0 new
      new:
        StrCmp $R1 1 0 old
      old:
        StrCmp $R1 2 0 end
	; Found DLL is older
        Call DownloadDLL

     end:
	StrCpy "$0" StopLocate
	StrCpy $DLL_found "true"
	Push "$0"

FunctionEnd

Function LocateCallback_71
	MoreInfo::GetProductVersion "$R9"
	Pop $0

        ${VersionCompare} "$0" "${DLL_VER2}" $R1

        StrCmp $R1 0 0 new
      new:
        StrCmp $R1 1 0 old
      old:
        StrCmp $R1 2 0 end
	; Found DLL is older
        Call PythonInstall

     end:
	StrCpy "$0" StopLocate
	StrCpy $DLL_found "true"
	Push "$0"

FunctionEnd

Function DownloadDLL
    MessageBox MB_OK "You will need to download the Microsoft Visual C++ 2005 Redistributable Package in order to run Blender. Pressing OK will take you to the download page, please follow the instructions on the page that appears."
    StrCpy $0 "http://www.microsoft.com/downloads/details.aspx?familyid=32BC1BEE-A3F9-4C13-9C99-220B62A191EE&displaylang=en"
    Call openLinkNewWindow
FunctionEnd

Function PythonInstall
    MessageBox MB_OK "You will need to install python 2.5 in order to run blender. Pressing OK will take you to the python.org website."
    StrCpy $0 "http://www.python.org"
    Call openLinkNewWindow
FunctionEnd

Function DataLocation
  nsDialogs::Create /NOUNLOAD 1018
  Pop $HWND
  
  ${If} $HWND == error
    Abort
  ${EndIf}
  
  ${NSD_CreateLabel} 0 0 100% 12u "Please specify where you wish to install Blender's user data files."
  ${NSD_CreateRadioButton} 0 20 100% 12u "Use the Application Data directory (Requires Windows 2000 or better)"
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
	StrCpy $SETUSERCONTEXT "false"
	${NSD_GetState} $HWND_APPDATA $R0
	${If} $R0 == "1"
	  ; FIXME: disabled 'all users' until fully multi-user compatible
	  ;StrCpy $SETUSERCONTEXT "true"
	  Call SetWinXPPathCurrentUser
	${Else}
	  ${NSD_GetState} $HWND_INSTDIR $R0
	  ${If} $R0 == "1"
	    Call SetWin9xPath
	  ${Else}
	    ${NSD_GetState} $HWND_HOMEDIR $R0
	    ${If} $R0 == "1"
	      ReadEnvStr $BLENDERHOME "HOME"
	    ${EndIf}
	  ${EndIf}
	${EndIf}
FunctionEnd

Var HWND_APPDATA_CURRENT
Var HWND_APPDATA_ALLUSERS

Function AppDataChoice
  StrCmp $SETUSERCONTEXT "false" skip
  
  nsDialogs::Create /NOUNLOAD 1018
  Pop $HWND
  
  ${NSD_CreateLabel} 0 0 100% 12u "Please choose which Application Data directory to use."
  ${NSD_CreateRadioButton} 0 40 100% 12u "Current User"
  Pop $HWND_APPDATA_CURRENT
  ${NSD_CreateRadioButton} 0 70 100% 12u "All Users"
  Pop $HWND_APPDATA_ALLUSERS
  
  SendMessage $HWND_APPDATA_CURRENT ${BM_SETCHECK} 1 0
  
  StrCmp $SETUSERCONTEXT "true" 0 skip ; show dialog if we need to set context, otherwise skip it
  nsDialogs::Show
  
skip:

FunctionEnd

Function AppDataChoiceOnLeave
	StrCmp $SETUSERCONTEXT "false" skip
	${NSD_GetState} $HWND_APPDATA_CURRENT $R0
	${If} $R0 == "1"
	   Call SetWinXPPathCurrentUser
	${Else}
	   Call SetWinXPPathAllUsers
	${EndIf}
skip:

FunctionEnd

Section "Blender-VERSION (required)" SecCopyUI
  SectionIn RO

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  ; Put file there
 [ROOTDIRCONTS]
     
  SetOutPath $BLENDERHOME\.blender
  [DOTBLENDERCONTS]
  
  SetOutPath $BLENDERHOME\.blender\scripts
  [SCRIPTCONTS]
  SetOutPath $BLENDERHOME\.blender\scripts\bpymodules
  [SCRIPTMODCONTS]
  SetOutPath $BLENDERHOME\.blender\scripts\bpymodules\colladaImEx
  [SCRIPTMODCOLLADACONT]
  SetOutPath $BLENDERHOME\.blender\scripts\bpydata
  [SCRIPTDATACONTS]
  SetOutPath $BLENDERHOME\.blender\scripts\bpydata\config
  [SCRIPTDATACFGCONTS]
  SetOutPath $BLENDERHOME\plugins\include
  [PLUGINCONTS]
  
  ; Language files
  [LANGUAGECONTS]
  
  SetOutPath $INSTDIR
  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\BlenderFoundation "Install_Dir" "$INSTDIR"
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blender" "DisplayName" "Blender (remove only)"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blender" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteUninstaller "uninstall.exe"

  ; Check for msvcr80.dll - give notice to download if not found
  MessageBox MB_OK "The installer will now check your system for the required system dlls."
  StrCpy $1 $WINDIR
  StrCpy $DLL_found "false"
  ${Locate} "$1" "/L=F /M=MSVCR80.DLL /S=0B" "LocateCallback_80"
  StrCmp $DLL_found "false" 0 +2
    Call DownloadDLL
  StrCpy $1 $WINDIR
  StrCpy $DLL_found "false"
  ${Locate} "$1" "/L=F /M=MSVCR71.DLL /S=0B" "LocateCallback_71"
  StrCmp $DLL_found "false" 0 +2
    Call PythonInstall
  
SectionEnd

Section "Add Start Menu shortcuts" Section2
  SetOutPath $INSTDIR
  CreateDirectory "$SMPROGRAMS\Blender Foundation\Blender\"
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\Blender.lnk" "$INSTDIR\Blender.exe" "" "$INSTDIR\blender.exe" 0
  CreateShortCut "$SMPROGRAMS\Blender Foundation\Blender\Readme.lnk" "$INSTDIR\Blender.html" "" "" 0
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
  [DELROOTDIRCONTS]
  
  Delete $INSTDIR\.blender\.bfont.ttf
  Delete $INSTDIR\.blender\.Blanguages
  ; remove shortcuts, if any.
  Delete "$SMPROGRAMS\Blender Foundation\Blender\*.*"
  Delete "$DESKTOP\Blender.lnk"
  ; remove directories used.
  RMDir /r $INSTDIR\.blender\locale
  MessageBox MB_YESNO "Erase .blender\scripts folder? (ALL contents will be erased!)" IDNO Next
  RMDir /r $INSTDIR\.blender\scripts
  RMDir /r $INSTDIR\.blender\scripts\bpymodules
  RMDir /r $INSTDIR\.blender\scripts\bpydata
  RMDir /r $INSTDIR\.blender\scripts\bpydata\config
Next:
  RMDir /r $INSTDIR\plugins\include
  RMDir /r $INSTDIR\plugins
  RMDir $INSTDIR\.blender
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
