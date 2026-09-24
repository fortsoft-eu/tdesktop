#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet(
        'Doctor', 'Prepare', 'Configure', 'Build', 'Rebuild', 'RebuildAll',
        'Open', 'Run', 'Setup'
    )]
    [string] $Action = 'Build',

    [switch] $UseTestApi,

    [ValidateRange(1, 64)]
    [int] $Jobs = 8,

    [switch] $DryRun
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoDir = $PSScriptRoot
$buildRoot = Split-Path -Parent $repoDir
$toolsDir = Join-Path $repoDir '.build-tools'
$outDir = Join-Path $repoDir 'out'
$pythonExe = Join-Path $toolsDir 'python310\tools\python.exe'
$librariesDir = Join-Path $buildRoot 'Libraries\win64'
$thirdPartyDir = Join-Path $buildRoot 'ThirdParty'
$utf8 = New-Object System.Text.UTF8Encoding($false)

function Invoke-Checked {
    param(
        [string] $Program,
        [string[]] $Arguments
    )

    if ($DryRun) {
        $displayArguments = ($Arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
        Write-Host "[$(Get-Location)] $Program $displayArguments"
        return
    }

    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed (exit $LASTEXITCODE): $Program"
    }
}

function Initialize-Compiler {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) {
        throw ('Install Visual Studio 2026 with Desktop development with C++, ' +
            'MSVC 14.44 and Windows SDK 10.0.26100.0.')
    }
    $vswhereArguments = @(
        '-products', '*',
        '-version', '[18.0,19.0)',
        '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
        '-property', 'installationPath'
    )
    $candidates = @(& $vswhere @vswhereArguments)
    $script:visualStudioDir = $null
    $script:visualStudioIde = $null

    foreach ($candidate in $candidates) {
        $ide = Join-Path $candidate 'Common7\IDE\devenv.exe'
        if (Test-Path -LiteralPath $ide) {
            $script:visualStudioIde = $ide
        }

        $findToolsets = @{
            LiteralPath = Join-Path $candidate 'VC\Tools\MSVC'
            Directory = $true
            ErrorAction = 'SilentlyContinue'
        }
        $toolsets = @(Get-ChildItem @findToolsets)
        $v143Path = 'MSBuild\Microsoft\VC\v170\Platforms\x64\PlatformToolsets\v143\Toolset.props'
        $v143 = Join-Path $candidate $v143Path
        if (
            @($toolsets | Where-Object Name -Like '14.44.*').Count -gt 0 -and
            (Test-Path -LiteralPath $v143)
        ) {
            $script:visualStudioDir = $candidate
        }
    }
    if (!$script:visualStudioDir) {
        throw 'Visual Studio 2026 with the MSVC 14.44 toolset was not found.'
    }

    New-Item -ItemType Directory -Path $toolsDir -Force | Out-Null
    $vcvars = Join-Path $script:visualStudioDir 'VC\Auxiliary\Build\vcvarsall.bat'
    $environmentScript = Join-Path $toolsDir 'compiler-environment.cmd'
    $environmentText = @(
        '@echo off'
        "call `"$vcvars`" x64 10.0.26100.0 -vcvars_ver=14.44 >nul"
        'if errorlevel 1 exit /b 1'
        'set'
        ''
    ) -join "`r`n"
    [IO.File]::WriteAllText($environmentScript, $environmentText, $utf8)
    $environmentLines = & $env:ComSpec /d /c ('"' + $environmentScript + '"')
    if ($LASTEXITCODE -ne 0) {
        throw 'Visual Studio could not initialize MSVC 14.44 / SDK 10.0.26100.0.'
    }
    foreach ($line in $environmentLines) {
        if ($line -match '^([^=]+)=(.*)$') {
            Set-Item -LiteralPath ('Env:' + $Matches[1]) -Value $Matches[2]
        }
    }
    $cmakeRoot = Join-Path $script:visualStudioDir 'Common7\IDE\CommonExtensions\Microsoft\CMake'
    $cmakeBin = Join-Path $cmakeRoot 'CMake\bin'
    $ninjaBin = Join-Path $cmakeRoot 'Ninja'
    $pythonDir = Split-Path -Parent $pythonExe
    $compilerPath = (
        $environmentLines |
            Where-Object { $_ -match '^PATH=' } |
            Select-Object -First 1
    ).Substring(5)

    $env:Path = "$pythonDir;$cmakeBin;$ninjaBin;$compilerPath"
    $env:CMAKE_BUILD_PARALLEL_LEVEL = [string] $Jobs
    $env:NUMBER_OF_PROCESSORS = [string] $Jobs
    $env:PYTHONUNBUFFERED = '1'
    $env:PYTHONUTF8 = '1'
    foreach ($program in @('git', 'cmake', 'ninja', 'cl', 'msbuild')) {
        if (!(Get-Command $program -ErrorAction SilentlyContinue)) {
            throw "Required tool not found: $program"
        }
    }
}

function Initialize-Python {
    if (!(Test-Path -LiteralPath $pythonExe)) {
        Write-Host 'Downloading project-local Python 3.10.11 from the Python NuGet package...'
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $archive = Join-Path $toolsDir 'python.3.10.11.zip'
        $download = @{
            UseBasicParsing = $true
            Uri = 'https://api.nuget.org/v3-flatcontainer/python/3.10.11/python.3.10.11.nupkg'
            OutFile = $archive
        }
        Invoke-WebRequest @download

        $extract = @{
            LiteralPath = $archive
            DestinationPath = Join-Path $toolsDir 'python310'
            Force = $true
        }
        Expand-Archive @extract
    }

    Invoke-Checked $pythonExe @(
        '-c',
        'import sys; assert sys.version_info[:2] == (3, 10); print(sys.version.split()[0])'
    )
}

function Assert-Submodules {
    $states = @(& git -C $repoDir submodule status --recursive)
    if ($LASTEXITCODE -ne 0) {
        throw 'Unable to read Git submodules.'
    }
    if (@($states | Where-Object { $_ -match '^[-U]' }).Count -gt 0) {
        throw ('Submodules are missing or conflicted. Resolve conflicts or initialize ' +
            'missing modules with git submodule update --init --recursive; ' +
            'preserve your own module changes.')
    }
    if (@($states | Where-Object { $_ -match '^\+' }).Count -gt 0) {
        Write-Warning 'Some submodules use a different revision. Keeping your local revisions.'
    }
}

function Prepare-Libraries {
    Initialize-Python
    Assert-Submodules

    Write-Host "Dependency sources and build outputs: $librariesDir"
    Write-Host "Dependency tools: $thirdPartyDir"
    Write-Host 'The first run downloads and compiles libraries; later runs reuse successful stages.'
    Invoke-Checked $pythonExe @(
        (Join-Path $repoDir 'Telegram\build\prepare\prepare.py'),
        'skip-release',
        'silent'
    )
}

function Configure-Telegram {
    Initialize-Python
    Assert-Submodules

    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $outDir 'Debug\TelegramForcePortable') -Force | Out-Null
    $apiCache = Join-Path $toolsDir 'telegram-api.cmake'
    $apiFile = Join-Path $repoDir 'telegram-api.local.json'
    if ($UseTestApi -or !(Test-Path -LiteralPath $apiFile)) {
        Write-Warning ('Using bundled Telegram API credentials for local testing. ' +
            'Telegram may limit login with these credentials.')
        $apiSettings = 'set(TDESKTOP_API_TEST ON CACHE BOOL "" FORCE)'
    } else {
        $api = Get-Content -LiteralPath $apiFile -Raw | ConvertFrom-Json
        if (
            [string] $api.api_id -notmatch '^[1-9][0-9]*$' -or
            [string] $api.api_hash -notmatch '^[a-fA-F0-9]{32}$'
        ) {
            throw ('Invalid API credentials in telegram-api.local.json: ' +
                'api_id must be positive and api_hash must have 32 hexadecimal characters.')
        }
        $apiSettings = @(
            'set(TDESKTOP_API_TEST OFF CACHE BOOL "" FORCE)'
            "set(TDESKTOP_API_ID $($api.api_id) CACHE STRING `"`" FORCE)"
            "set(TDESKTOP_API_HASH $($api.api_hash) CACHE STRING `"`" FORCE)"
        ) -join "`r`n"
    }
    [IO.File]::WriteAllText($apiCache, $apiSettings + "`r`n", $utf8)
    $env:CMAKE_GENERATOR = 'Visual Studio 18 2026'
    $env:CMAKE_GENERATOR_INSTANCE = $script:visualStudioDir
    Invoke-Checked $pythonExe @(
        (Join-Path $repoDir 'Telegram\configure.py'),
        '-GVisual Studio 18 2026',
        '-Ax64',
        "-Tv143,host=x64,version=$env:VCToolsVersion",
        '-C', $apiCache,
        '-DCMAKE_BUILD_TYPE=Debug',
        '-DCMAKE_SYSTEM_VERSION=10.0.26100.0',
        '-DCMAKE_CONFIGURATION_TYPES=Debug',
        '-DDESKTOP_APP_DISABLE_AUTOUPDATE=ON',
        '-DDESKTOP_APP_DISABLE_CRASH_REPORTS=ON'
    )
}

function Prepare-PersonalQt {
    $qtBase = Join-Path $librariesDir 'qt_5.15.19\qtbase'
    $qtGui = Join-Path $qtBase 'src\gui'
    Invoke-Checked $pythonExe @(
        (Join-Path $repoDir 'Telegram\build\personal\patch_qt_font.py'),
        $qtBase
    )

    Push-Location $qtGui
    try {
        Invoke-Checked 'nmake' @('/nologo', '/f', 'Makefile.Debug', '..\..\lib\Qt5Guid.lib')
        $builtLibrary = Join-Path $qtBase 'lib\Qt5Guid.lib'
        $installedLibrary = Join-Path $librariesDir 'Qt-5.15.19\lib\Qt5Guid.lib'
        if ($DryRun) {
            Write-Host "Copy updated Qt5Guid.lib and Qt5Guid.pdb to: $librariesDir\Qt-5.15.19\lib"
        } elseif (
            [IO.File]::GetLastWriteTimeUtc($builtLibrary) -ne
            [IO.File]::GetLastWriteTimeUtc($installedLibrary)
        ) {
            Copy-Item -LiteralPath $builtLibrary -Destination $installedLibrary -Force
            $copyPdb = @{
                LiteralPath = Join-Path $qtBase 'lib\Qt5Guid.pdb'
                Destination = Join-Path $librariesDir 'Qt-5.15.19\lib\Qt5Guid.pdb'
                Force = $true
            }
            Copy-Item @copyPdb
        }
    } finally {
        Pop-Location
    }
}

function Assert-TelegramBuild {
    $solutionExists = (
        (Test-Path -LiteralPath (Join-Path $outDir 'Telegram.slnx')) -or
        (Test-Path -LiteralPath (Join-Path $outDir 'Telegram.sln'))
    )
    if (!$solutionExists) {
        throw 'No configured build. First run build-local.cmd Setup (or Prepare followed by Configure).'
    }

    $cache = Join-Path $outDir 'CMakeCache.txt'
    $source = Select-String -LiteralPath $cache -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$'
    if (
        !$source -or
        [IO.Path]::GetFullPath($source.Matches[0].Groups[1].Value) -ne $repoDir -or
        !(Select-String -LiteralPath $cache -Pattern '^CMAKE_GENERATOR:INTERNAL=Visual Studio ' -Quiet)
    ) {
        throw 'The out directory is not a Visual Studio build of this checkout.'
    }

    $directory = Get-Item -LiteralPath $outDir
    if ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw 'Refusing to clean a redirected out directory.'
    }

    if (!$DryRun) {
        $exe = Join-Path $outDir 'Debug\Telegram.exe'
        $running = Get-Process -Name Telegram -ErrorAction SilentlyContinue |
            Where-Object { $_.Path -eq $exe }
        if ($running) {
            throw "Close this checkout's Telegram or its debugger before building: $exe"
        }
    }
}

function Build-Telegram {
    param(
        [switch] $Rebuild,
        [switch] $SkipQtPreparation
    )

    Assert-TelegramBuild
    if (!$SkipQtPreparation) {
        Prepare-PersonalQt
    }

    $cache = Join-Path $outDir 'CMakeCache.txt'
    if (Select-String -LiteralPath $cache -Pattern '^TDESKTOP_API_TEST:BOOL=ON$' -Quiet) {
        Write-Host 'API: bundled credentials for local testing.'
    }

    $buildArguments = @('--build', $outDir, '--config', 'Debug')
    if ($Rebuild) {
        $buildArguments += @('--clean-first', '--target', 'ALL_BUILD')
    } else {
        $buildArguments += @('--target', 'Telegram')
    }
    $buildArguments += @(
        '--parallel', [string] $Jobs,
        '--',
        '/p:UseMultiToolTask=true',
        '/p:EnforceProcessCountAcrossBuilds=true',
        "/p:CL_MPCount=$Jobs"
    )
    Invoke-Checked 'cmake' $buildArguments

    if ($DryRun) {
        return
    }

    $exe = Join-Path $outDir 'Debug\Telegram.exe'
    if (!(Test-Path -LiteralPath $exe)) {
        throw 'Build finished without the expected Telegram.exe.'
    }
    New-Item -ItemType Directory -Path (Join-Path $outDir 'Debug\TelegramForcePortable') -Force | Out-Null
    Write-Host "Built successfully: $exe"
}

function Rebuild-All {
    Assert-TelegramBuild

    $cache = Join-Path $outDir 'CMakeCache.txt'
    $qtCore = Select-String -LiteralPath $cache -Pattern '^Qt5Core_DIR:PATH=(.+)$'
    $expectedQtCore = Join-Path $librariesDir 'Qt-5.15.19\lib\cmake\Qt5Core'
    if (
        !$qtCore -or
        [IO.Path]::GetFullPath($qtCore.Matches[0].Groups[1].Value) -ne $expectedQtCore
    ) {
        throw 'RebuildAll supports the existing x64 Qt 5.15.19 dependency tree only.'
    }

    $rebuildScript = Join-Path $repoDir 'Telegram\build\personal\rebuild_dependencies.ps1'
    $parameters = @{
        LibrariesDir = $librariesDir
        ThirdPartyDir = $thirdPartyDir
        PythonExe = $pythonExe
        Jobs = $Jobs
        DryRun = $DryRun
    }
    & $rebuildScript @parameters
    Build-Telegram -Rebuild -SkipQtPreparation
}

Push-Location $repoDir
try {
    if ($DryRun -and $Action -notin @('Build', 'Rebuild', 'RebuildAll')) {
        throw 'DryRun is available for Build, Rebuild and RebuildAll only.'
    }
    if ($Action -ne 'Run' -and !$DryRun) {
        Initialize-Compiler
    }
    switch ($Action) {
        'Doctor' {
            Write-Host "Visual Studio: $script:visualStudioDir"
            Write-Host "MSVC: $env:VCToolsVersion"
            Write-Host "Windows SDK: $env:WindowsSDKVersion"
            Invoke-Checked 'cmake' @('--version')
            Invoke-Checked 'ninja' @('--version')
            if (Test-Path -LiteralPath $pythonExe) {
                Invoke-Checked $pythonExe @('--version')
            } else {
                Write-Host 'Project Python: not downloaded yet (Prepare will download it).'
            }

            Assert-Submodules
            Write-Host 'Git submodules: OK'
            Write-Host "Libraries: $librariesDir"
            $configured = (
                (Test-Path -LiteralPath (Join-Path $outDir 'Telegram.slnx')) -or
                (Test-Path -LiteralPath (Join-Path $outDir 'Telegram.sln'))
            )
            Write-Host "Configured: $configured"
            Write-Host "Executable: $(Test-Path -LiteralPath (Join-Path $outDir 'Debug\Telegram.exe'))"
        }
        'Prepare' {
            Prepare-Libraries
        }
        'Configure' {
            Configure-Telegram
        }
        'Build' {
            Build-Telegram
        }
        'Rebuild' {
            Build-Telegram -Rebuild
        }
        'RebuildAll' {
            Rebuild-All
        }
        'Setup' {
            Prepare-Libraries
            Configure-Telegram
            Build-Telegram
        }
        'Open' {
            $solution = Join-Path $outDir 'Telegram.slnx'
            if (!(Test-Path -LiteralPath $solution)) {
                $solution = Join-Path $outDir 'Telegram.sln'
            }
            if (!(Test-Path -LiteralPath $solution)) {
                throw 'Run Configure successfully before opening Visual Studio.'
            }
            if (!$script:visualStudioIde) {
                throw 'Visual Studio IDE not installed; use the Build action instead.'
            }

            $env:VCTargetsPath = Join-Path $script:visualStudioDir 'MSBuild\Microsoft\VC\v170\'
            $env:UseMultiToolTask = 'true'
            $env:EnforceProcessCountAcrossBuilds = 'true'
            $env:CL_MPCount = [string] $Jobs
            Start-Process -FilePath $script:visualStudioIde -ArgumentList ('"' + $solution + '"')
        }
        'Run' {
            $exe = Join-Path $outDir 'Debug\Telegram.exe'
            if (!(Test-Path -LiteralPath $exe)) {
                throw 'No executable yet. Run Build first.'
            }
            $cache = Join-Path $outDir 'CMakeCache.txt'
            if (!(Test-Path -LiteralPath $cache)) {
                throw 'Build configuration is missing.'
            }
            if (Select-String -LiteralPath $cache -Pattern '^TDESKTOP_API_TEST:BOOL=ON$' -Quiet) {
                Write-Warning ('TEST API: login can be limited by Telegram. ' +
                    'This still connects to real accounts and real messages.')
            }
            $dataDir = Join-Path $repoDir '.local-data'
            $portableDir = Join-Path $outDir 'Debug\TelegramForcePortable'
            New-Item -ItemType Directory -Path $dataDir -Force | Out-Null
            New-Item -ItemType Directory -Path $portableDir -Force | Out-Null
            $start = @{
                FilePath = $exe
                WorkingDirectory = $dataDir
                ArgumentList = @('-workdir', ('"' + $dataDir + '"'), '-noupdate')
            }
            Start-Process @start
        }
    }

    if ($DryRun) {
        Write-Host 'Dry run finished. No build tools were run and no files were changed.'
    }
} catch {
    Write-Host ("ERROR: " + $_.Exception.Message) -ForegroundColor Red
    exit 1
} finally {
    Pop-Location
}
