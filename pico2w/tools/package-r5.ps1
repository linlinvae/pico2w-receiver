# SPDX-License-Identifier: Apache-2.0
param([string]$StageDirectory='')
# Current Windows package; original R5 and Pico Control archives are preserved.
& "$PSScriptRoot/package-firmware.ps1" -UsbMode xinput -StageDirectory $StageDirectory
