# SPDX-License-Identifier: GPL-3.0-only
# Account-changing opt-in test: independent Windows VM clone only.
param([switch]$Run,[string]$BuildRoot)
if(!$Run){Write-Output 'Use -Run only on an independent Windows clone.';exit 0}
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class SoundCurrentGateToken {
 [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
 public static extern bool LogonUserW(string user,string domain,IntPtr password,int type,int provider,out IntPtr token);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
 public static extern IntPtr OpenMutexW(uint access,bool inherit,string name);
 [DllImport("kernel32.dll", SetLastError=true)] public static extern uint WaitForSingleObject(IntPtr handle,uint milliseconds);
 [DllImport("kernel32.dll", SetLastError=true)] public static extern bool ReleaseMutex(IntPtr handle);
 [DllImport("kernel32.dll", SetLastError=true)] public static extern bool CloseHandle(IntPtr handle);
}
'@
$base=if($BuildRoot){$BuildRoot}else{Join-Path $env:USERPROFILE 'SoundCurrent-integrated-build'}
$name='SCGate'+[guid]::NewGuid().ToString('N').Substring(0,8)
$random=New-Object byte[] 32
$rng=[Security.Cryptography.RandomNumberGenerator]::Create();$rng.GetBytes($random);$rng.Dispose()
$password=ConvertTo-SecureString ('Sc!'+[Convert]::ToBase64String($random)+'9a') -AsPlainText -Force
[Array]::Clear($random,0,$random.Length)
$user=$null;$token=[IntPtr]::Zero;$pointer=[IntPtr]::Zero;$owner=$null
function StartOwner([string]$Exe) {
 $info=New-Object Diagnostics.ProcessStartInfo
 $info.FileName=$Exe;$info.Arguments='--hold unused-fixture-directory'
 $info.UseShellExecute=$false;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
 $p=New-Object Diagnostics.Process;$p.StartInfo=$info
 if(!$p.Start()){throw 'Owner start failed'}
 return @{process=$p;ready=$p.StandardOutput.ReadLineAsync();stderr=$p.StandardError.ReadToEndAsync()}
}
function StopOwner($Item) {
 if($Item -and !$Item.process.HasExited){$Item.process.Kill();$Item.process.WaitForExit()}
 if($Item){$Item.process.Dispose()}
}
try {
 $user=New-LocalUser -Name $name -Password $password -AccountNeverExpires -PasswordNeverExpires
 Add-LocalGroupMember -Group (Get-LocalGroup -SID 'S-1-5-32-545') -Member $user
 $pointer=[Runtime.InteropServices.Marshal]::SecureStringToGlobalAllocUnicode($password)
 if(![SoundCurrentGateToken]::LogonUserW($name,$env:COMPUTERNAME,$pointer,2,0,[ref]$token)){throw 'Standard-user logon failed'}
 [Runtime.InteropServices.Marshal]::ZeroFreeGlobalAllocUnicode($pointer);$pointer=[IntPtr]::Zero
 foreach($product in @('soundcurrent-eq','soundcurrent-studio')) {
  $exe=Join-Path $base "$product\package\soundcurrent-windows-session-gate-smoke.exe"
  $owner=StartOwner $exe
  if(!$owner.ready.Wait(10000) -or $owner.ready.Result -ne 'ready'){throw 'Production gate owner not ready'}
  $context=[Security.Principal.WindowsIdentity]::Impersonate($token)
  $mutex=[IntPtr]::Zero;$held=$false
  try {
   if([Security.Principal.WindowsIdentity]::GetCurrent().User.Value -ne $user.SID.Value){throw 'Effective token is not standard-user SID'}
   $mutex=[SoundCurrentGateToken]::OpenMutexW(0x00100001,$false,'Global\SoundCurrent.AudioSession.v1')
   if($mutex -eq [IntPtr]::Zero){throw 'Standard user cannot open production gate with required rights'}
   if([SoundCurrentGateToken]::WaitForSingleObject($mutex,0) -ne 258){throw 'Standard-user token did not observe active ownership'}
   $owner.process.Kill();$owner.process.WaitForExit()
   $wait=[SoundCurrentGateToken]::WaitForSingleObject($mutex,5000)
   if($wait -notin @(0,128)){throw 'Standard-user token could not acquire after owner termination'}
   $held=$true
   # Keep the acquired handle/mutex owned by this thread, but revert the
   # effective token before launching the original user's executable.
   $context.Undo();$context.Dispose();$context=$null
   Write-Output 'Standard-user token opened, contended and acquired the production mutex'
   $contender=StartOwner $exe
   try {
    if(!$contender.process.WaitForExit(10000)){$contender.process.Kill();throw 'Primary-user contender timed out'}
    if($null -eq $contender.process.ExitCode -or $contender.process.ExitCode -ne 2){throw 'Primary-user contender was not excluded'}
   }finally{StopOwner $contender}
   $context=[Security.Principal.WindowsIdentity]::Impersonate($token)
   if([Security.Principal.WindowsIdentity]::GetCurrent().User.Value -ne $user.SID.Value){throw 'Release token mismatch'}
   if(![SoundCurrentGateToken]::ReleaseMutex($mutex)){throw 'Standard-user release failed'}
   $held=$false
  }finally{
   if($held){[void][SoundCurrentGateToken]::ReleaseMutex($mutex)}
   if($mutex -ne [IntPtr]::Zero){[void][SoundCurrentGateToken]::CloseHandle($mutex)}
   if($context){$context.Undo();$context.Dispose()}
  }
  StopOwner $owner;$owner=$null
  $owner=StartOwner $exe
  if(!$owner.ready.Wait(10000) -or $owner.ready.Result -ne 'ready'){throw 'Primary-user recovery failed'}
  StopOwner $owner;$owner=$null
  Write-Output "PASS: $product real standard-user SID access, contention, abandoned-owner acquisition, reverse exclusion and release"
 }
}finally{
 StopOwner $owner
 if($pointer -ne [IntPtr]::Zero){[Runtime.InteropServices.Marshal]::ZeroFreeGlobalAllocUnicode($pointer)}
 if($token -ne [IntPtr]::Zero){[void][SoundCurrentGateToken]::CloseHandle($token)}
 if($user){Remove-LocalUser -Name $name}
 $password.Dispose()
}
Write-Output 'Temporary account removed; no desktop or security-policy permissions changed'
