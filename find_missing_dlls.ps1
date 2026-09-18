$distPath = "E:\tifany\dist"
$msys = "C:\msys64\ucrt64\bin"
$objdump = "$msys\objdump.exe"

$allRefs = [System.Collections.Generic.HashSet[string]]::new()

# Scan all files in dist + the tools
$files = @(
    Get-ChildItem "$distPath\*.dll" | Select-Object -ExpandProperty FullName
    "$distPath\pdftoppm.exe"
    "$distPath\magick.exe"
    "$distPath\pdftotext.exe"
    "$distPath\tesseract.exe"
)

foreach ($f in $files) {
    if (-not (Test-Path $f)) { continue }
    try {
        $raw = & $objdump -p $f 2>$null
        $raw -split "`n" | Select-String "DLL Name:" | ForEach-Object {
            $dll = ($_ -replace ".*DLL Name:\s*","").Trim().ToLower()
            $null = $allRefs.Add($dll)
        }
    } catch {}
}

$missing = $allRefs | Where-Object {
    $_ -like "lib*" -and
    (-not (Test-Path "$distPath\$_")) -and
    (Test-Path "$msys\$_")
} | Sort-Object

if ($missing.Count -eq 0) {
    "ALL GOOD - no missing lib DLLs" | Out-File "$distPath\dll_check_result.txt"
} else {
    $missing | Out-File "$distPath\dll_check_result.txt"
    foreach ($dll in $missing) {
        Copy-Item "$msys\$dll" "$distPath\$dll" -Force
    }
    "Copied $($missing.Count) DLLs: $($missing -join ', ')" | Add-Content "$distPath\dll_check_result.txt"
}
