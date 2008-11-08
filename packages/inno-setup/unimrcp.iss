[Setup]
#include "setup.iss"
OutputBaseFilename=unimrcp-0.2.0

[Types]
Name: "full"; Description: "Full installation"
Name: "server"; Description: "Server installation"
Name: "client"; Description: "Client installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "server"; Description: "UniMRCP server"; Types: full server
Name: "server\cepstral"; Description: "Cepstral synthesizer plugin"; Types: full server
Name: "server\demosynth"; Description: "Demo synthesizer plugin"; Types: full server
Name: "server\demorecog"; Description: "Demo recognizer plugin"; Types: full server
Name: "client"; Description: "UniMRCP client (demo application)"; Types: full client

[Dirs]
Name: "{app}\data"
Name: "{app}\log"

[Files]
Source: "..\..\Release\bin\unimrcpserver.exe"; DestDir: "{app}\bin"; Components: server
Source: "..\..\Release\bin\unimrcpservice.exe"; DestDir: "{app}\bin"; Components: server
Source: "..\..\Release\bin\unimrcpclient.exe"; DestDir: "{app}\bin"; Components: client
Source: "..\..\Release\bin\*.dll"; DestDir: "{app}\bin"; Components: server client
Source: "..\..\Release\plugin\mrcpcepstral.dll"; DestDir: "{app}\plugin"; Components: server/cepstral
Source: "..\..\Release\plugin\demosynth.dll"; DestDir: "{app}\plugin"; Components: server/demosynth
Source: "..\..\Release\plugin\demorecog.dll"; DestDir: "{app}\plugin"; Components: server/demorecog
Source: "..\..\Release\conf\unimrcpserver.xml"; DestDir: "{app}\conf"; Components: server
Source: "..\..\Release\conf\unimrcpclient.xml"; DestDir: "{app}\conf"; Components: client

[Icons]
Name: "{group}\UniMRCP Server Console"; Filename: "{app}\bin\unimrcpserver.exe"; Parameters: "--root-dir ""{app}"""; Components: server
Name: "{group}\UniMRCP Client Console"; Filename: "{app}\bin\unimrcpclient.exe"; Parameters: "--root-dir ""{app}"""; Components: client
Name: "{group}\UniMRCP Service\Start Server"; Filename: "{app}\bin\unimrcpservice.exe"; Parameters: "--start"; Components: server
Name: "{group}\UniMRCP Service\Stop Server"; Filename: "{app}\bin\unimrcpservice.exe"; Parameters: "--stop"; Components: server
Name: "{group}\Uninstall"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\bin\unimrcpservice.exe"; Description: "Register service"; Parameters: "--register ""{app}"""; Components: server

[UninstallRun]
Filename: "{app}\bin\unimrcpservice.exe"; Parameters: "--unregister"; Components: server

[Code]
var
  Content: String;
  
procedure ModifyPluginConf(PluginName: String; Enable: Boolean);
var
  TextFrom: String;
  TextTo: String;
begin
  if Enable = True then
  begin
    TextFrom := 'class="' + PluginName + '" enable="0"';
    TextTo := 'class="' + PluginName + '" enable="1"';
  end
  else
  begin
    TextFrom := 'class="' + PluginName + '" enable="1"';
    TextTo := 'class="' + PluginName + '" enable="0"';
  end
  StringChange (Content, TextFrom, TextTo);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  CfgFile: String;
begin
  if CurStep = ssPostInstall then
  begin
    CfgFile := ExpandConstant('{app}\conf\unimrcpserver.xml');
    LoadStringFromFile (CfgFile, Content);
    ModifyPluginConf ('mrcpcepstral', IsComponentSelected('server\cepstral'));
    ModifyPluginConf ('demosynth', IsComponentSelected('server\demosynth'));
    ModifyPluginConf ('demorecog', IsComponentSelected('server\demorecog'));
    SaveStringToFile (CfgFile, Content, False);
  end
end;

