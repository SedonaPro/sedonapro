package sedona.sox;
import sedona.KitPart;
/** Read only the version kit parts before schema resolution needs manifests. */
public final class SchemaParts {
 public static void assertIdentity(SoxClient client,SoxComponent component)throws Exception{
  sedona.Schema schema=client.readSchema();KitPart[] parts=read(client);if(parts.length!=schema.kits.length)throw new IllegalStateException("Device schema changed; reconnect before writing");
  for(int i=0;i<parts.length;i++)if(!parts[i].name.equals(schema.kits[i].name)||parts[i].checksum!=schema.kits[i].checksum)throw new IllegalStateException("Device schema changed; reconnect before writing");
  Msg r=client.request(Msg.makeUpdateReq(component.id(),'t'));r.checkResponse('C');int id=r.u2(),what=r.u1(),kit=r.u1(),type=r.u1();String name=r.str();int parent=r.u2();
  if(id!=component.id()||what!='t'||kit!=component.type.kit.id||type!=component.type.id||!name.equals(component.name())||parent!=component.parentId())throw new IllegalStateException("Component identity changed; reconnect before writing");
 }
 public static KitPart[] read(SoxClient client)throws Exception{
  Msg response=client.request(Msg.prepareRequest('v'));response.checkResponse('V');int count=response.u1();KitPart[] parts=new KitPart[count];
  for(int i=0;i<count;i++)parts[i]=new KitPart(response.str(),response.i4());return parts;
 }
}
