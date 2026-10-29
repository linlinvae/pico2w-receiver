# SPDX-License-Identifier: Apache-2.0
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$source=Get-Content "$root/resources/bluepad32/src/components/bluepad32/uni_hid_device.c" -Raw
function Get-ExactFunction([string]$start,[string]$end) {
    $from=$source.IndexOf($start,[StringComparison]::Ordinal)
    $to=$source.IndexOf($end,$from,[StringComparison]::Ordinal)
    if($from -lt 0 -or $to -le $from) {throw "Missing pinned lifecycle function: $start"}
    return $source.Substring($from,$to-$from)
}
$fixture=@'
#include "uni.h"
#include "bt/uni_bt_bredr.h"
#include "bt/uni_bt_le.h"
'@
$fixture += [Environment]::NewLine
$fixture += Get-ExactFunction 'void uni_hid_device_disconnect(' 'void uni_hid_device_delete('
# Match the definition, not the forward declaration.
$fixture += Get-ExactFunction 'static void device_connection_timeout(btstack_timer_source_t* ts) {' 'static void start_connection_timeout(uni_hid_device_t* d) {'
$fixture += 'void receiver_test_vendor_timeout(uni_hid_device_t *d) { d->connection_timer.context=d; device_connection_timeout(&d->connection_timer); }'
[IO.File]::WriteAllText("$root/tests/build/vendor_lifecycle.c",$fixture)
