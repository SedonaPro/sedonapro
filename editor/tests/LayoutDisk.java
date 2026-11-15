import sedona.offline.*;import java.io.*;
public class LayoutDisk{public static void main(String[]args)throws Exception{OfflineApp app=OfflineApp.decodeApp(new File(args[0]));OfflineComponent c=(OfflineComponent)app.child(args.length>1?args[1]:"probe");System.out.println("DISK_META="+c.get(c.type.slot("meta")).encodeString());}}
