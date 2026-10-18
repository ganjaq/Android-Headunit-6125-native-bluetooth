package org.nextgendrive.hotspot;
public final class BootRecoveryPolicyTest {
 private static void check(boolean b,String name){if(!b)throw new AssertionError(name);}
 public static void main(String[] args){
  check(BootRecoveryPolicy.sameBoot(443,443),"same boot");
  check(!BootRecoveryPolicy.sameBoot(442,443),"reboot invalidates ownership");
  check(!BootRecoveryPolicy.sameBoot(-1,443),"legacy journal not trusted as same boot");
  check(!BootRecoveryPolicy.sameBoot(-1,-1),"unknown boot is not proof");
  check(BootRecoveryPolicy.recyclable(11,false),"confirmed AP off recovers old journal");
  check(!BootRecoveryPolicy.recyclable(11,true),"pending API request cannot be discarded");
  for(int ap:new int[]{0,10,12,13,14})check(!BootRecoveryPolicy.recyclable(ap,false),"foreign/transition/unknown AP retained");
  check(BootRecoveryPolicy.retryAllowed(0)&&BootRecoveryPolicy.retryAllowed(28),"bounded retries available");
  check(!BootRecoveryPolicy.retryAllowed(29)&&!BootRecoveryPolicy.retryAllowed(-1),"retry boundary");
  System.out.println("PASS: restart/legacy ownership, confirmed AP-OFF recovery, pending/foreign AP protection, bounded retries");
 }
}