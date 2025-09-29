unit Dashboard;

{$mode ObjFPC}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, Graphics, Dialogs, ExtCtrls, StdCtrls, Spin, EditBtn;

type

  { TDashboardForm }

  TDashboardForm = class(TForm)
    ButtonOK: TButton;
    ButtonCancel: TButton;
    CheckBoxAutorun: TCheckBox;
    Label1: TLabel;
    Label2: TLabel;
    Label3: TLabel;
    Label4: TLabel;
    Panel1: TPanel;
    StartTimeEdit: TTimeEdit;
    EndTimeEdit: TTimeEdit;
    DurationEdit: TTimeEdit;
    procedure ButtonCancelClick(Sender: TObject);
    procedure ButtonOKClick(Sender: TObject);
  private

  public

  end;

var
  DashboardForm: TDashboardForm = nil;

implementation

{$R *.lfm}

{ TDashboardForm }

procedure TDashboardForm.ButtonOKClick(Sender: TObject);
begin
  if StartTimeEdit.Time >= EndTimeEdit.Time then
  begin
    ShowMessage('Invalid startup time range');
    Exit;
  end;
  Self.ModalResult := 1;
  Close;
end;

procedure TDashboardForm.ButtonCancelClick(Sender: TObject);
begin
  Self.ModalResult := 0;
  Close;
end;

end.

