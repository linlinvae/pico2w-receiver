// SPDX-License-Identifier: Apache-2.0
using System;
using System.IO;
using System.Text;
using System.Windows.Forms;
using System.Runtime.InteropServices;
namespace PicoControl {
    static class ModernProgram {
        [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
        [STAThread] static int Main(string[] args) {
            try {
                if(args.Length>0 && args[0]=="--self-test") {File.WriteAllText(args[1],SelfTests.Run()+ModernTests.Run(),Encoding.UTF8);return 0;}
                if(args.Length>0 && args[0]=="--probe") {return RunProbe(args);}
                SetProcessDPIAware();Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
                bool demo=args.Length>0 && (args[0]=="--preview" || args[0]=="--demo");
                string output=args.Length>1 && args[0]=="--preview"?Path.GetFullPath(args[1]):null;
                using(var form=new ModernForm(demo,output,args.Length>2 && args[2]=="small"))Application.Run(form);
                return ModernForm.PreviewFailed?1:0;
            } catch(Exception e) {
                if(args.Length>1 && args[0]=="--self-test")File.WriteAllText(args[1],"FAILED: "+e,Encoding.UTF8);
                else MessageBox.Show("新版界面无法启动："+e.Message+"\r\n请确认电脑已安装 Microsoft Edge WebView2 Runtime。旧版 V0.2 仍可使用。","Pico Control",MessageBoxButtons.OK,MessageBoxIcon.Error);
                return 1;
            }
        }
        static int RunProbe(string[] args) {
            var b=new StringBuilder("Pico Control V0.3 · 只读检查\r\n");
            var ds=ReceiverReader.ReadAll();foreach(var d in ds)b.AppendLine(d+"："+(d.Status==null?d.Error:d.Status.PadName+" / "+d.Status.Battery));
            var input=new WindowsXInput();for(int i=0;i<4;i++) {PadState s;b.AppendLine("槽位 "+i+"："+(input.Read(i,out s)==0?"可用":"未连接"));}
            if(args.Length>1)File.WriteAllText(args[1],b.ToString(),Encoding.UTF8);return 0;
        }
    }
}
