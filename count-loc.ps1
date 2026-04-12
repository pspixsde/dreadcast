# Rough LOC estimate: non-blank lines in src/**/*.cpp, src/**/*.hpp, and root CMakeLists.txt.
# Does not count CHANGELOG.md, README.md, assets, or build/.

$RepoRoot = $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = Get-Location
}

$exts = @('*.cpp', '*.hpp')
$srcPath = Join-Path $RepoRoot 'src'

function Count-NonBlankLines([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return 0 }
    (Get-Content -LiteralPath $path -ErrorAction SilentlyContinue | Where-Object { $_.Trim() -ne '' }).Count
}

$srcLines = 0
$fileCount = 0

if (Test-Path -LiteralPath $srcPath) {
    $cppFiles = Get-ChildItem -LiteralPath $srcPath -Recurse -File -Include $exts -ErrorAction SilentlyContinue
    foreach ($f in $cppFiles) {
        $srcLines += Count-NonBlankLines $f.FullName
        $fileCount++
    }
}

$cmake = Join-Path $RepoRoot 'CMakeLists.txt'
$cmakeLines = Count-NonBlankLines $cmake
$total = $srcLines + $cmakeLines

Write-Host "Repo: $RepoRoot"
Write-Host "C++ files under src\: $fileCount"
# Write-Host "Non-blank lines under src\: $srcLines"
# Write-Host "Non-blank lines in CMakeLists.txt: $cmakeLines"
Write-Host "Total non-blank lines (src + CMakeLists.txt): $total"
# Write-Host "(CHANGELOG.md and README.md are not counted.)"

# RUN THIS IN THE PROJECT ROOT: powershell -NoProfile -ExecutionPolicy Bypass -File "C:\Users\emilk\Projects\Dreadcast\count-loc.ps1"