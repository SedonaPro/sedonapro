import java.net.InetAddress;
import sedona.*;
import sedona.dasp.DaspSocket;
import sedona.sox.*;

/**
 * SOX checks against the ESP32 Sedona (simulator or chip).
 * usage: SoxProbe host port info|values|write|add|save|reboot
 * Password: SEDONA_ADMIN_PASSWORD.  Prints "PROBE <cmd> OK" on success.
 */
public class SoxProbe {
  static SoxClient client;

  static SoxComponent path(SoxComponent root, String p) throws Exception {
    SoxComponent c = root;
    for (String n : p.split("/")) {
      if (n.isEmpty()) continue;
      c = c.child(n);
      if (c == null) throw new Exception("no component " + p);
      c = client.load(c.id());
    }
    return c;
  }

  static Value read(SoxComponent c, String slot) throws Exception {
    return client.readProp(c, c.type.slot(slot));
  }

  static byte[] fetch(String uri) throws Exception {
    Buf b = new Buf();
    client.getFile(uri, SoxFile.make(b), null, null);
    return b.trim();
  }

  /**
   * Run with an empty sedona.home (empty manifest cache), like a fresh MCS:
   * the schema must load from the device alone; then m:m.zip and every
   * m:<kit>-<checksum>.xml are checked against the kits the device runs.
   */
  static void manifests() throws Exception {
    java.io.File cache = sedona.manifest.ManifestDb.dir;
    String[] before = cache.list();
    if (before != null && before.length > 0) throw new Exception("manifest cache not empty: " + cache);
    Schema schema = client.readSchema();          // downloads what is missing (m:m.zip)
    java.util.Map<String, String> want = new java.util.TreeMap<String, String>();
    for (Kit k : schema.kits) {
      KitPart part = new KitPart(k.name, k.manifest.checksum);
      want.put(k.name + "/" + part + ".xml", part.toString());
      java.io.File f = sedona.manifest.ManifestDb.toFile(part);
      if (!f.isFile()) throw new Exception("not in cache after readSchema: " + f);
    }
    System.out.println("schema loaded from device: " + schema.kits.length + " kits, cache " + cache);

    byte[] zip = fetch("m:m.zip");
    String out = System.getProperty("probe.out");
    if (out != null) java.nio.file.Files.write(java.nio.file.Paths.get(out), zip);
    java.util.Map<String, byte[]> entries = new java.util.TreeMap<String, byte[]>();
    java.util.zip.ZipInputStream zin = new java.util.zip.ZipInputStream(new java.io.ByteArrayInputStream(zip));
    for (java.util.zip.ZipEntry e; (e = zin.getNextEntry()) != null; )
    {
      java.io.ByteArrayOutputStream bo = new java.io.ByteArrayOutputStream();
      byte[] chunk = new byte[4096];
      for (int n; (n = zin.read(chunk)) > 0; ) bo.write(chunk, 0, n);
      entries.put(e.getName(), bo.toByteArray());
    }
    if (!entries.keySet().equals(want.keySet()))
      throw new Exception("m.zip entries " + entries.keySet() + " != device kits " + want.keySet());
    System.out.println("m:m.zip " + zip.length + " bytes, " + entries.size() + " entries = device kits");

    for (java.util.Map.Entry<String, String> w : want.entrySet()) {
      byte[] xml = fetch("m:" + w.getValue() + ".xml");
      if (!java.util.Arrays.equals(xml, entries.get(w.getKey())))
        throw new Exception("m:" + w.getValue() + ".xml differs from m.zip entry");
      sedona.manifest.KitManifest km = sedona.manifest.ManifestZipUtil.extract(new Buf(xml), KitPart.parse(w.getValue()));
      if (km == null || !km.part().toString().equals(w.getValue()))
        throw new Exception("m:" + w.getValue() + ".xml: name/checksum inside do not match");
      System.out.println("  " + w.getValue() + "  " + xml.length + " bytes  ok");
    }
    for (String bad : new String[] {"m:nokit-00000000.xml", "m:../svm.conf", "m:sys-00000000.xml"}) {
      try { fetch(bad); throw new IllegalStateException(bad + " was served"); }
      catch (IllegalStateException e) { throw e; }
      catch (Exception e) { System.out.println("  " + bad + " -> refused (" + e.getMessage() + ")"); }
    }
    SoxComponent root = client.loadApp();
    System.out.println("app loaded with downloaded schema: " + root.children().length + " top components");
  }

  /**
   * Files the device serves besides the manifests: m:ui.zip (the kits'
   * widgets) must equal -Dprobe.ui (the file in the image), and names
   * leaving the manifests directory must be refused.
   */
  static void files() throws Exception {
    String local = System.getProperty("probe.ui");
    if (local != null) {
      byte[] want = java.nio.file.Files.readAllBytes(java.nio.file.Paths.get(local));
      byte[] got = fetch("m:ui.zip");
      if (!java.util.Arrays.equals(want, got))
        throw new Exception("m:ui.zip: " + got.length + " bytes, the image has " + want.length);
      System.out.println("m:ui.zip " + got.length + " bytes = image");
    }
    for (String bad : new String[] {"m:../svm.conf", "m:..\\svm.conf", "m:a/../../svm.conf", "m:"}) {
      try { fetch(bad); throw new IllegalStateException(bad + " was served"); }
      catch (IllegalStateException e) { throw e; }
      catch (Exception e) { System.out.println("  " + bad + " -> refused"); }
    }
  }

  public static void main(String[] args) throws Exception {
    DaspSocket socket = DaspSocket.open(-1, null, DaspSocket.SESSION_QUEUING);
    client = new SoxClient(socket, InetAddress.getByName(args[0]),
        Integer.parseInt(args[1]), "admin", System.getenv("SEDONA_ADMIN_PASSWORD"));
    String cmd = args[2];
    try {
      { java.util.Hashtable<String, String> o = new java.util.Hashtable<String, String>(); o.put("dasp.absMax", "1024"); client.connect(o); }   // large schemas (pro profile)
      if (cmd.equals("manifests")) { manifests(); System.out.println("PROBE " + cmd + " OK"); return; }
      if (cmd.equals("files")) { files(); System.out.println("PROBE " + cmd + " OK"); return; }
      SoxComponent root = client.loadApp();
      if (cmd.equals("info")) {
        VersionInfo v = client.readVersion();
        System.out.println("platformId " + v.platformId + ", kits " + v.kits.length);
        SoxComponent plat = path(root, "service/plat");
        System.out.println("plat type " + plat.type.qname);
        for (String s : new String[] {"platformId", "platformVer", "memAvailable",
                                      "minFreeHeap", "uptime", "resetReason"})
          System.out.println("  " + s + " = " + read(plat, s));
      } else if (cmd.equals("values")) {
        SoxComponent adder = path(root, "play/adder");
        Value a = read(adder, "out");
        Thread.sleep(1500);
        Value b = read(adder, "out");
        System.out.println("adder.out " + a + " -> " + b);
        if (a.equals(b)) throw new Exception("adder.out does not change: app not running");
      } else if (cmd.equals("write")) {
        SoxComponent ramp = path(root, "play/rampB");
        client.write(ramp, ramp.type.slot("period"), sedona.Float.make(2.5f));
        Value p = read(ramp, "period");
        System.out.println("rampB.period = " + p);
        if (!p.toString().startsWith("2.5")) throw new Exception("write not applied");
      } else if (cmd.equals("add")) {
        SoxComponent play = path(root, "play");
        Type t = client.readSchema().type("math::Add2");
        String name = "a" + (System.currentTimeMillis() % 100000);   // names: max 7 chars
        Slot[] cp = t.configProps();
        Value[] defs = new Value[cp.length];
        for (int i = 0; i < cp.length; ++i) defs[i] = cp[i].def();
        SoxComponent c = client.add(play, t, name, defs);
        System.out.println("added /play/" + name + " id " + c.id());
      } else if (cmd.startsWith("get:")) {
        // get:/path/to/comp.slot[,/other.slot]
        for (String spec : cmd.substring(4).split(",")) {
          int dot = spec.lastIndexOf('.');
          SoxComponent c = path(root, spec.substring(0, dot));
          System.out.println(spec + " = " + read(c, spec.substring(dot + 1)));
        }
      } else if (cmd.startsWith("set:")) {
        // set:/path/to/comp.slot=value (bool, byte, short, int, float, Str)
        String spec = cmd.substring(4);
        int eq = spec.indexOf('='), dot = spec.lastIndexOf('.', eq);
        SoxComponent c = path(root, spec.substring(0, dot));
        Slot slot = c.type.slot(spec.substring(dot + 1, eq));
        String v = spec.substring(eq + 1);
        Value val;
        if (slot.isAsStr()) val = sedona.Str.make(v);
        else switch (slot.type.id) {
          case Type.boolId:  val = sedona.Bool.make(Boolean.parseBoolean(v)); break;
          case Type.byteId:  val = sedona.Byte.make(Integer.parseInt(v)); break;
          case Type.shortId: val = sedona.Short.make(Integer.parseInt(v)); break;
          case Type.intId:   val = sedona.Int.make(Integer.parseInt(v)); break;
          case Type.floatId: val = sedona.Float.make(java.lang.Float.parseFloat(v)); break;
          default: throw new Exception("unsupported slot type " + slot.type);
        }
        client.write(c, slot, val);
        System.out.println(spec.substring(0, eq) + " := " + read(c, slot.name));
      } else if (cmd.startsWith("act:")) {
        // act:/path/to/comp.action (no argument)
        String spec = cmd.substring(4);
        int dot = spec.lastIndexOf('.');
        SoxComponent c = path(root, spec.substring(0, dot));
        client.invoke(c, c.type.slot(spec.substring(dot + 1)), null);
      } else if (cmd.equals("list")) {
        SoxComponent play = path(root, "play");
        StringBuilder sb = new StringBuilder("play:");
        for (SoxComponent c : play.children()) sb.append(' ').append(c.name());
        System.out.println(sb);
      } else if (cmd.equals("save") || cmd.equals("restart")) {
        client.invoke(root, root.type.slot(cmd), null);
      } else if (cmd.equals("reboot")) {
        SoxComponent plat = path(root, "service/plat");
        // the chip resets before it answers: a lost reply is expected
        try { client.invoke(plat, plat.type.slot("reboot"), null); }
        catch (SoxException e) { System.out.println("no reply (chip is resetting)"); }
      } else {
        throw new Exception("unknown command " + cmd);
      }
      System.out.println("PROBE " + cmd + " OK");
    } finally {
      try { client.close(); } catch (Exception e) { }
      socket.close();
    }
  }
}
