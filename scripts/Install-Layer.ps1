$RegistryPath = "HKLM:\Software\Khronos\OpenXR\1\ApiLayers\Implicit"
$JsonPath = Join-Path "$PSScriptRoot" "XR_APILAYER_NOVENDOR_toolkit.json"

# Search for Ultraleap.
$ultraleapPath = $null
$layers = Get-Item $RegistryPath 2> $null | Select-Object -ExpandProperty property
foreach ($entry in $layers)
{
	if ($entry -match ".*\\UltraleapHandTracking.json")
	{
		$ultraleapPath = $entry
		break
	}
}

# To guarantee the loading order of the API layers, we remove Ultraleap, add our layer, then re-add Ultraleap.
if ($ultraleapPath)
{
	Start-Process -FilePath powershell.exe -Verb RunAs -Wait -ArgumentList @"
		& {
			Remove-ItemProperty -Path $RegistryPath -Name '$ultraleapPath' -Force | Out-Null
			New-ItemProperty -Path $RegistryPath -Name '$jsonPath' -PropertyType DWord -Value 0 -Force | Out-Null
			New-ItemProperty -Path $RegistryPath -Name '$ultraleapPath' -PropertyType DWord -Value 0 -Force | Out-Null
		}
"@
}
else
{
	Start-Process -FilePath powershell.exe -Verb RunAs -Wait -ArgumentList @"
		& {
			If (-not (Test-Path $RegistryPath)) {
				New-Item -Path $RegistryPath -Force | Out-Null
			}
			New-ItemProperty -Path $RegistryPath -Name '$jsonPath' -PropertyType DWord -Value 0 -Force | Out-Null
		}
"@
}
