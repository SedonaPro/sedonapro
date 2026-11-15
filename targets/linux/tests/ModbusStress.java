import java.net.InetAddress;
import sedona.*;
import sedona.dasp.DaspSocket;
import sedona.sox.*;

/**
 * Modbus Master stress over loopback on an isolated test VM.
 *
 * usage: ModbusStress soxPort modbusPort churn <n>
 *          add a read point, wait for its value, delete it; n times
 *        ModbusStress soxPort modbusPort blackhole <n>
 *          master connects to a server that never answers (every request is
 *          in flight for the 100 ms timeout); the point address is changed n
 *          times and printInternalStateBackend dumps the register to the VM log
 *        ModbusStress soxPort modbusPort readdr <n>
 *          device address 2 (no answer, 100 ms timeouts) -> 1 while a request
 *          is in flight; count how often the point recovers
 * Password: SEDONA_ADMIN_PASSWORD.
 */
public class ModbusStress {
  static Schema schema;

  static SoxComponent add(SoxClient c, SoxComponent parent, String qname, String name,
                          String[] keys, Value[] overrides) throws Exception {
    Type t = schema.type(qname);
    Slot[] slots = t.configProps();
    Value[] values = new Value[slots.length];
    for (int i = 0; i < slots.length; ++i) {
      values[i] = slots[i].def();
      for (int j = 0; j < keys.length; ++j) if (slots[i].name.equals(keys[j])) values[i] = overrides[j];
    }
    return c.add(parent, t, name, values);
  }

  static boolean waitValue(SoxClient c, SoxComponent point, int expected, int ms) throws Exception {
    long end = System.currentTimeMillis() + ms;
    while (System.currentTimeMillis() < end) {
      if (c.readProp(point, point.type.slot("out")).equals(Int.make(expected))) return true;
      Thread.sleep(50);
    }
    return false;
  }

  public static void main(String[] args) throws Exception {
    int soxPort = Integer.parseInt(args[0]), mbPort = Integer.parseInt(args[1]);
    String mode = args[2];
    int n = Integer.parseInt(args[3]);
    DaspSocket sock = DaspSocket.open(-1, null, DaspSocket.SESSION_QUEUING);
    SoxClient c = new SoxClient(sock, InetAddress.getByName("127.0.0.1"), soxPort,
                                "admin", System.getenv("SEDONA_ADMIN_PASSWORD"));
    SoxComponent folder = null;
    try {
      { java.util.Hashtable<String, String> o = new java.util.Hashtable<String, String>(); o.put("dasp.absMax", "1024"); c.connect(o); }   // large schemas (pro profile)
      schema = c.readSchema();
      folder = add(c, c.loadApp(), "sys::Folder", "mbst", new String[0], new Value[0]);
      SoxComponent slave = add(c, folder, "sproModbusSlave::ModbusSlaveTcpService", "slave",
          new String[]{"ipAddress", "port", "slaveAddr"},
          new Value[]{Str.make("127.0.0.1"), Int.make(mbPort), Int.make(1)});
      SoxComponent source = add(c, slave, "sproModbusSlave::ModbusSlavePointWord", "source",
          new String[]{"address"}, new Value[]{Int.make(0)});
      c.write(slave, slave.type.slot("enable"), Bool.TRUE);
      Thread.sleep(700);
      c.write(source, source.type.slot("in"), Int.make(1234));
      SoxComponent net = add(c, folder, "sproModbusMaster::ModbusSlaveTcpAsyncNetwork", "master",
          new String[]{"ipAddress", "port", "pollPeriodMs"},
          new Value[]{Str.make("127.0.0.1"), Int.make(mbPort), Int.make(50)});
      int devAddr = mode.equals("readdr") ? 2 : 1;
      SoxComponent device = add(c, net, "sproModbusMaster::ModbusSlaveDevice", "dev",
          new String[]{"address", "enable"}, new Value[]{Int.make(devAddr), Bool.TRUE});
      c.write(net, net.type.slot("enable"), Bool.TRUE);

      if (mode.equals("blackhole")) {
        // the slave above is not used; mbPort+1 is the silent server
        c.write(net, net.type.slot("enable"), Bool.FALSE);
        c.write(net, net.type.slot("port"), Int.make(mbPort + 1));
        c.write(net, net.type.slot("enable"), Bool.TRUE);
        SoxComponent point = add(c, device, "sproModbusMaster::ModbusReadPointWord", "w",
            new String[]{"address"}, new Value[]{Int.make(0)});
        Thread.sleep(3000);                        // connect and start timing out
        for (int i = 0; i < n; ++i) {
          c.write(point, point.type.slot("address"), Int.make(10 + i));
          Thread.sleep(400);
          c.invoke(point, point.type.slot("printInternalStateBackend"), null);
          Thread.sleep(100);
        }
        System.out.println("BLACKHOLE DONE " + n);
      } else if (mode.equals("churn")) {
        for (int i = 0; i < n; ++i) {
          SoxComponent point = add(c, device, "sproModbusMaster::ModbusReadPointWord", "w" + i,
              new String[]{"address"}, new Value[]{Int.make(0)});
          if (!waitValue(c, point, 1234, 5000)) throw new AssertionError("no value at iteration " + i);
          c.delete(point);
        }
        System.out.println("CHURN OK " + n);
      } else {
        SoxComponent point = add(c, device, "sproModbusMaster::ModbusReadPointWord", "w",
            new String[]{"address"}, new Value[]{Int.make(0)});
        int ok = 0;
        for (int i = 0; i < n; ++i) {
          int value = 2000 + i;                    // a value the point has not read yet
          c.write(device, device.type.slot("address"), Int.make(2));
          c.write(source, source.type.slot("in"), Int.make(value));
          Thread.sleep(800);                       // requests now time out (100 ms each)
          c.write(device, device.type.slot("address"), Int.make(1));
          if (waitValue(c, point, value, 2000)) ok++;
        }
        System.out.println("READDR recovered " + ok + "/" + n);
      }
    } finally {
      if (folder != null && !c.isClosed()) c.delete(folder);
      c.close();
      sock.close();
    }
  }
}
