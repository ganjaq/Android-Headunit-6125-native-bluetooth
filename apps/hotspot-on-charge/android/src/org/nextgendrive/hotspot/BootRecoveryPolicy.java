package org.nextgendrive.hotspot;
/** Pure recovery decisions, independently tested without Android/network access. */
final class BootRecoveryPolicy {
 static boolean sameBoot(int saved,int current){return saved>=0&&current>=0&&saved==current;}
 static boolean recyclable(int apState,boolean pending){return apState==11&&!pending;}
 static boolean retryAllowed(int attempt){return attempt>=0&&attempt<29;}
}