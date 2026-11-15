import sedona.*;import sedona.sox.*;import sedona.dasp.*;import java.net.*;
public class FixturePeer{
 public static void main(String[]args)throws Exception{DaspSocket socket=DaspSocket.open(-1,null,DaspSocket.SESSION_QUEUING);SoxClient c=new SoxClient(socket,InetAddress.getByName("127.0.0.1"),12986,"admin","localJarTest");try{c.connect();SoxComponent target=c.loadApp().child(System.getProperty("fixture.component","probe"));Slot out=target.type.slot(args[0].contains("meta")?"meta":"out");if(args[0].startsWith("write"))c.write(target,out,Value.defaultForType(out.type.id).decodeString(args[1]));System.out.println("PEER_VALUE="+c.readProp(target,out).encodeString());}finally{c.close();socket.close();}}
}
