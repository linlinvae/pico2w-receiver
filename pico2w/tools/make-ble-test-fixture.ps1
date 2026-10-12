$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$source=Get-Content "$root/resources/bluepad32/src/components/bluepad32/bt/uni_bt_le.c" -Raw
function Get-ExactSection([string]$start,[string]$end) {
    $from=$source.IndexOf($start,[StringComparison]::Ordinal)
    if($from -lt 0) {throw "Missing vendor function anchor: $start"}
    $to=$source.IndexOf($end,$from,[StringComparison]::Ordinal)
    if($to -lt 0) {throw "Missing vendor function end: $end"}
    return $source.Substring($from,$to-$from)
}
# MinGW PE links unrelated public entry points from a full vendor translation
# unit even with section GC. Replay the exact discovery implementation and its
# three helpers instead; no rewritten parser or simplified discovery mock.
$fixture='#include "uni.h"'+[Environment]::NewLine
$fixture+=Get-ExactSection 'static void hog_connect(' 'static void resume_scanning_hint('
$fixture+=Get-ExactSection 'static void get_advertisement_data(' 'static void parse_report('
$fixture+=Get-ExactSection 'void uni_bt_le_on_gap_event_advertising_report(' 'void uni_bt_le_on_hci_disconnection_complete('
[IO.File]::WriteAllText("$root/tests/build/vendor_ble_discovery.c",$fixture)
