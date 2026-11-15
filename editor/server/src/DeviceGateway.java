import java.net.*;import java.util.*;import java.util.concurrent.atomic.AtomicLong;
import sedona.*;import sedona.dasp.*;import sedona.sox.*;
import ru.bccontrol.ontrol.sedonanet.util.ManifestArchive;
/** One explicitly selected device, bounded viewers and no automatic reconnection. */
final class DeviceGateway implements AutoCloseable {
 private DaspSocket socket;private SoxClient client,fileClient;
 private final Map<Integer,SoxComponent> components=new LinkedHashMap<>();
 private final List<Object> nodes=new ArrayList<>(),edges=new ArrayList<>();
 private final Set<Integer> subscribed=new HashSet<>();private final InterestRegistry viewers=new InterestRegistry();
 private final AtomicLong cov=new AtomicLong();private long connects,disconnects,subAdds,subRemoves,reads,sent,received;private String mode="OFF",lastError="";
 private Map<String,Object> origin=Collections.emptyMap();private WritePlan pending;
 private KitPart[] activeParts=new KitPart[0];
 private final Set<String> restartTokens=new HashSet<>();
 private final Set<String> createTokens=new HashSet<>();
 private final Set<String> linkTokens=new HashSet<>();
 synchronized List<Object> catalog()throws Exception{return KitCatalog.catalog(activeParts);}
 synchronized Map<String,Object> importKits(byte[] bytes)throws Exception{return KitCatalog.upload(bytes,activeParts);}
 synchronized byte[] exportManifests()throws Exception{if(activeParts.length==0)throw new IllegalStateException("Read a device first");return ManifestArchive.create(activeParts);}
 private static final boolean ALLOW_WRITES=Boolean.getBoolean("sedona.allowWrites");
 private byte[] readUiFile(SoxClient transfer)throws Exception{
  Buf buffer=new Buf();SoxFile delegate=SoxFile.make(buffer);
  SoxFile bounded=new SoxFile(){public int size(){return delegate.size();}public void open(String m)throws java.io.IOException{delegate.open(m);}public void close(){delegate.close();}public void read(int p,Buf b,int n)throws java.io.IOException{delegate.read(p,b,n);}public void write(int p,Buf b,int n)throws java.io.IOException{if(p<0||n<0||(long)p+n>ViewStore.LIMIT)throw new java.io.IOException("UI download exceeds limit");delegate.write(p,b,n);}};
  transfer.getFile("mcs-ui.json",bounded,null,null);return Arrays.copyOf(buffer.bytes,buffer.size);
 }
 Map<String,Object> loadUi(String consumer)throws Exception{
  SoxClient transfer;synchronized(this){viewers.expire(System.nanoTime());if(client==null||!mode.equals("LIVE")||!hasViewer(consumer))throw new IllegalStateException("Live viewer required");if(fileClient==client)throw new IllegalStateException("UI file transfer already in progress");transfer=client;fileClient=transfer;}
  try{Map<String,Object> document=ViewStore.decode(readUiFile(transfer));ViewStore.save(document);return document;}
  finally{synchronized(this){if(fileClient==transfer)fileClient=null;}}
 }
 Map<String,Object> saveUi(String consumer,Map<String,Object> document)throws Exception{
  byte[] payload=ViewStore.encode(document);SoxClient transfer;
  synchronized(this){requireWriter(consumer);if(pending!=null&&pending.deadline>System.nanoTime())throw new IllegalStateException("Finish pending parameter write first");ViewStore.save(document);transfer=client;fileClient=transfer;}
  try{transfer.putFile("mcs-ui.json",SoxFile.make(new Buf(payload)),null,null);byte[] readback=readUiFile(transfer);if(!Arrays.equals(payload,readback))throw new java.io.IOException("UI readback mismatch");}
  catch(Exception e){throw new IllegalStateException("Local UI copy saved; device file write/readback failed. Device copy may be incomplete; no automatic retry. "+e.getClass().getSimpleName());}
  finally{synchronized(this){if(fileClient==transfer)fileClient=null;}}
  return map("saved",true,"verified",true,"uri","mcs-ui.json","bytes",payload.length);
 }
 private static final class WritePlan {String token,consumer;SoxComponent component;Slot slot;Value before,desired;long deadline;}
 static Map<String,Object> map(Object...v){Map<String,Object>m=new LinkedHashMap<>();for(int i=0;i<v.length;i+=2)m.put(v[i].toString(),v[i+1]);return m;}
 synchronized Map<String,Object> connect(String host,int port,String user,String password,String requested)throws Exception{
  if(!requested.equals("LIVE")&&!requested.equals("SNAPSHOT"))throw new IllegalArgumentException("Mode must be LIVE or SNAPSHOT");
  if(client!=null)throw new IllegalStateException("Already connected; attach a viewer or disconnect first");
  nodes.clear();edges.clear();components.clear();lastError="";mode="CONNECTING";
  String stage="вход DASP/SOX";
  try{
   socket=DaspSocket.open(-1,null,DaspSocket.SESSION_QUEUING);client=new SoxClient(socket,InetAddress.getByName(host),port,user,password);
   Hashtable<String,String> options=new Hashtable<>();options.put("dasp.absMax","1024");client.connect(options);connects++;
   stage="чтение списка китов";
   KitPart[] parts=SchemaParts.read(client);stage="загрузка манифестов";ManifestArchive.loadSchema(parts,uri->{Buf b=new Buf();client.getFile(uri,SoxFile.make(b),null,null);return Arrays.copyOf(b.bytes,b.size);});activeParts=parts;KitCatalog.remember(parts);
   stage="чтение версии платформы";
   List<String> partNames=new ArrayList<>();for(KitPart part:parts)partNames.add(part.toString());origin=map("parts",partNames,"platformId",client.readVersion().platformId,"capturedAt",java.time.Instant.now().toString());
   stage="чтение дерева компонентов";readTree();
   mode=requested;if(requested.equals("SNAPSHOT")){closeTransport();mode="SNAPSHOT";return map("graph",graph(),"status",status());}
   String consumer=viewers.create(System.nanoTime());return map("consumer",consumer,"graph",graph(),"status",status());
  }catch(Exception e){String detail=String.valueOf(e.getMessage());if(password!=null&&!password.isEmpty())detail=detail.replace(password,"[hidden]");detail=detail.replaceAll("[\\r\\n]"," ");if(detail.length()>500)detail=detail.substring(0,500);lastError="Этап: "+stage+"; "+e.getClass().getSimpleName()+": "+detail;System.err.println(java.time.Instant.now()+" "+lastError);close();throw new IllegalStateException(lastError);}
 }
  /** Reads the whole component tree into nodes/edges (at connect and after an action that may rebuild part of it). */
 private void readTree()throws Exception{
  nodes.clear();edges.clear();components.clear();
   Set<Integer> seen=new HashSet<>();Deque<Integer> todo=new ArrayDeque<>();todo.add(0);
   while(!todo.isEmpty()){int id=todo.removeFirst();if(!seen.add(id))continue;if(seen.size()>512)throw new IllegalStateException("Prototype limit: 512 device components");
    
    SoxComponent c=client.load(id);client.update(c,SoxComponent.CONFIG|SoxComponent.RUNTIME|SoxComponent.LINKS);reads++;components.put(id,c);c.listener=(comp,mask)->cov.incrementAndGet();
    List<Object> ports=new ArrayList<>();for(Slot s:c.type.slots)if(s.isProp()&&!sensitive(s.name)){boolean ro=s.name.equals("meta")||s.facets.getb("readonly",false);{Map<String,Object> port=map("id",s.id,"name",s.name,"type",s.type.qname,"input",!ro,"output",true,"readonly",ro,"config",s.isConfig(),"hidden",s.facets.getb("hidden",false),"summary",s.facets.getb("summary",!s.isConfig()));String range=s.facets.gets("range","");if(!range.isBlank())port.put("range",Arrays.stream(range.split(",")).map(String::trim).toList());ports.add(port);}}
    List<Object> actions=new ArrayList<>();for(Slot s:c.type.slots)if(s.isAction()&&s.type.qname.endsWith("void"))actions.add(s.name);
    Map<String,Object> node=map("id",id,"name",c.name(),"type",c.type.qname,"kitPart",c.type.kit.manifest.part().toString(),"parent",c.parentId(),"ports",ports,"actions",actions);String widget=c.type.facets.gets("widget","");if(!widget.isBlank())node.put("widget",widget);nodes.add(node);
    for(Link link:client.links(c))edges.add(map("source",link.fromCompId,"sourceSlot",link.fromSlotId,"target",link.toCompId,"targetSlot",link.toSlotId));
    for(int child:c.childrenIds())todo.add(child);
   }
   // Deduplicate links returned for both source and destination components.
   Map<String,Object> unique=new LinkedHashMap<>();for(Object e:edges)unique.put(Json.write(e),e);edges.clear();edges.addAll(unique.values());
  }

 // ---- Kit widgets (docs/widgets.md): their files, and one confirmed action with the options it needs.
 Map<String,Object> kitUi(String consumer)throws Exception{
  SoxClient transfer=null;synchronized(this){viewers.expire(System.nanoTime());if(client!=null&&mode.equals("LIVE")&&hasViewer(consumer)&&fileClient!=client){transfer=client;fileClient=transfer;}}
  String problem="";
  if(transfer!=null)try{
   Buf buffer=new Buf();SoxFile delegate=SoxFile.make(buffer);
   SoxFile bounded=new SoxFile(){public int size(){return delegate.size();}public void open(String m)throws java.io.IOException{delegate.open(m);}public void close(){delegate.close();}public void read(int p,Buf b,int n)throws java.io.IOException{delegate.read(p,b,n);}public void write(int p,Buf b,int n)throws java.io.IOException{if(p<0||n<0||(long)p+n>KitWidgets.MAX_ZIP)throw new java.io.IOException("ui.zip exceeds limit");delegate.write(p,b,n);}};
   transfer.getFile("m:ui.zip",bounded,null,null);
   return map("source","device","kits",KitWidgets.fromZip(Arrays.copyOf(buffer.bytes,buffer.size)));
  }catch(Exception e){problem=e.getClass().getSimpleName()+": "+String.valueOf(e.getMessage());}
  finally{synchronized(this){if(fileClient==transfer)fileClient=null;}}
  java.nio.file.Path ui=java.nio.file.Paths.get(Env.home.getPath(),"ui");
  return map("source","sdk","path",ui.toString(),"deviceProblem",problem,"kits",KitWidgets.fromFolder(ui));
 }
 private static final class ActionPlan{String token,consumer;SoxComponent component;Slot action;final Map<Slot,Value> desired=new LinkedHashMap<>(),before=new LinkedHashMap<>();long deadline;}
 private ActionPlan actionPlan;
 synchronized Map<String,Object> prepareAction(String consumer,int componentId,String action,Map<?,?> values)throws Exception{
  requireWriter(consumer);long now=System.nanoTime();
  if(pending!=null&&pending.deadline>now)throw new IllegalStateException("Finish parameter write first");
  if(actionPlan!=null&&actionPlan.deadline>now&&!actionPlan.consumer.equals(consumer))throw new IllegalStateException("Another viewer owns the pending action");
  SoxComponent c=components.get(componentId);if(c==null)throw new IllegalArgumentException("Unknown component");
  if(c.type.facets.gets("widget","").isBlank())throw new IllegalArgumentException("Actions are invoked only on components with a kit widget");
  Slot a=c.type.slot(action);if(a==null||!a.isAction()||!a.type.qname.endsWith("void"))throw new IllegalArgumentException("No action "+action+" without an argument");
  if(values==null)values=Map.of();if(values.size()>32)throw new IllegalArgumentException("Too many options");
  SchemaParts.assertIdentity(client,c);
  ActionPlan p=new ActionPlan();List<Object> changes=new ArrayList<>();
  for(Map.Entry<?,?> e:values.entrySet()){
   Slot s=c.type.slot(String.valueOf(e.getKey()));
   if(s==null||!s.isProp()||!s.isConfig()||sensitive(s.name)||s.name.equals("meta")||s.facets.getb("readonly",false))throw new IllegalArgumentException("Option "+e.getKey()+" is not a writable configuration property");
   Value v=PropertyWrite.parse(s,String.valueOf(e.getValue())),b=client.readProp(c,s);
   p.desired.put(s,v);p.before.put(s,b);changes.add(map("slot",s.name,"before",b.encodeString(),"desired",v.encodeString()));
  }
  p.token=UUID.randomUUID().toString();p.consumer=consumer;p.component=c;p.action=a;p.deadline=now+60_000_000_000L;actionPlan=p;
  return map("token",p.token,"component",c.name(),"action",a.name,"changes",changes,"expiresInSeconds",60,"note","Nothing written yet");
 }
 synchronized Map<String,Object> commitAction(String consumer,String token)throws Exception{
  requireWriter(consumer);ActionPlan p=actionPlan;if(p==null||!p.token.equals(token)||!p.consumer.equals(consumer))throw new IllegalStateException("Action plan missing, consumed or belongs to another viewer");actionPlan=null;
  if(System.nanoTime()>p.deadline)throw new IllegalStateException("Action plan expired; prepare again");
  SchemaParts.assertIdentity(client,p.component);
  for(Map.Entry<Slot,Value> e:p.before.entrySet())if(!client.readProp(p.component,e.getKey()).equals(e.getValue()))throw new IllegalStateException("Option "+e.getKey().name+" changed since preview; nothing written. Prepare again");
  List<Object> written=new ArrayList<>();
  for(Map.Entry<Slot,Value> e:p.desired.entrySet()){
   Slot s=e.getKey();if(!p.before.get(s).equals(e.getValue()))client.write(p.component,s,e.getValue());
   Value back=client.readProp(p.component,s);if(!back.equals(e.getValue()))throw new IllegalStateException("Readback of "+s.name+" differs ("+back.encodeString()+"); action not invoked");
   written.add(map("slot",s.name,"readBack",back.encodeString()));
  }
  try{client.invoke(p.component,p.action,null);}catch(Exception e){ClientCache.forget(client);subscribed.clear();readTree();throw new IllegalStateException("Action outcome unknown; no automatic retry. The tree was read again.");}
  // The action may have replaced components under the same ids: read everything afresh.
  ClientCache.forget(client);subscribed.clear();readTree();reconcile();
  return map("status","DONE","component",p.component.name(),"action",p.action.name,"written",written,"time",java.time.Instant.now().toString(),"persistedToApp",false,"graph",graph());
 }
 synchronized void cancelAction(String consumer,String token){if(actionPlan!=null&&actionPlan.consumer.equals(consumer)&&actionPlan.token.equals(token))actionPlan=null;}
 static boolean sensitive(String s){s=s.toLowerCase(Locale.ROOT);return s.contains("password")||s.contains("secret")||s.contains("cred")||s.equals("key");}
 synchronized Map<String,Object> attach(){if(client==null||!mode.equals("LIVE"))throw new IllegalStateException("No live device");return map("consumer",viewers.create(System.nanoTime()),"graph",graph(),"status",status());}
 synchronized void watch(String consumer,Collection<Integer> ids)throws Exception{
  // A renewal can race a rebuild (an action, a delete): the browser took its interest from the
  // previous tree. Components gone since then are left out instead of failing the whole lease.
  if(ids.size()>128)throw new IllegalArgumentException("Maximum 128 watched components");
  List<Integer> known=new ArrayList<>();for(int id:ids)if(components.containsKey(id))known.add(id);
  viewers.renew(consumer,known,System.nanoTime());reconcile();
 }
 private void reconcile()throws Exception{
  if(client==null||fileClient==client)return;Set<Integer> desired=viewers.union();
  for(int id:new HashSet<>(subscribed))if(!desired.contains(id)&&components.containsKey(id)){client.unsubscribe(components.get(id),SoxComponent.CONFIG|SoxComponent.RUNTIME);subscribed.remove(id);subRemoves++;}
  for(int id:desired)if(!subscribed.contains(id)&&components.containsKey(id)){client.subscribe(components.get(id),SoxComponent.CONFIG|SoxComponent.RUNTIME);subscribed.add(id);subAdds++;}
 }
 synchronized void release(String id)throws Exception{viewers.release(id);if(viewers.size()==0)close();else reconcile();}
 synchronized void sweep(){try{viewers.expire(System.nanoTime());if(client!=null){if(client.isClosed()){lastError="SOX session closed by device or network; reconnect manually";System.err.println(java.time.Instant.now()+" "+lastError);close();}else if(viewers.size()==0){lastError="Browser heartbeat expired: no active stream or heartbeat for 8 seconds";System.err.println(java.time.Instant.now()+" "+lastError);close();}else reconcile();}}catch(Exception e){lastError="Subscription cleanup failed ("+e.getClass().getSimpleName()+"); connection closed";System.err.println(java.time.Instant.now()+" "+lastError);close();}}
 synchronized boolean hasViewer(String id){return viewers.contains(id)&&client!=null;}
 synchronized boolean streamAlive(String id){return client!=null&&viewers.touch(id,System.nanoTime());}
 private void closeTransport(){fileClient=null;pending=null;restartTokens.clear();createTokens.clear();linkTokens.clear();if(client!=null){try{if(client.session()!=null){sent+=client.session().numSent();received+=client.session().numReceived();}client.close();}catch(Exception ignored){}client=null;disconnects++;}if(socket!=null){try{socket.close();}catch(Exception ignored){}socket=null;}subRemoves+=subscribed.size();subscribed.clear();viewers.clear();}
 public synchronized void close(){closeTransport();mode="OFF";}
 synchronized Map<String,Object> graph(){return map("nodes",new ArrayList<>(nodes),"edges",new ArrayList<>(edges),"values",values(),"origin",origin);}
 private void requireWriter(String consumer){if(fileClient!=null&&fileClient==client)throw new IllegalStateException("UI file transfer in progress; wait before device mutation");if(!ALLOW_WRITES)throw new IllegalStateException("Device writes are disabled at server startup");if(client==null||!mode.equals("LIVE")||!viewers.contains(consumer))throw new IllegalStateException("Live viewer lease required");viewers.expire(System.nanoTime());if(!viewers.contains(consumer))throw new IllegalStateException("Viewer lease expired");}
 synchronized Map<String,Object> restartModel(String consumer,int componentId,String token)throws Exception{
  requireWriter(consumer);if(pending!=null&&pending.deadline>System.nanoTime())throw new IllegalStateException("Finish parameter write first");
  if(token==null||!token.matches("[a-zA-Z0-9-]{20,64}"))throw new IllegalArgumentException("Invalid command token");
  if(restartTokens.contains(token))throw new IllegalStateException("Restart token already consumed; no automatic retry");
  if(restartTokens.size()>=64)throw new IllegalStateException("Restart limit reached; reconnect");
  SoxComponent c=components.get(componentId);if(c==null||!c.type.qname.equals("bccSim::SimSession"))throw new IllegalArgumentException("Only bccSim::SimSession restart is supported");
  SchemaParts.assertIdentity(client,c);Slot restart=c.type.slot("restart"),generation=c.type.slot("generation"),state=c.type.slot("state"),busy=c.type.slot("busy"),fault=c.type.slot("fault");
  if(restart==null||!restart.isAction()||generation==null||state==null||busy==null||fault==null)throw new IllegalStateException("Session command schema unavailable");
  if(client.readProp(c,busy).encodeString().equals("true"))throw new IllegalStateException("Simulation session is busy");
  String before=client.readProp(c,generation).encodeString();restartTokens.add(token);
  try{client.invoke(c,restart,null);}catch(Exception e){throw new IllegalStateException("Restart outcome unknown; token consumed. Read state before another command.");}
  for(int i=0;i<15;i++){Thread.sleep(100);Value g=client.readProp(c,generation),st=client.readProp(c,state),f=client.readProp(c,fault);c.set(generation,g);c.set(state,st);c.set(fault,f);if(!g.encodeString().equals(before)&&st.encodeString().equals("1")&&f.encodeString().equals("0"))return map("status","VERIFIED","component",c.name(),"beforeGeneration",before,"generation",g.encodeString(),"state",st.encodeString(),"fault",f.encodeString(),"note","All clocks belonging to this session restarted; no App.save");}
  return map("status","OUTCOME_UNKNOWN","beforeGeneration",before,"note","Command sent once; read session status before retry");
 }
 synchronized Map<String,Object> prepareWrite(String consumer,int componentId,int slotId,String raw)throws Exception{
  requireWriter(consumer);long now=System.nanoTime();if(pending!=null&&pending.deadline>now&&!pending.consumer.equals(consumer))throw new IllegalStateException("Another viewer owns the pending write plan");
  SoxComponent c=components.get(componentId);if(c==null)throw new IllegalArgumentException("Unknown component");Slot slot=c.type.slot(slotId);Value desired=PropertyWrite.parse(slot,raw);SchemaParts.assertIdentity(client,c);Value before=client.readProp(c,slot);
  WritePlan p=new WritePlan();p.token=UUID.randomUUID().toString();p.consumer=consumer;p.component=c;p.slot=slot;p.desired=desired;p.before=before;p.deadline=now+30_000_000_000L;pending=p;
  return map("token",p.token,"component",c.name(),"slot",slot.name,"type",slot.type.qname,"before",before.encodeString(),"desired",desired.encodeString(),"expiresInSeconds",30,"note","No value written yet");
 }
 synchronized Map<String,Object> commitWrite(String consumer,String token)throws Exception{
  requireWriter(consumer);WritePlan p=pending;if(p==null||!p.token.equals(token)||!p.consumer.equals(consumer))throw new IllegalStateException("Write plan missing, consumed or belongs to another viewer");pending=null;
  if(System.nanoTime()>p.deadline)throw new IllegalStateException("Write plan expired; prepare again");
  SchemaParts.assertIdentity(client,p.component);Value actual=client.readProp(p.component,p.slot);if(!actual.equals(p.before))throw new IllegalStateException("Value changed since preview; nothing written. Prepare again");
  if(!actual.equals(p.desired))client.write(p.component,p.slot,p.desired);
  Value back=client.readProp(p.component,p.slot);boolean verified=back.equals(p.desired);p.component.set(p.slot,back);
  return map("status",verified?"VERIFIED":"READBACK_MISMATCH","component",p.component.name(),"slot",p.slot.name,"before",p.before.encodeString(),"requested",p.desired.encodeString(),"readBack",back.encodeString(),"time",java.time.Instant.now().toString(),"persistedToApp",false);
 }
 synchronized void cancelWrite(String consumer,String token){if(pending!=null&&pending.consumer.equals(consumer)&&pending.token.equals(token))pending=null;}
 synchronized Map<String,Object> saveLayout(String consumer,List<?> changes)throws Exception{
  requireWriter(consumer);if(pending!=null&&pending.deadline>System.nanoTime())throw new IllegalStateException("Finish pending parameter write first");
  if(changes.isEmpty()||changes.size()>64)throw new IllegalArgumentException("Layout batch: 1..64 components");
  List<Object> receipts=new ArrayList<>();Set<Integer> seen=new HashSet<>();
  // Validate the entire payload before any write. Each component is checked again before writing.
  for(Object raw:changes){Map<?,?> m=(Map<?,?>)raw;int id=layoutInt(m,"id",0,65534);layoutInt(m,"x",0,255);layoutInt(m,"y",0,255);layoutInt(m,"expectedMeta",Integer.MIN_VALUE,Integer.MAX_VALUE);if(!seen.add(id)||!components.containsKey(id))throw new IllegalArgumentException("Unknown/duplicate layout component");}
  String failure=null;
  for(Object raw:changes){Map<?,?> m=(Map<?,?>)raw;int id=layoutInt(m,"id",0,65534);try{
   SoxComponent c=components.get(id);Slot slot=c.type.slot("meta");if(slot==null||!slot.isProp()||slot.type.id!=Type.intId)throw new IllegalStateException("No integer meta slot");
   SchemaParts.assertIdentity(client,c);int before=((Int)client.readProp(c,slot)).val,expected=layoutInt(m,"expectedMeta",Integer.MIN_VALUE,Integer.MAX_VALUE);
   if((before&0xffff0000)!=(expected&0xffff0000))throw new IllegalStateException("Coordinates changed on device; reread before saving");
   int desired=(layoutInt(m,"x",0,255)<<24)|(layoutInt(m,"y",0,255)<<16)|(before&0xffff);
   if(before!=desired)client.write(c,slot,Int.make(desired));int back=((Int)client.readProp(c,slot)).val;c.set(slot,Int.make(back));
   receipts.add(map("id",id,"meta",back,"verified",back==desired));if(back!=desired)throw new IllegalStateException("Layout readback mismatch");
  }catch(Exception e){failure="Component #"+id+": "+e.getClass().getSimpleName();if(e instanceof IllegalStateException)failure=e.getMessage();break;}}
  boolean saveRequested=false;
  if(failure==null){try{SoxComponent app=components.get(0);SchemaParts.assertIdentity(client,app);Slot save=app.type.slot("save");if(save==null||!save.isAction())throw new IllegalStateException("App.save is unavailable");client.invoke(app,save,null);saveRequested=true;}catch(Exception e){failure="Coordinates updated in RAM; App.save failed or outcome unknown";}}
  return map("receipts",receipts,"complete",failure==null,"error",failure,"appSaveRequested",saveRequested,"persistenceVerified",false);
 }
 synchronized Map<String,Object> createComponent(String consumer,String qname,String kitPart,int parentId,int x,int y,String token)throws Exception{
  return createWithConfig(consumer,qname,kitPart,parentId,x,y,token,null);
 }
 synchronized Map<String,Object> duplicateComponent(String consumer,int sourceId,String token)throws Exception{
  requireWriter(consumer);SoxComponent source=components.get(sourceId);if(source==null)throw new IllegalArgumentException("Unknown source component");SchemaParts.assertIdentity(client,source);client.update(source,SoxComponent.TREE);
  if(source.childrenIds().length>0||sourceId==0)throw new IllegalArgumentException("Duplicate supports a single component without children");
  Slot meta=source.type.slot("meta");int value=((Int)client.readProp(source,meta)).val,x=(value>>>24)&255,y=(value>>>16)&255;
  x=x<=252?x+3:x-3;y=y<=252?y+3:y-3;
  return createWithConfig(consumer,source.type.qname,source.type.kit.manifest.part().toString(),source.parentId(),x,y,token,source);
 }
 private Map<String,Object> createWithConfig(String consumer,String qname,String kitPart,int parentId,int x,int y,String token,SoxComponent source)throws Exception{
  requireWriter(consumer);if(pending!=null&&pending.deadline>System.nanoTime())throw new IllegalStateException("Finish pending parameter write first");
  if(x<0||x>255||y<0||y>255||!token.matches("[a-f0-9-]{36}"))throw new IllegalArgumentException("Invalid create request");
  if(createTokens.contains(token))throw new IllegalStateException("Create request already attempted; reread device before retrying");if(createTokens.size()>=256)throw new IllegalStateException("Reconnect after 256 create attempts");
  SoxComponent parent=components.get(parentId);if(parent==null)throw new IllegalArgumentException("Unknown parent");SchemaParts.assertIdentity(client,parent);
  Type type=client.readSchema().type(qname);if(type==null||!type.isaComponent()||type.isAbstract()||!type.kit.manifest.part().toString().equals(kitPart))throw new IllegalArgumentException("Exact component type is not installed on device");
  client.update(parent,SoxComponent.TREE);Set<String> names=new HashSet<>();for(int id:parent.childrenIds())names.add(client.load(id).name());int i=1;while(names.contains("blk"+i))i++;String name="blk"+i;sedona.Component.assertName(name);
  Slot[] props=type.configProps();Value[] config=new Value[props.length];for(int k=0;k<props.length;k++){Slot s=props[k];if(source!=null&&sensitive(s.name))throw new IllegalArgumentException("Cannot duplicate credential-bearing components");config[k]=source==null?s.def():client.readProp(source,s);if(s.name.equals("meta")){int flags=((Int)config[k]).val&0xffff;config[k]=Int.make((x<<24)|(y<<16)|flags);}}
  createTokens.add(token);SoxComponent c;
  try{c=client.add(parent,type,name,config);}catch(Exception e){throw new IllegalStateException("SOX create failed or outcome unknown; do not repeat automatically. Reread device");}
  client.update(c,SoxComponent.CONFIG|SoxComponent.RUNTIME|SoxComponent.TREE);components.put(c.id(),c);c.listener=(comp,mask)->cov.incrementAndGet();
  List<Object> ports=new ArrayList<>();for(Slot s:c.type.slots)if(s.isProp()&&!sensitive(s.name)){boolean ro=s.name.equals("meta")||s.facets.getb("readonly",false);ports.add(map("id",s.id,"name",s.name,"type",s.type.qname,"input",!ro,"output",true,"readonly",ro,"config",s.isConfig(),"hidden",s.facets.getb("hidden",false),"summary",s.facets.getb("summary",!s.isConfig())));}
  nodes.add(map("id",c.id(),"name",c.name(),"type",c.type.qname,"kitPart",c.type.kit.manifest.part().toString(),"parent",c.parentId(),"ports",ports));
  boolean saved=false;try{SoxComponent app=components.get(0);SchemaParts.assertIdentity(client,app);Slot save=app.type.slot("save");if(save!=null&&save.isAction()){client.invoke(app,save,null);saved=true;}}catch(Exception ignored){}
  boolean verified=true;for(int k=0;k<props.length;k++)if(!client.readProp(c,props[k]).equals(config[k]))verified=false;
  return map("created",true,"id",c.id(),"name",c.name(),"configVerified",verified,"appSaveRequested",saved,"persistenceVerified",false,"graph",graph());
 }
 synchronized Map<String,Object> changeLink(String consumer,int source,int sourceSlot,int target,int targetSlot,boolean remove,String token)throws Exception{
  requireWriter(consumer);if(pending!=null&&pending.deadline>System.nanoTime())throw new IllegalStateException("Finish parameter write first");
  if(!token.matches("[a-f0-9-]{36}")||linkTokens.contains(token))throw new IllegalStateException("Link request already attempted or invalid");if(linkTokens.size()>=512)throw new IllegalStateException("Reconnect after 512 link attempts");
  SoxComponent a=components.get(source),b=components.get(target);if(a==null||b==null)throw new IllegalArgumentException("Unknown link component");SchemaParts.assertIdentity(client,a);if(a!=b)SchemaParts.assertIdentity(client,b);
  Slot out=a.type.slot(sourceSlot),in=b.type.slot(targetSlot);if(out==null||in==null||!out.isProp()||!in.isProp()||out.type.id!=in.type.id||sensitive(out.name)||sensitive(in.name)||out.name.equals("meta")||in.name.equals("meta"))throw new IllegalArgumentException("Invalid link slots/types");
  if(!remove&&in.facets.getb("readonly",false))throw new IllegalArgumentException("Target is readonly");
  Link wanted=new Link(source,sourceSlot,target,targetSlot);boolean found=false;for(Link l:client.links(b)){boolean same=l.fromCompId==source&&l.fromSlotId==sourceSlot&&l.toCompId==target&&l.toSlotId==targetSlot;if(same)found=true;if(!remove&&l.toCompId==target&&l.toSlotId==targetSlot)throw new IllegalStateException("Input already linked; reread device");}
  if(remove&&!found)throw new IllegalStateException("Link changed or missing; reread device");linkTokens.add(token);
  try{if(remove)client.unlink(wanted);else client.link(wanted);}catch(Exception e){throw new IllegalStateException("SOX link outcome unknown; reread device before retrying");}
  boolean exists=false;for(Link l:client.links(b))if(l.fromCompId==source&&l.fromSlotId==sourceSlot&&l.toCompId==target&&l.toSlotId==targetSlot)exists=true;
  if(exists==remove)throw new IllegalStateException("Link readback mismatch; reread device");
  Map<String,Object> edge=map("source",source,"sourceSlot",sourceSlot,"target",target,"targetSlot",targetSlot);edges.removeIf(e->Json.write(e).equals(Json.write(edge)));if(!remove)edges.add(edge);
  boolean saved=false;try{SoxComponent app=components.get(0);SchemaParts.assertIdentity(client,app);Slot save=app.type.slot("save");if(save!=null&&save.isAction()){client.invoke(app,save,null);saved=true;}}catch(Exception ignored){}
  return map("verified",true,"removed",remove,"appSaveRequested",saved,"graph",graph());
 }
 private static int layoutInt(Map<?,?> m,String key,int lo,int hi){Object v=m.get(key);if(!(v instanceof Number))throw new IllegalArgumentException("Missing layout field: "+key);double n=((Number)v).doubleValue();if(!java.lang.Double.isFinite(n)||n!=Math.rint(n)||n<lo||n>hi)throw new IllegalArgumentException("Layout field out of range: "+key);return (int)n;}
 synchronized Map<String,Object> liveValues(){Map<String,Object> all=values();Set<Integer> interest=viewers.union();all.keySet().removeIf(key->!interest.contains(Integer.parseInt(key.substring(1))));return all;}
 synchronized Map<String,Object> values(){Map<String,Object> result=new LinkedHashMap<>();for(SoxComponent c:components.values()){Map<String,Object> fields=new LinkedHashMap<>();for(Slot slot:c.type.slots)if(slot.isProp()&&!sensitive(slot.name)){try{Value v=c.get(slot);fields.put(slot.name,v==null?"--":v.toString());}catch(Exception e){fields.put(slot.name,"--");}}result.put("n"+c.id(),fields);}return result;}
 synchronized Map<String,Object> status(){long tx=sent,rx=received;if(client!=null&&client.session()!=null){tx+=client.session().numSent();rx+=client.session().numReceived();}return map("mode",mode,"viewers",viewers.size(),"subscriptions",subscribed.size(),"connects",connects,"disconnects",disconnects,"subscriptionAdds",subAdds,"subscriptionRemoves",subRemoves,"initialReads",reads,"covEvents",cov.get(),"daspSent",tx,"daspReceived",rx,"lastError",lastError,"autoReconnect",false,"archive",false,"writesEnabled",ALLOW_WRITES,"fileTransfer",fileClient!=null&&fileClient==client);}
}

