from pathlib import Path
import json,copy,xml.etree.ElementTree as E
r=Path(__file__).resolve().parents[1];out=r/'examples/svg-library';out.mkdir(parents=True,exist_ok=True)
old=json.loads((r/'examples/mcs-ahu/mcs-cascade.ui.json').read_text(encoding='utf-8'));doc=copy.deepcopy(old);doc.update(version=2,id='mcs-svg-scene-example-v1',title='Пример MCS · отдельные SVG',library=json.loads((r/'web/src/svgLibrary.json').read_text(encoding='utf-8'))['assets']);view=doc['views'][0];view.update(id='mcs-svg-scene-view',title='MCS · управляемые SVG')
root=E.fromstring(view['svg']);E.register_namespace('','http://www.w3.org/2000/svg')
for node in list(root):
 if node.tag.split('}')[-1]=='svg':root.remove(node)
for node in root.iter():
 if node.text and 'Библиотечные SVG' in node.text:node.text='Отдельные объекты MCS. Демонстрация состояний по модели cascade, не физическая установка.'
view['svg']=E.tostring(root,encoding='unicode');objects=[]
records=json.loads((r/'examples/mcs-ahu/sources-and-bindings.json').read_text(encoding='utf-8'))['objects']
mapping={'inlet':'mcs-duct-inlet','damper':'mcs-damper','duct1':'mcs-duct-short','filter':'mcs-filter','duct2':'mcs-duct-short','heater':'mcs-heater','duct3':'mcs-duct-short','fan':'mcs-fan-right','outlet':'mcs-duct-outlet'}
for rec in records:
 key=rec['key'];obj=dict(id='scene-'+key,assetId=mapping[key],label=key,x=rec['x']/1200*100,y=rec['y']/700*100,rotation=0,mode='static')
 if key=='damper':obj.update(label='Заслонка · демонстрация valve.out',mode='position',binding=dict(path='valve',slot='out',type='sys::float'),fault=dict(path='valve',slot='stuck',type='sys::bool'))
 elif key=='heater':obj.update(label='Нагреватель · opening',mode='position',binding=dict(path='coil',slot='opening',type='sys::float'))
 elif key=='fan':obj.update(label='Вентилятор · демонстрация run модели',mode='bool',binding=dict(path='.',slot='run',type='sys::bool'),fault=dict(path='.',slot='overloaded',type='sys::bool'))
 objects.append(obj)
view['objects']=objects
(out/'mcs-scene.ui.json').write_text(json.dumps(doc,ensure_ascii=False,indent=2),encoding='utf-8')
project=json.loads((r/'examples/mcs-ahu/mcs-cascade.sedona.json').read_text(encoding='utf-8'));project.update(title='Пример MCS · отдельные SVG · снимок');project['graph']['ui']=doc
(out/'mcs-scene.sedona.json').write_text(json.dumps(project,ensure_ascii=False,indent=2),encoding='utf-8')
print('Scene objects:',len(objects),'shared resources:',len(doc['library']),'bytes:',len(json.dumps(doc,ensure_ascii=False).encode()))
