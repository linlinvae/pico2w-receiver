export type MacroStep={Buttons:number;Duration:number;LT:number;RT:number}
export type Config={Invert:number;LeftDeadzone:number;RightDeadzone:number;Strong:number;Weak:number;Trigger:number;Map:number[];Steps:MacroStep[]}
export type Device={path:string;serial:string;error:string;status:null|{kind:number;pad:string;battery:string;detail:string;interval:number}}
export type Snapshot={type:'snapshot';demo:boolean;devices:Device[];selected:string;slot:number;confirmed:boolean;available:number[];inputReady:boolean;state:{Packet:number;Pad:{Buttons:number;LT:number;RT:number;LX:number;LY:number;RX:number;RY:number}};busy:boolean;configLoaded:boolean;rumbleActive:boolean;error:string}
type Reply={type:'reply';id:number;ok:boolean;data:unknown;error:string}
type WebView={postMessage:(message:unknown)=>void;addEventListener:(type:string,callback:(event:{data:Snapshot|Reply})=>void)=>void}
declare global{interface Window{chrome?:{webview:WebView};picoPreview?:{show:(page:string)=>void;runChecks:()=>Promise<string>}}}
const listeners=new Set<(snapshot:Snapshot)=>void>();let next=0
const pending=new Map<number,{resolve:(value:any)=>void;reject:(error:Error)=>void;timer:ReturnType<typeof setTimeout>}>()
window.chrome?.webview.addEventListener('message',({data})=>{
  if(data.type==='reply'){const p=pending.get(data.id);if(!p)return;clearTimeout(p.timer);pending.delete(data.id);data.ok?p.resolve(data.data):p.reject(new Error(data.error));}
  else listeners.forEach(fn=>fn(data))
})
export function subscribe(fn:(snapshot:Snapshot)=>void){listeners.add(fn);return()=>{listeners.delete(fn)}}
export function invoke<T=any>(command:string,args:Record<string,unknown>={}):Promise<T>{return new Promise((resolve,reject)=>{
  if(!window.chrome?.webview){reject(new Error('请从 PicoControl.exe 打开界面。'));return}
  const id=++next;const timer=setTimeout(()=>{pending.delete(id);reject(new Error('操作结果未确认，请等待接收器恢复后重读配置。'))},25000)
  pending.set(id,{resolve,reject,timer});window.chrome.webview.postMessage({id,command,args})
})}
export const names=['↑','↓','←','→','Start','Back','LS','RS','LB','RB','Guide','A','B','X','Y']
export const bits=[1,2,4,8,16,32,64,128,256,512,1024,4096,8192,16384,32768]
export function defaults():Config{return{Invert:0,LeftDeadzone:0,RightDeadzone:0,Strong:100,Weak:100,Trigger:255,Map:names.map((_,i)=>i),Steps:[]}}
export function parseButtons(text:string){if(!text.trim()||text.trim()==='释放')return 0;return text.split('+').reduce((mask,key)=>{const index=names.findIndex(n=>n.toLowerCase()===key.trim().toLowerCase());if(index<0)throw new Error(`未知宏按键：${key}。可写 A+B、LB、↑。`);return mask|bits[index]},0)}
export function formatButtons(mask:number){return names.filter((_,i)=>(mask&bits[i])!==0).join('+')}
export function validate(c:Config){
  const integer=(n:number,min:number,max:number)=>Number.isInteger(n)&&n>=min&&n<=max
  if(!integer(c.Invert,0,15)||!integer(c.LeftDeadzone,0,50)||!integer(c.RightDeadzone,0,50)||!integer(c.Strong,0,100)||!integer(c.Weak,0,100))throw new Error('死区为 0–50%，震动强度为 0–100%。')
  if(c.Map.length!==15||c.Map.some(v=>v!==255&&!integer(v,0,14)))throw new Error('按键映射无效。')
  if(c.Steps.length>16||(c.Trigger!==255&&!integer(c.Trigger,0,14))||(c.Trigger===255&&c.Steps.length))throw new Error('请为宏选择触发键，最多 16 步。')
  c.Steps.forEach((s,i)=>{if(!integer(s.Buttons,0,65535)||(s.Buttons&0x0800)||!integer(s.Duration,20,5000)||!integer(s.LT,0,255)||!integer(s.RT,0,255))throw new Error(`第 ${i+1} 步无效：保持 20–5000 毫秒，LT/RT 为 0–255。`)})
  if(c.Steps.reduce((total,s)=>total+s.Duration,0)>30000)throw new Error('宏总时长不能超过 30 秒。')
}
export const empty:Snapshot={type:'snapshot',demo:false,devices:[],selected:'',slot:-1,confirmed:false,available:[],inputReady:false,state:{Packet:0,Pad:{Buttons:0,LT:0,RT:0,LX:0,LY:0,RX:0,RY:0}},busy:false,configLoaded:false,rumbleActive:false,error:''}
