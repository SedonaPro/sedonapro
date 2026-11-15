import java.util.*;import java.nio.file.*;import sedona.*;
import ru.bccontrol.ontrol.sedonanet.util.ManifestArchive;
final class KitCatalog {
 private static final Path registry=Paths.get("data/catalog-parts.json");
 static synchronized void remember(KitPart[] active)throws Exception{
  LinkedHashMap<String,String> parts=new LinkedHashMap<>();if(Files.exists(registry))for(Object p:(List<?>)Json.object(Files.readString(registry)).get("parts")){KitPart k=KitPart.parse(p.toString());parts.put(k.name,k.toString());}
  for(KitPart p:active)parts.put(p.name,p.toString());Files.createDirectories(registry.getParent());Path temp=registry.resolveSibling("catalog-parts.json.tmp");Files.writeString(temp,Json.write(DeviceGateway.map("parts",new ArrayList<>(parts.values()))));Files.move(temp,registry,StandardCopyOption.REPLACE_EXISTING);
 }
 static synchronized List<Object> catalog(KitPart[] active)throws Exception{
  LinkedHashMap<String,KitPart> parts=new LinkedHashMap<>();
  if(Files.exists(registry))for(Object p:(List<?>)Json.object(Files.readString(registry)).get("parts")){KitPart k=KitPart.parse(p.toString());parts.put(k.name,k);}
  for(KitPart p:active)parts.put(p.name,p);if(parts.isEmpty())return new ArrayList<>();
  Schema schema=Schema.load(parts.values().toArray(new KitPart[0]));List<Object> result=new ArrayList<>();
  for(Kit kit:schema.kits)for(Type type:kit.types)if(type.isaComponent()&&!type.isAbstract()){
   List<Object> ports=new ArrayList<>();Map<String,Object> defaults=new LinkedHashMap<>();
   for(Slot s:type.slots)if(s.isProp()&&!DeviceGateway.sensitive(s.name)&&!s.name.equals("meta")){
    boolean ro=s.facets.getb("readonly",false);ports.add(DeviceGateway.map("id",s.id,"name",s.name,"type",s.type.qname,"input",!ro,"output",true,"readonly",ro,"config",s.isConfig(),"hidden",s.facets.getb("hidden",false),"summary",s.facets.getb("summary",!s.isConfig())));defaults.put(s.name,s.def().encodeString());
   }
   result.add(DeviceGateway.map("type",type.qname,"kitPart",kit.manifest.part().toString(),"ports",ports,"defaults",defaults));
  }return result;
 }
 static synchronized Map<String,Object> upload(byte[] bytes,KitPart[] active)throws Exception{
  KitPart[] imported=ManifestArchive.importManifests(bytes);LinkedHashMap<String,String> parts=new LinkedHashMap<>();
  String sha=HexFormat.of().formatHex(java.security.MessageDigest.getInstance("SHA-256").digest(bytes));Path original=Paths.get("data/kit-imports",sha+".bin");Files.createDirectories(original.getParent());if(!Files.exists(original))Files.write(original,bytes);
  if(Files.exists(registry))for(Object p:(List<?>)Json.object(Files.readString(registry)).get("parts")){KitPart k=KitPart.parse(p.toString());parts.put(k.name,k.toString());}
  // Keep the current exact device schema as dependency context for offline use.
  for(KitPart p:active)parts.put(p.name,p.toString());for(KitPart p:imported)parts.put(p.name,p.toString());
  Files.createDirectories(registry.getParent());Path temp=registry.resolveSibling("catalog-parts.json.tmp");Files.writeString(temp,Json.write(DeviceGateway.map("parts",new ArrayList<>(parts.values()))));Files.move(temp,registry,StandardCopyOption.REPLACE_EXISTING);
  List<String> names=new ArrayList<>();for(KitPart p:imported)names.add(p.toString());
  return DeviceGateway.map("imported",names,"sha256",sha,"note","Original retained locally; manifests cached; no executable code installed on device");
 }
}
