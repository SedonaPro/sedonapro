from pathlib import Path
import zipfile,json,xml.etree.ElementTree as E,hashlib
r=Path(__file__).resolve().parents[1];jar=Path('C:/MCSconfigurator_Windows_03_03_2026/MCSconfigurator/modules/kitPxMcs4svg-wb.jar')
E.register_namespace('','http://www.w3.org/2000/svg');entries=[];sources=[]
specs=[('fan-right','Вентилятор →',{'off':'Fans/Fans_Horz/Fans_Horz_Right/Fans_Horz_Right_Off.svg','on':'Fans/Fans_Horz/Fans_Horz_Right/Fans_Horz_Right_On.svg'}),('damper','Заслонка 0–100%',{k:f'Dampers/Damper_Horz_Parallel/Damper_Horz_Parallel_{i}.svg' for i,k in enumerate(['off','p25','p50','p75','on'])}),('heater','Нагреватель 0–100%',{k:f'Coils/Coils_Heating/Coils_Heating_{i}.svg' for i,k in enumerate(['off','p25','p50','p75','on'])}),('filter','Фильтр',{'off':'Filters/Filter_Double/Filter_Double_0.svg'}),('duct-short','Воздуховод 170',{'off':'Ductwork/Duct_Horz_Short.svg'}),('duct-inlet','Вход канала',{'off':'Ductwork/Duct_Horz_End_Left.svg'}),('duct-outlet','Выход канала',{'off':'Ductwork/Duct_Horz_End_Right.svg'})]
with zipfile.ZipFile(jar) as z:
 for ident,title,states in specs:
  asset=dict(id='mcs-'+ident,title=title,states={});dims=None
  for state,resource in states.items():
   raw=z.read(resource);root=E.fromstring(raw);vb=list(map(float,root.attrib['viewBox'].split()));size=vb[2:];assert dims is None or dims==size;dims=size
   for n in root.iter():
    if n.tag.endswith('animateTransform'):
     keep={k:v for k,v in n.attrib.items() if k in ['type','attributeName','from','to','dur','repeatCount']};n.attrib.clear();n.attrib.update(keep)
   asset['states'][state]=E.tostring(root,encoding='unicode');sources.append(dict(assetId=asset['id'],state=state,resource=resource,sha256=hashlib.sha256(raw).hexdigest()))
  asset.update(width=dims[0],height=dims[1]);entries.append(asset)
doc=dict(format='mcs-svg-library',version=1,assets=entries)
(r/'web/src/svgLibrary.json').write_text(json.dumps(doc,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
(r/'examples/svg-library').mkdir(parents=True,exist_ok=True)
(r/'examples/svg-library/mcs-base-library.json').write_text(json.dumps(doc,ensure_ascii=False,indent=2),encoding='utf-8')
(r/'examples/svg-library/sources.json').write_text(json.dumps(dict(jar=str(jar),jarSha256=hashlib.sha256(jar.read_bytes()).hexdigest(),sources=sources),indent=2),encoding='utf-8')
print('Library assets:',len(entries),'bytes:',len(json.dumps(doc,ensure_ascii=False).encode()))
