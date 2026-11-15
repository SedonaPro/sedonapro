import java.net.InetAddress;
import sedona.*;
import sedona.dasp.DaspSocket;
import sedona.sox.*;

/** Integration check against an isolated development VM. */
public class SoxSmoke {
  static SoxComponent add(SoxClient client, SoxComponent parent, Type type, String name) throws Exception {
    Slot[] props = type.configProps();
    Value[] values = new Value[props.length];
    for (int i = 0; i < props.length; ++i) values[i] = props[i].def();
    return client.add(parent, type, name, values);
  }
  public static void main(String[] args) throws Exception {
    DaspSocket socket = DaspSocket.open(-1, null, DaspSocket.SESSION_QUEUING);
    SoxClient client = new SoxClient(socket, InetAddress.getByName(args[0]),
        Integer.parseInt(args[1]), "admin", System.getenv("SEDONA_ADMIN_PASSWORD"));
    SoxComponent folder = null;
    try {
      { java.util.Hashtable<String, String> o = new java.util.Hashtable<String, String>(); o.put("dasp.absMax", "1024"); client.connect(o); }   // large schemas (pro profile)
      System.out.println("CONNECTED " + client.readVersion().platformId);
      Buf manifestZip = new Buf();
      client.getFile("m:m.zip", SoxFile.make(manifestZip), null, null);
      if (manifestZip.size < 1000) throw new AssertionError("Invalid manifest archive");
      System.out.println("MANIFEST TRANSFER PASS bytes=" + manifestZip.size);
      Schema schema = client.readSchema();
      System.out.println("SCHEMA kits=" + schema.kits.length);
      SoxComponent root = client.loadApp();
      for (SoxComponent child : root.children()) System.out.println("ROOT " + child.name() + " " + child.type);
      folder = add(client, root, schema.type("sys::Folder"), "smoke");
      SoxComponent a = add(client, folder, schema.type("types::ConstFloat"), "const");
      SoxComponent b = add(client, folder, schema.type("math::Add2"), "sum");
      client.write(a, a.type.slot("out"), sedona.Float.make(12.5f));
      client.write(b, b.type.slot("in2"), sedona.Float.make(7.5f));
      Link link = new Link(a, a.type.slot("out"), b, b.type.slot("in1"));
      client.link(link);
      Thread.sleep(700);
      Value result = client.readProp(b, b.type.slot("out"));
      if (!result.equals(sedona.Float.make(20.0f))) throw new AssertionError("Expected 20.0, got " + result);
      System.out.println("EDIT add/write/link/execute/read PASS out=" + result);
      client.unlink(link);
      client.rename(b, "renamed");
      client.delete(folder);
      folder = null;
      client.invoke(root, root.type.slot("save"), null);
      System.out.println("EDIT unlink/rename/delete/save PASS");
    } finally {
      if (folder != null && !client.isClosed()) client.delete(folder);
      client.close();
      socket.close();
    }
  }
}
