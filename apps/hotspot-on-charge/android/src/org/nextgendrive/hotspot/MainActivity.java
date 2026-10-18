package org.nextgendrive.hotspot;
import android.app.Activity;
import android.content.*;
import android.graphics.Color;
import android.net.Uri;
import android.os.*;
import android.provider.Settings;
import android.util.Log;
import android.view.View;
import android.widget.*;

/** Everyday control surface. Diagnostic controls are optional and collapsed. */
public final class MainActivity extends Activity {
 private final Handler handler=new Handler();
 private TextView headline,detail,diagnostic;
 private Switch toggle;
 private LinearLayout advanced;
 private Button settings;
 private boolean audit,rendering,enableAfterPermission;
 private final Runnable refresh=new Runnable(){public void run(){show();handler.postDelayed(this,1500);}};
 private SharedPreferences prefs(){return getSharedPreferences("ngd",0);}
 private int dp(int n){return Math.round(n*getResources().getDisplayMetrics().density);}
 private TextView label(LinearLayout root,String text,int size,int color){
  TextView t=new TextView(this);t.setText(text);t.setTextSize(size);t.setTextColor(color);t.setPadding(0,dp(8),0,dp(8));root.addView(t);return t;
 }
 @Override public void onCreate(Bundle state){super.onCreate(state);
  audit="AUDIT_ONLY".equals(getIntent().getStringExtra("mode"));
  if(audit){Log.i("NGDHotspotAudit",status());finish();return;}
  enableAfterPermission=state!=null&&state.getBoolean("enableAfterPermission",false);
  LinearLayout root=new LinearLayout(this);root.setOrientation(1);root.setPadding(dp(24),dp(24),dp(24),dp(24));root.setBackgroundColor(Color.WHITE);
  label(root,"NGD Hotspot",28,Color.rgb(24,42,57));
  label(root,"Internet dla radia, gdy ładujesz telefon.",16,Color.DKGRAY);
  headline=label(root,"",23,Color.rgb(0,112,96));
  detail=label(root,"",16,Color.DKGRAY);
  toggle=new Switch(this);toggle.setText("Automatycznie przy ładowaniu");toggle.setTextSize(18);toggle.setPadding(0,dp(24),0,dp(24));root.addView(toggle);
  toggle.setOnCheckedChangeListener((v,on)->{if(rendering)return;
   if(on&&!Settings.System.canWrite(this)){enableAfterPermission=true;openPermission();return;}
   action(on?"ARM":"DISARM");
  });
  settings=button(root,"Zezwól na zmianę ustawień",()->{enableAfterPermission=true;openPermission();});
  label(root,"Możesz zamknąć ten ekran. Automat działa w tle i dotyczy każdego ładowania, także poza autem. Po restarcie odblokuj telefon przynajmniej raz.",14,Color.GRAY);
  Button more=button(root,"Opcje zaawansowane",()->{});
  advanced=new LinearLayout(this);advanced.setOrientation(1);advanced.setVisibility(View.GONE);root.addView(advanced);
  more.setOnClickListener(v->{boolean visible=advanced.getVisibility()!=View.VISIBLE;advanced.setVisibility(visible?View.VISIBLE:View.GONE);more.setText(visible?"Ukryj opcje zaawansowane":"Opcje zaawansowane");show();});
  button(advanced,"Autostart i działanie w tle",()->openSettings("huawei.intent.action.HSM_STARTUPAPP_MANAGER",Settings.ACTION_APPLICATION_DETAILS_SETTINGS));
  button(advanced,"Optymalizacja baterii",()->openSettings(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS,Settings.ACTION_SETTINGS));
  button(advanced,"Ustawienia hotspotu",()->openSettings("android.settings.TETHER_SETTINGS",Settings.ACTION_WIRELESS_SETTINGS));
  button(advanced,"Test hotspotu — 45 sekund",()->new android.app.AlertDialog.Builder(this).setMessage("Test ma własny limit 45 sekund i spróbuje potem wyłączyć uruchomiony przez siebie hotspot. Automat nie ma tego limitu.").setPositiveButton("Uruchom test",(d,w)->action("TEST_ON")).setNegativeButton("Anuluj",null).show());
  button(advanced,"Cofnij test / zatrzymaj własny hotspot",()->action("RESTORE"));
  diagnostic=label(advanced,"",12,Color.GRAY);diagnostic.setTextIsSelectable(true);
  ScrollView scroll=new ScrollView(this);scroll.addView(root);setContentView(scroll);show();
 }
 @Override protected void onSaveInstanceState(Bundle out){out.putBoolean("enableAfterPermission",enableAfterPermission);super.onSaveInstanceState(out);}
 @Override protected void onResume(){super.onResume();if(audit)return;
  if(enableAfterPermission&&Settings.System.canWrite(this)){enableAfterPermission=false;action("ARM");}
  else if(prefs().getBoolean("armed",false))action("RESUME");
  handler.removeCallbacks(refresh);handler.post(refresh);
 }
 @Override protected void onPause(){handler.removeCallbacks(refresh);super.onPause();}
 @Override protected void onNewIntent(Intent i){super.onNewIntent(i);setIntent(i);if("AUDIT_ONLY".equals(i.getStringExtra("mode")))Log.i("NGDHotspotAudit",status());}
 private void openPermission(){try{startActivity(new Intent(Settings.ACTION_MANAGE_WRITE_SETTINGS,Uri.parse("package:"+getPackageName())));}catch(RuntimeException e){detail.setText("Otwórz informacje aplikacji i zezwól na zmianę ustawień systemowych.");}}
 private void openSettings(String first,String fallback){Intent i=new Intent(first);if(i.resolveActivity(getPackageManager())==null){i=new Intent(fallback);if(Settings.ACTION_APPLICATION_DETAILS_SETTINGS.equals(fallback))i.setData(Uri.parse("package:"+getPackageName()));}startActivity(i);}
 private Button button(LinearLayout r,String text,Runnable f){Button b=new Button(this);b.setText(text);b.setAllCaps(false);b.setOnClickListener(v->{try{f.run();}catch(Throwable e){detail.setText("Nie można otworzyć opcji. Sprawdź ustawienia aplikacji w telefonie.");}});r.addView(b);return b;}
 private void action(String a){try{startForegroundService(new Intent(this,PowerService.class).setAction(a));handler.postDelayed(()->show(),500);}catch(RuntimeException e){detail.setText("Nie można uruchomić automatu. Sprawdź zgodę na pracę aplikacji w tle.");}}
 private boolean charging(){Intent b=registerReceiver(null,new IntentFilter(Intent.ACTION_BATTERY_CHANGED));return b!=null&&b.getIntExtra(BatteryManager.EXTRA_PLUGGED,0)!=0;}
 private void show(){if(headline==null)return;
  boolean armed=prefs().getBoolean("armed",false),allowed=Settings.System.canWrite(this);
  String ap=PowerService.state(this),result=prefs().getString("result","");
  rendering=true;toggle.setChecked(armed||enableAfterPermission);rendering=false;
  settings.setVisibility(allowed?View.GONE:View.VISIBLE);
  if(!allowed){headline.setText("Potrzebna jednorazowa zgoda");detail.setText("Zezwól aplikacji na zmianę ustawień. Później hotspot będzie uruchamiany automatycznie przy ładowaniu.");}
  else if("13".equals(ap)){headline.setText("Hotspot jest włączony");detail.setText(armed?"Automat jest aktywny. Radio może połączyć się z siecią Wi-Fi telefonu.":"Automat jest wyłączony. Hotspot działa niezależnie od reguły ładowania.");}
  else if(!armed){headline.setText("Automat jest wyłączony");detail.setText("Włącz przełącznik poniżej, aby uruchamiać hotspot przy podłączeniu ładowarki.");}
  else if(!charging()){headline.setText("Gotowy — czeka na ładowanie");detail.setText("Po podłączeniu ładowarki aplikacja spróbuje włączyć hotspot.");}
  else if(result.startsWith("BLOCKED")||result.startsWith("UNSUPPORTED")||result.startsWith("FAIL")||result.startsWith("AUTO_START_TIMEOUT")){headline.setText("Nie udało się włączyć hotspotu");detail.setText("Sprawdź autostart i pracę w tle w opcjach zaawansowanych. Jeśli telefon blokuje automat, włącz hotspot w ustawieniach telefonu.");}
  else{headline.setText("Uruchamianie hotspotu…");detail.setText("Telefon jest podłączony do ładowania. Czekam na potwierdzenie włączenia hotspotu.");}
  if(advanced.getVisibility()==View.VISIBLE)diagnostic.setText(status());
 }
 private String status(){SharedPreferences p=prefs(),b=createDeviceProtectedStorageContext().getSharedPreferences("ngd_boot",0);
  return "NGD Hotspot 0.3 / Android "+Build.VERSION.RELEASE+"\nUzbrojenie: "+p.getBoolean("armed",false)+"\nZmiana ustawień: "+Settings.System.canWrite(this)+"\nStan AP: "+PowerService.state(this)+"\nWynik: "+p.getString("result","Brak testu")+"\nJournal: "+p.getBoolean("journal",false)+" / owned: "+p.getBoolean("owned",false)+"\nBoot sesji / obecny: "+p.getInt("sessionBoot",-1)+" / "+PowerService.bootCount(this)+"\nOstatni start: "+b.getString("last_event","brak")+" / boot "+b.getInt("last_event_boot",-1);
 }
}
