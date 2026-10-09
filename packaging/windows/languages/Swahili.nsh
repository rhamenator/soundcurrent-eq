;Language: Swahili (1089); altered NSIS source; native review unverified
;By Joost Verburg

!insertmacro LANGFILE "Swahili" "Swahili" "Kiswahili" "Kiswahili" ; See \Include\LangFile.nsh for a description of these parameters

!ifdef MUI_WELCOMEPAGE
  ${LangFileString} MUI_TEXT_WELCOME_INFO_TITLE "Karibu kwenye usakinishaji wa $(^NameDA)"
  ${LangFileString} MUI_TEXT_WELCOME_INFO_TEXT "Kisakinishi kitakuongoza kusakinisha $(^NameDA).$\r$\n$\r$\nInapendekezwa kufunga programu nyingine zote kabla ya kuanza. Hii itawezesha kusasisha faili husika za mfumo bila kuwasha kompyuta upya.$\r$\n$\r$\n$_CLICK"
!endif

!ifdef MUI_UNWELCOMEPAGE
  ${LangFileString} MUI_UNTEXT_WELCOME_INFO_TITLE "Karibu kwenye uondoaji wa $(^NameDA)"
  ${LangFileString} MUI_UNTEXT_WELCOME_INFO_TEXT "Programu hii itakuongoza kuondoa $(^NameDA).$\r$\n$\r$\nKabla ya kuanza, hakikisha $(^NameDA) haifanyi kazi.$\r$\n$\r$\n$_CLICK"
!endif

!ifdef MUI_LICENSEPAGE
  ${LangFileString} MUI_TEXT_LICENSE_TITLE "Makubaliano ya leseni"
  ${LangFileString} MUI_TEXT_LICENSE_SUBTITLE "Soma masharti ya leseni kabla ya kusakinisha $(^NameDA)."
  ${LangFileString} MUI_INNERTEXT_LICENSE_BOTTOM "Ukikubali masharti ya makubaliano, bofya Ninakubali ili kuendelea. Lazima ukubali makubaliano ili kusakinisha $(^NameDA)."
  ${LangFileString} MUI_INNERTEXT_LICENSE_BOTTOM_CHECKBOX "Ukikubali masharti ya makubaliano, weka alama kwenye kisanduku hapa chini. Lazima ukubali makubaliano ili kusakinisha $(^NameDA). $_CLICK"
  ${LangFileString} MUI_INNERTEXT_LICENSE_BOTTOM_RADIOBUTTONS "Ukikubali masharti ya makubaliano, chagua chaguo la kwanza hapa chini. Lazima ukubali makubaliano ili kusakinisha $(^NameDA). $_CLICK"
!endif

!ifdef MUI_UNLICENSEPAGE
  ${LangFileString} MUI_UNTEXT_LICENSE_TITLE "Makubaliano ya leseni"
  ${LangFileString} MUI_UNTEXT_LICENSE_SUBTITLE "Soma masharti ya leseni kabla ya kuondoa $(^NameDA)."
  ${LangFileString} MUI_UNINNERTEXT_LICENSE_BOTTOM "Ukikubali masharti ya makubaliano, bofya Ninakubali ili kuendelea. Lazima ukubali makubaliano ili kuondoa $(^NameDA)."
  ${LangFileString} MUI_UNINNERTEXT_LICENSE_BOTTOM_CHECKBOX "Ukikubali masharti ya makubaliano, weka alama kwenye kisanduku hapa chini. Lazima ukubali makubaliano ili kuondoa $(^NameDA). $_CLICK"
  ${LangFileString} MUI_UNINNERTEXT_LICENSE_BOTTOM_RADIOBUTTONS "Ukikubali masharti ya makubaliano, chagua chaguo la kwanza hapa chini. Lazima ukubali makubaliano ili kuondoa $(^NameDA). $_CLICK"
!endif

!ifdef MUI_LICENSEPAGE | MUI_UNLICENSEPAGE
  ${LangFileString} MUI_INNERTEXT_LICENSE_TOP "Bonyeza Page Down kuona sehemu iliyobaki ya makubaliano."
!endif

!ifdef MUI_COMPONENTSPAGE
  ${LangFileString} MUI_TEXT_COMPONENTS_TITLE "Chagua vipengele"
  ${LangFileString} MUI_TEXT_COMPONENTS_SUBTITLE "Chagua vipengele vya $(^NameDA) unavyotaka kusakinisha."
!endif

!ifdef MUI_UNCOMPONENTSPAGE
  ${LangFileString} MUI_UNTEXT_COMPONENTS_TITLE "Chagua vipengele"
  ${LangFileString} MUI_UNTEXT_COMPONENTS_SUBTITLE "Chagua vipengele vya $(^NameDA) unavyotaka kuondoa."
!endif

!ifdef MUI_COMPONENTSPAGE | MUI_UNCOMPONENTSPAGE
  ${LangFileString} MUI_INNERTEXT_COMPONENTS_DESCRIPTION_TITLE "Maelezo"
  !ifndef NSIS_CONFIG_COMPONENTPAGE_ALTERNATIVE
    ${LangFileString} MUI_INNERTEXT_COMPONENTS_DESCRIPTION_INFO "Weka kishale cha kipanya juu ya kipengele ili kuona maelezo yake."
  !else
    ${LangFileString} MUI_INNERTEXT_COMPONENTS_DESCRIPTION_INFO "Chagua kipengele ili kuona maelezo yake."
  !endif
!endif

!ifdef MUI_DIRECTORYPAGE
  ${LangFileString} MUI_TEXT_DIRECTORY_TITLE "Chagua mahali pa kusakinisha"
  ${LangFileString} MUI_TEXT_DIRECTORY_SUBTITLE "Chagua folda ya kusakinisha $(^NameDA)."
!endif

!ifdef MUI_UNDIRECTORYPAGE
  ${LangFileString} MUI_UNTEXT_DIRECTORY_TITLE "Chagua mahali pa kuondoa"
  ${LangFileString} MUI_UNTEXT_DIRECTORY_SUBTITLE "Chagua folda ya kuondoa $(^NameDA)."
!endif

!ifdef MUI_INSTFILESPAGE
  ${LangFileString} MUI_TEXT_INSTALLING_TITLE "Inasakinisha"
  ${LangFileString} MUI_TEXT_INSTALLING_SUBTITLE "Subiri wakati $(^NameDA) inasakinishwa."
  ${LangFileString} MUI_TEXT_FINISH_TITLE "Usakinishaji umekamilika"
  ${LangFileString} MUI_TEXT_FINISH_SUBTITLE "Usakinishaji umekamilika kwa mafanikio."
  ${LangFileString} MUI_TEXT_ABORT_TITLE "Usakinishaji umesitishwa"
  ${LangFileString} MUI_TEXT_ABORT_SUBTITLE "Usakinishaji haukukamilika kwa mafanikio."
!endif

!ifdef MUI_UNINSTFILESPAGE
  ${LangFileString} MUI_UNTEXT_UNINSTALLING_TITLE "Inaondoa"
  ${LangFileString} MUI_UNTEXT_UNINSTALLING_SUBTITLE "Subiri wakati $(^NameDA) inaondolewa."
  ${LangFileString} MUI_UNTEXT_FINISH_TITLE "Uondoaji umekamilika"
  ${LangFileString} MUI_UNTEXT_FINISH_SUBTITLE "Uondoaji umekamilika kwa mafanikio."
  ${LangFileString} MUI_UNTEXT_ABORT_TITLE "Uondoaji umesitishwa"
  ${LangFileString} MUI_UNTEXT_ABORT_SUBTITLE "Uondoaji haukukamilika kwa mafanikio."
!endif

!ifdef MUI_FINISHPAGE
  ${LangFileString} MUI_TEXT_FINISH_INFO_TITLE "Kukamilisha usakinishaji wa $(^NameDA)"
  ${LangFileString} MUI_TEXT_FINISH_INFO_TEXT "$(^NameDA) imesakinishwa kwenye kompyuta yako.$\r$\n$\r$\nBofya Maliza kufunga kisakinishi."
  ${LangFileString} MUI_TEXT_FINISH_INFO_REBOOT "Kompyuta lazima iwashwe upya ili kukamilisha usakinishaji wa $(^NameDA). Unataka kuiwasha upya sasa?"
!endif

!ifdef MUI_UNFINISHPAGE
  ${LangFileString} MUI_UNTEXT_FINISH_INFO_TITLE "Kukamilisha uondoaji wa $(^NameDA)"
  ${LangFileString} MUI_UNTEXT_FINISH_INFO_TEXT "$(^NameDA) imeondolewa kwenye kompyuta yako.$\r$\n$\r$\nBofya Maliza kufunga programu ya uondoaji."
  ${LangFileString} MUI_UNTEXT_FINISH_INFO_REBOOT "Kompyuta lazima iwashwe upya ili kukamilisha uondoaji wa $(^NameDA). Unataka kuiwasha upya sasa?"
!endif

!ifdef MUI_FINISHPAGE | MUI_UNFINISHPAGE
  ${LangFileString} MUI_TEXT_FINISH_REBOOTNOW "Washa upya sasa"
  ${LangFileString} MUI_TEXT_FINISH_REBOOTLATER "Nataka kuwasha upya mwenyewe baadaye"
  ${LangFileString} MUI_TEXT_FINISH_RUN "&Endesha $(^NameDA)"
  ${LangFileString} MUI_TEXT_FINISH_SHOWREADME "&Onyesha Readme"
  ${LangFileString} MUI_BUTTONTEXT_FINISH "&Maliza"  
!endif

!ifdef MUI_STARTMENUPAGE
  ${LangFileString} MUI_TEXT_STARTMENU_TITLE "Chagua folda ya menyu ya Mwanzo"
  ${LangFileString} MUI_TEXT_STARTMENU_SUBTITLE "Chagua folda ya menyu ya Mwanzo kwa njia za mkato za $(^NameDA)."
  ${LangFileString} MUI_INNERTEXT_STARTMENU_TOP "Chagua folda ya menyu ya Mwanzo ambamo njia za mkato za programu zitaundwa. Unaweza pia kuandika jina ili kuunda folda mpya."
  ${LangFileString} MUI_INNERTEXT_STARTMENU_CHECKBOX "Usiunde njia za mkato"
!endif

!ifdef MUI_UNCONFIRMPAGE
  ${LangFileString} MUI_UNTEXT_CONFIRM_TITLE "Ondoa $(^NameDA)"
  ${LangFileString} MUI_UNTEXT_CONFIRM_SUBTITLE "Ondoa $(^NameDA) kwenye kompyuta yako."
!endif

!ifdef MUI_ABORTWARNING
  ${LangFileString} MUI_TEXT_ABORTWARNING "Una uhakika unataka kuacha usakinishaji wa $(^Name)?"
!endif

!ifdef MUI_UNABORTWARNING
  ${LangFileString} MUI_UNTEXT_ABORTWARNING "Una uhakika unataka kuacha uondoaji wa $(^Name)?"
!endif

!ifdef MULTIUSER_INSTALLMODEPAGE
  ${LangFileString} MULTIUSER_TEXT_INSTALLMODE_TITLE "Chagua watumiaji"
  ${LangFileString} MULTIUSER_TEXT_INSTALLMODE_SUBTITLE "Chagua watumiaji ambao unataka kuwasakinishia $(^NameDA)."
  ${LangFileString} MULTIUSER_INNERTEXT_INSTALLMODE_TOP "Chagua kama unataka kusakinisha $(^NameDA) kwa ajili yako tu au kwa watumiaji wote wa kompyuta hii. $(^ClickNext)"
  ${LangFileString} MULTIUSER_INNERTEXT_INSTALLMODE_ALLUSERS "Sakinisha kwa kila mtu anayetumia kompyuta hii"
  ${LangFileString} MULTIUSER_INNERTEXT_INSTALLMODE_CURRENTUSER "Sakinisha kwa ajili yangu tu"
!endif
