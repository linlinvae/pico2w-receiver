// SPDX-License-Identifier: Apache-2.0
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Text;
using System.Windows.Forms;

namespace PicoControl {
    public sealed partial class MainForm : Form {
        readonly bool preview;
        readonly IXInput api;
        readonly RumbleSession rumble;
        readonly Stopwatch clock=Stopwatch.StartNew();
        readonly Timer inputTimer=new Timer(), statusTimer=new Timer();
        readonly BackgroundWorker reader=new BackgroundWorker();
        readonly ComboBox receivers=new ComboBox(), slots=new ComboBox();
        readonly Label subtitle=new Label(), footer=new Label();
        readonly CheckBox confirmed=new CheckBox();
        readonly Button strong=new Button(), weak=new Button(), both=new Button(), stop=new Button();
        readonly Panel rumbleTools=new Panel();
        readonly Button[] navigation=new Button[4];
        readonly Dashboard canvas=new Dashboard();
        List<ReceiverDevice> devices=new List<ReceiverDevice>();
        bool updating;
        volatile bool closing;
        string identity="", message="选择一个 XInput 槽位，开始查看输入。";
        long lastAvailability;
        public MainForm(bool demo) {
            preview=demo; api=demo ? (IXInput)new FakeXInput() : new WindowsXInput(); rumble=new RumbleSession(api);
            Text="Pico Control · 接收器管理"; ClientSize=new Size(1180,790); MinimumSize=new Size(1080,740);
            StartPosition=FormStartPosition.CenterScreen; BackColor=Dashboard.Background;
            Font=new Font("Microsoft YaHei UI",10); AutoScaleMode=AutoScaleMode.Dpi;
            var sidebar=new Panel { Dock=DockStyle.Left, Width=190, BackColor=Dashboard.Navy };
            Controls.Add(sidebar);
            var brand=new Label { Text="PICO\r\nCONTROL", Location=new Point(24,28),AutoSize=true,ForeColor=Color.White, Font=new Font("Segoe UI",14,FontStyle.Bold) };
            sidebar.Controls.Add(brand);
            var caption=new Label { Text="无线接收器上位机", Location=new Point(26,139),AutoSize=true,ForeColor=Color.FromArgb(168,190,197), Font=new Font("Microsoft YaHei UI",8) };sidebar.Controls.Add(caption);
            for(int i=0;i<4;i++) {
                int page=i;var b=new Button { Text=new string[]{"01    概览","02    输入测试","03    震动测试","04    手柄配置"}[i],Location=new Point(16,207+i*62),Size=new Size(158,48),FlatStyle=FlatStyle.Flat,TextAlign=ContentAlignment.MiddleLeft,Padding=new Padding(8,0,0,0),ForeColor=Color.White,Cursor=Cursors.Hand,Font=new Font("Microsoft YaHei UI",9) };
                b.FlatAppearance.BorderSize=0;b.Click+=delegate { SetPage(page); };sidebar.Controls.Add(b);navigation[i]=b;
            }
            var sideNote=new Label { Text="Pico 2 W\r\nV0.2 · Windows",ForeColor=Color.FromArgb(168,190,197),Location=new Point(26,ClientSize.Height-102),Size=new Size(154,72),Anchor=AnchorStyles.Left|AnchorStyles.Bottom,Font=new Font("Segoe UI",9) };sidebar.Controls.Add(sideNote);
            var body=new Panel { Dock=DockStyle.Fill,Padding=new Padding(24,0,24,0) };Controls.Add(body);body.BringToFront();
            var header=new Panel { Dock=DockStyle.Top,Height=187 };
            var title=new Label { Text="让手柄连接更清楚",Location=new Point(0,19),AutoSize=true,Font=new Font("Microsoft YaHei UI",19,FontStyle.Bold),ForeColor=Dashboard.Navy };
            subtitle.Text=demo ? "演示数据预览 · 未访问设备" : "连接、电量与实时输入，集中查看";subtitle.SetBounds(2,73,650,28);subtitle.ForeColor=Dashboard.Muted;
            var receiverLabel=new Label { Text="接收器",Location=new Point(2,108),Size=new Size(410,24),ForeColor=Dashboard.Muted };
            receivers.DropDownStyle=ComboBoxStyle.DropDownList;receivers.SetBounds(0,134,420,30);receivers.Items.Add("正在检测接收器…");receivers.SelectedIndex=0;
            var slotLabel=new Label { Text="输入测试槽位（请按键确认）",Location=new Point(450,108),Size=new Size(330,24),ForeColor=Dashboard.Muted };
            slots.DropDownStyle=ComboBoxStyle.DropDownList;slots.SetBounds(450,134,240,30);slots.Items.Add("请选择 XInput 槽位");
            for(int i=0;i<4;i++) slots.Items.Add("槽位 "+i+" · 检测中");slots.SelectedIndex=0;
            var export=new Button { Text="导出状态",Location=new Point(792,131),Size=new Size(110,33),FlatStyle=FlatStyle.Flat,BackColor=Color.White };export.FlatAppearance.BorderColor=Dashboard.Border;export.Click+=Export;
            header.Resize+=delegate { export.Left=header.ClientSize.Width-export.Width; };
            header.Controls.AddRange(new Control[]{title,subtitle,receiverLabel,receivers,slotLabel,slots,export});body.Controls.Add(header);
            footer.Dock=DockStyle.Bottom;footer.Height=36;footer.ForeColor=Dashboard.Muted;footer.Font=new Font("Microsoft YaHei UI",9);footer.TextAlign=ContentAlignment.MiddleLeft;body.Controls.Add(footer);
            rumbleTools.Dock=DockStyle.Bottom;rumbleTools.Height=144;rumbleTools.BackColor=Color.White;rumbleTools.Padding=new Padding(18);body.Controls.Add(rumbleTools);
            confirmed.Text="我已按键确认，选定的 XInput 槽位对应当前 Pico 接收器";confirmed.SetBounds(18,14,830,32);confirmed.ForeColor=Dashboard.Navy;
            confirmed.CheckedChanged+=delegate { if(!confirmed.Checked) rumble.Stop(); RefreshActions(); };rumbleTools.Controls.Add(confirmed);
            Button[] tests={strong,weak,both,stop};string[] names={"测试强马达","测试弱马达","同时测试","停止震动"};
            for(int i=0;i<4;i++) { var b=tests[i];b.Text=names[i];b.SetBounds(18+i*174,57,158,42);b.FlatStyle=FlatStyle.Flat;b.FlatAppearance.BorderColor=Dashboard.Border;b.BackColor=i==3 ? Dashboard.Navy : Dashboard.Teal;b.ForeColor=Color.White;rumbleTools.Controls.Add(b); }
            strong.Click+=delegate { StartRumble(20000,0); };weak.Click+=delegate { StartRumble(0,20000); };both.Click+=delegate { StartRumble(20000,20000); };
            stop.Click+=delegate { rumble.Stop();message="震动测试已停止。";RefreshActions(); };
            var motorNote=new Label { Text="每次测试 0.7 秒，约 30% 强度；切换槽位、断开或离开窗口会停止。",Location=new Point(18,108),Size=new Size(850,24),ForeColor=Dashboard.Muted,Font=new Font("Microsoft YaHei UI",9) };rumbleTools.Controls.Add(motorNote);
            canvas.Dock=DockStyle.Fill;body.Controls.Add(canvas);canvas.BringToFront();
            InitSettings(body);
            receivers.SelectedIndexChanged+=delegate { if(!updating) { CancelSelection(); ApplyDevice(); } };
            slots.SelectedIndexChanged+=delegate { if(!updating) { CancelSelection();canvas.Slot=slots.SelectedIndex-1;canvas.ClearTrace(); } };
            inputTimer.Interval=16;inputTimer.Tick+=InputTick;
            statusTimer.Interval=1000;statusTimer.Tick+=delegate { Poll(); };
            reader.DoWork+=delegate(object sender,DoWorkEventArgs e) { e.Result=ReceiverReader.ReadAll(); };
            reader.RunWorkerCompleted+=ReadComplete;
            Shown+=delegate { if(!preview) { inputTimer.Start();statusTimer.Start();Poll(); } };
            Deactivate+=delegate { if(!preview) { rumble.Stop();RefreshActions(); } };
            FormClosing+=delegate { closing=true;inputTimer.Stop();statusTimer.Stop();rumble.Stop(); };
            FormClosed+=delegate { inputTimer.Dispose();statusTimer.Dispose();rumble.Dispose(); };
            SetPage(0);
            if(demo) {
                var status=ReceiverStatus.Parse(new byte[]{1,80,50,87,1,2,1,2,255,3,0,0,2,120,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});
                devices.Add(new ReceiverDevice { Path="demo",Serial="DEMO · 示例数据",Status=status });
                updating=true;receivers.Items.Clear();receivers.Items.Add(devices[0]);receivers.SelectedIndex=0;slots.SelectedIndex=1;slots.Items[1]="槽位 0 · 示例数据";updating=false;
                ApplyDevice();canvas.Slot=0;canvas.InputReady=true;canvas.State=new PadState { Packet=1248,Pad=new Gamepad { Buttons=0x1001,LT=72,RT=184,LX=12500,LY=19200,RX=-7800,RY=-9000 } };
                for(int i=0;i<100;i++) canvas.AddTrace((float)(Math.Cos(i*.04)*.58),(float)(Math.Sin(i*.04)*.62),-.23f,-.27f);
                message="演示数据 · 实际使用时请自行选择槽位。";RefreshActions();
            }
        }
        public void SetPage(int page) {
            rumble.Stop();canvas.Page=page;rumbleTools.Visible=page==2;
            for(int i=0;i<4;i++)navigation[i].BackColor=i==page ? Dashboard.Teal : Dashboard.Navy;
            settings.Visible=page==3;canvas.Visible=page!=3;
            if(page==3)settings.BringToFront();
            canvas.Invalidate();RefreshActions();
        }
        public void SavePreview(string path) {
            using(var bitmap=new Bitmap(Width,Height)) {
                DrawToBitmap(bitmap,new Rectangle(Point.Empty,Size));
                // Native combo text is omitted by WM_PRINT on some Windows builds.
                // Render the same selected item explicitly for the demo artifact.
                using(var g=Graphics.FromImage(bitmap)) foreach(var combo in new ComboBox[]{receivers,slots}) {
                    if(combo.SelectedIndex<0)continue;
                    Point p=combo.PointToScreen(Point.Empty);p.Offset(-Left,-Top);
                    var area=new Rectangle(p.X+5,p.Y+1,combo.Width-28,combo.Height-2);
                    using(var background=new SolidBrush(Color.White))g.FillRectangle(background,p.X+1,p.Y+1,combo.Width-24,combo.Height-2);
                    TextRenderer.DrawText(g,combo.SelectedItem.ToString(),combo.Font,area,Dashboard.Navy,Color.White,TextFormatFlags.VerticalCenter|TextFormatFlags.EndEllipsis);
                }
                bitmap.Save(path);
            }
        }
        void CancelSelection() { rumble.Stop();confirmed.Checked=false;message="设备或槽位已变化，请重新按键确认。"; }
        void Poll() { if(!closing && !reader.IsBusy && !settingsWorker.IsBusy) reader.RunWorkerAsync(); }
        void ReadComplete(object sender,RunWorkerCompletedEventArgs e) {
            if(closing || IsDisposed) return;
            string selected=receivers.SelectedItem is ReceiverDevice ? ((ReceiverDevice)receivers.SelectedItem).Path : null;
            devices=e.Error==null ? (List<ReceiverDevice>)e.Result : new List<ReceiverDevice>();
            updating=true;receivers.BeginUpdate();receivers.Items.Clear();int index=-1;
            foreach(var d in devices) { int added=receivers.Items.Add(d);if(d.Path==selected)index=added; }
            if(devices.Count==0) { receivers.Items.Add(e.Error==null ? "未找到 Pico 状态接口" : "检测失败，请重试");index=0; }
            else if(index<0) index=0;
            receivers.SelectedIndex=index;receivers.EndUpdate();updating=false;ApplyDevice();
            if(e.Error!=null) message="检测失败："+e.Error.Message;
            RefreshActions();
        }
        void ApplyDevice() {
            var d=receivers.SelectedItem as ReceiverDevice;
            string next=d==null ? "" : d.Path+"|"+(d.Status==null ? -1 : d.Status.Kind);
            if(next!=identity) { CancelSelection();identity=next;canvas.ClearTrace(); }
            canvas.Device=d;
            SettingsDevice(d);
            canvas.Invalidate();RefreshActions();
        }
        bool ReceiverReady { get { return canvas.Device!=null && canvas.Device.Status!=null && canvas.Device.Status.Kind!=0; } }
        void InputTick(object sender,EventArgs e) {
            int slot=slots.SelectedIndex-1;canvas.Slot=slot;PadState state=new PadState();
            try {
                canvas.InputReady=slot>=0 && api.Read(slot,out state)==0;canvas.State=state;
                if(!canvas.InputReady) { canvas.State=new PadState();rumble.Stop();confirmed.Checked=false;canvas.ClearTrace(); }
                if(clock.ElapsedMilliseconds-lastAvailability>=500) {
                    lastAvailability=clock.ElapsedMilliseconds;updating=true;
                    for(int i=0;i<4;i++) { PadState s;bool connected=api.Read(i,out s)==0;string label="槽位 "+i+" · "+(connected ? "可用" : "未连接");if((string)slots.Items[i+1]!=label)slots.Items[i+1]=label; }
                    updating=false;
                }
            } catch(Exception ex) { canvas.InputReady=false;rumble.Stop();confirmed.Checked=false;message="输入读取失败："+ex.Message; }
            if(canvas.InputReady) canvas.AddTrace(Dashboard.Axis(state.Pad.LX),Dashboard.Axis(state.Pad.LY),Dashboard.Axis(state.Pad.RX),Dashboard.Axis(state.Pad.RY));
            rumble.Tick(clock.ElapsedMilliseconds,ReceiverReady && canvas.InputReady && confirmed.Checked);
            canvas.Invalidate();RefreshActions();
        }
        void RefreshActions() {
            bool enabled=!preview && ReceiverReady && canvas.InputReady && confirmed.Checked && !rumble.Active;
            strong.Enabled=weak.Enabled=both.Enabled=enabled;stop.Enabled=!preview && rumble.Active;
            footer.Text=preview ? "演示预览 · 所有数值为示例" : rumble.Active ? "正在测试 · 将自动停止" : message;
        }
        void StartRumble(ushort left,ushort right) {
            if(preview) return;
            bool started=rumble.Start(slots.SelectedIndex-1,confirmed.Checked,ReceiverReady,canvas.InputReady,clock.ElapsedMilliseconds,left,right);
            message=started ? "测试将在 0.7 秒后自动停止。" : "无法开始测试；检查接收器、槽位和确认选项。";
            if(rumble.LastError!=0)message="震动命令失败，Windows 返回 "+rumble.LastError+"。";
            RefreshActions();
        }
        void Export(object sender,EventArgs e) {
            using(var dialog=new SaveFileDialog { Filter="文本文件|*.txt",FileName="pico-control-status.txt" }) {
                if(dialog.ShowDialog(this)!=DialogResult.OK)return;
                var text=new StringBuilder("Pico Control V0.2\r\n");text.AppendLine("时间："+DateTimeOffset.Now.ToString("yyyy-MM-dd HH:mm:ss zzz"));
                text.AppendLine("模式："+(preview ? "演示数据" : "实时读取"));
                var d=canvas.Device;text.AppendLine("接收器："+(d==null ? "未找到" : d.ToString()));
                if(d!=null) { if(d.Status==null)text.AppendLine(d.Error);else { text.AppendLine("手柄："+d.Status.PadName);text.AppendLine("电量："+d.Status.Battery+" / "+d.Status.BatteryDetail);text.AppendLine("USB 配置间隔："+d.Status.Interval+" ms（非实测延迟）"); } }
                text.AppendLine("选定 XInput 槽位："+canvas.Slot+"；可用："+canvas.InputReady+"（由用户选择，未自动关联接收器）");
                var state=canvas.State;
                text.AppendLine("按钮："+state.Pad.Buttons.ToString("X4")+"；LT="+state.Pad.LT+"；RT="+state.Pad.RT);
                text.AppendLine("左摇杆："+state.Pad.LX+","+state.Pad.LY+"；右摇杆："+state.Pad.RX+","+state.Pad.RY);
                try { System.IO.File.WriteAllText(dialog.FileName,text.ToString(),Encoding.UTF8);message="状态已导出。"; } catch(Exception ex) { message="导出失败："+ex.Message; }
                RefreshActions();
            }
        }
    }
    public sealed class Dashboard : Control {
        public static readonly Color Background=Color.FromArgb(245,247,249), Navy=Color.FromArgb(22,43,52), Teal=Color.FromArgb(0,124,116),
            Muted=Color.FromArgb(109,126,133), Border=Color.FromArgb(222,229,232), Pale=Color.FromArgb(228,242,239);
        public int Page,Slot=-1;public bool InputReady;public PadState State;public ReceiverDevice Device;
        readonly List<PointF> leftTrace=new List<PointF>(),rightTrace=new List<PointF>();
        public Dashboard() { DoubleBuffered=true;BackColor=Background;ResizeRedraw=true; }
        public static float Axis(short value) { return value<0 ? value/32768f : value/32767f; }
        public void ClearTrace() { leftTrace.Clear();rightTrace.Clear(); }
        public void AddTrace(float lx,float ly,float rx,float ry) { leftTrace.Add(new PointF(lx,ly));rightTrace.Add(new PointF(rx,ry));if(leftTrace.Count>120){leftTrace.RemoveAt(0);rightTrace.RemoveAt(0);} }
        void TextAt(Graphics g,string text,float x,float y,float size,Color color,bool bold=false) {
            using(var font=new Font("Microsoft YaHei UI",size,bold ? FontStyle.Bold : FontStyle.Regular)) using(var brush=new SolidBrush(color)) g.DrawString(text,font,brush,x,y);
        }
        void Card(Graphics g,float x,float y,float w,float h) { using(var b=new SolidBrush(Color.White))g.FillRectangle(b,x,y,w,h);using(var p=new Pen(Border))g.DrawRectangle(p,x,y,w,h); }
        void Pill(Graphics g,string text,float x,float y,bool active) { using(var b=new SolidBrush(active ? Pale : Background))g.FillRectangle(b,x,y,126,28);TextAt(g,text,x+9,y+4,9,active ? Teal : Muted); }
        protected override void OnPaint(PaintEventArgs e) {
            base.OnPaint(e);var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;
            float scale=Math.Min(1,Height/(Page==2 ? 400f : Page==0 ? 540f : 565f));if(scale<=0)return;
            g.ScaleTransform(scale,scale);
            float w=Width/scale; if(w<500)return;
            var s=Device==null ? null : Device.Status;
            if(Page==0) {
                TextAt(g,"接收器概览",0,8,15,Navy,true);TextAt(g,"每秒更新状态与电量",0,38,10,Muted);
                float cw=(w-32)/3;
                for(int i=0;i<3;i++)Card(g,i*(cw+16),82,cw,160);
                TextAt(g,"接收器",18,99,10,Muted);TextAt(g,Device==null ? "等待接入" : s==null ? "读取异常" : "已接入",18,132,23,Navy,true);
                TextAt(g,Device==null ? "用 USB 连接 Pico 2 W" : "USB 状态接口",18,194,10,Muted);
                TextAt(g,"当前手柄",cw+34,99,10,Muted);TextAt(g,s==null ? "—" : s.Kind==1 ? "Xbox Series" : s.PadName,cw+34,137,18,Navy,true);
                TextAt(g,s!=null && s.Kind!=0 ? "无线连接已就绪" : "等待手柄连接",cw+34,194,10,Muted);
                TextAt(g,"电量",2*cw+50,99,10,Muted);TextAt(g,s==null ? "—" : s.Battery,2*cw+50,132,23,Teal,true);
                TextAt(g,s==null ? "等待状态读取" : s.Source==2 ? "Switch 五档读数" : s.Source==3 ? "HID 原始强度" : "电池服务读数",2*cw+50,194,10,Muted);
                Card(g,0,260,w,151);TextAt(g,"连接详情",18,276,12,Navy,true);
                TextAt(g,"电量来源",18,315,10,Muted);TextAt(g,s==null ? "—" : s.BatteryDetail,135,315,10,Navy);
                TextAt(g,"输入间隔",18,353,10,Muted);TextAt(g,s==null ? "—" : s.Interval+" ms · 固件配置值",135,353,10,Navy);
                TextAt(g,"下一步",0,433,12,Navy,true);TextAt(g,"选择上方输入测试槽位，按 A / B 或移动摇杆，确认对应的手柄。",0,465,10,Muted);
                TextAt(g,Device!=null && s==null ? Device.Error : "进入手柄配置页设置映射、死区与宏（需要 R5 固件）。",0,493,10,Muted);
            } else if(Page==1) {
                TextAt(g,"实时输入",0,7,15,Navy,true);Pill(g,InputReady ? "槽位 "+Slot+" · 可用" : "等待选择 / 连接",w-127,9,InputReady);
                TextAt(g,"显示选定 XInput 槽位的输出；左、右摇杆保留最近约 2 秒轨迹。",0,40,10,Muted);
                float sw=(w-16)/2;
                Stick(g,0,79,sw,252,"左摇杆",State.Pad.LX,State.Pad.LY,leftTrace);
                Stick(g,sw+16,79,sw,252,"右摇杆",State.Pad.RX,State.Pad.RY,rightTrace);
                Trigger(g,0,347,sw,"LT",State.Pad.LT);Trigger(g,sw+16,347,sw,"RT",State.Pad.RT);
                TextAt(g,"按键",0,432,11,Navy,true);
                string[] labels={"↑","↓","←","→","A","B","X","Y","LB","RB","LS","RS","Back","Start"};
                ushort[] masks={1,2,4,8,0x1000,0x2000,0x4000,0x8000,0x100,0x200,0x40,0x80,0x20,0x10};
                float bw=(w-13*6)/14;
                for(int i=0;i<labels.Length;i++) { bool active=InputReady && (State.Pad.Buttons&masks[i])!=0;float x=i*(bw+6);using(var brush=new SolidBrush(active ? Teal : Color.White))g.FillRectangle(brush,x,465,bw,44);TextAt(g,labels[i],x+5,477,labels[i].Length>3 ? 8 : 10,active ? Color.White : Muted,true); }
                TextAt(g,InputReady ? "数据包 "+State.Packet+" · 按键 0x"+State.Pad.Buttons.ToString("X4") : "请在上方选择槽位；按键与摇杆会在连接后显示。",0,528,9,Muted);
            } else {
                TextAt(g,"震动测试",0,7,15,Navy,true);TextAt(g,"分别检查双马达输出，测试会自动停止。",0,40,10,Muted);
                float cw=(w-32)/3;
                string[] title={"强马达","弱马达","双马达"}, detail={"检查低频震动","检查高频震动","检查双通道输出"};
                for(int i=0;i<3;i++) {
                    float x=i*(cw+16);Card(g,x,82,cw,176);
                    using(var p=new Pen(Teal,3)) { g.DrawEllipse(p,x+20,105,34,34);g.DrawArc(p,x+13,98,48,48,-65,130);if(i==2)g.DrawEllipse(p,x+64,105,34,34); }
                    TextAt(g,title[i],x+18,158,16,Navy,true);TextAt(g,detail[i],x+18,207,10,Muted);
                }
                Card(g,0,275,w,110);TextAt(g,"先确认测试目标",18,289,12,Navy,true);
                TextAt(g,"先到输入测试页按键确认槽位，再勾选下方确认项。",18,325,10,Muted);
                TextAt(g,"当前接收器："+(s==null ? "未找到" : s.PadName)+"     选定槽位："+(Slot<0 ? "未选择" : Slot.ToString()),18,351,10,Navy);
            }
        }
        void Stick(Graphics g,float x,float y,float w,float h,string label,short vx,short vy,List<PointF> trace) {
            Card(g,x,y,w,h);TextAt(g,label,x+18,y+14,12,Navy,true);
            float radius=77,cx=x+w/2,cy=y+132;
            using(var brush=new SolidBrush(Background))g.FillEllipse(brush,cx-radius,cy-radius,2*radius,2*radius);
            using(var pen=new Pen(Border)) { g.DrawEllipse(pen,cx-radius,cy-radius,2*radius,2*radius);g.DrawLine(pen,cx-radius,cy,cx+radius,cy);g.DrawLine(pen,cx,cy-radius,cx,cy+radius); }
            if(InputReady) { using(var brush=new SolidBrush(Color.FromArgb(110,Teal)))foreach(var p in trace)g.FillEllipse(brush,cx+p.X*radius-2,cy-p.Y*radius-2,4,4);using(var brush=new SolidBrush(Teal))g.FillEllipse(brush,cx+Axis(vx)*radius-6,cy-Axis(vy)*radius-6,12,12); }
            TextAt(g,"X  "+vx.ToString().PadLeft(6)+"     Y  "+vy.ToString().PadLeft(6),x+18,y+h-30,10,Muted);
        }
        void Trigger(Graphics g,float x,float y,float w,string label,byte value) {
            Card(g,x,y,w,65);TextAt(g,label,x+16,y+11,11,Navy,true);TextAt(g,value+" / 255",x+w-95,y+12,10,Muted);
            using(var brush=new SolidBrush(Background))g.FillRectangle(brush,x+16,y+42,w-32,7);
            if(InputReady)using(var brush=new SolidBrush(Teal))g.FillRectangle(brush,x+16,y+42,(w-32)*value/255,7);
        }
    }
}
