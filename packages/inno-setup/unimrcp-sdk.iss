[Setup]
AppName=UniMRCP
AppVerName=UniMRCP-0.2.0
AppPublisher=UniMRCP
AppPublisherURL=http://www.unimrcp.org/
AppSupportURL=http://groups.google.com/group/unimrcp
AppUpdatesURL=http://code.google.com/p/unimrcp/downloads/list
DefaultDirName={pf}\UniMRCP
DefaultGroupName=UniMRCP
OutputBaseFilename=unimrcp-sdk-0.2.0
Compression=lzma
InternalCompressLevel=max
SolidCompression=true

[Types]
Name: "full"; Description: "Full installation"
Name: "sdk"; Description: "SDK installation"
Name: "docs"; Description: "Documentation installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "sdk"; Description: "UniMRCP SDK (client, server and plugin development)"; Types: full sdk
Name: "docs"; Description: "UniMRCP documentation"; Types: full docs
Name: "docs\design"; Description: "Design concepts"; Types: full docs
Name: "docs\api"; Description: "API"; Types: full docs

[Files]
Source: "..\..\Release\bin\*.lib"; DestDir: "{app}\lib"; Components: sdk
Source: "..\..\docs\ea\*"; DestDir: "{app}\doc\ea"; Components: docs/design; Flags: recursesubdirs
Source: "..\..\docs\dox\*"; DestDir: "{app}\doc\dox"; Components: docs/api; Flags: recursesubdirs

[Icons]
Name: "{group}\UniMRCP Docs\Design concepts"; Filename: "{app}\doc\ea\index.htm"; Components: docs\design
Name: "{group}\UniMRCP Docs\API"; Filename: "{app}\doc\dox\html\index.html"; Components: docs\api
Name: "{group}\Uninstall"; Filename: "{uninstallexe}"

