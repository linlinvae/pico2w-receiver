// SPDX-License-Identifier: Apache-2.0
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Windows.Forms;

namespace PicoControl {
    public sealed partial class MainForm {
        readonly ConfigEditor settings=new ConfigEditor();
        readonly BackgroundWorker settingsWorker=new BackgroundWorker();
        volatile int configEpoch;
        string settingsPath="";
        bool settingsLoaded;
        void InitSettings(Panel body) {
            settings.Dock=DockStyle.Fill;body.Controls.Add(settings);settings.Visible=false;
            settings.ReadClicked+=delegate {ConfigOperation(0);};
            settings.ApplyClicked+=delegate {ConfigOperation(1);};
            settings.SaveClicked+=delegate {ConfigOperation(2);};
        }
        public void SaveSettingsPreviews(string dir) {
            var c=new ReceiverConfig {LeftDeadzone=12,RightDeadzone=7,Strong=60,Trigger=12,Steps=new MacroStep[]{new MacroStep {Buttons=4096,Duration=100},new MacroStep {Duration=80},new MacroStep {Buttons=4096,Duration=100},new MacroStep {Duration=80}}};
            c.Map[11]=14;settings.LoadEditor(c);settings.Note="演示配置 · B 触发 A 连按 · 未访问或写入接收器";
            SetPage(3);
            for(int i=0;i<3;i++) {settings.PreviewTab(i);Application.DoEvents();SavePreview(System.IO.Path.Combine(dir,"settings-"+i+".png"));}
        }
        void SettingsDevice(ReceiverDevice d) {
            string path=d==null?"":d.Path;
            if(path!=settingsPath) {settingsPath=path;configEpoch++;settingsLoaded=false;settings.Note="请先读取选定接收器的配置。编辑区保留本地内容。";}
            settings.Actions(!preview && d!=null && d.Status!=null && !settingsWorker.IsBusy,settingsLoaded && !preview && !settingsWorker.IsBusy);
        }
        void ConfigOperation(int action) {
            if(preview || settingsWorker.IsBusy)return;
            var d=canvas.Device;if(d==null || d.Status==null)return;
            if(action!=0 && !settingsLoaded)return;
            ReceiverConfig c=null;
            try {if(action!=0)c=settings.ReadEditor();}
            catch(Exception e) {settings.Note=e.Message;return;}
            rumble.Stop();int epoch=configEpoch;var requested=c;
            settings.Note=action==0?"正在读取接收器配置…":action==1?"正在应用，并核对回读…":"正在保存；接收器会自动重启，请勿拔出 USB…";
            settings.Actions(false,false);settings.Editable=false;receivers.Enabled=false;
            // A fresh worker per operation keeps the captured device and epoch immutable.
            settingsWorker.DoWork+=SettingsWork;
            settingsWorker.RunWorkerCompleted+=SettingsComplete;
            settingsWorker.RunWorkerAsync(new ConfigWork {Device=d,Config=requested,Action=action,Epoch=epoch});
        }
        sealed class ConfigWork {public ReceiverDevice Device;public ReceiverConfig Config;public int Action,Epoch;public uint Generation;}
        void SettingsWork(object sender,DoWorkEventArgs e) {
            var w=(ConfigWork)e.Argument;
            using(var port=new HidConfigPort(w.Device)) {
                var client=new ConfigClient(port,delegate {return !closing && configEpoch==w.Epoch;});
                if(w.Action==0)w.Config=client.Read();
                else if(w.Action==1)client.Apply(w.Config);
                else client.Save(w.Config);
                w.Generation=client.Generation;e.Result=w;
            }
        }
        void SettingsComplete(object sender,RunWorkerCompletedEventArgs e) {
            settingsWorker.DoWork-=SettingsWork;settingsWorker.RunWorkerCompleted-=SettingsComplete;
            if(closing || IsDisposed)return;
            settings.Editable=true;receivers.Enabled=true;
            if(e.Error!=null) {
                settings.Note=e.Error.Message;settingsLoaded=false;
            } else {
                var w=(ConfigWork)e.Result;
                if(w.Epoch!=configEpoch) {settingsLoaded=false;settings.Note="设备已变化，请重新读取配置。";}
                else {
                    settings.LoadEditor(w.Config);settingsLoaded=w.Action!=2;
                    settings.Note=w.Action==0?"已读取接收器当前配置 · Flash 代次 "+w.Generation:
                        w.Action==1?"已应用并回读核对；断电后会恢复上次永久保存的配置。":
                        "已永久保存 · 代次 "+w.Generation+"。接收器正在重启，恢复后可重读确认。";
                }
            }
            SettingsDevice(canvas.Device);Poll();
        }
    }
    public sealed class ConfigEditor : UserControl {
        public event EventHandler ReadClicked,ApplyClicked,SaveClicked;
        readonly ComboBox[] maps=new ComboBox[15];
        readonly NumericUpDown left=new NumericUpDown(),right=new NumericUpDown(),strong=new NumericUpDown(),weak=new NumericUpDown();
        readonly CheckBox[] inverted=new CheckBox[4];
        readonly ComboBox trigger=new ComboBox();
        readonly DataGridView steps=new DataGridView();
        readonly TabControl tabs=new TabControl();
        readonly Label note=new Label();
        readonly Button read=new Button(),apply=new Button(),save=new Button(),defaults=new Button();
        public string Note {set {note.Text=value;}}
        public bool Editable {set {tabs.Enabled=value;defaults.Enabled=value;}}
        public ConfigEditor() {
            BackColor=Color.White;Padding=new Padding(12);ForeColor=Dashboard.Navy;
            var actions=new FlowLayoutPanel {Dock=DockStyle.Bottom,Height=48,WrapContents=false,Padding=new Padding(0,7,0,0)};
            Button[] bs={read,apply,save,defaults};string[] captions={"读取接收器","立即应用","永久保存并重启","编辑区恢复默认"};
            for(int i=0;i<bs.Length;i++) {var b=bs[i];b.Text=captions[i];b.Width=i>=2?184:138;b.Height=32;b.FlatStyle=FlatStyle.Flat;b.FlatAppearance.BorderColor=Dashboard.Border;b.BackColor=i==1?Dashboard.Teal:Color.White;b.ForeColor=i==1?Color.White:Dashboard.Navy;actions.Controls.Add(b);}
            read.Click+=delegate {if(ReadClicked!=null)ReadClicked(this,EventArgs.Empty);};apply.Click+=delegate {if(ApplyClicked!=null)ApplyClicked(this,EventArgs.Empty);};save.Click+=delegate {if(SaveClicked!=null)SaveClicked(this,EventArgs.Empty);};
            defaults.Click+=delegate {LoadEditor(new ReceiverConfig());Note="编辑区已恢复默认；点击立即应用后才会改变接收器。";};
            Controls.Add(actions);
            note.Dock=DockStyle.Bottom;note.Height=48;note.ForeColor=Dashboard.Muted;note.Font=new Font("Microsoft YaHei UI",9);note.Text="请先读取接收器配置。永久保存前关闭手柄；配置供三种手柄共用。";Controls.Add(note);
            tabs.Dock=DockStyle.Fill;Controls.Add(tabs);tabs.BringToFront();
            var mapping=new TabPage("按键映射") {BackColor=Color.White,Padding=new Padding(12),AutoScroll=true};tabs.TabPages.Add(mapping);
            var table=new TableLayoutPanel {Dock=DockStyle.Top,Height=310,ColumnCount=6,RowCount=6};
            for(int i=0;i<3;i++) {table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,104));table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,33.33f));}
            for(int i=0;i<5;i++)table.RowStyles.Add(new RowStyle(SizeType.Absolute,46));
            for(int i=0;i<15;i++) {
                int col=(i/5)*2,row=i%5;var label=new Label {Text=ReceiverConfig.Names[i]+" →",Dock=DockStyle.Fill,TextAlign=ContentAlignment.MiddleLeft};
                var combo=new ComboBox {Dock=DockStyle.Fill,DropDownStyle=ComboBoxStyle.DropDownList,Margin=new Padding(3,8,15,5)};
                combo.Items.AddRange(ReceiverConfig.Names);combo.Items.Add("禁用");maps[i]=combo;table.Controls.Add(label,col,row);table.Controls.Add(combo,col+1,row);
            }
            var mapNote=new Label {Text="这里使用标准 XInput 名称。Switch 的 A/B、X/Y 请先在输入测试页确认。\r\n宏触发键在宏启用时被占用；其他按键、摇杆仍可正常操作。",Dock=DockStyle.Fill,ForeColor=Dashboard.Muted,AutoSize=true,Margin=new Padding(0,12,0,0)};
            table.Controls.Add(mapNote,0,5);table.SetColumnSpan(mapNote,6);mapping.Controls.Add(table);
            var analog=new TabPage("摇杆与震动") {BackColor=Color.White,Padding=new Padding(18),AutoScroll=true};tabs.TabPages.Add(analog);
            var fields=new TableLayoutPanel {Dock=DockStyle.Top,Height=305,ColumnCount=4,RowCount=5};
            fields.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,165));fields.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,50));fields.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,165));fields.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,50));
            for(int i=0;i<5;i++)fields.RowStyles.Add(new RowStyle(SizeType.Absolute,52));
            AddNumber(fields,"左摇杆死区 %",left,50,0,0);AddNumber(fields,"右摇杆死区 %",right,50,2,0);
            AddNumber(fields,"强马达强度 %",strong,100,0,1);AddNumber(fields,"弱马达强度 %",weak,100,2,1);
            string[] labels={"反转左 X 轴","反转左 Y 轴","反转右 X 轴","反转右 Y 轴"};
            for(int i=0;i<4;i++) {inverted[i]=new CheckBox {Text=labels[i],Dock=DockStyle.Fill};fields.Controls.Add(inverted[i],(i%2)*2,2+i/2);fields.SetColumnSpan(inverted[i],2);}
            var dzNote=new Label {Text="死区按 X/Y 各轴处理，中心归零后重新铺满行程。0% 关闭，最多 50%。\r\n震动强度会缩放游戏发来的马达指令。",AutoSize=true,ForeColor=Dashboard.Muted};fields.Controls.Add(dzNote,0,4);fields.SetColumnSpan(dzNote,4);analog.Controls.Add(fields);
            var macro=new TabPage("按键宏") {BackColor=Color.White,Padding=new Padding(12)};tabs.TabPages.Add(macro);
            var macroTop=new FlowLayoutPanel {Dock=DockStyle.Top,Height=40,WrapContents=false};
            macroTop.Controls.Add(new Label {Text="触发键",AutoSize=true,Margin=new Padding(0,8,10,0)});
            trigger.DropDownStyle=ComboBoxStyle.DropDownList;trigger.Width=140;trigger.Items.Add("关闭宏");trigger.Items.AddRange(ReceiverConfig.Names);macroTop.Controls.Add(trigger);
            var clear=new Button {Text="清空步骤",Width=100,Height=29};clear.Click+=delegate {steps.Rows.Clear();trigger.SelectedIndex=0;};macroTop.Controls.Add(clear);
            var example=new Button {Text="填入 A 连按示例",Width=184,Height=29};example.Click+=delegate {trigger.SelectedIndex=13;steps.Rows.Clear();steps.Rows.Add("A",100,0,0);steps.Rows.Add("",80,0,0);steps.Rows.Add("A",100,0,0);steps.Rows.Add("",80,0,0);};macroTop.Controls.Add(example);
            var macroNote=new Label {Dock=DockStyle.Bottom,Height=67,ForeColor=Dashboard.Muted,Font=new Font("Microsoft YaHei UI",9),Text="一次按下触发键执行一次，长按不循环；最多 16 步，总计不超过 30 秒。\r\n按键可写 A+B、LB、↑；空白表示释放。保持 20–5000 ms，LT/RT 为 0–255。\r\n点击行头选中后按 Delete 删除。宏输出使用目标按键；断线、USB 休眠会取消。"};
            steps.Dock=DockStyle.Fill;steps.BackgroundColor=Color.White;steps.BorderStyle=BorderStyle.FixedSingle;steps.AllowUserToAddRows=true;steps.AllowUserToDeleteRows=true;steps.AllowUserToResizeRows=false;steps.AutoSizeColumnsMode=DataGridViewAutoSizeColumnsMode.Fill;steps.RowHeadersWidth=38;steps.SelectionMode=DataGridViewSelectionMode.FullRowSelect;steps.MultiSelect=false;steps.RowTemplate.Height=28;
            steps.Columns.Add("buttons","按键（用 + 组合）");steps.Columns.Add("duration","保持时间 ms");steps.Columns.Add("lt","LT 0–255");steps.Columns.Add("rt","RT 0–255");steps.Columns[0].FillWeight=160;
            steps.DefaultValuesNeeded+=delegate(object sender,DataGridViewRowEventArgs e) {e.Row.Cells[1].Value=100;e.Row.Cells[2].Value=0;e.Row.Cells[3].Value=0;};
            macro.Controls.Add(steps);macro.Controls.Add(macroNote);macro.Controls.Add(macroTop);
            LoadEditor(new ReceiverConfig());Actions(false,false);
        }
        static void AddNumber(TableLayoutPanel t,string label,NumericUpDown n,int max,int col,int row) {
            n.Minimum=0;n.Maximum=max;n.Width=110;n.Margin=new Padding(3,10,3,3);
            t.Controls.Add(new Label {Text=label,Dock=DockStyle.Fill,TextAlign=ContentAlignment.MiddleLeft},col,row);t.Controls.Add(n,col+1,row);
        }
        public void Actions(bool canRead,bool canWrite) {read.Enabled=canRead;apply.Enabled=save.Enabled=canWrite;}
        public void LoadEditor(ReceiverConfig c) {
            for(int i=0;i<15;i++)maps[i].SelectedIndex=c.Map[i]==255?15:c.Map[i];
            left.Value=c.LeftDeadzone;right.Value=c.RightDeadzone;strong.Value=c.Strong;weak.Value=c.Weak;
            for(int i=0;i<4;i++)inverted[i].Checked=(c.Invert&(1<<i))!=0;
            trigger.SelectedIndex=c.Trigger==255?0:c.Trigger+1;steps.Rows.Clear();
            foreach(var s in c.Steps)steps.Rows.Add(ReceiverConfig.FormatButtons(s.Buttons),s.Duration,s.LT,s.RT);
        }
        public ReceiverConfig ReadEditor() {
            if(!steps.EndEdit())throw new ArgumentException("请先完成当前宏单元格的编辑。");
            var c=new ReceiverConfig {LeftDeadzone=(byte)left.Value,RightDeadzone=(byte)right.Value,Strong=(byte)strong.Value,Weak=(byte)weak.Value,Trigger=trigger.SelectedIndex<=0?(byte)255:(byte)(trigger.SelectedIndex-1)};
            for(int i=0;i<15;i++)c.Map[i]=maps[i].SelectedIndex==15?(byte)255:(byte)maps[i].SelectedIndex;
            for(int i=0;i<4;i++)if(inverted[i].Checked)c.Invert|=(byte)(1<<i);
            var list=new List<MacroStep>();
            foreach(DataGridViewRow row in steps.Rows) {
                if(row.IsNewRow)continue;
                ushort ms;byte lt,rt;
                if(!UInt16.TryParse(Convert.ToString(row.Cells[1].Value),out ms) || !Byte.TryParse(Convert.ToString(row.Cells[2].Value),out lt) || !Byte.TryParse(Convert.ToString(row.Cells[3].Value),out rt))
                    throw new ArgumentException("宏第 "+(row.Index+1)+" 行：请填写保持时间及 LT/RT 数值。");
                list.Add(new MacroStep {Buttons=ReceiverConfig.ParseButtons(Convert.ToString(row.Cells[0].Value)),Duration=ms,LT=lt,RT=rt});
            }
            c.Steps=list.ToArray();c.Validate();return c;
        }
        public void PreviewTab(int tab) {tabs.SelectedIndex=tab;}
    }
}
