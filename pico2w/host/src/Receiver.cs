// SPDX-License-Identifier: Apache-2.0
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace PicoControl {
    public sealed class ReceiverStatus {
        public int Kind, Source, Percent, Level, Charging, Reason, Interval, Raw, Min, Max;
        public bool Valid;
        public uint Age;
        public string PadName { get { return new string[] { "未连接手柄", "Xbox Series X|S", "Switch Pro", "Google Stadia" }[Kind]; } }
        public static ReceiverStatus Parse(byte[] r) {
            if (r == null || r.Length != 32 || r[0] != 1 || r[1] != 80 || r[2] != 50 || r[3] != 87 || r[4] != 1)
                throw new ArgumentException("状态协议不匹配（需要 P2W v1）。");
            if (r[5] > 3 || r[6] > 1) throw new ArgumentException("手柄类型或有效标志无效。");
            return new ReceiverStatus { Kind=r[5], Valid=r[6]==1, Source=r[7], Percent=r[8], Level=r[9], Charging=r[10],
                Reason=r[11], Interval=r[12], Age=BitConverter.ToUInt32(r,13), Raw=r[17], Min=r[18], Max=r[19] };
        }
        public string Battery {
            get {
                if (Kind==0) return "—";
                if (!Valid || Age>120000) return "未知";
                if (Source==1 && Percent<=100) return Percent+"%";
                if (Source==2 && Level<=4) return Level+" / 4 档";
                if (Source==3 && Max>Min && Raw>=Min && Raw<=Max) return "强度 "+Raw;
                return "未知";
            }
        }
        public string BatteryDetail {
            get {
                if (Kind==0) return "打开手柄，等待无线连接";
                if (Valid && Age>120000) return "读数已过期";
                if (!Valid) return Reason==1 ? "正在查询电量" : Reason==3 ? "读数无效或已过期" : "尚未取得有效电量";
                if (Source==1 && Percent<=100) return "BLE 电池服务 · "+Age/1000+" 秒前";
                if (Source==2 && Level<=4) return new string[]{"空","低","中","较高","满"}[Level]+(Charging==1 ? " · 充电中" : "")+" · Switch 五档读数";
                if (Source==3 && Max>Min && Raw>=Min && Raw<=Max) return "HID 原始强度 · 范围 "+Min+"–"+Max+"（不是百分比）";
                return "暂不支持此电量字段";
            }
        }
    }
    public sealed class ReceiverDevice {
        public string Path, Serial, Error;
        public ReceiverStatus Status;
        public override string ToString() { return "Pico 2 W · "+(String.IsNullOrEmpty(Serial) ? "接收器" : Serial); }
    }
    public static class ReceiverReader {
        [StructLayout(LayoutKind.Sequential)] struct InterfaceData { public int Size; public Guid Guid; public int Flags; public IntPtr Reserved; }
        [DllImport("hid.dll")] static extern void HidD_GetHidGuid(out Guid guid);
        [DllImport("setupapi.dll", CharSet=CharSet.Unicode)] static extern IntPtr SetupDiGetClassDevs(ref Guid guid, IntPtr enumerator, IntPtr parent, uint flags);
        [DllImport("setupapi.dll")] static extern bool SetupDiEnumDeviceInterfaces(IntPtr set, IntPtr info, ref Guid guid, uint index, ref InterfaceData data);
        [DllImport("setupapi.dll", CharSet=CharSet.Unicode)] static extern bool SetupDiGetDeviceInterfaceDetail(IntPtr set, ref InterfaceData data, IntPtr detail, uint size, out uint required, IntPtr info);
        [DllImport("setupapi.dll")] static extern bool SetupDiDestroyDeviceInfoList(IntPtr set);
        [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] static extern SafeFileHandle CreateFile(string path, uint access, uint share, IntPtr security, uint creation, uint flags, IntPtr template);
        [DllImport("hid.dll")][return:MarshalAs(UnmanagedType.U1)] static extern bool HidD_GetFeature(SafeFileHandle h, byte[] report, int length);
        [DllImport("hid.dll")][return:MarshalAs(UnmanagedType.U1)] static extern bool HidD_GetSerialNumberString(SafeFileHandle h, byte[] text, int length);
        [DllImport("hid.dll")][return:MarshalAs(UnmanagedType.U1)] static extern bool HidD_GetPreparsedData(SafeFileHandle h, out IntPtr data);
        [DllImport("hid.dll")][return:MarshalAs(UnmanagedType.U1)] static extern bool HidD_FreePreparsedData(IntPtr data);
        [DllImport("hid.dll")] static extern int HidP_GetCaps(IntPtr data, IntPtr caps);
        public static List<ReceiverDevice> ReadAll() {
            var result=new List<ReceiverDevice>(); Guid guid; HidD_GetHidGuid(out guid);
            IntPtr set=SetupDiGetClassDevs(ref guid,IntPtr.Zero,IntPtr.Zero,0x12);
            if (set==new IntPtr(-1)) throw new InvalidOperationException("无法枚举 HID 接口。");
            try {
                for(uint i=0;;i++) {
                    var data=new InterfaceData(); data.Size=Marshal.SizeOf(typeof(InterfaceData));
                    if(!SetupDiEnumDeviceInterfaces(set,IntPtr.Zero,ref guid,i,ref data)) break;
                    uint size; SetupDiGetDeviceInterfaceDetail(set,ref data,IntPtr.Zero,0,out size,IntPtr.Zero);
                    if(size<8 || size>65536) continue;
                    IntPtr detail=Marshal.AllocHGlobal((int)size);
                    try {
                        Marshal.WriteInt32(detail,IntPtr.Size==8 ? 8 : 6);
                        if(!SetupDiGetDeviceInterfaceDetail(set,ref data,detail,size,out size,IntPtr.Zero)) continue;
                        string path=Marshal.PtrToStringUni(IntPtr.Add(detail,4));
                        if(path==null || path.IndexOf("vid_2e8a&pid_10c8",StringComparison.OrdinalIgnoreCase)<0) continue;
                        var device=new ReceiverDevice { Path=path, Serial="", Error="" };
                        // Zero desired access: this status channel never writes configuration.
                        using(var h=CreateFile(path,0,3,IntPtr.Zero,3,0,IntPtr.Zero)) {
                            if(h.IsInvalid) { device.Error="状态接口无法打开"; result.Add(device); continue; }
                            IntPtr pp; if(!HidD_GetPreparsedData(h,out pp)) continue;
                            bool match=false; IntPtr caps=Marshal.AllocHGlobal(64);
                            try { match=HidP_GetCaps(pp,caps)==0x110000 && Marshal.ReadInt16(caps,0)==1 &&
                                (ushort)Marshal.ReadInt16(caps,2)==0xff00 && Marshal.ReadInt16(caps,8)==32;
                            } finally { Marshal.FreeHGlobal(caps); HidD_FreePreparsedData(pp); }
                            if(!match) continue;
                            var serial=new byte[256];
                            if(HidD_GetSerialNumberString(h,serial,serial.Length)) device.Serial=System.Text.Encoding.Unicode.GetString(serial).TrimEnd('\0');
                            var report=new byte[32]; report[0]=1;
                            if(!HidD_GetFeature(h,report,report.Length)) device.Error="状态读取失败；请检查接收器固件与 USB 连接";
                            else { try { device.Status=ReceiverStatus.Parse(report); } catch(ArgumentException e) { device.Error=e.Message; } }
                        }
                        result.Add(device);
                    } finally { Marshal.FreeHGlobal(detail); }
                }
            } finally { SetupDiDestroyDeviceInfoList(set); }
            return result;
        }
    }
    [StructLayout(LayoutKind.Sequential)] public struct Gamepad {
        public ushort Buttons; public byte LT, RT; public short LX, LY, RX, RY;
    }
    [StructLayout(LayoutKind.Sequential)] public struct PadState { public uint Packet; public Gamepad Pad; }
    [StructLayout(LayoutKind.Sequential)] public struct Vibration { public ushort Left, Right; }
    public interface IXInput {
        uint Read(int slot,out PadState state);
        uint Set(int slot,ushort left,ushort right);
    }
    public sealed class WindowsXInput : IXInput {
        [DllImport("xinput1_4.dll")] static extern uint XInputGetState(uint slot,out PadState state);
        [DllImport("xinput1_4.dll")] static extern uint XInputSetState(uint slot,ref Vibration value);
        public uint Read(int slot,out PadState state) { return XInputGetState((uint)slot,out state); }
        public uint Set(int slot,ushort left,ushort right) { var v=new Vibration { Left=left,Right=right }; return XInputSetState((uint)slot,ref v); }
    }
    public sealed class RumbleSession : IDisposable {
        readonly IXInput input;
        readonly object guard=new object();
        readonly System.Threading.Timer timer;
        readonly System.Diagnostics.Stopwatch clock=System.Diagnostics.Stopwatch.StartNew();
        bool active; int slot=-1; uint lastError;
        public bool Active { get { lock(guard)return active; } }
        public int Slot { get { lock(guard)return slot; } }
        public uint LastError { get { lock(guard)return lastError; } }
        long deadline,realDeadline;
        public RumbleSession(IXInput api) : this(api,true) { }
        public RumbleSession(IXInput api,bool timed) {
            input=api;
            if(timed)timer=new System.Threading.Timer(delegate { TimedStop(); },null,System.Threading.Timeout.Infinite,System.Threading.Timeout.Infinite);
        }
        public bool Start(int target,bool confirmed,bool receiverReady,bool slotReady,long now,ushort left,ushort right) {
            lock(guard) {
                if(target<0 || target>3 || !confirmed || !receiverReady || !slotReady || (left==0 && right==0)) return false;
                Stop();slot=target;lastError=input.Set(target,left,right);
                if(lastError!=0) { input.Set(target,0,0);slot=-1;return false; }
                deadline=now+700;realDeadline=clock.ElapsedMilliseconds+700;active=true;
                if(timer!=null)timer.Change(700,System.Threading.Timeout.Infinite);
                return true;
            }
        }
        public void Tick(long now,bool stillReady) { lock(guard) { if(active && (!stillReady || now>=deadline))Stop(); } }
        void TimedStop() {
            lock(guard) {
                if(!active)return;
                long remaining=realDeadline-clock.ElapsedMilliseconds;
                // An old queued callback must not stop a newly started test early.
                if(remaining>0)timer.Change((int)remaining,System.Threading.Timeout.Infinite);
                else Stop();
            }
        }
        public void Stop() {
            lock(guard) {
                if(!active)return;
                if(timer!=null)timer.Change(System.Threading.Timeout.Infinite,System.Threading.Timeout.Infinite);
                lastError=input.Set(slot,0,0);active=false;slot=-1;
            }
        }
        public void Dispose() { Stop();if(timer!=null)timer.Dispose(); }
    }
}
