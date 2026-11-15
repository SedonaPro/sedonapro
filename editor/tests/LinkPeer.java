import sedona.*;import sedona.sox.*;import sedona.dasp.*;import sedona.offline.*;import java.net.*;import java.io.*;
public class LinkPeer{
 public static void main(String[]args)throws Exception{
  if(args[0].equals("disk")){OfflineApp app=OfflineApp.decodeApp(new File(args[1]));System.out.println("DISK_LINKS="+app.getLinks().length);return;}
  DaspSocket socket=DaspSocket.open(-1,null,DaspSocket.SESSION_QUEUING);SoxClient c=new SoxClient(socket,InetAddress.getByName("127.0.0.1"),12986,"admin","localJarTest");try{c.connect();SoxComponent target=c.loadApp().child(args[1]);System.out.println("PEER_LINKS="+c.links(target).length);}finally{c.close();socket.close();}
 }
}
