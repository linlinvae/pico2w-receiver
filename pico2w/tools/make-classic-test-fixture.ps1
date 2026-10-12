$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$source=Get-Content "$root/resources/bluepad32/src/components/bluepad32/bt/uni_bt_bredr.c" -Raw
$from=$source.IndexOf('void uni_bt_bredr_on_l2cap_incoming_connection(', [StringComparison]::Ordinal)
$to=$source.IndexOf('void uni_bt_bredr_on_l2cap_channel_opened(', $from, [StringComparison]::Ordinal)
if($from -lt 0 -or $to -le $from) {throw 'Missing pinned Classic incoming function'}
# Execute the unmodified vendor READY-slot replacement and channel acceptance.
$fixture='#include "uni.h"'+[Environment]::NewLine+'#include "bt/uni_bt_allowlist.h"'+[Environment]::NewLine
$fixture+=$source.Substring($from,$to-$from)
[IO.File]::WriteAllText("$root/tests/build/vendor_classic_incoming.c",$fixture)
