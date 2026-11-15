import test from'node:test';import assert from'node:assert/strict';import fs from'node:fs';
import{validateComponents,validateParts,partItems,autoBind,UNBOUND}from'../web/src/components.js';import{validateUi}from'../web/src/uiDocument.js';import{viewInterest}from'../web/src/viewBindings.js';
const lib=JSON.parse(fs.readFileSync(new URL('../web/src/hvacLibrary.json',import.meta.url)));
// "group" places an item in the palette of a library; it is not part of a package.
const plain=({group,...rest})=>rest,fan=plain(lib.components.find(c=>c.id==='sl-c-fan')),assets=lib.assets.filter(a=>fan.objects.some(o=>o.assetId===a.id)).map(plain);
const part={id:'p1',componentId:fan.id,label:'Fan',x:10,y:20,bind:{run:{path:'fan',slot:'run',type:'bool'},speed:{path:'fan',slot:'out',type:'float'}}};
const doc={format:'mcs-sedona-ui',version:3,id:'t',title:'T',views:[{id:'v',title:'V',folder:'app',svg:'',parts:[part]}],library:assets,components:[fan]};
test('the bundled HVAC library is a valid set of components',()=>{assert.equal(lib.format,'mcs-sedona-ui-library');assert(validateComponents(lib.components,lib.assets,256).length>=20)});
test('a package with components round trips as version 3; broken parts are refused',()=>{
 assert.deepEqual(validateUi(doc),doc);assert.equal(validateUi({...doc,version:1}).version,3);
 for(const patch of [{componentId:'missing'},{x:101},{bind:{nope:part.bind.run}},{bind:{run:{path:'fan',slot:'run',type:'float'}}},{bind:{run:{path:'../x',slot:'run',type:'bool'}}},{bind:{run:{path:'fan',slot:'password',type:'bool'}}}])assert.throws(()=>validateParts([{...part,...patch}],[fan]));
 assert.throws(()=>validateComponents([{...fan,widgets:[{...fan.widgets[0],slot:'unknownPoint'}]}],assets));
 assert.throws(()=>validateComponents([{...fan,objects:[{...fan.objects[0],binding:{path:'fan',slot:'run',type:'bool'}}]}],assets));
 assert.throws(()=>validateUi({...doc,components:undefined}));
});
test('a part is drawn as its symbols and widgets reading the bound slots',()=>{
 const flat=partItems(doc.views[0],doc.components,{width:1200,height:700}),pic=flat.objects.find(o=>o.id==='p1~pic'),val=flat.widgets.find(w=>w.id==='p1~val');
 assert.deepEqual(pic.binding,part.bind.run);assert.equal(pic.fault,undefined);assert.equal(partItems({parts:[{...part,bind:{}}]},doc.components,{width:1200,height:700}).objects.find(o=>o.id==='p1~pic').binding.path,UNBOUND);assert.equal(pic.x,10);assert.equal(pic.y,20);
 assert.equal(val.path,'fan');assert.equal(val.slot,'out');assert(val.x>10&&val.y>20&&val.y<20+fan.height/700*100);
});
test('points take the slots of a block by name; bound points are polled',()=>{
 const node={id:'n5',name:'fan',ports:[{name:'run',type:'bool'},{name:'alarm',type:'bool'},{name:'out',type:'float'}]},choices=[{node,relative:'fan'}];
 assert.deepEqual(autoBind(choices,fan,'fan'),{run:{path:'fan',slot:'run',type:'bool'},fault:{path:'fan',slot:'alarm',type:'bool'},speed:{path:'fan',slot:'out',type:'float'}});
 assert.deepEqual(autoBind(choices,fan,'elsewhere'),{});
});
test('library groups make folders within folders',async()=>{
 const tree=(list,of)=>{const root={folders:{},items:[]};for(const c of list){let at=root;for(const name of String(of(c)||'').split('/').filter(Boolean))at=at.folders[name]||(at.folders[name]={folders:{},items:[]});at.items.push(c);}return root;};
 const root=tree(lib.components,c=>c.group);assert(Object.keys(root.folders).length>=3);assert.equal(root.items.length,0);
 assert(root.folders['Воздух'].folders['Движение'].items.some(c=>c.id==='sl-c-fan'));assert(lib.components.every(c=>typeof c.group==='string'&&c.group.includes('/')));
});
