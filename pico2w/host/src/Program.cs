// SPDX-License-Identifier: Apache-2.0
using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

namespace PicoControl {
    static class Program {
        [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
        [STAThread] static int Main(string[] args) {
            try {
                if(args.Length>0 && args[0]=="--self-test") {
                    string report=SelfTests.Run(); if(args.Length>1) File.WriteAllText(args[1],report,Encoding.UTF8); return 0;
                }
                if(args.Length>0 && args[0]=="--probe") {
                    var text=new StringBuilder("Pico Control V0.2 · 只读检查\r\n");
                    var devices=ReceiverReader.ReadAll(); text.AppendLine("HID 接收器数量："+devices.Count);
                    foreach(var d in devices) text.AppendLine(d+"："+(d.Status==null ? d.Error : d.Status.PadName+"；电量 "+d.Status.Battery));
                    var api=new WindowsXInput(); for(int i=0;i<4;i++) { PadState s; text.AppendLine("XInput 槽位 "+i+"："+(api.Read(i,out s)==0 ? "可用" : "未找到")); }
                    if(args.Length>1) File.WriteAllText(args[1],text.ToString(),Encoding.UTF8); return 0;
                }
                SetProcessDPIAware(); Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
                bool preview=args.Length>0 && args[0]=="--preview";
                using(var form=new MainForm(preview)) {
                    if(preview) {
                        string dir=args.Length>1 ? args[1] : AppDomain.CurrentDomain.BaseDirectory;
                        Directory.CreateDirectory(dir);form.ShowInTaskbar=false;form.Opacity=0;
                        if(args.Length>2 && args[2]=="small")form.Size=form.MinimumSize;
                        form.CreateControl(); form.Show(); Application.DoEvents();
                        for(int page=0;page<4;page++) {
                            form.SetPage(page); Application.DoEvents();
                            form.SavePreview(Path.Combine(dir,"preview-"+page+".png"));
                        }
                        form.SaveSettingsPreviews(dir);
                        form.Close(); return 0;
                    }
                    Application.Run(form);
                }
                return 0;
            } catch(Exception e) {
                if(args.Length>0) { if(args.Length>1 && args[0]!="--preview") File.WriteAllText(args[1],"FAILED: "+e,Encoding.UTF8); return 1; }
                MessageBox.Show("程序无法启动："+e.Message,"Pico Control",MessageBoxButtons.OK,MessageBoxIcon.Error); return 1;
            }
        }
    }
    public sealed class FakeXInput : IXInput {
        public readonly List<string> Writes=new List<string>(); public uint Result;
        public uint Read(int slot,out PadState s) { s=new PadState(); return Result; }
        public uint Set(int slot,ushort left,ushort right) { Writes.Add(slot+":"+left+":"+right); return Result; }
    }
    static class SelfTests {
        static int count;
        static void Check(bool condition,string label) { count++; if(!condition) throw new Exception("检查失败："+label); }
        static byte[] Report(int kind,int source,int value) {
            var r=new byte[32]; r[0]=1;r[1]=80;r[2]=50;r[3]=87;r[4]=1;r[5]=(byte)kind;r[6]=1;r[7]=(byte)source;
            r[8]=255;r[9]=255;r[10]=255;r[12]=2;
            if(source==1)r[8]=(byte)value; if(source==2)r[9]=(byte)value;
            return r;
        }
        public static string Run() {
            count=0;
            Check(Marshal.SizeOf(typeof(Gamepad))==12,"XInput gamepad layout");
            Check(Marshal.SizeOf(typeof(PadState))==16,"XInput state layout");
            Check(Marshal.SizeOf(typeof(Vibration))==4,"XInput vibration layout");
            Check(ReceiverStatus.Parse(Report(1,1,0)).Battery=="0%","empty is not unknown");
            Check(ReceiverStatus.Parse(Report(3,1,100)).Battery=="100%","BAS full");
            Check(ReceiverStatus.Parse(Report(1,1,101)).Battery=="未知","invalid percentage");
            for(int i=0;i<=4;i++) Check(ReceiverStatus.Parse(Report(2,2,i)).Battery==i+" / 4 档","Switch level "+i);
            Check(ReceiverStatus.Parse(Report(2,2,5)).Battery=="未知","invalid Switch level");
            var r=Report(1,3,0); r[17]=128;r[18]=1;r[19]=255;
            Check(ReceiverStatus.Parse(r).Battery=="强度 128","Xbox raw is not percent");
            r[19]=1;Check(ReceiverStatus.Parse(r).Battery=="未知","invalid raw range");
            r=Report(2,2,2);r[10]=1;Check(ReceiverStatus.Parse(r).BatteryDetail.Contains("充电中"),"charging");
            r=Report(3,1,50);r[6]=0; r[11]=1;Check(ReceiverStatus.Parse(r).Battery=="未知","invalid data");
            Check(ReceiverStatus.Parse(r).BatteryDetail=="正在查询电量","querying reason");
            r=Report(0,1,50);Check(ReceiverStatus.Parse(r).Battery=="—","disconnected clears display");
            r=Report(1,1,50);Array.Copy(BitConverter.GetBytes((uint)120001),0,r,13,4);
            Check(ReceiverStatus.Parse(r).Battery=="未知","stale battery");
            var invalids=new List<byte[]> { null,new byte[31],new byte[33] };
            foreach(int p in new int[]{0,1,2,3,4,5,6}) { r=Report(1,1,50);r[p]=99;invalids.Add(r); }
            foreach(var bad in invalids) { bool rejected=false;try { ReceiverStatus.Parse(bad); } catch(ArgumentException) { rejected=true; } Check(rejected,"bad protocol rejected"); }
            var api=new FakeXInput();var session=new RumbleSession(api,false);
            Check(!session.Start(0,false,true,true,0,1000,0) && api.Writes.Count==0,"no unconfirmed writes");
            Check(!session.Start(-1,true,true,true,0,1000,0),"no unselected slot");
            Check(!session.Start(4,true,true,true,0,1000,0),"invalid slot");
            Check(!session.Start(0,true,false,true,0,1000,0),"no receiver");
            Check(!session.Start(0,true,true,false,0,1000,0),"no XInput");
            Check(!session.Start(0,true,true,true,0,0,0),"zero test rejected");
            session.Stop();Check(api.Writes.Count==0,"idle stop does not touch other pads");
            Check(session.Start(2,true,true,true,100,20000,0),"start selected motor");
            session.Tick(799,true);Check(session.Active,"before deadline");session.Tick(800,true);
            Check(!session.Active && api.Writes[1]=="2:0:0","deadline stops same slot");
            int n=api.Writes.Count;session.Stop();Check(api.Writes.Count==n,"stop idempotent");
            session.Start(1,true,true,true,1000,0,20000);session.Tick(1001,false);
            Check(!session.Active && api.Writes[api.Writes.Count-1]=="1:0:0","disconnect cancels");
            session.Start(0,true,true,true,2000,100,0);session.Start(3,true,true,true,2001,0,100);
            Check(api.Writes[api.Writes.Count-2]=="0:0:0" && session.Slot==3,"restart stops prior target");session.Stop();
            api.Result=1167;Check(!session.Start(0,true,true,true,3000,100,0) && !session.Active,"failed output leaves no active session");
            Check(api.Writes[api.Writes.Count-1]=="0:0:0","failure attempts stop");
            var timedApi=new FakeXInput();
            using(var timed=new RumbleSession(timedApi)) {
                timed.Start(2,true,true,true,0,100,0);
                var wait=System.Diagnostics.Stopwatch.StartNew();
                while(timed.Active && wait.ElapsedMilliseconds<2500)System.Threading.Thread.Sleep(20);
                Check(!timed.Active && timedApi.Writes.Count==2 && timedApi.Writes[1]=="2:0:0","independent timeout without UI ticks");
            }
            Check(Dashboard.Axis(-32768)==-1 && Dashboard.Axis(32767)==1,"asymmetric axis endpoints");
            Check(Dashboard.Axis(0)==0,"axis center");
            return "PASS: "+count+" base checks; "+ConfigTests.Run()+" configuration checks.\r\nNo hardware opened; no vibration sent.\r\n";
        }
    }
}
