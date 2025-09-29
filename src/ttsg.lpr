program TTSG;

{$mode objfpc}{$H+}

uses
  {$IFDEF UNIX}
  cthreads,
  {$ENDIF}
  {$IFDEF HASAMIGA}
  athreads,
  {$ENDIF}
  Interfaces, // this includes the LCL widgetset
  Forms, Main, Dashboard;

{$R *.res}

begin
  RequireDerivedFormResource := True;
  Application.Scaled:=True;
  Application.ShowMainForm := False;
  {$PUSH}{$WARN 5044 OFF}
  Application.MainFormOnTaskbar := False;
  {$POP}
  Application.Initialize;
  Application.CreateForm(TMainForm, MainForm);
  Application.CreateForm(TDashboardForm, DashboardForm);
  MainForm.Startup;
  Application.Run;
end.

