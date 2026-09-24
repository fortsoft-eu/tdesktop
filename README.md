# Telegram Desktop 7.1.3 with a Windows 2000-style GUI

## English (United States)

Unofficial fork of [telegramdesktop/tdesktop][upstream], maintained in
[fortsoft-eu/tdesktop][fork]. It is based on Telegram Desktop 7.1.3.
New upstream functionality is incorporated selectively, rather than by
automatically following newer releases.

Windows 2000-style refers to the GUI appearance, not compatibility with
the Windows 2000 operating system.

### Local changes

- Windows 2000-style colors, square corners, 3D borders, controls, and scrollbars.
- Tahoma 8 pt in dialogs and system-installed Unifont for the main text.
- Custom profile panels and layouts for dialogs, emoji, stickers, calls,
  and screen sharing.
- Send a Copy for messages and media, including download preparation and
  photo-aware sending that reuses an existing Telegram photo when possible.
- A tray-menu action to clear the cache of all signed-in accounts.
- Personalized local and account defaults, extended profile details, and
  optional profile-details server integration.

This fork changes behavior as well as appearance. Some [account defaults][defaults]
update settings on Telegram's servers; review them before signing in.
The optional profile-details server is disabled in the
[example configuration][profile-example].

The related library forks are [lib_base][lib-base], [lib_ui][lib-ui],
[lib_lottie][lib-lottie], [lib_qr][lib-qr], and [lib_webview][lib-webview].
Their revisions are recorded as Git submodules in this repository.

### Build

The [local build wrapper][build] targets Windows x64 Debug with Qt 5.15.19.
It expects Visual Studio 2026, MSVC 14.44, and Windows SDK 10.0.26100.0.
Include submodules when cloning the repository. On a fresh build environment,
`build-local.cmd Setup` prepares dependencies, configures, and builds the client.

For an already prepared checkout, run one of these commands from the repository root:

| Command | Scope |
| --- | --- |
| `.\build-local.cmd Build` | Incremental build of Telegram and its project dependencies. |
| `.\build-local.cmd Rebuild` | Clean and rebuild the whole Debug solution. |
| `.\build-local.cmd RebuildAll` | Rebuild the existing external dependencies, including Qt, then the solution. |

`Build` and `Rebuild` also apply the local QtGui font patch and rebuild that
library incrementally if needed; they do not perform a full Qt rebuild.
`Setup` and `Prepare` can download or recreate dependency sources, so do not
use them as a replacement for rebuilding dependencies with local edits.

The executable is `out\Debug\Telegram.exe`. Use `build-local.cmd Open`
to open the solution or `run-local.cmd` to launch with the separate
`.local-data` profile. A separate profile still uses real Telegram accounts.
Keep API credentials, account data, build outputs, and crash dumps private.
The [API configuration template][api-example] contains placeholders only.

### License

GNU GPL version 3 or later with the OpenSSL linking exception.
See [license.txt][license] and the original [copyright notice][legal].
Original copyrights remain applicable; dependencies and bundled assets
retain their own licenses.

### Third-party

The following notices are retained from Telegram Desktop. The exact set of
dependencies depends on the build configuration.

* Qt 6 ([LGPL](http://doc.qt.io/qt-6/lgpl.html)) and Qt 5.15 ([LGPL](http://doc.qt.io/qt-5/lgpl.html)) slightly patched
* OpenSSL 3.2.1 ([Apache License 2.0](https://openssl-library.org/source/license/apache-license-2.0.txt))
* WebRTC ([New BSD License](https://github.com/desktop-app/tg_owt/blob/master/LICENSE))
* zlib ([zlib License](http://www.zlib.net/zlib_license.html))
* LZMA SDK 9.20 ([public domain](http://www.7-zip.org/sdk.html))
* liblzma ([public domain](http://tukaani.org/xz/))
* Google Breakpad ([License](https://chromium.googlesource.com/breakpad/breakpad/+/master/LICENSE))
* Google Crashpad ([Apache License 2.0](https://chromium.googlesource.com/crashpad/crashpad/+/master/LICENSE))
* GYP ([BSD License](https://github.com/bnoordhuis/gyp/blob/master/LICENSE))
* Ninja ([Apache License 2.0](https://github.com/ninja-build/ninja/blob/master/COPYING))
* OpenAL Soft ([LGPL](https://github.com/kcat/openal-soft/blob/master/COPYING))
* Opus codec ([BSD License](http://www.opus-codec.org/license/))
* FFmpeg ([LGPL](https://www.ffmpeg.org/legal.html))
* Guideline Support Library ([MIT License](https://github.com/Microsoft/GSL/blob/master/LICENSE))
* Range-v3 ([Boost License](https://github.com/ericniebler/range-v3/blob/master/LICENSE.txt))
* Open Sans font ([Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0.html))
* Vazirmatn font ([SIL Open Font License 1.1](https://github.com/rastikerdar/vazirmatn/blob/master/OFL.txt))
* Emoji alpha codes ([MIT License](https://github.com/emojione/emojione/blob/master/extras/alpha-codes/LICENSE.md))
* xxHash ([BSD License](https://github.com/Cyan4973/xxHash/blob/dev/LICENSE))
* QR Code generator ([MIT License](https://github.com/nayuki/QR-Code-generator#license))
* CMake ([New BSD License](https://github.com/Kitware/CMake/blob/master/Copyright.txt))
* Hunspell ([LGPL](https://github.com/hunspell/hunspell/blob/master/COPYING.LESSER))
* Ada ([Apache License 2.0](https://github.com/ada-url/ada/blob/main/LICENSE-APACHE))

## Česky

Neoficiální fork [telegramdesktop/tdesktop][upstream] spravovaný v repozitáři
[fortsoft-eu/tdesktop][fork]. Vychází z Telegram Desktop 7.1.3.
Novější funkcionality původního projektu se přebírají cíleně, nikoli
automatickým přechodem na nová vydání.

Windows 2000-style označuje vzhled GUI, nikoli kompatibilitu s operačním
systémem Windows 2000.

### Místní změny

- Barvy, hranaté rohy, 3D rámečky, ovládací prvky a scrollbary ve stylu Windows 2000.
- Tahoma 8 pt v dialozích a systémově nainstalovaný Unifont pro hlavní text.
- Vlastní profilové panely a rozvržení dialogů, emoji, samolepek, hovorů
  a sdílení obrazovky.
- Send a Copy pro zprávy a média včetně přípravy stažením a odesílání fotografií
  jako fotografií; pokud je to možné, použije se již existující fotografie v Telegramu.
- Příkaz v menu tray ikony pro vymazání cache všech přihlášených účtů.
- Osobní výchozí nastavení aplikace a účtů, rozšířené údaje profilů
  a volitelné propojení se serverem pro profilové údaje.

Fork mění nejen vzhled, ale i chování aplikace. Některá [výchozí nastavení účtů][defaults]
mění nastavení na serverech Telegramu; před přihlášením je zkontrolujte.
Volitelný server pro profilové údaje je ve [vzorové konfiguraci][profile-example]
vypnutý.

Související forky knihoven jsou [lib_base][lib-base], [lib_ui][lib-ui],
[lib_lottie][lib-lottie], [lib_qr][lib-qr] a [lib_webview][lib-webview].
Jejich konkrétní revize jsou v tomto repozitáři uložené jako Git submoduly.

### Sestavení

[Lokální sestavovací skript][build] používá Windows x64 Debug a Qt 5.15.19.
Vyžaduje Visual Studio 2026, MSVC 14.44 a Windows SDK 10.0.26100.0.
Repozitář klonujte včetně submodulů. V novém sestavovacím prostředí příkaz
`build-local.cmd Setup` připraví závislosti, nakonfiguruje a sestaví klienta.

V již připraveném pracovním adresáři spusťte jeden z příkazů v kořeni repozitáře:

| Příkaz | Rozsah |
| --- | --- |
| `.\build-local.cmd Build` | Přírůstkové sestavení Telegramu a jeho projektových závislostí. |
| `.\build-local.cmd Rebuild` | Vyčištění a nové sestavení celé Debug solution. |
| `.\build-local.cmd RebuildAll` | Nové sestavení existujících externích závislostí včetně Qt a potom solution. |

`Build` a `Rebuild` zároveň aplikují místní úpravu písem QtGui a v případě
potřeby tuto knihovnu přírůstkově sestaví; neprovádějí kompletní rebuild Qt.
`Setup` a `Prepare` mohou závislosti stahovat nebo znovu vytvořit jejich zdrojové
adresáře, proto jimi nenahrazujte rebuild závislostí obsahujících vlastní změny.

Výsledný program je `out\Debug\Telegram.exe`. Příkaz `build-local.cmd Open`
otevře solution a `run-local.cmd` spustí klienta s odděleným profilem
v `.local-data`. I oddělený profil používá skutečné účty Telegramu.
API údaje, data účtů, výstupy sestavení ani výpisy paměti nezveřejňujte.
[Vzorová konfigurace API][api-example] obsahuje pouze zástupné hodnoty.

### Licence

GNU GPL verze 3 nebo novější s výjimkou pro propojení s OpenSSL.
Viz [license.txt][license] a původní [oznámení o autorských právech][legal].
Původní autorská práva zůstávají zachována; závislosti a přibalené zdroje
mají vlastní licence uvedené v [přehledu třetích stran](#third-party).

[upstream]: https://github.com/telegramdesktop/tdesktop
[fork]: https://github.com/fortsoft-eu/tdesktop
[lib-base]: https://github.com/fortsoft-eu/lib_base
[lib-ui]: https://github.com/fortsoft-eu/lib_ui
[lib-lottie]: https://github.com/fortsoft-eu/lib_lottie
[lib-qr]: https://github.com/fortsoft-eu/lib_qr
[lib-webview]: https://github.com/fortsoft-eu/lib_webview
[build]: build-local.ps1
[defaults]: Telegram/SourceFiles/core/personal_account_defaults.cpp
[profile-example]: profile-details-server.example.json
[api-example]: telegram-api.example.json
[license]: license.txt
[legal]: LEGAL
