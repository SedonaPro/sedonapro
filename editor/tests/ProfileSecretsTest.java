import java.nio.file.*;import java.util.*;
public class ProfileSecretsTest{
 public static void main(String[]args)throws Exception{
  String password="local-only-test-\u041f\u0430\u0440\u043e\u043b\u044c-\ud83d\udd12";Map<String,Object> saved=ProfileSecrets.save("127.0.0.1",12987,"admin",password);String id=(String)saved.get("credentialId");
  try{String disk=Files.readString(Paths.get("data/credentials",id+".json"));if(disk.contains(password)||disk.contains("local-only-test"))throw new AssertionError("Plaintext on disk");if(!ProfileSecrets.resolve(id,"127.0.0.1",12987,"admin").equals(password))throw new AssertionError("Unicode roundtrip failed");
   try{ProfileSecrets.resolve(id,"192.0.2.10",12987,"admin");throw new AssertionError("Endpoint binding missing");}catch(IllegalArgumentException expected){}
   try{ProfileSecrets.resolve("../invalid","127.0.0.1",12987,"admin");throw new AssertionError("Traversal accepted");}catch(IllegalArgumentException expected){}
  }finally{ProfileSecrets.forget(id);}if(Files.exists(Paths.get("data/credentials",id+".json")))throw new AssertionError("Forget failed");System.out.println("PASS DPAPI: unicode roundtrip, encrypted disk, endpoint binding, path validation and deletion");
 }
}
