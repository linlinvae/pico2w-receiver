# SPDX-License-Identifier: Apache-2.0
param([string]$StageDirectory='')
# Current Switch package; original R1 archive is preserved.
& "$PSScriptRoot/package-firmware.ps1" -UsbMode switch -StageDirectory $StageDirectory
