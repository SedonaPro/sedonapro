export const PROFILE_KEY='mcs.sedona.connectionProfiles.v1';
export function cleanProfiles(input){
 if(!Array.isArray(input)||input.length>100)throw Error('Не более 100 профилей');const names=new Set();
 return input.map(p=>{if(!p||typeof p!=='object')throw Error('Некорректный профиль');const name=String(p.name||'').trim(),host=String(p.host||'').trim(),username=String(p.username||'');const port=Number(p.port);
 if(!name||name.length>80||names.has(name)||!host||host.length>253||/[\s/\\?#@]/.test(host)||!Number.isInteger(port)||port<1||port>65535||username.length>128)throw Error('Проверьте имя, адрес, порт и логин профиля');names.add(name);
 return {name,host,port,username,...(typeof p.credentialId==='string'&&/^[a-f0-9-]{36}$/.test(p.credentialId)?{credentialId:p.credentialId}:{})};});
}
export function loadProfiles(storage){const text=storage.getItem(PROFILE_KEY);return text?cleanProfiles(JSON.parse(text)):[]}
export function saveProfiles(storage,profiles){const clean=cleanProfiles(profiles);storage.setItem(PROFILE_KEY,JSON.stringify(clean));return clean}
export const LAST_PROFILE_KEY='mcs.sedona.lastProfile.v1';
export function lastProfile(storage){try{const profiles=loadProfiles(storage);return profiles.find(p=>p.name===storage.getItem(LAST_PROFILE_KEY))||profiles.at(-1)||null}catch{return null}}
