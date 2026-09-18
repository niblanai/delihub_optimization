$distPath = "E:\tifany\dist"
$distPyLib = "$distPath\python3.14"
$msysLib = "C:\msys64\ucrt64\lib\python3.14"
$cleanPath = "C:\Windows\System32;C:\Windows"

function Run-Pyz {
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo.FileName = "$distPath\python.exe"
    $p.StartInfo.Arguments = "`"$distPath\xls_reader.pyz`" `"C:\Users\Hoam Hasan\Desktop\إذن بيع داخلي   117122.xls`""
    $p.StartInfo.UseShellExecute = $false
    $p.StartInfo.RedirectStandardOutput = $true
    $p.StartInfo.RedirectStandardError = $true
    $p.StartInfo.EnvironmentVariables["PATH"] = $cleanPath
    $p.Start() | Out-Null
    $p.WaitForExit(8000)
    return @{ out = $p.StandardOutput.ReadToEnd(); err = $p.StandardError.ReadToEnd(); exit = $p.ExitCode }
}

for ($i = 0; $i -lt 20; $i++) {
    $r = Run-Pyz
    
    if ($r.out.Length -gt 5) {
        Write-Host "SUCCESS after $i fixes:"
        Write-Host $r.out
        break
    }
    
    $err = $r.err
    if ($err -match "No module named '([^']+)'") {
        $miss = $Matches[1]
        Write-Host "Pass $i - Missing: $miss"
        
        # Try pyd first
        $missPath = $miss -replace "\.", "\"
        $pydSearch = Get-ChildItem "$msysLib\lib-dynload" -Filter "${miss}*.pyd" -ErrorAction SilentlyContinue
        if (-not $pydSearch) {
            $baseMiss = ($miss -split "\.")[-1]
            $pydSearch = Get-ChildItem "$msysLib\lib-dynload" -Filter "${baseMiss}*.pyd" -ErrorAction SilentlyContinue
        }
        
        if ($pydSearch) {
            Copy-Item $pydSearch[0].FullName "$distPyLib\lib-dynload\$($pydSearch[0].Name)" -Force
            Write-Host "  -> Copied pyd: $($pydSearch[0].Name)"
        }
        else {
            # Try .py file
            $pyFile = "$msysLib\$missPath.py"
            $pkgInit = "$msysLib\$missPath\__init__.py"
            if (Test-Path $pyFile) {
                $dstDir = "$distPyLib\$(Split-Path $missPath -Parent)"
                New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
                Copy-Item $pyFile "$distPyLib\$missPath.py" -Force
                Write-Host "  -> Copied py: $miss.py"
            }
            elseif (Test-Path $pkgInit) {
                Copy-Item "$msysLib\$missPath" "$distPyLib\$missPath" -Recurse -Force
                Write-Host "  -> Copied package: $miss"
            }
            else {
                Write-Host "  -> CANNOT FIND: $miss"
                Write-Host "Full error: $err"
                break
            }
        }
    }
    else {
        Write-Host "Unexpected error: $err"
        break
    }
}
