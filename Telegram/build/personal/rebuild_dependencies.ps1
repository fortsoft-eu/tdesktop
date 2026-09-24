#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $LibrariesDir,

    [Parameter(Mandatory = $true)]
    [string] $ThirdPartyDir,

    [Parameter(Mandatory = $true)]
    [string] $PythonExe,

    [ValidateRange(1, 64)]
    [int] $Jobs = 8,

    [switch] $DryRun
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-ContainedPath {
    param(
        [string] $Root,
        [string] $Path
    )

    $rootPath = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $fullPath = [IO.Path]::GetFullPath($Path)
    if (!$fullPath.StartsWith($rootPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is outside the expected directory: $fullPath"
    }

    $current = $fullPath
    while ($current) {
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -LiteralPath $current -Force
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Refusing to rebuild through a redirected path: $current"
            }
        }
        $current = Split-Path -Parent $current
    }
}

function Assert-ExistingFile {
    param([string] $Path)

    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "The existing build is incomplete; required file is missing: $Path"
    }
}

function Add-Command {
    param(
        [string] $Directory,
        [string] $Program,
        [string[]] $Arguments
    )

    $workDir = Join-Path $LibrariesDir $Directory
    Assert-ContainedPath $LibrariesDir $workDir
    if (!(Test-Path -LiteralPath $workDir -PathType Container)) {
        throw "The existing dependency directory is missing: $workDir"
    }
    $steps.Add([pscustomobject]@{
        Directory = $workDir
        Program = $Program
        Arguments = $Arguments
        Destination = $null
    })
}

function Add-Copy {
    param(
        [string] $Source,
        [string] $Destination
    )

    $sourcePath = Join-Path $LibrariesDir $Source
    $destinationPath = Join-Path $LibrariesDir $Destination
    Assert-ContainedPath $LibrariesDir $sourcePath
    Assert-ContainedPath $LibrariesDir $destinationPath
    $steps.Add([pscustomobject]@{
        Directory = Split-Path -Parent $sourcePath
        Program = 'Copy-Item'
        Arguments = @($sourcePath)
        Destination = $destinationPath
    })
}

function Add-CMakeBuild {
    param(
        [string] $Directory,
        [string[]] $Configurations = @('Debug'),
        [switch] $Install
    )

    $buildDir = Join-Path $LibrariesDir $Directory
    $sourceDir = Join-Path $LibrariesDir ($Directory -split '\\')[0]
    $cache = Join-Path $buildDir 'CMakeCache.txt'
    Assert-ExistingFile $cache
    $source = Select-String -LiteralPath $cache -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$'
    if (
        !$source -or
        [IO.Path]::GetFullPath($source.Matches[0].Groups[1].Value) -ne $sourceDir -or
        !(Select-String -LiteralPath $cache -Pattern '^CMAKE_GENERATOR:INTERNAL=Ninja Multi-Config$' -Quiet)
    ) {
        throw "Unexpected dependency build configuration: $cache"
    }

    foreach ($configuration in $Configurations) {
        Assert-ExistingFile (Join-Path $buildDir "build-$configuration.ninja")
        Add-Command $Directory 'cmake' @(
            '--build', '.', '--config', $configuration, '--target', 'clean'
        )
    }
    foreach ($configuration in $Configurations) {
        Add-Command $Directory 'cmake' @(
            '--build', '.', '--config', $configuration, '--parallel', [string] $Jobs
        )
    }
    if ($Install) {
        Add-Command $Directory 'cmake' @(
            '--install', '.', '--config', $Configurations[-1], '--prefix', $localPrefix
        )
    }
}

function Add-MesonBuild {
    param(
        [string] $Directory,
        [switch] $Install
    )

    $buildDir = Join-Path $LibrariesDir $Directory
    Assert-ExistingFile (Join-Path $buildDir 'build.ninja')
    if ($Install) {
        $optionsFile = Join-Path $buildDir 'meson-info\intro-buildoptions.json'
        $options = Get-Content -LiteralPath $optionsFile -Raw | ConvertFrom-Json
        $prefix = @($options | Where-Object name -EQ 'prefix')
        if ($prefix.Count -ne 1 -or [IO.Path]::GetFullPath($prefix[0].value) -ne $localPrefix) {
            throw "Unexpected Meson installation prefix: $buildDir"
        }
    }

    Add-Command $Directory $meson @('compile', '--clean')
    Add-Command $Directory $meson @('compile', '-j', [string] $Jobs)
    if ($Install) {
        Add-Command $Directory $meson @('install', '--no-rebuild')
    }
}

function Add-MakeCommand {
    param(
        [string] $Directory,
        [string[]] $Arguments
    )

    Add-Command $Directory $make $Arguments
}

function Assert-MakePrefix {
    param(
        [string] $File,
        [string] $Name
    )

    $line = Select-String -LiteralPath $File -Pattern "^$Name=(.+)$"
    if (!$line) {
        throw "Missing installation prefix in: $File"
    }
    $prefix = $line.Matches[0].Groups[1].Value
    if ($prefix -match '^/([a-zA-Z])/(.*)$') {
        $prefix = $Matches[1] + ':/' + $Matches[2]
    }
    if ([IO.Path]::GetFullPath($prefix) -ne $localPrefix) {
        throw "Unexpected installation prefix in: $File"
    }
}

function Invoke-RebuildSteps {
    param([object[]] $Plan)

    $stepNumber = 0
    foreach ($step in $Plan) {
        $stepNumber++
        $displayArguments = ($step.Arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
        Write-Host "[$stepNumber/$($Plan.Count)] $($step.Directory)"
        Write-Host "  $($step.Program) $displayArguments $($step.Destination)"
        if ($DryRun) {
            continue
        }

        if ($step.Destination) {
            Copy-Item -LiteralPath $step.Arguments[0] -Destination $step.Destination -Force
            continue
        }

        Push-Location $step.Directory
        try {
            $arguments = $step.Arguments
            & $step.Program @arguments
            if ($LASTEXITCODE -ne 0) {
                throw "Dependency rebuild failed (exit $LASTEXITCODE): $($step.Directory)"
            }
        } finally {
            Pop-Location
        }
    }
}

$LibrariesDir = (Resolve-Path -LiteralPath $LibrariesDir).ProviderPath
$ThirdPartyDir = (Resolve-Path -LiteralPath $ThirdPartyDir).ProviderPath
$localPrefix = Join-Path $LibrariesDir 'local'
$meson = Join-Path $ThirdPartyDir 'python\Scripts\meson.exe'
$jom = Join-Path $ThirdPartyDir 'jom\jom.exe'
$make = Join-Path $ThirdPartyDir 'msys64\usr\bin\make.exe'
$perl = Join-Path $ThirdPartyDir 'msys64\mingw64\bin\perl.exe'
$steps = New-Object 'System.Collections.Generic.List[object]'

foreach ($tool in @(
    $meson,
    $jom,
    $make,
    $perl,
    (Join-Path $ThirdPartyDir 'msys64\mingw64\bin\nasm.exe')
)) {
    Assert-ContainedPath $ThirdPartyDir $tool
    Assert-ExistingFile $tool
}
Assert-ExistingFile $PythonExe
Assert-ExistingFile (Join-Path $PSScriptRoot 'patch_qt_font.py')
Assert-ContainedPath $LibrariesDir $localPrefix

foreach ($relativeFile in @(
    'lzma\C\Util\LzmaLib\LzmaLib.sln',
    'openssl3\makefile',
    'openssl3\configdata.pm',
    'libwebp\Makefile.vc',
    'libvpx\Makefile',
    'libvpx\config.mk',
    'libvpx\vpx.sln',
    'nv-codec-headers\Makefile',
    'ffmpeg\Makefile',
    'ffmpeg\ffbuild\config.mak',
    'breakpad\src\out\Debug_x64\build.ninja',
    'qt_5.15.19\Makefile',
    'qt_5.15.19\qtbase\qmake\Makefile',
    'qt_5.15.19\qtbase\bin\qmake.exe',
    'qt_5.15.19\qtbase\src\gui\text\qfontdatabase.cpp',
    'qt_5.15.19\qtbase\src\gui\painting\qpainter.cpp'
)) {
    $path = Join-Path $LibrariesDir $relativeFile
    Assert-ContainedPath $LibrariesDir $path
    Assert-ExistingFile $path
}

$opensslConfig = Join-Path $LibrariesDir 'openssl3\configdata.pm'
if (
    !(Select-String -LiteralPath $opensslConfig -Pattern '"target"\s*=>\s*"VC-WIN64A"' -Quiet) -or
    !(Select-String -LiteralPath $opensslConfig -Pattern '"build_type"\s*=>\s*"debug"' -Quiet)
) {
    throw 'RebuildAll requires the existing x64 Debug OpenSSL configuration.'
}
$qtMakefile = Join-Path $LibrariesDir 'qt_5.15.19\Makefile'
if (!(Select-String -LiteralPath $qtMakefile -Pattern ' -debug ' -Quiet)) {
    throw 'RebuildAll requires the existing Debug Qt 5.15.19 configuration.'
}
$qtPrefix = Join-Path $LibrariesDir 'Qt-5.15.19'
$qtPrefixPattern = '-prefix "?' + [Regex]::Escape($qtPrefix) + '"?\s'
if (!(Select-String -LiteralPath $qtMakefile -Pattern $qtPrefixPattern -Quiet)) {
    throw 'Unexpected Qt installation prefix in the existing Makefile.'
}
foreach ($outputPath in @(
    $qtPrefix,
    (Join-Path $LibrariesDir 'libvpx\x64\Debug'),
    (Join-Path $LibrariesDir 'libvpx\x64\Release'),
    (Join-Path $LibrariesDir 'libwebp\out\debug-static\x64'),
    (Join-Path $LibrariesDir 'libwebp\out\release-static\x64')
)) {
    Assert-ContainedPath $LibrariesDir $outputPath
}
Assert-MakePrefix (Join-Path $LibrariesDir 'libvpx\config.mk') 'PREFIX'
Assert-MakePrefix (Join-Path $LibrariesDir 'ffmpeg\ffbuild\config.mak') 'prefix'

Add-Command 'lzma\C\Util\LzmaLib' 'msbuild' @(
    'LzmaLib.sln', '/t:Rebuild', "/m:$Jobs",
    '/p:Configuration=Debug', '/p:Platform=x64'
)
Add-CMakeBuild 'zlib'
Add-CMakeBuild 'mozjpeg'

Add-Command 'openssl3' $jom @('clean')
Add-Command 'openssl3' $jom @("-j$Jobs", 'build_libs')
foreach ($file in @('libcrypto.lib', 'libssl.lib', 'ossl_static.pdb')) {
    Add-Copy "openssl3\$file" "openssl3\out.dbg\$file"
}

Add-CMakeBuild 'opus\out' -Configurations @('Debug', 'Release') -Install
Add-CMakeBuild 'rnnoise\out'
Add-MesonBuild 'dav1d\builddir-debug' -Install
Add-Copy 'local\lib\libdav1d.a' 'local\lib\dav1d.lib'
Add-MesonBuild 'openh264\builddir-debug' -Install
Add-Copy 'local\lib\libopenh264.a' 'local\lib\openh264.lib'
Add-CMakeBuild 'libavif' -Install
Add-CMakeBuild 'libde265' -Install

foreach ($configuration in @('debug-static', 'release-static')) {
    $webpArguments = @(
        '/nologo', '/f', 'Makefile.vc', "CFG=$configuration", 'OBJDIR=out', 'RTLIBCFG=static'
    )
    Add-Command 'libwebp' 'nmake' ($webpArguments + 'clean')
    Add-Command 'libwebp' 'nmake' ($webpArguments + 'all')
}
foreach ($library in @('webp', 'webpdemux', 'webpmux')) {
    $webpLibDir = 'libwebp\out\release-static\x64\lib'
    Add-Copy "$webpLibDir\lib$library.lib" "$webpLibDir\$library.lib"
}

Add-CMakeBuild 'libheif' -Install
Add-CMakeBuild 'libjxl' -Install
Add-MakeCommand 'libvpx' @('NO_LAUNCH_DEVENV=1', 'clean')
Add-MakeCommand 'libvpx' @('NO_LAUNCH_DEVENV=1', "-j$Jobs")
foreach ($configuration in @('Debug', 'Release')) {
    Add-Command 'libvpx' 'msbuild' @(
        'vpx.sln', '/t:Rebuild', "/m:$Jobs",
        "/p:Configuration=$configuration", '/p:Platform=x64'
    )
}
Add-MakeCommand 'libvpx' @('NO_LAUNCH_DEVENV=1', "-j$Jobs", 'install')
Add-MesonBuild 'liblcms2\out\Debug'
Add-MakeCommand 'nv-codec-headers' @("PREFIX=$($localPrefix.Replace('\', '/'))", 'install')
Add-MakeCommand 'ffmpeg' @('clean')
Add-MakeCommand 'ffmpeg' @("-j$Jobs")
Add-MakeCommand 'ffmpeg' @("-j$Jobs", 'install')
Add-CMakeBuild 'openal-soft\build'

Add-Command 'breakpad\src' 'ninja' @('-C', 'out/Debug_x64', '-t', 'clean')
Add-Command 'breakpad\src' 'ninja' @(
    '-C', 'out/Debug_x64', '-j', [string] $Jobs,
    'common', 'crash_generation_client', 'exception_handler'
)
Add-CMakeBuild 'tg_angle\out'

Add-Command 'qt_5.15.19' $PythonExe @(
    (Join-Path $PSScriptRoot 'patch_qt_font.py'),
    (Join-Path $LibrariesDir 'qt_5.15.19\qtbase')
)
Add-Command 'qt_5.15.19' $jom @('clean')
Add-Command 'qt_5.15.19\qtbase\qmake' $jom @("-j$Jobs")
Add-Command 'qt_5.15.19' $jom @("-j$Jobs")
Add-Command 'qt_5.15.19' $jom @("-j$Jobs", 'install')
Add-CMakeBuild 'tg_owt\out'
Add-CMakeBuild 'ada\out'
Add-CMakeBuild 'tde2e\out\Debug'

Write-Host 'Rebuilding existing dependencies; source checkouts and preparation cache keys are retained.'
Write-Host 'Opus, WebP and VPX also need their existing optimized variants for this Debug build.'

$savedEnvironment = @{}
foreach ($name in @('Path', 'NUMBER_OF_PROCESSORS')) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
try {
    if (!$DryRun) {
        $extraPaths = @(
            (Join-Path $ThirdPartyDir 'python\Scripts'),
            (Join-Path $ThirdPartyDir 'msys64\mingw64\bin'),
            (Join-Path $ThirdPartyDir 'msys64\usr\bin'),
            (Join-Path $LibrariesDir 'gas-preprocessor')
        )
        $env:Path = $env:Path + ';' + ($extraPaths -join ';')
        $env:NUMBER_OF_PROCESSORS = [string] $Jobs
        foreach ($program in @('cmake', 'ninja', 'msbuild', 'nmake', 'cl', 'lib', 'rc')) {
            if (!(Get-Command $program -ErrorAction SilentlyContinue)) {
                throw "Required tool not found: $program. Use build-local.cmd RebuildAll."
            }
        }
    }

    Invoke-RebuildSteps $steps.ToArray()
} finally {
    if (!$DryRun) {
        foreach ($name in $savedEnvironment.Keys) {
            [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process')
        }
    }
}
