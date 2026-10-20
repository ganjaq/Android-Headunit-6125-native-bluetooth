param(
    [Parameter(Mandatory=$true)][string]$Python,
    [string]$OutputDirectory = '',
    [string]$AdbDirectory = ''
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo 'dist/public-installer' }
$destination = [System.IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $destination) { throw 'Output exists; choose a new directory. No overwrite.' }
Push-Location $repo
try {
    if (-not $AdbDirectory) { $AdbDirectory = Join-Path $repo '.local/google-platform-tools/platform-tools' }
    $AdbDirectory = [System.IO.Path]::GetFullPath($AdbDirectory)
    & $Python -c 'import sys; sys.path.insert(0,"tools"); import ngd_components as c; c.load_bundle(); c.verify_adb(sys.argv[1]); print("BUNDLED_COMPONENTS_AND_ADB_VERIFIED")' $AdbDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Complete reviewed project components and official ADB are required.' }
    & $Python -m PyInstaller --version
    if ($LASTEXITCODE -ne 0) { throw 'Install PyInstaller 6.22.3 in an isolated build environment first.' }
    & $Python -m PyInstaller --noconfirm --clean --onefile --windowed --noupx `
        --name NGD-Bluetooth-Installer --distpath $destination `
        --workpath (Join-Path $repo '.local/public-installer-build') `
        --specpath (Join-Path $repo '.local/public-installer-spec') `
        --paths (Join-Path $repo 'tools') `
        --add-data "$(Join-Path $repo 'profiles');profiles" `
        --add-data "$(Join-Path $repo 'installer/components');installer/components" `
        --add-data "$AdbDirectory;platform-tools" `
        (Join-Path $repo 'tools/ngd_public_installer.py')
    if ($LASTEXITCODE -ne 0) { throw 'EXE build failed.' }
    Copy-Item -LiteralPath (Join-Path $repo 'docs/PUBLIC_INSTALLER.md') -Destination (Join-Path $destination 'README-PL-EN.md')
    Copy-Item -LiteralPath (Join-Path $repo 'LICENSE') -Destination $destination
    Copy-Item -LiteralPath (Join-Path $repo 'NOTICE') -Destination $destination
    Copy-Item -LiteralPath (Join-Path $repo 'licenses/AOSP-NOTICE.txt') -Destination (Join-Path $destination 'AOSP-NOTICE.txt')
    $runtime = (& $Python -c 'import sys; print(sys.base_prefix)').Trim()
    $licenses = Join-Path $destination 'third-party-licenses'
    New-Item -ItemType Directory -Path $licenses | Out-Null
    Copy-Item -LiteralPath (Join-Path $runtime 'LICENSE.txt') -Destination (Join-Path $licenses 'Python.txt')
    $tkLicense = Join-Path $runtime 'tcl/tk8.6/license.terms'
    if (Test-Path -LiteralPath $tkLicense) { Copy-Item -LiteralPath $tkLicense -Destination (Join-Path $licenses 'Tk.txt') }
    $packager = (& $Python -c 'import importlib.metadata as m; print(m.distribution("pyinstaller").locate_file("pyinstaller-6.22.3.dist-info/licenses"))').Trim()
    if (-not (Test-Path -LiteralPath $packager)) { throw 'PyInstaller licence directory missing.' }
    Copy-Item -LiteralPath $packager -Destination (Join-Path $licenses 'PyInstaller') -Recurse
    Copy-Item -LiteralPath (Join-Path $AdbDirectory 'NOTICE.txt') -Destination (Join-Path $licenses 'ADB-NOTICE.txt')
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $destination 'NGD-Bluetooth-Installer.exe')).Hash.ToLowerInvariant()
    "$hash  NGD-Bluetooth-Installer.exe" | Set-Content -Encoding ascii (Join-Path $destination 'SHA256SUMS.txt')
    Write-Output "COMPLETE_PUBLIC_INSTALLER_BUILT $destination; PROJECT_COMPONENTS_AND_ADB_INCLUDED; OEM_READ_LOCALLY; NO_DEVICE_ACCESSED"
} finally { Pop-Location }
