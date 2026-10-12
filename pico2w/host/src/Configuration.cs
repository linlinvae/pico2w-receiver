// SPDX-License-Identifier: Apache-2.0
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using Microsoft.Win32.SafeHandles;

namespace PicoControl {
    public sealed class MacroStep {
        public ushort Buttons, Duration;
        public byte LT, RT;
    }
    public sealed class ReceiverConfig {
        public static readonly string[] Names={"↑","↓","←","→","Start","Back","LS","RS","LB","RB","Guide","A","B","X","Y"};
        public static readonly ushort[] Bits={1,2,4,8,16,32,64,128,256,512,1024,4096,8192,16384,32768};
        public byte Invert, LeftDeadzone, RightDeadzone, Strong=100, Weak=100, Trigger=255;
        public byte[] Map=new byte[15];
        public MacroStep[] Steps=new MacroStep[0];
        public ReceiverConfig() { for(byte i=0;i<15;i++)Map[i]=i; }
        public void Validate() {
            if(Invert>15 || LeftDeadzone>50 || RightDeadzone>50 || Strong>100 || Weak>100 || Map==null || Map.Length!=15)
                throw new ArgumentException("死区须为 0–50%，震动强度须为 0–100%。");
            foreach(byte m in Map)if(m>=15 && m!=255)throw new ArgumentException("按键映射无效。");
            if(Steps==null || Steps.Length>16 || (Trigger>=15 && Trigger!=255) || (Trigger==255 && Steps.Length!=0))
                throw new ArgumentException("宏最多 16 步；填写宏步骤时请选择触发键。");
            int total=0;
            foreach(var s in Steps) {
                if(s==null || (s.Buttons&0x0800)!=0 || s.Duration<20 || s.Duration>5000)
                    throw new ArgumentException("每个宏步骤的保持时间须为 20–5000 毫秒。");
                total+=s.Duration;
            }
            if(total>30000)throw new ArgumentException("宏总时长不能超过 30 秒。");
        }
        public byte[] Encode() {
            Validate();var b=new byte[128];Array.Copy(Encoding.ASCII.GetBytes("PCF1"),b,4);
            b[4]=1;b[5]=Invert;b[6]=LeftDeadzone;b[7]=RightDeadzone;b[8]=Strong;b[9]=Weak;b[10]=Trigger;b[11]=(byte)Steps.Length;
            Array.Copy(Map,0,b,12,15);
            for(int i=0;i<Steps.Length;i++) { var s=Steps[i];int p=28+i*6;Put16(b,p,s.Buttons);Put16(b,p+2,s.Duration);b[p+4]=s.LT;b[p+5]=s.RT; }
            Array.Copy(BitConverter.GetBytes(Crc32(b,124)),0,b,124,4);return b;
        }
        public static ReceiverConfig Decode(byte[] b) {
            if(b==null || b.Length!=128 || Encoding.ASCII.GetString(b,0,4)!="PCF1" || b[4]!=1 || b[27]!=0 || BitConverter.ToUInt32(b,124)!=Crc32(b,124))
                throw new ArgumentException("配置内容或校验不匹配。");
            var c=new ReceiverConfig { Invert=b[5],LeftDeadzone=b[6],RightDeadzone=b[7],Strong=b[8],Weak=b[9],Trigger=b[10] };
            if(b[11]>16)throw new ArgumentException("宏步骤数量无效。");
            Array.Copy(b,12,c.Map,0,15);c.Steps=new MacroStep[b[11]];
            for(int i=0;i<16;i++) {
                int p=28+i*6;
                if(i<c.Steps.Length)c.Steps[i]=new MacroStep {Buttons=BitConverter.ToUInt16(b,p),Duration=BitConverter.ToUInt16(b,p+2),LT=b[p+4],RT=b[p+5]};
                else for(int j=0;j<6;j++)if(b[p+j]!=0)throw new ArgumentException("未使用的宏步骤须为空。");
            }
            c.Validate();return c;
        }
        public static uint Crc32(byte[] b,int n) { uint c=0xffffffff;for(int i=0;i<n;i++) {c^=b[i];for(int j=0;j<8;j++)c=(c>>1)^((c&1)!=0?0xedb88320u:0);}return c^0xffffffff; }
        public static ushort Crc16(byte[] b,int n) { ushort c=0xffff;for(int i=0;i<n;i++) {c^=(ushort)(b[i]<<8);for(int j=0;j<8;j++)c=(ushort)((c<<1)^((c&0x8000)!=0?0x1021:0));}return c; }
        public static void Put16(byte[] b,int p,ushort v) {b[p]=(byte)v;b[p+1]=(byte)(v>>8);}
        public static bool Equal(byte[] a,byte[] b) {if(a.Length!=b.Length)return false;for(int i=0;i<a.Length;i++)if(a[i]!=b[i])return false;return true;}
        public static ushort ParseButtons(string value) {
            if(String.IsNullOrWhiteSpace(value) || value.Trim()=="释放")return 0;
            ushort mask=0;
            foreach(string part in value.Split('+')) {int i=Array.FindIndex(Names,delegate(string n){return String.Equals(n,part.Trim(),StringComparison.OrdinalIgnoreCase);});
                if(i<0)throw new ArgumentException("宏按键请用 A+B、LB、↑ 等名称；空白表示释放。");mask|=Bits[i];}
            return mask;
        }
        public static string FormatButtons(ushort mask) {var s=new System.Collections.Generic.List<string>();for(int i=0;i<15;i++)if((mask&Bits[i])!=0)s.Add(Names[i]);return String.Join("+",s.ToArray());}
    }
    public interface IFeaturePort : IDisposable { bool Set(byte[] report); bool Get(byte[] report); ReceiverStatus Status(); }
    public sealed class HidConfigPort : IFeaturePort {
        readonly SafeFileHandle handle;
        [StructLayout(LayoutKind.Sequential)] struct Attributes {public int Size;public ushort Vendor,Product,Version;}
        [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern SafeFileHandle CreateFile(string path,uint access,uint share,IntPtr security,uint creation,uint flags,IntPtr template);
        [DllImport("hid.dll")][return:MarshalAs(UnmanagedType.U1)] static extern bool HidD_GetAttributes(SafeFileHandle h,ref Attributes a);
        [DllImport("hid.dll")][return:MarshalAs(UnmanagedType.U1)] static extern bool HidD_GetFeature(SafeFileHandle h,byte[] b,int size);
        [DllImport("hid.dll")][return:MarshalAs(UnmanagedType.U1)] static extern bool HidD_SetFeature(SafeFileHandle h,byte[] b,int size);
        [DllImport("hid.dll")][return:MarshalAs(UnmanagedType.U1)] static extern bool HidD_GetPreparsedData(SafeFileHandle h,out IntPtr p);
        [DllImport("hid.dll")][return:MarshalAs(UnmanagedType.U1)] static extern bool HidD_FreePreparsedData(IntPtr p);
        [DllImport("hid.dll")] static extern int HidP_GetCaps(IntPtr p,IntPtr caps);
        [DllImport("hid.dll")][return:MarshalAs(UnmanagedType.U1)] static extern bool HidD_GetSerialNumberString(SafeFileHandle h,byte[] b,int size);
        public HidConfigPort(ReceiverDevice selected) {
            if(selected==null || String.IsNullOrEmpty(selected.Path))throw new ArgumentException("请先选择接收器。");
            handle=CreateFile(selected.Path,0xc0000000,3,IntPtr.Zero,3,0,IntPtr.Zero);
            try {
                if(handle.IsInvalid)throw new InvalidOperationException("无法打开选定接收器的配置接口。");
                var a=new Attributes {Size=Marshal.SizeOf(typeof(Attributes))};
                if(!HidD_GetAttributes(handle,ref a) || a.Vendor!=0x2e8a || a.Product!=0x10c8)throw new InvalidOperationException("选定设备身份已变化，请重新选择。");
                IntPtr pp;if(!HidD_GetPreparsedData(handle,out pp))throw new InvalidOperationException("无法检查配置接口。");
                var caps=Marshal.AllocHGlobal(64);bool match;
                try {match=HidP_GetCaps(pp,caps)==0x110000 && Marshal.ReadInt16(caps,0)==1 && (ushort)Marshal.ReadInt16(caps,2)==0xff00 && Marshal.ReadInt16(caps,8)==32;}
                finally {Marshal.FreeHGlobal(caps);HidD_FreePreparsedData(pp);}
                if(!match)throw new InvalidOperationException("选定接口不支持 Pico 配置。");
                if(!String.IsNullOrEmpty(selected.Serial)) {var b=new byte[256];
                    if(!HidD_GetSerialNumberString(handle,b,b.Length) || Encoding.Unicode.GetString(b).TrimEnd('\0')!=selected.Serial)throw new InvalidOperationException("接收器序列号已变化，请重新选择。");}
                Status();
            } catch {handle.Dispose();throw;}
        }
        public bool Set(byte[] b) {return HidD_SetFeature(handle,b,b.Length);}
        public bool Get(byte[] b) {return HidD_GetFeature(handle,b,b.Length);}
        public ReceiverStatus Status() {var b=new byte[32];b[0]=1;if(!Get(b))throw new InvalidOperationException("接收器状态读取失败。");return ReceiverStatus.Parse(b);}
        public void Dispose() {handle.Dispose();}
    }
    public sealed class ConfigClient {
        readonly IFeaturePort port;readonly Func<bool> live;
        static int nextToken=Environment.TickCount; // Unique requests prevent an old response from acknowledging a new write.
        public uint Generation;
        public ConfigClient(IFeaturePort p,Func<bool> isLive) {port=p;live=isLive;}
        ushort Token() {ushort t;do {t=(ushort)Interlocked.Increment(ref nextToken);}while(t==0);return t;}
        void CheckLive() {if(!live())throw new OperationCanceledException("设备选择已变化，操作已停止。");}
        public static byte[] Packet(byte op,ushort token,byte offset,byte[] data,byte length) {
            if(length>19 || (data!=null && data.Length<length))throw new ArgumentException("命令长度无效。");
            var q=new byte[32];q[0]=2;q[1]=80;q[2]=50;q[3]=67;q[4]=1;q[5]=op;ReceiverConfig.Put16(q,6,token);q[8]=offset;q[9]=length;
            if(data!=null)Array.Copy(data,0,q,11,length);
            var payload=new byte[31];Array.Copy(q,1,payload,0,31);ReceiverConfig.Put16(q,30,ReceiverConfig.Crc16(payload,29));return q;
        }
        public static bool Response(byte[] r,byte[] q) {
            if(r==null || r.Length!=32 || r[0]!=3 || r[1]!=80 || r[2]!=50 || r[3]!=67 || r[4]!=1 || r[9]>19 || r[5]!=q[5] || r[6]!=q[6] || r[7]!=q[7] || r[8]!=q[8])return false;
            var p=new byte[31];Array.Copy(r,1,p,0,31);return BitConverter.ToUInt16(r,30)==ReceiverConfig.Crc16(p,29);
        }
        static void Error(byte status) {
            if(status==0)return;
            if(status==9)throw new InvalidOperationException("请关闭手柄，等待无线连接和配对结束，再保存。临时配置仍已应用。");
            if(status==1)throw new InvalidOperationException("接收器正忙，请稍后重读配置。");
            if(status==5)throw new InvalidOperationException("Flash 写入失败，未确认永久保存；请重读配置。接收器将重启。");
            throw new InvalidOperationException("接收器拒绝配置命令（状态 "+status+"），请重读配置。");
        }
        byte[] Request(byte op,ushort token,byte offset,byte[] data,byte length,bool save) {
            CheckLive();byte[] q=Packet(op,token,offset,data,length);
            if(!port.Set(q))throw new InvalidOperationException(save?"保存命令结果未确认，请等待重启后重读配置。":"配置命令发送失败；请确认已刷入 R5 固件并重新插拔 USB。");
            var time=Stopwatch.StartNew();
            while(time.ElapsedMilliseconds<(save?3500:600)) {
                CheckLive();var r=new byte[32];r[0]=3;
                if(port.Get(r) && Response(r,q)) {
                    if(save && r[10]==1) {Thread.Sleep(10);continue;}
                    Error(r[10]);return r;
                }
                Thread.Sleep(10);
            }
            throw new InvalidOperationException(save?"保存结果未确认，请等待接收器重启后重读配置。":"未收到匹配的配置回复；需要 R5 固件，请刷入后重新插拔 USB。");
        }
        public void Info() {var r=Request(1,Token(),0,null,0,false);if(r[9]!=10 || r[11]!=1 || (r[12]&31)!=31 || r[13]!=128 || r[14]!=16)throw new InvalidOperationException("配置协议版本不支持。");Generation=BitConverter.ToUInt32(r,15);}
        public ReceiverConfig Read() {
            Info();var b=new byte[128];
            for(int i=0;i<128;i+=19) {byte n=(byte)Math.Min(19,128-i);var r=Request(2,Token(),(byte)i,null,n,false);if(r[9]!=n)throw new InvalidOperationException("配置回复长度不匹配。");Array.Copy(r,11,b,i,n);}
            return ReceiverConfig.Decode(b);
        }
        public void Apply(ReceiverConfig c) {
            byte[] b=c.Encode();Info();ushort t=Token();Request(3,t,0,null,0,false);bool staging=true;
            try {
                for(int i=0;i<128;i+=19) {byte n=(byte)Math.Min(19,128-i);var part=new byte[n];Array.Copy(b,i,part,0,n);Request(4,t,(byte)i,part,n,false);}
                Request(5,t,0,null,0,false);staging=false;
                if(!ReceiverConfig.Equal(b,Read().Encode()))throw new InvalidOperationException("应用后的配置回读不一致，请重新读取。");
            } finally {if(staging && live())try {Request(8,t,0,null,0,false);}catch {}}
        }
        public void Save(ReceiverConfig c) {
            CheckLive();if(port.Status().Kind!=0)throw new InvalidOperationException("请先关闭手柄，等待接收器显示未连接，再永久保存。");
            Apply(c);var checksum=new byte[4];Array.Copy(c.Encode(),124,checksum,0,4);
            var r=Request(6,Token(),0,checksum,4,true);
            if(r[9]!=10)throw new InvalidOperationException("保存回复不完整，结果未确认。");Generation=BitConverter.ToUInt32(r,15);
        }
    }
}
