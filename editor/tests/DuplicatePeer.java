import sedona.*;import sedona.sox.*;import sedona.dasp.*;import sedona.offline.*;import java.net.*;import java.io.*;
public class DuplicatePeer{
 public static void main(String[]args)throws Exception{
  if(args[0].equals("disk")){OfflineApp app=OfflineApp.decodeApp(new File(args[1]));OfflineComponent c=(OfflineComponent)app.child(args[2]);System.out.println("CONFIG="+c.get(c.type.slot("maxTime")).encodeString());return;}
  DaspSocket socket=DaspSocket.open(-1,null,DaspSocket.SESSION_QUEUING);SoxClient client=new SoxClient(socket,InetAddress.getByName("127.0.0.1"),12986,"admin","localJarTest");try{client.connect();SoxComponent c=client.loadApp().child(args[1]);System.out.println("CONFIG="+client.readProp(c,c.type.slot("maxTime")).encodeString());System.out.println("LINKS="+client.links(c).length);}finally{client.close();socket.close();}
 }
}
