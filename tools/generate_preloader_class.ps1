param(
    [Parameter(Mandatory = $true)][string]$ClassFile,
    [Parameter(Mandatory = $true)][string]$OutputFile
)

if (-not (Test-Path $ClassFile)) {
    Write-Error "Class file not found: $ClassFile"
    exit 1
}

$bytes = [System.IO.File]::ReadAllBytes($ClassFile)
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("// Auto-generated — do not edit.")
[void]$sb.AppendLine("#pragma once")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("#include <cstddef>")
[void]$sb.AppendLine("#include <cstdint>")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("inline constexpr std::size_t preloader_classSizes = $($bytes.Length);")
[void]$sb.Append("inline const unsigned char preloader_class[] = {")

for ($i = 0; $i -lt $bytes.Length; $i++) {
    if ($i % 16 -eq 0) {
        [void]$sb.AppendLine()
        [void]$sb.Append("    ")
    }
    [void]$sb.Append("0x{0:X2}" -f $bytes[$i])
    if ($i -lt $bytes.Length - 1) {
        [void]$sb.Append(", ")
    }
}

[void]$sb.AppendLine()
[void]$sb.AppendLine("};")
[void]$sb.AppendLine("")

$dir = Split-Path $OutputFile -Parent
if (-not (Test-Path $dir)) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
}

[System.IO.File]::WriteAllText($OutputFile, $sb.ToString(), [System.Text.UTF8Encoding]::new($false))
Write-Host "Wrote $OutputFile ($($bytes.Length) bytes)"
