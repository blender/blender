;
; $Id$
;
; Blender Self-Installer for Windows (NSIS - http://nsis.sourceforge.net)
;

!include "MUI.nsh"
!include "FileFunc.nsh"
!include "WordFunc.nsh"

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
Page custom DataLocation
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

BrandingText "http://www.blender.org/bf"
ComponentText "This will install Blender VERSION on your computer."

DirText "Use the field below to specify the folder where you want Blender to be copied to. To specify a different folder, type a new name or use the Browse button to select an existing folder."

SilentUnInstall normal

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
Var winversion
Var DLL_found

Function SetWinXPPath
  StrCpy $BLENDERHOME "$PROFILE\Application Data\Blender Foundation\Blender"
FunctionEnd

Function SetWin9xPath
  StrCpy $BLENDERHOME $INSTDIR
FunctionEnd

Function .onInit
  Call GetWindowsVersion
  Pop $R0
  Strcpy $winversion $R0
  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "RELDIR\data.ini"
FunctionEnd

!define DLL_VER "8.00.50727.42"

Function LocateCallback

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

Function DownloadDLL
    MessageBox MB_OK "You will need to download the Microsoft Visual C++ 2005 Redistributable Package in order to run Blender. Pressing OK will take you to the download page, please follow the instructions on the page that appears."
    StrCpy $0 "http://www.microsoft.com/downloads/details.aspx?familyid=32BC1BEE-A3F9-4C13-9C99-220B62A191EE&displaylang=en"
    Call openLinkNewWindow
FunctionEnd


Var HWND
Var DLGITEM
Var is2KXP

Function DataLocation
  !insertmacro MUI_HEADER_TEXT "$(TEXT_IO_TITLE)" ""

  ; Set default choice
  !insertmacro MUI_INSTALLOPTIONS_WRITE "data.ini" "Field 3" "State" 1
  
  StrCpy $R1 $winversion 2
  StrCmp $R1 "NT" do_win2kxp
  StrCmp $winversion "2000" do_win2kxp
  StrCmp $winversion "XP" do_win2kxp
  StrCmp $winversion "2003" do_win2kxp
  
  ;else...
  Strcpy $is2KXP "false"

  Goto continue

  do_win2kXP:
    Strcpy $is2KXP "true"
    
  continue: 
  
  !insertmacro MUI_INSTALLOPTIONS_INITDIALOG "data.ini"
  Pop $HWND
  
  Strcmp $is2KXP "true" do_dlg
  
  ; Disable App Data option on Win9x
  
  GetDlgItem $DLGITEM $HWND 1201
  EnableWindow $DLGITEM 0  
  
  do_dlg:
  
    !insertmacro MUI_INSTALLOPTIONS_SHOW
    !insertmacro MUI_INSTALLOPTIONS_READ $R0 "data.ini" "Field 2" "State" ; App Dir
    Strcmp $R0 1 do_app_data
    !insertmacro MUI_INSTALLOPTIONS_READ $R0 "data.ini" "Field 3" "State" ; Inst Dir
    Strcmp $R0 1 do_inst_path
    !insertmacro MUI_INSTALLOPTIONS_READ $R0 "data.ini" "Field 4" "State" ; Home Dir
    Strcmp $R0 1 do_home_path
  
  Goto end
  
  do_app_data:
    Call SetWinXPPath
    Goto end
  do_home_path:
    ReadEnvStr $BLENDERHOME "HOME"
    Goto end
  do_inst_path:
    Call SetWin9xPath
  end:
  
FunctionEnd

Section "Blender-VERSION (required)" SecCopyUI
  SectionIn RO
    
; Sets $BLENDERHOME to suit Windows version...

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
  ${Locate} "$1" "/L=F /M=MSVCR80.DLL /S=0B" "LocateCallback"
  StrCmp $DLL_found "false" 0 +2
    Call DownloadDLL
  
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
