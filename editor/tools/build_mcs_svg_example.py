"""Compose original MCS SVG objects at native dimensions; no device writes."""
from pathlib import Path
import copy,hashlib,json,re,uuid,zipfile,xml.etree.ElementTree as E,urllib.request
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'examples/mcs-ahu';OUT.mkdir(parents=True,exist_ok=True)
JAR=Path('C:/MCSconfigurator_Windows_03_03_2026/MCSconfigurator/modules/kitPxMcs4svg-wb.jar')
NS='http://www.w3.org/2000/svg';E.register_namespace('',NS)
def el(tag,attrs=None,text=None):
 n=E.Element('{'+NS+'}'+tag,{str(k):str(v) for k,v in (attrs or {}).items()});n.text=text;return n
svg=el('svg',dict(viewBox='0 0 1200 700',width=1200,height=700))
svg.append(el('rect',dict(width=1200,height=700,fill='#f1f6f3')))
svg.append(el('rect',dict(x=24,y=24,width=1152,height=95,rx=16,fill='#173e34')))
svg.append(el('text',dict(x=52,y=66,fill='#ffffff',**{'font-family':'Arial','font-size':25,'font-weight':600}),'Каскадное регулирование нагрева'))
svg.append(el('text',dict(x=52,y=94,fill='#b9d7c8',**{'font-family':'Arial','font-size':13}),'MCS SEDONA  /  192.0.2.10  /  app/sim/cascade'))
svg.append(el('rect',dict(x=24,y=140,width=1152,height=214,rx=16,fill='#ffffff',stroke='#d3e1d8')))
svg.append(el('text',dict(x=50,y=171,fill='#5b7466',**{'font-family':'Arial','font-size':12}),'Библиотечные SVG — статическая иллюстрация тракта. Значения ниже связаны с моделью Sedona.'))
placements=[
 ('inlet','Ductwork/Duct_Horz_End_Left.svg',50,228),
 ('damper','Dampers/Damper_Horz_Parallel/Damper_Horz_Parallel_0.svg',170,228),
 ('duct1','Ductwork/Duct_Horz_Short.svg',250,228),
 ('filter','Filters/Filter_Double/Filter_Double_0.svg',420,228),
 ('duct2','Ductwork/Duct_Horz_Short.svg',500,228),
 ('heater','Coils/Coils_Heating/Coils_Heating_0.svg',670,228),
 ('duct3','Ductwork/Duct_Horz_Short.svg',750,228),
 ('fan','Fans/Fans_Horz/Fans_Horz_Right/Fans_Horz_Right_Off.svg',920,228),
 ('outlet','Ductwork/Duct_Horz_End_Right.svg',1050,228)]
records=[];assets=OUT/'assets';assets.mkdir(exist_ok=True)
with zipfile.ZipFile(JAR) as z:
 for key,resource,x,y in placements:
  raw=z.read(resource);(assets/(key+'.svg')).write_bytes(raw)
  n=E.fromstring(raw);vb=list(map(float,n.attrib['viewBox'].split()));width,height=vb[2:]
  # Prefix every local ID and its references when combining separate documents.
  ids={item.attrib['id']:key+'_'+item.attrib['id'] for item in n.iter() if 'id' in item.attrib}
  for item in n.iter():
   for attr,value in list(item.attrib.items()):
    if attr=='id':item.set(attr,ids[value]);continue
    value=re.sub(r'url\(\s*[\'"]?#([^\)\'" ]+)[\'"]?\s*\)',lambda m:'url(#'+ids.get(m[1],m[1])+')',value)
    if attr.endswith('href') and value.startswith('#'):value='#'+ids.get(value[1:],value[1:])
    item.set(attr,value)
  n.set('x',str(x));n.set('y',str(y));n.set('width',str(width));n.set('height',str(height));svg.append(n)
  records.append(dict(key=key,resource=resource,sha256=hashlib.sha256(raw).hexdigest(),x=x,y=y,width=width,height=height,scale=1))
for x,title in [(175,'ЗАСЛОНКА'),(424,'ФИЛЬТР'),(674,'НАГРЕВАТЕЛЬ'),(924,'ВЕНТИЛЯТОР')]:
 svg.append(el('text',dict(x=x,y=209,fill='#587465',**{'font-family':'Arial','font-size':10,'letter-spacing':'1'}),title))
svg.append(el('text',dict(x=50,y=385,fill='#234f3c',**{'font-family':'Arial','font-size':15,'font-weight':600}),'Параметры каскада · привязки к реальным слотам модели'))
SVG=E.tostring(svg,encoding='unicode');(OUT/'mcs-cascade.svg').write_text(SVG,encoding='utf-8')
device=json.loads((OUT/'device-graph.json').read_text(encoding='utf-8'));nodes={n['name']:n for n in device['nodes'] if n['parent']==40};nodes['.']=next(n for n in device['nodes'] if n['id']==40)
specs=[('value','valve','out','Положение клапана','доля 0–1',3,5,58),('value','coil','airOut','Воздух после нагрева','°C',2,29,58),('value','room','out','Температура помещения','°C',2,53,58),('indicator','.','run','Расчёт модели','',0,77,58),('value','water','out','Температура воды','°C',1,5,76),('value','master','out','Задание на нагрев','°C',2,29,76),('setpoint','setpt','after','Цель после шага','°C',1,53,76),('indicator','master','good','PID: признак good','',0,77,76)]
widgets=[];bindings=[]
for i,(kind,path,slot,label,unit,decimals,x,y) in enumerate(specs):
 node=nodes[path];port=next(p for p in node['ports'] if p['name']==slot)
 if kind=='setpoint':assert not port['readonly']
 widgets.append(dict(id='mcs-widget-'+str(i+1),kind=kind,path=path,slot=slot,type=port['type'],label=label,unit=unit,decimals=decimals,x=x,y=y))
 bindings.append(dict(label=label,path='app/sim/cascade'+('/'+path if path!='.' else ''),slot=slot,type=port['type'],kind=kind,verifiedFrom='device snapshot',readOnlyDuringAcceptance=True))
doc=dict(format='mcs-sedona-ui',version=1,id='mcs-svg-cascade-example-v1',title='Пример MCS · каскад нагрева',views=[dict(id='mcs-cascade-view',title='MCS · каскад нагрева',folder='app/sim/cascade',svg=SVG,widgets=widgets)])
(OUT/'mcs-cascade.ui.json').write_text(json.dumps(doc,ensure_ascii=False,indent=2),encoding='utf-8')
graph=copy.deepcopy(device)
for i,n in enumerate(graph['nodes']):
 num=n['id'];n['id']='n'+str(num);m=int(graph['values'].get(n['id'],{}).get('meta',0));n.update(x=((m>>24)&255)*16,y=((m>>16)&255)*16,coordinateSource='meta')
for i,e in enumerate(graph['edges']):e.update(id='real'+str(i),source='n'+str(e['source']),target='n'+str(e['target']))
graph.update(ui=doc,synthetic=False)
project=dict(format='mcs-sedona-project',version=1,title='Пример MCS · снимок .184',graph=graph)
(OUT/'mcs-cascade.sedona.json').write_text(json.dumps(project,ensure_ascii=False,indent=2),encoding='utf-8')
report=dict(sourceJar=str(JAR),jarSha256=hashlib.sha256(JAR.read_bytes()).hexdigest(),sourceSvgCount=973,objects=records,bindings=bindings,packageBytes=len(json.dumps(doc,ensure_ascii=False).encode()),deviceWrites=False,staticEquipmentNotStateBindings=True)
(OUT/'sources-and-bindings.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
req=urllib.request.Request('http://127.0.0.1:8098/api/ui/save',data=json.dumps(dict(document=doc),ensure_ascii=False).encode(),headers={'Content-Type':'application/json'})
print(json.load(urllib.request.urlopen(req,timeout=20)));print('Example package bytes:',report['packageBytes'])
