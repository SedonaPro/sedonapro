import java.net.InetAddress;
import sedona.*;
import sedona.dasp.DaspSocket;
import sedona.sox.*;

/** Exercises native Modbus TCP master and slave only through loopback. */
public class ModbusSmoke {
  static SoxComponent add(SoxClient c, Schema s, SoxComponent parent, String qname,
                          String name, String[] keys, Value[] overrides) throws Exception {
    Type t = s.type(qname);
    Slot[] slots = t.configProps();
    Value[] values = new Value[slots.length];
    for (int i=0; i<slots.length; ++i) {
      values[i] = slots[i].def();
      for (int j=0; j<keys.length; ++j) if (slots[i].name.equals(keys[j])) values[i] = overrides[j];
    }
    return c.add(parent, t, name, values);
  }
  public static void main(String[] args) throws Exception {
    DaspSocket sock = DaspSocket.open(-1, null, DaspSocket.SESSION_QUEUING);
    SoxClient c = new SoxClient(sock, InetAddress.getByName("127.0.0.1"), 11876,
                               "admin", System.getenv("SEDONA_ADMIN_PASSWORD"));
    SoxComponent folder = null;
    try {
      { java.util.Hashtable<String, String> o = new java.util.Hashtable<String, String>(); o.put("dasp.absMax", "1024"); c.connect(o); }   // large schemas (pro profile)
      Schema s = c.readSchema();
      folder = add(c,s,c.loadApp(),"sys::Folder","mbtest",new String[0],new Value[0]);
      SoxComponent slave = add(c,s,folder,"sproModbusSlave::ModbusSlaveTcpService","slave",
          new String[]{"ipAddress","port","slaveAddr"},
          new Value[]{Str.make("127.0.0.1"),Int.make(15020),Int.make(1)});
      SoxComponent source = add(c,s,slave,"sproModbusSlave::ModbusSlavePointWord","source",
          new String[]{"address"},new Value[]{Int.make(0)});
      c.write(slave,slave.type.slot("enable"),Bool.TRUE);
      Thread.sleep(700);
      c.write(source,source.type.slot("in"),Int.make(1234));
      SoxComponent net = add(c,s,folder,"sproModbusMaster::ModbusSlaveTcpAsyncNetwork","master",
          new String[]{"ipAddress","port","pollPeriodMs"},
          new Value[]{Str.make("127.0.0.1"),Int.make(15020),Int.make(100)});
      SoxComponent device = add(c,s,net,"sproModbusMaster::ModbusSlaveDevice","dev",
          new String[]{"address","enable"},new Value[]{Int.make(1),Bool.TRUE});
      SoxComponent point = add(c,s,device,"sproModbusMaster::ModbusReadPointWord","word",
          new String[]{"address"},new Value[]{Int.make(0)});
      c.write(net,net.type.slot("enable"),Bool.TRUE);
      for (int expected : new int[]{1234,4321}) {
        c.write(source,source.type.slot("in"),Int.make(expected));
        Value actual = null;
        for (int attempt=0; attempt<40; ++attempt) {
          Thread.sleep(150);
          actual = c.readProp(point,point.type.slot("out"));
          if (actual.equals(Int.make(expected))) break;
        }
        if (!Int.make(expected).equals(actual)) throw new AssertionError("Modbus expected="+expected+" got="+actual);
        System.out.println("MODBUS TCP native slave -> master PASS value="+actual);
      }
    } finally {
      if (folder != null && !c.isClosed()) c.delete(folder);
      c.close(); sock.close();
    }
  }
}
