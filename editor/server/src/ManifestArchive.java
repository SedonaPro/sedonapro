package ru.bccontrol.ontrol.sedonanet.util;
import java.io.*;import java.util.*;import java.util.zip.*;import sedona.*;import sedona.manifest.*;import sedona.xml.*;
/** Canonical m: manifest exchange. No extraction to device-supplied filesystem paths. */
public final class ManifestArchive {
 public static final int MAX_ARCHIVE=8*1024*1024,MAX_XML=512*1024;
 public interface Fetcher {byte[] get(String uri)throws Exception;}
 private ManifestArchive(){}
 private static String key(KitPart p){return p.name+"-"+String.format(java.util.Locale.ROOT,"%08x",p.checksum);}
 private static KitManifest xml(byte[] bytes)throws Exception{
  if(bytes.length>MAX_XML)throw new IOException("Manifest XML too large");
  String text=new String(bytes,"UTF-8").toUpperCase(java.util.Locale.ROOT);if(text.contains("<!DOCTYPE")||text.contains("<!ENTITY"))throw new IOException("DTD/entity not allowed in manifests");
  KitManifest m=KitManifest.fromXml(XParser.make("device manifest",new ByteArrayInputStream(bytes)).parse());
  if(m==null||!m.name.matches("[A-Za-z_][A-Za-z0-9_]*"))throw new IOException("Invalid kit name");return m;
 }
 private static Map<String,KitManifest> unpack(byte[] bytes)throws Exception{
  if(bytes==null||bytes.length==0||bytes.length>MAX_ARCHIVE)throw new IOException("Invalid manifest payload size");Map<String,KitManifest> result=new LinkedHashMap<String,KitManifest>();
  if(bytes.length<4||bytes[0]!='P'||bytes[1]!='K'){KitManifest m=xml(bytes);result.put(key(m.part()),m);return result;}
  int total=0,count=0;
  try(ZipInputStream zin=new ZipInputStream(new ByteArrayInputStream(bytes))){ZipEntry entry;
   while((entry=zin.getNextEntry())!=null){if(++count>512)throw new IOException("Too many archive entries");String name=entry.getName();
    if(name.startsWith("/")||name.contains("\\")||name.contains(":")||java.util.Arrays.asList(name.split("/")).contains(".."))throw new IOException("Unsafe archive entry");
    if(entry.isDirectory())continue;ByteArrayOutputStream out=new ByteArrayOutputStream();byte[] buffer=new byte[4096];int n;
    while((n=zin.read(buffer))!=-1){total+=n;if(out.size()+n>MAX_XML||total>MAX_ARCHIVE)throw new IOException("Manifest archive expands beyond limit");out.write(buffer,0,n);}
    if(name.endsWith(".xml")){byte[] payload=out.toByteArray();String text=new String(payload,"UTF-8");String upper=text.toUpperCase(java.util.Locale.ROOT);if(upper.contains("<!DOCTYPE")||upper.contains("<!ENTITY"))throw new IOException("DTD/entity not allowed in manifests");if(!text.contains("<kitManifest"))continue;KitManifest m=xml(payload);String k=key(m.part());if(result.put(k,m)!=null)throw new IOException("Duplicate kit part: "+k);}
   }
  }
  if(result.isEmpty())throw new IOException("Archive contains no manifests");return result;
 }
 public static KitManifest[] validate(byte[] bytes,KitPart[] required)throws Exception{
  Map<String,KitManifest> entries=unpack(bytes);KitManifest[] result=new KitManifest[required.length];
  for(int i=0;i<required.length;i++){result[i]=entries.get(key(required[i]));if(result[i]==null)throw new IOException("Archive missing kit part: "+required[i]);}return result;
 }
 public static KitPart[] importManifests(byte[] bytes)throws Exception{
  Map<String,KitManifest> entries=unpack(bytes);for(KitManifest m:entries.values())if(!ManifestDb.save(m))throw new IOException("Cannot cache manifest "+m.part());
  return entries.values().stream().map(KitManifest::part).toArray(KitPart[]::new);
 }
 public static byte[] create(KitPart[] required)throws Exception{
  LinkedHashMap<String,KitPart> unique=new LinkedHashMap<String,KitPart>();for(KitPart p:required)unique.put(key(p),p);
  ByteArrayOutputStream out=new ByteArrayOutputStream();
  try(ZipOutputStream zip=new ZipOutputStream(out)){
   for(KitPart p:unique.values()){KitManifest m=ManifestDb.load(p);if(m==null||m.checksum!=p.checksum||!m.name.equals(p.name))throw new IOException("Missing exact manifest: "+p);
    ByteArrayOutputStream xml=new ByteArrayOutputStream();XWriter writer=new XWriter(xml);try{m.encodeXml(writer);}finally{writer.close();}
    if(xml.size()>MAX_XML)throw new IOException("Manifest too large: "+p);ZipEntry e=new ZipEntry(p+".xml");e.setTime(0);zip.putNextEntry(e);zip.write(xml.toByteArray());zip.closeEntry();
   }
  }
  byte[] bytes=out.toByteArray();validate(bytes,unique.values().toArray(new KitPart[0]));return bytes;
 }
 public static Schema loadSchema(KitPart[] parts,Fetcher fetcher)throws Exception{
  try{return Schema.load(parts);}catch(Schema.MissingKitManifestException missing){
   Map<String,KitManifest> resolved=new LinkedHashMap<String,KitManifest>();Exception archiveError=null;
   for(String uri:new String[]{"m:m.zip","m/m.zip"})try{resolved.putAll(unpack(fetcher.get(uri)));break;}catch(Exception e){archiveError=e;}
   for(KitPart p:missing.parts)if(!resolved.containsKey(key(p))){
    try{resolved.put(key(p),validate(fetcher.get("m:"+p+".xml"),new KitPart[]{p})[0]);}catch(Exception e){if(archiveError!=null)e.addSuppressed(archiveError);throw new IOException("Device cannot supply required manifest "+p,e);}
   }
   for(KitPart p:missing.parts){KitManifest m=resolved.get(key(p));if(m==null||!ManifestDb.save(m))throw new IOException("Cannot cache manifest "+p);}
   return Schema.load(parts);
  }
 }
}
