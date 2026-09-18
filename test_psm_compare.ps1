$distPath = "E:\tifany\dist"
$dir = Get-Item "F:\tifany\src\اذونات"
$pdfs = Get-ChildItem -LiteralPath $dir.FullName -Filter "*.pdf"
$results = [System.Collections.Generic.List[string]]::new()

foreach ($pdf in $pdfs) {
    $safeName = "inv_" + ($pdf.BaseName -replace '\D','')
    $imgBase = "$env:TEMP\$safeName"
    
    & "$distPath\pdftoppm.exe" -r 300 -png -f 1 -l 1 $pdf.FullName $imgBase 2>$null
    $imgFile = "$imgBase-1.png"
    if (-not (Test-Path $imgFile)) { $results.Add("NO_IMG: $($pdf.Name)"); continue }
    
    $enh = "${imgBase}_enh.png"
    & "$distPath\magick.exe" $imgFile -colorspace Gray -contrast-stretch 0 -sharpen 0x1.5 -crop "2481x1400+0+450" +repage $enh 2>$null
    if (-not (Test-Path $enh)) { $enh = $imgFile }

    # Test PSM 4 (Arabic only)
    $out4 = "${imgBase}_ocr4"
    & "$distPath\tesseract.exe" $enh $out4 -l ara --psm 4 --oem 1 "--tessdata-dir" "$distPath\tessdata" 2>$null
    $lines4 = (Get-Content "$out4.txt" -Encoding UTF8 -ErrorAction SilentlyContinue) | 
              Where-Object { $_ -match '[\u0600-\u06FF]' -and $_ -match '\d+[\.,]\d+' }
    
    # Test PSM 6 (Arabic only)
    $out6 = "${imgBase}_ocr6"
    & "$distPath\tesseract.exe" $enh $out6 -l ara --psm 6 --oem 1 "--tessdata-dir" "$distPath\tessdata" 2>$null
    $lines6 = (Get-Content "$out6.txt" -Encoding UTF8 -ErrorAction SilentlyContinue) | 
              Where-Object { $_ -match '[\u0600-\u06FF]' -and $_ -match '\d+[\.,]\d+' }

    $results.Add("=== $($pdf.Name) ===")
    $results.Add("  PSM4: $($lines4.Count) lines  |  PSM6: $($lines6.Count) lines")
    
    # Show PSM6 lines (what we're currently using)
    if ($lines6.Count -gt 0) {
        foreach ($l in $lines6) { $results.Add("  P6: $l") }
    } else {
        $results.Add("  PSM6: NO PRODUCT LINES FOUND!")
        # Show PSM4 as fallback info
        foreach ($l in $lines4) { $results.Add("  P4: $l") }
    }
    
    Remove-Item $imgFile,$enh,"$out4.txt","$out6.txt" -ErrorAction SilentlyContinue
}

$results | Set-Content "E:\tifany\psm_compare.txt" -Encoding UTF8
Write-Host "Done - $($results.Count) lines"
