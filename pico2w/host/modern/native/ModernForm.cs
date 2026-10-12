// SPDX-License-Identifier: Apache-2.0
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Web.WebView2.Core;
using Microsoft.Web.WebView2.WinForms;
namespace PicoControl {
    public sealed class WebRequest {public int id;public string command;public Dictionary<string,object> args;}
    public sealed class ModernForm : Form {
        readonly WebView2 web=new WebView2();readonly JavaScriptSerializer json=new JavaScriptSerializer {MaxJsonLength=65536};
        readonly bool demo;readonly string output;readonly Timer timer=new Timer();
        readonly IXInput input;readonly RumbleSession rumble;readonly Stopwatch clock=Stopwatch.StartNew();
        List<ReceiverDevice> devices=new List<ReceiverDevice>();string selected="",configPath="",padIdentity="";
        int slot=-1;bool confirmed,polling,busy,ready;volatile bool closing;volatile int epoch;
        long lastPoll,lastSend;PadState state;bool inputReady;string statusError="";ReceiverConfig demoConfig=new ReceiverConfig();
        public static bool PreviewFailed;
        public ModernForm(bool preview,string previews,bool small) {
            demo=preview;output=previews;input=preview?(IXInput)new FakeXInput():new WindowsXInput();rumble=new RumbleSession(input);
            Text="Pico Control · V0.3";ClientSize=small?new Size(1060,740):new Size(1480,920);MinimumSize=new Size(1060,740);StartPosition=FormStartPosition.CenterScreen;
            BackColor=Color.FromArgb(247,248,250);web.Dock=DockStyle.Fill;Controls.Add(web);
            if(previews!=null) {ShowInTaskbar=false;StartPosition=FormStartPosition.Manual;Location=new Point(-3000,-3000);}
            if(demo) {devices.Add(new ReceiverDevice {Path="demo",Serial="P2W-2401 · 示例",Status=new ReceiverStatus {Kind=2,Valid=true,Source=2,Level=3,Interval=2,Age=12000}});selected="demo";slot=0;state=new PadState {Packet=1284,Pad=new Gamepad {Buttons=0x1001,LT=72,RT=184,LX=12200,LY=16000,RX=-6800,RY=-3200}};inputReady=true;}
            timer.Interval=32;timer.Tick+=Tick;
            Shown+=async delegate {try {await Initialize();}catch(Exception e) {if(output!=null) {Directory.CreateDirectory(output);File.WriteAllText(Path.Combine(output,"FAILED.txt"),e.ToString());PreviewFailed=true;Close();}else {MessageBox.Show(this,"启动失败："+e.Message+"\r\n请安装 WebView2 Runtime，或继续使用 V0.2。","Pico Control");Close();}}};
            Deactivate+=delegate {rumble.Stop();};
            FormClosing+=delegate {closing=true;epoch++;timer.Stop();rumble.Stop();};
            FormClosed+=delegate {timer.Dispose();rumble.Dispose();web.Dispose();};
        }
        async Task Initialize() {
            string assets=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"ui");if(!File.Exists(Path.Combine(assets,"index.html")))throw new FileNotFoundException("缺少本地界面文件，请完整解压发行包。");
            string profile=demo?Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"preview-profile"):Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"PicoControl","WebView");
            var environment=await CoreWebView2Environment.CreateAsync(null,profile);await web.EnsureCoreWebView2Async(environment);
            var core=web.CoreWebView2;core.SetVirtualHostNameToFolderMapping("pico-control.local",assets,CoreWebView2HostResourceAccessKind.Deny);
            if(demo)await core.AddScriptToExecuteOnDocumentCreatedAsync("window.picoErrors=[];addEventListener('error',e=>window.picoErrors.push(e.message));addEventListener('unhandledrejection',e=>window.picoErrors.push(String(e.reason)));");
            core.Settings.AreDefaultContextMenusEnabled=false;core.Settings.AreDevToolsEnabled=demo;core.Settings.IsStatusBarEnabled=false;
            core.NavigationStarting+=delegate(object sender,CoreWebView2NavigationStartingEventArgs e) {if(!Allowed(e.Uri))e.Cancel=true;};
            core.NewWindowRequested+=delegate(object sender,CoreWebView2NewWindowRequestedEventArgs e){e.Handled=true;};
            core.PermissionRequested+=delegate(object sender,CoreWebView2PermissionRequestedEventArgs e){e.State=CoreWebView2PermissionState.Deny;};
            core.WebMessageReceived+=Message;
            core.NavigationCompleted+=delegate(object sender,CoreWebView2NavigationCompletedEventArgs e) {if(!e.IsSuccess) {statusError="界面加载失败："+e.WebErrorStatus;return;}ready=true;timer.Start();SendSnapshot();};
            ready=true;core.Navigate("https://pico-control.local/index.html");
        }
        public static bool Allowed(string source) {Uri u;return Uri.TryCreate(source,UriKind.Absolute,out u) && u.Scheme=="https" && u.Host=="pico-control.local" && u.IsDefaultPort;}
        ReceiverDevice Device {get {return devices.Find(delegate(ReceiverDevice d){return d.Path==selected;});}}
        void Post(object value) {if(ready && !closing && !IsDisposed)web.CoreWebView2.PostWebMessageAsJson(json.Serialize(value));}
        void Reply(int id,object value,Exception error) {Post(new {type="reply",id=id,ok=error==null,data=value,error=error==null?null:error.Message});SendSnapshot();}
        void SelectionChanged() {rumble.Stop();confirmed=false;configPath="";epoch++;}
        void Message(object sender,CoreWebView2WebMessageReceivedEventArgs e) {
            if(!Allowed(e.Source) || closing)return;
            WebRequest q=null;
            try {
                string text=e.WebMessageAsJson;if(text.Length>65536)throw new ArgumentException("请求太大。");q=json.Deserialize<WebRequest>(text);
                if(q==null || q.id<=0 || q.args==null)throw new ArgumentException("请求格式无效。");
                switch(q.command) {
                    case "hello":Reply(q.id,new {version="0.3",demo=demo},null);if(output!=null)BeginPreview();break;
                    case "selectReceiver":
                        if(busy)throw new InvalidOperationException("配置操作正在进行。");string path=Convert.ToString(q.args["path"]);
                        if(devices.Find(delegate(ReceiverDevice d){return d.Path==path;})==null)throw new ArgumentException("选定接收器已断开。");
                        if(selected!=path) {selected=path;SelectionChanged();}Reply(q.id,null,null);break;
                    case "selectSlot":
                        int target=Convert.ToInt32(q.args["slot"]);if(target< -1 || target>3)throw new ArgumentException("槽位无效。");
                        rumble.Stop();confirmed=false;slot=target;Reply(q.id,null,null);break;
                    case "confirmSlot":confirmed=Convert.ToBoolean(q.args["confirmed"]);Reply(q.id,null,null);break;
                    case "stop":rumble.Stop();Reply(q.id,null,null);break;
                    case "rumble":
                        if(busy)throw new InvalidOperationException("请先等待配置操作完成。");
                        var receiver=Device;PadState s;
                        if(!rumble.Start(slot,confirmed,receiver!=null && receiver.Status!=null && receiver.Status.Kind!=0,slot>=0 && (demo || input.Read(slot,out s)==0),clock.ElapsedMilliseconds,
                            Convert.ToUInt16(q.args["strong"]),Convert.ToUInt16(q.args["weak"])))throw new InvalidOperationException("请按键确认选定槽位与接收器对应，并勾选确认项。");
                        Reply(q.id,null,null);break;
                    case "readConfig":case "applyConfig":case "saveConfig":Config(q);break;
                    case "export":Export(q.id);break;
                    case "previewIdle":if(!demo)throw new ArgumentException("无效命令。");devices[0].Status.Kind=0;Reply(q.id,null,null);break;
                    default:throw new ArgumentException("不支持的操作。");
                }
            } catch(Exception ex) {if(q!=null)Reply(q.id,null,ex);}
        }
        void Config(WebRequest q) {
            if(busy)throw new InvalidOperationException("请等待当前配置操作完成。");
            var d=Device;if(d==null || d.Status==null)throw new InvalidOperationException("请先选择可用接收器。");
            if(q.command!="readConfig" && configPath!=d.Path)throw new InvalidOperationException("请先读取选定接收器的配置。");
            ReceiverConfig c=null;if(q.command!="readConfig") {c=json.Deserialize<ReceiverConfig>(json.Serialize(q.args["config"]));c.Validate();}
            if(q.command=="saveConfig" && d.Status.Kind!=0)throw new InvalidOperationException("请先关闭手柄，等接收器显示未连接再永久保存。");
            int captured=epoch;busy=true;rumble.Stop();SendSnapshot();var requested=c;
            Task.Factory.StartNew(delegate {
                object result=null;Exception error=null;
                try {
                    uint generation=0;
                    if(demo) {if(q.command=="readConfig")c=ReceiverConfig.Decode(demoConfig.Encode());else {demoConfig=ReceiverConfig.Decode(requested.Encode());c=demoConfig;}if(q.command=="saveConfig")generation=1;}
                    else using(var port=new HidConfigPort(d)) {var client=new ConfigClient(port,delegate {return !closing && epoch==captured;});if(q.command=="readConfig")c=client.Read();else if(q.command=="applyConfig")client.Apply(c);else client.Save(c);generation=client.Generation;}
                    result=new {config=c,generation=generation,saved=q.command=="saveConfig"};
                } catch(Exception ex) {error=ex;}
                if(closing)return;
                try {BeginInvoke((Action)delegate {busy=false;if(epoch!=captured) {configPath="";Reply(q.id,null,new OperationCanceledException("设备已变化，请重新读取。"));}
                    else {configPath=error==null && q.command!="saveConfig"?d.Path:"";Reply(q.id,result,error);}});}catch(InvalidOperationException) {}
            });
        }
        void Tick(object sender,EventArgs e) {
            if(closing || !ready)return;
            if(!demo && !polling && !busy && clock.ElapsedMilliseconds-lastPoll>=1000) {
                polling=true;lastPoll=clock.ElapsedMilliseconds;
                Task.Factory.StartNew(delegate {
                    List<ReceiverDevice> list=null;Exception error=null;try {list=ReceiverReader.ReadAll();}catch(Exception ex){error=ex;}
                    if(closing)return;
                    try {BeginInvoke((Action)delegate {polling=false;devices=list??new List<ReceiverDevice>();statusError=error==null?"":error.Message;
                        if(Device==null) {SelectionChanged();selected=devices.Count>0?devices[0].Path:"";}
                        var d=Device;string identity=d==null?"":d.Path+"|"+(d.Status==null?-1:d.Status.Kind);
                        if(identity!=padIdentity) {padIdentity=identity;confirmed=false;rumble.Stop();}SendSnapshot();});}catch(InvalidOperationException) {}
                });
            }
            if(!demo) {inputReady=slot>=0 && input.Read(slot,out state)==0;if(!inputReady) {state=new PadState();confirmed=false;rumble.Stop();}}
            var current=Device;rumble.Tick(clock.ElapsedMilliseconds,current!=null && current.Status!=null && current.Status.Kind!=0 && inputReady && confirmed);
            if(clock.ElapsedMilliseconds-lastSend>=40) {lastSend=clock.ElapsedMilliseconds;SendSnapshot();}
        }
        void SendSnapshot() {
            var list=new List<object>();foreach(var d in devices)list.Add(new {path=d.Path,serial=d.Serial,error=d.Error,status=d.Status==null?null:new {kind=d.Status.Kind,pad=d.Status.PadName,battery=d.Status.Battery,detail=d.Status.BatteryDetail,interval=d.Status.Interval}});
            var available=new List<int>();for(int i=0;i<4;i++) {PadState s;if(demo?i==0:input.Read(i,out s)==0)available.Add(i);}
            Post(new {type="snapshot",demo=demo,devices=list,selected=selected,slot=slot,confirmed=confirmed,available=available,inputReady=inputReady,state=state,busy=busy,configLoaded=configPath==selected && selected!="",rumbleActive=rumble.Active,error=statusError});
        }
        void Export(int id) {
            rumble.Stop();using(var dialog=new SaveFileDialog {Filter="文本文件|*.txt",FileName="pico-control-status.txt"}) {
                if(dialog.ShowDialog(this)!=DialogResult.OK) {Reply(id,new {cancelled=true},null);return;}
                var d=Device;var b=new StringBuilder("Pico Control V0.3\r\n");b.AppendLine("时间："+DateTimeOffset.Now.ToString("yyyy-MM-dd HH:mm:ss zzz"));b.AppendLine("模式："+(demo?"演示":"实时"));
                b.AppendLine("接收器："+(d==null?"未连接":d.ToString()));if(d!=null && d.Status!=null)b.AppendLine("手柄："+d.Status.PadName+"；电量："+d.Status.Battery+" / "+d.Status.BatteryDetail);
                b.AppendLine("槽位："+slot+"；已确认："+confirmed+"；输入可用："+inputReady);b.AppendLine(json.Serialize(state));
                File.WriteAllText(dialog.FileName,b.ToString(),Encoding.UTF8);Reply(id,new {file=dialog.FileName},null);
            }
        }
        bool previewStarted;
        async void BeginPreview() {
            if(previewStarted)return;previewStarted=true;
            try {
                Directory.CreateDirectory(output);await Task.Delay(700);
                await web.CoreWebView2.ExecuteScriptAsync("try { window.picoPreview.runChecks().then(r=>window.picoPreviewResult=r).catch(e=>window.picoPreviewResult='FAILED: '+e.message) } catch(e) {window.picoPreviewResult='FAILED: '+e.message}");
                string checks=null;
                for(int i=0;i<200;i++) {await Task.Delay(50);var raw=await web.CoreWebView2.ExecuteScriptAsync("window.picoPreviewResult || null");checks=json.Deserialize<string>(raw);if(checks!=null)break;}
                File.WriteAllText(Path.Combine(output,"ui-checks.json"),json.Serialize(checks));
                if(checks==null || !checks.StartsWith("PASS:")) {
                    File.WriteAllText(Path.Combine(output,"js-errors.json"),await web.CoreWebView2.ExecuteScriptAsync("window.picoErrors"));
                    using(var failed=File.Create(Path.Combine(output,"failed.png")))await web.CoreWebView2.CapturePreviewAsync(CoreWebView2CapturePreviewImageFormat.Png,failed);
                    throw new InvalidOperationException("界面操作检查失败："+checks);
                }
                foreach(string page in new string[]{"overview","input","rumble","mapping","analog","macro"}) {
                    await web.CoreWebView2.ExecuteScriptAsync("window.picoPreview.show('"+page+"')");await Task.Delay(230);
                    using(var file=File.Create(Path.Combine(output,page+".png")))await web.CoreWebView2.CapturePreviewAsync(CoreWebView2CapturePreviewImageFormat.Png,file);
                }
            } catch(Exception e) {PreviewFailed=true;File.WriteAllText(Path.Combine(output,"FAILED.txt"),e.ToString());}
            Close();
        }
    }
    static class ModernTests {
        public static string Run() {
            string[] denied={"http://pico-control.local/","https://example.com/","https://pico-control.local.evil/","https://pico-control.local:9999/","file:///C:/test","bad"};
            foreach(var s in denied)if(ModernForm.Allowed(s))throw new Exception("Native bridge accepts untrusted origin");
            if(!ModernForm.Allowed("https://pico-control.local/index.html"))throw new Exception("Local origin rejected");
            var serializer=new JavaScriptSerializer();var c=new ReceiverConfig {Trigger=12,Steps=new MacroStep[]{new MacroStep {Buttons=4096,Duration=100},new MacroStep {Duration=80}}};
            var copy=serializer.Deserialize<ReceiverConfig>(serializer.Serialize(c));if(!ReceiverConfig.Equal(c.Encode(),copy.Encode()))throw new Exception("JSON configuration mismatch");
            return "PASS: 8 desktop bridge checks. Origins, config JSON roundtrip.\r\n";
        }
    }
}
