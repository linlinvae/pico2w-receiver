// SPDX-License-Identifier: Apache-2.0
using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
namespace PicoControl {
    static class ConfigTests {
        static int count;
        static void Check(bool pass,string name) {count++;if(!pass)throw new Exception("配置自测失败："+name);}
        static void Reject(Action f,string name) {bool rejected=false;try {f();}catch(ArgumentException){rejected=true;}Check(rejected,name);}
        static void Fail(Action f,string name) {bool rejected=false;try {f();}catch(InvalidOperationException){rejected=true;}Check(rejected,name);}
        public static string Run() {
            count=0;
            byte[] text=System.Text.Encoding.ASCII.GetBytes("123456789");Check(ReceiverConfig.Crc32(text,9)==0xcbf43926 && ReceiverConfig.Crc16(text,9)==0x29b1,"CRC vectors");
            byte[] vectors=new byte[256];
            using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("PicoControl.ConfigVectors")) {
                if(stream==null || stream.Read(vectors,0,256)!=256)throw new Exception("缺少固件配置互操作向量。");
            }
            var expected=new byte[128];Array.Copy(vectors,expected,128);
            Check(ReceiverConfig.Equal(expected,new ReceiverConfig().Encode()),"C firmware defaults match C#");
            Array.Copy(vectors,128,expected,0,128);var sample=ReceiverConfig.Decode(expected);
            Check(sample.Map[11]==14 && sample.LeftDeadzone==12 && sample.RightDeadzone==7 && sample.Invert==5 && sample.Strong==60 && sample.Trigger==12 && sample.Steps.Length==2,"C firmware sample decoded");
            Check(sample.Steps[0].Buttons==4096 && sample.Steps[0].RT==25 && sample.Steps[1].Duration==80 && sample.Steps[1].LT==10,"C macro bytes");
            Check(ReceiverConfig.Equal(expected,sample.Encode()),"C firmware sample reencoded byte for byte");
            for(int i=0;i<128;i++) {var bad=(byte[])expected.Clone();bad[i]^=1;Reject(delegate {ReceiverConfig.Decode(bad);},"corruption "+i);}
            Reject(delegate {new ReceiverConfig {LeftDeadzone=51}.Encode();},"deadzone range");
            Reject(delegate {new ReceiverConfig {Strong=101}.Encode();},"rumble range");
            Reject(delegate {new ReceiverConfig {Steps=new MacroStep[]{new MacroStep {Duration=100}}}.Encode();},"macro trigger required");
            Reject(delegate {new ReceiverConfig {Trigger=0,Steps=new MacroStep[]{new MacroStep {Duration=19}}}.Encode();},"duration minimum");
            Reject(delegate {ReceiverConfig.ParseButtons("A+Unknown");},"unknown macro buttons");
            Check(ReceiverConfig.ParseButtons("a+B+↑")==0x3001 && ReceiverConfig.ParseButtons("释放")==0,"macro parsing");
            Check(ReceiverConfig.FormatButtons(0x3001)=="↑+A+B","macro formatting");
            var p=new SimulatedFeatures();var client=new ConfigClient(p,delegate {return true;});
            Check(ReceiverConfig.Equal(new ReceiverConfig().Encode(),client.Read().Encode()),"read in seven chunks");
            client.Apply(sample);Check(ReceiverConfig.Equal(sample.Encode(),p.Current),"atomic apply and readback");
            Check(p.Commands.Contains(3) && p.Commands.Contains(5) && !p.Commands.Contains(6),"apply does not save");
            client.Save(sample);Check(p.Saves==1 && client.Generation==1,"save acknowledgment");
            p.Connected=true;int writes=p.Commands.Count;Fail(delegate {client.Save(sample);},"save rejects connected pad before write");Check(writes==p.Commands.Count,"connected save sends no commands");
            p=new SimulatedFeatures {RejectPut=true};client=new ConfigClient(p,delegate {return true;});Fail(delegate {client.Apply(sample);},"PUT error");Check(!p.Commands.Contains(5) && p.Commands.Contains(8) && ReceiverConfig.Equal(p.Current,new ReceiverConfig().Encode()),"failed upload cancelled without apply");
            p=new SimulatedFeatures {MismatchReadback=true};client=new ConfigClient(p,delegate {return true;});Fail(delegate {client.Apply(sample);},"readback mismatch");Check(p.Saves==0,"mismatch never saves");
            p=new SimulatedFeatures {SaveStatus=9};client=new ConfigClient(p,delegate {return true;});Fail(delegate {client.Save(sample);},"firmware pending pairing rejects save");
            p=new SimulatedFeatures {SaveStatus=5};client=new ConfigClient(p,delegate {return true;});Fail(delegate {client.Save(sample);},"flash IO failure not success");
            p=new SimulatedFeatures();client=new ConfigClient(p,delegate {return p.Commands.Count<3;});bool cancelled=false;try {client.Apply(sample);}catch(OperationCanceledException) {cancelled=true;}Check(cancelled && !p.Commands.Contains(5),"selection change cancels before apply");
            var q=ConfigClient.Packet(4,0x1234,19,new byte[]{8,9},2);Check(q[0]==2 && q[6]==0x34 && q[7]==0x12 && q[8]==19 && q[9]==2 && q[11]==8,"packet offsets");
            var r=(byte[])q.Clone();r[0]=3;Check(ConfigClient.Response(r,q),"valid correlated reply");
            r[6]^=1;Check(!ConfigClient.Response(r,q),"wrong token rejected");r=(byte[])q.Clone();r[0]=3;r[8]++;Check(!ConfigClient.Response(r,q),"wrong offset rejected");r=(byte[])q.Clone();r[0]=3;r[20]^=1;Check(!ConfigClient.Response(r,q),"corrupt reply rejected");
            return count.ToString();
        }
        sealed class SimulatedFeatures : IFeaturePort {
            public byte[] Current=new ReceiverConfig().Encode();byte[] staging,response;
            public List<byte> Commands=new List<byte>();
            public bool Connected,RejectPut,MismatchReadback;public byte SaveStatus;public uint Saves;
            public bool Set(byte[] q) {
                Commands.Add(q[5]);response=new byte[32];Array.Copy(q,response,11);response[0]=3;response[9]=0;response[10]=0;
                switch(q[5]) {
                    case 1: Info();break;
                    case 2: response[9]=q[9];Array.Copy(MismatchReadback?new ReceiverConfig().Encode():Current,q[8],response,11,q[9]);break;
                    case 3: staging=new byte[128];break;
                    case 4: if(RejectPut)response[10]=4;else Array.Copy(q,11,staging,q[8],q[9]);break;
                    case 5: Current=ReceiverConfig.Decode(staging).Encode();staging=null;break;
                    case 6: response[10]=SaveStatus;if(q[9]!=4 || !ReceiverConfig.Equal(new byte[]{q[11],q[12],q[13],q[14]},new byte[]{Current[124],Current[125],Current[126],Current[127]}))throw new Exception("SAVE not bound to readback CRC");if(SaveStatus==0)Saves++;Info();break;
                    case 8: staging=null;break;
                    default: return false;
                }
                var p=new byte[31];Array.Copy(response,1,p,0,31);ReceiverConfig.Put16(response,30,ReceiverConfig.Crc16(p,29));return true;
            }
            void Info() {response[9]=10;response[11]=1;response[12]=31;response[13]=128;response[14]=16;Array.Copy(BitConverter.GetBytes(Saves),0,response,15,4);}
            public bool Get(byte[] b) {if(b[0]!=3 || response==null)return false;Array.Copy(response,b,32);return true;}
            public ReceiverStatus Status() {return new ReceiverStatus {Kind=Connected?1:0};}
            public void Dispose() {}
        }
    }
}
