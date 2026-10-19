param(
    [Parameter(Mandatory=$true)][string]$Python,
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo 'dist/public-installer' }
$destination = [System.IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $destination) { throw 'Output exists; choose a new directory. No overwrite.' }
Push-Location $repo
try {
    & $Python -m PyInstaller --version
    if ($LASTEXITCODE -ne 0) { throw 'Install PyInstaller 6.22.3 in an isolated build environment first.' }
    & $Python -m PyInstaller --noconfirm --clean --onefile --windowed --noupx `
        --name NGD-Bluetooth-Installer --distpath $destination `
        --workpath (Join-Path $repo '.local/public-installer-build') `
        --specpath (Join-Path $repo '.local/public-installer-spec') `
        --paths (Join-Path $repo 'tools') `
        --add-data "$(Join-Path $repo 'profiles');profiles" `
        (Join-Path $repo 'tools/ngd_public_installer.py')
    if ($LASTEXITCODE -ne 0) { throw 'EXE build failed.' }
    Copy-Item -LiteralPath (Join-Path $repo 'docs/PUBLIC_INSTALLER.md') -Destination (Join-Path $destination 'README-PL-EN.md')
    Copy-Item -LiteralPath (Join-Path $repo 'LICENSE') -Destination $destination
    Copy-Item -LiteralPath (Join-Path $repo 'NOTICE') -Destination $destination
    $runtime = (& $Python -c 'import sys; print(sys.base_prefix)').Trim()
    $licenses = Join-Path $destination 'third-party-licenses'
    New-Item -ItemType Directory -Path $licenses | Out-Null
    Copy-Item -LiteralPath (Join-Path $runtime 'LICENSE.txt') -Destination (Join-Path $licenses 'Python.txt')
    $tkLicense = Join-Path $runtime 'tcl/tk8.6/license.terms'
    if (Test-Path -LiteralPath $tkLicense) { Copy-Item -LiteralPath $tkLicense -Destination (Join-Path $licenses 'Tk.txt') }
    $packager = (& $Python -c 'import importlib.metadata as m; print(m.distribution("pyinstaller").locate_file("pyinstaller-6.22.3.dist-info/licenses"))').Trim()
    if (-not (Test-Path -LiteralPath $packager)) { throw 'PyInstaller licence directory missing.' }
    Copy-Item -LiteralPath $packager -Destination (Join-Path $licenses 'PyInstaller') -Recurse
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $destination 'NGD-Bluetooth-Installer.exe')).Hash.ToLowerInvariant()
    "$hash  NGD-Bluetooth-Installer.exe" | Set-Content -Encoding ascii (Join-Path $destination 'SHA256SUMS.txt')
    Write-Output "PUBLIC_INSTALLER_BUILT $destination; NO_OEM_PAYLOAD; NO_DEVICE_ACCESSED"
} finally { Pop-Location }
