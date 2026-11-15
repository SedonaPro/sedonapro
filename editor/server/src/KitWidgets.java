import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.util.regex.Pattern;
import java.util.zip.*;

/**
 * Widgets shipped with kits (docs/widgets.md): a device's m:ui.zip or the SDK's home/ui folder, read
 * with limits on size, count and names and handed to the browser as text. The browser checks each
 * template against the kit manifests and rebuilds the SVG from a whitelist; nothing here is run.
 */
final class KitWidgets {
  static final String FORMAT = "sedonapro-ui/1";
  static final int MAX_ZIP = 4 * 1024 * 1024, MAX_FILE = 256 * 1024, MAX_FILES = 1000;
  private static final long MAX_TOTAL = 16L * 1024 * 1024;
  private static final Pattern PATH =
      Pattern.compile("[A-Za-z][A-Za-z0-9_]{0,63}(/[A-Za-z0-9_-][A-Za-z0-9_.-]{0,99}){1,3}");
  private static final Pattern KEPT = Pattern.compile(".*\\.(widget\\.json|svg)|texts\\.[a-z]{2,3}\\.json");

  private KitWidgets() {}

  static boolean safe(String path) {
    return PATH.matcher(path).matches() && !path.contains("..");
  }

  /** {kits: {kit: {version, checksum, files: {path: text}}}} from a device's ui.zip. */
  static Map<String, Object> fromZip(byte[] zip) throws IOException {
    if (zip.length > MAX_ZIP) throw new IOException("ui.zip larger than " + MAX_ZIP + " bytes");
    Map<String, byte[]> entries = new LinkedHashMap<>();
    long total = 0;
    try (ZipInputStream in = new ZipInputStream(new ByteArrayInputStream(zip))) {
      for (ZipEntry e; (e = in.getNextEntry()) != null; ) {
        if (e.isDirectory()) continue;
        if (entries.size() >= MAX_FILES) throw new IOException("ui.zip has too many files");
        String name = e.getName();
        if (!name.equals("index.json") && !safe(name)) throw new IOException("ui.zip: bad file name " + name);
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] buf = new byte[8192];
        for (int n; (n = in.read(buf)) > 0; ) {
          out.write(buf, 0, n);
          if (out.size() > MAX_FILE) throw new IOException("ui.zip: a file larger than " + MAX_FILE);
        }
        total += out.size();
        if (total > MAX_TOTAL) throw new IOException("ui.zip too large unpacked");
        if (entries.put(name, out.toByteArray()) != null) throw new IOException("ui.zip: duplicate " + name);
      }
    }
    byte[] index = entries.get("index.json");
    if (index == null) throw new IOException("ui.zip has no index.json");
    Map<String, Object> r = Json.object(new String(index, StandardCharsets.UTF_8));
    if (!FORMAT.equals(r.get("format")) || !(r.get("kits") instanceof Map<?, ?> kits))
      throw new IOException("ui.zip: unknown index format");
    Map<String, Object> out = new TreeMap<>();
    for (var k : kits.entrySet()) {
      String kit = String.valueOf(k.getKey());
      Map<?, ?> info = k.getValue() instanceof Map<?, ?> m ? m : Map.of();
      Map<String, Object> files = new TreeMap<>();
      for (var e : entries.entrySet())
        if (e.getKey().startsWith(kit + "/")) {
          String rel = e.getKey().substring(kit.length() + 1);
          if (KEPT.matcher(rel).matches()) files.put(rel, new String(e.getValue(), StandardCharsets.UTF_8));
        }
      out.put(kit, DeviceGateway.map("version", String.valueOf(info.get("version")), "checksum", String.valueOf(info.get("checksum")), "files", files));
    }
    return out;
  }

  /** The same from an SDK's ui folder (one sub-folder per kit). */
  static Map<String, Object> fromFolder(Path ui) throws IOException {
    Map<String, Object> out = new TreeMap<>();
    if (!Files.isDirectory(ui)) return out;
    try (var kits = Files.list(ui)) {
      for (Path dir : kits.sorted().toList()) {
        if (!Files.isDirectory(dir) || Files.isSymbolicLink(dir)) continue;
        String kit = dir.getFileName().toString();
        Map<String, Object> files = new TreeMap<>();
        try (var walk = Files.walk(dir, 3)) {
          for (Path f : walk.filter(Files::isRegularFile).toList()) {
            String rel = dir.relativize(f).toString().replace('\\', '/');
            if (!safe(kit + "/" + rel) || !KEPT.matcher(rel).matches() || Files.isSymbolicLink(f)) continue;
            if (Files.size(f) > MAX_FILE || files.size() >= MAX_FILES) continue;
            files.put(rel, Files.readString(f, StandardCharsets.UTF_8));
          }
        }
        out.put(kit, DeviceGateway.map("version", "", "checksum", "", "files", files));
      }
    }
    return out;
  }
}
