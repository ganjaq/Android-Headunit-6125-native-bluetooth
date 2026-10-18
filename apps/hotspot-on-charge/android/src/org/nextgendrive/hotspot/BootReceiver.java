package org.nextgendrive.hotspot;
import android.content.*;
import android.os.UserManager;
import android.util.Log;
public final class BootReceiver extends BroadcastReceiver {
 @Override public void onReceive(Context c,Intent i){
  String a=i.getAction();
  if(!Intent.ACTION_BOOT_COMPLETED.equals(a)&&!Intent.ACTION_LOCKED_BOOT_COMPLETED.equals(a)&&!Intent.ACTION_MY_PACKAGE_REPLACED.equals(a))return;
  boolean unlocked=c.getSystemService(UserManager.class).isUserUnlocked();
  Context storage=unlocked?c:c.createDeviceProtectedStorageContext();
  boolean armed=storage.getSharedPreferences(unlocked?"ngd":"ngd_boot",0).getBoolean("armed",false);
  Log.i("NGDHotspot","BOOT_RECEIVER event="+a+" unlocked="+unlocked+" armed="+armed);
  if(armed)try{c.startForegroundService(new Intent(c,PowerService.class).setAction(a));}
  catch(RuntimeException e){Log.e("NGDHotspot","BOOT_SERVICE_BLOCKED "+e.getClass().getSimpleName());}
 }
}