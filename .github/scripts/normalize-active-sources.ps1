$ErrorActionPreference = 'Stop'
$utf8Strict = New-Object System.Text.UTF8Encoding($false, $true)
$utf8Out = New-Object System.Text.UTF8Encoding($false)
$legacy = [System.Text.Encoding]::GetEncoding(936)
$files = @(
  'ExtremeCopy/Core/XCAsyncFileDataTransFilter.h',
  'ExtremeCopy/Core/XCAsyncFileDataTransFilter.cpp',
  'ExtremeCopy/Core/XCDestinationFilter.cpp',
  'ExtremeCopy/App/XCConfiguration.cpp'
)
foreach ($path in $files) {
  $bytes = [IO.File]::ReadAllBytes($path)
  try {
    [void]$utf8Strict.GetString($bytes)
    Write-Host "UTF-8 already: $path"
  }
  catch {
    $text = $legacy.GetString($bytes)
    [IO.File]::WriteAllText($path, $text, $utf8Out)
    Write-Host "Converted CP936 -> UTF-8: $path"
  }
}
git diff --check
