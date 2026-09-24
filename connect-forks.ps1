#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [ValidateNotNullOrEmpty()]
    [string] $Repository,

    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9-]*$')]
    [string] $Owner = 'fortsoft-eu'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (!$PSBoundParameters.ContainsKey('Repository')) {
    $Repository = $PSScriptRoot
}

function Invoke-RepoGit {
    param(
        [string] $Repo,
        [string[]] $GitArgs
    )

    $result = @(& git -C $Repo @GitArgs)
    if ($LASTEXITCODE -ne 0) {
        throw "Git failed in ${Repo}: git $($GitArgs -join ' ')"
    }
    $result
}

function Get-RepoConfig {
    param(
        [string] $Repo,
        [string] $Key,
        [string] $File
    )

    $configArgs = @('config', '--local', '--get-all', $Key)
    if ($File) {
        $configArgs = @('config', '--file', $File, '--get-all', $Key)
    }
    $result = @(& git -C $Repo @configArgs)
    if ($LASTEXITCODE -notin @(0, 1)) {
        throw "Cannot read Git configuration: $Repo / $Key"
    }
    if ($result.Count -gt 1) {
        throw "Multiple values for $Key in $Repo; review them manually."
    }
    if ($result.Count -eq 1) {
        return $result[0]
    }
    return $null
}

$root = (Resolve-Path -LiteralPath $Repository).ProviderPath
$modules = @('lib_base', 'lib_ui', 'lib_lottie', 'lib_qr', 'lib_webview')
$repos = @(
    [pscustomobject]@{
        Name = 'tdesktop'
        Path = $root
        Upstream = 'https://github.com/telegramdesktop/tdesktop.git'
        Fork = "https://github.com/$Owner/tdesktop.git"
    }
)
foreach ($module in $modules) {
    $repos += [pscustomobject]@{
        Name = $module
        Path = Join-Path $root "Telegram/$module"
        Upstream = "https://github.com/desktop-app/$module.git"
        Fork = "https://github.com/$Owner/$module.git"
    }
}

foreach ($repo in $repos) {
    $top = Invoke-RepoGit $repo.Path @('rev-parse', '--show-toplevel')
    if ([IO.Path]::GetFullPath($top) -ne [IO.Path]::GetFullPath($repo.Path)) {
        throw "Not an initialized repository: $($repo.Path)"
    }
    $origin = Get-RepoConfig $repo.Path 'remote.origin.url'
    $upstream = Get-RepoConfig $repo.Path 'remote.upstream.url'
    if ($origin -and $origin -notin @($repo.Upstream, $repo.Fork)) {
        throw "Unexpected origin in $($repo.Name); no configuration changed."
    }
    if ($upstream -and $upstream -ne $repo.Upstream) {
        throw "Unexpected upstream in $($repo.Name); no configuration changed."
    }
    if (!$origin -and !$upstream) {
        throw "Neither origin nor upstream is configured in $($repo.Name)."
    }
    if ($origin -eq $repo.Upstream -and $upstream) {
        throw "Both origin and upstream point to the original $($repo.Name); review manually."
    }
    foreach ($remote in @('origin', 'upstream')) {
        if (Get-RepoConfig $repo.Path "remote.$remote.pushurl") {
            throw "Explicit pushurl in $($repo.Name)/$remote; review manually."
        }
        $mirror = Get-RepoConfig $repo.Path "remote.$remote.mirror"
        if ($mirror -and $mirror -ne 'false') {
            throw "Mirror remote in $($repo.Name)/$remote; review manually."
        }
    }
    if ($repo.Name -ne 'tdesktop') {
        $key = "submodule.Telegram/$($repo.Name)"
        $modulePath = Get-RepoConfig $root "$key.path" '.gitmodules'
        $moduleUrl = Get-RepoConfig $root "$key.url" '.gitmodules'
        if (
            $modulePath -ne "Telegram/$($repo.Name)" -or
            $moduleUrl -notin @($repo.Upstream, $repo.Fork)
        ) {
            throw "Unexpected .gitmodules entry for $($repo.Name)."
        }
    }
}

if (!$PSCmdlet.ShouldProcess($root, "Connect six repositories to $Owner forks and update .gitmodules")) {
    return
}

$backup = Join-Path $root ('.build-tools/fork-connection-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root '.gitmodules') `
    -Destination (Join-Path $backup 'gitmodules.before')
foreach ($repo in $repos) {
    $configPath = Invoke-RepoGit $repo.Path @('rev-parse', '--git-path', 'config')
    if (![IO.Path]::IsPathRooted($configPath)) {
        $configPath = Join-Path $repo.Path $configPath
    }
    Copy-Item -LiteralPath $configPath `
        -Destination (Join-Path $backup "$($repo.Name).config.before")
}
Write-Host "Configuration backup: $backup"

foreach ($repo in $repos) {
    $origin = Get-RepoConfig $repo.Path 'remote.origin.url'
    $upstream = Get-RepoConfig $repo.Path 'remote.upstream.url'
    if ($origin -eq $repo.Upstream) {
        Invoke-RepoGit $repo.Path @('remote', 'rename', 'origin', 'upstream')
        $origin = $null
    } elseif (!$upstream) {
        Invoke-RepoGit $repo.Path @('remote', 'add', 'upstream', $repo.Upstream)
    }
    if (!$origin) {
        Invoke-RepoGit $repo.Path @('remote', 'add', 'origin', $repo.Fork)
    }
    Invoke-RepoGit $repo.Path @('config', '--local', 'remote.pushDefault', 'origin')

    if ($repo.Name -ne 'tdesktop') {
        $urlKey = "submodule.Telegram/$($repo.Name).url"
        Invoke-RepoGit $root @('config', '--file', '.gitmodules', $urlKey, $repo.Fork)
        Invoke-RepoGit $root @('config', '--local', $urlKey, $repo.Fork)
    }
    Write-Host "$($repo.Name): origin = $($repo.Fork)"
}

$gitmodulesPath = Join-Path $root '.gitmodules'
$gitmodulesText = [IO.File]::ReadAllText($gitmodulesPath)
[IO.File]::WriteAllText(
    $gitmodulesPath,
    ($gitmodulesText -replace '\r?\n', "`r`n"),
    [Text.UTF8Encoding]::new($false)
)

Write-Host 'Done. No fetch, checkout, reset, commit, push, or staging was performed.'
Write-Host 'Branches and HEAD commits are unchanged; this does not move your work onto v7.1.3.'
Write-Host 'Review .gitmodules before committing it. Detached submodules still need working branches.'
