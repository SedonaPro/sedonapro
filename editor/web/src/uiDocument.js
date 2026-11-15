import{validateWidgets}from'./widgetRules.js';
import{validateComponents,validateParts}from'./components.js';
import{validateLibrary,validateObjects}from'./svgObjects.js';
export function newUi(){return {format:'mcs-sedona-ui',version:1,id:crypto.randomUUID(),title:'Визуализация Sedona',views:[]};}
export function validateUi(input){
 if(!input||input.format!=='mcs-sedona-ui'||![1,2,3].includes(input.version)||!/^[A-Za-z0-9_-]{1,64}$/.test(input.id)||!Array.isArray(input.views)||input.views.length>128)throw Error('Некорректный пакет визуализации');
 const library=input.library===undefined?undefined:validateLibrary(input.library);
 const components=input.components===undefined?undefined:validateComponents(input.components,library||[]);
 const ids=new Set();const views=input.views.map(v=>{if(!v||typeof v.id!=='string'||!/^[A-Za-z0-9_-]{1,64}$/.test(v.id)||ids.has(v.id)||typeof v.folder!=='string'||v.folder.length>2048||typeof v.svg!=='string'||v.svg.length>512*1024)throw Error('Некорректный экран');ids.add(v.id);const result={id:v.id,title:String(v.title||'Экран').slice(0,128),folder:v.folder,svg:v.svg};if(v.widgets!==undefined)result.widgets=validateWidgets(v.widgets);if(v.objects!==undefined)result.objects=validateObjects(v.objects,library||[]);if(v.parts!==undefined)result.parts=validateParts(v.parts,components||[]);return result;});
 const extended=library!==undefined||views.some(v=>v.objects!==undefined),composed=components!==undefined||views.some(v=>v.parts!==undefined);
 // Version 3: graphic components (pictures with named points inside) and their placed parts.
 const doc={format:input.format,version:composed?3:extended?2:Math.min(2,input.version),id:input.id,title:String(input.title||'Визуализация').slice(0,128),views,...(library?{library}:{}),...(components?{components}:{})};if(new TextEncoder().encode(JSON.stringify(doc)).length>512*1024)throw Error('Пакет больше 512 KiB');return doc;
}
// Only inert SVG drawing elements. Rendering is additionally isolated in an <img>.
export function safeSvg(text){
 if(text.length>512*1024||/<!DOCTYPE|<!ENTITY/i.test(text))throw Error('SVG слишком большой или содержит DTD');
 const doc=new DOMParser().parseFromString(text,'image/svg+xml');if(doc.querySelector('parsererror')||doc.documentElement.localName!=='svg')throw Error('Некорректный SVG');
 // Editor namespaces and RDF metadata identify the drawing; they are not network requests.
 for(const el of [...doc.querySelectorAll('*')])if(el.localName==='metadata'||el.localName==='namedview'&&el.namespaceURI==='http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd')el.remove();
 const allowed=new Set('svg g defs symbol use path rect circle ellipse line polyline polygon text tspan title desc style linearGradient radialGradient stop clipPath mask pattern marker animateTransform'.split(' '));
 function references(value,where){if(/javascript:|data:|https?:|@import|expression\(|\\/i.test(value))throw Error('SVG: запрещён внешний ресурс или код ('+where+')');if(/url\(/i.test(value)&&[...value.matchAll(/url\(([^)]*)\)/gi)].some(m=>!/^['"]?#[A-Za-z0-9_.:-]+['"]?$/.test(m[1].trim())))throw Error('SVG: разрешены только ссылки url(#id) ('+where+')');}
 for(const el of [...doc.querySelectorAll('*')]){if(el.namespaceURI!=='http://www.w3.org/2000/svg'||!allowed.has(el.localName))throw Error('SVG: неподдерживаемый элемент '+el.localName);
  if(el.localName==='animateTransform'){
   const permitted=new Set(['type','attributeName','from','to','dur','repeatCount']);if([...el.attributes].some(a=>!permitted.has(a.name))||el.getAttribute('type')!=='rotate'||el.getAttribute('attributeName')!=='transform'||el.getAttribute('repeatCount')!=='indefinite')throw Error('SVG: разрешена только локальная анимация вращения');
   for(const key of ['from','to']){const n=(el.getAttribute(key)||'').trim().split(/[ ,]+/).map(Number);if(![1,3].includes(n.length)||!n.every(Number.isFinite))throw Error('SVG: некорректный угол вращения');}
   const duration=el.getAttribute('dur')||'';if(!/^\d+(\.\d+)?s$/.test(duration)||parseFloat(duration)<0.05||parseFloat(duration)>120)throw Error('SVG: некорректный период вращения');
  }
  if(el.localName==='style'){const css=el.textContent;if(/@|\/\*/.test(css))throw Error('SVG: CSS-директивы и комментарии пока не поддерживаются');references(css,'style');}
  for(const attr of [...el.attributes]){const value=attr.value.trim();if(attr.namespaceURI==='http://www.w3.org/2000/xmlns/')continue;
   if(attr.namespaceURI&&!['http://www.w3.org/XML/1998/namespace','http://www.w3.org/1999/xlink'].includes(attr.namespaceURI)){el.removeAttributeNode(attr);continue;}
   if(/^on/i.test(attr.localName))throw Error('SVG: обработчик событий запрещён ('+attr.name+')');
   if(/href$/i.test(attr.localName)&&!/^#[A-Za-z0-9_.:-]+$/.test(value))throw Error('SVG: внешняя ссылка запрещена ('+attr.name+')');
   if(attr.localName==='base')throw Error('SVG: xml:base не поддерживается');
   if(!['id','class','label','lang','space'].includes(attr.localName))references(value,el.localName+'/@'+attr.name);
  }
 }
 return new XMLSerializer().serializeToString(doc.documentElement);
}

export function safeUi(input){const doc=validateUi(input);for(const view of doc.views)if(view.svg)view.svg=safeSvg(view.svg);for(const asset of doc.library||[])for(const k of Object.keys(asset.states))asset.states[k]=safeSvg(asset.states[k]);return doc;}
