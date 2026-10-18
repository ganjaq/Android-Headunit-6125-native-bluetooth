package org.nextgendrive.hotspot;
import android.app.*;
import android.content.*;
import android.net.ConnectivityManager;
import android.net.wifi.WifiManager;
import android.os.*;
import android.provider.Settings;
import android.util.Log;

/** Charging automation only: no root, Bluetooth, SSID/password or mobile-data changes. */
public final class PowerService extends Service {
 private final Handler handler=new Handler();
 private SharedPreferences p,boot;
 private ConnectivityManager net; private WifiManager wifi;
 private boolean registered,pending; private int autoGeneration,startGeneration;
 private final BroadcastReceiver receiver=new BroadcastReceiver(){@Override public void onReceive(Context c,Intent i){
  String a=i.getAction();
  if(Intent.ACTION_USER_UNLOCKED.equals(a)){initialize();autoStart("USER_UNLOCKED");}
  else if(Intent.ACTION_POWER_CONNECTED.equals(a)){if(initialize())autoStart("POWER_CONNECTED");}
  else if(Intent.ACTION_POWER_DISCONNECTED.equals(a)){autoGeneration++;if(initialize())restore("POWER_DISCONNECTED");}
 }};
 static String state(Context c){try{return Integer.toString(c.getSystemService(WifiManager.class).getWifiApState());}catch(Throwable e){return "UNAVAILABLE: "+e.getClass().getSimpleName();}}
 static int bootCount(Context c){try{return Settings.Global.getInt(c.getContentResolver(),"boot_count",-1);}catch(RuntimeException e){return -1;}}
 private boolean unlocked(){return getSystemService(UserManager.class).isUserUnlocked();}
 private boolean plugged(){Intent b=registerReceiver(null,new IntentFilter(Intent.ACTION_BATTERY_CHANGED));return b!=null&&b.getIntExtra(BatteryManager.EXTRA_PLUGGED,0)!=0;}
 private boolean initialize(){
  if(!unlocked())return false;
  if(p==null){p=getSharedPreferences("ngd",0);net=getSystemService(ConnectivityManager.class);wifi=getSystemService(WifiManager.class);
   if(p.getBoolean("owned",false)&&!BootRecoveryPolicy.sameBoot(p.getInt("sessionBoot",-1),bootCount(this))){p.edit().putBoolean("owned",false).commit();result("OLD_BOOT_OWNERSHIP_INVALIDATED journal="+p.getBoolean("journal",false));}
  }
  boot.edit().putBoolean("armed",p.getBoolean("armed",false)).commit();return true;
 }
 @Override public void onCreate(){super.onCreate();
  boot=createDeviceProtectedStorageContext().getSharedPreferences("ngd_boot",0);
  getSystemService(NotificationManager.class).createNotificationChannel(new NotificationChannel("ngd-hotspot","NGD hotspot automation",NotificationManager.IMPORTANCE_LOW));
  initialize();IntentFilter f=new IntentFilter(Intent.ACTION_POWER_CONNECTED);f.addAction(Intent.ACTION_POWER_DISCONNECTED);f.addAction(Intent.ACTION_USER_UNLOCKED);registerReceiver(receiver,f);registered=true;
 }
 private void foreground(){
  boolean armed=p!=null?p.getBoolean("armed",false):boot.getBoolean("armed",false);
  Intent i=new Intent(this,MainActivity.class);
  Notification n=new Notification.Builder(this,"ngd-hotspot").setSmallIcon(android.R.drawable.stat_notify_sync)
   .setContentTitle("NGD hotspot — "+(!unlocked()?"odblokuj telefon":armed?"uzbrojony":"test"))
   .setContentText(!unlocked()?"Po restarcie odblokuj telefon PIN-em.":"Ładowanie → hotspot. Dotknij, aby wyłączyć.")
   .setOngoing(true).setContentIntent(PendingIntent.getActivity(this,0,i,PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE)).build();startForeground(31,n);
 }
 private void result(String s){Log.i("NGDHotspot",s);SharedPreferences target=p!=null?p:boot;target.edit().putString("result",s).putLong("time",System.currentTimeMillis()).commit();}
 @Override public IBinder onBind(Intent i){return null;}
 @Override public int onStartCommand(Intent i,int flags,int id){
  foreground();String a=i==null?"RESUME":i.getAction();
  boot.edit().putString("last_event",a).putInt("last_event_boot",bootCount(this)).commit();
  if(!initialize()){result("WAIT_FIRST_UNLOCK "+a);return boot.getBoolean("armed",false)?START_STICKY:START_NOT_STICKY;}
  if("TEST_ON".equals(a)){autoGeneration++;enable("EXPLICIT_TEST");}
  else if("ARM".equals(a)){p.edit().putBoolean("armed",true).commit();boot.edit().putBoolean("armed",true).commit();foreground();autoStart("ARM");}
  else if("DISARM".equals(a)){autoGeneration++;p.edit().putBoolean("armed",false).commit();boot.edit().putBoolean("armed",false).commit();foreground();restore("DISARM");}
  else if("RESTORE".equals(a)||"TEST_TIMEOUT_RESTORE".equals(a)){autoGeneration++;restore(a);}
  else autoStart(a);
  return p.getBoolean("armed",false)||p.getBoolean("owned",false)?START_STICKY:START_NOT_STICKY;
 }
 private void autoStart(String cause){int generation=++autoGeneration;autoAttempt(cause,generation,0);}
 private void autoAttempt(String cause,int generation,int attempt){
  if(generation!=autoGeneration||!initialize())return;
  if(!p.getBoolean("armed",false)){if(p.getBoolean("owned",false))restore(cause);else stopSelf();return;}
  foreground();
  if(!plugged()){result("ARMED_WAITING_FOR_CHARGER "+cause);return;}
  if(!Settings.System.canWrite(this)){result("BLOCKED_WRITE_SETTINGS_NOT_GRANTED");return;}
  int ap;try{ap=wifi.getWifiApState();}catch(Throwable e){result("UNSUPPORTED_AUTOMATION "+e.getClass().getSimpleName()+" — otwórz ustawienia hotspotu");return;}
  if(ap==WifiManager.WIFI_AP_STATE_ENABLED){if(!p.getBoolean("owned",false))result("ALREADY_ON_NOT_CHANGED owned=false");return;}
  if(!pending)enable(cause);
  if(!BootRecoveryPolicy.retryAllowed(attempt)){result("AUTO_START_TIMEOUT state="+ap+" "+cause);return;}
  handler.postDelayed(()->autoAttempt(cause,generation,attempt+1),2000);
 }
 private void enable(String cause){
  if(pending){result("PENDING_START");return;}
  if(!Settings.System.canWrite(this)){result("BLOCKED_WRITE_SETTINGS_NOT_GRANTED");return;}
  try{
   int ap=wifi.getWifiApState();
   if(ap==WifiManager.WIFI_AP_STATE_ENABLED){result("ALREADY_ON_NOT_CHANGED owned="+p.getBoolean("owned",false));return;}
   if(ap!=WifiManager.WIFI_AP_STATE_DISABLED){result("WAIT_AP_TRANSITION state="+ap+" "+cause);return;}
   if(p.getBoolean("journal",false)&&BootRecoveryPolicy.recyclable(ap,pending)){
    // Confirmed AP OFF closes a previous attempt, including legacy journals after power loss.
    // Do not restore a pre-reboot Wi-Fi preference into the current boot.
    result("RECOVER_CLOSED_AP_OFF old_boot="+p.getInt("sessionBoot",-1)+" current_boot="+bootCount(this));
    p.edit().putBoolean("journal",false).putBoolean("owned",false).commit();
   }
   p.edit().putBoolean("journal",true).putBoolean("wifiBefore",wifi.isWifiEnabled()).putBoolean("owned",false).putInt("sessionBoot",bootCount(this)).commit();pending=true;final int start=++startGeneration;
   handler.postDelayed(()->{if(pending&&start==startGeneration){pending=false;result("START_CALLBACK_TIMEOUT");}},20000);
   if("EXPLICIT_TEST".equals(cause))getSystemService(AlarmManager.class).setExactAndAllowWhileIdle(AlarmManager.ELAPSED_REALTIME_WAKEUP,SystemClock.elapsedRealtime()+45000,
    PendingIntent.getForegroundService(this,45,new Intent(this,PowerService.class).setAction("TEST_TIMEOUT_RESTORE"),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE));
   result("START_REQUEST "+cause);
   net.startTethering(ConnectivityManager.TETHERING_WIFI,false,new ConnectivityManager.OnStartTetheringCallback(){
    @Override public void onTetheringStarted(){if(start!=startGeneration)return;pending=false;p.edit().putBoolean("owned",true).commit();
     if(!plugged()&&!"EXPLICIT_TEST".equals(cause)){restore("UNPLUGGED_DURING_START");return;}
     handler.postDelayed(()->{if(wifi.getWifiApState()==WifiManager.WIFI_AP_STATE_ENABLED)result("PASS_HOTSPOT_ON "+cause);else result("FAIL_CALLBACK_WITHOUT_AP_ON state="+state(PowerService.this));},1000);}
    @Override public void onTetheringFailed(){if(start!=startGeneration)return;pending=false;result("FAIL_START_CALLBACK state="+state(PowerService.this));}
   },handler);
  }catch(Throwable e){pending=false;result("BLOCKED_START "+e.getClass().getName()+": "+e.getMessage());}
 }
 private void restore(String cause){restoreAttempt(cause,0);}
 private void restoreAttempt(String cause,int attempt){
  if(pending){if(attempt>=20){result("RESTORE_START_TIMEOUT "+cause);return;}result("WAIT_RESTORE_START_PENDING "+cause);handler.postDelayed(()->restoreAttempt(cause,attempt+1),1000);return;}
  if(!p.getBoolean("journal",false)){result("NO_OWN_CHANGE_TO_RESTORE "+cause);if(!p.getBoolean("armed",false))stopSelf();return;}
  try{
   int ap=wifi.getWifiApState();
   if(ap==WifiManager.WIFI_AP_STATE_ENABLED&&!p.getBoolean("owned",false)){result("BLOCKED_RESTORE_FOREIGN_OR_UNCONFIRMED_HOTSPOT");return;}
   if(ap==WifiManager.WIFI_AP_STATE_ENABLED)net.stopTethering(ConnectivityManager.TETHERING_WIFI);
   waitForOff(cause,0);
  }catch(Throwable e){result("RESTORE_FAILED "+e.toString());}
 }
 private void waitForOff(String cause,int tries){
  try{
   if(wifi.getWifiApState()!=WifiManager.WIFI_AP_STATE_DISABLED){if(tries>=40){result("RESTORE_TIMEOUT state="+state(this));return;}handler.postDelayed(()->waitForOff(cause,tries+1),500);return;}
   if(!BootRecoveryPolicy.sameBoot(p.getInt("sessionBoot",-1),bootCount(this))){
    p.edit().putBoolean("journal",false).putBoolean("owned",false).commit();result("PASS_OLD_BOOT_AP_OFF_CURRENT_WIFI_PRESERVED "+cause);if(!p.getBoolean("armed",false))stopSelf();return;
   }
   boolean before=p.getBoolean("wifiBefore",false);
   if(before!=wifi.isWifiEnabled()&&!wifi.setWifiEnabled(before)){result("RESTORE_WIFI_REQUEST_REJECTED");return;}
   verifyWifi(cause,before,0);
  }catch(Throwable e){result("RESTORE_FAILED "+e.toString());}
 }
 private void verifyWifi(String cause,boolean before,int tries){
  if(wifi.isWifiEnabled()!=before){if(tries>=20){result("RESTORE_WIFI_TIMEOUT");return;}handler.postDelayed(()->verifyWifi(cause,before,tries+1),500);return;}
  p.edit().putBoolean("journal",false).putBoolean("owned",false).commit();result("PASS_RESTORED_AP_OFF_WIFI_BASELINE "+cause);if(!p.getBoolean("armed",false))stopSelf();
 }
 @Override public void onDestroy(){autoGeneration++;if(registered)unregisterReceiver(receiver);handler.removeCallbacksAndMessages(null);super.onDestroy();}
}