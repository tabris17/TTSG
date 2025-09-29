unit Main;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, eventlog, Forms, Controls, Graphics, Dialogs, ExtCtrls, Menus, Countdown, Dashboard, JwaWinType;

type

  { TMainForm }

  TMainForm = class(TForm)
    EventLog: TEventLog;
    MenuItemCountdown: TMenuItem;
    MenuItemOpen: TMenuItem;
    MenuItemExit: TMenuItem;
    Separator1: TMenuItem;
    Timer: TTimer;
    TrayMenu: TPopupMenu;
    TrayIcon: TTrayIcon;
    procedure FormCreate(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure MenuItemCountdownClick(Sender: TObject);
    procedure MenuItemExitClick(Sender: TObject);
    procedure MenuItemOpenClick(Sender: TObject);
    procedure TimerTimer(Sender: TObject);
  private
    FStartTime: TDateTime;
    FEndTime: TDateTime;
    FDuration: integer;
    FStartupTime: TDateTime;
    FGoodbyeTime: TDateTime;
    FCustomFont: Pointer;
    FCountdownForm: TCountdownForm;
    procedure LoadCustomFont;
  public
    procedure SetParamters(const StartTime, EndTime: TDateTime; const Duration: integer);
    procedure Startup;
  end;

var
  MainForm: TMainForm;

const
  APP_NAME = 'TTSG';
  FONT_NAME = APP_NAME;
  MSG_TTSG_STARTED = APP_NAME + ' started';
  EVENT_ID_STARTUP = 12;
  EVENT_ID_TTSG = 65535;
  REG_AUTORUN = 'SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run';
  MSG_TTSG = 'Right now!';

  RRF_RT_REG_SZ = 2;

  PARAM_DEFAULT_START = 8 * 60 + 30;
  PARAM_DEFAULT_END = 10 * 60 + 30;
  PARAM_DEFAULT_DURATION = 8 * 60;

implementation

uses
  Windows, DateUtils, JwaWinGDI, JwaWinReg;

function FindSystemStartupTime(const StartTime, EndTime: TDateTime): TDateTime; inline;
var
  LogHandle: THandle;
  EventLogRecord: PEventLogRecord;
  Buffer: array of Byte;
  BytesRead, MinBytesNeeded: DWORD;
  LogTime: TDateTime;
  Found: Boolean;
  EarliestTime: TDateTime;
  APtr: integer = 0;
  LocalBias: Integer;
  TimeZoneInfo: TTimeZoneInformation;
  SourcePtr: PWideChar;
begin
  Result := 0;
  EarliestTime := 0;
  Found := False;

  case GetTimeZoneInformation(TimeZoneInfo) of
    TIME_ZONE_ID_STANDARD, TIME_ZONE_ID_DAYLIGHT:
      LocalBias := -(TimeZoneInfo.Bias * 60 + TimeZoneInfo.DaylightBias * 60);
    else
      LocalBias := -(TimeZoneInfo.Bias * 60);
  end;

  LogHandle := OpenEventLogW(nil, 'System');
  if LogHandle <> INVALID_HANDLE_VALUE then
  begin
    try
      SetLength(Buffer, 4096);
      while ReadEventLogW(LogHandle, EVENTLOG_SEQUENTIAL_READ or EVENTLOG_BACKWARDS_READ, 0,
        @Buffer[0], Length(Buffer), BytesRead, MinBytesNeeded) do
      begin
        APtr := 0;
        while APtr < BytesRead do
        begin
          EventLogRecord := PEventLogRecord(@Buffer[APtr]);
          LogTime := UnixToDateTime(EventLogRecord^.TimeGenerated + LocalBias);
          if EventLogRecord^.EventID = EVENT_ID_STARTUP then
          begin
            if (LogTime >= StartTime) and (LogTime <= EndTime) then
            begin
              if (not Found) or (LogTime < EarliestTime) then
              begin
                EarliestTime := LogTime;
                Found := True;
              end;
            end;
          end
          else if LogTime < StartTime then
            Break;
          Inc(APtr, EventLogRecord^.Length);
        end;
      end;
    finally
      CloseEventLog(LogHandle);
    end;
  end;

  if Found then
    Exit(EarliestTime);

  LogHandle := OpenEventLogW(nil, 'Application');
  if LogHandle <> INVALID_HANDLE_VALUE then
  begin
    try
      SetLength(Buffer, 4096);
      while ReadEventLogW(LogHandle, EVENTLOG_SEQUENTIAL_READ or EVENTLOG_BACKWARDS_READ, 0,
        @Buffer[0], Length(Buffer), BytesRead, MinBytesNeeded) do
      begin
        APtr := 0;
        while APtr < BytesRead do
        begin
          EventLogRecord := PEventLogRecord(@Buffer[APtr]);
          LogTime := UnixToDateTime(EventLogRecord^.TimeGenerated + LocalBias);
          if EventLogRecord^.EventID = EVENT_ID_TTSG then
          begin
            SourcePtr := PWideChar(@Buffer[APtr + SizeOf(TEventLogRecord)]);
            if SourcePtr = APP_NAME then
            begin
              if (LogTime >= StartTime) and (LogTime <= EndTime) then
              begin
                if (not Found) or (LogTime < EarliestTime) then
                begin
                  EarliestTime := LogTime;
                  Found := True;
                end;
              end;
            end;
          end
          else if LogTime < StartTime then
            Break;
          Inc(APtr, EventLogRecord^.Length);
        end;
      end;
    finally
      CloseEventLog(LogHandle);
    end;
  end;

  if Found then
    Result := EarliestTime;
end;

function TodayOf(const AHour, AMinute, ASecond: Word): TDateTime; inline;
var
  TheDay: TDateTime;
begin
  TheDay := Today;
  Result := EncodeDateTime(YearOf(TheDay), MonthOf(TheDay), DayOf(TheDay), AHour, AMinute, ASecond, 0);
end;

function TodayOf(const Time: TDateTime): TDateTime; inline; overload;
begin
  Result := Date + Frac(Time);
end;

function TimeToInt(const Time: TDateTime): integer; inline;
var
  Hours, Minutes, S, MS: Word;
begin
  DecodeTime(Time, Hours, Minutes, S, MS);
  Result := Hours * 60 + Minutes;
end;

function IntToTime(const AMinutes: integer): TDateTime; inline;
begin
  Result := EncodeTime(AMinutes div 60, AMinutes mod 60, 0, 0);
end;

function RegGetValueW(hkey: HKEY; lpSubKey: LPCWSTR; lpValue: LPCWSTR;
  dwFlags: DWORD; pdwType: LPDWORD; pvData: PVOID; pcbData: LPDWORD): LONG; stdcall; external 'advapi32' name 'RegGetValueW';

{$R *.lfm}

{ TMainForm }

procedure TMainForm.FormCreate(Sender: TObject);
begin
  LoadCustomFont;
  EventLog.EventIDOffset := EVENT_ID_TTSG - 1;
  EventLog.Info(MSG_TTSG_STARTED);
  FStartTime := 0;
  FEndTime := 0;
  FDuration := 0;
  FCountdownForm := nil;
  Application.Title := APP_NAME;
end;

procedure TMainForm.FormDestroy(Sender: TObject);
begin
  Freemem(FCustomFont);
end;

procedure TMainForm.MenuItemCountdownClick(Sender: TObject);
begin
  if Assigned(FCountdownForm) then
  begin                               
    MenuItemCountdown.Checked := False;
    FreeAndNil(FCountdownForm);
  end
  else
  begin                         
    MenuItemCountdown.Checked := True;
    FCountdownForm := TCountdownForm.Create(Self);
    FCountdownForm.SetCountdown(FGoodbyeTime - Now);
    FCountdownForm.Show;
  end;
end;

procedure TMainForm.MenuItemExitClick(Sender: TObject);
begin
  Close;
end;

procedure TMainForm.MenuItemOpenClick(Sender: TObject);
var
  RegKey: HKEY;
  CommandLine: UnicodeString;
begin
  with DashboardForm do
  begin
    StartTimeEdit.Time := FStartTime;
    EndTimeEdit.Time := FEndTime;
    DurationEdit.Time := IntToTime(FDuration);
    CheckboxAutorun.Checked := ERROR_SUCCESS = RegGetValueW(
      HKEY_CURRENT_USER,
      unicodestring(REG_AUTORUN),
      unicodestring(APP_NAME),
      RRF_RT_REG_SZ,
      nil, nil, nil
    );
    if ShowModal > 0 then
    begin
      SetParamters(TodayOf(StartTimeEdit.Time), TodayOf(EndTimeEdit.Time), TimeToInt(DurationEdit.Time));
      if ERROR_SUCCESS <> RegOpenKeyW(HKEY_CURRENT_USER, unicodestring(REG_AUTORUN), RegKey) then
        ShowMessage('Failed to write to the registry')
      else
      begin
        if CheckboxAutorun.Checked then
        begin
          CommandLine := unicodestring(Format('"%s" %d %d %d', [
            ParamStr(0),
            TimeToInt(StartTimeEdit.Time),
            TimeToInt(EndTimeEdit.Time),
            TimeToInt(DurationEdit.Time)
          ]));
          RegSetValueExW(RegKey, unicodestring(APP_NAME), 0, REG_SZ, PByte(CommandLine), Length(CommandLine) * SizeOf(WideChar));
        end
        else
          RegDeleteValueW(RegKey, unicodestring(APP_NAME));
        RegCloseKey(RegKey);
      end;
    end;
  end;
end;

procedure TMainForm.TimerTimer(Sender: TObject);
var
  Remaining: TDateTime;
begin
  Remaining := FGoodbyeTime - Now;
  if Remaining <= 0 then
  begin
    Timer.Enabled := False;
    MessageDlg(APP_NAME, MSG_TTSG, mtConfirmation, [mbOK], 0);
  end;
  if Assigned(FCountdownForm) then
    FCountdownForm.SetCountdown(Remaining);
end;

procedure TMainForm.LoadCustomFont;
var
  FontStream: TResourceStream;
  FontHandle: DWORD;
begin
  FontStream := TResourceStream.Create(HInstance, 'TTSG_FONT', RT_RCDATA);
  try
    GetMem(FCustomFont, FontStream.Size);
    FontStream.ReadBuffer(FCustomFont^, FontStream.Size);
    AddFontMemResourceEx(FCustomFont, FontStream.Size, nil, @FontHandle);
  finally
    FontStream.Free;
  end;
end;

procedure TMainForm.SetParamters(const StartTime, EndTime: TDateTime; const Duration: integer);
begin
  FStartTime := StartTime;
  FEndTIme := EndTime;
  FDuration := Duration;
  FStartupTime := FindSystemStartupTime(StartTime, EndTIme);
  if FStartupTime > 0 then
  begin
    FGoodbyeTime := FStartupTime + IntToTime(Duration);
    Timer.Enabled := True;
    if not Assigned(FCountdownForm) then
      FCountdownForm := TCountdownForm.Create(Self);
    MenuItemCountdown.Checked := True;
    MenuItemCountdown.Enabled := True;
    FCountdownForm.SetCountdown(FGoodbyeTime - Now);
  end
  else
  begin
    Timer.Enabled := False;
    MenuItemCountdown.Checked := False;
    MenuItemCountdown.Enabled := False;
    if Assigned(FCountdownForm) then
      FreeAndNil(FCountdownForm);
  end;
  {$PUSH}{$WARN 5044 OFF}
  ShowWindow(Application.Handle, SW_HIDE);
  {$POP}
end;

procedure TMainForm.Startup;
var
  StartTime, EndTime, Duration: integer;
begin
  if ParamCount >= 3 then
  begin
    if not TryStrToInt(ParamStr(1), StartTime) then
      StartTime := PARAM_DEFAULT_START;
    if not TryStrToInt(ParamStr(2), EndTime) then
      EndTime := PARAM_DEFAULT_END;
    if not TryStrToInt(ParamStr(3), Duration) then
      Duration := PARAM_DEFAULT_DURATION;
  end
  else
  begin
    StartTime := PARAM_DEFAULT_START;
    EndTime := PARAM_DEFAULT_END;
    Duration := PARAM_DEFAULT_DURATION;
  end;
  SetParamters(
    TodayOf(StartTime div 60, StartTime mod 60, 0),
    TodayOf(EndTime div 60, EndTime mod 60, 0),
    Duration
  );
end;

end.

