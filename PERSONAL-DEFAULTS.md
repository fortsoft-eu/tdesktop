# Osobní výchozí nastavení

Základ klienta zůstává Telegram Desktop 7.1.3. Úpravy nejsou novou instalací
Windows 2000 ani návratem celé aplikace na starší Telegram.

## Použití a čistý profil

Sestavení: `build-local.cmd Build`. Spuštění: `run-local.cmd`.
Vlastní klient ukládá profil do `.local-data` v kořeni tohoto repozitáře.

Pro čistý test ukončete vlastní klient přes **Quit**, přejmenujte `.local-data`
například na `.local-data-backup` a znovu spusťte `run-local.cmd`.
Vznikne nový profil a bude nutné znovu přihlášení. Složku lze místo přejmenování
smazat, ale pak nezůstane místní záloha. Oficiální profil v AppData se nemění.
`telegram-api.local.json` nemažte; nepatří do profilu.

Lokální vymazání profilu nemaže účet ani cloudové zprávy. Nastavení soukromí,
oznámení účtu a časové limity níže se ukládají na server. Jejich jednorázové
nastavení bylo výslovně schváleno a po čistém přihlášení se provede znovu.
Může se projevit na dalších zařízeních. Neprobíhá ukončování existujících relací
ani mazání platebních nebo doručovacích údajů.

## Vzhled

- Systémový **Unifont** se nepřibaluje ani neinstaluje. Jeho fontový engine
  používá velikost **16 fyzických pixelů**, bez vyhlazování, bez přizpůsobování
  velikosti Open Sans. Ostatní fonty včetně náhradních matematických znaků
  mohou mít jiné velikosti a používají vyhlazování, s výjimkou Tahomy níže.
- Celé nastavení, jeho dialogy, kontextová menu a názvy složek vlevo používají
  systémovou **Tahomu bez vyhlazování**, odpovídající dialogovému písmu 8 pt
  (11 px při 96 DPI). Tahoma se může škálovat s rozhraním; chaty a pole
  pro psaní nadále používají Unifont.
- Neutrální odstíny textu se při vykreslení mění na černou. Barevné texty
  zůstávají barevné; šedé čáry, ikony, fotografie a pozadí se tím nemění.
  Na ručně zvoleném tmavém pozadí může černý text ztratit kontrast.
- Tlačítka mají barvu RGB 212, 208, 200, hranaté rámečky a menší výšku.
  Vypínače jsou nahrazené klasickými zatržítky. Posuvníky v obou hlavních
  typech rolovaných panelů používají Qt Windows, včetně šipek a tažení jezdce.
- Celé nastavení má šedobéžové pozadí RGB 212, 208, 200. Odsazení a řádky nastavení,
  seznamů chatů, kontaktů a členů jsou menší.
- Běžné svislé seznamy včetně fór mají mezi profilovkami 6 px při měřítku
  100 %. Kompaktní fórum zobrazuje název a poslední zprávu bez samostatného
  řádku náhledů témat; seznam témat po otevření fóra zůstává dostupný.
  Chaty se štítky si podle zadání ponechávají vyšší řádky.
- Přetrvávající plovoucí popisky polí, včetně **Caption**, jsou nad rámečkem
  a nezmenšují se ani nezmizí po napsání textu. Běžná nápověda prázdného
  vyhledávání zůstává v jeho poli.
- Vyhledávání vlevo má výšku 28 px při měřítku 100 %; text má 1 px vnitřního
  levého odsazení za dvoupixelovým rámečkem. Dřívější oprava Backspace zůstává.
- Emoji / Stickers / GIFs používají skutečný `QTabBar` ve stylu Qt `Windows`
  a pozadí RGB 212, 208, 200. Rozměry, kreslení textu, ovládání klávesnicí
  a šipky při nedostatku místa zajišťuje Qt. Záložky mají přirozenou šířku
  podle popisků a nadále používají systémový Unifont. Soubory emoji, obrázků
  a samolepek se neupravují. Tato změna nepřesouvá dosavadní zatržítka.
- Pravý detail uživatele a skupiny zachovává rozložení podle 6.2.4.
  **Send message** se zobrazuje i u právě otevřeného chatu a příslušné akce
  mají podobu tlačítek. Dialog samolepek podle 7.0.9 zůstává zachovaný.
- Vnořené tlačítko časového pásma u **Business hours** je odstraněné.
  Při dostupném odlišném časovém pásmu celý řádek přepne jeho zobrazení
  a rozbalí časy; jinak řádek dál rozbaluje a sbaluje přehled.

- Jezdce hodnot používají skutečný `QSlider` se stylem Qt Windows. Přepínače
  s kruhovou volbou vykresluje standardní Qt Windows radio indicator.
- Menu prohlížeče médií jsou světlá, s černým textem a pozadím formulářů.
- Nativní záhlaví Windows používá systémové motivy, barvy a tlačítka. Klient
  mu nevnucuje modrou ani barvy motivu Telegramu; volba systémového rámečku
  zůstává dostupná v nastavení.
- Add to Contacts, Send Message a Block User v profilu mají černý text
  uprostřed a boční odsazení. Block User má stejnou výšku jako Add to Contacts.
- Spodní tlačítka dialogů se sjednocují podle nejširšího popisku, včetně Add
  vlevo u obrázků a dvojice tlačítek u samolepek. Při úzkém okně je šířka
  omezena dostupným místem, aby se tlačítka nepřekrývala.
- Seznam chatů ponechává samostatné místo pro rolovací pruh; čas zprávy
  se nevykresluje pod ním. Výška okna nastavení se nemění podle blízkosti
  konce posouvaného obsahu.

V **Chat Settings → Solid color** jsou volby **White** (#FFFFFF),
**Windows** (#D4D0C8) a **Workspace** (#808080). Workspace je prvotní pozadí;
pozdější ruční změna barvy, obrázku či motivu se zachová. Workspace označuje
plochu aplikace za podokny, nikoli modrou plochu Windows. Dobový seznam hodnot
je doložen v [záznamu na Microsoft Q&A](https://learn.microsoft.com/en-my/answers/questions/2591458/how-to-set-inactive-title-bar-color-in-windows-8).

## Lokální výchozí hodnoty ze snímků

| Oblast | Výchozí hodnoty |
|---|---|
| Oznámení | Všechny účty vypnuto, desktop vypnuto, blikání hlavního panelu zapnuto, zvuk vypnuto, připnuté zprávy zapnuto |
| Počítadla | Zahrnovat ztlumené chaty i složky, počítat nepřečtené zprávy |
| Integrace oznámení | Oznámení Windows vypnuta, respektování Focus mode vypnuto |
| Chat | Dlaždice pozadí vypnuty, adaptivní široké rozložení zapnuto, rychlá akce Change folder |
| Emoji | Velká emoji a automatické nahrazování vypnuto; návrhy emoji i animovaných emoji zapnuty; populární samolepky podle emoji vypnuty |
| Zprávy | Ctrl+Enter pro odeslání, dvojklik Reply, tlačítko Reply zapnuto, tlačítko Reaction vypnuto, přetažení na další kanál vypnuto |
| Okno | Nezobrazovat název chatu a účtu v titulku, zobrazovat počet nepřečtených; systémový rámeček zapnutý |
| Systém | Ikona v tray i hlavním panelu zapnuta, monochromatická ikona vypnuta, obnovení oken vypnuto, kontrola pravopisu zapnuta |
| Start a aktualizace | Autostart, Send To a automatické aktualizace vypnuty; registry oficiální instalace se neupravují |
| Video | Hardwarové dekódování zapnuto; výchozí backend Auto |
| Animace | Samolepky v panelu i chatu zapnuty; animace emoji, chatových efektů, volání a rozhraní vypnuty; posouvání bez dojíždění |
| Stahování | Výchozí složka, neptat se pro každý soubor; soukromé chaty, skupiny i kanály mají fotografie/soubory a autoplay videí, videovzkazů i GIFů zapnuté, limit 4000 MB |
| Fotografie | Experimentální Send Large Photos zapnuto, komprimované fotografie do 2560 px; běžná možnost HD zůstává |
| Jazyk | Honest Telegram (`honesttelegram`), získaný přes standardní Telegram API; při nedostupné síti zůstává dostupný dosavadní jazyk |

Nové profily dostávají lokální hodnoty při vytvoření. Existující osobní profil
je převezme jednou. Pozdější ruční změny mají přednost. Obnovení čistého profilu
odstraní i značky jednorázového převzetí. Písmo, vzhled ovládacích prvků
a černý neutrální text jsou vlastnostmi tohoto sestavení, nikoli jednorázovými
nastaveními profilu.

## Export jednotlivého chatu

Každé nové otevření nastavení exportu chatu zapne všechny druhy médií
(fotografie, videa, hlasové zprávy, videovzkazy, samolepky, GIFy a soubory)
a nastaví limit velikosti na 4000 MB. Dřívější uložené odškrtnutí se nepřebírá.
Ruční změny se použijí pro právě připravovaný export; příští otevření opět
začne se všemi volbami zapnutými. Toto pravidlo se netýká celkového exportu
účtu a neobchází serverová omezení přístupu k obsahu.

## Nastavení účtu na serveru

| Nastavení | Hodnota |
|---|---|
| Phone number, Forwarded messages, Calls, Voice messages | Nobody |
| Last seen & online, Profile photos, Bio | Everybody |
| Birthday, Gifts, Saved Music, Invites | Nobody |
| Messages | Everybody, bez požadavku na Premium nebo platbu Stars |
| Archive and Mute unknown chats | Vypnuto |
| Suggest frequent contacts | Vypnuto |
| Soukromé chaty / skupiny / kanály | Oznámení zapnuta / vypnuta / vypnuta |
| Reakce na zprávy a hlasy v anketách | Zapnuto |
| Contact joined Telegram | Zapnuto |
| Accept calls on this device | Vypnuto |
| Show 18+ Content | Zapnuto pouze při povolení serverem |
| Delete my account if away for | 24 měsíců, hodnota 720 dní používaná klientem |
| Terminate old sessions if inactive for | 12 měsíců, 365 dní |

Výjimky konkrétních uživatelů a chatů se zachovávají. Premium a jiná serverová
omezení se neobcházejí. Výsledek jednotlivých operací se ukládá do lokálních
předvoleb a do `log.txt` pod `Personal defaults:`. Úspěšné operace se neopakují,
dočasné chyby se zkusí při příštím přihlášeném startu a nepodporované volby
zůstanou označené jako nedostupné. Automatické testy nepoužívají přihlášený účet;
přijetí jednotlivých požadavků serverem tedy ověří až skutečné přihlášení.

## Experimentální volby

Zapnuto: Hide AI button, Add View Profile, Show Peer IDs, Show Channel Joined Date,
Show tabbed panel by click, Disable auto-play of the next track, Multi-thread
video decoding, Enable precise High DPI scaling, Use Qt RHI renderer,
FreeType font engine a Send Large Photos.

Vypnuto: Force embedded search, profile media tabs i jejich dělení na fotografie
a videa, Unlimited recent stickers, External media viewer, Hide reply button,
Force non-native notifications availability, GNotification, Modern macOS
notifications, High DPI downscale, Vulkan, QScroller, Disable Touch Bar,
Adjust size of new chat windows, Prefer IPv6, Skip URL scheme register,
Deadlock Detector, webview inspecting a legacy Edge WebView.

Některé volby jsou na Windows nebo s použitým Qt 5.15.19 nedostupné.
Nastavená hodnota Use Qt RHI renderer neznamená, že Qt 5 skutečně používá RHI.
Favorite folder button nemá předepsaný odkaz a zůstává beze změny.

## Sestavení a ověření

`build-local.cmd Build` před kompilací upraví a sestaví pouze Debug část
QtGui z `../Libraries/win64/qt_5.15.19/qtbase`. Skript
`Telegram/build/personal/patch_qt_font.py` napojuje pevnou velikost Unifontu
a převod neutrálního textu na černý pouze pro tento klient pomocí proměnných
prostředí. Úpravy fontového enginu jsou nutné pro oddělení vyhlazování
Unifontu a náhradních znaků uvnitř jednoho textu.

Kontrola nativních záložek 31. 8. 2026 prošla při měřítku 100 % / 96 DPI
a 150 % / 144 DPI. Samostatný prvek vytvořený stejnou funkcí jako panel
se ve všech třech vybraných záložkách pixelově shodoval se standardním
`QTabBar` se stejnou paletou a fontem. Prošlo ovládání myší a klávesnicí,
šířky podle textu, šipky při zúžení i rozložení zprava doleva. Existující
zatržítko zůstalo vlevo. Protokoly jsou v
`.build-tools/current-visual-test-{100,150}-native-tabs-v1/evidence/`.
Testy používaly nové nepřihlášené profily; neověřovaly odesílání emoji ani
samolepek ze skutečného chatu. Dočasný test byl ze zdrojů odstraněn.
Následné Debug sestavení z 31. 8. 2026 uspělo; výsledkem je
`out/Debug/Telegram.exe`. Protokol je v `.build-tools/native-tabbar-build-final.log`.

Další kontroly GUI z 31. 8. 2026 prošly při 100 % / 96 DPI i 150 % / 144 DPI
v `.build-tools/current-visual-test-{100,150}-ui-followups-v2/evidence/`.
Ověřily Tahoma v nastavení a ve složkách, vypnuté vyhlazování, skutečný QSlider
včetně klávesnice a souběhu s aktualizací přehrávání, shodné rozměry tlačítek
Add / Send / Cancel a dvojice u samolepek, výšku a černý text profilových
akcí, skrytí titulku Telegramu a nativní vykreslování rámu Windows.
Rastr diakritiky v popisku byl shodný s neoříznutým Unifontem při obou
měřítkách a kurzor prokazatelně měnil viditelnost. Nastavení barvy záhlaví
na systémovou hodnotu bylo ověřeno v kódu; atributy barvy jsou podle
[dokumentace Microsoftu](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute)
určené pro nastavování, nikoli čtení. První verze testu je chybně zkoušela číst;
tato chybná kontrola byla nahrazena kontrolou nativního rámu.
Testy proběhly bez přihlášení a neposílaly zprávy ani exportní požadavky.
Celé přihlášené nastavení, skutečný export a koncové posouvání panelu nebyly
v tomto automatickém testu procházeny; tyto cesty byly zkontrolovány v kódu.
Dočasný scénář byl opět odstraněn. Finální Debug sestavení bez scénáře
uspělo; protokol je `.build-tools/ui-followups-build-final.log`
a výsledkem je `out/Debug/Telegram.exe`.

Dřívější širší test `current-visual-test-100-v3-user` potvrdil pixelovou
shodu Unifontu se snímkem, ale skončil třemi neúspěšnými kontrolami kolem
výchozího pozadí a okamžiku uložení značky dokončeného stažení jazyka.
Tyto kontroly nejsou testem nativních záložek vyřešeny.

Pravý profilový panel byl dále ověřen 31. 8. 2026 při 100 % / 96 DPI
a 150 % / 144 DPI. Obsah vyhrazuje šířku nativního posuvníku i po zúžení
panelu. Profilové akce a tlačítka dialogů používají nevyhlazenou Tahomu
8 pt Bold, profilové popisky a systémové stavy Tahomu 8 pt Regular.
Jména, biografie, poznámky a uživatelská jména zůstávají v Unifontu;
doplňující systémové údaje jsou oddělené od biografie. Narozeniny jsou
obyčejný údaj, bez tlačítka a akce pro nákup dárku. Profilové akce již
nevynucují velká písmena. Add / Send / Cancel mají shodné rozměry a výšku
22 px při 100 %, 33 px při 150 %.

Test `current-visual-test-{100,150}-profile-followups-v2` prošel bez chyby
v obou měřítkách. Ověřil skutečný profil s místními fiktivními údaji,
geometrii obsahu vůči posuvníku, narozeniny mimo tlačítko, styly textu,
rozměry a barvy akcí. V rastru zkoušeného popisku Tahoma nebyly žádné
vyhlazené pixely; systémový stav člena odpovídal rastru Tahomy a vlastní
text stavu rastru Unifontu. Snímky a protokoly jsou pod
`.build-tools/current-visual-test-{100,150}-profile-followups-v2/evidence/`.
Test měl zakázané síťové spojení a nepoužíval skutečný přihlášený účet.
Neprokazuje opravu ranního pádu ani stabilitu po celé noci. Původní dump
a místní diagnostický spouštěč popisuje `BUILD-WINDOWS-CS.md`.

Dočasný scénář byl odstraněn a finální Debug sestavení bez něj uspělo.
Výstup je `out/Debug/Telegram.exe`; protokol
`.build-tools/profile-followups-build-final.log`.

## Další úpravy ovládacích prvků (31. srpna 2026)

Profilové akce mají jednotnou šířku 160 bodů při měřítku 100 %, výšku
22 bodů, černý centrovaný text a malé ikony. Také samostatný profil používá
vystouplá tlačítka Add to contacts a Block user. Popisek telefonu je přímo
`Cell phone`. Volby White / Windows / Workspace mají stejná kompaktní
tlačítka. Časy v seznamu chatů jsou Tahoma 8 Regular; datum hlasování
Tahoma 8 Bold a čas hlasování Tahoma 8 Regular.

Počítadla na ikoně používají systémovou Tahomu v malých velikostech,
s plným hintingem a bez vyhlazení. Malá ikona v systémovém záhlaví
neobsahuje počítadlo, aby nepřekrývalo logo Telegramu.

Standardní dialogy mají pozadí `#d4d0c8`; vlastní vstupní pole zůstávají
bílá a popisek nad polem přebírá pozadí dialogu. Rolovací plocha sady
samolepek i plochy emoji, GIFů a samolepek v přepínacím panelu jsou bílé.
Záložky a spodní ovládací část panelu zůstávají v barvě formuláře.

Běžná kontextová menu, včetně nabídky ikony v oznamovací oblasti, mají
neprůhledné pozadí, vystouplý rámeček kreslený stylem Qt Windows,
kompaktní řádky a Tahomu 8. Stejný rámeček a pozadí mají informační
panely toast; jejich text používá černou Tahomu 8.

Vystouplá tlačítka typu RoundButton, FlatButton a profilová SettingsButton
posouvají při stisknutí obsah podle metrik stylu Qt. RoundButton a
FlatButton včetně přihlašovacích obrazovek používají nevyhlazenou
Tahomu 8 Bold. Pole Cloud Password má při přihlášení i v nastavení
celkem 37 bodů včetně popisku; heslo zůstává maskované. Texty obrazovky
pro zadání hesla jsou černé v Tahomě 8, nadpis tučný. Odkazy si zachovávají
barevné odlišení. Nadpis Send an image používá Tahomu 8 Bold, popisek
Caption, nápověda a Send as a document Tahomu 8 Regular. Text vložený
uživatelem do popisku fotografie zůstává v Unifontu.

### Místní kopie zpráv a souborů

Zobrazený text lze kopírovat také v chatech s omezením přeposílání.
Save as může uložit již dostupná úplná data dokumentu nebo fotografie
z paměti či místního souboru. Tato cesta nic nedotahuje ze serveru.
Zápis používá dočasný soubor a atomické dokončení, aby chyba nezničila
existující cílový soubor. Pokud jsou dostupná původní obrazová data,
ukládají se beze změny; samotný dekódovaný obraz se uloží jako JPEG.

Položka Send a copy otevře výběr příjemce. Z místního textu, fotografie
či dokumentu vytvoří novou zprávu, nikoli serverové přeposlání původní
zprávy. Zachovává text, formátování a popisek; nepřenáší původní metadata
přeposlání. Před odesláním kontroluje dostupnost místních souborů a
oprávnění cílového chatu. Samotný náhled nebo neúplné části streamovaného
videa nestačí jako úplný soubor. Chybějící data se touto funkcí nestahují.

### Výchozí sada emoji

Pokud profil nemá uloženou platnou volbu, výchozí sadou je Twemoji (id 2).
Chybějící sada se začne stahovat po přihlášení a po dokončení se použije.
Při chybě se automatické stahování opakuje po dvou minutách, dokud je
příslušná relace dostupná a uživatel nezvolil jinou sadu. Uložené sady
Android, JoyPixels nebo Mac mají přednost. Také výslovná volba Mac (id 0)
se nyní ukládá; nepředstavuje chybějící nastavení. Do dokončení stahování
se zobrazuje přibalená sada.

### Ověření této úpravy

Čtyři oddělené syntetické profily prošly scénářem bez chyb:
`.build-tools/current-visual-test-100-compact-ui-v7-default`,
`current-visual-test-150-compact-ui-v7-mac`,
`current-visual-test-100-compact-ui-v7-android` a
`current-visual-test-100-compact-ui-v7-joypixels` (vše v `.build-tools`).
Protokoly a snímky jsou v jejich podsložce `evidence`.

Kontroly zahrnují rozměry a fonty při měřítku 100 % a 150 %, posun textu
stisknutých tlačítek, profilové akce, heslové pole, dialog odeslání obrázku,
menu a informační panely. Místní dokument a fotografie byly uloženy se
shodnými daty; neexistující zdroj nepoškodil původní cílový soubor.
Skutečné menu hlavního chatu nabídlo Save As, Copy Text a Send a copy
u testovací zprávy s omezeným přeposíláním. Otevřel se i výběr příjemce.

U emoji bylo ověřeno automatické zahájení stahování po vytvoření relace,
zachování každé uložené sady a uložení ruční volby Mac během stahování.
Testovací profily měly síť zablokovanou: nebylo ověřeno dokončení přenosu
sady ze živého účtu ani doručení skutečné zprávy. Žádná zpráva se během
těchto testů neposílala skutečnému příjemci.

Finální Debug sestavení bez dočasného scénáře uspělo:
`.build-tools/compact-ui-build-final-20260831.log`.
Prázdný profil následně dosáhl `launch_finished`; protokol je
`.build-tools/current-visual-test-100-compact-ui-final-smoke/evidence/test_log.txt`.
Tato poslední kontrola ověřovala pouze spuštění, ne celý scénář.
Testovací proces byl poté ukončen; běžný nainstalovaný Telegram se neměnil.

### Kurzor nad tlačítky

Tlačítka, přepínače a položky menu používají běžnou šipku místo ručičky,
také po opětovném povolení ovládacího prvku. Textové odkazy zachovávají
ručičku; odkazy uvnitř popisku zatržítka jsou odlišeny od samotného
zatržítka. Výslovné nastavení ručičky bylo odstraněno také z ovládání
přehrávání příběhů, zastavení nahrávání a tlačítek před spuštěním aplikace.

### Další sjednocení formulářů a menu

Experimentální nastavení má zatržítka vlevo, černé nadpisy, zapuštěný
rámeček vyhledávání, šedé pozadí a vystouplý rámeček formuláře.
Rozvržení nastavení rezervuje místo pro svislý posuvník, takže popisy
nepokračují pod ním. Rámeček dialogu se přičítá k šířce obsahu; nevytváří
nežádoucí vodorovný posuvník.

Dialogy kontaktu, jména a uživatelského jména oddělují písmo popisků od
obsahu. Jméno kontaktu a vlastní vstupy zůstávají Unifontem, popisky mají
Tahomu 8. Pole pro jméno a uživatelské jméno jsou nižší a mají vnitřní
odsazení od 3D rámečku. Výslovné vypnutí classicSettingsStyle v těchto
formulářích zastavuje dědění písma z dříve aktivního panelu nastavení.

Tahoma je také v nadpisech a stavových textech výběru příjemce, na kartách
složek, v datech a časech zpráv a přehrávače a v doplněných potvrzovacích
dialozích. Nadpis blokování se jménem uživatele a vlastní URL při potvrzení
odkazu zachovávají původní písmo. Seznamy zahrnutých a vyloučených chatů
v editoru složky mají bílé pozadí; nadpisy používají Tahomu 8 Bold.

Profilová tlačítka Message / Mute / Call / More mají vystouplý vzhled,
Tahomu 8 Bold a posun textu i ikony při stisku. Nabídka příloh dostala
3D rámeček. Pás ikon sad emoji a nálepek již nemá postranní stmívání.

Neaktivní časový údaj v menu má Tahomu 8, šedý text a bílý reliéf.
Osobní úprava Qt jinak převádí neutrální barvy textu na černou, proto se
pro tuto výslovnou výjimku barví maska již vykreslených znaků. Není potřeba
přepínat globální pravidlo barev textu během vykreslování.

Ověření: oba scénáře v `.build-tools/current-visual-test-100-experimental-ui-v5`
a `.build-tools/current-visual-test-150-experimental-ui-v5` prošly bez chyb.
Snímky skutečných formulářů jsou v `evidence/screenshots`. Kontroly zahrnují
zachování Unifontu ve jménu a polích kontaktu, přepnutí zatržítka i celé
řádky, stabilní konec nastavení, hledání bez výsledků a jeho zrušení,
posun stisknutého profilového tlačítka a přesné barvy neaktivního popisku.
Testy používaly oddělené syntetické profily se zablokovaným připojením;
žádná zpráva se neposílala skutečnému příjemci ani se neměnil živý účet.
Dočasný testovací scénář byl poté odstraněn.

Finální Debug sestavení bez dočasného scénáře uspělo; protokol je
.build-tools/experimental-ui-build-final-20260831.log.

### Menu, média, složky a uchování pozadí

Hlavní menu používá Tahomu 8 Bold pro příkazy a název aplikace,
Tahomu 8 pro emoji status a verzi. Uživatelské jméno se nemění.
Nadpisy seznamů médií jsou tučné; čas, velikost a datum souborů
používají černou Tahomu 8. Samotné URL odkazů mají Tahomu 8,
názvy souborů a obsah odkazovaných stránek si ponechávají původní písmo.

Nastavení složek má odsazený úvod, černé nadpisy, tučné názvy,
menší počty chatů, zatržítko Show Folder Tags vlevo a šedý dolní okraj.
Názvy složek v levém pruhu jsou tučné. Vyhledávací kategorie používají
skutečný QTabBar se stejným stylem jako panel emoji; dostupné jsou
nativní šipky i klávesová navigace. Pole zprávy má zapuštěný rámeček
a vnitřní odsazení, včetně odsazení překryvů od rámečku.

Bezpečnostní informační dialog má Tahomu 8 a vystouplý rámeček.
Výběr nepřekládaných jazyků má zatržítka vlevo a rámečky formuláře
i vyhledávání. Volba jazyků a ukládání zůstávají funkční.

Workspace je prvotní pozadí. Inicializace výchozích hodnot nyní mění
pouze původní výchozí tapetu, takže ani chybějící příznak prvního
nastavení nepřepíše vlastní barvu či obrázek. Stávající migrační klíč
zůstává zachován kvůli kompatibilitě.

Scénáře při 100 % a 150 % v adresářích
`.build-tools/current-visual-test-100-menu-media-language-v4` a
`.build-tools/current-visual-test-150-menu-media-language-v4` prošly bez chyb.
Snímky a protokoly jsou v `evidence`. Ověřeny byly i všechny tři barvy
při chybějícím inicializačním příznaku. V odděleném syntetickém profilu
přežily Workspace i White skutečné ukončení a nové spuštění aplikace;
protokoly jsou v `restart-workspace` a `restart-white` prvního adresáře.
Účet byl syntetický a měl blokované připojení. Testy neměnily živý profil.

Finální Debug sestavení bez dočasného scénáře uspělo; protokol je
`.build-tools/menu-media-language-build-final.log`.

### Další sjednocení klasického rozhraní

Dialogy sdílejí vystouplý rámeček; běžné nadpisy mají Tahomu 8 Bold. Pole s popiskem
nad vstupem oddělují popisek od vnitřního prostoru, aby text ani kurzor
nezasahovaly do rámečku. Placeholdery uvnitř polí zachovávají původní barvy;
samostatné popisky zůstávají černé. Proxy a přihlašovací pole mají kompaktní
výšku. Nastavení, export a Power Saving používají společné klasické styly.
Správa místního úložiště má opět starší seznamové rozhraní.

Časy a data zpráv mají bílou Tahomu 8 s tmavým podkladem pro čitelnost;
datové oddělovače uvádějí i rok. Délka videa v přehledu je bílá Tahoma 8.
Kalendář a výběr měsíce/roku mají klasické rámečky, dny jsou tučné a řádky
výběru měsíce se při rolování nedeformují.

Nativní záložky mají tučnou aktivní položku a kolečko pouze posouvá pás
záložek. Výběr dárků používá stejné záložky. Add Album, Translate To,
Join Channel a příbuzné akce mají malé vystouplé ovládací prvky; volná plocha
vedle nich není tlačítko. Samostatné nabídky a ikony zůstávají mimo tlačítko.

Vyhledávání, pole zprávy a hledání emoji/nálepek/GIFů mají sjednocené levé
odsazení a zapuštěný rámeček. Obsah seznamů rezervuje šířku nativního
posuvníku. Panel emoji je samostatné Qt toolwindow, které zůstává po odjetí
myši otevřené a zavírá se křížkem. Profil má Notifications se zatržítkem
vlevo a QR kód v horní nabídce. Příběhy automaticky nepřecházejí na další.
Kontextové menu reakcí používá kompaktní neprůhledné rozvržení; oddělovače
menu jsou tenké. Elipsy v aplikací vytvářeném textu používají znak ….

Ověření: scénáře při 100 % a 150 % prošly bez chyb v adresářích
`.build-tools/current-visual-test-100-ui-unify-v4` a
`.build-tools/current-visual-test-150-ui-unify-v4`. Snímky skutečných
formulářů a protokoly jsou v `evidence`. Kontroly zahrnují kompaktní proxy,
vytvoření kanálu, výběr země, kalendář, úložiště, bílé datum a čas,
oddělené klikací plochy, odsazení polí a zachování panelu emoji po odjetí
myši, zavření a opětovném otevření. Živý účet, nákup dárku, mazání cache
ani všechny nabídky závislé na serverových datech se nezkoušely.

Testovací scénář byl vrácen přesně na původní prázdnou podobu. Finální
Debug sestavení bez tohoto scénáře uspělo; protokol je
`.build-tools/video-pinned-build-final.log`.

## Doplnění kontroly rozhraní a pádu z 31. srpna 2026

Pád po stisku Speakers and camera (21:40) způsobovalo vykreslování animace
SettingsButton přes zaniklý lokální styl z SectionBuilder. RippleButton nyní
vlastní kopii stylu animace. Zásobník pádu je uložen lokálně pod
`.build-tools/ui-polish-crash/stack.txt`; výpis paměti se nikam neposílal.

Společné vyhledávání ve výběrech kontaktů a nového hovoru má při 100 %
výšku 28 px, bílé pozadí bez nevyplněného pravého pruhu a klasický rámeček.
Místo pro posuvník se rezervuje pouze při přeplnění, vybrané kontakty mohou
dál zabírat více řádků a výška zahrnuje celý poslední řádek. Start New Call
je malé klasické tlačítko s neaktivním okolím; jeho popis i nápověda pro
výběr citace používají Tahomu 8. Checkbox Notifications v bočním profilu
začíná na původní pozici popisku. Nebezpečné akce tvoří v menu skupinu
oddělenou jedním oddělovačem. Tlačítka odmítají ručičku, skutečné odkazy ji
zachovávají. Skok na poslední zprávu má klasický vystouplý rámeček.

Panel emoji má pevnou šířku. Svislé změny velikosti mění i obsah a posuvnou
oblast; průběžné přepočítávání hlavního okna nepřepisuje ručně zvolenou
výšku zobrazeného panelu.

Ověření: 18 kroků prošlo při 100 % i 150 % bez chyb v
`.build-tools/current-visual-test-100-ui-polish-v3` a
`.build-tools/current-visual-test-150-ui-polish-v3` (protokoly a snímky v
`evidence`). Regresní kontrola mění původní styl po konstrukci tlačítka a
ověřuje zachování jeho animace, navíc vykresluje stisk skutečného tlačítka
Speakers and camera. Mikrofon, kameru ani skutečný hovor test nezapíná.
Scénář je vrácen na původní prázdnou podobu. Finální Debug sestavení bez
scénáře uspělo; protokol je `.build-tools/ui-polish-build-final.log`.

## Bílé lišty pod malými akcemi a návrat Battery and animations

Malá tlačítka pod konverzací, připnutými zprávami a přehledem událostí
zůstávají uprostřed bílé lišty s původní výškou velké akce (46 px při
100 %). Stejné pozadí dostaly společné spodní ovládací prvky včetně
otevření původního chatu. Prázdná část lišty nespouští akci.

Translate to má bílé okolí a oddělenou ikonu menu. Tlačítko při stisku
překresluje klasický rámeček a posouvá text i ikonu; uvolnění mimo něj
akci zruší. Zachovává Tahomu 8. Společná obsluha klávesnice už nevolá
akci dvakrát, když nad tlačítkem stojí myš. Battery and animations se
vrátilo na původní běžnou položku v hlavním i pokročilém nastavení
a pomocném seznamu animací; ostatní úpravy nastavení zůstaly zachované.

Ověření: všech 10 kroků prošlo při 100 % i 150 % bez chyb v
`.build-tools/current-visual-test-100-action-bars-v3` a
`.build-tools/current-visual-test-150-action-bars-v3`. Kontroly zahrnují
barvu pozadí, původní výšku spodních lišt, neaktivní okolí tlačítek,
skutečné překreslení po stisku, zapnutí a vypnutí překladu myší,
mezerníkem i Enterem a vrácenou položku Battery and animations.
Dočasný scénář byl vrácen na původní prázdnou podobu.
Finální Debug sestavení bez scénáře uspělo; protokol je
`.build-tools/action-bars-build-final.log`.
