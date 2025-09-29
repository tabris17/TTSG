unit Countdown;

{$mode ObjFPC}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, Graphics, Dialogs, StdCtrls;

type

  { TCountdownForm }

  TCountdownForm = class(TForm)
    CountdownLabel: TLabel;
    TTSGLabel: TLabel;
    procedure FormClose(Sender: TObject; var CloseAction: TCloseAction);
    procedure FormCloseQuery(Sender: TObject; var CanClose: Boolean);
    procedure FormCreate(Sender: TObject);
    procedure FormShow(Sender: TObject);
  private

  public
    procedure SetCountdown(Remaining: TDateTime);
  end;

implementation

uses
  Main, Windows;

function FindProgmanWindow: HWND;
var
  Child: HWND;
begin
  Result := 0;
  repeat
    Result := FindWindowExW(0, Result, 'Progman', nil);
    Child := FindWindowExW(Result, 0, 'SHELLDLL_DefView', nil);
    if Child <> 0 then Break;
  until Result = 0;
end;

{$R *.lfm}

{ TCountdownForm }

procedure TCountdownForm.FormCreate(Sender: TObject);
var
  ExStyle: Long;
begin
  CountdownLabel.Font.Name := FONT_NAME;
  TTSGLabel.Font.Name := FONT_NAME;

  Windows.SetParent(Handle, FindProgmanWindow);
  ExStyle := GetWindowLong(Handle, GWL_EXSTYLE) or WS_EX_LAYERED or WS_EX_TOOLWINDOW;
  SetWindowLong(Handle, GWL_EXSTYLE, ExStyle);
  SetWindowPos(Handle, 0, Left, Top, Width, Height, SWP_FRAMECHANGED or SWP_NOMOVE or SWP_NOSIZE or SWP_NOZORDER);
  Color := clBlack;
  SetLayeredWindowAttributes(Handle, ColorToRGB(clBlack), 0, LWA_COLORKEY);
end;

procedure TCountdownForm.FormShow(Sender: TObject);
var
  OffsetX: longint = 0;
  OffsetY: longint = 0;
  i: integer;
  AMonitor: TMonitor;
begin
  for i := 0 to Screen.MonitorCount - 1 do
  begin
    AMonitor := Screen.Monitors[i];
    if AMonitor.BoundsRect.Left < OffsetX then
      OffsetX := AMonitor.BoundsRect.Left;
    if AMonitor.BoundsRect.Top < OffsetY then
      OffsetY := AMonitor.BoundsRect.Top;
  end;
  with Screen.PrimaryMonitor do
  begin
    Self.Left := Abs(OffsetX) + BoundsRect.Left + WorkareaRect.Width - Self.Width;
    Self.Top := Abs(OffsetY) + BoundsRect.Top + WorkareaRect.Height - Self.Height;
  end;
end;

procedure TCountdownForm.FormCloseQuery(Sender: TObject; var CanClose: Boolean);
begin
  CanClose := False;
end;

procedure TCountdownForm.FormClose(Sender: TObject; var CloseAction: TCloseAction);
begin
  CloseAction := caFree;
end;

procedure TCountdownForm.SetCountdown(Remaining: TDateTime);
var
  Hours, Minutes, Seconds, MSec: Word;
begin
  if Remaining > 0 then
  begin
    DecodeTime(Remaining, Hours, Minutes, Seconds, MSec);
    CountdownLabel.Caption := Format('%.2d:%.2d', [Hours, Minutes]);
  end
  else
  begin
    CountdownLabel.Caption := '00:00';
  end;
end;

end.

