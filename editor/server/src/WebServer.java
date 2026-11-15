import com.sun.net.httpserver.*;import java.net.*;import java.nio.file.*;import java.io.*;import java.util.*;import java.util.concurrent.*;
public final class WebServer {
 private static final DeviceGateway gateway=new DeviceGateway();private static final Semaphore streams=new Semaphore(8);
 private static final Path web=Paths.get("web/dist").toAbsolutePath().normalize();
 public static void main(String[]args)throws Exception{
  int port=Integer.parseInt(System.getProperty("port","8098"));HttpServer server=HttpServer.create(new InetSocketAddress("127.0.0.1",port),32);
  server.setExecutor(new ThreadPoolExecutor(4,20,30,TimeUnit.SECONDS,new ArrayBlockingQueue<>(32)));
  server.createContext("/api/",WebServer::api);server.createContext("/",WebServer::file);
  ScheduledExecutorService cleanup=Executors.newSingleThreadScheduledExecutor();cleanup.scheduleAtFixedRate(gateway::sweep,1,1,TimeUnit.SECONDS);
  Runtime.getRuntime().addShutdownHook(new Thread(()->{gateway.close();cleanup.shutdownNow();server.stop(0);}));server.start();System.out.println("Sedona Web: http://127.0.0.1:"+port+" ; OFF by default, local access; writes require explicit startup enable");
 }
 static void api(HttpExchange x)throws IOException{
  try{
   String origin=x.getRequestHeaders().getFirst("Origin"),host=x.getRequestHeaders().getFirst("Host");
   if(host==null||!host.matches("(127\\.0\\.0\\.1|localhost):[0-9]+"))throw new IllegalArgumentException("Local host required");
   if(origin!=null&&!origin.equals("http://"+host))throw new IllegalArgumentException("Same origin required");
   String path=x.getRequestURI().getPath();boolean get=x.getRequestMethod().equals("GET");
   if(path.equals("/api/events")&&get){events(x);return;}
   if(path.equals("/api/kits/export")&&get){byte[] bytes=gateway.exportManifests();x.getResponseHeaders().set("Content-Type","application/zip");x.getResponseHeaders().set("Content-Disposition","attachment; filename=sedona-manifests.zip");x.getResponseHeaders().set("Cache-Control","no-store");x.sendResponseHeaders(200,bytes.length);try(OutputStream out=x.getResponseBody()){out.write(bytes);}return;}
   Object result;
   if(path.equals("/api/catalog")&&get)result=gateway.catalog();
   else if(path.equals("/api/kits/import")&&x.getRequestMethod().equals("POST")){byte[] payload=x.getRequestBody().readNBytes(8*1024*1024+1);if(payload.length>8*1024*1024)throw new IllegalArgumentException("Kit upload exceeds 8 MiB");result=gateway.importKits(payload);}
   else if(path.equals("/api/status")&&get)result=gateway.status();
   else if(path.equals("/api/graph")&&get)result=gateway.graph();
   else if(path.equals("/api/ui/list")&&get)result=ViewStore.list();
   else{
    if(!x.getRequestMethod().equals("POST")){reply(x,405,DeviceGateway.map("error","POST required"));return;}
    int limit=path.startsWith("/api/ui/")?ViewStore.LIMIT+4096:65536;byte[] bytes=x.getRequestBody().readNBytes(limit+1);if(bytes.length>limit)throw new IllegalArgumentException("Request too large");Map<String,Object> b=Json.object(new String(bytes,java.nio.charset.StandardCharsets.UTF_8));
    switch(path){
     case "/api/ui/save":result=ViewStore.save((Map<String,Object>)b.get("document"));break;
     case "/api/ui/load":result=ViewStore.load(str(b,"id"));break;
     case "/api/ui/device/save":result=gateway.saveUi(str(b,"consumer"),(Map<String,Object>)b.get("document"));break;
     case "/api/ui/device/load":result=gateway.loadUi(str(b,"consumer"));break;
     case "/api/connect":int port=((Number)b.getOrDefault("port",1876)).intValue();if(port<1||port>65535)throw new IllegalArgumentException("Port range");String hostName=str(b,"host"),userName=str(b,"username"),password=str(b,"password");if(password.isEmpty()&&b.get("credentialId") instanceof String&&!((String)b.get("credentialId")).isEmpty())password=ProfileSecrets.resolve((String)b.get("credentialId"),hostName,port,userName);result=gateway.connect(hostName,port,userName,password,str(b,"mode"));break;
     case "/api/profiles/password":result=ProfileSecrets.save(str(b,"host"),integer(b,"port"),str(b,"username"),str(b,"password"));break;
     case "/api/profiles/forget":ProfileSecrets.forget(str(b,"credentialId"));result=DeviceGateway.map("forgotten",true);break;
     case "/api/attach":result=gateway.attach();break;
     case "/api/watch":List<Integer> ids=new ArrayList<>();for(Object id:(List<?>)b.getOrDefault("ids",Collections.emptyList()))ids.add(((Number)id).intValue());gateway.watch(str(b,"consumer"),ids);result=gateway.status();break;
     case "/api/release":gateway.release(str(b,"consumer"));result=gateway.status();break;
     case "/api/disconnect":gateway.close();result=gateway.status();break;
     case "/api/model/restart":result=gateway.restartModel(str(b,"consumer"),integer(b,"componentId"),str(b,"token"));break;
     case "/api/kitui":result=gateway.kitUi(str(b,"consumer"));break;
     case "/api/action/prepare":result=gateway.prepareAction(str(b,"consumer"),integer(b,"componentId"),str(b,"action"),(Map<?,?>)b.getOrDefault("values",Collections.emptyMap()));break;
     case "/api/action/commit":result=gateway.commitAction(str(b,"consumer"),str(b,"token"));break;
     case "/api/action/cancel":gateway.cancelAction(str(b,"consumer"),str(b,"token"));result=DeviceGateway.map("cancelled",true);break;
     case "/api/write/prepare":result=gateway.prepareWrite(str(b,"consumer"),integer(b,"componentId"),integer(b,"slotId"),str(b,"value"));break;
     case "/api/write/commit":result=gateway.commitWrite(str(b,"consumer"),str(b,"token"));break;
     case "/api/write/cancel":gateway.cancelWrite(str(b,"consumer"),str(b,"token"));result=DeviceGateway.map("cancelled",true);break;
     case "/api/layout/save":result=gateway.saveLayout(str(b,"consumer"),(List<?>)b.get("changes"));break;
     case "/api/component/create":result=gateway.createComponent(str(b,"consumer"),str(b,"type"),str(b,"kitPart"),integer(b,"parentId"),integer(b,"x"),integer(b,"y"),str(b,"token"));break;
     case "/api/component/duplicate":result=gateway.duplicateComponent(str(b,"consumer"),integer(b,"sourceId"),str(b,"token"));break;
     case "/api/link/change":result=gateway.changeLink(str(b,"consumer"),integer(b,"source"),integer(b,"sourceSlot"),integer(b,"target"),integer(b,"targetSlot"),Boolean.TRUE.equals(b.get("remove")),str(b,"token"));break;
     default:reply(x,404,DeviceGateway.map("error","Unknown endpoint"));return;
    }
   }reply(x,200,result);
  }catch(Exception e){reply(x,400,DeviceGateway.map("error",safeError(e)));}
 }
 static String safeError(Exception e){String m=e.getMessage();if(e instanceof IllegalArgumentException||e instanceof IllegalStateException)return m==null?"Invalid request":m;return "Device operation failed ("+e.getClass().getSimpleName()+"). Check address, credentials and manifests.";}
 static String str(Map<String,Object>b,String key){Object v=b.get(key);if(!(v instanceof String))throw new IllegalArgumentException("Missing field: "+key);return (String)v;}
 static int integer(Map<String,Object>b,String key){Object v=b.get(key);if(!(v instanceof Number))throw new IllegalArgumentException("Missing integer: "+key);double d=((Number)v).doubleValue();if(d<0||d>65535||d!=Math.rint(d))throw new IllegalArgumentException("Integer out of range: "+key);return (int)d;}
 static void events(HttpExchange x)throws IOException{
  String query=x.getRequestURI().getQuery(),consumer=query!=null&&query.startsWith("consumer=")?query.substring(9):"";
  if(!gateway.hasViewer(consumer)||!streams.tryAcquire()){reply(x,409,DeviceGateway.map("error","Stream unavailable"));return;}
  x.getResponseHeaders().set("Content-Type","text/event-stream; charset=utf-8");x.getResponseHeaders().set("Cache-Control","no-store");x.sendResponseHeaders(200,0);
  try(OutputStream out=x.getResponseBody()){while(gateway.hasViewer(consumer)){String event="data: "+Json.write(DeviceGateway.map("status",gateway.status(),"values",gateway.liveValues()))+"\n\n";out.write(event.getBytes(java.nio.charset.StandardCharsets.UTF_8));out.flush();if(!gateway.streamAlive(consumer))break;Thread.sleep(1000);}}
  catch(InterruptedException e){Thread.currentThread().interrupt();}catch(IOException ignored){}finally{streams.release();x.close();}
 }
 static void reply(HttpExchange x,int code,Object data)throws IOException{byte[] b=Json.write(data).getBytes(java.nio.charset.StandardCharsets.UTF_8);x.getResponseHeaders().set("Content-Type","application/json; charset=utf-8");x.getResponseHeaders().set("Cache-Control","no-store");x.sendResponseHeaders(code,b.length);try(OutputStream out=x.getResponseBody()){out.write(b);}}
 static void file(HttpExchange x)throws IOException{
  if(!x.getRequestMethod().equals("GET")){x.sendResponseHeaders(405,-1);x.close();return;}Path p=web.resolve(x.getRequestURI().getPath().substring(1)).normalize();
  if(!p.startsWith(web)){x.sendResponseHeaders(403,-1);x.close();return;}if(Files.isDirectory(p))p=p.resolve("index.html");if(!Files.exists(p)){x.sendResponseHeaders(404,-1);x.close();return;}
  String name=p.toString();String mime=name.endsWith(".html")?"text/html":name.endsWith(".js")?"text/javascript":name.endsWith(".css")?"text/css":name.endsWith(".svg")?"image/svg+xml":"application/octet-stream";
  byte[] b=Files.readAllBytes(p);x.getResponseHeaders().set("Content-Type",mime+"; charset=utf-8");if(name.endsWith(".html"))x.getResponseHeaders().set("Cache-Control","no-store");x.getResponseHeaders().set("X-Content-Type-Options","nosniff");x.sendResponseHeaders(200,b.length);try(OutputStream out=x.getResponseBody()){out.write(b);}
 }
}
