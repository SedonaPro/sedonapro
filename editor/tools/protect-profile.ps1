$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false)
[Console]::InputEncoding=[System.Text.UTF8Encoding]::new($false)
try {
 Add-Type -AssemblyName System.Security
 $request=[Console]::In.ReadToEnd() | ConvertFrom-Json
 $entropy=[Text.Encoding]::UTF8.GetBytes([string]$request.binding)
 if($request.action -eq 'protect') {
  $plain=[Text.Encoding]::UTF8.GetBytes([string]$request.value)
  $result=[Security.Cryptography.ProtectedData]::Protect($plain,$entropy,[Security.Cryptography.DataProtectionScope]::CurrentUser)
  [Array]::Clear($plain,0,$plain.Length)
  $value=[Convert]::ToBase64String($result)
 } elseif($request.action -eq 'unprotect') {
  $plain=[Security.Cryptography.ProtectedData]::Unprotect([Convert]::FromBase64String([string]$request.value),$entropy,[Security.Cryptography.DataProtectionScope]::CurrentUser)
  $value=[Text.Encoding]::UTF8.GetString($plain)
  [Array]::Clear($plain,0,$plain.Length)
 } else { throw 'Invalid action' }
 @{value=$value} | ConvertTo-Json -Compress
} catch { [Console]::Error.WriteLine('Windows credential protection failed'); exit 1 }
