// NEXTGENDRIVE: AOSP Java metadata to the existing QTI legacy AVRCP interface.
// Copyright 2026 NEXTGENDRIVE contributors. SPDX-License-Identifier: Apache-2.0
// No adapter/HCI ownership, pairing, volume, or audio routing operations.
#include <jni.h>
#include <android/log.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dlfcn.h>
#include <sys/cdefs.h>
struct RawAddress { uint8_t address[6]; };
using bt_status_t = int;
#include "bt_rc_ext.h"
static_assert(sizeof(RawAddress)==6, "RawAddress ABI");
static_assert(sizeof(btrc_interface_t)==192, "QTI target ABI from binary");
static_assert(sizeof(btrc_callbacks_t)==184, "QTI callback ABI");
static_assert(offsetof(btrc_callbacks_t,connection_state_cb)==176, "QTI callbacks");
static_assert(offsetof(btrc_interface_t,register_notification_rsp)==80, "QTI rsp ABI");
static_assert(offsetof(btrc_interface_t,cleanup)==184, "QTI cleanup ABI");
struct BtApiPrefix { size_t size; void* prefix[21]; const void*(*get_profile_interface)(const char*); };
static_assert(offsetof(BtApiPrefix,get_profile_interface)==176, "Verified BT ABI");
#define LOG(...) __android_log_print(ANDROID_LOG_INFO,"NgdAvrcpLegacy",__VA_ARGS__)
static JavaVM* vm;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static jobject java_object;
static jmethodID song_method,status_method,key_method;
static jfieldID attrs_fields[7],status_state,status_duration,status_position;
static const btrc_interface_t* rc;
static bool running;
static uint64_t track_serial=1;
struct Peer { bool used; RawAddress addr; uint32_t subscriptions; };
static Peer peers[2];
struct Env {
 JNIEnv* e=nullptr; bool attached=false;
 Env(){ if(vm && vm->GetEnv((void**)&e,JNI_VERSION_1_6)==JNI_EDETACHED){attached=vm->AttachCurrentThread(&e,nullptr)==JNI_OK;} }
 ~Env(){if(attached)vm->DetachCurrentThread();}
};
static jobject object_ref(JNIEnv* e){pthread_mutex_lock(&mutex);jobject o=running&&java_object?e->NewLocalRef(java_object):nullptr;pthread_mutex_unlock(&mutex);return o;}
static bool exception(JNIEnv* e){if(!e->ExceptionCheck())return false;e->ExceptionClear();LOG("Java callback exception suppressed; no payload logged");return true;}
static size_t utf8(const uint16_t* s,size_t len,char* out,size_t cap){
 size_t n=0;if(!cap)return 0;
 for(size_t i=0;i<len;i++){
  uint32_t c=s[i];
  if(c>=0xd800&&c<=0xdbff){if(i+1<len&&s[i+1]>=0xdc00&&s[i+1]<=0xdfff)c=0x10000+((c-0xd800)<<10)+(s[++i]-0xdc00);else c=0xfffd;}
  else if(c>=0xdc00&&c<=0xdfff)c=0xfffd;
  unsigned bytes=c<0x80?1:c<0x800?2:c<0x10000?3:4;if(n+bytes>=cap)break;
  if(bytes==1)out[n++]=(char)c;
  else {for(unsigned k=bytes-1;k>0;k--)out[n+k]=(char)(0x80|((c>>(6*(bytes-1-k)))&63));out[n]=(char)((bytes==2?0xc0:bytes==3?0xe0:0xf0)|(c>>(6*(bytes-1))));n+=bytes;}
 }
 out[n]=0;return n;
}
struct Status { btrc_play_status_t state=BTRC_PLAYSTATE_STOPPED; uint32_t duration=0,position=0xffffffff; };
static Status status(){
 Status s;Env en;if(!en.e)return s;JNIEnv* e=en.e;jobject o=object_ref(e);if(!o)return s;
 jobject p=e->CallObjectMethod(o,status_method);
 if(!exception(e)&&p){s.state=(btrc_play_status_t)(uint8_t)e->GetByteField(p,status_state);jlong d=e->GetLongField(p,status_duration),v=e->GetLongField(p,status_position);s.duration=d<0?0:d>0xffffffffLL?0xffffffff:(uint32_t)d;s.position=v<0?0xffffffff:v>0xffffffffLL?0xffffffff:(uint32_t)v;e->DeleteLocalRef(p);}
 e->DeleteLocalRef(o);return s;
}
static btrc_element_attr_val_t* attributes(uint8_t count,const btrc_media_attr_t* ids,uint8_t* returned){
 *returned=0;if(count==255)return nullptr;uint32_t wanted[8];unsigned n=0;
 if(count==0){for(unsigned i=1;i<=7;i++)wanted[n++]=i;}
 else if(ids){for(unsigned i=0;i<count&&n<8;i++){unsigned id=(unsigned)ids[i];if(id<1||id>7)continue;bool duplicate=false;for(unsigned j=0;j<n;j++)if(wanted[j]==id)duplicate=true;if(!duplicate)wanted[n++]=id;}}
 if(!n)return nullptr;
 auto* result=(btrc_element_attr_val_t*)calloc(n,sizeof(btrc_element_attr_val_t));if(!result)return nullptr;
 Env en;if(!en.e){free(result);return nullptr;}JNIEnv* e=en.e;jobject o=object_ref(e);if(!o){free(result);return nullptr;}
 jobject song=e->CallObjectMethod(o,song_method);bool failed=exception(e);
 for(unsigned i=0;i<n;i++){
  result[i].attr_id=wanted[i];if(!song||failed)continue;
  auto str=(jstring)e->GetObjectField(song,attrs_fields[wanted[i]-1]);if(exception(e)||!str)continue;
  jsize len=e->GetStringLength(str);const jchar* data=e->GetStringChars(str,nullptr);
  if(data){utf8(data,(size_t)len,(char*)result[i].text,sizeof(result[i].text));e->ReleaseStringChars(str,data);}else exception(e);
  e->DeleteLocalRef(str);
 }
 if(song)e->DeleteLocalRef(song);e->DeleteLocalRef(o);*returned=(uint8_t)n;return result;
}
static void key(int id,int pressed){Env en;if(!en.e)return;jobject o=object_ref(en.e);if(o){en.e->CallVoidMethod(o,key_method,(jint)id,pressed?JNI_TRUE:JNI_FALSE);exception(en.e);en.e->DeleteLocalRef(o);}}
static int peer_index(RawAddress* a,bool create){for(int i=0;i<2;i++)if(peers[i].used&&!memcmp(&peers[i].addr,a,6))return i;if(create)for(int i=0;i<2;i++)if(!peers[i].used){peers[i].used=true;peers[i].addr=*a;peers[i].subscriptions=0;return i;}return -1;}
static void notification(btrc_event_id_t id,btrc_notification_type_t type,RawAddress* a){
 if(!rc||!a)return;btrc_register_notification_t p={};Status s=status();
 pthread_mutex_lock(&mutex);uint64_t serial=track_serial;pthread_mutex_unlock(&mutex);
 switch(id){
 case BTRC_EVT_PLAY_STATUS_CHANGED:p.play_status=s.state;break;
 case BTRC_EVT_TRACK_CHANGE:for(int i=0;i<8;i++)p.track[i]=(uint8_t)(serial>>(56-8*i));break;
 case BTRC_EVT_PLAY_POS_CHANGED:p.song_pos=s.position;break;
 case BTRC_EVT_APP_SETTINGS_CHANGED:p.player_setting.num_attr=0;break;
 case BTRC_EVT_ADDR_PLAYER_CHANGE:p.addr_player_changed.player_id=1;p.addr_player_changed.uid_counter=1;break;
 case BTRC_EVT_UIDS_CHANGED:p.uid_counter=1;break;
 default:break;
 }
 int result=rc->register_notification_rsp(id,type,&p,a);LOG("notification event=%d type=%d status=%d",id,type,result);
}
static void features(RawAddress*,btrc_remote_features_t f){LOG("remote_features=%u",(unsigned)f);}
static void get_status(RawAddress* a){if(!rc||!a)return;Status s=status();int r=rc->get_play_status_rsp(a,s.state,s.duration,s.position);LOG("play_status response=%d",r);}
static void get_attrs(uint8_t n,btrc_media_attr_t* ids,RawAddress* a){if(!rc||!a)return;uint8_t out=0;auto* values=attributes(n,ids,&out);int r=rc->get_element_attr_rsp(a,out,values);LOG("element_attributes requested=%u returned=%u title_present=%d response=%d",n,out,values&&out&&values[0].text[0]!=0,r);free(values);}
static void register_event(btrc_event_id_t id,uint32_t,RawAddress* a){if(!a||id<1||id>13)return;pthread_mutex_lock(&mutex);int i=peer_index(a,true);if(i>=0)peers[i].subscriptions|=1u<<(unsigned)id;pthread_mutex_unlock(&mutex);notification(id,BTRC_NOTIFICATION_TYPE_INTERIM,a);}
static void volume(uint8_t,uint8_t,RawAddress*){LOG("remote volume event observed; volume handling unchanged");}
static void passthrough(int id,int pressed,RawAddress*){LOG("passthrough id=%d pressed=%d",id,pressed);key(id,pressed);}
static void connection(bool connected,bool,RawAddress* a){if(!a)return;pthread_mutex_lock(&mutex);int i=peer_index(a,connected);if(i>=0&&!connected)peers[i]={};pthread_mutex_unlock(&mutex);LOG("rc_connection=%d",connected);}
static void list_app_attrs(RawAddress* a){if(rc)rc->list_player_app_attr_rsp(a,0,nullptr);}
static void list_app_values(btrc_player_attr_t,RawAddress* a){if(rc)rc->list_player_app_value_rsp(a,0,nullptr);}
static void app_values(uint8_t,btrc_player_attr_t*,RawAddress* a){btrc_player_settings_t s={};if(rc)rc->get_player_app_value_rsp(a,&s);}
static void app_attr_text(uint8_t,btrc_player_attr_t*,RawAddress* a){if(rc)rc->get_player_app_attr_text_rsp(a,0,nullptr);}
static void app_value_text(uint8_t,uint8_t,uint8_t*,RawAddress* a){if(rc)rc->get_player_app_value_text_rsp(a,0,nullptr);}
static void set_app_value(btrc_player_settings_t*,RawAddress* a){if(rc)rc->set_player_app_value_rsp(a,BTRC_STS_BAD_PARAM);}
static void addressed(uint16_t player,RawAddress* a){if(rc)rc->set_addressed_player_rsp(a,player==1?BTRC_STS_NO_ERROR:BTRC_STS_INV_PLAYER);}
static void browsed(uint16_t player,RawAddress* a){if(rc)rc->set_browsed_player_rsp(a,player==1?BTRC_STS_NO_ERROR:BTRC_STS_INV_PLAYER,1,BTRC_CHARSET_ID_UTF8,0,nullptr);}
static void folder(uint8_t scope,uint32_t start,uint32_t end,uint8_t num,uint32_t* ids,RawAddress* a){
 if(!rc||!a)return;if(start>0||start>end){rc->get_folder_items_list_rsp(a,BTRC_STS_INV_RANGE,1,0,nullptr);return;}
 auto* item=(btrc_folder_items_t*)calloc(1,sizeof(btrc_folder_items_t));if(!item)return;
 uint8_t count=0;
 if(scope==BTRC_SCOPE_PLAYER_LIST){item->item_type=BTRC_ITEM_PLAYER;item->player.player_id=1;item->player.major_type=1;item->player.play_status=(uint8_t)status().state;item->player.charset_id=BTRC_CHARSET_ID_UTF8;memcpy(item->player.name,"Android",8);}
 else if(scope==BTRC_SCOPE_FILE_SYSTEM||scope==BTRC_SCOPE_NOW_PLAYING){item->item_type=BTRC_ITEM_MEDIA;item->media.uid[7]=1;item->media.type=BTRC_MEDIA_TYPE_AUDIO;item->media.charset_id=BTRC_CHARSET_ID_UTF8;
  auto* vals=attributes(num==255?255:num,(btrc_media_attr_t*)ids,&count);item->media.num_attrs=count;item->media.p_attrs=vals;
  uint8_t titles=0;btrc_media_attr_t title_id=BTRC_MEDIA_ATTR_TITLE;auto* title=attributes(1,&title_id,&titles);if(titles&&title)memcpy(item->media.name,title[0].text,sizeof(item->media.name));free(title);
 }
 else {free(item);rc->get_folder_items_list_rsp(a,BTRC_STS_BAD_PARAM,1,0,nullptr);return;}
 int r=rc->get_folder_items_list_rsp(a,BTRC_STS_NO_ERROR,1,1,item);LOG("folder_items scope=%u response=%d",scope,r);
 if(item->item_type==BTRC_ITEM_MEDIA)free(item->media.p_attrs);free(item);
}
static void change_path(uint8_t,uint8_t*,RawAddress* a){if(rc)rc->change_path_rsp(a,BTRC_STS_INV_DIRECTORY,0);}
static void item_attrs(uint8_t,uint8_t*,uint16_t,uint8_t n,btrc_media_attr_t* ids,RawAddress* a){if(!rc)return;uint8_t out=0;auto* values=attributes(n,ids,&out);rc->get_item_attr_rsp(a,BTRC_STS_NO_ERROR,out,values);free(values);}
static void play_item(uint8_t,uint16_t,uint8_t* uid,RawAddress* a){bool valid=uid&&uid[7]==1;if(rc)rc->play_item_rsp(a,valid?BTRC_STS_NO_ERROR:BTRC_STS_INV_ITEM);if(valid){key(0x44,1);key(0x44,0);}}
static void total_items(uint8_t scope,RawAddress* a){if(rc)rc->get_total_num_of_items_rsp(a,scope<=BTRC_SCOPE_NOW_PLAYING?BTRC_STS_NO_ERROR:BTRC_STS_BAD_PARAM,1,1);}
static void search(uint16_t,uint16_t,uint8_t*,RawAddress* a){if(rc)rc->search_rsp(a,BTRC_STS_SRCH_NOT_SPRTD,1,0);}
static void add_item(uint8_t,uint8_t*,uint16_t,RawAddress* a){if(rc)rc->add_to_now_playing_rsp(a,BTRC_STS_BAD_PARAM);}
static btrc_callbacks_t callbacks={sizeof(callbacks),features,get_status,list_app_attrs,list_app_values,app_values,app_attr_text,app_value_text,set_app_value,get_attrs,register_event,volume,passthrough,addressed,browsed,folder,change_path,item_attrs,play_item,total_items,search,add_item,connection};
extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* v,void*){vm=v;return JNI_VERSION_1_6;}
extern "C" JNIEXPORT jboolean JNICALL Java_com_android_bluetooth_avrcp_NgdAvrcpLegacy_init(JNIEnv* e,jclass,jobject object){
 if(!object)return JNI_FALSE;
 void* h=dlopen("libbluetooth_jni_ngd.so",RTLD_NOW|RTLD_NOLOAD);if(!h){LOG("Existing gated JNI not loaded");return JNI_FALSE;}
 auto get=(const BtApiPrefix*(*)())dlsym(h,"_ZN7android21getBluetoothInterfaceEv");const auto* bt=get?get():nullptr;
 if(!bt||bt->size!=280||!bt->get_profile_interface){dlclose(h);LOG("Unexpected BT ABI");return JNI_FALSE;}
 rc=(const btrc_interface_t*)bt->get_profile_interface("avrcp");dlclose(h);
 if(!rc||rc->size!=sizeof(btrc_interface_t)||!rc->init||!rc->get_element_attr_rsp||!rc->register_notification_rsp||!rc->cleanup){rc=nullptr;LOG("Unexpected legacy AVRCP ABI");return JNI_FALSE;}
 jclass c=e->GetObjectClass(object);
 if(!c||exception(e)){rc=nullptr;return JNI_FALSE;}
 song_method=e->GetMethodID(c,"getCurrentSongInfo","()Lcom/android/bluetooth/avrcp/Metadata;");
 if(exception(e)||!song_method){e->DeleteLocalRef(c);rc=nullptr;return JNI_FALSE;}
 status_method=e->GetMethodID(c,"getPlayStatus","()Lcom/android/bluetooth/avrcp/PlayStatus;");
 if(exception(e)||!status_method){e->DeleteLocalRef(c);rc=nullptr;return JNI_FALSE;}
 key_method=e->GetMethodID(c,"sendMediaKeyEvent","(IZ)V");e->DeleteLocalRef(c);
 if(exception(e)||!key_method){rc=nullptr;return JNI_FALSE;}
 c=e->FindClass("com/android/bluetooth/avrcp/Metadata");
 if(exception(e)||!c){rc=nullptr;return JNI_FALSE;}
 const char* names[]={"title","artist","album","trackNum","numTracks","genre","duration"};
 for(int i=0;i<7;i++){attrs_fields[i]=e->GetFieldID(c,names[i],"Ljava/lang/String;");if(exception(e)||!attrs_fields[i]){e->DeleteLocalRef(c);rc=nullptr;return JNI_FALSE;}}
 e->DeleteLocalRef(c);c=e->FindClass("com/android/bluetooth/avrcp/PlayStatus");
 if(exception(e)||!c){rc=nullptr;return JNI_FALSE;}
 status_state=e->GetFieldID(c,"state","B");
 if(exception(e)||!status_state){e->DeleteLocalRef(c);rc=nullptr;return JNI_FALSE;}
 status_duration=e->GetFieldID(c,"duration","J");
 if(exception(e)||!status_duration){e->DeleteLocalRef(c);rc=nullptr;return JNI_FALSE;}
 status_position=e->GetFieldID(c,"position","J");e->DeleteLocalRef(c);
 if(exception(e)||!status_position){rc=nullptr;return JNI_FALSE;}
 pthread_mutex_lock(&mutex);if(running){pthread_mutex_unlock(&mutex);return JNI_TRUE;}java_object=e->NewGlobalRef(object);running=java_object!=nullptr;pthread_mutex_unlock(&mutex);
 if(!running){rc=nullptr;return JNI_FALSE;}
 int r=rc->init(&callbacks,1);LOG("legacy_init status=%d interface_size=%zu callbacks_size=%zu",r,rc->size,sizeof(callbacks));
 if(r!=0){pthread_mutex_lock(&mutex);running=false;e->DeleteGlobalRef(java_object);java_object=nullptr;pthread_mutex_unlock(&mutex);rc=nullptr;return JNI_FALSE;}
 return JNI_TRUE;
}
extern "C" JNIEXPORT void JNICALL Java_com_android_bluetooth_avrcp_NgdAvrcpLegacy_update(JNIEnv*,jclass,jboolean metadata,jboolean state,jboolean queue){
 Peer snapshot[2];pthread_mutex_lock(&mutex);if(!running){pthread_mutex_unlock(&mutex);return;}if(metadata)track_serial++;memcpy(snapshot,peers,sizeof(peers));pthread_mutex_unlock(&mutex);
 for(auto& p:snapshot){if(!p.used)continue;for(unsigned id=1;id<=13;id++){
  bool changed=(state&&(id==1||id==5))||(metadata&&(id==2||id==9||id==12))||(queue&&(id==9||id==10));
  if(changed&&(p.subscriptions&(1u<<id))){notification((btrc_event_id_t)id,BTRC_NOTIFICATION_TYPE_CHANGED,&p.addr);pthread_mutex_lock(&mutex);int i=peer_index(&p.addr,false);if(i>=0)peers[i].subscriptions&=~(1u<<id);pthread_mutex_unlock(&mutex);}
 }}
}
extern "C" JNIEXPORT void JNICALL Java_com_android_bluetooth_avrcp_NgdAvrcpLegacy_cleanup(JNIEnv* e,jclass){
 pthread_mutex_lock(&mutex);bool was=running;running=false;pthread_mutex_unlock(&mutex);
 if(was&&rc)rc->cleanup();pthread_mutex_lock(&mutex);if(java_object)e->DeleteGlobalRef(java_object);java_object=nullptr;memset(peers,0,sizeof(peers));pthread_mutex_unlock(&mutex);LOG("legacy_cleanup");
}
#ifdef NGD_BRIDGE_TEST
#include <assert.h>
int main(){char b[12];uint16_t s[]={0x017c,0x00f3,0xd83d,0xde42};assert(utf8(s,4,b,sizeof(b))==8);assert(!strcmp(b,"żó🙂"));char c[7];assert(utf8(s,4,c,sizeof(c))==4);assert(!strcmp(c,"żó"));uint16_t bad[]={0xd800};assert(utf8(bad,1,b,sizeof(b))==3);assert(!strcmp(b,"\xef\xbf\xbd"));assert(sizeof(btrc_element_attr_val_t)==65540);assert(offsetof(btrc_callbacks_t,get_element_attr_cb)==72);assert(offsetof(btrc_callbacks_t,register_notification_cb)==80);assert(offsetof(btrc_callbacks_t,passthrough_cmd_cb)==96);return 0;}
#endif
