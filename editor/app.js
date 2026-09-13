const icons={"":"·",home:"⌂",work:"☷",media:"♪",settings:"⚙",terminal:">_",list:"☷",copy:"▣",paste:"▤",undo:"↶",switch:"⇄",snip:"✂",notes:"▧",file:"▧",save:"◆",play:"▶",next:"▶|",back:"|◀",lock:"⌁",audio:"♪",upload:"↑",download:"↓",refresh:"↻"};
const colors={cyan:"#22d3ee",violet:"#a78bfa",amber:"#f59e0b",green:"#22c55e",blue:"#3b82f6",rose:"#f43f5e"};
const fallback={version:2,platform:"windows",grid:{cols:12,rows:4},brightness:80,screensaver_sec:180,pages:[{id:"home",title:"Home",icon:"home",buttons:[]}]};
let config=structuredClone(fallback),pageIndex=0,widgetIndex=-1;

const $=id=>document.getElementById(id);
const pageList=$("pageList"),grid=$("widgetGrid"),nav=$("previewNav"),form=$("widgetForm");

function pos(widget,index){
  const p=widget.position||{col:index%12,row:Math.floor(index/12),col_span:1,row_span:1};
  return {col:+p.col||0,row:+p.row||0,col_span:+p.col_span||1,row_span:+p.row_span||1};
}

function layoutErrors(page){
  const occupied=new Set(),errors=[];
  page.buttons.forEach((widget,index)=>{
    const p=pos(widget,index);
    if(p.col<0||p.row<0||p.col+p.col_span>12||p.row+p.row_span>4){errors.push(`${widget.label}: outside grid`);return;}
    for(let row=p.row;row<p.row+p.row_span;row++)for(let col=p.col;col<p.col+p.col_span;col++){
      const key=`${col}:${row}`;if(occupied.has(key))errors.push(`${widget.label}: overlaps another card`);occupied.add(key);
    }
  });
  return errors;
}

function actionErrors(action,label,allowMacro=true){
  if(!action||!Object.keys(action).length)return [];
  const errors=[],prefix=label||"Widget",type=action.type;
  if(!["hid_keys","hid_consumer","text","macro","agent","goto_page","delay","brightness"].includes(type))return [`${prefix}: unknown action type`];
  if(type==="hid_keys"&&(!Array.isArray(action.keys)||!action.keys.length||action.keys.length>4))errors.push(`${prefix}: keys must contain 1–4 entries`);
  if(type==="hid_consumer"&&!action.key)errors.push(`${prefix}: consumer key is missing`);
  if(type==="text"&&typeof action.text!=="string")errors.push(`${prefix}: text is missing`);
  if(type==="agent"&&!action.command)errors.push(`${prefix}: agent command is missing`);
  if(type==="goto_page"&&!config.pages.some(page=>page.id===action.page))errors.push(`${prefix}: target page does not exist`);
  if(type==="delay"&&(!Number.isFinite(action.ms)||action.ms<0))errors.push(`${prefix}: delay is invalid`);
  if(type==="brightness"&&(!Number.isInteger(action.percent)||action.percent<1||action.percent>100))errors.push(`${prefix}: brightness must be 1–100`);
  if(type==="macro"){
    if(!allowMacro||!Array.isArray(action.steps)||!action.steps.length||action.steps.length>8)errors.push(`${prefix}: macro must contain 1–8 non-nested steps`);
    else action.steps.forEach(step=>errors.push(...actionErrors(step,prefix,false)));
  }
  return errors;
}

function pageErrors(page){
  const errors=layoutErrors(page);
  page.buttons.forEach(widget=>{
    const hasAction=widget.action&&Object.keys(widget.action).length;
    const hasLongPress=widget.long_press&&Object.keys(widget.long_press).length;
    if((widget.label||"").length>23)errors.push(`${widget.label}: label is too long`);
    if(!widget.tile&&!widget.slider&&!hasAction&&!hasLongPress)errors.push(`${widget.label}: no action or data source`);
    if(widget.tile&&!['cpu','mem','disk'].includes(widget.tile))errors.push(`${widget.label}: unknown telemetry tile`);
    errors.push(...actionErrors(widget.action,widget.label),...actionErrors(widget.long_press,widget.label));
  });
  return errors;
}

function validate(page){
  const errors=pageErrors(page),state=$("validationState");state.textContent=errors[0]||"Config valid";state.className=`state ${errors.length?"error":"ok"}`;
  return errors;
}

function render(){
  const page=config.pages[pageIndex];
  if(!page)return;
  pageList.replaceChildren(...config.pages.map((item,index)=>{
    const button=document.createElement("button");button.className=`page-item ${index===pageIndex?"active":""}`;
    button.textContent=`${icons[item.icon]||"·"}  ${item.title}`;button.onclick=()=>{pageIndex=index;widgetIndex=-1;render();};return button;
  }));
  $("pageName").textContent=page.title;$("previewTitle").textContent=page.title;
  grid.replaceChildren(...page.buttons.map((widget,index)=>renderWidget(widget,index)));
  nav.replaceChildren(...config.pages.slice(0,4).map((item,index)=>{
    const button=document.createElement("button");button.className=`nav-item ${index===pageIndex?"active":""}`;
    button.textContent=`${icons[item.icon]||"·"}  ${item.title}`;button.onclick=()=>{pageIndex=index;widgetIndex=-1;render();};return button;
  }));
  validate(page);renderInspector();
}

function renderWidget(widget,index){
  const p=pos(widget,index),card=document.createElement("div");
  card.className=`widget ${widget.variant||""} ${widget.tile?"tile":""} ${index===widgetIndex?"selected":""}`;
  card.style.setProperty("--accent",colors[widget.color]||widget.color||colors.cyan);
  card.style.gridColumn=`${p.col+1} / span ${p.col_span}`;card.style.gridRow=`${p.row+1} / span ${p.row_span}`;
  card.draggable=true;card.dataset.index=index;
  const icon=document.createElement("span");icon.className="widget-icon";icon.textContent=icons[widget.icon]||"";card.append(icon);
  const label=document.createElement("span");label.className="widget-label";label.textContent=widget.label||"Untitled";card.append(label);
  if(widget.tile){const value=document.createElement("span");value.className="metric";value.textContent="--";card.append(value);}
  const hint=document.createElement("span");hint.className="widget-hint";hint.textContent=widget.hint||"";card.append(hint);
  card.onclick=()=>{widgetIndex=index;render();};
  card.ondragstart=event=>event.dataTransfer.setData("text/plain",String(index));
  return card;
}

grid.ondragover=event=>event.preventDefault();
grid.ondrop=event=>{
  event.preventDefault();const index=Number(event.dataTransfer.getData("text/plain"));if(!Number.isInteger(index))return;
  const rect=grid.getBoundingClientRect(),widget=config.pages[pageIndex].buttons[index],p=pos(widget,index);
  p.col=Math.max(0,Math.min(12-p.col_span,Math.floor((event.clientX-rect.left)/rect.width*12)));
  p.row=Math.max(0,Math.min(4-p.row_span,Math.floor((event.clientY-rect.top)/rect.height*4)));
  widget.position=p;widgetIndex=index;render();
};

function renderInspector(){
  const widget=config.pages[pageIndex].buttons[widgetIndex];
  $("emptyInspector").hidden=!!widget;form.hidden=!widget;if(!widget)return;
  for(const name of ["label","hint","icon","color","variant"]){form.elements[name].value=widget[name]||"";}
  const p=pos(widget,widgetIndex);for(const name of ["col","row","col_span","row_span"])form.elements[name].value=p[name];
  form.elements.action.value=JSON.stringify(widget.action||{},null,2);$("formError").textContent="";
  form.elements.long_press.value=JSON.stringify(widget.long_press||{},null,2);
}

function updateSelected(){
  const widget=config.pages[pageIndex].buttons[widgetIndex];if(!widget)return;
  for(const name of ["label","hint","icon","color","variant"]){const value=form.elements[name].value;if(value)widget[name]=value;else delete widget[name];}
  widget.position={};for(const name of ["col","row","col_span","row_span"])widget.position[name]=Number(form.elements[name].value);
  try{
    const action=JSON.parse(form.elements.action.value||"{}"),longPress=JSON.parse(form.elements.long_press.value||"{}");
    if(Object.keys(action).length)widget.action=action;else delete widget.action;
    if(Object.keys(longPress).length)widget.long_press=longPress;else delete widget.long_press;
    $("formError").textContent="";
  }catch(error){$("formError").textContent="Action fields must contain valid JSON";return;}
  render();
}

for(const name of ["label","hint","icon","color","variant","col","row","col_span","row_span"]){form.elements[name].addEventListener("change",updateSelected);}
form.elements.action.addEventListener("change",updateSelected);
form.elements.long_press.addEventListener("change",updateSelected);

$("addWidget").onclick=()=>{
  const page=config.pages[pageIndex];if(page.buttons.length>=20){$("fileStatus").textContent="A page can contain at most 20 widgets";return;}page.buttons.push({label:"New action",icon:"list",hint:"Add shortcut",variant:"compact",color:"cyan",position:{col:0,row:0,col_span:3,row_span:1},action:{type:"hid_keys",keys:["PRIMARY","A"]}});widgetIndex=page.buttons.length-1;render();
};
$("duplicateWidget").onclick=()=>{const page=config.pages[pageIndex];if(page.buttons.length>=20){$("fileStatus").textContent="A page can contain at most 20 widgets";return;}const copy=structuredClone(page.buttons[widgetIndex]);copy.label=`${copy.label} copy`.slice(0,23);copy.position.col=Math.min(12-copy.position.col_span,copy.position.col+1);page.buttons.push(copy);widgetIndex=page.buttons.length-1;render();};
$("deleteWidget").onclick=()=>{config.pages[pageIndex].buttons.splice(widgetIndex,1);widgetIndex=-1;render();};
$("addPage").onclick=()=>{if(config.pages.length>=8){$("fileStatus").textContent="The firmware supports at most 8 pages";return;}const n=config.pages.length+1;config.pages.push({id:`page${n}`,title:`Page ${n}`,icon:"list",grid:{cols:12,rows:4},buttons:[]});pageIndex=config.pages.length-1;widgetIndex=-1;render();};

const iconSelect=form.elements.icon;for(const [value,symbol] of Object.entries(icons)){const option=document.createElement("option");option.value=value;option.textContent=`${symbol} ${value||"none"}`;iconSelect.append(option);}
const colorSelect=form.elements.color;for(const value of Object.keys(colors)){const option=document.createElement("option");option.value=value;option.textContent=value;colorSelect.append(option);}

$("openButton").onclick=()=>$("fileInput").click();
$("fileInput").onchange=async event=>{try{config=JSON.parse(await event.target.files[0].text());pageIndex=0;widgetIndex=-1;$("fileStatus").textContent=`Opened ${event.target.files[0].name}`;render();}catch(error){$("fileStatus").textContent=`Could not open file: ${error.message}`;}};
$("downloadButton").onclick=()=>{const ids=config.pages.map(page=>page.id),duplicateId=ids.find((id,index)=>!id||ids.indexOf(id)!==index),invalid=config.pages.find(page=>pageErrors(page).length);if(duplicateId!==undefined){$("fileStatus").textContent="Every page needs a unique id";return;}if(invalid){$("fileStatus").textContent=`Fix config errors on ${invalid.title}`;return;}const blob=new Blob([JSON.stringify(config,null,2)+"\n"],{type:"application/json"}),url=URL.createObjectURL(blob),link=document.createElement("a");link.href=url;link.download="config.json";link.click();URL.revokeObjectURL(url);$("fileStatus").textContent="Downloaded config.json — ready for agent upload";};

fetch("../storage/config.json").then(response=>{if(!response.ok)throw new Error();return response.json();}).then(data=>{config=data;render();}).catch(()=>render());
