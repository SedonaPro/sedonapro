import sedona.*;import java.nio.file.*;import java.util.*;
public class ExportCatalog {
 public static void main(String[]args)throws Exception{
  String[] names={"sys-d3984c51","types-10936551","math-c22b255c","logic-9fe95ce1","control-808b7db3"};KitPart[] parts=new KitPart[names.length];for(int i=0;i<parts.length;i++)parts[i]=KitPart.parse(names[i]);Schema schema=Schema.load(parts);
  String[] types={"types::ConstFloat","types::ConstBool","math::Add2","math::Mul2","math::Sub2","math::Div2","logic::And2","logic::Or2","logic::Not"};List<Object> result=new ArrayList<>();
  for(String qname:types){Type type=schema.type(qname);List<Object> ports=new ArrayList<>();Map<String,Object> defaults=new LinkedHashMap<>();
   for(Slot s:type.slots)if(s.isProp()&&!s.name.equals("meta")){boolean ro=s.facets.getb("readonly",false);ports.add(DeviceGateway.map("id",s.id,"name",s.name,"type",s.type.qname,"input",!ro,"output",true,"readonly",ro,"config",s.isConfig()));defaults.put(s.name,s.def().encodeString());}
   result.add(DeviceGateway.map("type",qname,"kitPart",type.kit.manifest.part().toString(),"ports",ports,"defaults",defaults));
  }
  Files.writeString(Paths.get(args[0]),Json.write(result));System.out.println("Exported "+result.size()+" real Sedona types");
 }
}
