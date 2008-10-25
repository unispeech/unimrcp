[Setup]
AppName=UniMRCP
AppVerName=UniMRCP-0.2.0
AppPublisher=UniMRCP
AppPublisherURL=http://www.unimrcp.org/
AppSupportURL=http://groups.google.com/group/unimrcp
AppUpdatesURL=http://code.google.com/p/unimrcp/downloads/list
DefaultDirName={pf}\UniMRCP
DefaultGroupName=UniMRCP
OutputBaseFilename=unimrcp-0.2.0
Compression=lzma
InternalCompressLevel=max
SolidCompression=true

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

