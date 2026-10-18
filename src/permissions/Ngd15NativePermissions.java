import java.lang.reflect.*;
import java.io.*;
import java.util.*;
import java.nio.file.*;
import java.nio.file.attribute.*;
public class Ngd15NativePermissions {
 static final String PKG="org.codeaurora.bluetooth", FILE="/data/local/tmp/ngd15n-permissions.baseline";
 public static void main(String[] args) throws Exception {
  if(args.length!=1)throw new IllegalArgumentException("capture|restore|check");
  Class<?> sm=Class.forName("android.os.ServiceManager"),api=Class.forName("android.content.pm.IPackageManager");
  Object binder=sm.getMethod("getService",String.class).invoke(null,"package");
  Object pm=Class.forName("android.content.pm.IPackageManager$Stub").getMethod("asInterface",Class.forName("android.os.IBinder")).invoke(null,binder);
  Method get=api.getMethod("getPermissionFlags",String.class,String.class,int.class), check=api.getMethod("checkPermission",String.class,String.class,int.class),update=api.getMethod("updatePermissionFlags",String.class,String.class,int.class,int.class,boolean.class,int.class);
  int orphan=(Integer)check.invoke(pm,"android.permission.ACCESS_BLUETOOTH_SHARE",PKG,0);
  if(orphan!=-1)throw new IllegalStateException("STOP: pre-existing ACCESS_BLUETOOTH_SHARE grant; do not revoke blindly");
  if(args[0].equals("assert-no-share-grant")){System.out.println("SHARE_GRANT_ABSENT");return;}
  String[] perms;
  TreeMap<String,int[]> saved=new TreeMap<>();
  if(args[0].equals("capture")) {
   Object info=api.getMethod("getPackageInfo",String.class,int.class,int.class).invoke(pm,PKG,4096,0);
   perms=(String[])info.getClass().getField("requestedPermissions").get(info);
   for(String perm:perms)saved.put(perm,new int[]{(Integer)get.invoke(pm,perm,PKG,0),(Integer)check.invoke(pm,perm,PKG,0)});
   Path f=Paths.get(FILE);
   Set<PosixFilePermission> mode=PosixFilePermissions.fromString("rw-------");
   try(BufferedWriter w=Files.newBufferedWriter(f,java.nio.charset.StandardCharsets.UTF_8,StandardOpenOption.CREATE_NEW,StandardOpenOption.WRITE)) {
    Files.setPosixFilePermissions(f,mode);
    for(Map.Entry<String,int[]> e:saved.entrySet())w.write(e.getKey()+" "+e.getValue()[0]+" "+e.getValue()[1]+"\n");
   }
   System.out.println("PERMISSION_BASELINE_SAVED "+saved.size());return;
  }
  Path f=Paths.get(FILE);
  if(Files.isSymbolicLink(f)||!Files.getOwner(f).getName().equals("root")||!Files.getPosixFilePermissions(f).equals(PosixFilePermissions.fromString("rw-------")))throw new IllegalStateException("Unsafe baseline owner/mode");
  for(String row:Files.readAllLines(f,java.nio.charset.StandardCharsets.UTF_8)){String[] v=row.split(" ");if(v.length!=3||!v[0].matches("[A-Za-z0-9_.]+"))throw new IllegalStateException("Invalid baseline");saved.put(v[0],new int[]{Integer.parseInt(v[1]),Integer.parseInt(v[2])});}
  int changed=0;
  for(Map.Entry<String,int[]> e:saved.entrySet()) {
   String perm=e.getKey();int flags=(Integer)get.invoke(pm,perm,PKG,0),wanted=e.getValue()[0],diff=flags^wanted;
   if(diff!=0&&args[0].equals("restore")){update.invoke(pm,perm,PKG,diff,wanted,false,0);++changed;}
   int actual=(Integer)get.invoke(pm,perm,PKG,0),grant=(Integer)check.invoke(pm,perm,PKG,0);
   if(actual!=wanted||grant!=e.getValue()[1])throw new IllegalStateException("Permission differs: "+perm+" flags="+actual+" expected="+wanted+" grant="+grant+" expected="+e.getValue()[1]);
  }
  System.out.println("PERMISSION_BASELINE_MATCH count="+saved.size()+" flags_restored="+changed+" orphan_grant_absent=true");
 }
}
