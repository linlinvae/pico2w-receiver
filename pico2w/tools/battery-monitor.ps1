# SPDX-License-Identifier: Apache-2.0
param([switch]$SelfTest)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class PicoBattery {
 [StructLayout(LayoutKind.Sequential)] struct InterfaceData {
  public int size; public Guid guid; public int flags; public IntPtr reserved;
 }
 [DllImport("hid.dll")] static extern void HidD_GetHidGuid(out Guid guid);
 [DllImport("setupapi.dll", CharSet=CharSet.Unicode)] static extern IntPtr SetupDiGetClassDevs(ref Guid guid, IntPtr enumerator, IntPtr parent, uint flags);
 [DllImport("setupapi.dll")] static extern bool SetupDiEnumDeviceInterfaces(IntPtr set, IntPtr info, ref Guid guid, uint index, ref InterfaceData data);
 [DllImport("setupapi.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool SetupDiGetDeviceInterfaceDetail(IntPtr set, ref InterfaceData data, IntPtr detail, uint size, out uint required, IntPtr info);
 [DllImport("setupapi.dll")] static extern bool SetupDiDestroyDeviceInfoList(IntPtr set);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern SafeFileHandle CreateFile(string name, uint access, uint share, IntPtr security, uint creation, uint flags, IntPtr template);
 [DllImport("hid.dll")] static extern bool HidD_GetFeature(SafeFileHandle handle, byte[] report, int length);
 [DllImport("hid.dll")] static extern bool HidD_GetPreparsedData(SafeFileHandle handle, out IntPtr data);
 [DllImport("hid.dll")] static extern bool HidD_FreePreparsedData(IntPtr data);
 [DllImport("hid.dll")] static extern int HidP_GetCaps(IntPtr data, IntPtr caps);
 public static byte[][] ReadAll() {
  var reports = new List<byte[]>(); Guid guid; HidD_GetHidGuid(out guid);
  IntPtr set = SetupDiGetClassDevs(ref guid, IntPtr.Zero, IntPtr.Zero, 0x12);
  if (set == new IntPtr(-1)) return reports.ToArray();
  try {
   for (uint i=0;;i++) {
    var data = new InterfaceData(); data.size=Marshal.SizeOf(typeof(InterfaceData));
    if (!SetupDiEnumDeviceInterfaces(set, IntPtr.Zero, ref guid, i, ref data)) break;
    uint size; SetupDiGetDeviceInterfaceDetail(set, ref data, IntPtr.Zero, 0, out size, IntPtr.Zero);
    if (size < 8 || size > 65536) continue;
    IntPtr detail = Marshal.AllocHGlobal((int)size);
    try {
     Marshal.WriteInt32(detail, IntPtr.Size==8 ? 8 : 6);
     if (!SetupDiGetDeviceInterfaceDetail(set, ref data, detail, size, out size, IntPtr.Zero)) continue;
     string path = Marshal.PtrToStringUni(IntPtr.Add(detail,4));
     if (path==null || path.IndexOf("vid_2e8a&pid_10c8",StringComparison.OrdinalIgnoreCase)<0) continue;
     using (var handle=CreateFile(path,0,3,IntPtr.Zero,3,0,IntPtr.Zero)) {
      if (handle.IsInvalid) continue;
      IntPtr pp; if (!HidD_GetPreparsedData(handle,out pp)) continue;
      bool match=false; IntPtr caps=Marshal.AllocHGlobal(64);
      try {
       match=HidP_GetCaps(pp,caps)==0x110000 && Marshal.ReadInt16(caps,0)==1 &&
        (ushort)Marshal.ReadInt16(caps,2)==0xff00 && Marshal.ReadInt16(caps,8)==32;
      } finally { Marshal.FreeHGlobal(caps); HidD_FreePreparsedData(pp); }
      if (!match) continue;
      byte[] report=new byte[32]; report[0]=1;
      if (HidD_GetFeature(handle,report,report.Length) && Valid(report)) reports.Add(report);
     }
    } finally { Marshal.FreeHGlobal(detail); }
   }
  } finally { SetupDiDestroyDeviceInfoList(set); }
  return reports.ToArray();
 }
 public static bool Valid(byte[] r) {
  return r!=null && r.Length==32 && r[0]==1 && r[1]==80 && r[2]==50 && r[3]==87 && r[4]==1;
 }
}
'@
function Format-Battery([byte[]]$r) {
    if (!( [PicoBattery]::Valid($r))) { throw '状态协议不匹配' }
    $names=@('未连接手柄','Xbox Series X|S','Switch Pro','Google Stadia')
    $name=if($r[5] -lt $names.Length){$names[$r[5]]}else{'未知手柄'}
    if($r[5] -eq 0) { return "$name`r`n等待连接" }
    if($r[6] -ne 1) {
        $why=switch($r[11]) { 1 {'正在查询'} 2 {'电池服务不可用或读取失败'} 3 {'读数无效或已过期'} default {'暂无有效读数'} }
        return "$name`r`n电量：未知（$why）"
    }
    $age=[BitConverter]::ToUInt32($r,13)/1000
    if($r[7] -eq 1 -and $r[8] -le 100) {
        return "$name`r`n电量：$($r[8])%`r`n来源：BLE 电池服务；读数距今 $([math]::Round($age)) 秒"
    }
    if($r[7] -eq 2 -and $r[9] -le 4) {
        $labels=@('空','低','中','较高','满')
        $charge=if($r[10] -eq 1){'；充电中'}else{''}
        return "$name`r`n电量：$($labels[$r[9]])（$($r[9])/4 档$charge）`r`n来源：Switch 输入报告；五档读数"
    }
    if($r[7] -eq 3 -and $r[17] -ge $r[18] -and $r[17] -le $r[19]) {
        return "$name`r`n电量：HID 强度 $($r[17])（范围 $($r[18])–$($r[19])）`r`n来源：Xbox HID 原始电量字段；此数值不是百分比"
    }
    return "$name`r`n电量：未知"
}
if($SelfTest) {
    [byte[]]$r=1,80,50,87,1,1,1,1,0,255,255,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    if((Format-Battery $r) -notmatch '0%'){throw 'Empty battery test failed'}
    $r[8]=100; if((Format-Battery $r) -notmatch '100%'){throw 'Full battery test failed'}
    $r[5]=2; $r[7]=2; $r[9]=3; if((Format-Battery $r) -notmatch '3/4'){throw 'Switch level test failed'}
    $r[5]=1; $r[7]=3; $r[17]=128; $r[18]=1; $r[19]=255; if((Format-Battery $r) -notmatch '128'){throw 'Xbox raw strength test failed'}
    $r[6]=0; $r[11]=2; if((Format-Battery $r) -notmatch '未知'){throw 'Unknown battery test failed'}
    $r[5]=0; if((Format-Battery $r) -notmatch '未连接'){throw 'Disconnect test failed'}
    $r[4]=99; if([PicoBattery]::Valid($r)){throw 'Version validation failed'}
    Write-Output 'Battery monitor: native API declarations compiled; 7 protocol checks passed. No USB device accessed.'
    exit
}
[System.Windows.Forms.Application]::EnableVisualStyles()
$form=New-Object System.Windows.Forms.Form
$form.Text='Pico 2 W R4 电量'; $form.ClientSize=New-Object System.Drawing.Size(540,220)
$form.StartPosition='CenterScreen'
$label=New-Object System.Windows.Forms.Label
$label.Dock='Fill'; $label.Padding=New-Object System.Windows.Forms.Padding(20)
$label.Font=New-Object System.Drawing.Font('Microsoft YaHei UI',12)
$form.Controls.Add($label)
$timer=New-Object System.Windows.Forms.Timer; $timer.Interval=1000
$refresh={
    try {
        $reports=[PicoBattery]::ReadAll()
        if($reports.Length -eq 0){$label.Text="未找到 R4 接收器状态接口。`r`n请刷入 R4 固件，并重新插入接收器。"}
        else{$label.Text=(($reports | ForEach-Object { Format-Battery $_ }) -join "`r`n`r`n")+"`r`nUSB 输入间隔：2 ms（配置值）"}
    } catch {$label.Text="读取失败：$($_.Exception.Message)"}
}
$timer.Add_Tick($refresh); $form.Add_Shown({& $refresh; $timer.Start()})
$form.Add_FormClosed({$timer.Stop(); $timer.Dispose()})
[System.Windows.Forms.Application]::Run($form)
