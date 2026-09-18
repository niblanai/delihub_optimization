$distPath = "E:\tifany\dist"
$msys = "C:\msys64\ucrt64\bin"
$objdump = "$msys\objdump.exe"

$allRefs = [System.Collections.Generic.HashSet[string]]::new()

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

$winSys = @("kernel32","ntdll","user32","advapi32","shell32","ole32","gdi32","ws2_32",
            "bcrypt","crypt32","wintrust","version","shlwapi","msvcrt","ucrtbase",
            "vcruntime","msvcp","cfgmgr32","setupapi","winmm","imm32","combase",
            "comdlg32","coml2","secur32","rpcrt4","psapi","dbghelp")

$missing = $allRefs | Where-Object {
    $dll = $_
    $isWin = ($winSys | Where-Object { $dll -like "*$_*" -or $dll -match "^api-ms" }).Count -gt 0
    (-not $isWin) -and
    (-not (Test-Path "$distPath\$dll")) -and
    (Test-Path "$msys\$dll")
} | Sort-Object

$result = @()
$result += "=== Missing DLLs ==="
if ($missing.Count -eq 0) {
    $result += "ALL GOOD"
} else {
    foreach ($dll in $missing) {
        $size = [math]::Round((Get-Item "$msys\$dll").Length/1KB)
        $result += "COPY: $dll ($size KB)"
        Copy-Item "$msys\$dll" "$distPath\$dll" -Force
    }
}
$result | Set-Content "$distPath\dll_check2.txt"
