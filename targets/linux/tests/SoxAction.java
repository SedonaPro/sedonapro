import java.net.InetAddress;
import sedona.dasp.DaspSocket;
import sedona.sox.*;

/**
 * Invoke one action on the app root of a local test VM.
 * usage: SoxAction host port save|restart
 * Password is read from SEDONA_ADMIN_PASSWORD.
 */
public class SoxAction {
  public static void main(String[] args) throws Exception {
    DaspSocket socket = DaspSocket.open(-1, null, DaspSocket.SESSION_QUEUING);
    SoxClient client = new SoxClient(socket, InetAddress.getByName(args[0]),
        Integer.parseInt(args[1]), "admin", System.getenv("SEDONA_ADMIN_PASSWORD"));
    try {
      { java.util.Hashtable<String, String> o = new java.util.Hashtable<String, String>(); o.put("dasp.absMax", "1024"); client.connect(o); }   // large schemas (pro profile)
      SoxComponent root = client.loadApp();
      client.invoke(root, root.type.slot(args[2]), null);
      System.out.println("ACTION " + args[2] + " OK");
    } finally {
      try { client.close(); } catch (Exception e) { }
      socket.close();
    }
  }
}
