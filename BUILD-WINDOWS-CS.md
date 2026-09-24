# Custom Telegram Desktop for Windows

## English (United States)

This fork uses a Windows 2000-style interface and is based on Telegram
Desktop 7.1.3. The build commands below use the existing local checkout,
not the latest upstream branch. Run them from the repository root in
PowerShell or Command Prompt. The wrapper initializes the compiler itself;
a separate Native Tools Command Prompt is not required.

### Build, rebuild, and full rebuild

| Command | Scope |
| --- | --- |
| `build-local.cmd Build` | Incremental Debug build of Telegram and its project dependencies. Applies the personal QtGui patch and updates QtGui when needed. |
| `build-local.cmd Rebuild` | Cleans the generated Debug solution and rebuilds all its projects, including the in-tree libraries and submodules. Reuses external libraries; QtGui preparation is still incremental. |
| `build-local.cmd RebuildAll` | Cleans and rebuilds the existing external dependency trees, including Qt 5.15.19, then performs `Rebuild`. |

```powershell
.\build-local.cmd Build
.\build-local.cmd Rebuild
.\build-local.cmd RebuildAll
```

These are three alternatives; running all three in sequence is unnecessary.
The application is always built as **x64 Debug**, producing
`out\Debug\Telegram.exe`. `RebuildAll` preserves the existing optimized
variants of Opus, WebP, and VPX required by the dependency recipes, rebuilding
those variants too. It does not build a Release Telegram executable.

`RebuildAll` works with the already configured dependency trees in
`../Libraries/win64`, including local source changes. It does not invoke
`Prepare`, download or clone sources, reset repositories, or delete source
directories. It uses the build tools' clean targets, rebuilds the libraries,
and updates the installed copies. Build tools in `../ThirdParty` are reused,
not reinstalled. Header-only dependencies have nothing to compile.

Before the first clean, the script checks the required build files,
installation prefixes, and dependency paths. A missing or incompatible tree
stops the command; the script does not silently replace it with fresh sources.
A failed build stops subsequent steps. Do not run concurrent builds against
these shared libraries. Close this checkout's Telegram/debugger before an
actual build; the scripts do not terminate processes.

Preview the commands without compiling, cleaning, patching, or copying files:

```powershell
.\build-local.cmd RebuildAll -DryRun
```

`-DryRun` also works with `Build` and `Rebuild`. It checks existing build
metadata, but it cannot prove that compilation or linking will succeed.
Set the parallel job limit with `-Jobs`, for example:

```powershell
.\build-local.cmd RebuildAll -Jobs 4
```

Rebuilds do not delete `.local-data` or portable account directories inside
`out\Debug`. They replace compiled outputs; they do not erase the entire
`out` directory. The first `RebuildAll` command added on September 22, 2026
was checked using Windows PowerShell 5.1 parsing, read-only previews, and
mocked build commands. A real full rebuild was **not** run as part of that
script change.

### Setup, configuration, and launch

For a new build environment, `build-local.cmd Setup` prepares dependencies,
configures the solution, and builds Telegram. Initial preparation can take
hours and tens of gigabytes. `Prepare` uses the upstream preparation script
with `skip-release silent`; completed stages are cached. A changed recipe
can recreate its dependency source directory, so **do not use Prepare or
Setup as a substitute for RebuildAll when preserving local dependency edits**.

| Command | Purpose |
| --- | --- |
| `build-local.cmd Doctor` | Inspect tools, submodules, and existing output. |
| `build-local.cmd Prepare` | Download and build dependencies using the upstream preparation recipes. |
| `build-local.cmd Configure` | Generate the Visual Studio solution in `out`. |
| `build-local.cmd Open` | Open the generated solution in Visual Studio. |
| `build-local.cmd Run` | Start the Debug client with its isolated profile. |

The solution is `out\Telegram.slnx` with this Visual Studio version; the
wrapper also accepts `out\Telegram.sln`. Double-clicking `build-local.cmd`
runs `Build` and keeps the console open. `run-local.cmd` starts the client.
These scripts do not install over the regular Telegram client.

This environment uses Visual Studio 2026, MSVC 14.44 (`v143`), Windows SDK
10.0.26100.0, Visual Studio's CMake and Ninja, project-local Python 3.10.11
in `.build-tools/python310`, and Qt 5.15.19. Build Tools supply the compiler;
Community supplies the IDE when installed. Do not retarget to `v145`.
Dependencies are in `../Libraries/win64`, tools in `../ThirdParty`.
Moving the checkout and libraries requires reconfiguration.

### API credentials and profile data

`Configure` reads `telegram-api.local.json` when present. Otherwise, or with
`-UseTestApi`, it uses the bundled API credentials, which may limit login.
To use personal credentials, obtain them at
[Telegram API development tools](https://my.telegram.org/apps), copy
`telegram-api.example.json` to `telegram-api.local.json`, fill in `api_id`
and `api_hash`, then run `Configure` and `Build`.

Credentials are not printed as command-line arguments. They are still
present in local build files and the binary. Do not publish the local JSON,
`out`, `.build-tools`, or account data. See the
[Telegram API documentation](https://core.telegram.org/api/obtaining_api_id).

`Run` uses `.local-data`, separate from the installed client's profile and
from build outputs. Back it up to preserve the session. Do not copy the
installed client's `tdata`; sign in separately. Launching the EXE directly
or from Visual Studio uses the portable profile under
`out\Debug\TelegramForcePortable` instead.

An isolated profile still connects to real Telegram accounts and messages.
Actions can affect other devices. Automatic updates and crash reporting are
disabled in this configuration; security updates must be incorporated
deliberately and rebuilt.

### Interface and source layout

The Qt `Windows` style renders square buttons, input frames, checkboxes,
radio buttons, sliders, and tabs. Avatars and message/media masks are square;
the underlying photos, videos, emoji, and stickers are not modified.
The system-installed Unifont is used at 16 physical pixels without smoothing.
Other fallback fonts, including mathematical symbols, retain their own
rendering. Settings and dialog controls use non-antialiased Tahoma 8 pt.
Neutral text is black; colored text retains its color. Form controls use
RGB 212, 208, 200. This is an interface style, not Windows 2000 emulation.

The account defaults, scope of server-side settings, and detailed historical
UI checks are documented in [PERSONAL-DEFAULTS.md](PERSONAL-DEFAULTS.md).
Selected sticker-set controls were restored from `v7.0.9`, and the compact
right-side user/group profile header from `v6.2.4`; the client base remains
7.1.3. Newer API support and selected fixes were retained. Broadcast channels,
forum topics, Saved Messages, and standalone profiles do not all use the
compact right-column layout.

Backspace in the main search field deletes text and keeps focus after the
last character. Escape and Tab keep their existing behavior. Each newly
opened per-chat export enables all media categories with a 4000 MB limit.
User changes still apply to that individual export.

| Area | Source location |
| --- | --- |
| Chat list | `Telegram/SourceFiles/dialogs/` |
| Messages and compose field | `Telegram/SourceFiles/history/` |
| Main window and navigation | `Telegram/SourceFiles/window/` |
| Settings | `Telegram/SourceFiles/settings/` |
| User, group, and channel information | `Telegram/SourceFiles/info/` |
| Shared UI controls | `Telegram/SourceFiles/ui/`, `Telegram/lib_ui/` |
| Geometry and styling | `.style` files in the relevant component |
| Default strings | `Telegram/Resources/langs/lang.strings` |

Do not edit generated files in `out/gen`. Qt font changes are applied by
`Telegram/build/personal/patch_qt_font.py`. The upstream starting revision
was `087b18b89194ab474526ca61b7992f32f52c983b`, obtained August 30, 2026.
The working branch is `master`. New upstream versions are not merged
automatically; only deliberately selected functionality is incorporated.

### Local crash diagnostics and historical verification

`run-local-diagnostic.cmd` starts this Debug client with `.local-data` and
a local unhandled-exception monitor. It does not change system WER settings
or stop an installed Telegram instance. This checkout must be closed first.
Crash records are saved to `.local-data/crash-diagnostics`. If the EXE has
not changed, the corresponding EXE and PDB are copied after the process exits.
Do not rebuild during a diagnostic session. Full dumps can occupy gigabytes
and contain messages and login keys. They are never uploaded automatically;
do not publish them.

Historical checks on August 30-31, 2026 recorded a successful initial Debug
build, an approximately 116-second single-file rebuild, and an approximately
10-second no-change build. Later UI tests covered native tabs, compact
profiles, search behavior, and controls at 100% and 150% scaling. Their logs
are under `.build-tools`, with details and limitations in the Czech record
below and `PERSONAL-DEFAULTS.md`. These are historical results, not a fresh
verification of every subsequent change.

The August 31 crash dump recorded `0xc0000005`, with a stack through
`QObjectPrivate::ConnectionData::removeConnection()` and
`ConnectionPointer::reset()` during `MTP::details::SessionPrivate`
destruction. That dump did not identify who corrupted the connection pointer
and did not establish a completed fix. The original artifacts were retained
under `.build-tools/morning-crash-20260831/`.

## Čeština — Vlastní Telegram Desktop pro Windows

### Vzhled Windows 2000 a ostré rohy

Tato osobní verze používá klasický styl Qt `Windows` a hranaté
ovládací prvky. Běžná tlačítka, rámečky vstupních polí a zaškrtávací políčka
Telegramu jsou napojena na vykreslování tohoto stylu. Nejde o emulaci celých
Windows 2000: rozložení aplikace, ikony, systémové dialogy a některé speciální
prvky Telegramu zůstávají vlastní nebo závislé na současném systému.

- Profilovky a jejich výchozí barevné náhrady jsou čtvercové. Barevné náhrady
  mají jednolitou výplň bez svislého přechodu, včetně Uložených zpráv a odpovědí.
- Zprávy nemají výstupky. Zaoblené masky obrázků, videí, panelů a tlačítek jsou
  odstraněné i v cestě pro vykreslování přes grafickou kartu.
- Samotné soubory fotografií, videí a samolepek se nemění. Pokud je zaoblení
  součástí původního obrázku nebo kresby samolepky, zůstane v jejím obsahu.
- Výchozí font je **Unifont nainstalovaný v systému**, pevně 16 fyzických
  pixelů bez vyhlazování. Žádný Unifont se nepřibaluje ani neinstaluje.
  Ostatní náhradní fonty se vyhlazují a nemají vynucenou velikost 16 px.
  Experimentální nastavení používají systémovou Tahomu ve velikosti
  běžných dialogů Windows 2000 (8 pt, přibližně 11 px při 96 DPI).
- Neutrální text je černý, barevné texty zůstávají barevné. Tlačítka,
  experimentální nastavení a panel Emoji / Stickers / GIFs mají pozadí
  RGB 212, 208, 200. Nové jednobarevné pozadí chatu lze změnit v nastavení.
- Experimentální volba `FreeType font engine` je výchozím nastavením
  zapnutá už před vytvořením rozhraní Qt. Uložená volba uživatele má
  nadále přednost, takže pozdější ruční vypnutí se při restartu nepřepíše.

Nynější výchozí hodnoty, úpravy rozhraní, rozsah změn účtu na serveru
a postup pro čistý profil jsou v [PERSONAL-DEFAULTS.md](PERSONAL-DEFAULTS.md).
Neprobíhá globální přepis všech barev motivu. Výjimky jsou výše uvedené
části, pro které uživatel výslovně zadal nové barvy.

Zásady pro rohy jsou v `Telegram/lib_ui/ui/style/style_radius.h`, napojení
klasických ovládacích prvků v `style_classic.cpp` a výchozí font v
`ui/style/style_core_font.cpp`. Pevná fyzická velikost je v řešení fontu a v osobní úpravě QtGui.
Změny jsou také uvnitř submodulu `Telegram/lib_ui`; při zálohování vlastních
úprav je nutné uchovat jeho pracovní soubory včetně nových pomocných souborů.

Vlastní údaje ze souboru `telegram-api.local.json` byly načteny příkazem
`Configure`. Pro tuto konfiguraci je testovací API vypnuté. Soubor s údaji
ani adresáře `out`, `.build-tools` a `.local-data` nepatří do Gitu.

Pixelová kontrola používá originální řádek označený „12“ ze Screenshot_289.png,
výřez x=28, y=183, 400 × 17. Po oříznutí pouze bílých okrajů je vzorek
398 × 16 pixelů. Kontroluje se přímý text Qt i textový vykreslovač Telegramu,
bez přepočítání velikosti snímku nebo prahování barev. Aktuální výsledky
budou uvedeny v PERSONAL-DEFAULTS.md po dokončení nové kontroly.

Předchozí kontroly hranatých masek profilovek, médií a zpráv zůstávají
v `.build-tools/system-unifont-evidence`; jejich tehdejší velikost 12 fyzických
pixelů již není aktuálním nastavením písma.

Zdroje jsou z [oficiálního repozitáře](https://github.com/telegramdesktop/tdesktop),
větev `dev`, staženo 30. 8. 2026. Výchozí revize je
`087b18b89194ab474526ca61b7992f32f52c983b` (verze 7.1.3).
Vlastní úpravy používají větev `master`.
Místní úpravy rozhraní jsou popsané níže; základem aplikace zůstává verze 7.1.3.

### Návrat vybraných prvků rozhraní z 7.0.9

Referencí je přímo značka `v7.0.9` v tomto repozitáři. Nejde o návrat celého
klienta na starou verzi.

- Dialog sady samolepek má původní tlačítka Přidat/Zrušit, původní nabídku
  a sdílení odkazem přes schránku. Platí také pro sady masek a emoji.
  Zmizelo velké zaoblené tlačítko s počtem položek a nové spodní tlačítko
  Odstranit/Upravit; příslušné původní akce zůstávají v nabídce.
- Záhlaví profilu opět používá statický text místo běžícího dlouhého jména.
  Tlačítka a řádek hudby používají původní odezvu na kliknutí, bez později
  přidaného zvýraznění na barevném pozadí. Společné prvky se mění i v profilu
  otevřeném mimo pravý panel.
- Velké rozbalovací záhlaví bylo už součástí 7.0.9. Dodatečný požadavek níže
  jej nahrazuje pouze v pravém detailu uživatele a skupiny verzí z 6.2.4.
- Zachovány jsou novější opravy pádů při řazení samolepek, správné určení
  samolepky ve smíšené sadě, opravy vykreslování a čitelnosti světlého pozadí,
  současné API a oddělení Uložených zpráv od vlastního profilu.
- Při změně stavu sady se obnovuje výchozí styl dialogu, aby v něm po
  nabídce Premium nezůstalo rozložení pro prémiové tlačítko.

Úpravy jsou v `Telegram/SourceFiles/boxes/sticker_set_box.cpp` a v souborech
`info_profile_top_bar.*`, `info_profile_top_bar_action_button.*` a
`info_profile_music_button.cpp` pod `Telegram/SourceFiles/info/profile/`.
Protokol nového sestavení: `.build-tools/gui-709-build.log`.

Debug sestavení s těmito úpravami úspěšně dokončeno 30. 8. 2026 ve 22:55.
Klient byl spuštěn přes `Run` s odděleným profilem. Již přihlášená relace se
obnovila a byl pozorován pravý panel skupiny se sdílenými médii a členy.
Kontrola zdrojů potvrdila shodu ovládání sady s 7.0.9 (kromě obnovení výchozího
stylu při změně stavu) i zachování současného kódu mřížky samolepek beze změn.
Ruční test dialogu a dalších typů profilů není dokončený: během kontroly
začal uživatel okno ovládat, proto automatické klikání nepokračovalo.

Po přihlášení ověřte otevření nenainstalované i nainstalované sady samolepek
a profil uživatele, skupiny a kanálu v pravém panelu. Zkontrolujte také dlouhé
jméno, posouvání panelu, sdílená média a návrat zpět. Otevření sady ji samo
neinstaluje; u tlačítek mazání a odesílání počítejte se skutečným účtem.

### Pravý detail uživatele a skupiny podle 6.2.4

Pouze detail v pravém sloupci používá kompaktní záhlaví převzaté z `v6.2.4`:
malou čtvercovou profilovku vlevo, jméno a stav vedle ní a samostatnou horní
lištu. Údaje mají původní odsazení s informační ikonou a řádek oznámení.
Odkazy na sdílená média a seznam členů zůstávají dostupné. Nabídka a volání
jsou dostupné v horní liště. Současné údaje a bezpečnostní upozornění zůstávají.

Podmínka se vztahuje na uživatele, běžné skupiny a superskupiny. Nevztahuje se
na vysílací kanály, témata fóra, Uložené zprávy ani profil otevřený mimo pravý
sloupec. Při přesunutí panelu mezi sloupci se rozložení znovu vytvoří pro cílové
umístění. Dialog samolepek se touto změnou neupravuje.

Samostatný převzatý kód je v `info_profile_cover_classic.*`; zapojení je
v `info_profile_inner_widget.*`, `info_wrap_widget.cpp` a odpovídajících
částech `info_profile_actions.*`. Základ klienta zůstává 7.1.3.

Kontrola za běhu 31. 8. 2026 prošla bez selhání. Ověřila kompaktní záhlaví
u uživatele a skupiny v šířkách 392 a 324 bodů a přechod mezi pravým panelem
a samostatným profilem. Snímky byly pořízeny před dokončením načítání fotografií;
nejde o úplnou vizuální kontrolu všech načtených dat. Protokol je
`.build-tools/profile-search-evidence/latest-test-log.txt`. Test nepoužil
odesílání zpráv ani změnu účtu a jeho dočasná kopie přihlášení byla odstraněna.

### Backspace ve vyhledávání

Backspace v levém horním vyhledávání maže text. Po vymazání posledního znaku
i při dalším stisknutí v prázdném poli zůstává fokus ve vyhledávání.
Test ověřil smazání posledního znaku, dalších 20 stisků, Ctrl-Backspace a následné psaní.
Klávesy Escape a Tab zachovávají své dosavadní chování. Úprava je v
`Telegram/SourceFiles/dialogs/dialogs_widget.cpp`.

### Ověřený stav k 30. 8. 2026

- Všechny zdroje a submoduly jsou stažené; všech 32 kroků přípravy knihoven prošlo.
- Úspěšně vytvořen `out/Telegram.slnx` a sestaven `out/Debug/Telegram.exe`, verze 7.1.3.0, x64 Debug.
- Ověřen opakovaný překlad jediného C++ souboru a spojení programu: přibližně 116 sekund.
  Test změnil pouze časový údaj souboru; jeho obsah zůstal shodný.
- Sestavení bez změn prošlo za přibližně 10 sekund a EXE se znovu nevytvářelo.
- Po úpravě GUI bylo ověřeno spuštění s již přihlášeným profilem a zobrazení
  pravého panelu skupiny; rozsah kontroly je popsaný výše. Nové přihlášení
  nebylo automatizováno.

Podrobné místní protokoly jsou v `.build-tools/setup.log`,
`.build-tools/incremental.log` a `.build-tools/no-change.log`.

### Aktuální výsledné sestavení

Debug sestavení z 31. 8. 2026 obsahuje skutečný `QTabBar` pro panel
Emoji / Stickers / GIFs ve stylu Qt `Windows`. Kontrola samostatného prvku
při měřítku 100 % a 150 % prošla, včetně pixelového porovnání se standardním
Qt prvkem. Dočasný test byl odstraněn a následné sestavení uspělo.
Protokol: `.build-tools/native-tabbar-build-final.log`.
Podrobný rozsah ověření i nevyřešené výsledky širších testů nastavení jsou
v `PERSONAL-DEFAULTS.md`. Historické výsledky výše se vztahují k tehdejším úpravám.

### Sestavení a spuštění

Příkazy spouštějte z této složky. Nemusíte ručně otevírat speciální vývojářský
terminál: skript načte správný kompilátor sám.

#### Build, rebuild a kompletní rebuild

| Příkaz | Rozsah |
| --- | --- |
| `.\build-local.cmd Build` | Přírůstkové sestavení Telegramu a jeho projektových závislostí; příprava osobní úpravy QtGui. |
| `.\build-local.cmd Rebuild` | Vyčištění Debug solution a nové sestavení všech jejích projektů včetně knihoven a submodulů uvnitř repozitáře. Externí knihovny se použijí hotové; příprava QtGui zůstává přírůstková. |
| `.\build-local.cmd RebuildAll` | Vyčištění a nové sestavení stávajících externích knihoven včetně Qt 5.15.19, potom `Rebuild`. |

Jde o tři alternativy, nikoli příkazy k postupnému spuštění. Výsledek je vždy
`out\Debug\Telegram.exe`, x64 Debug. Závislosti Opus, WebP a VPX potřebují
podle současných receptů také optimalizované varianty; ty se znovu sestaví
rovněž. Release varianta samotného Telegramu se nesestavuje.

`RebuildAll` používá současné zdroje v `../Libraries/win64` včetně místních
úprav. Nespouští `Prepare`, nestahuje ani neklonuje zdroje, nemaže jejich
adresáře a nemění revize. Před prvním čištěním kontroluje konfigurace, cesty
a instalační umístění. Při chybě zastaví další kroky. Hotové nástroje
v `../ThirdParty` nepřeinstalovává; hlavičkové knihovny není co kompilovat.

Používají se čisticí cíle sestavovacích nástrojů, nikoli smazání celého `out`.
Složka `.local-data` ani přenosné profily v `out\Debug` se nemažou. Před
skutečným sestavením zavřete tento Telegram a jeho debugger. Skript procesy
neukončuje. Nad stejnými knihovnami nespouštějte souběžná sestavení.

Pouhý výpis kroků bez čištění, kompilace, úprav a kopírování souborů:

```powershell
.\build-local.cmd RebuildAll -DryRun
```

`-DryRun` funguje také pro `Build` a `Rebuild`; kontroluje metadata, nikoli
úspěšnost kompilace. Počet souběžných úloh lze omezit například příkazem
`.\build-local.cmd RebuildAll -Jobs 4`.

Úprava skriptů z 22. 9. 2026 byla ověřena parserem Windows PowerShellu 5.1,
náhledy příkazů a simulovanými sestavovacími nástroji. Skutečný kompletní
rebuild během této úpravy spuštěn nebyl.

#### Počáteční příprava a běžné spuštění

```powershell
.\build-local.cmd Setup
```

`Setup` připraví knihovny, nakonfiguruje projekt a sestaví 64bitový Debug klient.
První příprava může trvat hodiny a spotřebovat desítky GB. Příští sestavení
využije hotové knihovny a přeloží jen změněné části.

`Prepare` může při změně receptu znovu vytvořit zdrojový adresář knihovny.
Pro rebuild se zachováním místních úprav externích knihoven proto nepoužívejte
`Setup` ani `Prepare` jako náhradu `RebuildAll`.

Po úpravě funkce, metody nebo vzhledu už stačí:

```powershell
.\build-local.cmd Build
.\build-local.cmd Run
```

Výsledný program je `out\Debug\Telegram.exe`. Pro běžné spouštění vlastní verze
používejte `Run`, aby měla stálou oddělenou složku s daty.
Dvojklik na `build-local.cmd` spustí sestavení a nechá okno s výsledkem otevřené.
Dvojklik na `run-local.cmd` spustí vlastní klient s odděleným profilem.

Další příkazy:

| Příkaz | Co udělá |
| --- | --- |
| `.\build-local.cmd Doctor` | Zkontroluje nástroje, submoduly a přítomnost výsledku. |
| `.\build-local.cmd Prepare` | Stáhne a sestaví knihovny podle oficiálního builderu. |
| `.\build-local.cmd Configure` | Vygeneruje projekt Visual Studia v `out`. |
| `.\build-local.cmd Open` | Otevře vygenerovaný projekt ve Visual Studiu. |
| `.\build-local.cmd Build -Jobs 4` | Sestaví klienta s menším počtem souběžných úloh. |

Skript nic neinstaluje do vaší běžné instalace Telegramu. Automatické aktualizace
a odesílání hlášení o pádech jsou ve vlastní konfiguraci vypnuté.
Bez automatických aktualizací se samy nenainstalují ani bezpečnostní opravy;
vlastní verzi je potřeba průběžně aktualizovat ze zdrojů a znovu sestavit.

### Přihlášení při testování

Spustíte klienta a přihlásíte se obvyklým způsobem svým účtem. Aktuální
konfigurace již používá vaše vlastní API údaje z `telegram-api.local.json`.
Hash se zadává do místní konfigurace pro sestavení, nikoli při přihlašování
v okně Telegramu.

Oddělený profil neznamená testovací server: připojujete se ke skutečnému účtu.
Odeslané zprávy a smazání se projeví i na ostatních zařízeních. Ověření vzhledu
proto začněte například v Uložených zprávách.

`Run` ukládá data do `.local-data`, mimo oficiální profil a mimo sestavovací složku
`out`. Složka je vyloučená z Gitu. Nekopírujte sem `tdata` své běžné instalace;
přihlaste se samostatně. Zálohujte `.local-data`, pokud chcete uchovat tuto relaci.
Spuštění EXE přímo nebo z Visual Studia používá jiný přenosný profil
`out\Debug\TelegramForcePortable`; pro stálý profil používejte `Run`.

Pokud se projeví omezení přihlášení nebo budete chtít dlouhodobě používat vlastní
identitu aplikace, lze později jednorázově vyplnit vlastní údaje:

1. Na [my.telegram.org/apps](https://my.telegram.org/apps) otevřete API development tools a vytvořte aplikaci.
2. Zkopírujte `telegram-api.example.json` na `telegram-api.local.json`.
3. Do kopie vyplňte přidělené `api_id` a `api_hash`.
4. Spusťte `Configure`, potom `Build` a `Run`.

Existující místní JSON má při konfiguraci přednost. Údaje se neposílají do Gitu
ani se nevypisují jako parametry příkazu; zůstávají ale v místních sestavovacích
souborech a ve výsledném klientovi. Nesdílejte celý obsah `out` a `.build-tools`.
Podrobnosti jsou v [dokumentaci Telegram API](https://core.telegram.org/api/obtaining_api_id).

### Kde měnit GUI

| Oblast | Zdrojové soubory |
| --- | --- |
| Seznam chatů | `Telegram/SourceFiles/dialogs/` |
| Zprávy, konverzace a pole pro psaní | `Telegram/SourceFiles/history/` |
| Hlavní okno a navigace | `Telegram/SourceFiles/window/` |
| Nastavení | `Telegram/SourceFiles/settings/` |
| Informace o uživateli, skupině nebo kanálu | `Telegram/SourceFiles/info/` |
| Sdílené prvky rozhraní | `Telegram/SourceFiles/ui/`, `Telegram/lib_ui/` |
| Rozměry, mezery a styly | soubory `.style` v příslušné oblasti |
| Výchozí texty | `Telegram/Resources/langs/lang.strings` |

Chování se mění v C++ souborech `.cpp` / `.h`. Rozměry upravujte v `.style`,
aby fungovalo zvětšení rozhraní. Soubory v `out/gen` jsou generované a neupravují se.
`Telegram/lib_ui` je samostatný Git submodul: změny uložte commitem uvnitř něj
a následně uložte nový odkaz na submodul také v hlavním repozitáři.

Pro první úpravu změňte jednu drobnou věc, sestavte a ověřte ji. Před dalším
sestavením zavřete vlastní testovací klient, aby Windows neblokoval jeho EXE.
Oficiální Telegram nemusíte zavírat.

### Nástroje a umístění

- Visual Studio 2026, MSVC 14.44, Windows SDK 10.0.26100.0.
- CMake a Ninja z instalace Visual Studia.
- Python 3.10.11 pouze v `.build-tools/python310`; systémový Python se nemění.
- Qt 5.15.19, tedy výchozí varianta oficiálního Windows builderu.
- Závislosti podle upstreamu v `../Libraries/win64` a nástroje v `../ThirdParty`.
- Debug sestavení, bez zdlouhavé optimalizace celé aplikace pro Release.

Na tomto počítači mají Build Tools úplnou podporu staršího toolsetu `v143`;
skript je používá pro kompilaci a Community IDE pro editaci. Nepřepínejte projekt
na nejnovější toolset `v145`. Tato složka a sousední knihovny tvoří jeden celek;
po přesunu je potřeba nová konfigurace a případně příprava knihoven.

Příprava knihoven přebírá oficiální `Telegram/build/prepare/prepare.py` s parametry
`skip-release silent`. Úspěšné kroky má v cache; po změně receptu automaticky
znovu vytvoří dotčenou knihovnu. Složky `Libraries` a `ThirdParty` proto nepoužívejte
pro jiné vlastní soubory.

Při chybě sítě zopakujte `Prepare`. Pro záznam výstupu lze použít:

```powershell
.\build-local.cmd Prepare *> .build-tools\prepare-manual.log
```

Chyba „Neither Qt6 nor Qt5 is found“ před dokončením přípravy znamená, že zatím
nebyly sestaveny knihovny. Samotný úspěšný `Doctor` ještě nepotvrzuje sestavení
celého Telegramu. Za hotový build považujte až úspěšný `Build` a existující EXE.

[Oficiální návod pro Windows](https://github.com/telegramdesktop/tdesktop/blob/dev/docs/building-win.md).

### Zachování vlastních změn

Pracovní větev je `master`. Novější oficiální verze se automaticky neslučují;
přebírají se pouze záměrně vybrané funkce. Rebuild zachovává současné revize
a místní úpravy. Případné cílené převzetí může vyžadovat změnu knihoven nebo
vyřešení konfliktů a je samostatnou operací, nikoli součástí sestavení.

Nepoužívejte `git reset --hard` nebo `git clean` k běžnému sestavení: mohly by
zahodit vaše úpravy či místní soubory.

### Další úpravy GUI z 31. 8. 2026

Nastavení a názvy složek používají nevyhlazenou Tahomu 8 pt. Jezdce hodnot
jsou standardní Qt Windows, menu prohlížeče médií světlá a nativní záhlaví
ponechává systémové motivy a barvy. Add / Send / Cancel a dvojice tlačítek
u samolepek mají sjednocené rozměry; profilové akce mají černý text uprostřed.
Export jednotlivého chatu začíná při každém novém otevření se všemi médii
a limitem 4000 MB. Podrobnosti a omezení ověření jsou v `PERSONAL-DEFAULTS.md`.

### Lokální diagnostika pádu po delším běhu

`run-local-diagnostic.cmd` spustí tento Debug klient se stejným odděleným
profilem `.local-data` a místním sledováním neošetřených výjimek. Nezastavuje
nainstalovaný Telegram a nemění systémové nastavení WER. Tento checkout musí
být před spuštěním zavřený; skript běžící klient neukončuje.

Při neošetřeném pádu uloží úplný výpis paměti a záznam výjimky pod
`.local-data/crash-diagnostics/`. Po ukončení procesu přidá odpovídající EXE
a PDB, pokud se mezitím EXE na disku nezměnilo. Během diagnostického běhu
klienta nepřekompilovávejte. Záznam může zabrat několik GB.

**Dump může obsahovat zprávy, soubory i přihlašovací klíče. Je určený jen
pro místní rozbor; nic se automaticky nenahrává a celá složka je ignorovaná
Gitem. Nezveřejňujte ji.** Běžné `run-local.cmd` zůstává beze změny.

Záznam pádu z 31. 8. 2026 11:02:56 a dump z 11:02:58 potvrdily výjimku
`0xc0000005` při zápisu na adresu `0xd59ead40`. Odpovídající zásobník vede
přes `QObjectPrivate::ConnectionData::removeConnection()` a
`ConnectionPointer::reset()` z destruktoru `MTP::details::SessionPrivate`.
Původní dump neobsahuje paměť dotčeného objektu spojení; neprokazuje, kdo
ukazatel poškodil. Samotná příčina tedy zatím není opravená. Původní EXE,
PDB a dump jsou zachované v `.build-tools/morning-crash-20260831/`.
