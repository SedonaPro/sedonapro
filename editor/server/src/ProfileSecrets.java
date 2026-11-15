import java.nio.file.*;import java.nio.charset.StandardCharsets;import java.util.*;import java.util.concurrent.TimeUnit;
/** DPAPI CurrentUser. Plaintext crosses only an anonymous pipe, never command arguments or HTTP responses. */
final class ProfileSecrets {
 private static final Path root=Paths.get("data/credentials");
 private static Path file(String id){if(!id.matches("[a-f0-9-]{36}"))throw new IllegalArgumentException("Invalid saved password reference");return root.resolve(id+".json");}
 private static String binding(String host,int port,String user){return Json.write(Arrays.asList("mcs-sedona-profile-v1",host,port,user));}
 private static String crypt(String action,String value,String binding)throws Exception{
  if(!System.getProperty("os.name").startsWith("Windows"))throw new IllegalStateException("Saved passwords require Windows DPAPI");
  Process process=new ProcessBuilder("powershell.exe","-NoProfile","-NonInteractive","-ExecutionPolicy","Bypass","-File",Paths.get("tools/protect-profile.ps1").toAbsolutePath().toString()).start();
  java.util.concurrent.FutureTask<byte[]> capture=new java.util.concurrent.FutureTask<>(()->process.getInputStream().readNBytes(16385));Thread reader=new Thread(capture,"profile-dpapi-output");reader.setDaemon(true);reader.start();
  try{try(var out=process.getOutputStream()){out.write(Json.write(DeviceGateway.map("action",action,"value",value,"binding",binding)).getBytes(StandardCharsets.UTF_8));}
   if(!process.waitFor(15,TimeUnit.SECONDS)){process.destroyForcibly();throw new IllegalStateException("Windows credential protection timed out");}
   if(process.exitValue()!=0)throw new IllegalStateException("Windows could not protect/unprotect the saved password");
   byte[] output=capture.get(1,TimeUnit.SECONDS);if(output.length>16384)throw new IllegalStateException("Invalid DPAPI response");return (String)Json.object(new String(output,StandardCharsets.UTF_8).strip().replace("\uFEFF","")).get("value");
  }finally{process.destroy();}
 }
 static synchronized Map<String,Object> save(String host,int port,String user,String password)throws Exception{
  if(host.isBlank()||host.length()>253||user.length()>128||port<1||port>65535||password.length()>2048)throw new IllegalArgumentException("Invalid profile credentials");
  String cipher=crypt("protect",password,binding(host,port,user)),id=UUID.randomUUID().toString();Files.createDirectories(root);Path target=file(id),temp=root.resolve(id+".tmp");
  Files.writeString(temp,Json.write(DeviceGateway.map("host",host,"port",port,"username",user,"cipher",cipher)),StandardCharsets.UTF_8);Files.move(temp,target,StandardCopyOption.REPLACE_EXISTING);
  return DeviceGateway.map("credentialId",id,"protectedBy","Windows DPAPI CurrentUser");
 }
 static synchronized String resolve(String id,String host,int port,String user)throws Exception{
  Path path=file(id);if(!Files.exists(path))throw new IllegalStateException("Saved password missing; enter it again");Map<String,Object> saved=Json.object(Files.readString(path,StandardCharsets.UTF_8));
  if(!host.equals(saved.get("host"))||!user.equals(saved.get("username"))||((Number)saved.get("port")).intValue()!=port)throw new IllegalArgumentException("Saved password belongs to another address, port or login");
  return crypt("unprotect",(String)saved.get("cipher"),binding(host,port,user));
 }
 static synchronized void forget(String id)throws Exception{Files.deleteIfExists(file(id));}
}
