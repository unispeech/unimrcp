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

[Files]
Source: "..\..\Release\bin\unimrcpserver.exe"; DestDir: "{app}\bin"; Components: server
Source: "..\..\Release\bin\unimrcpclient.exe"; DestDir: "{app}\bin"; Components: client
Source: "..\..\Release\bin\*.dll"; DestDir: "{app}\bin"; Components: server client
Source: "..\..\Release\plugin\mrcpcepstral.dll"; DestDir: "{app}\plugin"; Components: server/cepstral
Source: "..\..\Release\plugin\demosynth.dll"; DestDir: "{app}\plugin"; Components: server/demosynth
Source: "..\..\Release\plugin\demorecog.dll"; DestDir: "{app}\plugin"; Components: server/demorecog
Source: "..\..\Release\conf\unimrcpserver.xml"; DestDir: "{app}\conf"; Components: server
Source: "..\..\Release\conf\unimrcpclient.xml"; DestDir: "{app}\conf"; Components: client

[Icons]
Name: "{group}\UniMRCP Server"; Filename: "{app}\bin\unimrcpserver.exe"; Parameters: "-c ""{app}\conf"" -p ""{app}\plugin"""; Components: server
Name: "{group}\UniMRCP Client"; Filename: "{app}\bin\unimrcpclient.exe"; Parameters: "-c ""{app}\conf"""; Components: client
Name: "{group}\Uninstall"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\bin\unimrcpserver.exe"; Description: "Register service"; Parameters: "--register"; Components: server

[UninstallRun]
Filename: "{app}\bin\unimrcpserver.exe"; Parameters: "--unregister"; Components: server

[Code]
procedure ModifyPluginConf(Content: String, PluginName: String, Enable: Boolean);
var
begin
  if Enable = True then
    StringChange (Content, 'class="mrcpcepstral" enable="0"', 'class="mrcpcepstral" enable="1"');
  else
    StringChange (Content, 'class="mrcpcepstral" enable="1"', 'class="mrcpcepstral" enable="0"');
  end
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Content: String;
  CfgFile: String;
begin
  if CurStep = ssPostInstall then
  begin
    CfgFile := ExpandConstant('{app}\conf\unimrcpserver.xml');
    LoadStringFromFile (CfgFile, Content);
    ModifyPluginConf (Content, 'mrcpcepstral', IsComponentSelected('server\cepstral');
    SaveStringToFile (CfgFile, Content, False);
  end
end;

